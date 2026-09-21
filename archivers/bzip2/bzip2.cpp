// bzip2 1.0.8 CLI for Braam (ported from upstream bzip2.c).
#include <string.h>

#include "braam.h"
#include "kernel/args.h"

#define SM_I2O  1
#define SM_F2O  2
#define SM_F2F  3
#define OM_Z    1
#define OM_UNZ  2
#define OM_TEST 3

#define ERROR_IF_EOF(i)         \
    {                           \
        if ((i) == EOF)         \
            co_await ioError(); \
    }
#define ERROR_IF_NOT_ZERO(i)    \
    {                           \
        if ((i) != 0)           \
            co_await ioError(); \
    }
#define ERROR_IF_MINUS_ONE(i)   \
    {                           \
        if ((i) == (-1))        \
            co_await ioError(); \
    }

#define APPEND_FILESPEC(root, name) root = snocString((root), (name))
#define APPEND_FLAG(root, name)     root = snocString((root), (name))
#define SET_BINARY_MODE(fd)         ((void)0)

static const int FILE_NAME_LEN     = 1034;
static const int BZ_N_SUFFIX_PAIRS = 4;

Int32 verbosity;
Bool keepInputFiles, smallMode, deleteOutputOnInterrupt;
Bool forceOverwrite, testFailsExist, unzFailsExist, noisy;
Int32 numFileNames, numFilesProcessed, blockSize100k;
Int32 exitValue;
Int32 opMode;
Int32 srcMode;
Int32 longestFileName;
Char inName[FILE_NAME_LEN];
Char outName[FILE_NAME_LEN];
Char tmpName[FILE_NAME_LEN];
Char *progName;
Char progNameReally[FILE_NAME_LEN];
FILE *outputHandleJustInCase;
Int32 workFactor;

static Task<void> ioError(void);
static Task<void> outOfMemory(void);
static Task<void> configError(void);
static Task<void> panic(const Char *s);
static Task<void> crcError(void);
static Task<void> cleanUpAndFail(Int32 ec);
static Task<void> compressedStreamEOF(void);
static Task<void> cadvise(void);
static Task<void> showFileNames(void);
static Task<void> applySavedFileAttrToOutputFile(IntNative fd);
static Task<Bool> fileExists(Char *name);
static Task<FILE *> fopen_output_safely(Char *name, const char *mode);
static Task<Bool> notAStandardFile(Char *name);
static Task<Int32> countHardLinks(Char *name);
static Task<void> compress(Char *name);
static Task<void> uncompress(Char *name);
static Task<void> testf(Char *name);
static Task<void> usage(Char *fullProgName);
static Task<void> license(void);
static Task<void> redundant(Char *flag);
typedef struct zzzz {
    Char *name;
    struct zzzz *link;
} Cell;

static Task<void> addFlagsFromEnvVar(Cell **argList, Char *varName);

static Task<void> copyFileName(Char *to, Char *from);
static void *myMalloc(Int32 n);
static void setExit(Int32 v)
{
    if (v > exitValue)
        exitValue = v;
}

typedef struct {
    UChar b[8];
} UInt64;

static void uInt64_from_UInt32s(UInt64 *n, UInt32 lo32, UInt32 hi32)
{
    n->b[7] = (UChar)((hi32 >> 24) & 0xFF);
    n->b[6] = (UChar)((hi32 >> 16) & 0xFF);
    n->b[5] = (UChar)((hi32 >> 8) & 0xFF);
    n->b[4] = (UChar)(hi32 & 0xFF);
    n->b[3] = (UChar)((lo32 >> 24) & 0xFF);
    n->b[2] = (UChar)((lo32 >> 16) & 0xFF);
    n->b[1] = (UChar)((lo32 >> 8) & 0xFF);
    n->b[0] = (UChar)(lo32 & 0xFF);
}

static double uInt64_to_double(UInt64 *n)
{
    double base = 1.0;
    double sum  = 0.0;
    for (Int32 i = 0; i < 8; i++) {
        sum += base * (double)(n->b[i]);
        base *= 256.0;
    }
    return sum;
}

static void uInt64_toAscii(char *outbuf, UInt64 *n)
{
    Int32 i;
    UChar buf[32];
    Int32 nBuf    = 0;
    UInt64 n_copy = *n;
    do {
        Int32 rem = 0;
        for (i = 7; i >= 0; i--) {
            unsigned tmp = (unsigned)rem * 256 + n_copy.b[i];
            n_copy.b[i]  = (UChar)(tmp / 10);
            rem          = tmp % 10;
        }
        buf[nBuf++] = (UChar)(rem + '0');
    } while (n_copy.b[0] | n_copy.b[1] | n_copy.b[2] | n_copy.b[3] | n_copy.b[4] | n_copy.b[5] |
             n_copy.b[6] | n_copy.b[7]);
    outbuf[nBuf] = 0;
    for (i = 0; i < nBuf; i++)
        outbuf[i] = (char)buf[nBuf - i - 1];
}

/*---------------------------------------------------*/
/*--- Processing of complete files and streams    ---*/
/*---------------------------------------------------*/

static const unsigned BZ_IO_CHUNK = 5000;

static Task<void> b2_pump_out(bz_stream &bzs, UChar *obuf, FILE *out)
{
    unsigned n = BZ_IO_CHUNK - bzs.avail_out;
    if (n == 0)
        co_return;
    size_t w = co_await b_fwrite(obuf, 1, n, out);
    if (w != n || b_ferror(out)) {
        co_await ioError();
        co_return;
    }
}

static Task<Bool> myfeof(FILE *f)
{
    int c = co_await b_fgetc(f);
    if (c == EOF)
        co_return True;
    b_ungetc(c, f);
    co_return False;
}

