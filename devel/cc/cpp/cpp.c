// UNIX V7 source code: see /COPYRIGHT or www.tuhs.org for details.

//
// C preprocessor
// written by John F. Reiser
// July/August 1978
//
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "intern.h"

//
// The one and only instance of the preprocessor's mutable state, declared in
// defs.h.  Bundles what used to be ~40 file-scope globals.
//
struct cppstate cpp;

//
// The four large arrays, kept out of `cpp' so that no struct on this machine
// exceeds 4,096 words (defs.h says why).  Nothing else about them is special.
//
char arena[8 + BUFSIZ + BUFSIZ + 8];
char side_buf[SBSIZE];
struct symtab symbols[SYMSIZ];
struct symtab *paint_stack[SYMSIZ];

//
// Where `make install' puts each target's headers: <prefix>/share/vcc/<target>/include
// is searched after the -I directories unless -nostdinc is given.  The build
// passes the configured prefix in; a bare compile of these sources falls back to
// the default one.
//
#ifndef VCC_SHARE_DIR
#define VCC_SHARE_DIR "/usr/local/share/vcc"
#endif

//
// The targets the compiler supports, and the macros each predefines.  The
// RISC-V, AArch64, ARM32, x86-64, AVR, MSP430 and wasm32 sets are clang's, so a
// header written for clang selects the same branches here; MMIX's is GCC's, since LLVM has no MMIX;
// `besm6' is what the v7besm sources key on.  Every target but BESM-6 has the coroutines:
// __vcc_coroutines__. Ordinary macros, freely #undef'able (§6.10.8.4 covers only the standard
// ones).
//
struct target {
    const char *name;
    const char *macros[24];
};

// What GCC predefines on Linux in C11 mode, after the architecture's own macros.
#define LINUX_MACROS "__linux__", "__linux", "__gnu_linux__", "__unix__", "__unix"

static const struct target targets[] = {
    { "besm6", { "besm6", "__besm6__" } },
    { "riscv64",
      { "__riscv", "__riscv_xlen=64", "__LP64__", "_LP64", "__riscv_float_abi_double",
        "__riscv_mul", "__riscv_div", "__vcc_coroutines__" } },
    { "riscv64-linux",
      { "__riscv", "__riscv_xlen=64", "__riscv_flen=64", "__LP64__", "_LP64",
        "__riscv_float_abi_double", "__riscv_mul", "__riscv_div", "__CHAR_UNSIGNED__", "__ELF__",
        LINUX_MACROS, "__vcc_coroutines__" } },
    { "riscv32",
      { "__riscv", "__riscv_xlen=32", "__ILP32__", "_ILP32", "__riscv_float_abi_double",
        "__riscv_mul", "__riscv_div", "__vcc_coroutines__" } },
    { "aarch64",
      { "__aarch64__", "__ARM_ARCH=8", "__ARM_64BIT_STATE", "__LP64__", "_LP64",
        "__CHAR_UNSIGNED__", "__ELF__", "__vcc_coroutines__" } },
    { "arm32",
      { "__arm__", "__ARM_ARCH=7", "__ARM_ARCH_7A__", "__ARM_ARCH_PROFILE='A'", "__ARM_32BIT_STATE",
        "__ARM_EABI__", "__ARMEL__", "__ARM_PCS_VFP", "__VFP_FP__", "__ARM_FP=0xe",
        "__ARM_FEATURE_IDIV", "__ILP32__", "_ILP32", "__CHAR_UNSIGNED__", "__WCHAR_UNSIGNED__",
        "__ELF__", "__vcc_coroutines__" } },
    { "x86_64",
      { "__x86_64__", "__x86_64", "__amd64__", "__amd64", "__LP64__", "_LP64", "__SSE__",
        "__SSE2__", "__SSE_MATH__", "__SSE2_MATH__", "__code_model_small__", "__ELF__",
        "__vcc_coroutines__" } },
    { "x86_64-linux",
      { "__x86_64__", "__x86_64", "__amd64__", "__amd64", "__LP64__", "_LP64", "__SSE__",
        "__SSE2__", "__SSE_MATH__", "__SSE2_MATH__", "__code_model_small__", "__ELF__",
        LINUX_MACROS, "__vcc_coroutines__" } },
    { "aarch64-linux",
      { "__aarch64__", "__ARM_ARCH=8", "__ARM_64BIT_STATE", "__LP64__", "_LP64",
        "__CHAR_UNSIGNED__", "__ELF__", LINUX_MACROS, "__vcc_coroutines__" } },
    { "aarch64-darwin",
      { "__aarch64__", "__arm64__", "__arm64", "__ARM_ARCH=8", "__ARM_64BIT_STATE", "__LP64__",
        "_LP64", "__APPLE__", "__MACH__", "__vcc_coroutines__" } },
    { "avr",
      { "__AVR", "__AVR__", "__AVR_ARCH__=51", "__AVR_ATmega1280__", "__AVR_HAVE_MUL__",
        "__AVR_HAVE_MOVW__", "__AVR_HAVE_LPMX__", "__AVR_HAVE_ELPM__", "__AVR_HAVE_ELPMX__",
        "__AVR_HAVE_JMP_CALL__", "__AVR_2_BYTE_PC__", "__ELF__", "__vcc_coroutines__" } },
    { "msp430", { "__MSP430__", "__CHAR_UNSIGNED__", "__ELF__", "__vcc_coroutines__" } },
    { "mmix",
      { "__mmix__", "__MMIX__", "__MMIX_ABI_MMIXWARE__", "__LP64__", "_LP64",
        "__vcc_coroutines__" } },
    { "wasm32",
      { "__wasm__", "__wasm", "__wasm32__", "__wasm32", "__ILP32__", "_ILP32",
        "__wasm_bulk_memory__", "__wasm_bulk_memory_opt__", "__wasm_multivalue__",
        "__wasm_mutable_globals__", "__wasm_nontrapping_fptoint__", "__wasm_reference_types__",
        "__wasm_sign_ext__", "__vcc_coroutines__" } }, // _Coro, _Yield, _Await, __co_*
    { "wasm32-braam",
      { "__wasm__", "__wasm", "__wasm32__", "__wasm32", "__ILP32__", "_ILP32",
        "__wasm_bulk_memory__", "__wasm_bulk_memory_opt__", "__wasm_multivalue__",
        "__wasm_mutable_globals__", "__wasm_nontrapping_fptoint__", "__wasm_reference_types__",
        "__wasm_sign_ext__", "__vcc_coroutines__", "__braam__" } }, // Braam's process model
};

