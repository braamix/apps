// zipnote.cpp — zipnote.c, by Mark Adler.
//
// One of the three tools built from zip's own sources: it reads an archive
// and writes it back, and never compresses. Upstream built them with -DUTIL,
// which strips the create path out of the shared files; here they link the
// same objects and --gc-sections keeps what is not reached out of the binary.
//
// Its own message routines are what the shared files call, so this file
// supplies ziperr_msg, zipwarn, zipmessage and zipmessage_nl.

#include "revision.h"
#include "zip.h"

// Calculate size of static line buffer used in write (-w) mode.
#define WRBUFSIZ 2047
// The line buffer size should be at least as large as FNMAX.
#if FNMAX > WRBUFSIZ
#undef WRBUFSIZ
#define WRBUFSIZ FNMAX
#endif

// Character to mark zip entry names in the comment file
#define MARK  '@'
#define MARKE " (comment above this line)"
#define MARKZ " (zip file comment below this line)"

// Temporary zip file pointer
local FILE *tempzf;

// Local functions
local Task<void> license OF((void));
local Task<void> help OF((void));
local Task<void> version_info OF((void));
local Task<void> putclean OF((char *, extent));
// getline name conflicts with GNU getline() function
local Task<char *> zgetline OF((char *, extent));
local int catalloc OF((char *far *, char *));
int main OF((int, char **));

// keep compiler happy until implement long options - 11/4/2003 EG
struct option_struct far options[] = {
    // short longopt        value_type        negatable        ID    name
    { "h", "help", o_NO_VALUE, o_NOT_NEGATABLE, 'h', "help" },
    // the end of the list
    { NULL, NULL, o_NO_VALUE, o_NOT_NEGATABLE, 0, NULL } // end has option_ID = 0
};

int set_filetype(char *out_path)
{
    return ZE_OK;
}

// rename a split
// A split has a tempfile name until it is closed, then
// here rename it as out_path the final name for the split.
Task<int> rename_split(char *temp_name, char *out_path)
{
    int r;
    // Replace old zip file with new zip file, leaving only the new one
    if ((r = co_await replace(out_path, temp_name)) != ZE_OK) {
        co_await zipwarn("new zip file left as: ", temp_name);
        free((zvoid *)tempzip);
        tempzip = NULL;
        ZIPERR(r, "was replacing split file");
    }
    if (zip_attributes) {
        setfileattr(out_path, zip_attributes);
    }
    co_return ZE_OK;
}

Task<void> zipmessage_nl(ZCONST char *a, int nl)
{
    if (noisy) {
        co_await b_fprintf(mesg, "%s", a);
        if (nl) {
            co_await b_fprintf(mesg, "\n");
            mesg_line_started = 0;
        } else {
            mesg_line_started = 1;
        }
        co_await b_fflush(mesg);
    }
}

Task<void> zipmessage(ZCONST char *a, ZCONST char *b)
{
    if (noisy) {
        if (mesg_line_started)
            co_await b_fprintf(mesg, "\n");
        co_await b_fprintf(mesg, "%s%s\n", a, b);
        mesg_line_started = 0;
        co_await b_fflush(mesg);
    }
}

Task<void> ziperr_msg(int c, ZCONST char *h)
{
    co_await b_fprintf(mesg, "zipnote error: %s (%s)\n", ZIPERRORS(c), h);
    if (tempzf != NULL)
        co_await b_fclose(tempzf);
    if (tempzip != NULL) {
        co_await destroy(tempzip);
        free((zvoid *)tempzip);
    }
    if (zipfile != NULL)
        free((zvoid *)zipfile);
    zip_fail(c, "");
}

Task<void> zipwarn(ZCONST char *a, ZCONST char *b)
{
    co_await b_fprintf(mesg, "zipnote warning: %s%s\n", a, b);
}

local Task<void> license(void)
// Print license information to stdout.
{
    extent i; // counter for copyright array

    for (i = 0; i < sizeof(swlicense) / sizeof(char *); i++)
        co_await b_puts(swlicense[i]);
}

