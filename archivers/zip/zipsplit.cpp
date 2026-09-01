// zipsplit.cpp — zipsplit.c, by Mark Adler.
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

#define DEFSIZ    36000L         // Default split size (change in help() too)
#define NL        1              // Number of bytes written for a \n
#define INDEX     "zipsplit.idx" // Name of index file
#define TEMPL_FMT "%%0%dld.zip"
#define TEMPL_SIZ 17
#define ZPATH_SEP '.'

// Local functions
local zvoid *talloc OF((extent));
local void tfree OF((zvoid *));
local void tfreeall OF((void));
local Task<void> license OF((void));
local Task<void> help OF((void));
local Task<void> version_info OF((void));
local extent simple OF((uzoff_t *, extent, uzoff_t, uzoff_t));
local int descmp OF((ZCONST zvoid *, ZCONST zvoid *));
local Task<extent> greedy OF((uzoff_t *, extent, uzoff_t, uzoff_t));
local Task<int> retry OF((void));
int main OF((int, char **));

// Output zip files
local char namepat[TEMPL_SIZ]; // name namepat for output files
local int zipsmade  = 0;       // number of zip files made
local int indexmade = 0;       // true if index file made
local char *path    = NULL;    // space for full name
local char *name;              // where name goes in path[]

// The talloc() and tree() routines extend malloc() and free() to keep
// track of all allocated memory.  Then the tfreeall() routine uses this
// information to free all allocated memory before exiting.

#define TMAX 6      // set intelligently by examining the code
zvoid *talls[TMAX]; // malloc'ed pointers to track
int talln = 0;      // number of entries in talls[]

int set_filetype(char *out_path)
{
    return ZE_OK;
}

// rename a split
// A split has a tempfile name until it is closed, then
// here rename it as out_path the final name for the split.
//
// This is not used in zipsplit but is referenced by the generic split
// writing code.  If zipsplit is made split aware (so can write splits of
// splits, if that makes sense) then this would get used.  But if that
// happens these utility versions should be dropped and the main ones
// used.
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

local zvoid *talloc(extent s)
{
    zvoid *p;

    if ((p = (zvoid *)malloc(s)) != NULL)
        talls[talln++] = p;
    return p;
}

local void tfree(zvoid *p)
{
    int i;

    free(p);
    i = talln;
    while (i--)
        if (talls[i] == p)
            break;
    if (i >= 0) {
        while (++i < talln)
            talls[i - 1] = talls[i];
        talln--;
    }
}

local void tfreeall()
// free everything talloc'ed and not tfree'd
{
    while (talln)
        free(talls[--talln]);
}

Task<void> ziperr_msg(int c, ZCONST char *h)
{
    co_await b_fprintf(mesg, "zipsplit error: %s (%s)\n", ZIPERRORS(c), h);
    if (indexmade) {
        strcpy(name, INDEX);
        co_await destroy(path);
    }
    for (; zipsmade; zipsmade--) {
        sprintf(name, namepat, zipsmade);
        co_await destroy(path);
    }
    tfreeall();
    if (zipfile != NULL)
        free((zvoid *)zipfile);
    zip_fail(c, "");
}

Task<void> zipwarn(ZCONST char *a, ZCONST char *b)
{
    co_await b_fprintf(mesg, "zipsplit warning: %s%s\n", a, b);
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
        "ZipSplit %s (%s)",
        "Usage:  zipsplit [-tipqs] [-n size] [-r room] [-b path] zipfile",
        "  -t   report how many files it will take, but don't make them",
        "  -i   make index (zipsplit.idx) and count its size against first zip file",
        "  -n   make zip files no larger than \"size\" (default = 36000)",
        "  -r   leave room for \"room\" bytes on the first disk (default = 0)",
        "  -b   use \"path\" for the output zip files",
        "  -q   quieter operation, suppress some informational messages",
        "  -p   pause between output zip files",
        "  -s   do a sequential split even if it takes more zip files",
        "  -h   show this help    -v   show version info    -L   show software license"
    };

    for (i = 0; i < sizeof(copyright) / sizeof(char *); i++) {
        co_await b_fprintf(stdout, copyright[i], "zipsplit");
        co_await b_fputc('\n', stdout);
    }
    for (i = 0; i < sizeof(text) / sizeof(char *); i++) {
        co_await b_fprintf(stdout, text[i], VERSION, REVDATE);
        co_await b_fputc('\n', stdout);
    }
}

