// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       args.h
/// \brief      Argument parsing
//
//  Authors:    Lasse Collin
//              Jia Tan
//
///////////////////////////////////////////////////////////////////////////////

typedef struct {
    /// Full command line (views into the process argv blob).
    Args args;

    /// Index in \a args of the first file operand, if any.
    usize file_index;

    /// Number of file operands; zero when \a stdin_only is set.
    unsigned int file_count;

    /// No file operands were given; read from standard input.
    bool stdin_only;

    /// Name of the file from which to read filenames. This is NULL
    /// if --files or --files0 was not used.
    char *files_name;

    /// File opened for reading from which filenames are read. This is
    /// non-NULL only if files_name is non-NULL.
    FILE *files_file;

    /// Delimiter for filenames read from files_file
    char files_delim;

} args_info;

extern bool opt_stdout;
extern bool opt_force;
extern bool opt_keep_original;
extern bool opt_synchronous;
extern bool opt_robot;
extern bool opt_ignore_check;

extern const char stdin_filename[];

extern void args_parse(args_info *args, Args cmdargs);
extern void args_free(void);
