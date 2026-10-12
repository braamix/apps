#include "driver.h"

#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srcloc.h"
#include "tac.h"
#include "wio.h"
#include "xalloc.h"

static FILE *output_file;

//
// Structure to hold parsed arguments
//
typedef struct {
    int verbose;       // -v or --verbose
    int help;          // -h or --help
    int debug;         // -D or --debug
    char *input_file;  // Input filename
    char *output_file; // Output filename (optional)
} Args;

// Long-option values for the backend flags start here (outside the ASCII range so
// they do not collide with the short options).
enum { OPT_COLOR = 999, OPT_BACKEND = 1000 };

static int num_flags(const Backend *backend)
{
    int n = 0;
    if (backend->flags) {
        while (backend->flags[n].name)
            n++;
    }
    return n;
}

//
// Function to print usage information
//
static void print_usage(const char *prog_name, const Backend *backend)
{
    const char *p = strrchr(prog_name, '/');
    if (p) {
        prog_name = p + 1;
    }
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "    %s [options] input-filename [output-filename]\n", prog_name);
    fprintf(stderr, "Options:\n");
    for (int i = 0; i < num_flags(backend); i++) {
        fprintf(stderr, "        --%-14s%s\n", backend->flags[i].name, backend->flags[i].help);
    }
    fprintf(stderr, "        --color=WHEN    Color diagnostics: always, never or auto (default)\n");
    fprintf(stderr, "    -v, --verbose       Enable verbose mode\n");
    fprintf(stderr, "    -D, --debug         Print debug information\n");
    fprintf(stderr, "    -h, --help          Show this help message\n");
}

//
// Generate output filename from input filename
//
static char *generate_output_filename(const char *input_file, const char *new_ext)
{
    // Find the last '.' in input_file to replace extension
    const char *ext    = strrchr(input_file, '.');
    size_t base_len    = ext ? (size_t)(ext - input_file) : strlen(input_file);
    size_t new_ext_len = strlen(new_ext);

    // Allocate memory for new filename
    char *filename = malloc(base_len + new_ext_len + 1);
    if (!filename) {
        diag_error(diag_loc, "out of memory");
        return NULL;
    }

    // Copy base name and append new extension
    strncpy(filename, input_file, base_len);
    strcpy(filename + base_len, new_ext);
    return filename;
}

//
// Parse command-line arguments using getopt_long
//
static int parse_args(int argc, char *argv[], Args *args, const Backend *backend)
{
    int nflags                  = num_flags(backend);
    struct option *long_options = calloc(nflags + 5, sizeof(struct option));
    if (!long_options) {
        diag_error(diag_loc, "out of memory");
        return -1;
    }
    long_options[0] = (struct option){ "verbose", no_argument, 0, 'v' };
    long_options[1] = (struct option){ "help", no_argument, 0, 'h' };
    long_options[2] = (struct option){ "debug", no_argument, 0, 'D' };
    long_options[3] = (struct option){ "color", required_argument, 0, OPT_COLOR };
    for (int i = 0; i < nflags; i++) {
        long_options[4 + i] =
            (struct option){ backend->flags[i].name, no_argument, 0, OPT_BACKEND + i };
    }

    int opt;
    int option_index = 0;
    int status       = 0;

    if (argc < 2) {
        // Show usage.
        args->help = 1;
        goto done;
    }
    while ((opt = getopt_long(argc, argv, "vhD", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'v':
            args->verbose = 1;
            break;
        case 'h':
            args->help = 1;
            goto done;
        case 'D':
            args->debug = 1;
            break;
        case OPT_COLOR:
            if (diag_set_color(optarg) < 0) {
                status = -1;
                goto done;
            }
            break;
        case '?': // Unknown option
            status = -1;
            goto done;
        default:
            backend->flag(opt - OPT_BACKEND);
            break;
        }
    }

    // Check for input filename (required)
    if (optind < argc) {
        args->input_file = argv[optind++];
    } else {
        diag_error(diag_loc, "no input file");
        status = -1;
        goto done;
    }

    // Check for output filename (optional)
    if (optind < argc) {
        args->output_file = argv[optind];
    } else {
        // Generate output filename based on input
        args->output_file = generate_output_filename(args->input_file, backend->output_ext());
        if (!args->output_file) {
            status = -1;
        }
    }
done:
    free(long_options);
    return status;
}

_Noreturn void fatal_error(const char *message, ...);

static void open_output(const Args *args)
{
    output_file = stdout;
    if (args->output_file[0] != '-') {
        output_file = fopen(args->output_file, "w");
        if (!output_file) {
            fatal_error("cannot create '%s': %s", args->output_file, strerror(errno));
        }
    }
}

static void close_output(void)
{
    if (output_file != stdout) {
        fclose(output_file);
    }
}

//
// Main processing function
//
static void process_file(const Args *args, const Backend *backend)
{
    if (args->verbose) {
        printf("Processing %s in verbose mode\n", args->input_file);
    }
    if (args->debug) {
        printf("Debug: Input = %s, Output = %s\n", args->input_file, args->output_file);
    }
    open_output(args);

    WFILE input;
    if (wopen(&input, args->input_file, "r") < 0) {
        fatal_error("cannot open '%s': %s", args->input_file, strerror(errno));
    }
    if (!tac_import_begin_stream(&input)) {
        fatal_error("'%s' is not TAC from this version of the compiler", args->input_file);
    }

    // Phase 1: read all toplevels into a linked chain for global-name resolution.
    Tac_TopLevel *head = NULL, **tail_ptr = &head;
    for (;;) {
        Tac_TopLevel *tac = tac_import_toplevel(&input);
        if (!tac)
            break;
        *tail_ptr = tac;
        tail_ptr  = &tac->next;
    }
    wclose(&input);

    // Phase 2: codegen each toplevel with the full program chain as context.
    for (const Tac_TopLevel *tl = head; tl; tl = tl->next) {
        if (args->debug)
            tac_print_toplevel(stdout, tl, 0);
        backend->codegen(head, tl, output_file);
    }
    tac_free_toplevel(head);
    close_output();

    if (args->debug) {
        xreport_lost_memory();
    }
    xfree_all();
}

//
// Error handling
//
void _Noreturn fatal_error(const char *message, ...)
{
    va_list ap;
    va_start(ap, message);
    diag_vreport(diag_loc, "error", message, ap);
    va_end(ap);
    exit(1);
}

int backend_main(int argc, char *argv[], const Backend *backend)
{
    Args args = { 0 };

    diag_progname = argv[0];
    if (parse_args(argc, argv, &args, backend) != 0) {
        print_usage(argv[0], backend);
        return 1;
    }

    if (args.help) {
        print_usage(argv[0], backend);
        return 0;
    }

    process_file(&args, backend);
    return 0;
}