local Task<void> version_info(void)
// Print verbose info about program version and compile time options
// to stdout.
{
    extent i; // counter in text arrays

    // Options info array
    static ZCONST char *comp_opts[] = { NULL };

    for (i = 0; i < sizeof(versinfolines) / sizeof(char *); i++) {
        co_await b_fprintf(stdout, versinfolines[i], "ZipSplit", VERSION, REVDATE);
        co_await b_fputc('\n', stdout);
    }

    co_await version_local();

    co_await b_puts("ZipSplit special compilation options:");
    for (i = 0; (int)i < (int)(sizeof(comp_opts) / sizeof(char *) - 1); i++) {
        co_await b_fprintf(stdout, "\t%s\n", comp_opts[i]);
    }
    if (i == 0)
        co_await b_puts("\t[none]");
}

local extent simple(uzoff_t *a, extent n, uzoff_t c, uzoff_t d)
{
    extent k;  // current bin number
    uzoff_t t; // space used in current bin

    t = k = 0;
    while (n--) {
        if (*a + t > c - (k == 0 ? d : 0)) {
            k++;
            t = 0;
        }
        t += *a;
        *a++ = k;
    }
    return k + 1;
}

local int descmp(ZCONST zvoid *a, ZCONST zvoid *b)
{
    return **(uzoff_t **)a < **(uzoff_t **)b ? 1 : (**(uzoff_t **)a > **(uzoff_t **)b ? -1 : 0);
}

local Task<extent> greedy(uzoff_t *a, extent n, uzoff_t c, uzoff_t d)
{
    uzoff_t *b;  // space left in each bin (malloc'ed for each m)
    uzoff_t *e;  // copy of argument a[] (malloc'ed)
    extent i;    // steps through items
    extent j;    // steps through bins
    extent k;    // best bin to put current item in
    extent m;    // current number of bins
    uzoff_t **s; // pointers to e[], sorted descending (malloc'ed)
    uzoff_t t;   // space left in best bin (index k)

    // Algorithm:
    // 1. Copy a[] to e[] and sort pointers to e[0..n-1] (in s[]), in
    // descending order.
    // 2. Compute total of s[] and set m to the smallest number of bins of
    // capacity c that can hold the total.
    // 3. Allocate m bins.
    // 4. For each item in s[], starting with the largest, put it in the
    // bin with the smallest current capacity greater than or equal to the
    // item's size.  If no bin has enough room, increment m and go to step 4.
    // 5. Else, all items ended up in a bin--co_return m.

    // Copy a[] to e[], put pointers to e[] in s[], and sort s[].  Also compute
    // the initial number of bins (minus 1).
    if ((e = (uzoff_t *)malloc(n * sizeof(uzoff_t))) == NULL ||
        (s = (uzoff_t **)malloc(n * sizeof(uzoff_t *))) == NULL) {
        if (e != NULL)
            free((zvoid *)e);
        ziperr(ZE_MEM, "was trying a smart split");
        co_return 0; // only to make compiler happy
    }
    memcpy((char *)e, (char *)a, n * sizeof(uzoff_t));
    for (t = i = 0; i < n; i++)
        t += *(s[i] = e + i);
    m = (extent)((t + c - 1) / c) - 1; // pre-decrement for loop
    // mergesort: equal sizes are common, and their order decides which output
    // file each lands in. qsort here is not stable.
    if (mergesort((char *)s, n, sizeof(ulg *), descmp) != 0) {
        free((zvoid *)s);
        free((zvoid *)e);
        ziperr(ZE_MEM, "was trying a smart split");
        co_return 0; // only to make compiler happy
    }

    // Stuff bins until successful
    do {
        // Increment the number of bins, allocate and initialize bins
        if ((b = (uzoff_t *)malloc(++m * sizeof(uzoff_t))) == NULL) {
            free((zvoid *)s);
            free((zvoid *)e);
            ziperr(ZE_MEM, "was trying a smart split");
        }
        b[0] = c - d; // leave space in first bin
        for (j = 1; j < m; j++)
            b[j] = c;

        // Fill the bins greedily
        for (i = 0; i < n; i++) {
            // Find smallest bin that will hold item i (size s[i])
            t = c + 1;
            for (k = j = 0; j < m; j++)
                if (*s[i] <= b[j] && b[j] < t)
                    t = b[k = j];

            // If no bins big enough for *s[i], try next m
            if (t == c + 1)
                break;

            // Diminish that bin and save where it goes
            b[k] -= *s[i];
            a[(int)((uzoff_t *)(s[i]) - (uzoff_t *)e)] = k;
        }

        // Clean up
        free((zvoid *)b);

        // Do until all items put in a bin
    } while (i < n);

    // Done--clean up and co_return the number of bins needed
    free((zvoid *)s);
    free((zvoid *)e);
    co_return m;
}