static const struct target *target; // selected by -t; default riscv64, like lower
static int opt_nostdinc;            // -nostdinc: no target include directory
static char std_include[256];       // VCC_SHARE_DIR/<target>/include

//
// Select the target by name, or fail listing the valid ones.
//
static void select_target(const char *name)
{
    unsigned i;

    for (i = 0; i < sizeof(targets) / sizeof(targets[0]); i++) {
        if (strcmp(targets[i].name, name) == 0) {
            target = &targets[i];
            return;
        }
    }
    pperror("unknown target '%s'", name);
    fprintf(stderr, "Valid targets:");
    for (i = 0; i < sizeof(targets) / sizeof(targets[0]); i++)
        fprintf(stderr, " %s", targets[i].name);
    fprintf(stderr, "\n");
    exit(1);
}

//
// Print the command-line help and the meaning of each option.
//
void usage()
{
    printf("Usage:\n");
    printf("    %s [options] [infile [outfile]]\n", cpp.prog_name ? cpp.prog_name : "cpp");
    printf("Options:\n");
    printf(
        "    -t target           Target: besm6, riscv64, riscv32, aarch64, arm32, x86_64, avr, "
        "msp430, mmix, wasm32 or wasm32-braam (default riscv64)\n");
    printf("    -I path             Add path to the search list for header files\n");
    printf("    -nostdinc           Do not search the target's standard include directory\n");
    printf("    -D macro[=value]    Fake a definition at the beginning\n");
    printf("    -U macro            Undefine a macro at the beginning\n");
    printf("    -R                  Allow macro recursion\n");
    printf("    -P                  Inhibit generation of line markers\n");
    printf("    -C                  Do not discard comments\n");
    printf("    -w                  Suppress warnings\n");
    printf("    -trigraphs          Translate trigraphs\n");
    printf("    -E                  Ignored for compatibility\n");
    printf("    --color=WHEN        Color diagnostics: always, never or auto (default)\n");
}

//
// The three startup phases, each a function of its own rather than a stretch of
// main().  That is not cosmetic here: main() stays on the stack for the whole
// run, under every macro expansion, and on the BESM-6 the frame it would need
// for all three at once is 500 words of the 4,096 there are -- see README.md,
// "Building for the BESM-6".  Split, each phase's frame is gone before the next
// begins, and main()'s is a handful of words.
//

