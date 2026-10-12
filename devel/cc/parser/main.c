#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "scanner.h"
#include "xalloc.h"

//
// Level of scope for nested compound operators.
// Usually it comes from semantic level, but let's define it here instead.
//
int scope_level;

//
// The location of an error in the command line rather than the source.
//
static const SrcLoc no_loc;

//
// Enum for output format
//
typedef enum {
    FORMAT_AST, // Default: binary AST
    FORMAT_YAML
} OutputFormat;

//
// Structure to hold parsed arguments
//
typedef struct {
    int verbose;         // -v or --verbose
    int help;            // -h or --help
    int debug;           // -D or --debug
    OutputFormat format; // Output format (--ast, --yaml)
    char *input_file;    // Input filename
    char *output_file;   // Output filename (optional)
} Args;

//
// Function to print usage information
//
void print_usage(const char *prog_name)
{
    const char *p = strrchr(prog_name, '/');
    if (p) {
        prog_name = p + 1;
    }
    diag_printf("Usage:\n");
    diag_printf("    %s [options] input-filename [output-filename]\n", prog_name);
    diag_printf("Options:\n");
    diag_printf("    --ast            Emit AST in binary format (default)\n");
    diag_printf("    --yaml           Emit YAML format\n");
    diag_printf("    --max-errors=N   Stop after N errors (0: no limit; default 20)\n");
    diag_printf("    --no-show-source Don't show the source line under a diagnostic\n");
    diag_printf("    --color=WHEN     Color diagnostics: always, never or auto (default)\n");
    diag_printf("    -v, --verbose    Enable verbose mode\n");
    diag_printf("    -D, --debug      Print debug information\n");
    diag_printf("    -h, --help       Show this help message\n");
}

//
// Initialize Args structure with default values
//
void init_args(Args *args)
{
    args->verbose     = 0;
    args->help        = 0;
    args->debug       = 0;
    args->format      = FORMAT_AST; // Default format
    args->input_file  = NULL;
    args->output_file = NULL;
}

//
// Generate output filename from input filename based on format
//
char *generate_output_filename(const char *input_file, OutputFormat format)
{
    // Find the last '.' in input_file to replace extension
    const char *ext     = strrchr(input_file, '.');
    size_t base_len     = ext ? (size_t)(ext - input_file) : strlen(input_file);
    const char *new_ext = (format == FORMAT_YAML) ? ".yaml" : ".ast";
    size_t new_ext_len  = strlen(new_ext);

    // Allocate memory for new filename
    char *output_file = malloc(base_len + new_ext_len + 1);
    if (!output_file) {
        diag_error(diag_loc, "out of memory");
        return NULL;
    }

    // Copy base name and append new extension
    strncpy(output_file, input_file, base_len);
    strcpy(output_file + base_len, new_ext);
    return output_file;
}