static Task<void> compressStream(FILE *stream, FILE *zStream)
{
    static UChar ibuf[BZ_IO_CHUNK];
    static UChar obuf[BZ_IO_CHUNK];
    bz_stream bzs = {};
    bzs.bzalloc   = NULL;
    bzs.bzfree    = NULL;
    bzs.opaque    = NULL;
    int ret       = BZ2_bzCompressInit(&bzs, blockSize100k, verbosity, workFactor);
    if (ret == BZ_CONFIG_ERROR) {
        co_await configError();
        co_return;
    }
    if (ret == BZ_MEM_ERROR) {
        co_await outOfMemory();
        co_return;
    }
    if (ret != BZ_OK) {
        co_await panic("compress:unexpected error");
        co_return;
    }

    if (b_ferror(stream) || b_ferror(zStream)) {
        BZ2_bzCompressEnd(&bzs);
        co_await ioError();
        co_return;
    }

    if (verbosity >= 2)
        co_await b_fprintf(stderr, "\n");

    for (;;) {
        if (sig_take(SIG_INT)) {
            BZ2_bzCompressEnd(&bzs);
            co_await b2_on_int();
            co_await ioError();
            co_return;
        }
        if (co_await myfeof(stream))
            break;
        size_t nIbuf = co_await b_fread(ibuf, sizeof(UChar), BZ_IO_CHUNK, stream);
        if (b_ferror(stream)) {
            BZ2_bzCompressEnd(&bzs);
            co_await ioError();
            co_return;
        }
        if (nIbuf == 0)
            continue;
        bzs.next_in  = (char *)ibuf;
        bzs.avail_in = (unsigned)nIbuf;
        while (bzs.avail_in > 0) {
            bzs.next_out  = (char *)obuf;
            bzs.avail_out = BZ_IO_CHUNK;
            ret           = BZ2_bzCompress(&bzs, BZ_RUN);
            if (ret != BZ_OK && ret != BZ_RUN_OK) {
                BZ2_bzCompressEnd(&bzs);
                if (ret == BZ_MEM_ERROR)
                    co_await outOfMemory();
                else
                    co_await panic("compress:unexpected error");
                co_return;
            }
            co_await b2_pump_out(bzs, obuf, zStream);
        }
    }

    for (;;) {
        bzs.next_out  = (char *)obuf;
        bzs.avail_out = BZ_IO_CHUNK;
        ret           = BZ2_bzCompress(&bzs, BZ_FINISH);
        co_await b2_pump_out(bzs, obuf, zStream);
        if (ret == BZ_FINISH_OK)
            continue;
        if (ret == BZ_STREAM_END)
            break;
        BZ2_bzCompressEnd(&bzs);
        if (ret == BZ_MEM_ERROR)
            co_await outOfMemory();
        else
            co_await panic("compress:unexpected error");
        co_return;
    }

    UInt32 nbytes_in_lo32  = bzs.total_in_lo32;
    UInt32 nbytes_in_hi32  = bzs.total_in_hi32;
    UInt32 nbytes_out_lo32 = bzs.total_out_lo32;
    UInt32 nbytes_out_hi32 = bzs.total_out_hi32;

    if (BZ2_bzCompressEnd(&bzs) != BZ_OK) {
        co_await ioError();
        co_return;
    }

    if (b_ferror(zStream)) {
        co_await ioError();
        co_return;
    }
    if (co_await b_fflush(zStream) == EOF) {
        co_await ioError();
        co_return;
    }
    if (zStream != stdout) {
        int fd = b_fileno(zStream);
        if (fd < 0) {
            co_await ioError();
            co_return;
        }
        co_await applySavedFileAttrToOutputFile(fd);
        if (co_await b_fclose(zStream) == EOF) {
            outputHandleJustInCase = NULL;
            co_await ioError();
            co_return;
        }
        outputHandleJustInCase = NULL;
    }
    outputHandleJustInCase = NULL;
    if (b_ferror(stream)) {
        co_await ioError();
        co_return;
    }
    if (co_await b_fclose(stream) == EOF) {
        co_await ioError();
        co_return;
    }

    if (verbosity >= 1) {
        if (nbytes_in_lo32 == 0 && nbytes_in_hi32 == 0) {
            co_await b_fprintf(stderr, " no data compressed.\n");
        } else {
            Char buf_nin[32], buf_nout[32];
            UInt64 nbytes_in, nbytes_out;
            double nbytes_in_d, nbytes_out_d;
            uInt64_from_UInt32s(&nbytes_in, nbytes_in_lo32, nbytes_in_hi32);
            uInt64_from_UInt32s(&nbytes_out, nbytes_out_lo32, nbytes_out_hi32);
            nbytes_in_d  = uInt64_to_double(&nbytes_in);
            nbytes_out_d = uInt64_to_double(&nbytes_out);
            uInt64_toAscii(buf_nin, &nbytes_in);
            uInt64_toAscii(buf_nout, &nbytes_out);
            co_await b_fprintf(stderr,
                               "%6.3f:1, %6.3f bits/byte, "
                               "%5.2f%% saved, %s in, %s out.\n",
                               nbytes_in_d / nbytes_out_d, (8.0 * nbytes_out_d) / nbytes_in_d,
                               100.0 * (1.0 - nbytes_out_d / nbytes_in_d), buf_nin, buf_nout);
        }
    }
}

static Task<Bool> b2_decompress_loop(FILE *zStream, FILE *stream, Bool discard)
{
    static UChar ibuf[BZ_IO_CHUNK];
    static UChar obuf[BZ_IO_CHUNK];
    bz_stream bzs   = {};
    int streamNo    = 0;
    int end_of_file = 0;

    bzs.bzalloc = NULL;
    bzs.bzfree  = NULL;
    bzs.opaque  = NULL;

    for (;;) {
        int ret = BZ2_bzDecompressInit(&bzs, verbosity, (int)smallMode);
        if (ret != BZ_OK) {
            if (ret == BZ_CONFIG_ERROR)
                co_await configError();
            else if (ret == BZ_MEM_ERROR)
                co_await outOfMemory();
            else
                co_await ioError();
            co_return False;
        }
        streamNo++;

        for (;;) {
            if (bzs.avail_in == 0 && !end_of_file) {
                if (sig_take(SIG_INT)) {
                    BZ2_bzDecompressEnd(&bzs);
                    co_await b2_on_int();
                    co_await ioError();
                    co_return False;
                }
                size_t n = co_await b_fread(ibuf, sizeof(UChar), BZ_IO_CHUNK, zStream);
                if (b_ferror(zStream)) {
                    BZ2_bzDecompressEnd(&bzs);
                    co_await ioError();
                    co_return False;
                }
                if (n == 0)
                    end_of_file = 1;
                bzs.next_in  = (char *)ibuf;
                bzs.avail_in = (unsigned)n;
            }
            bzs.next_out  = (char *)obuf;
            bzs.avail_out = BZ_IO_CHUNK;
            ret           = BZ2_bzDecompress(&bzs);
            if (ret == BZ_DATA_ERROR_MAGIC) {
                BZ2_bzDecompressEnd(&bzs);
                if (forceOverwrite) {
                    co_await b_rewind(zStream);
                    end_of_file = 0;
                    while (!co_await myfeof(zStream)) {
                        size_t nr = co_await b_fread(obuf, sizeof(UChar), BZ_IO_CHUNK, zStream);
                        if (b_ferror(zStream)) {
                            co_await ioError();
                            co_return False;
                        }
                        if (nr > 0 && !discard) {
                            co_await b_fwrite(obuf, sizeof(UChar), nr, stream);
                            if (b_ferror(stream)) {
                                co_await ioError();
                                co_return False;
                            }
                        }
                    }
                    co_return True;
                }
                if (streamNo == 1)
                    co_return False;
                if (noisy)
                    co_await b_fprintf(stderr, "\n%s: %s: trailing garbage after EOF ignored\n",
                                       progName, inName);
                co_return True;
            }
            if (ret == BZ_OK || ret == BZ_STREAM_END) {
                unsigned produced = BZ_IO_CHUNK - bzs.avail_out;
                if (produced > 0 && !discard) {
                    co_await b_fwrite(obuf, sizeof(UChar), produced, stream);
                    if (b_ferror(stream)) {
                        BZ2_bzDecompressEnd(&bzs);
                        co_await ioError();
                        co_return False;
                    }
                }
            }
            if (ret == BZ_STREAM_END) {
                if (BZ2_bzDecompressEnd(&bzs) != BZ_OK) {
                    co_await panic("decompress:end");
                    co_return False;
                }
                if (bzs.avail_in == 0 && (end_of_file || co_await myfeof(zStream)))
                    co_return True;
                end_of_file = 0;
                break;
            }
            if (ret != BZ_OK) {
                BZ2_bzDecompressEnd(&bzs);
                if (ret == BZ_DATA_ERROR)
                    co_await crcError();
                else if (ret == BZ_MEM_ERROR)
                    co_await outOfMemory();
                else if (ret == BZ_UNEXPECTED_EOF)
                    co_await compressedStreamEOF();
                else
                    co_await panic("decompress:unexpected error");
                co_return False;
            }
            if (end_of_file && bzs.avail_in == 0) {
                BZ2_bzDecompressEnd(&bzs);
                co_await compressedStreamEOF();
                co_return False;
            }
        }
    }
}

