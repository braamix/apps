// zip.h — Zip 3, by Mark Adler.
//
// Info-ZIP licence; see LICENSE beside this file. Upstream's tailor.h and
// unix/osdep.h are written out here: one target, so the portability layer is
// twenty lines rather than nine hundred.
#pragma once

#include "braam.h"

// ------------------------------------------------------------ what tailor.h did

#define local  static
#define OF(a)  a
#define OFT(a) a
#define ZCONST const
#define zvoid  void

// The 16-bit memory models are gone with the ports that had them.
#define near
#define far
#define Far
#define Near

typedef unsigned char uch;  // unsigned 8-bit
typedef unsigned short ush; // unsigned 16-bit
typedef unsigned long ulg;  // unsigned 32-bit: long is 32 bits on wasm32
typedef u32 z_uint4;

typedef usize extent;

// Zip64 throughout: an offset is 64-bit whatever the filesystem holds.
typedef i64 zoff_t;
typedef u64 uzoff_t;

#define ZIP64_SUPPORT
#define UNICODE_SUPPORT
#define NO_BZIP2_SUPPORT
#define DYN_ALLOC

#define CBSZ 16384 // copy buffer
#define SBSZ CBSZ  // copy buffer for a stored entry, see zipup()

#define NULL  nullptr
#define EOF   (-1)
#define TRUE  1
#define FALSE 0

#define zcalloc(items, size) (zvoid far *)calloc((usize)(items), (usize)(size))
#define zcfree               free

// The file-type bits of a Unix mode. The filesystem keeps no mode, so what an
// entry carries is synthesized in filetime(); these are what says which.
#define S_IFMT  0170000
#define S_IFLNK 0120000
#define S_IFREG 0100000
#define S_IFDIR 0040000

#define MSDOS_HIDDEN_ATTR 0x02
#define MSDOS_DIR_ATTR    0x10

// -------------------------------------------------------------------- limits

#define MIN_MATCH 3
#define MAX_MATCH 258

#ifndef WSIZE
#define WSIZE (0x8000) // 32K; must be a power of two
#endif

#define MIN_LOOKAHEAD (MAX_MATCH + MIN_MATCH + 1)
#define MAX_DIST      (WSIZE - MIN_LOOKAHEAD)

#define FNMAX 1024

// Lengths of headers after signatures, in bytes.
#define LOCHEAD 26
#define CENHEAD 42
#define ENDHEAD 18
#define EC64LOC 16
#define EC64REC 52

// ---------------------------------------------------------------- structures

typedef struct iztimes {
    time_t atime;
    time_t mtime;
    time_t ctime;
} iztimes;

struct zlist {
    // See the central header in zipfile.cpp for what vem..off are.
    ush vem, ver, flg, how;
    ulg tim, crc;
    uzoff_t siz, len;
    ush nam, ext, cext, com; // offset of ext must be >= LOCHEAD
    ulg dsk;
    ush att, lflg; // offset of lflg must be >= LOCHEAD
    uzoff_t off;
    ulg atx;
    char *name;    // file name in the zip file
    char *extra;   // extra field, set only if ext != 0
    char *cextra;  // extra in central, set only if cext != 0
    char *comment; // comment, set only if com != 0
    char *iname;   // internal file name after cleanup, stored in the archive
    char *zname;   // external version of the internal name
    char *oname;   // display version of the name used in messages
    char *uname;   // UTF-8 version of iname
    char *zuname;  // escaped Unicode zname from uname
    char *ouname;  // display version of zuname
    int mark;      // marker for files to operate on
    int trash;     // marker for files to delete
    int current;   // marker for files current to what is on the OS
    int dosflag;   // set to force MSDOS file attributes
    struct zlist far *nxt;
};

struct flist {
    char *name;  // raw internal file name
    char *iname; // internal file name after cleanup
    char *zname; // external version of the internal name
    char *oname; // display version of the internal name
    char *uname; // UTF-8 name
    int dosflag;
    uzoff_t usize; // usize from the initial scan
    struct flist far * far * lst;
    struct flist far *nxt;
};

