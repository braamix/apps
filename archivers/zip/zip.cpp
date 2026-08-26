// zip.cpp — zip.c, by Mark Adler. The options, the help, and the driver.
//
// Upstream's main() less the branches that read an existing archive: -d, -u,
// -f, -F, -o, -g and --out are the update path's, and arrive with it. The
// option table is whole — all eighty-one of its live entries — and so is the
// switch that dispatches it.
//
// signal(SIGINT, handler) has no counterpart: a caught signal here abandons
// the call the process is parked on with Err(Intr), which the read path turns
// into ZE_ABORT, so there is no handler to install and none to write.

#include "zip.h"

#include "crc32.h"
#include "proc/rt.h"
#include "revision.h"

#define RETURN(x) co_return (x)
#define FINISH(e) co_return (co_await finish(e))
#define PATHCUT   '/'

local Task<void> license OF((void));
local Task<int> finish OF((int));

// Local option flags
#define DELETE  0
#define ADD     1
#define UPDATE  2
#define FRESHEN 3
#define ARCHIVE 4
local int action       = ADD;  // one of ADD, UPDATE, FRESHEN, DELETE, or ARCHIVE
local int comadd       = 0;    // 1=add comments for new files
local int zipedit      = 0;    // 1=edit zip comment and all file comments
local int latest       = 0;    // 1=set zip file time to time of latest file
local int test         = 0;    // 1=test zip file with unzip -t
local char *unzip_path = NULL; // where to find unzip
local int tempdir      = 0;    // 1=use temp directory (-b)
local int junk_sfx     = 0;    // 1=junk the sfx prefix

// ------------------------------------------------------------------ messages

// Upstream ended the process here. Sys::Exit only records a status and a
// process ends when its root task returns, so the code is kept and the caller
// unwinds with it; zip_fatal_h is what a plain function leaves behind when it
// cannot write a message at all.
void zip_fail(int c, ZCONST char *h)
{
    if (zip_fatal == ZE_OK) {
        zip_fatal   = c;
        zip_fatal_h = h;
    }
}

Task<void> ziperr_msg(int c, ZCONST char *h)
{
    zip_fail(c, h);

    if (mesg_line_started) {
        co_await zfputc('\n', mesg);
        mesg_line_started = 0;
    }
    co_await zfprintf(zstderr, "\nzip error: %s (%s)\n", ZIPERRORS(c), h ? h : "");
    co_await zfflush(zstderr);
}

Task<void> zipwarn(ZCONST char *a, ZCONST char *b)
{
    if (mesg_line_started) {
        co_await zfputc('\n', mesg);
        mesg_line_started = 0;
    }
    co_await zfprintf(zstderr, "\tzip warning: %s%s\n", a, b);
    co_await zfflush(zstderr);
}

Task<void> zipmessage_nl(ZCONST char *a, int nl)
{
    if (noisy) {
        co_await zfprintf(mesg, "%s", a);
        if (nl) {
            co_await zfputc('\n', mesg);
            mesg_line_started = 0;
        } else {
            mesg_line_started = 1;
        }
        co_await zfflush(mesg);
    }
}

Task<void> zipmessage(ZCONST char *a, ZCONST char *b)
{
    if (noisy) {
        if (mesg_line_started) {
            co_await zfputc('\n', mesg);
            mesg_line_started = 0;
        }
        co_await zfprintf(mesg, "%s%s\n", a, b);
        co_await zfflush(mesg);
    }
}

// structure used by add_filter to store filters
struct filterlist_struct {
    char flag;
    char *pattern;
    struct filterlist_struct *next;
};
struct filterlist_struct *filterlist = NULL; // start of list
struct filterlist_struct *lastfilter = NULL; // last filter in list

// structure used by add_filearg to store file arguments
struct filelist_struct {
    char *name;
    struct filelist_struct *next;
};
long filearg_count               = 0;
struct filelist_struct *filelist = NULL; // start of list
struct filelist_struct *lastfile = NULL; // last file in list

local void freeup(void)
// Free all allocations in the 'found' list, the 'zfiles' list and
// the 'patterns' list.
{
    struct flist far *f; // steps through found list
    struct zlist far *z; // pointer to next entry in zfiles list

    for (f = found; f != NULL; f = fexpel(f))
        ;
    while (zfiles != NULL) {
        z = zfiles->nxt;
        if (zfiles->zname && zfiles->zname != zfiles->name)
            free((zvoid *)(zfiles->zname));
        if (zfiles->name)
            free((zvoid *)(zfiles->name));
        if (zfiles->iname)
            free((zvoid *)(zfiles->iname));
        if (zfiles->cext && zfiles->cextra && zfiles->cextra != zfiles->extra)
            free((zvoid *)(zfiles->cextra));
        if (zfiles->ext && zfiles->extra)
            free((zvoid *)(zfiles->extra));
        if (zfiles->com && zfiles->comment)
            free((zvoid *)(zfiles->comment));
        if (zfiles->oname)
            free((zvoid *)(zfiles->oname));
        if (zfiles->uname)
            free((zvoid *)(zfiles->uname));
        if (zfiles->zuname)
            free((zvoid *)(zfiles->zuname));
        if (zfiles->ouname)
            free((zvoid *)(zfiles->ouname));
        farfree((zvoid far *)zfiles);
        zfiles = z;
        zcount--;
    }

    if (patterns != NULL) {
        while (pcount-- > 0) {
            if (patterns[pcount].zname != NULL)
                free((zvoid *)(patterns[pcount].zname));
        }
        free((zvoid *)patterns);
        patterns = NULL;
    }

    // Upstream closed the log file here; nothing opens one yet.
}

local Task<int> add_filter(int flag, char *pattern)
{
    char *iname, *p = NULL;
    FILE *fp;
    struct filterlist_struct *filter = NULL;

    // should never happen
    if (flag != 'R' && flag != 'x' && flag != 'i') {
        ZIPERR(ZE_LOGIC, "bad flag to add_filter");
    }
    if (pattern == NULL) {
        ZIPERR(ZE_LOGIC, "null pattern to add_filter");
    }

    if (pattern[0] == '@') {
        // read file with 1 pattern per line
        if (pattern[1] == '\0') {
            ZIPERR(ZE_PARMS, "missing file after @");
        }
        Result<FILE> opened = Err(Error::NoMemory);
        if (Task<Result<FILE>> t = File::open(Str(pattern + 1, strlen(pattern + 1))))
            opened = co_await t;
        FILE pf = opened.is_ok() ? move(opened.value()) : File::of(0, FileMode::Read);
        fp      = opened.is_ok() ? &pf : NULL;
        if (fp == NULL) {
            zsprintf(errbuf, "%c pattern file '%s'", flag, pattern);
            ZIPERR(ZE_OPEN, errbuf);
        }
        while ((p = co_await getnam(fp)) != NULL) {
            if ((filter = (struct filterlist_struct *)malloc(sizeof(struct filterlist_struct))) ==
                NULL) {
                ZIPERR(ZE_MEM, "adding filter");
            }
            if (filterlist == NULL) {
                // first filter
                filterlist = filter; // start of list
                lastfilter = filter;
            } else {
                lastfilter->next = filter; // link to last filter in list
                lastfilter       = filter;
            }
            iname = ex2in(p, 0, (int *)NULL);
            free(p);
            if (iname != NULL) {
                lastfilter->pattern = in2ex(iname);
                free(iname);
            } else {
                lastfilter->pattern = NULL;
            }
            lastfilter->flag = flag;
            pcount++;
            lastfilter->next = NULL;
        }
        co_await zfclose(fp);
    } else {
        // single pattern
        if ((filter = (struct filterlist_struct *)malloc(sizeof(struct filterlist_struct))) ==
            NULL) {
            ZIPERR(ZE_MEM, "adding filter");
        }
        if (filterlist == NULL) {
            // first pattern
            filterlist = filter; // start of list
            lastfilter = filter;
        } else {
            lastfilter->next = filter; // link to last filter in list
            lastfilter       = filter;
        }
        iname = ex2in(pattern, 0, (int *)NULL);
        if (iname != NULL) {
            lastfilter->pattern = in2ex(iname);
            free(iname);
        } else {
            lastfilter->pattern = NULL;
        }
        lastfilter->flag = flag;
        pcount++;
        lastfilter->next = NULL;
    }

    co_return pcount;
}

local Task<int> filterlist_to_patterns(void)
{
    unsigned i;
    struct filterlist_struct *next = NULL;

    if (pcount == 0) {
        patterns = NULL;
        co_return 0;
    }
    if ((patterns = (struct plist *)malloc((pcount + 1) * sizeof(struct plist))) == NULL) {
        ZIPERR(ZE_MEM, "was creating pattern list");
    }

    for (i = 0; i < pcount && filterlist != NULL; i++) {
        switch (filterlist->flag) {
        case 'i':
            icount++;
            break;
        case 'R':
            Rcount++;
            break;
        }
        patterns[i].select = filterlist->flag;
        patterns[i].zname  = filterlist->pattern;
        next               = filterlist->next;
        free(filterlist);
        filterlist = next;
    }

    co_return pcount;
}

local Task<long> add_name(char *filearg)
{
    char *name                        = NULL;
    struct filelist_struct *fileentry = NULL;

    if ((fileentry = (struct filelist_struct *)malloc(sizeof(struct filelist_struct))) == NULL) {
        ZIPERR(ZE_MEM, "adding file");
    }
    if ((name = (char *)malloc(strlen(filearg) + 1)) == NULL) {
        ZIPERR(ZE_MEM, "adding file");
    }
    strcpy(name, filearg);
    fileentry->next = NULL;
    fileentry->name = name;
    if (filelist == NULL) {
        // first file argument
        filelist = fileentry; // start of list
        lastfile = fileentry;
    } else {
        lastfile->next = fileentry; // link to last filter in list
        lastfile       = fileentry;
    }
    filearg_count++;

    co_return filearg_count;
}