static Task<Bool> uncompressStream(FILE *zStream, FILE *stream)
{
    if (b_ferror(stream) || b_ferror(zStream)) {
        co_await ioError();
        co_return False;
    }
    Bool ok = co_await b2_decompress_loop(zStream, stream, False);
    if (!ok)
        co_return False;

    if (b_ferror(zStream)) {
        co_await ioError();
        co_return False;
    }
    if (stream != stdout) {
        int fd = b_fileno(stream);
        if (fd < 0) {
            co_await ioError();
            co_return False;
        }
        co_await applySavedFileAttrToOutputFile(fd);
    }
    if (co_await b_fclose(zStream) == EOF) {
        co_await ioError();
        co_return False;
    }
    if (b_ferror(stream)) {
        co_await ioError();
        co_return False;
    }
    if (co_await b_fflush(stream) != 0) {
        co_await ioError();
        co_return False;
    }
    if (stream != stdout) {
        if (co_await b_fclose(stream) == EOF) {
            outputHandleJustInCase = NULL;
            co_await ioError();
            co_return False;
        }
        outputHandleJustInCase = NULL;
    }
    outputHandleJustInCase = NULL;
    if (verbosity >= 2)
        co_await b_fprintf(stderr, "\n    ");
    co_return True;
}

static Task<Bool> testStream(FILE *zStream)
{
    if (b_ferror(zStream)) {
        co_await ioError();
        co_return False;
    }
    Bool ok = co_await b2_decompress_loop(zStream, NULL, True);
    if (!ok) {
        if (verbosity == 0)
            co_await b_fprintf(stderr, "%s: %s: ", progName, inName);
        co_return False;
    }
    if (b_ferror(zStream)) {
        co_await ioError();
        co_return False;
    }
    if (co_await b_fclose(zStream) == EOF) {
        co_await ioError();
        co_return False;
    }
    if (verbosity >= 2)
        co_await b_fprintf(stderr, "\n    ");
    co_return True;
}

/*---------------------------------------------------*/
/*--- Error [non-] handling grunge                ---*/
/*---------------------------------------------------*/

static Task<void> cadvise(void)
{
    if (noisy)
        co_await b_fprintf(stderr,
                           "\nIt is possible that the compressed file(s) have become corrupted.\n"
                           "You can use the -tvv option to test integrity of such files.\n\n"
                           "You can use the `bzip2recover' program to attempt to recover\n"
                           "data from undamaged sections of corrupted files.\n\n");
    co_return;
}

/*---------------------------------------------*/
static Task<void> showFileNames(void)
{
    if (noisy)
        co_await b_fprintf(stderr, "\tInput file = %s, output file = %s\n", inName, outName);
    co_return;
}

/*---------------------------------------------*/
static Task<void> cleanUpAndFail(Int32 ec)
{
    IntNative retVal;
    struct stat statBuf;

    if (srcMode == SM_F2F && opMode != OM_TEST && deleteOutputOnInterrupt) {
        /* Check whether input file still exists.  Delete output file
           only if input exists to avoid loss of data.  Joerg Prante, 5
           January 2002.  (JRS 06-Jan-2002: other changes in 1.0.2 mean
           this is less likely to happen.  But to be ultra-paranoid, we
           do the check anyway.)  */
        retVal = co_await b_stat(inName, &statBuf);
        if (retVal == 0) {
            if (noisy)
                co_await b_fprintf(stderr, "%s: Deleting output file %s, if it exists.\n", progName,
                                   outName);
            if (outputHandleJustInCase != NULL)
                co_await b_fclose(outputHandleJustInCase);
            retVal = co_await b_unlink(outName);
            if (retVal != 0)
                co_await b_fprintf(stderr,
                                   "%s: WARNING: deletion of output file "
                                   "(apparently) failed.\n",
                                   progName);
        } else {
            co_await b_fprintf(stderr, "%s: WARNING: deletion of output file suppressed\n",
                               progName);
            co_await b_fprintf(stderr, "%s:    since input file no longer exists.  Output file\n",
                               progName);
            co_await b_fprintf(stderr, "%s:    `%s' may be incomplete.\n", progName, outName);
            co_await b_fprintf(stderr,
                               "%s:    I suggest doing an integrity test (bzip2 -tv)"
                               " of it.\n",
                               progName);
        }
    }

    if (noisy && numFileNames > 0 && numFilesProcessed < numFileNames) {
        co_await b_fprintf(stderr,
                           "%s: WARNING: some files have not been processed:\n"
                           "%s:    %d specified on command line, %d not processed yet.\n\n",
                           progName, progName, numFileNames, numFileNames - numFilesProcessed);
    }
    setExit(ec);
    b2_fatal = 1;
    co_return;
}

/*---------------------------------------------*/
static Task<void> panic(const Char *s)
{
    co_await b_fprintf(stderr,
                       "\n%s: PANIC -- internal consistency error:\n"
                       "\t%s\n"
                       "\tThis is a BUG.  Please report it to:\n"
                       "\tbzip2-devel@sourceware.org\n",
                       progName, s);
    co_await showFileNames();
    co_await cleanUpAndFail(3);
    co_return;
}

/*---------------------------------------------*/
static Task<void> crcError(void)
{
    co_await b_fprintf(stderr, "\n%s: Data integrity error when decompressing.\n", progName);
    co_await showFileNames();
    co_await cadvise();
    co_await cleanUpAndFail(2);
    co_return;
}

/*---------------------------------------------*/
static Task<void> compressedStreamEOF(void)
{
    if (noisy) {
        co_await b_fprintf(stderr,
                           "\n%s: Compressed file ends unexpectedly;\n\t"
                           "perhaps it is corrupted?  *Possible* reason follows.\n",
                           progName);
        co_await b_perror(progName);
        co_await showFileNames();
        co_await cadvise();
    }
    co_await cleanUpAndFail(2);
    co_return;
}

/*---------------------------------------------*/
static Task<void> ioError(void)
{
    co_await b_fprintf(stderr,
                       "\n%s: I/O or other error, bailing out.  "
                       "Possible reason follows.\n",
                       progName);
    co_await b_perror(progName);
    co_await showFileNames();
    co_await cleanUpAndFail(1);
    co_return;
}