struct plist {
    char *zname;
    int select; // 'i' or 'x'
};

// Internal file attribute.
#define UNKNOWN (-1)
#define BINARY  0
#define ASCII   1

// Extra field IDs.
#define EF_IZUNIX  0x5855 // UNIX ("UX")
#define EF_IZUNIX2 0x7855 // Info-ZIP's new Unix ("Ux")
#define EF_TIME    0x5455 // universal timestamp ("UT")

#define EF_SIZE_MAX ((unsigned)0xFFFF)
#define EB_HEADSIZE 4
#define EB_ID       0
#define EB_LEN      2

#define LF    10
#define CR    13
#define CTRLZ 26

// Password fetch results.
#define IZ_PW_ENTERED    0
#define IZ_PW_CANCEL     -1
#define IZ_PW_CANCELALL  -2
#define IZ_PW_ERROR      5
#define IZ_PW_SKIPVERIFY IZ_PW_CANCEL

#define ZP_PW_ENTER  0
#define ZP_PW_VERIFY 1

#include "ziperr.h"

#define CRCVAL_INITIAL 0L

// zip_fzofft()'s format assembly. zoff_t is long long here, so the prefix is
// "ll" and a hex value is sixteen digits wide.
#define ZOFF_T_FORMAT_SIZE_PREFIX "ll"
#define FZOFFT_FMT                ZOFF_T_FORMAT_SIZE_PREFIX
#define FZOFFT_HEX_WID_VALUE      "16"
#define FZOFFT_HEX_WID            ((char *)-1)
#define FZOFFT_HEX_DOT_WID        ((char *)-2)

#define DOSTIME_MINIMUM    ((ulg)0x00210000L)
#define DOSTIME_2038_01_18 ((ulg)0x74320000L)

// Compression methods.
#define BEST                  -1
#define STORE                 0
#define DEFLATE               8
#define LAST_KNOWN_COMPMETHOD DEFLATE

// ------------------------------------------------------------------- globals

extern uch upper[256]; // country dependent case map
extern uch lower[256];
extern ZCONST ulg near *crc_32_tab;

// Names are case sensitive here, as on Unix.
#define case_map(c) (c)
#define to_up(c)    toupper(c)

// Pattern matching: Unix style, as on every system with a real shell. The
// 16-bit memory models are gone, so far and near allocation is plain malloc.
#define MATCH     shmatch
#define PAD       0
#define PATHCUT   '/'
#define farmalloc malloc
#define farfree   free

// The multi-byte layer collapses: names are UTF-8, and every routine that
// walks one here walks bytes.
#define CLEN(ptr)        1
#define PREINCSTR(ptr)   (++(ptr))
#define POSTINCSTR(ptr)  ((ptr)++)
#define INCSTR(ptr)      PREINCSTR(ptr)
#define lastchar(ptr)    ((*(ptr)) == '\0' ? '\0' : (ptr)[strlen(ptr) - 1])
#define MBSCHR(str, c)   strchr(str, c)
#define MBSRCHR(str, c)  strrchr(str, c)
#define MB_CLEN(ptr)     (1)
#define MB_NEXTCHAR(ptr) ((ptr)++)
#define SETLOCALE(category, locale)

extern char errbuf[FNMAX + 4081];

extern int recurse; // recurse into directories encountered
extern int dispose; // remove files after putting them in the archive
extern int pathput; // store the path with the name
extern int method;  // restriction on the compression method
extern int dosify;  // make new entries look like MSDOS
extern int level;   // compression level
extern int verbose; // report oddities in the zip file structure
extern int fix;     // 1 = fix the archive, 2 = -FF
extern int filesync;
extern int adjust;
extern int noisy; // false for quiet operation
extern int translate_eol;
extern int no_wild;
extern int allow_regex;
extern int wild_stop_at_dir;
extern int using_utf8;
extern int utf8_force;
extern int unicode_escape_all;
extern int unicode_mismatch;
extern int use_wide_to_mb_default;
extern int hidden_files;
extern int volume_label;
extern int dirnames;
extern int filter_match_case;
extern int diff_mode;
extern int linkput;         // store symbolic links as such
extern int extra_fields;    // 0 = minimum, 1 = don't copy old, 2 = keep old
extern int use_descriptors; // use data descriptors (extended headings)
extern int allow_empty_archive;
extern int copy_only;
extern int zip_to_stdout;
extern int output_seekable; // 1 = the output can be seeked
extern int allow_fifo;
extern int show_files;
extern int zip_attributes;
extern char *special; // suffixes not worth compressing
extern char *label;   // the volume label, for -$

