// zipcloak.cpp — zipcloak.c, by Mark Adler.
//
// One of the three tools built from zip's own sources: it reads an archive
// and writes it back, and never compresses. Upstream built them with -DUTIL,
// which strips the create path out of the shared files; here they link the
// same objects and --gc-sections keeps what is not reached out of the binary.
//
// Its own message routines are what the shared files call, so this file
// supplies ziperr_msg, zipwarn, zipmessage and zipmessage_nl.

#include "crc32.h"
#include "crypt.h"
#include "revision.h"
#include "ttyio.h"
#include "zip.h"

local Task<void> license OF((void));
local Task<void> help OF((void));
local Task<void> version_info OF((void));

// Temporary zip file pointer
local FILE *tempzf;

// Pointer to CRC-32 table (used for decryption/encryption)
// crc_32_tab is globals.cpp's, as it is for every one of the four.

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

// *********************************************************************
// Issue a message for the error, clean up files and memory, and exit.
Task<void> ziperr_msg(int code, ZCONST char *msg)
{
    co_await b_fprintf(mesg, "zipcloak error: %s (%s)\n", ZIPERRORS(code), msg);
    if (tempzf != NULL)
        co_await b_fclose(tempzf);
    if (tempzip != NULL) {
        co_await destroy(tempzip);
        free((zvoid *)tempzip);
    }
    if (zipfile != NULL)
        free((zvoid *)zipfile);
    zip_fail(code, "");
}

// *********************************************************************
// Print a warning message to mesg (usually stderr) and return.
Task<void> zipwarn(ZCONST char *msg1, ZCONST char *msg2)
{
    co_await b_fprintf(mesg, "zipcloak warning: %s%s\n", msg1, msg2);
}

// *********************************************************************
// Upon getting a user interrupt, turn echo back on for tty and abort
// cleanly using ziperr().

static ZCONST char *publicnote[] = {
    "The encryption code of this program is not copyrighted and is",
    "put in the publicnote domain. It was originally written in Europe",
    "and can be freely distributed in both source and object forms",
    "from any country, including the USA under License Exception",
    "TSU of the U.S. Export Administration Regulations (section",
    "740.13(e)) of 6 June 2002.  (Prior to January 2000, re-export",
    "from the US was a violation of US law.)"
};

// *********************************************************************
// Print license information to stdout.
local Task<void> license(void)
{
    extent i; // counter for copyright array

    for (i = 0; i < sizeof(swlicense) / sizeof(char *); i++) {
        co_await b_puts(swlicense[i]);
    }
    co_await b_fputc('\n', stdout);
    co_await b_fprintf(stdout, "Export notice:\n");
    for (i = 0; i < sizeof(publicnote) / sizeof(char *); i++) {
        co_await b_puts(publicnote[i]);
    }
}

static ZCONST char *help_info[] = {
    "",
    "ZipCloak %s (%s)",
    "Usage:  zipcloak [-dq] [-b path] zipfile",
    "  the default action is to encrypt all unencrypted entries in the zip file",
    "",
    "  -d  --decrypt      decrypt encrypted entries (copy if given wrong password)",
    "  -b  --temp-path    use \"path\" for the temporary zip file",
    "  -O  --output-file  write output to new zip file",
    "  -q  --quiet        quiet operation, suppress some informational messages",
    "  -h  --help         show this help",
    "  -v  --version      show version info",
    "  -L  --license      show software license"
};