/*---------------------------------------------*/
static Task<void> outOfMemory(void)
{
    co_await b_fprintf(stderr, "\n%s: couldn't allocate enough memory\n", progName);
    co_await showFileNames();
    co_await cleanUpAndFail(1);
    co_return;
}

/*---------------------------------------------*/
static Task<void> configError(void)
{
    co_await b_fprintf(stderr,
                       "bzip2: I'm not configured correctly for this platform!\n"
                       "\tI require Int32, Int16 and Char to have sizes\n"
                       "\tof 4, 2 and 1 bytes to run properly, and they don't.\n"
                       "\tProbably you can fix this by defining them correctly,\n"
                       "\tand recompiling.  Bye!\n");
    setExit(3);
    b2_fatal = 1;
    co_return;
}

/*---------------------------------------------------*/
/*--- The main driver machinery                   ---*/
/*---------------------------------------------------*/

/* All rather crufty.  The main problem is that input files
   are stat()d multiple times before use.  This should be
   cleaned up.
*/

/*---------------------------------------------*/
static Task<void> pad(Char *s)
{
    Int32 i;
    if ((Int32)strlen(s) >= longestFileName)
        co_return;
    for (i = 1; i <= longestFileName - (Int32)strlen(s); i++)
        co_await b_fprintf(stderr, " ");
}

/*---------------------------------------------*/
static Task<void> copyFileName(Char *to, Char *from)
{
    if (strlen(from) > (size_t)FILE_NAME_LEN - 10) {
        co_await b_fprintf(stderr,
                           "bzip2: file name\n`%s'\n"
                           "is suspiciously (more than %d chars) long.\n"
                           "Try using a reasonable file name instead.  Sorry! :-)\n",
                           from, FILE_NAME_LEN - 10);
        setExit(1);
        b2_fatal = 1;
        co_return;
    }
    strncpy(to, from, FILE_NAME_LEN - 10);
    to[FILE_NAME_LEN - 10] = '\0';
    co_return;
}

/*---------------------------------------------*/
static Task<Bool> fileExists(Char *name)
{
    FILE *tmp   = co_await b_fopen(name, "rb");
    Bool exists = (tmp != NULL);
    if (tmp != NULL)
        co_await b_fclose(tmp);
    co_return exists;
}

/*---------------------------------------------*/
/* Open an output file safely with O_EXCL and good permissions.
   This avoids a race condition in versions < 1.0.2, in which
   the file was first opened and then had its interim permissions
   set safely.  We instead use co_await b_open() to create the file with
   the interim permissions required. (--- --- rw-).

   For non-Unix platforms, if we are not worrying about
   security issues, simple this simply behaves like fopen.
*/
static Task<FILE *> fopen_output_safely(Char *name, const char *mode)
{
    int fh = co_await b_open(name, O_WRONLY | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR);
    if (fh == -1)
        co_return (FILE *) NULL;
    FILE *fp = co_await b_fdopen(fh, mode);
    if (fp == NULL)
        co_await b_close(fh);
    co_return fp;
}

/*---------------------------------------------*/
/*--
  if in doubt, return True
--*/
static Task<Bool> notAStandardFile(Char *name)
{
    struct stat statBuf;
    int i = co_await b_lstat(name, &statBuf);
    if (i != 0)
        co_return True;
    co_return MY_S_ISREG(statBuf.st_mode) ? False : True;
}

/*---------------------------------------------*/
/*--
  rac 11/21/98 see if file has hard links to it
--*/
static Task<Int32> countHardLinks(Char *name)
{
    struct stat statBuf;
    int i = co_await b_lstat(name, &statBuf);
    if (i != 0)
        co_return 0;
    co_return (Int32)(statBuf.st_nlink - 1);
}

/*---------------------------------------------*/
/* Copy modification date, access date, permissions and owner from the
   source to destination file.  We have to copy this meta-info off
   into fileMetaInfo before starting to compress / decompress it,
   because doing it afterwards means we get the wrong access time.

   To complicate matters, in compress() and decompress() below, the
   sequence of tests preceding the call to co_await saveInputFileMetaInfo()
   involves calling fileExists(), which in turn establishes its result
   by attempting to co_await b_fopen() the file, and if successful, immediately
   co_await b_fclose()ing it again.  So we have to assume that the co_await b_fopen() call
   does not cause the access time field to be updated.

   Reading of the man page for stat() (man 2 stat) on RedHat 7.2 seems
   to imply that merely doing co_await b_open() will not affect the access time.
   Therefore we merely need to hope that the C library only does
   co_await b_open() as a result of co_await b_fopen(), and not any kind of read()-ahead
   cleverness.

   It sounds pretty fragile to me.  Whether this carries across
   robustly to arbitrary Unix-like platforms (or even works robustly
   on this one, RedHat 7.2) is unknown to me.  Nevertheless ...
*/
static Task<void> saveInputFileMetaInfo(Char *srcName)
{
    (void)srcName;
    co_return;
}

static Task<void> applySavedTimeInfoToOutputFile(Char *dstName)
{
    (void)dstName;
    co_return;
}

static Task<void> applySavedFileAttrToOutputFile(IntNative fd)
{
    (void)fd;
    co_return;
}

/*---------------------------------------------*/
static Bool containsDubiousChars(Char *name)
{
#if BZ_UNIX
    /* On unix, files can contain any characters and the file expansion
     * is performed by the shell.
     */
    return False;
#else  /* ! BZ_UNIX */
    /* On non-unix (Win* platforms), wildcard characters are not allowed in
     * filenames.
     */
    for (; *name != '\0'; name++)
        if (*name == '?' || *name == '*')
            return True;
    return False;
#endif /* BZ_UNIX */
}

/*---------------------------------------------*/
const Char *zSuffix[BZ_N_SUFFIX_PAIRS]   = { ".bz2", ".bz", ".tbz2", ".tbz" };
const Char *unzSuffix[BZ_N_SUFFIX_PAIRS] = { "", "", ".tar", ".tar" };

static Bool hasSuffix(Char *s, const Char *suffix)
{
    Int32 ns = strlen(s);
    Int32 nx = strlen(suffix);
    if (ns < nx)
        return False;
    if (strcmp(s + ns - nx, suffix) == 0)
        return True;
    return False;
}

static Bool mapSuffix(Char *name, const Char *oldSuffix, const Char *newSuffix)
{
    if (!hasSuffix(name, oldSuffix))
        return False;
    name[strlen(name) - strlen(oldSuffix)] = 0;
    strcat(name, newSuffix);
    return True;
}