//
// Build the scan tables: mark which bytes are identifier chars, digits, quotes,
// comment starters, whitespace, etc.  The scanner then classifies a character
// with a single table lookup.
//
static void build_scan_tables(void)
{
    int i, c;
    const char *p;

    p = "_ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    while ((c = *p++)) {
        cpp.fast_tab[(unsigned char)c] |= IB | NB | SB;
        cpp.char_class[(unsigned char)c] = IDENT;
    }
    p = "0123456789.";
    while ((c = *p++)) {
        cpp.fast_tab[(unsigned char)c] |= NB | SB;
        cpp.char_class[(unsigned char)c] = NUMBR;
    }
    // High bytes (0x80-0xFF) are treated as identifier characters so UTF-8 names
    // like "длина" work.  NOT NB: they are never digits.  The five marker bytes
    // 0xFA-0xFE live in this range but valid UTF-8 never produces 0xF5-0xFF, so we
    // hand the whole range IB here and clear it back off the markers below.
    for (i = 0x80; i <= 0xFF; i++) {
        cpp.fast_tab[i] |= IB | SB;
        cpp.char_class[i] = IDENT;
    }
    p = "\n\"'/\\";
    while ((c = *p++))
        cpp.fast_tab[(unsigned char)c] |= SB;
    p = "\n\"'\\";
    while ((c = *p++))
        cpp.fast_tab[(unsigned char)c] |= QB;
    p = "*\n";
    while ((c = *p++))
        cpp.fast_tab[(unsigned char)c] |= CB;
    // Marker bytes sit in the high range just marked as identifier chars, so
    // assign (=, not |=) to clear the IB bit off them: a marker must never be
    // treated as an identifier character.
    cpp.fast_tab[WARN_MARK]        = WB;
    cpp.fast_tab[STRINGIZE_MARK]   = WB;
    cpp.fast_tab[PASTE_MARK]       = WB;
    cpp.fast_tab[COMMA_PASTE_MARK] = WB;
    // The blue-paint region-end marker only needs to stop the fast scanner (SB);
    // it must NOT carry WB/IB so the body-push loop and identifier scan ignore it.
    cpp.fast_tab[PAINT_END_MARK] = SB;
    cpp.char_class[WARN_MARK] = cpp.char_class[STRINGIZE_MARK] = cpp.char_class[PASTE_MARK] =
        cpp.char_class[COMMA_PASTE_MARK] = cpp.char_class[PAINT_END_MARK] = 0;
    cpp.fast_tab['\0'] |= CB | QB | SB | WB;
    for (i = ALFSIZ; --i >= 0;)
        cpp.slow_tab[i] = cpp.fast_tab[i] | SB;
    p = " \t\013\f\r"; // note no \n;	\v not legal for vertical tab?
    while ((c = *p++))
        cpp.char_class[(unsigned char)c] = BLANK;
}