local Task<int> DisplayRunningStats(void)
{
    char tempstrg[100];

    if (mesg_line_started) {
        co_await zfprintf(mesg, "\n");
        mesg_line_started = 0;
    }
    if (logfile_line_started) {
        co_await zfprintf(logfile, "\n");
        logfile_line_started = 0;
    }
    if (display_volume) {
        if (noisy) {
            co_await zfprintf(mesg, "%lu>%lu: ", current_in_disk + 1, current_disk + 1);
            mesg_line_started = 1;
        }
        if (logall) {
            co_await zfprintf(logfile, "%lu>%lu: ", current_in_disk + 1, current_disk + 1);
            logfile_line_started = 1;
        }
    }
    if (display_counts) {
        if (noisy) {
            co_await zfprintf(mesg, "%3ld/%3ld ", files_so_far, files_total - files_so_far);
            mesg_line_started = 1;
        }
        if (logall) {
            co_await zfprintf(logfile, "%3ld/%3ld ", files_so_far, files_total - files_so_far);
            logfile_line_started = 1;
        }
    }
    if (display_bytes) {
        // since file sizes can change as we go, use bytes_so_far from
        // initial scan so all adds up
        WriteNumString(bytes_so_far, tempstrg);
        if (noisy) {
            co_await zfprintf(mesg, "[%4s", tempstrg);
            mesg_line_started = 1;
        }
        if (logall) {
            co_await zfprintf(logfile, "[%4s", tempstrg);
            logfile_line_started = 1;
        }
        if (bytes_total >= bytes_so_far) {
            WriteNumString(bytes_total - bytes_so_far, tempstrg);
            if (noisy)
                co_await zfprintf(mesg, "/%4s] ", tempstrg);
            if (logall)
                co_await zfprintf(logfile, "/%4s] ", tempstrg);
        } else {
            WriteNumString(bytes_so_far - bytes_total, tempstrg);
            if (noisy)
                co_await zfprintf(mesg, "-%4s] ", tempstrg);
            if (logall)
                co_await zfprintf(logfile, "-%4s] ", tempstrg);
        }
    }
    if (noisy)
        co_await zfflush(mesg);
    if (logall)
        co_await zfflush(logfile);

    co_return 0;
}
local Task<void> help(void)
// Print help (along with license info) to stdout.
{
    extent i; // counter for help array

    // help array
    static ZCONST char *text[] = {
        "Zip %s (%s). Usage:",
        "zip [-options] [-b path] [-t mmddyyyy] [-n suffixes] [zipfile list] [-xi list]",
        "  The default action is to add or replace zipfile entries from list, which",
        "  can include the special name - to compress standard input.",
        "  If zipfile and list are omitted, zip compresses stdin to stdout.",
        "  -f   freshen: only changed files  -u   update: only changed or new files",
        "  -d   delete entries in zipfile    -m   move into zipfile (delete OS files)",
        "  -r   recurse into directories     -j   junk (don't record) directory names",
        "  -0   store only                   -l   convert LF to CR LF (-ll CR LF to LF)",
        "  -1   compress faster              -9   compress better",
        "  -q   quiet operation              -v   verbose operation/print version info",
        "  -c   add one-line comments        -z   add zipfile comment",
        "  -@   read names from stdin        -o   make zipfile as old as latest entry",
        "  -x   exclude the following names  -i   include only the following names",
        "  -F   fix zipfile (-FF try harder) -D   do not add directory entries",
        "  -A   adjust self-extracting exe   -J   junk zipfile prefix (unzipsfx)",
        "  -T   test zipfile integrity       -X   eXclude eXtra file attributes",
        "  -y   store symbolic links as the link instead of the referenced file",
// "  -R   PKZIP recursion (see manual)",
#if CRYPT
        "  -e   encrypt                      -n   don't compress these suffixes"
#else
        "  -h   show this help               -n   don't compress these suffixes"
#endif
        ,
        "  -h2  show more help",
        "  "
    };

    for (i = 0; i < sizeof(copyright) / sizeof(char *); i++) {
        co_await zfprintf(zstdout, copyright[i], "zip");
        co_await zfputc_out('\n');
    }
    for (i = 0; i < sizeof(text) / sizeof(char *); i++) {
        co_await zfprintf(zstdout, text[i], VERSION, REVDATE);
        co_await zfputc_out('\n');
    }
}