/*---------------------------------------------*/
static Task<void> compress(Char *name)
{
    FILE *inStr  = NULL;
    FILE *outStr = NULL;
    Int32 n, i;
    struct stat statBuf;

    deleteOutputOnInterrupt = False;

    if (name == NULL && srcMode != SM_I2O) {
        co_await panic("compress: bad modes");
        co_return;
    }

    switch (srcMode) {
    case SM_I2O:
        co_await copyFileName(inName, (Char *)"(stdin)");
        co_await copyFileName(outName, (Char *)"(stdout)");
        break;
    case SM_F2F:
        co_await copyFileName(inName, name);
        co_await copyFileName(outName, name);
        strcat(outName, ".bz2");
        break;
    case SM_F2O:
        co_await copyFileName(inName, name);
        co_await copyFileName(outName, (Char *)"(stdout)");
        break;
    }

    if (srcMode != SM_I2O && containsDubiousChars(inName)) {
        if (noisy)
            co_await b_fprintf(stderr, "%s: There are no files matching `%s'.\n", progName, inName);
        setExit(1);
        co_return;
    }
    if (srcMode != SM_I2O && !co_await fileExists(inName)) {
        co_await b_fprintf(stderr, "%s: Can't open input file %s: %s.\n", progName, inName,
                           strerror(errno));
        setExit(1);
        co_return;
    }
    for (i = 0; i < BZ_N_SUFFIX_PAIRS; i++) {
        if (hasSuffix(inName, zSuffix[i])) {
            if (noisy)
                co_await b_fprintf(stderr, "%s: Input file %s already has %s suffix.\n", progName,
                                   inName, zSuffix[i]);
            setExit(1);
            co_return;
        }
    }
    if (srcMode == SM_F2F || srcMode == SM_F2O) {
        co_await b_stat(inName, &statBuf);
        if (MY_S_ISDIR(statBuf.st_mode)) {
            co_await b_fprintf(stderr, "%s: Input file %s is a directory.\n", progName, inName);
            setExit(1);
            co_return;
        }
    }
    if (srcMode == SM_F2F && !forceOverwrite && co_await notAStandardFile(inName)) {
        if (noisy)
            co_await b_fprintf(stderr, "%s: Input file %s is not a normal file.\n", progName,
                               inName);
        setExit(1);
        co_return;
    }
    if (srcMode == SM_F2F && co_await fileExists(outName)) {
        if (forceOverwrite) {
            co_await b_unlink(outName);
        } else {
            co_await b_fprintf(stderr, "%s: Output file %s already exists.\n", progName, outName);
            setExit(1);
            co_return;
        }
    }
    if (srcMode == SM_F2F && !forceOverwrite && (n = co_await countHardLinks(inName)) > 0) {
        co_await b_fprintf(stderr, "%s: Input file %s has %d other link%s.\n", progName, inName, n,
                           n > 1 ? "s" : "");
        setExit(1);
        co_return;
    }

    if (srcMode == SM_F2F) {
        /* Save the file's meta-info before we open it.  Doing it later
           means we mess up the access times. */
        co_await saveInputFileMetaInfo(inName);
    }

    switch (srcMode) {
    case SM_I2O:
        inStr  = stdin;
        outStr = stdout;
        if (co_await b_isatty(b_fileno(stdout))) {
            co_await b_fprintf(stderr, "%s: I won't write compressed data to a terminal.\n",
                               progName);
            co_await b_fprintf(stderr, "%s: For help, type: `%s --help'.\n", progName, progName);
            setExit(1);
            co_return;
        };
        break;

    case SM_F2O:
        inStr  = co_await b_fopen(inName, "rb");
        outStr = stdout;
        if (co_await b_isatty(b_fileno(stdout))) {
            co_await b_fprintf(stderr, "%s: I won't write compressed data to a terminal.\n",
                               progName);
            co_await b_fprintf(stderr, "%s: For help, type: `%s --help'.\n", progName, progName);
            if (inStr != NULL)
                co_await b_fclose(inStr);
            setExit(1);
            co_return;
        };
        if (inStr == NULL) {
            co_await b_fprintf(stderr, "%s: Can't open input file %s: %s.\n", progName, inName,
                               strerror(errno));
            setExit(1);
            co_return;
        };
        break;

    case SM_F2F:
        inStr  = co_await b_fopen(inName, "rb");
        outStr = co_await fopen_output_safely(outName, "wb");
        if (outStr == NULL) {
            co_await b_fprintf(stderr, "%s: Can't create output file %s: %s.\n", progName, outName,
                               strerror(errno));
            if (inStr != NULL)
                co_await b_fclose(inStr);
            setExit(1);
            co_return;
        }
        if (inStr == NULL) {
            co_await b_fprintf(stderr, "%s: Can't open input file %s: %s.\n", progName, inName,
                               strerror(errno));
            if (outStr != NULL)
                co_await b_fclose(outStr);
            setExit(1);
            co_return;
        };
        break;

    default:
        co_await panic("compress: bad srcMode");
        co_return;
    }

    if (verbosity >= 1) {
        co_await b_fprintf(stderr, "  %s: ", inName);
        co_await pad(inName);
        co_await b_fflush(stderr);
    }

    /*--- Now the input and output handles are sane.  Do the Biz. ---*/
    outputHandleJustInCase  = outStr;
    deleteOutputOnInterrupt = True;
    if (srcMode == SM_F2F)
        b2_remove_out = outName;
    co_await compressStream(inStr, outStr);
    outputHandleJustInCase = NULL;

    /*--- If there was an I/O error, we won't get here. ---*/
    if (srcMode == SM_F2F) {
        co_await applySavedTimeInfoToOutputFile(outName);
        deleteOutputOnInterrupt = False;
        if (!keepInputFiles) {
            IntNative retVal = co_await b_unlink(inName);
            ERROR_IF_NOT_ZERO(retVal);
        }
    }

    deleteOutputOnInterrupt = False;
}

