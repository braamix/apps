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
    FORMAT_YAML,
    FORMAT_DOT
} OutputFormat;

//
// Structure to hold parsed arguments
//
typedef struct {
    int verbose;         // -v or --verbose
    int help;            // -h or --help
    int debug;           // -D or --debug
    OutputFormat format; // Output format (--ast, --yaml, --dot)
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
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "    %s [options] input-filename [output-filename]\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    --ast            Emit AST in binary format (default)\n");
    fprintf(stderr, "    --yaml           Emit YAML format\n");
    fprintf(stderr, "    --dot            Emit Graphviz DOT script\n");
    fprintf(stderr, "    --max-errors=N   Stop after N errors (0: no limit; default 20)\n");
    fprintf(stderr, "    --no-show-source Don't show the source line under a diagnostic\n");
    fprintf(stderr, "    --color=WHEN     Color diagnostics: always, never or auto (default)\n");
    fprintf(stderr, "    -v, --verbose    Enable verbose mode\n");
    fprintf(stderr, "    -D, --debug      Print debug information\n");
    fprintf(stderr, "    -h, --help       Show this help message\n");
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
    const char *new_ext = (format == FORMAT_DOT)    ? ".dot"
                          : (format == FORMAT_YAML) ? ".yaml"
                                                    : ".ast";
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
        { "dot", no_argument, 0, 'd' },     //
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
        case 'd':
            args->format = FORMAT_DOT;
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
// Main processing function
//
void process_file(const Args *args)
{
    if (args->verbose) {
        printf("Processing %s in verbose mode\n", args->input_file);
    }
    if (args->debug) {
        printf("Debug: Format = %d, Input = %s, Output = %s\n", args->format, args->input_file,
               args->output_file);
        parser_debug = 1;
        import_debug = 1;
        export_debug = 1;
        wio_debug    = 1;
    }
    FILE *input_file = fopen(args->input_file, "r");
    if (!input_file) {
        diag_error(no_loc, "cannot open '%s': %s", args->input_file, strerror(errno));
        exit(1);
    }
    scanner_set_input_name(args->input_file);
    Program *program = parse(input_file);
    fclose(input_file);

    FILE *output_file = stdout;
    if (args->output_file[0] != '-') {
        output_file = fopen(args->output_file, "w");
        if (!output_file) {
            diag_error(no_loc, "cannot create '%s': %s", args->output_file, strerror(errno));
            exit(1);
        }
    }

    switch (args->format) {
    default:
    case FORMAT_AST:
        if (args->verbose) {
            printf("Emitting AST in binary format to %s\n", args->output_file);
        }
        if (args->debug) {
            print_program(stdout, program);
        }
        export_ast(fileno(output_file), program);
        break;
    case FORMAT_YAML:
        if (args->verbose) {
            printf("Emitting YAML format to %s\n", args->output_file);
        }
        export_yaml(output_file, program);
        break;
    case FORMAT_DOT:
        if (args->verbose) {
            printf("Emitting Graphviz DOT script to %s\n", args->output_file);
        }
        export_dot(output_file, program);
        break;
    }

    if (output_file != stdout) {
        fclose(output_file);
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

int main(int argc, char *argv[])
{
    Args args;
    init_args(&args);
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
    process_file(&args);

    return 0;
}