extern int force_zip64;
extern int zip64_entry;   // the current entry needs Zip64
extern int zip64_archive; // at least one entry needs Zip64

extern char *key;     // scramble password, or NULL
extern char *tempath; // where a temporary file goes
extern char *tempzip; // temp file name
extern char *zipfile; // new or existing archive
extern char *in_path;
extern char *in_split_path;
extern char *out_path;
extern FILE *y;       // output file, global for splits
extern FILE *in_file; // current input archive, for splits
extern FILE *mesg;    // where informational output goes

extern uzoff_t zipbeg; // starting offset of the zip structures
extern uzoff_t cenbeg; // starting offset of the central directory
extern uzoff_t tempzn; // count of bytes written to the output zip file

extern ulg before, after;
extern ulg skip_this_disk;
extern int des_good;
extern ulg des_crc;
extern uzoff_t des_csize, des_usize;

// Progress dots and running counts.
extern zoff_t dot_size;
extern zoff_t dot_count;
extern int display_counts;
extern int display_bytes;
extern int display_globaldots;
extern int display_volume;
extern int display_usize;
extern ulg files_so_far, bad_files_so_far, files_total;
extern uzoff_t bytes_so_far, good_bytes_so_far, bad_bytes_so_far, bytes_total;

// The log file. Never opened yet; every `if (logall)` is therefore false.
extern int logall;
extern FILE *logfile;
extern int logfile_append;
extern char *logfile_path;
extern int logfile_line_started;
extern int mesg_line_started;

extern time_t scan_delay, scan_dot_time, scan_start, scan_last;
extern int scan_started;
extern uzoff_t scan_count;

// Splits.
extern ulg total_disks, current_in_disk, skip_current_disk;
extern uzoff_t current_in_offset;
extern ulg current_local_disk, current_disk, cd_start_disk, zip64_eocd_disk;
extern uzoff_t cd_start_offset, cd_entries_this_disk, total_cd_entries;
extern uzoff_t zip64_eocd_offset;
extern char *current_local_tempname;
extern FILE *current_local_file;
extern uzoff_t current_local_offset;
extern uzoff_t bytes_this_split, bytes_this_entry, bytes_prev_splits;
extern int read_split_archive, split_method, split_bell, noisy_splits;
extern uzoff_t split_size;

// The archive's own directory, in memory.
extern struct zlist far *zfiles;
extern extent zcount;
extern int zipfile_exists;
extern ush zcomlen;
extern char *zcomment;
extern struct flist far **fsort;
extern struct zlist far **zsort;
extern struct zlist far **zusort;
extern struct flist far *found;
extern struct flist far * far * fnxt;
extern extent fcount;

extern struct plist *patterns;
extern unsigned pcount, icount, Rcount;

// ----------------------------------------------------------------- diagnostics

#define diag(where)
#define Assert(cond, msg)
#define Trace(x)
#define Tracev(x)
#define Tracevv(x)
#define Tracec(c, x)
#define Tracecv(c, x)

Task<void> zipmessage_nl OF((ZCONST char *, int));
Task<void> zipmessage OF((ZCONST char *, ZCONST char *));
Task<void> zipwarn OF((ZCONST char *, ZCONST char *));

// Upstream's ziperr() ended the process. Sys::Exit only records a status here
// and a process ends when its root task returns, so nothing can die where it
// stands: the code is recorded and the caller unwinds with it. Every ziperr()
// is therefore a return, which upstream's never had to be.
extern int zip_fatal;            // ZE_OK until something has gone wrong
extern ZCONST char *zip_fatal_h; // what it was about