// keep compiler happy until implement long options - 11/4/2003 EG
struct option_struct far options[] = {
    // short longopt        value_type        negatable        ID    name
    { "h", "help", o_NO_VALUE, o_NOT_NEGATABLE, 'h', "help" },
    // the end of the list
    { NULL, NULL, o_NO_VALUE, o_NOT_NEGATABLE, 0, NULL } // end has option_ID = 0
};

local Task<int> retry(void)
{
    char m[10];
    co_await b_fputs("Error writing to disk--redo entire disk? ", mesg);
    co_await b_fgets(m, 10, stdin);
    co_return *m == 'y' || *m == 'Y';
}

Task<i32> proc_main(Args args)
{
    char **argv;
    int argc = zargv(args, &argv);

    uzoff_t *a;           // malloc'ed list of sizes, dest bins
    extent *b;            // heads of bin linked lists (malloc'ed)
    uzoff_t c;            // bin capacity, start of central directory
    int d;                // if true, just report the number of disks
    FILE *e;              // input zip file
    FILE *f;              // output index and zip files
    extent g;             // number of bins from greedy(), entry to write
    int h;                // how to split--true means simple split, counter
    zoff_t i = 0;         // size of index file plus room to leave
    extent j;             // steps through zip entries, bins
    int k;                // next argument type
    extent *n = NULL;     // next item in bin list (heads in b)
    uzoff_t *p;           // malloc'ed list of sizes, dest bins for greedy()
    char *q;              // steps through option characters
    int r;                // temporary variable, counter
    extent s;             // number of bins needed
    zoff_t t;             // total of sizes, end of central directory
    int u;                // flag to wait for user on output files
    struct zlist far **w; // malloc'ed table for zfiles linked list
    int x;                // if true, make an index file
    struct zlist far *z;  // steps through zfiles linked list
    char errbuf[5000];

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

    // Informational messages are written to stdout.
    mesg = stdout;

    init_upper(); // build case map table

    // Go through argv
    // ^C is asked for; there is no handler, and a delivered signal
    // abandons the call the process is parked on.
    if (Task<Result<void>> sc = sig_catch(SIG_INT))
        co_await sc;
    k = h = x = d = u = 0;
    c                 = DEFSIZ;
    for (r = 1; r < argc; r++)
        if (*argv[r] == '-') {
            if (argv[r][1])
                for (q = argv[r] + 1; *q; q++)
                    switch (*q) {
                    case 'b': // Specify path for output files
                        if (k) {
                            ziperr(ZE_PARMS, "options are separate and precede zip file");
                        } else
                            k = 1; // Next non-option is path
                        break;
                    case 'h': // Show help
                        co_await help();
                        co_return ZE_OK;
                    case 'i': // Make an index file
                        x = 1;
                        break;
                    case 'l':
                    case 'L': // Show copyright and disclaimer
                        co_await license();
                        co_return ZE_OK;
                    case 'n': // Specify maximum size of resulting zip files
                        if (k) {
                            ziperr(ZE_PARMS, "options are separate and precede zip file");
                        } else
                            k = 2; // Next non-option is size
                        break;
                    case 'p':
                        u = 1;
                        break;
                    case 'q': // Quiet operation, suppress info messages
                        noisy = 0;
                        break;
                    case 'r':
                        if (k) {
                            ziperr(ZE_PARMS, "options are separate and precede zip file");
                        } else
                            k = 3; // Next non-option is room to leave
                        break;
                    case 's':
                        h = 1; // Only try simple
                        break;
                    case 't': // Just report number of disks
                        d = 1;
                        break;
                    case 'v': // Show version info
                        co_await version_info();
                        co_return ZE_OK;
                    default:
                        ziperr(ZE_PARMS, "Use option -h for help.");
                    }
            else
                ziperr(ZE_PARMS, "zip file cannot be stdin");
        } else
            switch (k) {
            case 0:
                if (zipfile == NULL) {
                    if ((zipfile = ziptyp(argv[r])) == NULL)
                        ziperr(ZE_MEM, "was processing arguments");
                } else
                    ziperr(ZE_PARMS, "can only specify one zip file");
                break;
            case 1:
                tempath = argv[r];
                k       = 0;
                break;
            case 2:
                if ((c = (ulg)atol(argv[r])) < 100) // 100 is smallest zip file
                    ziperr(ZE_PARMS, "invalid size given. Use option -h for help.");
                k = 0;
                break;
            default: // k must be 3
                i = (ulg)atol(argv[r]);
                k = 0;
                break;
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

    // Make a list of sizes and check against capacity.  Also compute the
    // size of the index file.
    c -= ENDHEAD + 4; // subtract overhead/zipfile
    if ((a = (uzoff_t *)talloc(zcount * sizeof(uzoff_t))) == NULL ||
        (w = (struct zlist far **)talloc(zcount * sizeof(struct zlist far *))) == NULL) {
        ziperr(ZE_MEM, "was computing split");
        co_return 1;
    }
    t = 0;
    for (j = 0, z = zfiles; j < zcount; j++, z = z->nxt) {
        w[j] = z;
        if (x)
            i += z->nam + 6 + NL;
        // New scanzip_reg only reads central directory so use cext for ext
        t += a[j] =
            8 + LOCHEAD + CENHEAD + 2 * (zoff_t)z->nam + 2 * (zoff_t)z->cext + z->com + z->siz;
        if (a[j] > c) {
            sprintf(errbuf, "Entry is larger than max split size of: %s",
                     zip_fzofft(c, NULL, "u"));
            co_await zipwarn(errbuf, "");
            co_await zipwarn("use -n to set split size", "");
            ziperr(ZE_BIG, z->zname);
        }
    }

    // Decide on split to use, report number of files
    if (h)
        s = simple(a, zcount, c, i);
    else {
        if ((p = (uzoff_t *)talloc(zcount * sizeof(uzoff_t))) == NULL)
            ziperr(ZE_MEM, "was computing split");
        memcpy((char *)p, (char *)a, zcount * sizeof(uzoff_t));
        s = simple(a, zcount, c, i);
        g = co_await greedy(p, zcount, c, i);
        if (s <= g)
            tfree((zvoid *)p);
        else {
            tfree((zvoid *)a);
            a = p;
            s = g;
        }
    }
    co_await b_fprintf(stdout, "%ld zip files w%s be made (%s%% efficiency)\n", (ulg)s,
                      d ? "ould" : "ill",
                      zip_fzofft(((200 * ((t + c - 1) / c)) / s + 1) / 2, NULL, "d"));
    if (d) {
        tfreeall();
        free((zvoid *)zipfile);
        zipfile = NULL;
        co_return ZE_OK;
    }

    // Set up path for output files
    // Point "name" past the path, where the filename should go
    if ((path = (char *)talloc(tempath == NULL ? 13 : strlen(tempath) + 14)) == NULL)
        ziperr(ZE_MEM, "was making output file names");
    if (tempath == NULL)
        name = path;
    else {
        // Copy the output path to the target
        strcpy(path, tempath);
        if (path[0] && path[strlen(path) - 1] != '/')
            strcat(path, "/");
        name = path + strlen(path);
    }

    // Make linked lists of results
    if ((b = (extent *)talloc(s * sizeof(extent))) == NULL ||
        (n = (extent *)talloc(zcount * sizeof(extent))) == NULL)
        ziperr(ZE_MEM, "was computing split");
    for (j = 0; j < s; j++)
        b[j] = (extent)-1;
    j = zcount;
    while (j--) {
        g    = (extent)a[j];
        n[j] = b[g];
        b[g] = j;
    }

    // Make a name namepat for the zip files that is eight or less characters
    // before the .zip, and that will not overwrite the original zip file.
    for (k = 1, j = s; j >= 10; j /= 10)
        k++;
    if (k > 7)
        ziperr(ZE_PARMS, "way too many zip files must be made");
    // XXX, ugly ....
    // Find the final "path" separator character
    if ((q = strrchr(zipfile, '/')) != NULL)
        q++;
    else
        q = zipfile;

    r = 0;
    while ((g = *q++) != '\0' && g != ZPATH_SEP && r < 8 - k)
        namepat[r++] = (char)g;
    if (r == 0)
        namepat[r++] = '_';
    else if (g >= '0' && g <= '9')
        namepat[r - 1] = (char)(namepat[r - 1] == '_' ? '-' : '_');
    sprintf(namepat + r, TEMPL_FMT, k);

    // Make the zip files from the linked lists of entry numbers
    if ((e = co_await b_fopen(zipfile, FOPR)) == NULL)
        ziperr(ZE_NAME, zipfile);
    free((zvoid *)zipfile);
    zipfile = NULL;
    for (j = 0; j < s; j++) {
        // jump here on a disk retry
    redobin:

        current_disk         = 0;
        cd_start_disk        = 0;
        cd_entries_this_disk = 0;

        // prompt if requested
        if (u) {
            char m[10];
            co_await b_fprintf(mesg, "Insert disk #%ld of %ld and hit return: ", (ulg)j + 1, (ulg)s);
            co_await b_fgets(m, 10, stdin);
        }

        // write index file on first disk if requested
        if (j == 0 && x) {
            strcpy(name, INDEX);
            co_await b_fprintf(stdout, "creating: %s\n", path);
            indexmade = 1;
            if ((f = co_await b_fopen(path, "w")) == NULL) {
                if (u && co_await retry())
                    goto redobin;
                ziperr(ZE_CREAT, path);
            }
            for (j = 0; j < zcount; j++)
                co_await b_fprintf(f, "%5s %s\n", zip_fzofft((a[j] + 1), NULL, "d"), w[j]->zname);

            if ((j = b_ferror(f)) != 0 || co_await b_fclose(f)) {
                if (j)
                    co_await b_fclose(f);
                if (u && co_await retry())
                    goto redobin;
                ziperr(ZE_WRITE, path);
            }
        }

        // create output zip file j
        sprintf(name, namepat, j + 1L);
        co_await b_fprintf(stdout, "creating: %s\n", path);
        zipsmade = j + 1;
        if ((y = f = co_await b_fopen(path, FOPW)) == NULL) {
            if (u && co_await retry())
                goto redobin;
            ziperr(ZE_CREAT, path);
        }
        bytes_this_split = 0;
        tempzn           = 0;

        // write local headers and copy compressed data
        for (g = b[j]; g != (extent)-1; g = (extent)n[g]) {
            if (co_await b_fseeko(e, w[g]->off, SEEK_SET))
                ziperr(b_ferror(e) ? ZE_READ : ZE_EOF, zipfile);
            in_file = e;
            if ((r = co_await zipcopy(w[g])) != ZE_OK) {
                if (r == ZE_TEMP) {
                    if (u && co_await retry())
                        goto redobin;
                    ziperr(ZE_WRITE, path);
                } else
                    ziperr(r, zipfile);
            }
        }

        // write central headers
        if ((c = co_await b_ftello(f)) == (uzoff_t)-1) {
            if (u && co_await retry())
                goto redobin;
            ziperr(ZE_WRITE, path);
        }
        for (g = b[j], k = 0; g != (extent)-1; g = n[g], k++)
            if ((r = co_await putcentral(w[g])) != ZE_OK) {
                if (u && co_await retry())
                    goto redobin;
                ziperr(ZE_WRITE, path);
            }

        // write end-of-central header
        cd_start_offset  = c;
        total_cd_entries = k;
        if ((t = co_await b_ftello(f)) == (zoff_t)-1 ||
            (r = co_await putend((zoff_t)k, t - c, c, (extent)0, (char *)NULL)) != ZE_OK ||
            b_ferror(f) || co_await b_fclose(f)) {
            if (u && co_await retry())
                goto redobin;
            ziperr(ZE_WRITE, path);
        }
    }
    co_await b_fclose(e);

    // Done!
    if (u)
        co_await b_fputs("Done.\n", mesg);
    tfreeall();

    co_return (0);
}