/*---------------------------------------------*/
static Task<void> uncompress(Char *name)
{
    FILE *inStr  = NULL;
    FILE *outStr = NULL;
    Int32 n, i;
    Bool magicNumberOK;
    Bool cantGuess;
    struct stat statBuf;

    deleteOutputOnInterrupt = False;

    if (name == NULL && srcMode != SM_I2O) {
        co_await panic("uncompress: bad modes");
        co_return;
    }

    cantGuess = False;
    switch (srcMode) {
    case SM_I2O:
        co_await copyFileName(inName, (Char *)"(stdin)");
        co_await copyFileName(outName, (Char *)"(stdout)");
        break;
    case SM_F2F:
        co_await copyFileName(inName, name);
        co_await copyFileName(outName, name);
        for (i = 0; i < BZ_N_SUFFIX_PAIRS; i++)
            if (mapSuffix(outName, zSuffix[i], unzSuffix[i]))
                break;
        if (i >= BZ_N_SUFFIX_PAIRS) {
            cantGuess = True;
            strcat(outName, ".out");
        }
        break;
    case SM_F2O:
        co_await copyFileName(inName, name);
        co_await copyFileName(outName, (Char *)"(stdout)");
        break;
    }

    if (srcMode != SM_I2O && containsDubiousChars(inName)) {
        if (noisy)
            co_await b_fprintf(stderr, "%s: There are no files matching `%s'.\n", progName, inName);
        setExit(1);
        co_return;
    }
    if (srcMode != SM_I2O && !co_await fileExists(inName)) {
        co_await b_fprintf(stderr, "%s: Can't open input file %s: %s.\n", progName, inName,
                           strerror(errno));
        setExit(1);
        co_return;
    }
    if (srcMode == SM_F2F || srcMode == SM_F2O) {
        co_await b_stat(inName, &statBuf);
        if (MY_S_ISDIR(statBuf.st_mode)) {
            co_await b_fprintf(stderr, "%s: Input file %s is a directory.\n", progName, inName);
            setExit(1);
            co_return;
        }
    }
    if (srcMode == SM_F2F && !forceOverwrite && co_await notAStandardFile(inName)) {
        if (noisy)
            co_await b_fprintf(stderr, "%s: Input file %s is not a normal file.\n", progName,
                               inName);
        setExit(1);
        co_return;
    }
    if (/* srcMode == SM_F2F implied && */ cantGuess) {
        if (noisy)
            co_await b_fprintf(stderr, "%s: Can't guess original name for %s -- using %s\n",
                               progName, inName, outName);
        /* just a warning, no return */
    }
    if (srcMode == SM_F2F && co_await fileExists(outName)) {
        if (forceOverwrite) {
            co_await b_unlink(outName);
        } else {
            co_await b_fprintf(stderr, "%s: Output file %s already exists.\n", progName, outName);
            setExit(1);
            co_return;
        }
    }
    if (srcMode == SM_F2F && !forceOverwrite && (n = co_await countHardLinks(inName)) > 0) {
        co_await b_fprintf(stderr, "%s: Input file %s has %d other link%s.\n", progName, inName, n,
                           n > 1 ? "s" : "");
        setExit(1);
        co_return;
    }

    if (srcMode == SM_F2F) {
        /* Save the file's meta-info before we open it.  Doing it later
           means we mess up the access times. */
        co_await saveInputFileMetaInfo(inName);
    }

    switch (srcMode) {
    case SM_I2O:
        inStr  = stdin;
        outStr = stdout;
        if (co_await b_isatty(b_fileno(stdin))) {
            co_await b_fprintf(stderr, "%s: I won't read compressed data from a terminal.\n",
                               progName);
            co_await b_fprintf(stderr, "%s: For help, type: `%s --help'.\n", progName, progName);
            setExit(1);
            co_return;
        };
        break;

    case SM_F2O:
        inStr  = co_await b_fopen(inName, "rb");
        outStr = stdout;
        if (inStr == NULL) {
            co_await b_fprintf(stderr, "%s: Can't open input file %s:%s.\n", progName, inName,
                               strerror(errno));
            if (inStr != NULL)
                co_await b_fclose(inStr);
            setExit(1);
            co_return;
        };
        break;

    case SM_F2F:
        inStr  = co_await b_fopen(inName, "rb");
        outStr = co_await fopen_output_safely(outName, "wb");
        if (outStr == NULL) {
            co_await b_fprintf(stderr, "%s: Can't create output file %s: %s.\n", progName, outName,
                               strerror(errno));
            if (inStr != NULL)
                co_await b_fclose(inStr);
            setExit(1);
            co_return;
        }
        if (inStr == NULL) {
            co_await b_fprintf(stderr, "%s: Can't open input file %s: %s.\n", progName, inName,
                               strerror(errno));
            if (outStr != NULL)
                co_await b_fclose(outStr);
            setExit(1);
            co_return;
        };
        break;

    default:
        co_await panic("uncompress: bad srcMode");
        co_return;
    }

    if (verbosity >= 1) {
        co_await b_fprintf(stderr, "  %s: ", inName);
        co_await pad(inName);
        co_await b_fflush(stderr);
    }

    /*--- Now the input and output handles are sane.  Do the Biz. ---*/
    outputHandleJustInCase  = outStr;
    deleteOutputOnInterrupt = True;
    if (srcMode == SM_F2F)
        b2_remove_out = outName;
    magicNumberOK          = co_await uncompressStream(inStr, outStr);
    outputHandleJustInCase = NULL;

    /*--- If there was an I/O error, we won't get here. ---*/
    if (magicNumberOK) {
        if (srcMode == SM_F2F) {
            co_await applySavedTimeInfoToOutputFile(outName);
            deleteOutputOnInterrupt = False;
            if (!keepInputFiles) {
                IntNative retVal = co_await b_unlink(inName);
                ERROR_IF_NOT_ZERO(retVal);
            }
        }
    } else {
        unzFailsExist           = True;
        deleteOutputOnInterrupt = False;
        if (srcMode == SM_F2F) {
            IntNative retVal = co_await b_unlink(outName);
            ERROR_IF_NOT_ZERO(retVal);
        }
    }
    deleteOutputOnInterrupt = False;

    if (magicNumberOK) {
        if (verbosity >= 1)
            co_await b_fprintf(stderr, "done\n");
    } else {
        setExit(2);
        if (verbosity >= 1)
            co_await b_fprintf(stderr, "not a bzip2 file.\n");
        else
            co_await b_fprintf(stderr, "%s: %s is not a bzip2 file.\n", progName, inName);
    }
}

/*---------------------------------------------*/
static Task<void> testf(Char *name)
{
    FILE *inStr = NULL;
    Bool allOK;
    struct stat statBuf;

    deleteOutputOnInterrupt = False;

    if (name == NULL && srcMode != SM_I2O) {
        co_await panic("testf: bad modes");
        co_return;
    }

    co_await copyFileName(outName, (Char *)"(none)");
    switch (srcMode) {
    case SM_I2O:
        co_await copyFileName(inName, (Char *)"(stdin)");
        break;
    case SM_F2F:
    case SM_F2O:
        co_await copyFileName(inName, name);
        break;
    }

    if (srcMode != SM_I2O && containsDubiousChars(inName)) {
        if (noisy)
            co_await b_fprintf(stderr, "%s: There are no files matching `%s'.\n", progName, inName);
        setExit(1);
        co_return;
    }
    if (srcMode != SM_I2O && !co_await fileExists(inName)) {
        co_await b_fprintf(stderr, "%s: Can't open input %s: %s.\n", progName, inName,
                           strerror(errno));
        setExit(1);
        co_return;
    }
    if (srcMode != SM_I2O) {
        co_await b_stat(inName, &statBuf);
        if (MY_S_ISDIR(statBuf.st_mode)) {
            co_await b_fprintf(stderr, "%s: Input file %s is a directory.\n", progName, inName);
            setExit(1);
            co_return;
        }
    }

    switch (srcMode) {
    case SM_I2O:
        if (co_await b_isatty(b_fileno(stdin))) {
            co_await b_fprintf(stderr, "%s: I won't read compressed data from a terminal.\n",
                               progName);
            co_await b_fprintf(stderr, "%s: For help, type: `%s --help'.\n", progName, progName);
            setExit(1);
            co_return;
        };
        inStr = stdin;
        break;

    case SM_F2O:
    case SM_F2F:
        inStr = co_await b_fopen(inName, "rb");
        if (inStr == NULL) {
            co_await b_fprintf(stderr, "%s: Can't open input file %s:%s.\n", progName, inName,
                               strerror(errno));
            setExit(1);
            co_return;
        };
        break;

    default:
        co_await panic("testf: bad srcMode");
        co_return;
    }

    if (verbosity >= 1) {
        co_await b_fprintf(stderr, "  %s: ", inName);
        co_await pad(inName);
        co_await b_fflush(stderr);
    }

    /*--- Now the input handle is sane.  Do the Biz. ---*/
    outputHandleJustInCase = NULL;
    allOK                  = co_await testStream(inStr);

    if (allOK && verbosity >= 1)
        co_await b_fprintf(stderr, "ok\n");
    if (!allOK)
        testFailsExist = True;
}

