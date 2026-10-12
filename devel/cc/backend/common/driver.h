#ifndef BACKEND_DRIVER_H
#define BACKEND_DRIVER_H

#include <stdio.h>

#include "tac.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// The command-line driver shared by every code generator: argument parsing, TAC
// import, the per-toplevel loop and the output file.  A backend describes itself
// with a Backend and calls backend_main() from its own main().
//

// A backend-specific long option without an argument, e.g. --madlen.
typedef struct {
    const char *name; // option name, without the leading "--"
    const char *help; // one-line description for the usage text
} BackendFlag;

typedef struct {
    const BackendFlag *flags; // backend-specific flags, terminated by a NULL name; may be NULL
    // Called once per backend flag given on the command line, with its index in `flags`.
    void (*flag)(int index);
    // Extension of the default output file name (e.g. ".s"), asked after the flags.
    const char *(*output_ext)(void);
    // Translate one toplevel to assembly.  `program` heads the whole translation unit.
    void (*codegen)(const Tac_TopLevel *program, const Tac_TopLevel *tl, FILE *out);
} Backend;

int backend_main(int argc, char *argv[], const Backend *backend);

#ifdef __cplusplus
}
#endif

#endif // BACKEND_DRIVER_H