local Task<void> help_extended(void)
// Print extended help to stdout.
{
    extent i; // counter for help array

    // help array
    static ZCONST char *text[] = {
        "",
        "Extended Help for Zip",
        "",
        "See the Zip Manual for more detailed help",
        "",
        "",
        "Zip stores files in zip archives.  The default action is to add or replace",
        "zipfile entries.",
        "",
        "Basic command line:",
        "  zip options archive_name file file ...",
        "",
        "Some examples:",
        "  Add file.txt to z.zip (create z if needed):      zip z file.txt",
        "  Zip all files in current dir:                    zip z *",
        "  Zip files in current dir and subdirs also:       zip -r z .",
        "",
        "Basic modes:",
        " External modes (selects files from file system):",
        "        add      - add new files/update existing files in archive (default)",
        "  -u    update   - add new files/update existing files only if later date",
        "  -f    freshen  - update existing files only (no files added)",
        "  -FS   filesync - update if date or size changed, delete if no OS match",
        " Internal modes (selects entries in archive):",
        "  -d    delete   - delete files from archive (see below)",
        "  -U    copy     - select files in archive to copy (use with --out)",
        "",
        "Basic options:",
        "  -r        recurse into directories (see Recursion below)",
        "  -m        after archive created, delete original files (move into archive)",
        "  -j        junk directory names (store just file names)",
        "  -q        quiet operation",
        "  -v        verbose operation (just \"zip -v\" shows version information)",
        "  -c        prompt for one-line comment for each entry",
        "  -z        prompt for comment for archive (end with just \".\" line or EOF)",
        "  -@        read names to zip from stdin (one path per line)",
        "  -o        make zipfile as old as latest entry",
        "",
        "",
        "Syntax:",
        "  The full command line syntax is:",
        "",
        "    zip [-shortopts ...] [--longopt ...] [zipfile [path path ...]] [-xi list]",
        "",
        "  Any number of short option and long option arguments are allowed",
        "  (within limits) as well as any number of path arguments for files",
        "  to zip up.  If zipfile exists, the archive is read in.  If zipfile",
        "  is \"-\", stream to stdout.  If any path is \"-\", zip stdin.",
        "",
        "Options and Values:",
        "  For short options that take values, use -ovalue or -o value or -o=value",
        "  For long option values, use either --longoption=value or --longoption value",
        "  For example:",
        "    zip -ds 10 --temp-dir=path zipfile path1 path2 --exclude pattern pattern",
        "  Avoid -ovalue (no space between) to avoid confusion",
        "  In particular, be aware of 2-character options.  For example:",
        "    -d -s is (delete, split size) while -ds is (dot size)",
        "  Usually better to break short options across multiple arguments by function",
        "    zip -r -dbdcds 10m -lilalf logfile archive input_directory -ll",
        "",
        "  All args after just \"--\" arg are read verbatim as paths and not options.",
        "    zip zipfile path path ... -- verbatimpath verbatimpath ...",
        "  Use -nw to also disable wildcards, so paths are read literally:",
        "    zip zipfile -nw -- \"-leadingdashpath\" \"a[path].c\" \"path*withwildcard\"",
        "  You may still have to escape or quote arguments to avoid shell expansion",
        "",
        "Wildcards:",
        "  Internally zip supports the following wildcards:",
        "    ?       (or %% or #, depending on OS) matches any single character",
        "    *       matches any number of characters, including zero",
        "    [list]  matches char in list (regex), can do range [ac-f], all but [!bf]",
        "  If port supports [], must escape [ as [[] or use -nw to turn off wildcards",
        "  For shells that expand wildcards, escape (\\* or \"*\") so zip can recurse",
        "    zip zipfile -r . -i \"*.h\"",
        "",
        "  Normally * crosses dir bounds in path, e.g. 'a*b' can match 'ac/db'.  If",
        "   -ws option used, * does not cross dir bounds but ** does",
        "",
        "  For DOS and Windows, [list] is now disabled unless the new option",
        "  -RE       enable [list] (regular expression) matching",
        "  is used to avoid problems with file paths containing \"[\" and \"]\":",
        "    zip files_ending_with_number -RE foo[0-9].c",
        "",
        "Include and Exclude:",
        "  -i pattern pattern ...   include files that match a pattern",
        "  -x pattern pattern ...   exclude files that match a pattern",
        "  Patterns are paths with optional wildcards and match paths as stored in",
        "  archive.  Exclude and include lists end at next option, @, or end of line.",
        "    zip -x pattern pattern @ zipfile path path ...",
        "",
        "Case matching:",
        "  On most OS the case of patterns must match the case in the archive, unless",
        "  the -ic option is used.",
        "  -ic       ignore case of archive entries",
        "  This option not available on case-sensitive file systems.  On others, case",
        "  ignored when matching files on file system but matching against archive",
        "  entries remains case sensitive for modes -f (freshen), -U (archive copy),",
        "  and -d (delete) because archive paths are always case sensitive.  With",
        "  -ic, all matching ignores case, but it's then possible multiple archive",
        "  entries that differ only in case will match.",
        "",
        "End Of Line Translation (text files only):",
        "  -l        change CR or LF (depending on OS) line end to CR LF (Unix->Win)",
        "  -ll       change CR LF to CR or LF (depending on OS) line end (Win->Unix)",
        "  If first buffer read from file contains binary the translation is skipped",
        "",
        "Recursion:",
        "  -r        recurse paths, include files in subdirs:  zip -r a path path ...",
        "  -R        recurse current dir and match patterns:   zip -R a ptn ptn ...",
        "  Use -i and -x with either to include or exclude paths",
        "  Path root in archive starts at current dir, so if /a/b/c/file and",
        "   current dir is /a/b, 'zip -r archive .' puts c/file in archive",
        "",
        "Date filtering:",
        "  -t date   exclude before (include files modified on this date and later)",
        "  -tt date  include before (include files modified before date)",
        "  Can use both at same time to set a date range",
        "  Dates are mmddyyyy or yyyy-mm-dd",
        "",
        "Deletion, File Sync:",
        "  -d        delete files",
        "  Delete archive entries matching internal archive paths in list",
        "    zip archive -d pattern pattern ...",
        "  Can use -t and -tt to select files in archive, but NOT -x or -i, so",
        "    zip archive -d \"*\" -t 2005-12-27",
        "  deletes all files from archive.zip with date of 27 Dec 2005 and later",
        "  Note the * (escape as \"*\" on Unix) to select all files in archive",
        "",
        "  -FS       file sync",
        "  Similar to update, but files updated if date or size of entry does not",
        "  match file on OS.  Also deletes entry from archive if no matching file",
        "  on OS.",
        "    zip archive_to_update -FS -r dir_used_before",
        "  Result generally same as creating new archive, but unchanged entries",
        "  are copied instead of being read and compressed so can be faster.",
        "      WARNING:  -FS deletes entries so make backup copy of archive first",
        "",
        "Compression:",
        "  -0        store files (no compression)",
        "  -1 to -9  compress fastest to compress best (default is 6)",
        "  -Z cm     set compression method to cm:",
        "              store   - store without compression, same as option -0",
        "              deflate - original zip deflate, same as -1 to -9 (default)",
        "            if bzip2 is enabled:",
        "              bzip2 - use bzip2 compression (need modern unzip)",
        "",
        "Encryption:",
        "  -e        use standard (weak) PKZip 2.0 encryption, prompt for password",
        "  -P pswd   use standard encryption, password is pswd",
        "",
        "Splits (archives created as a set of split files):",
        "  -s ssize  create split archive with splits of size ssize, where ssize nm",
        "              n number and m multiplier (kmgt, default m), 100k -> 100 kB",
        "  -sp       pause after each split closed to allow changing disks",
        "      WARNING:  Archives created with -sp use data descriptors and should",
        "                work with most unzips but may not work with some",
        "  -sb       ring bell when pause",
        "  -sv       be verbose about creating splits",
        "      Split archives CANNOT be updated, but see --out and Copy Mode below",
        "",
        "Using --out (output to new archive):",
        "  --out oa  output to new archive oa",
        "  Instead of updating input archive, create new output archive oa.",
        "  Result is same as without --out but in new archive.  Input archive",
        "  unchanged.",
        "      WARNING:  --out ALWAYS overwrites any existing output file",
        "  For example, to create new_archive like old_archive but add newfile1",
        "  and newfile2:",
        "    zip old_archive newfile1 newfile2 --out new_archive",
        "  Cannot update split archive, so use --out to out new archive:",
        "    zip in_split_archive newfile1 newfile2 --out out_split_archive",
        "  If input is split, output will default to same split size",
        "  Use -s=0 or -s- to turn off splitting to convert split to single file:",
        "    zip in_split_archive -s 0 --out out_single_file_archive",
        "      WARNING:  If overwriting old split archive but need less splits,",
        "                old splits not overwritten are not needed but remain",
        "",
        "Copy Mode (copying from archive to archive):",
        "  -U        (also --copy) select entries in archive to copy (reverse delete)",
        "  Copy Mode copies entries from old to new archive with --out and is used by",
        "  zip when either no input files on command line or -U (--copy) used.",
        "    zip inarchive --copy pattern pattern ... --out outarchive",
        "  To copy only files matching *.c into new archive, excluding foo.c:",
        "    zip old_archive --copy \"*.c\" --out new_archive -x foo.c",
        "  If no input files and --out, copy all entries in old archive:",
        "    zip old_archive --out new_archive",
        "",
        "Streaming and FIFOs:",
        "  prog1 | zip -ll z -      zip output of prog1 to zipfile z, converting CR LF",
        "  zip - -R \"*.c\" | prog2   zip *.c files in current dir and stream to prog2 ",
        "  prog1 | zip | prog2      zip in pipe with no in or out acts like zip - -",
        "  If Zip is Zip64 enabled, streaming stdin creates Zip64 archives by default",
        "   that need PKZip 4.5 unzipper like UnZip 6.0",
        "  WARNING:  Some archives created with streaming use data descriptors and",
        "            should work with most unzips but may not work with some",
        "  Can use -fz- to turn off Zip64 if input not large (< 4 GB):",
        "    prog_with_small_output | zip archive -fz-",
        "",
        "  Zip now can read Unix FIFO (named pipes).  Off by default to prevent zip",
        "  from stopping unexpectedly on unfed pipe, use -FI to enable:",
        "    zip -FI archive fifo",
        "",
        "Dots, counts:",
        "  -db       display running count of bytes processed and bytes to go",
        "              (uncompressed size, except delete and copy show stored size)",
        "  -dc       display running count of entries done and entries to go",
        "  -dd       display dots every 10 MB (or dot size) while processing files",
        "  -dg       display dots globally for archive instead of for each file",
        "    zip -qdgds 10m   will turn off most output except dots every 10 MB",
        "  -ds siz   each dot is siz processed where siz is nm as splits (0 no dots)",
        "  -du       display original uncompressed size for each entry as added",
        "  -dv       display volume (disk) number in format in_disk>out_disk",
        "  Dot size is approximate, especially for dot sizes less than 1 MB",
        "  Dot options don't apply to Scanning files dots (dot/2sec) (-q turns off)",
        "",
        "Logging:",
        "  -lf path  open file at path as logfile (overwrite existing file)",
        "  -la       append to existing logfile",
        "  -li       include info messages (default just warnings and errors)",
        "",
        "Testing archives:",
        "  -T        test completed temp archive with unzip before updating archive",
        "  -TT cmd   use command cmd instead of 'unzip -tqq' to test archive",
        "             On Unix, to use unzip in current directory, could use:",
        "               zip archive file1 file2 -T -TT \"./unzip -tqq\"",
        "             In cmd, {} replaced by temp archive path, else temp appended.",
        "             The return code is checked for success (0 on Unix)",
        "",
        "Fixing archives:",
        "  -F        attempt to fix a mostly intact archive (try this first)",
        "  -FF       try to salvage what can (may get more but less reliable)",
        "  Fix options copy entries from potentially bad archive to new archive.",
        "  -F tries to read archive normally and copy only intact entries, while",
        "  -FF tries to salvage what can and may result in incomplete entries.",
        "  Must use --out option to specify output archive:",
        "    zip -F bad.zip --out fixed.zip",
        "  Use -v (verbose) with -FF to see details:",
        "    zip reallybad.zip -FF -v --out fixed.zip",
        "  Currently neither option fixes bad entries, as from text mode ftp get.",
        "",
        "Difference mode:",
        "  -DF       (also --dif) only include files that have changed or are",
        "             new as compared to the input archive",
        "  Difference mode can be used to create incremental backups.  For example:",
        "    zip --dif full_backup.zip -r somedir --out diff.zip",
        "  will store all new files, as well as any files in full_backup.zip where",
        "  either file time or size have changed from that in full_backup.zip,",
        "  in new diff.zip.  Output archive not excluded automatically if exists,",
        "  so either use -x to exclude it or put outside what is being zipped.",
        "",
        "DOS Archive bit (Windows only):",
        "  -AS       include only files with the DOS Archive bit set",
        "  -AC       after archive created, clear archive bit of included files",
        "      WARNING: Once the archive bits are cleared they are cleared",
        "               Use -T to test the archive before the bits are cleared",
        "               Can also use -sf to save file list before zipping files",
        "",
        "Show files:",
        "  -sf       show files to operate on and exit (-sf- logfile only)",
        "  -su       as -sf but show escaped UTF-8 Unicode names also if exist",
        "  -sU       as -sf but show escaped UTF-8 Unicode names instead",
        "  Any character not in the current locale is escaped as #Uxxxx, where x",
        "  is hex digit, if 16-bit code is sufficient, or #Lxxxxxx if 24-bits",
        "  are needed.  If add -UN=e, Zip escapes all non-ASCII characters.",
        "",
        "Unicode:",
        "  If compiled with Unicode support, Zip stores UTF-8 path of entries.",
        "  This is backward compatible.  Unicode paths allow better conversion",
        "  of entry names between different character sets.",
        "",
        "  New Unicode extra field includes checksum to verify Unicode path",
        "  goes with standard path for that entry (as utilities like ZipNote",
        "  can rename entries).  If these do not match, use below options to",
        "  set what Zip does:",
        "      -UN=Quit     - if mismatch, exit with error",
        "      -UN=Warn     - if mismatch, warn, ignore UTF-8 (default)",
        "      -UN=Ignore   - if mismatch, quietly ignore UTF-8",
        "      -UN=No       - ignore any UTF-8 paths, use standard paths for all",
        "  An exception to -UN=N are entries with new UTF-8 bit set (instead",
        "  of using extra fields).  These are always handled as Unicode.",
        "",
        "  Normally Zip escapes all chars outside current char set, but leaves",
        "  as is supported chars, which may not be OK in path names.  -UN=Escape",
        "  escapes any character not ASCII:",
        "    zip -sU -UN=e archive",
        "  Can use either normal path or escaped Unicode path on command line",
        "  to match files in archive.",
        "",
        "  Zip now stores UTF-8 in entry path and comment fields on systems",
        "  where UTF-8 char set is default, such as most modern Unix, and",
        "  and on other systems in new extra fields with escaped versions in",
        "  entry path and comment fields for backward compatibility.",
        "  Option -UN=UTF8 will force storing UTF-8 in entry path and comment",
        "  fields:",
        "      -UN=UTF8     - store UTF-8 in entry path and comment fields",
        "  This option can be useful for multi-byte char sets on Windows where",
        "  escaped paths and comments can be too long to be valid as the UTF-8",
        "  versions tend to be shorter.",
        "",
        "  Only UTF-8 comments on UTF-8 native systems supported.  UTF-8 comments",
        "  for other systems planned in next release.",
        "",
        "Self extractor:",
        "  -A        Adjust offsets - a self extractor is created by prepending",
        "             the extractor executable to archive, but internal offsets",
        "             are then off.  Use -A to fix offsets.",
        "  -J        Junk sfx - removes prepended extractor executable from",
        "             self extractor, leaving a plain zip archive.",
        "",
        "More option highlights (see manual for additional options and details):",
        "  -b dir    when creating or updating archive, create the temp archive in",
        "             dir, which allows using seekable temp file when writing to a",
        "             write once CD, such archives compatible with more unzips",
        "             (could require additional file copy if on another device)",
        "  -MM       input patterns must match at least one file and matched files",
        "             must be readable or exit with OPEN error and abort archive",
        "             (without -MM, both are warnings only, and if unreadable files",
        "             are skipped OPEN error (18) returned after archive created)",
        "  -nw       no wildcards (wildcards are like any other character)",
        "  -sc       show command line arguments as processed and exit",
        "  -sd       show debugging as Zip does each step",
        "  -so       show all available options on this system",
        "  -X        default=strip old extra fields, -X- keep old, -X strip most",
        "  -ws       wildcards don't span directory boundaries in paths",
        ""
    };

    for (i = 0; i < sizeof(text) / sizeof(char *); i++) {
        co_await zfprintf(zstdout, text[i]);
        co_await zfputc_out('\n');
    }
}