local Task<void> help(void)
// Print help (along with license info) to stdout.
{
    extent i; // counter for help array

    // help array
    static ZCONST char *text[] = {
        "",
        "ZipNote %s (%s)",
        "Usage:  zipnote [-w] [-q] [-b path] zipfile",
        "  the default action is to write the comments in zipfile to stdout",
        "  -w   write the zipfile comments from stdin",
        "  -b   use \"path\" for the temporary zip file",
        "  -q   quieter operation, suppress some informational messages",
        "  -h   show this help    -v   show version info    -L   show software license",
        "",
        "Example:",
        "     zipnote foo.zip > foo.tmp",
        "     ed foo.tmp",
        "     ... then you edit the comments, save, and exit ...",
        "     zipnote -w foo.zip < foo.tmp",
        "",
        "  \"@ name\" can be followed by an \"@=newname\" line to change the name"
    };

    for (i = 0; i < sizeof(copyright) / sizeof(char *); i++) {
        co_await b_fprintf(stdout, copyright[i], "zipnote");
        co_await b_fputc('\n', stdout);
    }
    for (i = 0; i < sizeof(text) / sizeof(char *); i++) {
        co_await b_fprintf(stdout, text[i], VERSION, REVDATE);
        co_await b_fputc('\n', stdout);
    }
}

// XXX put this in version.c

local Task<void> version_info(void)
// Print verbose info about program version and compile time options
// to stdout.
{
    extent i; // counter in text arrays

    // Options info array
    static ZCONST char *comp_opts[] = { NULL };

    for (i = 0; i < sizeof(copyright) / sizeof(char *); i++) {
        co_await b_fprintf(stdout, copyright[i], "zipnote");
        co_await b_fputc('\n', stdout);
    }

    for (i = 0; i < sizeof(versinfolines) / sizeof(char *); i++) {
        co_await b_fprintf(stdout, versinfolines[i], "ZipNote", VERSION, REVDATE);
        co_await b_fputc('\n', stdout);
    }

    co_await version_local();

    co_await b_puts("ZipNote special compilation options:");
    for (i = 0; (int)i < (int)(sizeof(comp_opts) / sizeof(char *) - 1); i++) {
        co_await b_fprintf(stdout, "\t%s\n", comp_opts[i]);
    }
    if (i == 0)
        co_await b_puts("\t[none]");
}

local Task<void> putclean(char *s, extent n)
{
    int c; // next character in string
    int e; // last character written

    e = '\n'; // if empty, write nothing
    while (n--) {
        c = *(uch *)s++;
        if (c == MARK || c == '\\')
            co_await b_fputc('\\', stdout);
        if (c >= ' ' || c == '\t' || c == '\n') {
            e = c;
            co_await b_fputc(e, stdout);
        }
    }
    if (e != '\n')
        co_await b_fputc('\n', stdout);
}

local Task<char *> zgetline(char *buf, extent size)
{
    char *line;
    unsigned len;

    line = co_await b_fgets(buf, (int)size, stdin);
    if (line != NULL && (len = strlen(line)) > 0) {
        if (len == size - 1 && line[len - 1] != '\n') {
            // buffer is full and record delimiter not seen -> overflow
            line = NULL;
        } else {
            // delete trailing record delimiter
            if (line[len - 1] == '\n')
                line[len - 1] = '\0';
        }
    }
    co_return line;
}

local int catalloc(char *far *a, char *s)
{
    char *p; // temporary pointer
    char *q; // temporary pointer

    for (p = q = s; *q; *p++ = *q++)
        if (*q == '\\' && *(q + 1))
            q++;
    *p = 0;
    if ((p = (char *)malloc(strlen(*a) + strlen(s) + 3)) == NULL)
        return ZE_MEM;
    strcat(strcat(strcpy(p, *a), **a ? "\r\n" : ""), s);
    free((zvoid *)*a);
    *a = p;
    return ZE_OK;
}