/*---------------------------------------------*/
static Task<void> license(void)
{
    co_await b_fprintf(stderr,

                       "bzip2, a block-sorting file compressor.  "
                       "Version %s.\n"
                       "   \n"
                       "   Copyright (C) 1996-2019 by Julian Seward.\n"
                       "   \n"
                       "   This program is free software; you can redistribute it and/or modify\n"
                       "   it under the terms set out in the LICENSE file, which is included\n"
                       "   in the bzip2 source distribution.\n"
                       "   \n"
                       "   This program is distributed in the hope that it will be useful,\n"
                       "   but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
                       "   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
                       "   LICENSE file for more details.\n"
                       "   \n",
                       BZ2_bzlibVersion());
    co_return;
}

/*---------------------------------------------*/
static Task<void> usage(Char *fullProgName)
{
    co_await b_fprintf(stderr,
                       "bzip2, a block-sorting file compressor.  "
                       "Version %s.\n"
                       "\n   usage: %s [flags and input files in any order]\n"
                       "\n"
                       "   -h --help           print this message\n"
                       "   -d --decompress     force decompression\n"
                       "   -z --compress       force compression\n"
                       "   -k --keep           keep (don't delete) input files\n"
                       "   -f --force          overwrite existing output files\n"
                       "   -t --test           test compressed file integrity\n"
                       "   -c --stdout         output to standard out\n"
                       "   -q --quiet          suppress noncritical error messages\n"
                       "   -v --verbose        be verbose (a 2nd -v gives more)\n"
                       "   -L --license        display software version & license\n"
                       "   -V --version        display software version & license\n"
                       "   -s --small          use less memory (at most 2500k)\n"
                       "   -1 .. -9            set block size to 100k .. 900k\n"
                       "   --fast              alias for -1\n"
                       "   --best              alias for -9\n"
                       "\n"
                       "   If invoked as `bzip2', default action is to compress.\n"
                       "              as `bunzip2',  default action is to decompress.\n"
                       "              as `bzcat', default action is to decompress to stdout.\n"
                       "\n"
                       "   If no file names are given, bzip2 compresses or decompresses\n"
                       "   from standard input to standard output.  You can combine\n"
                       "   short flags, so `-v -4' means the same as -v4 or -4v, &c.\n"
#if BZ_UNIX
                       "\n"
#endif
                       ,

                       BZ2_bzlibVersion(), fullProgName);
    co_return;
}

/*---------------------------------------------*/
static Task<void> redundant(Char *flag)
{
    co_await b_fprintf(stderr, "%s: %s is redundant in versions 0.9.5 and above\n", progName, flag);
    co_return;
}

/*---------------------------------------------*/
/*--
  All the garbage from here to main() is purely to
  implement a linked list of command-line arguments,
  into which main() copies argv[1 .. argc-1].

  The purpose of this exercise is to facilitate
  the expansion of wildcard characters * and ? in
  filenames for OSs which don't know how to do it
  themselves, like MSDOS, Windows 95 and NT.

  The actual Dirty Work is done by the platform-
  specific macro APPEND_FILESPEC.
--*/

/*---------------------------------------------*/
static void *myMalloc(Int32 n)
{
    void *p = malloc((size_t)n);
    if (p == NULL) {
        exitValue = 1;
        b2_fatal  = 1;
    }
    return p;
}

/*---------------------------------------------*/
static Cell *mkCell(void)
{
    Cell *c;

    c       = (Cell *)myMalloc(sizeof(Cell));
    c->name = NULL;
    c->link = NULL;
    return c;
}

/*---------------------------------------------*/
static Cell *snocString(Cell *root, Char *name)
{
    if (root == NULL) {
        Cell *tmp = mkCell();
        tmp->name = (Char *)myMalloc(5 + strlen(name));
        strcpy(tmp->name, name);
        return tmp;
    } else {
        Cell *tmp = root;
        while (tmp->link != NULL)
            tmp = tmp->link;
        tmp->link = snocString(tmp->link, name);
        return root;
    }
}

/*---------------------------------------------*/
static Task<void> addFlagsFromEnvVar(Cell **argList, Char *varName)
{
    Int32 i, j, k;
    Char *envbase, *p;

    envbase = getenv(varName);
    if (envbase != NULL) {
        p = envbase;
        i = 0;
        while (True) {
            if (p[i] == 0)
                break;
            p += i;
            i = 0;
            while (isspace((Int32)(p[0])))
                p++;
            while (p[i] != 0 && !isspace((Int32)(p[i])))
                i++;
            if (i > 0) {
                k = i;
                if (k > FILE_NAME_LEN - 10)
                    k = FILE_NAME_LEN - 10;
                for (j = 0; j < k; j++)
                    tmpName[j] = p[j];
                tmpName[k] = 0;
                APPEND_FLAG(*argList, tmpName);
            }
        }
    }
    co_return;
}

/*---------------------------------------------*/
#define ISFLAG(s) (strcmp(aa->name, (s)) == 0)