local Task<void> version_info(void)
// Print verbose info about program version and compile time options
// to stdout.
{
    extent i; // counter in text arrays
    char *envptr;

    // Bzip2 option string storage (with version).

    // Options info array
    static ZCONST char *comp_opts[] = {
        "DYN_ALLOC",
        "SYMLINK_SUPPORT      (symbolic links supported)",
        "LARGE_FILE_SUPPORT   (can read and write large files on file system)",
        "ZIP64_SUPPORT        (use Zip64 to store large files in archives)",
        "UNICODE_SUPPORT      (store and read UTF-8 Unicode paths)",

        "STORE_UNIX_UIDs_GIDs (store UID/GID sizes/values using new extra field)",
        "UIDGID_16BIT         (old Unix 16-bit UID/GID extra field also used)",

        NULL
    };

    static ZCONST char *zipenv_names[] = { "ZIP", "ZIPOPT" };

    for (i = 0; i < sizeof(copyright) / sizeof(char *); i++) {
        co_await zfprintf(zstdout, copyright[i], "zip");
        co_await zfputc_out('\n');
    }

    for (i = 0; i < sizeof(versinfolines) / sizeof(char *); i++) {
        co_await zfprintf(zstdout, versinfolines[i], "Zip", VERSION, REVDATE);
        co_await zfputc_out('\n');
    }

    version_local();

    co_await zfputs_nl("Zip special compilation options:");
#if WSIZE != 0x8000
    co_await zfprintf(zstdout, "\tWSIZE=%u\n", WSIZE);
#endif

    // Fill in bzip2 version.  (32-char limit valid as of bzip 1.0.3.)

    for (i = 0; (int)i < (int)(sizeof(comp_opts) / sizeof(char *) - 1); i++) {
        co_await zfprintf(zstdout, "\t%s\n", comp_opts[i]);
    }
#if CRYPT
    co_await zfprintf(zstdout, "\t[encryption, version %d.%d%s of %s] (modified for Zip 3)\n\n",
                      CR_MAJORVER, CR_MINORVER, CR_BETA_VER, CR_VERSION_DATE);
    for (i = 0; i < sizeof(cryptnote) / sizeof(char *); i++) {
        co_await zfprintf(zstdout, cryptnote[i]);
        co_await zfputc_out('\n');
    }
    ++i; // crypt support means there IS at least one compilation option
#endif   // CRYPT
    if (i == 0)
        co_await zfputs_nl("\t[none]");

    co_await zfputs_nl("\nZip environment options:");
    for (i = 0; i < sizeof(zipenv_names) / sizeof(char *); i++) {
        envptr = getenv(zipenv_names[i]);
        co_await zfprintf(zstdout, "%16s:  %s\n", zipenv_names[i],
                          ((envptr == (char *)NULL || *envptr == 0) ? "[none]" : envptr));
    }
}

// The options table. Each row is:
//   shortopt, longopt, value_type, negatable, ID, name
// value_type and negatable are zip.h's constants; name is the short
// description -so lists and some errors name.

// Most option IDs are set to the shortopt char.  For
// multichar short options set to arbitrary unused constant.
#define o_AC  0x101
#define o_AS  0x102
#define o_C2  0x103
#define o_C5  0x104
#define o_db  0x105
#define o_dc  0x106
#define o_dd  0x107
#define o_des 0x108
#define o_df  0x109
#define o_DF  0x110
#define o_dg  0x111
#define o_ds  0x112
#define o_du  0x113
#define o_dv  0x114
#define o_FF  0x115
#define o_FI  0x116
#define o_FS  0x117
#define o_h2  0x118
#define o_ic  0x119
#define o_jj  0x120
#define o_la  0x121
#define o_lf  0x122
#define o_li  0x123
#define o_ll  0x124
#define o_mm  0x125
#define o_MM  0x126
#define o_nw  0x127
#define o_RE  0x128
#define o_sb  0x129
#define o_sc  0x130
#define o_sd  0x131
#define o_sf  0x132
#define o_so  0x133
#define o_sp  0x134
#define o_su  0x135
#define o_sU  0x136
#define o_sv  0x137
#define o_tt  0x138
#define o_TT  0x139
#define o_UN  0x140
#define o_ve  0x141
#define o_VV  0x142
#define o_ws  0x143
#define o_ww  0x144
#define o_z64 0x145