//
// Parse the command line: options begin with '-', otherwise the argument is the
// input file (first) or output file (second).  A bare "-" holds either place for
// the standard stream.
//
static void parse_args(int argc, char *argv[])
{
    int i, nfiles = 0;

    cpp.inc_file[cpp.inc_level = 0] = "";
    for (i = 1; i < argc; i++) {
        switch (argv[i][0]) {
        case '-':
            switch (argv[i][1]) {
            case 'P':
                cpp.opt_no_lines++;
            case 'E':
                continue;
            case 'R':
                ++cpp.opt_recurse;
                continue;
            case 'C':
                cpp.opt_keep_comments++;
                continue;
            case 'w':
                cpp.opt_no_warnings++;
                continue;
            case 't':
                if (strcmp(argv[i], "-trigraphs") == 0) {
                    cpp.opt_trigraphs++;
                    continue;
                }
                // -tNAME or -t NAME
                if (argv[i][2])
                    select_target(argv[i] + 2);
                else if (i + 1 < argc)
                    select_target(argv[++i]);
                else {
                    pperror("missing argument to '-t'");
                    exit(1);
                }
                continue;
            case 'n':
                if (strcmp(argv[i], "-nostdinc") == 0) {
                    opt_nostdinc++;
                    continue;
                }
                pperror("unknown option '%s'", argv[i]);
                exit(1);
            case '-':
                if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
                    select_target(argv[++i]);
                    continue;
                }
                if (strncmp(argv[i], "--target=", 9) == 0) {
                    select_target(argv[i] + 9);
                    continue;
                }
                if (strncmp(argv[i], "--color=", 8) == 0) {
                    const char *when = argv[i] + 8;
                    if (strcmp(when, "always") == 0)
                        cpp.opt_color = 1;
                    else if (strcmp(when, "never") == 0)
                        cpp.opt_color = -1;
                    else if (strcmp(when, "auto") == 0)
                        cpp.opt_color = 0;
                    else {
                        pperror("invalid argument '%s' to '--color'", when);
                        exit(1);
                    }
                    continue;
                }
                pperror("unknown option '%s'", argv[i]);
                exit(1);
            case 'D':
                if (cpp.pre_defs_end >= cpp.pre_defs + NPREDEF) {
                    pperror("too many '-D' options; '%s' is ignored", argv[i]);
                    continue;
                }
                // ignore plain "-D" (no argument)
                if (*(argv[i] + 2))
                    *cpp.pre_defs_end++ = argv[i] + 2;
                continue;
            case 'U':
                if (cpp.pre_undefs_end >= cpp.pre_undefs + NPREDEF) {
                    pperror("too many '-U' options; '%s' is ignored", argv[i]);
                    continue;
                }
                *cpp.pre_undefs_end++ = argv[i] + 2;
                continue;
            case 'I':
                if (cpp.ndirs > MAXDIRS)
                    pperror("too many '-I' directories; '%s' is ignored", argv[i]);
                else
                    cpp.search_dirs[cpp.ndirs++] = argv[i] + 2;
                continue;
            case '\0':
                if (nfiles++ >= 2)
                    pperror("unexpected argument '%s'", argv[i]);
                continue;
            default:
                pperror("unknown option '%s'", argv[i]);
                exit(1);
            }
        default:
            if (nfiles++ == 0) {
                cpp.in_fd = open(argv[i], READ);
                if (cpp.in_fd < 0) {
                    pperror("cannot open '%s': %s", argv[i], strerror(errno));
                    exit(1);
                }
                cpp.inc_file[cpp.inc_level] = save_string(argv[i]);
                cpp.search_dirs[0] = cpp.inc_dir[cpp.inc_level] = dir_of(argv[i]);

                // too dangerous to have file name in same syntactic position
                // be input or output file depending on file redirections,
                // so force output to stdout, willy-nilly
                //      [i don't see what the problem is.  jfr]
                //
            } else if (nfiles == 2) {
                static char sobuf[BUFSIZ];
                cpp.out_file = fopen(argv[i], "w");
                if (!cpp.out_file) {
                    pperror("cannot create '%s': %s", argv[i], strerror(errno));
                    exit(1);
                }
                fclose(stdout);
                setbuffer(cpp.out_file, sobuf, sizeof(sobuf));
            } else
                pperror("unexpected argument '%s'", argv[i]);
        }
    }
    if (isatty(cpp.in_fd)) {
        usage();
        exit(1);
    }
}