Task<i32> bzip2_main(Args args)
{
    Int32 i, j;
    Cell *argList;
    Cell *aa;
    Bool decode;

    if (sizeof(Int32) != 4 || sizeof(UInt32) != 4 || sizeof(Int16) != 2 || sizeof(UInt16) != 2 ||
        sizeof(Char) != 1 || sizeof(UChar) != 1) {
        co_await configError();
        co_return exitValue;
    }

    outputHandleJustInCase  = NULL;
    smallMode               = False;
    keepInputFiles          = False;
    forceOverwrite          = False;
    noisy                   = True;
    verbosity               = 0;
    blockSize100k           = 9;
    testFailsExist          = False;
    unzFailsExist           = False;
    numFileNames            = 0;
    numFilesProcessed       = 0;
    workFactor              = 30;
    deleteOutputOnInterrupt = False;
    exitValue               = 0;
    i = j = 0;

    strncpy(inName, "(none)", FILE_NAME_LEN - 10);
    inName[FILE_NAME_LEN - 10] = 0;
    strncpy(outName, "(none)", FILE_NAME_LEN - 10);
    outName[FILE_NAME_LEN - 10] = 0;

    if (args.size() == 0) {
        strncpy(progNameReally, "bzip2", FILE_NAME_LEN - 10);
        progNameReally[FILE_NAME_LEN - 10] = '\0';
    } else {
        Str leaf = path_basename(args[0]);
        usize pn = leaf.size();
        if (pn >= FILE_NAME_LEN)
            pn = FILE_NAME_LEN - 1;
        memcpy(progNameReally, leaf.data(), pn);
        progNameReally[pn] = '\0';
    }
    progName = &progNameReally[0];

    argList = NULL;
    co_await addFlagsFromEnvVar(&argList, (Char *)"BZIP2");
    co_await addFlagsFromEnvVar(&argList, (Char *)"BZIP");
    for (usize ai = 1; ai < args.size(); ai++) {
        char *spec = (char *)myMalloc((Int32)args[ai].size() + 1);
        memcpy(spec, args[ai].data(), args[ai].size());
        spec[args[ai].size()] = '\0';
        APPEND_FILESPEC(argList, spec);
    }

    longestFileName = 7;
    numFileNames    = 0;
    decode          = True;
    for (aa = argList; aa != NULL; aa = aa->link) {
        if (ISFLAG("--")) {
            decode = False;
            continue;
        }
        if (aa->name[0] == '-' && decode)
            continue;
        numFileNames++;
        if (longestFileName < (Int32)strlen(aa->name))
            longestFileName = (Int32)strlen(aa->name);
    }

    if (numFileNames == 0)
        srcMode = SM_I2O;
    else
        srcMode = SM_F2F;

    opMode = OM_Z;
    if (strstr(progName, "unzip") != 0 || strstr(progName, "UNZIP") != 0)
        opMode = OM_UNZ;
    if (strstr(progName, "z2cat") != 0 || strstr(progName, "Z2CAT") != 0 ||
        strstr(progName, "zcat") != 0 || strstr(progName, "ZCAT") != 0) {
        opMode  = OM_UNZ;
        srcMode = (numFileNames == 0) ? SM_I2O : SM_F2O;
    }

    for (aa = argList; aa != NULL; aa = aa->link) {
        if (ISFLAG("--"))
            break;
        if (aa->name[0] == '-' && aa->name[1] != '-') {
            for (j = 1; aa->name[j] != '\0'; j++) {
                switch (aa->name[j]) {
                case 'c':
                    srcMode = SM_F2O;
                    break;
                case 'd':
                    opMode = OM_UNZ;
                    break;
                case 'z':
                    opMode = OM_Z;
                    break;
                case 'f':
                    forceOverwrite = True;
                    break;
                case 't':
                    opMode = OM_TEST;
                    break;
                case 'k':
                    keepInputFiles = True;
                    break;
                case 's':
                    smallMode = True;
                    break;
                case 'q':
                    noisy = False;
                    break;
                case '1':
                    blockSize100k = 1;
                    break;
                case '2':
                    blockSize100k = 2;
                    break;
                case '3':
                    blockSize100k = 3;
                    break;
                case '4':
                    blockSize100k = 4;
                    break;
                case '5':
                    blockSize100k = 5;
                    break;
                case '6':
                    blockSize100k = 6;
                    break;
                case '7':
                    blockSize100k = 7;
                    break;
                case '8':
                    blockSize100k = 8;
                    break;
                case '9':
                    blockSize100k = 9;
                    break;
                case 'V':
                case 'L':
                    co_await license();
                    break;
                case 'v':
                    verbosity++;
                    break;
                case 'h':
                    co_await usage(progName);
                    co_return 0;
                default:
                    co_await b_fprintf(stderr, "%s: Bad flag `%s'\n", progName, aa->name);
                    co_await usage(progName);
                    co_return 1;
                }
            }
        }
    }

    for (aa = argList; aa != NULL; aa = aa->link) {
        if (ISFLAG("--"))
            break;
        if (ISFLAG("--stdout"))
            srcMode = SM_F2O;
        else if (ISFLAG("--decompress"))
            opMode = OM_UNZ;
        else if (ISFLAG("--compress"))
            opMode = OM_Z;
        else if (ISFLAG("--force"))
            forceOverwrite = True;
        else if (ISFLAG("--test"))
            opMode = OM_TEST;
        else if (ISFLAG("--keep"))
            keepInputFiles = True;
        else if (ISFLAG("--small"))
            smallMode = True;
        else if (ISFLAG("--quiet"))
            noisy = False;
        else if (ISFLAG("--version"))
            co_await license();
        else if (ISFLAG("--license"))
            co_await license();
        else if (ISFLAG("--exponential"))
            workFactor = 1;
        else if (ISFLAG("--repetitive-best"))
            co_await redundant(aa->name);
        else if (ISFLAG("--repetitive-fast"))
            co_await redundant(aa->name);
        else if (ISFLAG("--fast"))
            blockSize100k = 1;
        else if (ISFLAG("--best"))
            blockSize100k = 9;
        else if (ISFLAG("--verbose"))
            verbosity++;
        else if (ISFLAG("--help")) {
            co_await usage(progName);
            co_return 0;
        } else if (strncmp(aa->name, "--", 2) == 0) {
            co_await b_fprintf(stderr, "%s: Bad flag `%s'\n", progName, aa->name);
            co_await usage(progName);
            co_return 1;
        }
    }

    if (verbosity > 4)
        verbosity = 4;
    if (opMode == OM_Z && smallMode && blockSize100k > 2)
        blockSize100k = 2;

    if (opMode == OM_TEST && srcMode == SM_F2O) {
        co_await b_fprintf(stderr, "%s: -c and -t cannot be used together.\n", progName);
        co_return 1;
    }

    if (srcMode == SM_F2O && numFileNames == 0)
        srcMode = SM_I2O;
    if (opMode != OM_Z)
        blockSize100k = 0;

    if (opMode == OM_Z) {
        if (srcMode == SM_I2O)
            co_await compress(NULL);
        else {
            decode = True;
            for (aa = argList; aa != NULL; aa = aa->link) {
                if (ISFLAG("--")) {
                    decode = False;
                    continue;
                }
                if (aa->name[0] == '-' && decode)
                    continue;
                numFilesProcessed++;
                co_await compress(aa->name);
                if (b2_fatal)
                    break;
            }
        }
    } else if (opMode == OM_UNZ) {
        unzFailsExist = False;
        if (srcMode == SM_I2O)
            co_await uncompress(NULL);
        else {
            decode = True;
            for (aa = argList; aa != NULL; aa = aa->link) {
                if (ISFLAG("--")) {
                    decode = False;
                    continue;
                }
                if (aa->name[0] == '-' && decode)
                    continue;
                numFilesProcessed++;
                co_await uncompress(aa->name);
                if (b2_fatal)
                    break;
            }
        }
        if (unzFailsExist) {
            setExit(2);
            co_return exitValue;
        }
    } else {
        testFailsExist = False;
        if (srcMode == SM_I2O)
            co_await testf(NULL);
        else {
            decode = True;
            for (aa = argList; aa != NULL; aa = aa->link) {
                if (ISFLAG("--")) {
                    decode = False;
                    continue;
                }
                if (aa->name[0] == '-' && decode)
                    continue;
                numFilesProcessed++;
                co_await testf(aa->name);
                if (b2_fatal)
                    break;
            }
        }
        if (testFailsExist) {
            if (noisy) {
                co_await b_fprintf(stderr,
                                   "\n"
                                   "You can use the `bzip2recover' program to attempt to recover\n"
                                   "data from undamaged sections of corrupted files.\n\n");
            }
            setExit(2);
            co_return exitValue;
        }
    }

    aa = argList;
    while (aa != NULL) {
        Cell *aa2 = aa->link;
        if (aa->name != NULL)
            free(aa->name);
        free(aa);
        aa = aa2;
    }

    if (b2_fatal)
        co_return exitValue ? exitValue : 1;
    co_return exitValue;
}