// the below is mainly from the old main command line
// switch with a few changes
struct option_struct far options[] = {
    // short longopt        value_type        negatable        ID    name
    { "0", "store", o_NO_VALUE, o_NOT_NEGATABLE, '0', "store" },
    { "1", "compress-1", o_NO_VALUE, o_NOT_NEGATABLE, '1', "compress 1" },
    { "2", "compress-2", o_NO_VALUE, o_NOT_NEGATABLE, '2', "compress 2" },
    { "3", "compress-3", o_NO_VALUE, o_NOT_NEGATABLE, '3', "compress 3" },
    { "4", "compress-4", o_NO_VALUE, o_NOT_NEGATABLE, '4', "compress 4" },
    { "5", "compress-5", o_NO_VALUE, o_NOT_NEGATABLE, '5', "compress 5" },
    { "6", "compress-6", o_NO_VALUE, o_NOT_NEGATABLE, '6', "compress 6" },
    { "7", "compress-7", o_NO_VALUE, o_NOT_NEGATABLE, '7', "compress 7" },
    { "8", "compress-8", o_NO_VALUE, o_NOT_NEGATABLE, '8', "compress 8" },
    { "9", "compress-9", o_NO_VALUE, o_NOT_NEGATABLE, '9', "compress 9" },
    { "A", "adjust-sfx", o_NO_VALUE, o_NOT_NEGATABLE, 'A', "adjust self extractor offsets" },
    { "b", "temp-path", o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'b', "dir to use for temp archive" },
    { "c", "entry-comments", o_NO_VALUE, o_NOT_NEGATABLE, 'c', "add comments for each entry" },
    { "d", "delete", o_NO_VALUE, o_NOT_NEGATABLE, 'd', "delete entries from archive" },
    { "db", "display-bytes", o_NO_VALUE, o_NEGATABLE, o_db, "display running bytes" },
    { "dc", "display-counts", o_NO_VALUE, o_NEGATABLE, o_dc, "display running file count" },
    { "dd", "display-dots", o_NO_VALUE, o_NEGATABLE, o_dd, "display dots as process each file" },
    { "dg", "display-globaldots", o_NO_VALUE, o_NEGATABLE, o_dg,
      "display dots for archive instead of files" },
    { "ds", "dot-size", o_REQUIRED_VALUE, o_NOT_NEGATABLE, o_ds,
      "set progress dot size - default 10M bytes" },
    { "du", "display-usize", o_NO_VALUE, o_NEGATABLE, o_du, "display uncompressed size in bytes" },
    { "dv", "display-volume", o_NO_VALUE, o_NEGATABLE, o_dv, "display volume (disk) number" },
    { "D", "no-dir-entries", o_NO_VALUE, o_NOT_NEGATABLE, 'D',
      "no entries for dirs themselves (-x */)" },
    { "DF", "difference-archive", o_NO_VALUE, o_NOT_NEGATABLE, o_DF,
      "create diff archive with changed/new files" },
    { "e", "encrypt", o_NO_VALUE, o_NOT_NEGATABLE, 'e', "encrypt entries, ask for password" },
    { "F", "fix", o_NO_VALUE, o_NOT_NEGATABLE, 'F', "fix mostly intact archive (try first)" },
    { "FF", "fixfix", o_NO_VALUE, o_NOT_NEGATABLE, o_FF,
      "try harder to fix archive (not as reliable)" },
    { "FI", "fifo", o_NO_VALUE, o_NEGATABLE, o_FI, "read Unix FIFO (zip will wait on open pipe)" },
    { "FS", "filesync", o_NO_VALUE, o_NOT_NEGATABLE, o_FS,
      "add/delete entries to make archive match OS" },
    { "f", "freshen", o_NO_VALUE, o_NOT_NEGATABLE, 'f', "freshen existing archive entries" },
    { "fd", "force-descriptors", o_NO_VALUE, o_NOT_NEGATABLE, o_des,
      "force data descriptors as if streaming" },
    { "fz", "force-zip64", o_NO_VALUE, o_NEGATABLE, o_z64,
      "force use of Zip64 format, negate prevents" },
    { "g", "grow", o_NO_VALUE, o_NOT_NEGATABLE, 'g', "grow existing archive instead of replace" },
    { "h", "help", o_NO_VALUE, o_NOT_NEGATABLE, 'h', "help" },
    { "H", "", o_NO_VALUE, o_NOT_NEGATABLE, 'h', "help" },
    { "?", "", o_NO_VALUE, o_NOT_NEGATABLE, 'h', "help" },
    { "h2", "more-help", o_NO_VALUE, o_NOT_NEGATABLE, o_h2, "extended help" },
    { "i", "include", o_VALUE_LIST, o_NOT_NEGATABLE, 'i', "include only files matching patterns" },
    { "j", "junk-paths", o_NO_VALUE, o_NOT_NEGATABLE, 'j',
      "strip paths and just store file names" },
    { "J", "junk-sfx", o_NO_VALUE, o_NOT_NEGATABLE, 'J', "strip self extractor from archive" },
    { "k", "DOS-names", o_NO_VALUE, o_NOT_NEGATABLE, 'k', "force use of 8.3 DOS names" },
    { "l", "to-crlf", o_NO_VALUE, o_NOT_NEGATABLE, 'l', "convert text file line ends - LF->CRLF" },
    { "ll", "from-crlf", o_NO_VALUE, o_NOT_NEGATABLE, o_ll,
      "convert text file line ends - CRLF->LF" },
    { "lf", "logfile-path", o_REQUIRED_VALUE, o_NOT_NEGATABLE, o_lf,
      "log to log file at path (default overwrite)" },
    { "la", "log-append", o_NO_VALUE, o_NEGATABLE, o_la, "append to existing log file" },
    { "li", "log-info", o_NO_VALUE, o_NEGATABLE, o_li, "include informational messages in log" },
    { "L", "license", o_NO_VALUE, o_NOT_NEGATABLE, 'L', "display license" },
    { "m", "move", o_NO_VALUE, o_NOT_NEGATABLE, 'm', "add files to archive then delete files" },
    { "mm", "", o_NO_VALUE, o_NOT_NEGATABLE, o_mm, "not used" },
    { "MM", "must-match", o_NO_VALUE, o_NOT_NEGATABLE, o_MM,
      "error if in file not matched/not readable" },
    { "n", "suffixes", o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'n',
      "suffixes to not compress: .gz:.zip" },
    { "nw", "no-wild", o_NO_VALUE, o_NOT_NEGATABLE, o_nw, "no wildcards during add or update" },
    { "o", "latest-time", o_NO_VALUE, o_NOT_NEGATABLE, 'o',
      "use latest entry time as archive time" },
    { "O", "output-file", o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'O',
      "set out zipfile different than in zipfile" },
    { "p", "paths", o_NO_VALUE, o_NOT_NEGATABLE, 'p', "store paths" },
    { "P", "password", o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'P',
      "encrypt entries, option value is password" },
    { "q", "quiet", o_NO_VALUE, o_NOT_NEGATABLE, 'q', "quiet" },
    { "r", "recurse-paths", o_NO_VALUE, o_NOT_NEGATABLE, 'r', "recurse down listed paths" },
    { "R", "recurse-patterns", o_NO_VALUE, o_NOT_NEGATABLE, 'R',
      "recurse current dir and match patterns" },
    { "RE", "regex", o_NO_VALUE, o_NOT_NEGATABLE, o_RE, "allow [list] matching (regex)" },
    { "s", "split-size", o_REQUIRED_VALUE, o_NOT_NEGATABLE, 's',
      "do splits, set split size (-s=0 no splits)" },
    { "sp", "split-pause", o_NO_VALUE, o_NOT_NEGATABLE, o_sp,
      "pause while splitting to select destination" },
    { "sv", "split-verbose", o_NO_VALUE, o_NOT_NEGATABLE, o_sv,
      "be verbose about creating splits" },
    { "sb", "split-bell", o_NO_VALUE, o_NOT_NEGATABLE, o_sb,
      "when pause for next split ring bell" },
    { "sc", "show-command", o_NO_VALUE, o_NOT_NEGATABLE, o_sc, "show command line" },
    { "sd", "show-debug", o_NO_VALUE, o_NOT_NEGATABLE, o_sd, "show debug" },
    { "sf", "show-files", o_NO_VALUE, o_NEGATABLE, o_sf, "show files to operate on and exit" },
    { "so", "show-options", o_NO_VALUE, o_NOT_NEGATABLE, o_so, "show options" },
    { "su", "show-unicode", o_NO_VALUE, o_NEGATABLE, o_su, "as -sf but also show escaped Unicode" },
    { "sU", "show-just-unicode", o_NO_VALUE, o_NEGATABLE, o_sU,
      "as -sf but only show escaped Unicode" },
    { "t", "from-date", o_REQUIRED_VALUE, o_NOT_NEGATABLE, 't', "exclude before date" },
    { "tt", "before-date", o_REQUIRED_VALUE, o_NOT_NEGATABLE, o_tt, "include before date" },
    { "T", "test", o_NO_VALUE, o_NOT_NEGATABLE, 'T', "test updates before replacing archive" },
    { "TT", "unzip-command", o_REQUIRED_VALUE, o_NOT_NEGATABLE, o_TT,
      "unzip command to use, name is added to end" },
    { "u", "update", o_NO_VALUE, o_NOT_NEGATABLE, 'u', "update existing entries and add new" },
    { "U", "copy-entries", o_NO_VALUE, o_NOT_NEGATABLE, 'U',
      "select from archive instead of file system" },
    { "UN", "unicode", o_REQUIRED_VALUE, o_NOT_NEGATABLE, o_UN,
      "UN=quit, warn, ignore, no, escape" },
    { "v", "verbose", o_NO_VALUE, o_NOT_NEGATABLE, 'v', "display additional information" },
    { "", "version", o_NO_VALUE, o_NOT_NEGATABLE, o_ve,
      "(if no other args) show version information" },
    { "ws", "wild-stop-dirs", o_NO_VALUE, o_NOT_NEGATABLE, o_ws,
      "* stops at /, ** includes any /" },
    { "x", "exclude", o_VALUE_LIST, o_NOT_NEGATABLE, 'x', "exclude files matching patterns" },
    // {"X",  "no-extra",    o_NO_VALUE,       o_NOT_NEGATABLE, 'X',  "no extra"},
    { "X", "strip-extra", o_NO_VALUE, o_NEGATABLE, 'X',
      "-X- keep all ef, -X strip but critical ef" },
    { "y", "symlinks", o_NO_VALUE, o_NOT_NEGATABLE, 'y', "store symbolic links" },
    { "z", "archive-comment", o_NO_VALUE, o_NOT_NEGATABLE, 'z', "ask for archive comment" },
    { "Z", "compression-method", o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'Z', "compression method" },
    { "@", "names-stdin", o_NO_VALUE, o_NOT_NEGATABLE, '@',
      "get file names from stdin, one per line" },
    // the end of the list
    { NULL, NULL, o_NO_VALUE, o_NOT_NEGATABLE, 0, NULL } // end has option_ID = 0
};
// Print license information to stdout.
local Task<void> license(void)
{
    extent i; // counter for copyright array

    for (i = 0; i < sizeof(swlicense) / sizeof(char *); i++)
        co_await zfputs_nl(swlicense[i]);
}

// Process -o and -m (if specified), free up malloc'ed stuff, and return the
// code e.
//
// -o set the archive's time to that of the latest entry in it. Nothing here
// can set an mtime — touch_path moves one to now and there is no other setter
// — so it says so and leaves the archive alone.
local Task<int> finish(int e)
{
    int r;

    if (latest && zipfile && strcmp(zipfile, "-")) {
        co_await zipwarn("cannot set the archive's time: ", "the filesystem has no way to set one");
    }

    // If dispose, delete all files in the zip file that were added.
    if (dispose) {
        diag("deleting files");
        if (zfiles == NULL) {
            co_await zipwarn("zip file is empty, nothing to delete", "");
        } else {
            if ((r = co_await trash()) != ZE_OK)
                co_await ziperr_msg(r, "was deleting moved files and directories");
        }
    }

    co_return e;
}

// setup for writing zip file on stdout
local Task<int> zipstdout(void)
{
    mesg = zstderr;

    Result<TtyInfo> t = Err(Error::NoMemory);
    if (Task<Result<TtyInfo>> tk = tty_of(SYS_STDOUT))
        t = co_await tk;
    if (t.is_ok() && t.value().console)
        ZIPERR(ZE_PARMS, "cannot write zip file to terminal")

    if ((zipfile = (char *)malloc(4)) == NULL)
        ZIPERR(ZE_MEM, "was processing arguments")
    strcpy(zipfile, "-");
    co_return ZE_OK;
}

// ------------------------------------------------------------------- the run
//
// Upstream's main(), less the branches that read an existing archive: those
// are readzipfile() and zipcopy()'s, and they arrive with the update path.
// The order is upstream's, and every step it leaves out is marked.