//
// Finish the include search path, then register the built-in directives and the
// predefined macros -- including the -D and -U options staged by parse_args().
//
static void register_builtins(void)
{
    int i;
    char *tf, **cp2;

    // after user -I files here is the target's standard include directory
    if (!opt_nostdinc) {
        snprintf(std_include, sizeof(std_include), "%s/%s/include", VCC_SHARE_DIR, target->name);
        cpp.search_dirs[cpp.ndirs++] = std_include;
    }
    cpp.search_dirs[cpp.ndirs++] = 0;
    cpp.sym_define               = install_directive("define");
    cpp.sym_undef                = install_directive("undef");
    cpp.sym_include              = install_directive("include");
    cpp.sym_elif                 = install_directive("elif");
    cpp.sym_else                 = install_directive("else");
    cpp.sym_endif                = install_directive("endif");
    cpp.sym_ifdef                = install_directive("ifdef");
    cpp.sym_ifndef               = install_directive("ifndef");
    cpp.sym_if                   = install_directive("if");
    cpp.sym_line                 = install_directive("line");
    cpp.sym_error                = install_directive("error");
    cpp.sym_pragma               = install_directive("pragma");
    for (i = sizeof(cpp.macro_bits) / sizeof(cpp.macro_bits[0]); --i >= 0;)
        cpp.macro_bits[i] = 0;
    // The target's macros (-t).  Ordinary and #undef'able (§6.10.8.4 applies to
    // none of them); the BESM-6 sources of v7besm, this one included, tune
    // themselves on `besm6'.
    for (i = 0; i < sizeof(target->macros) / sizeof(target->macros[0]) && target->macros[i]; i++)
        define_symbol(target->macros[i]);
    cpp.sym_line_macro             = define_symbol("__LINE__");
    cpp.sym_file_macro             = define_symbol("__FILE__");
    cpp.sym_pragma_op              = define_symbol("_Pragma");
    cpp.sym_line_macro->predefined = 1; // §6.10.8.4: no #define/#undef
    cpp.sym_file_macro->predefined = 1;

    // C11 §6.10.8 standard predefined macros with fixed bodies.  Unlike
    // __LINE__/__FILE__ these need no per-expansion synthesis, so they are just
    // registered like ordinary macros.  Each is flagged predefined so §6.10.8.4
    // rejects any #define or #undef of it (including from a -D/-U option).
    define_symbol("__STDC__=1");
    cpp.last_sym->predefined = 1;
    define_symbol("__STDC_VERSION__=201112L");
    cpp.last_sym->predefined = 1;
    define_symbol("__STDC_HOSTED__=1");
    cpp.last_sym->predefined = 1;

    // C11 §6.10.8.3 conditional feature macros.  __STDC_HOSTED__ is 1 above, so
    // §4p6 would otherwise oblige this implementation to ship <complex.h>,
    // <stdatomic.h> and <threads.h>.  It does not and will not: the BESM-6 has
    // one native float format and no complex type, no atomic instructions, and
    // no threads under this kernel.  Announcing that here is what lets a
    // portable source #ifdef its way past all three instead of failing to find
    // a header.  __STDC_NO_VLA__ likewise: the front end has no variable-length
    // arrays.
    define_symbol("__STDC_NO_COMPLEX__=1");
    cpp.last_sym->predefined = 1;
    define_symbol("__STDC_NO_ATOMICS__=1");
    cpp.last_sym->predefined = 1;
    define_symbol("__STDC_NO_THREADS__=1");
    cpp.last_sym->predefined = 1;
    define_symbol("__STDC_NO_VLA__=1");
    cpp.last_sym->predefined = 1;
    {
        time_t now          = time((time_t *)0);
        const struct tm *tm = localtime(&now);
        char dtbuf[64];
        // "Mmm dd yyyy": %e is space-padded so days < 10 keep the 11-char shape.
        strftime(dtbuf, sizeof(dtbuf), "__DATE__=\"%b %e %Y\"", tm);
        define_symbol(dtbuf);
        cpp.last_sym->predefined = 1;
        strftime(dtbuf, sizeof(dtbuf), "__TIME__=\"%H:%M:%S\"", tm);
        define_symbol(dtbuf);
        cpp.last_sym->predefined = 1;
    }

    tf                          = cpp.inc_file[cpp.inc_level];
    cpp.inc_file[cpp.inc_level] = "command line";
    cpp.line_no[cpp.inc_level]  = 1;
    cp2                         = cpp.pre_defs;
    while (cp2 < cpp.pre_defs_end)
        define_symbol(*cp2++);
    cp2 = cpp.pre_undefs;
    while (cp2 < cpp.pre_undefs_end) {
        char *p;
        if ((p = find_char(*cp2, '=')))
            *p++ = '\0';
        lookup(*cp2++, DROP);
    }
    cpp.inc_file[cpp.inc_level] = tf;
}

//
// Program entry point.  Startup runs in a fixed order: seed the state fields
// that need a non-zero initial value, build the scan tables, parse the command
// line, register the built-ins, then set the buffer pointers and hand control to
// process_directives, which runs until end of input.  The process exit status is
// the number of errors reported.
//
int main(int argc, char *argv[])
{
    // Fields that used to have static initializers referencing other fields;
    // set them up before anything runs (the instance is otherwise zeroed).
    cpp.side_ptr       = side_buf;
    cpp.pre_defs_end   = cpp.pre_defs;
    cpp.pre_undefs_end = cpp.pre_undefs;
    cpp.in_fd          = STDIN;
    cpp.ndirs          = 1;

    // Diagnostic prefix: the basename of argv[0] (fallback "cpp").
    cpp.prog_name = "cpp";
    if (argc > 0 && argv[0] && argv[0][0]) {
        char *slash   = strrchr(argv[0], '/');
        cpp.prog_name = slash ? slash + 1 : argv[0];
    }

    cpp.out_file = stdout;
    target       = &targets[1]; // riscv64

    build_scan_tables();

    parse_args(argc, argv);

    // No reset of cpp.exit_code here: an error parse_args() reported (an
    // extraneous file name, a full -D table) must count toward the exit status.
    cpp.inc_fd[cpp.inc_level] = cpp.in_fd;

    register_builtins();

    cpp.buf_start = arena + 8;
    cpp.buf_mid   = cpp.buf_start + BUFSIZ;
    cpp.buf_end   = cpp.buf_mid + BUFSIZ;

    // Point the buffer cursors at the (empty) buffer and run the main loop.
    // The first refill inside process_directives reads the actual input.
    cpp.true_level  = 0;
    cpp.false_level = 0;
    cpp.line_no[0]  = 1;
    emit_line_marker();
    cpp.out_ptr = cpp.tok_ptr = cpp.buf_end;
    process_directives(cpp.buf_end);
    exit_cpp();
}