// *********************************************************************
// Print help (along with license info) to stdout.
local Task<void> help(void)
{
    extent i; // counter for help array

    for (i = 0; i < sizeof(help_info) / sizeof(char *); i++) {
        co_await b_fprintf(stdout, help_info[i], VERSION, REVDATE);
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

    for (i = 0; i < sizeof(copyright) / sizeof(char *); i++) {
        co_await b_fprintf(stdout, copyright[i], "zipcloak");
        co_await b_fputc('\n', stdout);
    }
    co_await b_fputc('\n', stdout);

    for (i = 0; i < sizeof(versinfolines) / sizeof(char *); i++) {
        co_await b_fprintf(stdout, versinfolines[i], "ZipCloak", VERSION, REVDATE);
        co_await b_fputc('\n', stdout);
    }

    co_await version_local();

    co_await b_puts("ZipCloak special compilation options:");
    for (i = 0; (int)i < (int)(sizeof(comp_opts) / sizeof(char *) - 1); i++) {
        co_await b_fprintf(stdout, "\t%s\n", comp_opts[i]);
    }
    co_await b_fprintf(stdout, "\t[encryption, version %d.%d%s of %s]\n", CR_MAJORVER, CR_MINORVER,
                      CR_BETA_VER, CR_VERSION_DATE);
}

// options for zipcloak - 3/5/2004 EG
struct option_struct far options[] = {
    // short longopt        value_type        negatable        ID    name
    { "b", "temp-path", o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'b', "path for temp file" },
    { "d", "decrypt", o_NO_VALUE, o_NOT_NEGATABLE, 'd', "decrypt" },
    { "h", "help", o_NO_VALUE, o_NOT_NEGATABLE, 'h', "help" },
    { "L", "license", o_NO_VALUE, o_NOT_NEGATABLE, 'L', "license" },
    { "l", "", o_NO_VALUE, o_NOT_NEGATABLE, 'L', "license" },
    { "O", "output-file", o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'O', "output to new archive" },
    { "v", "version", o_NO_VALUE, o_NOT_NEGATABLE, 'v', "version" },
    // the end of the list
    { NULL, NULL, o_NO_VALUE, o_NOT_NEGATABLE, 0, NULL } // end has option_ID = 0
};

// *********************************************************************
// Encrypt or decrypt all of the entries in a zip file.  See the command
// help in help() above.

Task<i32> proc_main(Args args)
{
    char **argv;
    int argc = zargv(args, &argv);

    int attr;                  // attributes of zip file
    zoff_t start_offset;       // start of central directory
    int decrypt;               // decryption flag
    int temp_path;             // 1 if next argument is path for temp files
    char passwd[IZ_PWLEN + 1]; // password for encryption or decryption
    char verify[IZ_PWLEN + 1]; // password for encryption or decryption
    int res;                   // result code
    zoff_t length;             // length of central directory
    FILE *inzip, *outzip;      // input and output zip files
    struct zlist far *z;       // steps through zfiles linked list
    // used by get_option
    unsigned long option; // option ID returned by get_option
    int argcnt  = 0;      // current argcnt in argv
    int argnum  = 0;      // arg number
    int optchar = 0;      // option state
    char *value = NULL;   // non-option arg, option value or NULL
    int negated = 0;      // 1 = option negated
    int fna     = 0;      // current first non-opt arg
    int optnum  = 0;      // index in table

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

    crc_32_tab = get_crc_table();
    // initialize crc table for crypt

    // Go through argv
    zipfile = tempzip = NULL;
    tempzf            = NULL;
    // ^C is asked for; there is no handler, and a delivered signal
    // abandons the call the process is parked on.
    if (Task<Result<void>> sc = sig_catch(SIG_INT))
        co_await sc;
    temp_path = decrypt = 0;

    // new command line

    zipfile  = NULL;
    out_path = NULL;

    // make copy of argv that can use with insert_arg()
    argv = co_await copy_args(argv, 0);

    // -------------------------------------------
    // Process command line using get_option
    // -------------------------------------------
    //
    // Each call to get_option() returns either a command
    // line option and possible value or a non-option argument.
    // Arguments are permuted so that all options (-r, -b temp)
    // are returned before non-option arguments (zipfile).
    // Returns 0 when nothing left to read.

    // set argnum = 0 on first call to init get_option
    argnum = 0;

    // get_option returns the option ID and updates parameters:
    // argv    - usually same as argv if no argument file support
    // argcnt  - current argc for argv
    // value   - char* to value (free() when done with it) or NULL if no value
    // negated - option was negated with trailing -

    while ((option = co_await get_option(&argv, &argcnt, &argnum, &optchar, &value, &negated, &fna,
                                         &optnum, 0))) {
        switch (option) {
        case 'b': // Specify path for temporary file
            if (temp_path) {
                ziperr(ZE_PARMS, "more than one temp_path");
            }
            temp_path = 1;
            tempath   = value;
            break;
        case 'd':
            decrypt = 1;
            break;
        case 'h': // Show help
            co_await help();
            co_return ZE_OK;
        case 'l':
        case 'L': // Show copyright and disclaimer
            co_await license();
            co_return ZE_OK;
        case 'O': // Output to new zip file instead of updating original zip file
            if ((out_path = ziptyp(value)) == NULL) {
                ziperr(ZE_MEM, "was processing arguments");
            }
            free(value);
            break;
        case 'q': // Quiet operation, suppress info messages
            noisy = 0;
            break;
        case 'v': // Show version info
            co_await version_info();
            co_return ZE_OK;
        case o_NON_OPTION_ARG:
            // not an option
            // no more options as permuting
            // just dash also ends up here

            if (strcmp(value, "-") == 0) {
                ziperr(ZE_PARMS, "zip file cannot be stdin");
            } else if (zipfile != NULL) {
                ziperr(ZE_PARMS, "can only specify one zip file");
            }

            if ((zipfile = ziptyp(value)) == NULL) {
                ziperr(ZE_MEM, "was processing arguments");
            }
            free(value);
            break;

        default:
            ziperr(ZE_PARMS, "unknown option");
        }
    }

    free_args(argv);

    if (zipfile == NULL)
        ziperr(ZE_PARMS, "need to specify zip file");

    // in_path is the input zip file
    if ((in_path = (char *)malloc(strlen(zipfile) + 1)) == NULL) {
        ziperr(ZE_MEM, "input");
    }
    strcpy(in_path, zipfile);

    // out_path defaults to in_path
    if (out_path == NULL) {
        if ((out_path = (char *)malloc(strlen(zipfile) + 1)) == NULL) {
            ziperr(ZE_MEM, "output");
        }
        strcpy(out_path, zipfile);
    }

    // Read zip file
    if ((res = co_await readzipfile()) != ZE_OK)
        ziperr(res, zipfile);
    if (zfiles == NULL)
        ziperr(ZE_NAME, zipfile);

    // Check for something to do
    for (z = zfiles; z != NULL; z = z->nxt) {
        if (decrypt ? z->flg & 1 : !(z->flg & 1))
            break;
    }
    if (z == NULL) {
        ziperr(ZE_NONE, decrypt ? "no encrypted files" : "all files encrypted already");
    }

    // Before we get carried away, make sure zip file is writeable
    if ((inzip = co_await b_fopen(zipfile, "a")) == NULL)
        ziperr(ZE_CREAT, zipfile);
    co_await b_fclose(inzip);
    attr = getfileattr(zipfile);

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
        if ((y = tempzf = outzip = co_await b_fopen(tempzip, FOPW_TMP)) == NULL) {
            ZIPERR(ZE_TEMP, tempzip);
        }
    }

    // Get password
    if (co_await getp("Enter password: ", passwd, IZ_PWLEN + 1) == NULL)
        ziperr(ZE_PARMS, "no terminal to read a password from");

    if (decrypt == 0) {
        if (co_await getp("Verify password: ", verify, IZ_PWLEN + 1) == NULL)
            ziperr(ZE_PARMS, "no terminal to read a password from");

        if (strcmp(passwd, verify))
            ziperr(ZE_PARMS, "password verification failed");

        if (*passwd == '\0')
            ziperr(ZE_PARMS, "zero length password not allowed");
    }

    // Open input zip file again, copy preamble if any
    if ((in_file = co_await b_fopen(zipfile, FOPR)) == NULL)
        ziperr(ZE_NAME, zipfile);

    if (zipbeg && (res = co_await bfcopy(zipbeg)) != ZE_OK) {
        ziperr(res, res == ZE_TEMP ? tempzip : zipfile);
    }
    tempzn = zipbeg;

    // Go through local entries, copying, encrypting, or decrypting
    for (z = zfiles; z != NULL; z = z->nxt) {
        if (decrypt && (z->flg & 1)) {
            co_await b_fprintf(stdout, "decrypting: %s", z->zname);
            co_await b_fflush(stdout);
            if ((res = co_await zipbare(z, passwd)) != ZE_OK) {
                if (res != ZE_MISS)
                    ziperr(res, "was decrypting an entry");
                co_await b_fprintf(stdout, " (wrong password--just copying)");
                co_await b_fflush(stdout);
            }
            co_await b_fputc('\n', stdout);

        } else if ((!decrypt) && !(z->flg & 1)) {
            co_await b_fprintf(stdout, "encrypting: %s\n", z->zname);
            co_await b_fflush(stdout);
            if ((res = co_await zipcloak(z, passwd)) != ZE_OK) {
                ziperr(res, "was encrypting an entry");
            }
        } else {
            co_await b_fprintf(stdout, "   copying: %s\n", z->zname);
            co_await b_fflush(stdout);
            if ((res = co_await zipcopy(z)) != ZE_OK) {
                ziperr(res, "was copying an entry");
            }
        } // if
    } // for

    co_await b_fclose(in_file);

    // Write central directory and end of central directory

    // get start of central
    if ((start_offset = co_await b_ftello(outzip)) == (zoff_t)-1)
        ziperr(ZE_TEMP, tempzip);

    for (z = zfiles; z != NULL; z = z->nxt) {
        if ((res = co_await putcentral(z)) != ZE_OK)
            ziperr(res, tempzip);
    }

    // get end of central
    if ((length = co_await b_ftello(outzip)) == (zoff_t)-1)
        ziperr(ZE_TEMP, tempzip);

    length -= start_offset; // compute length of central
    if ((res = co_await putend((zoff_t)zcount, length, start_offset, zcomlen, zcomment)) != ZE_OK) {
        ziperr(res, tempzip);
    }
    tempzf = NULL;
    if (co_await b_fclose(outzip))
        ziperr(ZE_TEMP, tempzip);
    if ((res = co_await replace(out_path, tempzip)) != ZE_OK) {
        co_await zipwarn("new zip file left as: ", tempzip);
        free((zvoid *)tempzip);
        tempzip = NULL;
        ziperr(res, "was replacing the original zip file");
    }
    free((zvoid *)tempzip);
    tempzip = NULL;
    setfileattr(zipfile, attr);
    free((zvoid *)in_path);
    free((zvoid *)out_path);

    free((zvoid *)zipfile);
    zipfile = NULL;

    // Done!
    co_return (0);
}