// Records the code without writing anything. What a plain function does: a
// coroutine's message needs a co_await, and ct_init is not a coroutine.
void zip_fail OF((int, ZCONST char *));

// Records it and writes the message. Only from a coroutine.
Task<void> ziperr_msg OF((int, ZCONST char *));

#define ZIPERR(c, h)               \
    {                              \
        co_await ziperr_msg(c, h); \
        co_return (c);             \
    }
#define ziperr(c, h) ZIPERR(c, h)

// A can't-happen. Nothing here can unwind, so it records and returns and the
// coroutine above notices zip_fatal. A function and not a macro: Result has an
// error() of its own, and a macro would rewrite every call of it.
inline void error(ZCONST char *h)
{
    zip_fail(ZE_LOGIC, h);
}

// ------------------------------------------------------------------ zipup.cpp

int percent OF((uzoff_t, uzoff_t));
Task<int> zipup OF((struct zlist far *));
Task<int> is_seekable OF((FILE *));

// The deflate output sink. trees.cpp emits into out_buf and calls this when it
// fills; upstream wrote straight to the archive, which cannot be done from a
// plain function here. The coroutine layer drains it at each block boundary,
// so it never holds more than one block.
void flush_outbuf OF((char *, unsigned *));

extern String *defl_sink;
Task<int> defl_drain(void);

int seekable OF((void));
extern Task<unsigned>(*read_buf) OF((char *, unsigned int));

// ---------------------------------------------------------------- deflate.cpp

Task<void> lm_init OF((int, ush *));
void lm_free OF((void));
Task<uzoff_t> deflate OF((void));

// ------------------------------------------------------------------ trees.cpp

void ct_init OF((ush *, int *));
int ct_tally OF((int, int));
uzoff_t flush_block OF((char far *, ulg, int));
void bi_init OF((char *, unsigned int, int));

// ---------------------------------------------------------------- zipfile.cpp

Task<int> putlocal OF((struct zlist far *, int));
Task<int> putextended OF((struct zlist far *));
Task<int> putcentral OF((struct zlist far *));
Task<int> putend OF((uzoff_t, uzoff_t, uzoff_t, extent, char *));
struct zlist far *zsearch OF((ZCONST char *));
char *ziptyp OF((char *));
Task<int> trash OF((void));
char *get_extra_field OF((ush, char *, unsigned));
char *copy_nondup_extra_fields OF((char *, unsigned, char *, unsigned, unsigned *));

#define PUTLOCAL_WRITE   0
#define PUTLOCAL_REWRITE 1

// Every write of archive data goes through bfwrite, which is what counts the
// bytes and, once splits arrive, decides which file they land in.
#define zfwrite(b, s, c) bfwrite(b, s, c, BFWRITE_DATA)

// Split archives. bfwrite() is what every header write goes through.
#define BFWRITE_DATA          0
#define BFWRITE_LOCALHEADER   1
#define BFWRITE_CENTRALHEADER 2
#define BFWRITE_HEADER        3

Task<usize> bfwrite OF((ZCONST void *buffer, usize size, usize count, int));
Task<int> close_split OF((ulg, FILE *, char *));

// ----------------------------------------------------------------- fileio.cpp

Task<char *> getnam OF((FILE *));
struct flist far *fexpel OF((struct flist far *));
char *last OF((char *, int));
Task<int> check_dup OF((void));
int filter OF((char *, int));
Task<int> newname OF((char *, int, int));
Task<int> proc_archive_name OF((char *, int));

// Unicode. Names are UTF-8 here already, so the wide-character conversion
// layer is the identity and only the escaping half does anything.
typedef unsigned long zwchar;
int is_ascii_string OF((char *));
char *local_to_utf8_string OF((char *));
char *utf8_to_local_string OF((char *));
char *utf8_to_escape_string OF((char *));
char *local_to_escape_string OF((char *));
char *local_to_display_string OF((char *));
char *wide_char_to_escape_string OF((zwchar));
char *wide_to_local_string OF((zwchar *));
char *wide_to_escape_string OF((zwchar *));
char *wide_to_utf8_string OF((zwchar *));
zwchar *local_to_wide_string OF((char *));
zwchar *utf8_to_wide_string OF((char *));
long ucs4_char_from_utf8 OF((ZCONST char **));