//
// Parse command-line arguments using getopt_long
//
int parse_args(int argc, char *argv[], Args *args)
{
    static struct option long_options[] = {
        { "verbose", no_argument, 0, 'v' }, //
        { "help", no_argument, 0, 'h' },    //
        { "debug", no_argument, 0, 'D' },   //
        { "ast", no_argument, 0, 'a' },     //
        { "yaml", no_argument, 0, 'y' },    //
        { "max-errors", required_argument, 0, 256 },
        { "no-show-source", no_argument, 0, 257 },
        { "color", required_argument, 0, 258 },
        {}, //
    };

    int opt;
    int option_index = 0;

    if (argc < 2) {
        // Show usage.
        args->help = 1;
        return 0;
    }
    while ((opt = getopt_long(argc, argv, "vhD", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'v':
            args->verbose = 1;
            break;
        case 'h':
            args->help = 1;
            return 0;
        case 'D':
            args->debug = 1;
            break;
        // Long options without short equivalents
        case 'y':
            args->format = FORMAT_YAML;
            break;
        case 'a':
            args->format = FORMAT_AST;
            break;
        case 256:
            diag_max_errors = atoi(optarg);
            break;
        case 257:
            diag_show_source = 0;
            break;
        case 258:
            if (diag_set_color(optarg) < 0) {
                return -1;
            }
            break;
        case '?': // Unknown option
            return -1;
        }
    }

    // Check for input filename (required)
    if (optind < argc) {
        args->input_file = argv[optind++];
    } else {
        diag_error(diag_loc, "no input file");
        return -1;
    }

    // Check for output filename (optional)
    if (optind < argc) {
        args->output_file = argv[optind];
    } else {
        // Generate output filename based on input and format
        args->output_file = generate_output_filename(args->input_file, args->format);
        if (!args->output_file) {
            return -1;
        }
    }

    return 0;
}

//
// The text of the input and of every file its line markers name, for the caret
// under a diagnostic (srcloc_set_text): a plain function cannot open a file on Braam.
// A file that cannot be read is left out.
//
static coro(braam_call *) void load_sources(const char *name, const char *text, size_t len)
{
    srcloc_set_text(name, text, len);
    const char *loaded[64];
    int nloaded = 0;
    for (size_t at = 0; at < len; at++) {
        // A line '# <digits> "<name>"', at the start of a line.
        if ((at > 0 && text[at - 1] != '\n') || text[at] != '#')
            continue;
        size_t p = at + 1;
        while (p < len && text[p] == ' ')
            p++;
        if (p == len || text[p] < '0' || text[p] > '9')
            continue;
        while (p < len && text[p] >= '0' && text[p] <= '9')
            p++;
        while (p < len && text[p] == ' ')
            p++;
        if (p == len || text[p] != '"')
            continue;
        size_t q = ++p;
        while (q < len && text[q] != '"' && text[q] != '\n' && text[q] != '\\')
            q++;
        if (q == len || text[q] != '"' || q - p >= 512)
            continue;
        char path[512];
        memcpy(path, text + p, q - p);
        path[q - p]          = 0;
        const char *interned = srcloc_intern(path);
        int seen             = interned == srcloc_intern(name);
        for (int k = 0; k < nloaded && !seen; k++)
            seen = loaded[k] == interned;
        if (seen || nloaded == 64)
            continue;
        loaded[nloaded++] = interned;
        size_t n;
        char *src = await read_file(path, &n);
        if (src)
            srcloc_set_text(path, src, n);
    }
}

//
// Main processing function
//
coro(braam_call *) void process_file(const Args *args)
{
    if (args->verbose) {
        await printf("Processing %s in verbose mode\n", args->input_file);
    }
    if (args->debug) {
        trace_printf("Debug: Format = %d, Input = %s, Output = %s\n", args->format,
                     args->input_file, args->output_file);
        parser_debug = 1;
        import_debug = 1;
        export_debug = 1;
        wio_debug    = 1;
    }
    size_t input_len;
    char *input_text = await read_file(args->input_file, &input_len);
    if (!input_text) {
        diag_error(no_loc, "cannot open '%s': %s", args->input_file, strerror(errno));
        exit(1);
    }
    scanner_set_input_name(args->input_file);
    await load_sources(args->input_file, input_text, input_len);
    Program *program = parse(input_text, input_len);

    FILE *output_file = stdout;
    if (args->output_file[0] != '-') {
        output_file = await fopen(args->output_file, "w");
        if (!output_file) {
            diag_error(no_loc, "cannot create '%s': %s", args->output_file, strerror(errno));
            exit(1);
        }
    }

    switch (args->format) {
    default:
    case FORMAT_AST:
        if (args->verbose) {
            await printf("Emitting AST in binary format to %s\n", args->output_file);
        }
        if (args->debug) {
            await print_program(stdout, program);
        }
        await export_ast(fileno(output_file), program);
        break;
    case FORMAT_YAML:
        if (args->verbose) {
            await printf("Emitting YAML format to %s\n", args->output_file);
        }
        await export_yaml(output_file, program);
        break;
    }

    if (output_file != stdout) {
        await fclose(output_file);
    }
    free_program(program);
    nametab_destroy();
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
    diag_unwind();
}

coro(braam_call *) int main(int argc, char *argv[])
{
    Args args;
    init_args(&args);
    await diag_init_color();
    diag_progname   = argv[0];
    parser_recovery = true;
    diag_max_errors = 20;

    if (parse_args(argc, argv, &args) != 0) {
        print_usage(argv[0]);
        return 1;
    }

    if (args.help) {
        print_usage(argv[0]);
        return 0;
    }

    // Pass args to backend for processing
    await process_file(&args);

    return 0;
}