local Task<int> zipmain(Args argv)
{
    int d;                 // true if just adding to a zip file
    struct flist far *f;   // steps through found linked list
    int i;                 // arg counter, root directory flag
    int kk;                // next arg type
    uzoff_t c;             // start of central directory
    uzoff_t t;             // length of central directory
    zoff_t k;              // marked counter, comment size, entry count
    uzoff_t n;             // total of entry len's
    int o;                 // true if there were any ZE_OPEN errors
    int r;                 // temporary variable
    int s;                 // flag to read names from stdin
    uzoff_t csize;         // compressed file size for stats
    uzoff_t usize;         // uncompressed file size for stats
    int first_listarg = 0; // index of first arg of the file list
    struct zlist far *z;   // steps through zfiles linked list
    int bad_open_is_error = 0;
    struct filelist_struct *filearg;

    unsigned long option; // option ID returned by get_option
    int argcnt  = 0;
    int argnum  = 0;
    int optchar = 0;
    char *value = NULL;
    int negated = 0;
    int fna     = 0;
    int optnum  = 0;

    int show_options    = 0;
    int show_what_doing = 0;
    int show_args       = 0;
    int seen_doubledash = 0;
    int key_needed      = 0;
    int have_out        = 0;
    char **args         = NULL;

    FILE *comment_stream = NULL;
    char *e              = NULL;
    char *pp             = NULL;

    mesg = zstdout;
    init_upper();
    crc_32_tab = get_crc_table();
    co_await zclock_init();

    action         = ADD;
    comment_stream = zstderr;

    // Copy the arguments so envargs() and the parser may rewrite them. Str is
    // not NUL-terminated, so this is a copy either way — and args[0] is the
    // program name, which get_option skips as argv[0].
    argcnt = (int)argv.size();
    if ((args = (char **)malloc((argcnt + 2) * sizeof(char *))) == NULL)
        ZIPERR(ZE_MEM, "was copying args")
    for (i = 0; i < argcnt; i++) {
        if ((args[i] = (char *)malloc(argv[i].size() + 1)) == NULL)
            ZIPERR(ZE_MEM, "was copying args")
        memcpy(args[i], argv[i].data(), argv[i].size());
        args[i][argv[i].size()] = 0;
    }
    args[argcnt] = NULL;

    // ZIPOPT and ZIP, in that order, in front of the command line.
    envargs(&argcnt, &args, "ZIPOPT", "ZIP");
    expand_args(&argcnt, &args);

    zipfile = tempzip = NULL;
    y                 = NULL;
    d                 = 0;
    kk                = 0;
    s                 = 0;

    while ((option = co_await get_option(&args, &argcnt, &argnum, &optchar, &value, &negated, &fna,
                                         &optnum, 0))) {
        switch (option) {
        case '0':
            method = STORE;
            level  = 0;
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            // Set the compression efficacy
            level = (int)option - '0';
            break;
        case 'A': // Adjust unzipsfx'd zipfile:  adjust offsets only
            adjust = 1;
            break;
        case 'b': // Specify path for temporary file
            tempdir = 1;
            tempath = value;
            break;
        case 'c': // Add comments for new files in zip file
            comadd = 1;
            break;

            // -C, -C2, and -C5 are with -V

        case 'd': // Delete files from zip file
            if (action != ADD) {
                ZIPERR(ZE_PARMS, "specify just one action");
            }
            action = DELETE;
            break;
        case o_db:
            if (negated)
                display_bytes = 0;
            else
                display_bytes = 1;
            break;
        case o_dc:
            if (negated)
                display_counts = 0;
            else
                display_counts = 1;
            break;
        case o_dd:
            // display dots
            display_globaldots = 0;
            if (negated) {
                dot_count = 0;
            } else {
                // set default dot size if dot_size not set (dot_count = 0)
                if (dot_count == 0)
                    // default to 10 MB
                    dot_size = 10 * 0x100000;
                dot_count = -1;
            }
            break;
        case o_dg:
            // display dots globally for archive instead of for each file
            if (negated) {
                display_globaldots = 0;
            } else {
                display_globaldots = 1;
                // set default dot size if dot_size not set (dot_count = 0)
                if (dot_count == 0)
                    dot_size = 10 * 0x100000;
                dot_count = -1;
            }
            break;
        case o_ds:
            // input dot_size is now actual dot size to account for
            // different buffer sizes
            if (value == NULL)
                dot_size = 10 * 0x100000;
            else if (value[0] == '\0') {
                // default to 10 MB
                dot_size = 10 * 0x100000;
                free(value);
            } else {
                dot_size = (zoff_t)(co_await ReadNumString(value));
                if (dot_size == (zoff_t)-1) {
                    zsprintf(errbuf, "option -ds (--dot-size) has bad size:  '%s'", value);
                    free(value);
                    ZIPERR(ZE_PARMS, errbuf);
                }
                if (dot_size < 0x400) {
                    // < 1 KB so there is no multiplier, assume MB
                    dot_size *= 0x100000;

                } else if (dot_size < 0x400L * 32) {
                    // 1K <= dot_size < 32K
                    zsprintf(errbuf, "dot size must be at least 32 KB:  '%s'", value);
                    free(value);
                    ZIPERR(ZE_PARMS, errbuf);

                } else {
                    // 32K <= dot_size
                }
                free(value);
            }
            dot_count = -1;
            break;
        case o_du:
            if (negated)
                display_usize = 0;
            else
                display_usize = 1;
            break;
        case o_dv:
            if (negated)
                display_volume = 0;
            else
                display_volume = 1;
            break;
        case 'D': // Do not add directory entries
            dirnames = 0;
            break;
        case o_DF: // Create a difference archive
            diff_mode           = 1;
            allow_empty_archive = 1;
            break;
        case 'e': // Encrypt
#if !CRYPT
            ZIPERR(ZE_PARMS, "encryption not supported");
#else  // CRYPT
            if (key)
                free(key);
            key_needed = 1;
#endif // !CRYPT
            break;
        case 'F': // fix the zip file
            fix = 1;
            break;
        case o_FF: // try harder to fix file
            fix = 2;
            break;
        case o_FI:
            if (negated)
                allow_fifo = 0;
            else
                allow_fifo = 1;
            break;
        case o_FS: /* delete exiting entries in archive where there is
                      no matching file on file system */
            filesync = 1;
            break;
        case 'f': // Freshen zip file--overwrite only
            if (action != ADD) {
                ZIPERR(ZE_PARMS, "specify just one action");
            }
            action = FRESHEN;
            break;
        case 'g': // Allow appending to a zip file
            d = 1;
            break;
        case 'h':
        case 'H':
        case '?': // Help
            co_await help();
            FINISH(ZE_OK);

        case o_h2: // Extended Help
            co_await help_extended();
            FINISH(ZE_OK);

        // -i is with -x
        case 'j': // Junk directory names
            pathput = 0;
            break;
        case 'J': // Junk sfx prefix
            junk_sfx = 1;
            break;
        case 'k': // Make entries using DOS names (k for Katz)
            dosify = 1;
            break;
        case 'l': // Translate end-of-line
            translate_eol = 1;
            break;
        case o_ll:
            translate_eol = 2;
            break;
        case o_lf:
            // open a logfile
            // allow multiple use of option but only last used
            if (logfile_path) {
                free(logfile_path);
            }
            logfile_path = value;
            break;
        case o_la:
            // append to existing logfile
            if (negated)
                logfile_append = 0;
            else
                logfile_append = 1;
            break;
        case o_li:
            // log all including informational messages
            if (negated)
                logall = 0;
            else
                logall = 1;
            break;
        case 'L': // Show license
            co_await license();
            FINISH(ZE_OK);
        case 'm': // Delete files added or updated in zip file
            dispose = 1;
            break;
        case o_mm: // To prevent use of -mm for -MM
            ZIPERR(ZE_PARMS, "-mm not supported, Must_Match is -MM");
            dispose = 1;
            break;
        case o_MM: // Exit with error if input file can't be read
            bad_open_is_error = 1;
            break;
        case 'n': // Don't compress files with a special suffix
            special = value;
            /* special = NULL; */ // will be set at next argument
            break;
        case o_nw: // no wildcards - wildcards are handled like other characters
            no_wild = 1;
            break;
        case 'o': // Set zip file time to time of latest file in it
            latest = 1;
            break;
        case 'O': // Set output file different than input archive
            out_path = ziptyp(value);
            free(value);
            have_out = 1;
            break;
        case 'p':  // Store path with name
            break; // (do nothing as annoyance avoidance)
        case 'P':  // password for encryption
            if (key != NULL) {
                free(key);
            }
#if CRYPT
            key        = value;
            key_needed = 0;
#else
            ZIPERR(ZE_PARMS, "encryption not supported");
#endif // CRYPT
            break;
        case 'q': // Quiet operation
            noisy = 0;
            if (verbose)
                verbose--;
            break;
        case 'r': // Recurse into subdirectories, match full path
            if (recurse == 2) {
                ZIPERR(ZE_PARMS, "do not specify both -r and -R");
            }
            recurse = 1;
            break;
        case 'R': // Recurse into subdirectories, match filename
            if (recurse == 1) {
                ZIPERR(ZE_PARMS, "do not specify both -r and -R");
            }
            recurse = 2;
            break;

        case o_RE: // Allow [list] matching (regex)
            allow_regex = 1;
            break;

        case o_sc: // show command line args
            show_args = 1;
            break;
#ifdef UNICODE_TEST
        case o_sC: // create empty files from archive names
            create_files = 1;
            show_files   = 1;
            break;
#endif
        case o_sd: // show debugging
            show_what_doing = 1;
            break;
        case o_sf: // show files to operate on
            if (!negated)
                show_files = 1;
            else
                show_files = 2;
            break;
        case o_so: // show all options
            show_options = 1;
            break;
        case o_su: // -sf but also show Unicode if exists
            if (!negated)
                show_files = 3;
            else
                show_files = 4;
            break;
        case o_sU: // -sf but only show Unicode if exists or normal if not
            if (!negated)
                show_files = 5;
            else
                show_files = 6;
            break;

        case 's': // enable split archives
            // get the split size from value
            if (strcmp(value, "-") == 0) {
                // -s- do not allow splits
                split_method = -1;
            } else {
                split_size = co_await ReadNumString(value);
                if (split_size == (uzoff_t)-1) {
                    zsprintf(errbuf, "bad split size:  '%s'", value);
                    ZIPERR(ZE_PARMS, errbuf);
                }
                if (split_size == 0) {
                    // do not allow splits
                    split_method = -1;
                } else {
                    if (split_method == 0) {
                        split_method = 1;
                    }
                    if (split_size < 0x400) {
                        // < 1 KB there is no multiplier, assume MB
                        split_size *= 0x100000;
                    }
                    // By setting the minimum split size to 64 KB we avoid
                    // not having enough room to write a header unsplit
                    // which is required
                    if (split_size < 0x400L * 64) {
                        // split_size < 64K
                        zsprintf(errbuf, "minimum split size is 64 KB:  '%s'", value);
                        free(value);
                        ZIPERR(ZE_PARMS, errbuf);
                    }
                }
            }
            free(value);
            break;
        case o_sb: // when pause for next split ring bell
            split_bell = 1;
            break;
        case o_sp: // enable split select - pause splitting between splits
            use_descriptors = 1;
            split_method    = 2;
            break;
        case o_sv: // be verbose about creating splits
            noisy_splits = 1;
            break;

        case 't': // Exclude files earlier than specified date
        {
            int yyyy, mm, dd; // results of sscanf()

            // Support ISO 8601 & American dates
            if (zparse_date(value, &yyyy, &mm, &dd) != 3 || mm < 1 || mm > 12 || dd < 1 ||
                dd > 31) {
                ZIPERR(ZE_PARMS, "invalid date entered for -t option - use mmddyyyy or yyyy-mm-dd");
            }
            before = dostime(yyyy, mm, dd, 0, 0, 0);
        }
            free(value);
            break;
        case o_tt: // Exclude files at or after specified date
        {
            int yyyy, mm, dd; // results of sscanf()

            // Support ISO 8601 & American dates
            if (zparse_date(value, &yyyy, &mm, &dd) != 3 || mm < 1 || mm > 12 || dd < 1 ||
                dd > 31) {
                ZIPERR(ZE_PARMS,
                       "invalid date entered for -tt option - use mmddyyyy or yyyy-mm-dd");
            }
            after = dostime(yyyy, mm, dd, 0, 0, 0);
        }
            free(value);
            break;
        case 'T': // test zip file
            test = 1;
            break;
        case o_TT: // command path to use instead of 'unzip -t '
            if (unzip_path)
                free(unzip_path);
            unzip_path = value;
            break;
        case 'U': // Select archive entries to keep or operate on
            if (action != ADD) {
                ZIPERR(ZE_PARMS, "specify just one action");
            }
            action = ARCHIVE;
            break;
        case o_UN: // Unicode
            if (abbrevmatch("quit", value, 0, 1)) {
                // Unicode path mismatch is error
                unicode_mismatch = 0;
            } else if (abbrevmatch("warn", value, 0, 1)) {
                // warn of mismatches and continue
                unicode_mismatch = 1;
            } else if (abbrevmatch("ignore", value, 0, 1)) {
                // ignore mismatches and continue
                unicode_mismatch = 2;
            } else if (abbrevmatch("no", value, 0, 1)) {
                // no use Unicode path
                unicode_mismatch = 3;
            } else if (abbrevmatch("escape", value, 0, 1)) {
                // escape all non-ASCII characters
                unicode_escape_all = 1;

            } else if (abbrevmatch("UTF8", value, 0, 1)) {
                // force storing UTF-8 as standard per AppNote bit 11
                utf8_force = 1;

            } else {
                co_await zipwarn("-UN must be Quit, Warn, Ignore, No, Escape, or UTF8: ", value);

                free(value);
                ZIPERR(ZE_PARMS, "-UN (unicode) bad value");
            }
            free(value);
            break;
        case 'u': // Update zip file--overwrite only if newer
            if (action != ADD) {
                ZIPERR(ZE_PARMS, "specify just one action");
            }
            action = UPDATE;
            break;
        case 'v':                                        // Either display version information or
        case o_ve:                                       // Mention oddities in zip file structure
            if (option == o_ve ||                        // --version
                (argcnt == 2 && strlen(args[1]) == 2)) { // -v only
                // display version
                co_await version_info();
                FINISH(ZE_OK);
            } else {
                noisy = 1;
                verbose++;
            }
            break;
        case o_ws: // Wildcards do not include directory boundaries in matches
            wild_stop_at_dir = 1;
            break;

        case 'i': // Include only the following files
            // if nothing matches include list then still create an empty archive
            allow_empty_archive = 1;
        case 'x': // Exclude following files
            co_await add_filter((int)option, value);
            free(value);
            break;
#ifdef S_IFLNK
        case 'y': // Store symbolic links as such
            linkput = 1;
            break;
#endif            // S_IFLNK
        case 'z': // Edit zip file comment
            zipedit = 1;
            break;
        case 'Z': // Compression method
            if (abbrevmatch("deflate", value, 0, 1)) {
                // deflate
                method = DEFLATE;
            } else if (abbrevmatch("store", value, 0, 1)) {
                // store
                method = STORE;
            } else if (abbrevmatch("bzip2", value, 0, 1)) {
                // bzip2
                ZIPERR(ZE_COMPERR, "Compression method bzip2 not enabled");
            } else {
                co_await zipwarn("valid compression methods are:  store, deflate)", "");
                co_await zipwarn("unknown compression method found:  ", value);
                free(value);
                ZIPERR(ZE_PARMS, "Option -Z (--compression-method):  unknown method");
            }
            free(value);
            break;
        case '@': // read file names from stdin
            comment_stream = NULL;
            s              = 1; // defer -@ until have zipfile name
            break;
        case 'X':
            if (negated)
                extra_fields = 2;
            else
                extra_fields = 0;
            break;
        case o_des:
            use_descriptors = 1;
            break;

        case o_z64: // Force creation of Zip64 entries
            if (negated) {
                force_zip64 = 0;
            } else {
                force_zip64 = 1;
            }
            break;

        case o_NON_OPTION_ARG:
            // not an option
            // no more options as permuting
            // just dash also ends up here

            if (recurse != 2 && kk == 0 && patterns == NULL) {
                // have all filters so convert filterlist to patterns array
                // as PROCNAME needs patterns array
                co_await filterlist_to_patterns();
            }

            // "--" stops arg processing for remaining args
            // ignore only first --
            if (strcmp(value, "--") == 0 && seen_doubledash == 0) {
                // --
                seen_doubledash = 1;
                if (kk == 0) {
                    ZIPERR(ZE_PARMS, "can't use -- before archive name");
                }

                // just ignore as just marks what follows as non-option arguments

            } else if (kk == 6) {
                // value is R pattern
                co_await add_filter((int)'R', value);
                free(value);
                if (first_listarg == 0) {
                    first_listarg = argnum;
                }
            } else
                switch (kk) {
                case 0:
                    // first non-option arg is zipfile name
                    if (strcmp(value, "-") == 0) { // output zipfile is dash
                        // just a dash
                        if ((r = co_await zipstdout()) != ZE_OK)
                            co_return r;
                    } else {
                        // name of zipfile
                        if ((zipfile = ziptyp(value)) == NULL) {
                            ZIPERR(ZE_MEM, "was processing arguments");
                        }
                        // read zipfile if exists
                        // if ((r = readzipfile()) != ZE_OK) {
                        // ZIPERR(r, zipfile);
                        // }
                        free(value);
                    }
                    if (show_what_doing) {
                        co_await zfprintf(mesg, "sd: Zipfile name '%s'\n", zipfile);
                        co_await zfflush(mesg);
                    }
                    // if in_path not set, use zipfile path as usual for input
                    // in_path is used as the base path to find splits
                    if (in_path == NULL) {
                        if ((in_path = (char *)malloc(strlen(zipfile) + 1)) == NULL) {
                            ZIPERR(ZE_MEM, "was processing arguments");
                        }
                        strcpy(in_path, zipfile);
                    }
                    // if out_path not set, use zipfile path as usual for output
                    // out_path is where the output archive is written
                    if (out_path == NULL) {
                        if ((out_path = (char *)malloc(strlen(zipfile) + 1)) == NULL) {
                            ZIPERR(ZE_MEM, "was processing arguments");
                        }
                        strcpy(out_path, zipfile);
                    }
                    kk = 3;
                    if (s) {
                        // do -@ and get names from stdin
                        // should be able to read names from
                        // stdin and output to stdout, but
                        // this was not allowed in old code.
                        // This check moved to kk = 3 case to fix.
                        // if (strcmp(zipfile, "-") == 0) {
                        // ZIPERR(ZE_PARMS, "can't use - and -@ together");
                        // }
                        while ((pp = co_await getnam(&File::stdin())) != NULL) {
                            kk = 4;
                            if (recurse == 2) {
                                // reading patterns from stdin
                                co_await add_filter((int)'R', pp);
                            } else {
                                // file argument now processed later
                                co_await add_name(pp);
                            }
                            // if ((r = PROCNAME(pp)) != ZE_OK) {
                            // if (r == ZE_MISS)
                            // zipwarn("name not matched: ", pp);
                            // else {
                            // ZIPERR(r, pp);
                            // }
                            // }
                            free(pp);
                        }
                        s = 0;
                    }
                    if (recurse == 2) {
                        // rest are -R patterns
                        kk = 6;
                    }
                    break;

                case 3:
                case 4:
                    // no recurse and -r file names
                    // can't read filenames -@ and input - from stdin at
                    // same time
                    if (s == 1 && strcmp(value, "-") == 0) {
                        ZIPERR(ZE_PARMS, "can't read input (-) and filenames (-@) both from stdin");
                    }
                    // add name to list for later processing
                    co_await add_name(value);
                    // if ((r = PROCNAME(value)) != ZE_OK) {
                    // if (r == ZE_MISS)
                    // zipwarn("name not matched: ", value);
                    // else {
                    // ZIPERR(r, value);
                    // }
                    // }
                    if (kk == 3) {
                        first_listarg = argnum;
                        kk            = 4;
                    }
                    break;

                } // switch kk
            break;

        default:
            // should never get here as get_option will exit if not in table
            zsprintf(errbuf, "no such option ID: %ld", option);
            ZIPERR(ZE_PARMS, errbuf);

        } // switch
    }

    // do processing of command line and one-time tasks

    if (key_needed)
        ZIPERR(ZE_PARMS, "encryption is not built in yet")

    // The patterns -i, -x and -R named, in the order they were given.
    if (filterlist != NULL) {
        if (kk < 3)
            ZIPERR(ZE_PARMS, "no zip file specified")
        if ((r = co_await filterlist_to_patterns()) != ZE_OK)
            co_return r;
    }

    if (show_options) {
        co_await zfprintf(zstdout, "available options:\n");
        co_await zfprintf(zstdout, " %-2s  %-18s %-4s %-3s %-30s\n", "sh", "long", "val", "neg",
                          "description");
        co_await zfprintf(zstdout, " %-2s  %-18s %-4s %-3s %-30s\n", "--", "----", "---", "---",
                          "-----------");
        for (i = 0; options[i].option_ID; i++) {
            ZCONST char *v = "";
            switch (options[i].value_type) {
            case o_REQUIRED_VALUE:
                v = "req";
                break;
            case o_OPTIONAL_VALUE:
                v = "opt";
                break;
            case o_VALUE_LIST:
                v = "list";
                break;
            case o_ONE_CHAR_VALUE:
                v = "char";
                break;
            case o_NUMBER_VALUE:
                v = "num";
                break;
            }
            co_await zfprintf(zstdout, " %-2s  %-18s %-4s %-3s %-30s\n", options[i].shortopt,
                              options[i].longopt, v,
                              options[i].negatable == o_NEGATABLE ? "neg" : "", options[i].name);
        }
        co_return ZE_OK;
    }

    if (kk < 3)
        ZIPERR(ZE_PARMS, "no zip file specified")

    // Not yet: -d, -u, -f, -F, -FF, -o, -g and --out all read the archive that
    // is there, which is readzipfile()'s job and the update path's.
    if (action != ADD)
        ZIPERR(ZE_COMPERR, "only adding to a new archive is supported so far")
    if (fix || adjust || filesync || diff_mode || copy_only || zip_to_stdout)
        ZIPERR(ZE_COMPERR, "that option needs the update path, which is not here yet")

    out_path = zipfile;

    // Not yet, and it must not be silent: reading what is already in the
    // archive is readzipfile()'s, and until that is here an existing archive
    // would be replaced rather than added to.
    {
        Result<FileInfo> st = Err(Error::NoMemory);
        if (Task<Result<FileInfo>> tk = stat_of(Str(zipfile, strlen(zipfile))))
            st = co_await tk;
        if (st.is_ok())
            ZIPERR(ZE_COMPERR,
                   "the archive exists, and adding to one needs the "
                   "update path, which is not here yet")
    }

    // Names from stdin, one per line.
    if (s) {
        char *nm;
        while ((nm = co_await getnam(&File::stdin())) != NULL) {
            co_await add_name(nm);
            free(nm);
        }
    }

    // Turn each name into entries: a file is one, a directory is its tree.
    if (filelist) {
        for (; filelist;) {
            if ((r = co_await procname(filelist->name, filter_match_case)) != ZE_OK) {
                if (r == ZE_MISS) {
                    co_await zipwarn("name not matched: ", filelist->name);
                    if (bad_open_is_error)
                        ZIPERR(ZE_OPEN, filelist->name)
                } else {
                    ZIPERR(r, filelist->name)
                }
            }
            free(filelist->name);
            filearg  = filelist;
            filelist = filelist->next;
            free(filearg);
        }
    }
    if (zip_fatal != ZE_OK)
        co_return zip_fatal;

    // Recurse from the current directory for -R.
    if (recurse == 2) {
        if ((r = co_await procname((char *)".", filter_match_case)) != ZE_OK) {
            if (r == ZE_MISS)
                co_await zipwarn("name not matched: ", ".");
            else
                ZIPERR(r, ".")
        }
    }

    if ((r = co_await check_dup()) != ZE_OK) {
        if (r == ZE_PARMS)
            ZIPERR(ZE_PARMS, "cannot repeat names in zip file")
        else
            ZIPERR(r, "was processing list of files")
    }

    if (zcount == 0 && fcount == 0) {
        if (!allow_empty_archive)
            ZIPERR(ZE_NONE, zipfile)
    }

    // Open the temporary archive. SYS_O_EXCL beside SYS_O_CREATE is what
    // mkstemp gave upstream: a name nobody else has taken.
    tempzip = tempname(zipfile);
    if (tempzip == NULL)
        ZIPERR(ZE_MEM, "allocating temp filename")

    Result<i32> fd = Err(Error::NoMemory);
    if (Task<Result<i32>> tk =
            open_at(Str(tempzip, strlen(tempzip)),
                    SYS_O_READ | SYS_O_WRITE | SYS_O_CREATE | SYS_O_EXCL | SYS_O_TRUNC))
        fd = co_await tk;
    if (fd.is_err())
        ZIPERR(ZE_TEMP, tempzip)

    // File::of() borrows the descriptor and will not close it, and the store
    // refuses to rename a file something still holds open — so the descriptor
    // is closed by hand once the stream has been flushed.
    u32 yfd  = (u32)fd.value();
    FILE tmp = File::of(yfd, FileMode::Update);
    tmp.set_buffering(Buffering::Full);
    tmp.reserve(SYS_READ_MAX);
    y               = &tmp;
    output_seekable = 1;
    tempzn          = 0;

    // Write the new entries, in the order the names were found.
    o = 0;
    for (f = found; f != NULL;) {
        struct zlist far *nz = (struct zlist far *)farmalloc(sizeof(struct zlist));
        if (nz == NULL)
            ZIPERR(ZE_MEM, "was adding files to zip file")
        memset((char *)nz, 0, sizeof(struct zlist));
        nz->name    = f->name;
        nz->iname   = f->iname;
        nz->zname   = f->zname;
        nz->oname   = f->oname;
        nz->uname   = f->uname;
        nz->dosflag = f->dosflag;
        nz->mark    = 1;
        nz->nxt     = NULL;

        // In order, so the archive lists as the names were found.
        struct zlist far **w = &zfiles;
        while (*w != NULL)
            w = &(*w)->nxt;
        *w = nz;
        zcount++;

        co_await DisplayRunningStats();
        if (noisy) {
            co_await zfprintf(mesg, "%s %s", action == ADD ? "  adding:" : "updating:", nz->oname);
            mesg_line_started = 1;
            co_await zfflush(mesg);
        }
        r = co_await zipup(nz);
        if (r != ZE_OK && r != ZE_OPEN && r != ZE_MISS)
            ZIPERR(r, nz->oname)
        if (r == ZE_OPEN || r == ZE_MISS) {
            o        = 1;
            nz->mark = 0;
            if (noisy) {
                co_await zfputc('\n', mesg);
                mesg_line_started = 0;
            }
            co_await zipwarn(r == ZE_OPEN ? "could not open for reading: " : "name not matched: ",
                             nz->oname);
            if (bad_open_is_error)
                ZIPERR(r, nz->oname)
        }
        if (zip_fatal != ZE_OK)
            co_return zip_fatal;

        // Move on, and take the found entry with it: the zlist owns the names.
        struct flist far *nxt = f->nxt;
        f->name = f->iname = f->zname = f->oname = f->uname = NULL;
        fexpel(f);
        f = nxt;
    }

    // Write central directory and end header.
    k = 0;      // keep count for end header
    c = tempzn; // get start of central
    n = t = 0;
    for (z = zfiles; z != NULL; z = z->nxt) {
        if (!z->mark)
            continue;
        if ((r = co_await putcentral(z)) != ZE_OK)
            ZIPERR(r, tempzip)
        tempzn += 4 + CENHEAD + z->nam + z->cext + z->com;
        n += z->len;
        t += z->siz;
        k++;
    }

    if (k == 0)
        co_await zipwarn("zip file empty", "");
    if (verbose) {
        co_await zfprintf(mesg, "total bytes=%s, compressed=%s -> %d%% savings\n",
                          zip_fzofft(n, NULL, "u"), zip_fzofft(t, NULL, "u"), percent(n, t));
        co_await zfflush(mesg);
    }

    t = tempzn - c; // compute length of central
    if ((r = co_await putend((extent)k, t, c, zcomlen, zcomment)) != ZE_OK)
        ZIPERR(r, tempzip)

    if ((co_await zfclose(y)).is_err())
        ZIPERR(ZE_TEMP, tempzip)
    y = NULL;
    if (Task<void> cl = close_fd(yfd))
        co_await cl;

    // Replace the old archive with the new one, which is what leaves the
    // original alone when a run is interrupted.
    if ((r = co_await replace(zipfile, tempzip)) != ZE_OK)
        ZIPERR(r, zipfile)
    free(tempzip);
    tempzip = NULL;

    csize = t;
    usize = n;
    (void)csize;
    (void)usize;
    (void)d;
    (void)first_listarg;
    (void)seen_doubledash;
    (void)have_out;
    (void)show_what_doing;
    (void)show_args;
    (void)comment_stream;
    (void)e;
    (void)pp;

    co_return co_await finish(o ? ZE_OPEN : ZE_OK);
}

Task<i32> proc_main(Args args)
{
    zstdout = &File::stdout();
    zstderr = &File::stderr();
    mesg    = zstdout;

    // ^C is asked for, or the default action stands. There is no handler:
    // a delivered signal abandons the parked call with Err(Intr).
    if (Task<Result<void>> t = sig_catch(SIG_INT))
        co_await t;

    int r = ZE_LOGIC;
    if (Task<int> t = zipmain(args))
        r = co_await t;

    // zip_fail() records without writing, because a plain function cannot
    // co_await one out. If nothing else reported the run, this does.
    if (r == ZE_OK && zip_fatal != ZE_OK) {
        r = zip_fatal;
        co_await zfprintf(zstderr, "\nzip error: %s (%s)\n", ZIPERRORS(r),
                          zip_fatal_h ? zip_fatal_h : "");
    }

    // The temp archive must not outlive a failed run.
    if (r != ZE_OK && tempzip != NULL) {
        if (y != NULL)
            co_await zfclose(y);
        co_await destroy(tempzip);
    }

    freeup();
    co_await zfflush(zstdout);
    co_await zfflush(zstderr);

    if (r == ZE_ABORT)
        co_return 130;
    co_return r;
}