// ------------------------------------------------- long option support
//
// The value is always returned as a string.
#define o_NO_VALUE       0 // this option does not take a value
#define o_REQUIRED_VALUE 1 // this option requires a value
#define o_OPTIONAL_VALUE 2 // value is optional, see get_option()
#define o_VALUE_LIST     3 // this option takes a list of values
#define o_ONE_CHAR_VALUE 4 // next char is value, does not end the short opt
#define o_NUMBER_VALUE   5 // value is an integer, likewise

// A dash following the option, but before any value, sets negated.
#define o_NOT_NEGATABLE 0
#define o_NEGATABLE     1

#define o_NO_OPTION_MATCH -1

// Returned by get_option; do not use these as option IDs.
#define o_NON_OPTION_ARG ((unsigned long)0xFFFF)
#define o_ARG_FILE_ERR   ((unsigned long)0xFFFE)

struct option_struct {
    char *shortopt;          // sequence of chars that is the short option
    char Far *longopt;       // the long option string
    int value_type;          // from above
    int negatable;           // from above
    unsigned long option_ID; // what get_option returns for this option
    char Far *name;          // optional string named on some errors
};

extern struct option_struct far options[];

// The option parser.
Task<unsigned long> get_option OF((char ***pargs, int *argc, int *argnum, int *optchar,
                                   char **value, int *negated, int *first_nonopt_arg,
                                   int *option_num, int recursion_depth));
Task<char **> copy_args OF((char **args, int max_args));
int free_args OF((char **args));
Task<int> insert_arg OF((char ***args, ZCONST char *arg, int insert_at, int free_args));
extern int enable_permute;
extern int doubledash_ends_options;
char *msname OF((char *));
Task<int> destroy OF((char *));
Task<int> replace OF((char *, char *));
char *tempname OF((char *));
ulg dostime OF((int, int, int, int, int, int));
ulg unix2dostime OF((time_t *));
time_t dos2unixtime OF((ulg));
int issymlnk OF((ulg a));
Task<extent> rdsymlnk OF((char *, char *, extent));

// -------------------------------------------------------------------- util.cpp

int fseekable OF((FILE *));
char *isshexp OF((char *));
int shmatch OF((ZCONST char *, ZCONST char *, int));
int abbrevmatch OF((char *, char *, int, int));
void envargs OF((int *, char ***, char *, char *));
void expand_args OF((int *, char ***));
zvoid far **search OF((ZCONST zvoid *, ZCONST zvoid far **, extent,
                       int (*)(ZCONST zvoid *, ZCONST zvoid far *)));
int is_text_buf OF((ZCONST char *buf_ptr, unsigned buf_size));
char *zip_fuzofft OF((uzoff_t, char *, char *));
char *zip_fzofft OF((zoff_t, char *, char *));
Task<int> DisplayNumString OF((FILE * file, uzoff_t i));
int WriteNumString OF((uzoff_t num, char *outstring));
Task<uzoff_t> ReadNumString OF((char *numstring));
void init_upper OF((void));
int namecmp OF((ZCONST char *string1, ZCONST char *string2));

// ---------------------------------------------- what unix/unix.c did, in braam

// The filesystem keeps no mode, no owner and no atime or ctime, so `a` is a
// synthesized Unix mode and iztimes carries mtime three times over.
Task<ulg> filetime OF((char *, ulg *, zoff_t *, iztimes *));
Task<int> set_extra_field OF((struct zlist far *, iztimes *));
Task<int> deletedir OF((char *));
Task<int> procname OF((char *, int));
char *in2ex OF((char *));
char *ex2in OF((char *, int, int *));
int fseekable OF((FILE *));
Task<void> version_local OF((void));