Task<i32> proc_main(Args args)
{
    char **argv;
    int argc = zargv(args, &argv);

    char abf[WRBUFSIZ + 1]; // input line buffer
    char *a;                // pointer to line buffer or NULL
    zoff_t c;               // start of central directory
    int k;                  // next argument type
    char *q;                // steps through option arguments
    int r;                  // arg counter, temporary variable
    zoff_t s;               // length of central directory
    int t;                  // attributes of zip file
    int w;                  // true if updating zip file from stdin
    FILE *x;                // input file for testing if can write it
    struct zlist far *z;    // steps through zfiles linked list

    // For Unix, set the locale to UTF-8.  Any UTF-8 locale is
    // OK and they should all be the same.  This allows seeing,
    // writing, and displaying (if the fonts are loaded) all
    // characters in UTF-8.
    // The character set here is UTF-8 and there is no locale to set it
    // with, so the UTF-8 general-purpose bit may always be used.
    using_utf8 = 1;

    // If no argv, show help
    if (argc == 1) {
        co_await help();
        co_return ZE_OK;
    }

    // Direct info messages to stderr; stdout is used for data output.
    mesg = stderr;

    init_upper(); // build case map table

    // Go through argv
    zipfile = tempzip = NULL;
    tempzf            = NULL;
    // ^C is asked for; there is no handler, and a delivered signal
    // abandons the call the process is parked on.
    if (Task<Result<void>> sc = sig_catch(SIG_INT))
        co_await sc;
    k = w = 0;
    for (r = 1; r < argc; r++)
        if (*argv[r] == '-') {
            if (argv[r][1])
                for (q = argv[r] + 1; *q; q++)
                    switch (*q) {
                    case 'b': // Specify path for temporary file
                        if (k) {
                            ziperr(ZE_PARMS, "use -b before zip file name");
                        } else
                            k = 1; // Next non-option is path
                        break;
                    case 'h': // Show help
                        co_await help();
                        co_return ZE_OK;
                    case 'l':
                    case 'L': // Show copyright and disclaimer
                        co_await license();
                        co_return ZE_OK;
                    case 'q': // Quiet operation, suppress info messages
                        noisy = 0;
                        break;
                    case 'v': // Show version info
                        co_await version_info();
                        co_return ZE_OK;
                    case 'w':
                        w = 1;
                        break;
                    default:
                        ziperr(ZE_PARMS, "unknown option");
                    }
            else
                ziperr(ZE_PARMS, "zip file cannot be stdin");
        } else if (k == 0) {
            if (zipfile == NULL) {
                if ((zipfile = ziptyp(argv[r])) == NULL)
                    ziperr(ZE_MEM, "was processing arguments");
            } else
                ziperr(ZE_PARMS, "can only specify one zip file");
        } else {
            tempath = argv[r];
            k       = 0;
        }
    if (zipfile == NULL)
        ziperr(ZE_PARMS, "need to specify zip file");

    if ((in_path = (char *)malloc(strlen(zipfile) + 1)) == NULL) {
        ziperr(ZE_MEM, "input");
    }
    strcpy(in_path, zipfile);

    // Read zip file
    if ((r = co_await readzipfile()) != ZE_OK)
        ziperr(r, zipfile);
    if (zfiles == NULL)
        ziperr(ZE_NAME, zipfile);

    // Put comments to stdout, if not -w
    if (!w) {
        for (z = zfiles; z != NULL; z = z->nxt) {
            co_await b_fprintf(stdout, "%c %s\n", MARK, z->zname);
            co_await putclean(z->comment, z->com);
            co_await b_fprintf(stdout, "%c%s\n", MARK, MARKE);
        }
        co_await b_fprintf(stdout, "%c%s\n", MARK, MARKZ);
        co_await putclean(zcomment, zcomlen);
        co_return ZE_OK;
    }

    // If updating comments, make sure zip file is writeable
    if ((x = co_await b_fopen(zipfile, "a")) == NULL)
        ziperr(ZE_CREAT, zipfile);
    co_await b_fclose(x);
    t = getfileattr(zipfile);

    // Process stdin, replacing comments
    z = zfiles;
    while ((a = co_await zgetline(abf, WRBUFSIZ + 1)) != NULL &&
           (a[0] != MARK || strcmp(a + 1, MARKZ))) { // while input and not file comment
        if (a[0] != MARK || a[1] != ' ')             // better be "@ name"
            ziperr(ZE_NOTE, "unexpected input");
        while (z != NULL && strcmp(a + 2, z->zname))
            z = z->nxt; // allow missing entries in order
        if (z == NULL)
            ziperr(ZE_NOTE, "unknown entry name");
        if ((a = co_await zgetline(abf, WRBUFSIZ + 1)) != NULL && a[0] == MARK && a[1] == '=') {
            if (z->name != z->iname)
                free((zvoid *)z->iname);
            if ((z->iname = (char *)malloc(strlen(a + 1))) == NULL)
                ziperr(ZE_MEM, "was changing name");
            strcpy(z->iname, a + 2);

            // Don't update z->nam here, we need the old value a little later.....
            // The update is handled in zipcopy().
            a = co_await zgetline(abf, WRBUFSIZ + 1);
        }
        if (z->com) // change zip entry comment
            free((zvoid *)z->comment);
        z->comment    = (char *)malloc(1);
        *(z->comment) = 0;
        while (a != NULL && *a != MARK) {
            if ((r = catalloc(&(z->comment), a)) != ZE_OK)
                ziperr(r, "was building new zipentry comments");
            a = co_await zgetline(abf, WRBUFSIZ + 1);
        }
        z->com = strlen(z->comment);
        z      = z->nxt; // point to next entry
    }
    if (a != NULL) // change zip file comment
    {
        zcomment  = (char *)malloc(1);
        *zcomment = 0;
        while ((a = co_await zgetline(abf, WRBUFSIZ + 1)) != NULL)
            if ((r = catalloc(&zcomment, a)) != ZE_OK)
                ziperr(r, "was building new zipfile comment");
        zcomlen = strlen(zcomment);
    }

    // Open output zip file for writing
    {
        int i;

        // use mkstemp to avoid race condition and compiler warning

        if (tempath != NULL) {
            // if -b used to set temp file dir use that for split temp
            if ((tempzip = (char *)malloc(strlen(tempath) + 12)) == NULL) {
                ZIPERR(ZE_MEM, "allocating temp filename");
            }
            strcpy(tempzip, tempath);
            if (lastchar(tempzip) != '/')
                strcat(tempzip, "/");
        } else {
            // create path by stripping name and appending namepat
            if ((tempzip = (char *)malloc(strlen(zipfile) + 12)) == NULL) {
                ZIPERR(ZE_MEM, "allocating temp filename");
            }
            strcpy(tempzip, zipfile);
            for (i = strlen(tempzip); i > 0; i--) {
                if (tempzip[i - 1] == '/')
                    break;
            }
            tempzip[i] = '\0';
        }
        // tempname() carries proc_random() where upstream handed a
        // "ziXXXXXX" template to mkstemp; the open names SYS_O_EXCL.
        {
            char *nm = tempname(tempzip);
            free(tempzip);
            tempzip = nm;
            if (tempzip == NULL) {
                ZIPERR(ZE_MEM, "allocating temp filename");
            }
        }
        if ((tempzf = y = co_await b_fopen(tempzip, FOPW)) == NULL) {
            ZIPERR(ZE_TEMP, tempzip);
        }
    }

    // Open input zip file again, copy preamble if any
    if ((in_file = co_await b_fopen(zipfile, FOPR)) == NULL)
        ziperr(ZE_NAME, zipfile);

    if (zipbeg && (r = co_await bfcopy(zipbeg)) != ZE_OK)
        ziperr(r, r == ZE_TEMP ? tempzip : zipfile);
    tempzn = zipbeg;

    // Go through local entries, copying them over as is
    fix = 3; // needed for zipcopy if name changed
    for (z = zfiles; z != NULL; z = z->nxt) {
        if ((r = co_await zipcopy(z)) != ZE_OK)
            ziperr(r, "was copying an entry");
    }
    co_await b_fclose(x);

    // Write central directory and end of central directory with new comments
    if ((c = co_await b_ftello(y)) == (zoff_t)-1) // get start of central
        ziperr(ZE_TEMP, tempzip);
    for (z = zfiles; z != NULL; z = z->nxt)
        if ((r = co_await putcentral(z)) != ZE_OK)
            ziperr(r, tempzip);
    if ((s = co_await b_ftello(y)) == (zoff_t)-1) // get end of central
        ziperr(ZE_TEMP, tempzip);
    s -= c; // compute length of central
    if ((r = co_await putend((zoff_t)zcount, s, c, zcomlen, zcomment)) != ZE_OK)
        ziperr(r, tempzip);
    tempzf = NULL;
    if (co_await b_fclose(y))
        ziperr(ZE_TEMP, tempzip);
    if ((r = co_await replace(zipfile, tempzip)) != ZE_OK) {
        co_await zipwarn("new zip file left as: ", tempzip);
        free((zvoid *)tempzip);
        tempzip = NULL;
        ziperr(r, "was replacing the original zip file");
    }
    free((zvoid *)tempzip);
    tempzip = NULL;
    setfileattr(zipfile, t);
    free((zvoid *)zipfile);
    zipfile = NULL;

    // Done!
    co_return (0);
}
