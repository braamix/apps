//
// vcc: the C compiler driver.
//
// Turns C source into objects and executables for one target by driving the
// toolchain, one sub-tool per stage:
//
//     vcpp       preprocess      .c   -> .i
//     vparse     parse           .i   -> .ast
//     vlower     lower + opt     .ast -> .tac
//     vgen<T>    code gen        .tac -> .s
//     as         assemble        .s   -> .o     (b6as | clang | msp430-elf-as | mmix-...-as)
//     ld         link            .o   -> a.out  (b6ld | ld.lld | wasm-ld | msp430-elf-ld |
//                                               mmix-...-ld)
//
// The target is chosen with -t: x86_64-linux, aarch64-linux and riscv64-linux are hosted,
// linked by the system C compiler against glibc, and aarch64-darwin against macOS's libSystem; riscv64,
// riscv32, aarch64, arm32, x86_64, avr, msp430, mmix and besm6 are bare metal, wasm32
// a WebAssembly module, run under node, and wasm32-braam a process of Braam.  By default it is the
// host, where that is one of the hosted targets, else riscv64. Input files are dispatched by
// suffix: .c runs the full pipeline, .S is preprocessed assembly (cpp -> as), .s is assembled
// directly, and .o is passed straight to the linker, as is a .a archive.
//
// Selection of the last stage to run is controlled by -E (stop after cpp),
// -S (stop after codegen, emit assembly) and -c (stop after as, emit object).
// With none of those, the objects are linked into an executable; the objects
// compiled from sources are then temporaries, as with gcc.  On the BESM-6,
// -Sbemsh and -Smadlen behave like -S but also select the assembly dialect.
//
// THE INSTALLATION IS RELOCATABLE.  Our own passes are looked up in the directory
// vcc itself runs from, and each target's headers and libraries under
// <that>/../share/vcc/<target>, so a copied tree keeps working.  The assembler and
// linker belong to other projects and are found on PATH.  Every sub-tool can be
// overridden with an environment variable (VCC_CPP, VCC_AS, ...); that is how the
// tests run the driver against the build tree.  VCC_AS and VCC_LD may carry
// arguments of their own, e.g. "clang --target=msp430 -c".
//
// Ported from the v7besm project's b6cc (cmd/cc), itself a modern rewrite of the
// Unix v7 cc(1) driver.
//
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifndef RISCV64_AS
#define RISCV64_AS ""
#endif
#ifndef RISCV64_LD
#define RISCV64_LD ""
#endif
#ifndef RISCV32_AS
#define RISCV32_AS ""
#endif
#ifndef RISCV32_LD
#define RISCV32_LD ""
#endif
#ifndef AARCH64_AS
#define AARCH64_AS ""
#endif
#ifndef AARCH64_LD
#define AARCH64_LD ""
#endif
#ifndef ARM32_AS
#define ARM32_AS ""
#endif
#ifndef ARM32_LD
#define ARM32_LD ""
#endif
#ifndef X86_64_AS
#define X86_64_AS ""
#endif
#ifndef X86_64_LD
#define X86_64_LD ""
#endif
#ifndef AVR_AS
#define AVR_AS ""
#endif
#ifndef AVR_LD
#define AVR_LD ""
#endif
#ifndef MSP430_AS
#define MSP430_AS ""
#endif
#ifndef MSP430_LD
#define MSP430_LD ""
#endif
#ifndef MMIX_AS
#define MMIX_AS ""
#endif
#ifndef MMIX_LD
#define MMIX_LD ""
#endif
#ifndef RISCV64_LDFLAGS
#define RISCV64_LDFLAGS ""
#endif
#ifndef RISCV32_LDFLAGS
#define RISCV32_LDFLAGS ""
#endif
#ifndef AARCH64_LDFLAGS
#define AARCH64_LDFLAGS ""
#endif
#ifndef ARM32_LDFLAGS
#define ARM32_LDFLAGS ""
#endif
#ifndef X86_64_LDFLAGS
#define X86_64_LDFLAGS ""
#endif
#ifndef AVR_LDFLAGS
#define AVR_LDFLAGS ""
#endif
#ifndef MSP430_LDFLAGS
#define MSP430_LDFLAGS ""
#endif
#ifndef MMIX_LDFLAGS
#define MMIX_LDFLAGS ""
#endif
#ifndef MSP430_LIBGCC
#define MSP430_LIBGCC ""
#endif
#ifndef MMIX_LIBGCC
#define MMIX_LIBGCC ""
#endif
#ifndef WASM32_AS
#define WASM32_AS ""
#endif
#ifndef WASM32_LD
#define WASM32_LD ""
#endif
// Braam's wasm features, as libc/wasm32 assembles with them (WASM32_FEATURES there).
#ifndef WASM32_FEATURES
#define WASM32_FEATURES \
    "-mreference-types -mbulk-memory -msign-ext -mmutable-globals -mnontrapping-fptoint"
#endif
#ifndef X86_64_LINUX_CC
#define X86_64_LINUX_CC ""
#endif
#ifndef AARCH64_LINUX_CC
#define AARCH64_LINUX_CC ""
#endif
#ifndef RISCV64_LINUX_CC
#define RISCV64_LINUX_CC ""
#endif
#ifndef AARCH64_DARWIN_CC
#define AARCH64_DARWIN_CC ""
#endif

// The hosted target of the machine vcc runs on, if there is one: the default target,
// and the one whose C compiler is plain `cc`.
#if defined(__linux__) && defined(__x86_64__)
#define HOST_TARGET "x86_64-linux"
#elif defined(__linux__) && defined(__aarch64__)
#define HOST_TARGET "aarch64-linux"
#elif defined(__linux__) && defined(__riscv) && __riscv_xlen == 64
#define HOST_TARGET "riscv64-linux"
#elif defined(__APPLE__) && defined(__aarch64__)
#define HOST_TARGET "aarch64-darwin"
#else
#define HOST_TARGET ""
#endif

static char *progname = "vcc"; // diagnostic prefix: basename of argv[0]

//
// A target: its code generator, and how to assemble and link for it.  The
// assembler and linker are given as an environment override, the path found when
// vcc was configured (may be empty), and the names to look up on PATH: GNU
// binutils by each prefix in turn, then clang and ld.lld where the target has a
// clang triple.  Which of the two a tool is decides its flags (is_llvm).  The
// targets other than the BESM-6, the MSP430 and MMIX are linked with a linker
// script for qemu `virt` (`microvm` for x86-64, `arduino-mega` for AVR).  The
// MSP430 is linked for mspsim, and its link drops the sections nothing reaches
// (vgenmsp430 gives every function and variable one) and ends with GCC's libgcc.a
// when it was found, so that objects compiled by GCC link too.  MMIX is the same
// with the GNU MMIX binutils, for Knuth's mmix: as GCC runs them, the assembler
// with -x (it expands an out-of-range branch and allocates the base registers) and
// the linker with no script, text from 0x100, its output Knuth's .mmo.  wasm32 has no
// binutils: clang assembles and wasm-ld links, with no script, a module for node.
//
// A hosted target (ARCH_HOSTED) both assembles and links with a C compiler: the one
// found when vcc was configured, else <prefix>-gcc, else `cc` on the host itself, else
// clang.  It supplies the startup files and the C library; we add libvcc.a, but on
// macOS, which needs no runtime of ours, and links position independent.
//
enum arch { ARCH_BESM6, ARCH_CROSS, ARCH_HOSTED };

struct target {
    const char *name;
    enum arch arch;
    const char *codegen;          // our code generator, next to vcc
    const char *as_default;       // configure-time assembler path, or ""
    const char *ld_default;       // configure-time linker path, or ""
    const char *ld_default_flags; // its flags, when it is GNU ld: those of ld_flags, and
                                  // --no-warn-rwx-segments where it has the option
    const char *prefixes;         // GNU binutils prefixes, blank-separated (BESM-6: b6as, b6ld)
    const char *as_flags;         // GNU as flags, blank-separated, or NULL
    const char *ld_flags;         // GNU ld flags, blank-separated, or NULL
    const char *triple;           // clang --target, or NULL: no clang
    const char *clang_flags;      // more clang flags, blank-separated, or NULL
    const char *ld_flag;          // linker flag for either linker, or NULL
    const char *libgcc;           // configure-time libgcc.a, linked last when present, or NULL
    bool no_script;               // the linker's default script, unless -T names one
    const char *gen_flag;         // code generator flag, or NULL
    bool pie;                     // hosted: a position-independent executable, no -no-pie
    const char *llvm_ld;          // the LLVM linker, when not ld.lld
    const char *llvm_ld_flags;    // its flags, blank-separated, or NULL
    bool braam;                   // a Braam process: --initial-memory, then the braam section
};

#define RISCV_PREFIXES "riscv64-unknown-elf riscv64-elf riscv64-linux-gnu"

static const struct target targets[] = {
    { "besm6", ARCH_BESM6, "vgenbesm6", "", "", "" },
    { "riscv64", ARCH_CROSS, "vgenriscv64", RISCV64_AS, RISCV64_LD, RISCV64_LDFLAGS, RISCV_PREFIXES,
      "-march=rv64imfd -mabi=lp64d", NULL, "riscv64", "-march=rv64imfd -mabi=lp64d" },
    { "riscv32", ARCH_CROSS, "vgenriscv32", RISCV32_AS, RISCV32_LD, RISCV32_LDFLAGS, RISCV_PREFIXES,
      "-march=rv32imfd -mabi=ilp32d", "-m elf32lriscv", "riscv32", "-march=rv32imfd -mabi=ilp32d" },
    { "aarch64", ARCH_CROSS, "vgenaarch64", AARCH64_AS, AARCH64_LD, AARCH64_LDFLAGS,
      "aarch64-none-elf aarch64-elf aarch64-linux-gnu", NULL, NULL, "aarch64-none-elf", NULL },
    { "arm32", ARCH_CROSS, "vgenarm32", ARM32_AS, ARM32_LD, ARM32_LDFLAGS, "arm-none-eabi",
      "-mcpu=cortex-a15 -mfpu=vfpv3-d16 -mfloat-abi=hard", NULL, "armv7a-none-eabihf",
      "-mcpu=cortex-a15 -mfpu=vfpv3-d16" },
    { "x86_64", ARCH_CROSS, "vgenx86", X86_64_AS, X86_64_LD, X86_64_LDFLAGS,
      "x86_64-elf x86_64-linux-gnu", "--64", NULL, "x86_64-none-elf", NULL },
    { "avr", ARCH_CROSS, "vgenavr", AVR_AS, AVR_LD, AVR_LDFLAGS, "avr", "-mmcu=atmega1280",
      "-m avr51", "avr", "-mmcu=atmega1280" },
    { "msp430", ARCH_CROSS, "vgenmsp430", MSP430_AS, MSP430_LD, MSP430_LDFLAGS,
      "msp430-elf msp430-unknown-elf", "-mcpu=msp430", NULL, NULL, NULL, "--gc-sections",
      MSP430_LIBGCC },
    { "mmix", ARCH_CROSS, "vgenmmix", MMIX_AS, MMIX_LD, MMIX_LDFLAGS, "mmix-knuth-mmixware",
      "-x -no-predefined-syms", NULL, NULL, NULL, "--defsym=__.MMIX.start..text=0x100", MMIX_LIBGCC,
      true },
    { .name             = "wasm32",
      .arch             = ARCH_CROSS,
      .codegen          = "vgenwasm",
      .as_default       = WASM32_AS,
      .ld_default       = WASM32_LD,
      .ld_default_flags = "",
      .prefixes         = "",
      .triple           = "wasm32",
      .clang_flags      = "--no-default-config " WASM32_FEATURES,
      .no_script        = true,
      .llvm_ld          = "wasm-ld",
      .llvm_ld_flags    = "--stack-first -z stack-size=1048576" },
    // Braam's process ABI (docs/Braam.md §7.1): the memory imported, no entry (the
    // exports are crt0.o's), the stack Braam's own programs have.
    { .name             = "wasm32-braam",
      .arch             = ARCH_CROSS,
      .codegen          = "vgenwasm",
      .as_default       = WASM32_AS,
      .ld_default       = WASM32_LD,
      .ld_default_flags = "",
      .prefixes         = "",
      .triple           = "wasm32",
      .clang_flags      = "--no-default-config " WASM32_FEATURES,
      .no_script        = true,
      .llvm_ld          = "wasm-ld",
      .llvm_ld_flags =
          "--no-entry --import-memory --stack-first -z stack-size=131072 --gc-sections",
      .braam = true },
    { .name       = "x86_64-linux",
      .arch       = ARCH_HOSTED,
      .codegen    = "vgenx86",
      .as_default = X86_64_LINUX_CC,
      .ld_default = X86_64_LINUX_CC,
      .prefixes   = "x86_64-linux-gnu",
      .triple     = "x86_64-linux-gnu",
      .gen_flag   = "--linux" },
    { .name       = "aarch64-linux",
      .arch       = ARCH_HOSTED,
      .codegen    = "vgenaarch64",
      .as_default = AARCH64_LINUX_CC,
      .ld_default = AARCH64_LINUX_CC,
      .prefixes   = "aarch64-linux-gnu",
      .triple     = "aarch64-linux-gnu",
      .gen_flag   = "--linux" },
    { .name       = "riscv64-linux",
      .arch       = ARCH_HOSTED,
      .codegen    = "vgenriscv64",
      .as_default = RISCV64_LINUX_CC,
      .ld_default = RISCV64_LINUX_CC,
      .prefixes   = "riscv64-linux-gnu",
      .triple     = "riscv64-linux-gnu",
      .gen_flag   = "--linux" },
    { .name       = "aarch64-darwin",
      .arch       = ARCH_HOSTED,
      .codegen    = "vgenaarch64",
      .as_default = AARCH64_DARWIN_CC,
      .ld_default = AARCH64_DARWIN_CC,
      .prefixes   = "aarch64-apple-darwin",
      .triple     = "arm64-apple-macos",
      .gen_flag   = "--darwin",
      .pie        = true },
};

static const struct target *target; // set by -t, else the default

static void diag_prefix(const char *kind); // "vcc: kind: ", below

//
// A growable vector of C strings, used for argument lists and file lists.
// The stored pointers are borrowed unless noted; the vector owns only its
// backing array, freed with vec_free().
//
struct vec {
    char **data;
    size_t len;
    size_t cap;
};

static void vec_push(struct vec *v, char *s)
{
    if (v->len == v->cap) {
        v->cap  = v->cap ? v->cap * 2 : 8;
        v->data = realloc(v->data, v->cap * sizeof(*v->data));
        if (!v->data) {
            diag_prefix("error");
            fprintf(stderr, "out of memory\n");
            exit(1);
        }
    }
    v->data[v->len++] = s;
}

static void vec_free(struct vec *v)
{
    free(v->data);
    v->data = NULL;
    v->len = v->cap = 0;
}

//
// Parsed command-line state.
//
static bool opt_c; // -c: compile/assemble only, no link
static bool opt_S; // -S: compile to assembly only
static bool opt_E; // -E: preprocess only
static bool opt_g; // -g: request debug info (a no-op; README.md, "Reserved options")
static bool opt_O; // -O: request optimization (a no-op; README.md, "Reserved options")
static char
    opt_x; // -x LANG: the input language for every file, as its suffix ('c', 'S', 's'); 0 by suffix
static bool opt_v;        // -v: echo each sub-command before running it
static bool opt_nostdlib; // -nostdlib: skip the library dir, crt0.o and the implicit -l's
static unsigned long braam_pages = 4; // --initial-pages: a Braam process's initial memory, as
                                      // BRAAM_BIN_INITIAL_PAGES in braam-core's BraamProgram.cmake
static bool opt_nostdinc;             // -nostdinc: skip the target's standard include dir
static char *outfile;                 // -o NAME: explicit output name
static char *linkscript;      // -T FILE: linker script (not besm6), instead of the standard one
static char *max_errors_flag; // --max-errors=N for parse and lower, from -fmax-errors=N
static int no_show_source;    // -fno-diagnostics-show-caret: no source line in diagnostics
static int color = -1;        // -fdiagnostics-color: 1 always, 0 never, -1 when stderr is a tty
static char *color_flag;      // --color=always|never for every pass, or NULL for auto
static char *codegen_dialect; // -Sbemsh/-Smadlen: dialect flag for the BESM-6 codegen, or NULL

static struct vec sources;  // input .c/.s files to compile
static struct vec objects;  // .o (and produced) files to link
static struct vec cppflags; // -D/-I/-U pass-throughs for the preprocessor
static struct vec ldflags;  // -L/-l pass-throughs for the linker
static struct vec tmpfiles; // temp files to unlink on exit
static struct vec owned;    // heap-allocated file names to free on exit

static char *exe_dir;   // directory vcc runs from (owned), or NULL if unknown
static char *share_dir; // <exe_dir>/../share/vcc/<target> (owned)

static int errflag; // set nonzero on any failure; becomes the exit status

//
// Take ownership of a heap-allocated string so it is freed at exit.  Every
// generated file name flows through here; the sources/objects/tmpfiles vectors
// only ever borrow these pointers.  Returns its argument for convenient
// chaining, e.g. own(replace_suffix(src, "o")).
//
static char *own(char *s)
{
    if (s)
        vec_push(&owned, s);
    return s;
}

//
// Return a newly allocated (and owned) concatenation of two strings.
//
static char *concat(const char *a, const char *b)
{
    size_t n = strlen(a) + strlen(b) + 1;
    char *s  = malloc(n);
    if (!s) {
        diag_prefix("error");
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    snprintf(s, n, "%s%s", a, b);
    return own(s);
}

//
// Print "vcc: kind: " to stderr, colored as the passes color their diagnostics
// (libutil/srcloc.c): when -fdiagnostics-color says so, or by default when stderr
// is a terminal, NO_COLOR is unset or empty and TERM is not "dumb".
//
static void diag_prefix(const char *kind)
{
    if (color < 0) {
        const char *no_color = getenv("NO_COLOR");
        const char *term     = getenv("TERM");
        color = isatty(2) && !(no_color && no_color[0]) && !(term && strcmp(term, "dumb") == 0);
    }
    if (color)
        fprintf(stderr, "\033[1m%s:\033[0m %s%s:\033[0m ", progname,
                strcmp(kind, "note") == 0 ? "\033[1;36m" : "\033[1;31m", kind);
    else
        fprintf(stderr, "%s: %s: ", progname, kind);
}

//
// Report an error, printf-style, prefixed with "vcc: ".  Records the failure
// but does not exit; the driver decides whether to keep going.
//
static void error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    diag_prefix("error");
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    errflag = 1;
}

//
// Remove every temp file we created.  Registered with atexit(), so it also
// runs on error paths that call exit().
//
static void cleanup(void)
{
    for (size_t i = 0; i < tmpfiles.len; i++)
        unlink(tmpfiles.data[i]);
    for (size_t i = 0; i < owned.len; i++)
        free(owned.data[i]);
    vec_free(&tmpfiles);
    vec_free(&owned);
}

//
// Return the file-name suffix character after the final '.', or 0 if the name
// has no extension.  E.g. suffix_of("foo/bar.c") == 'c'.
//
static char suffix_of(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot || strchr(dot, '/'))
        return 0;
    return dot[1] ? dot[1] : 0;
}

//
// True if `name` ends in ".a": an archive, which goes to the linker as is.
//
static bool is_archive(const char *name)
{
    size_t n = strlen(name);
    return n > 2 && strcmp(name + n - 2, ".a") == 0;
}

//
// Return a newly allocated copy of the base name of `name` (path stripped)
// with its suffix replaced by `.suf`.  Used to derive default output names,
// e.g. replace_suffix("src/foo.c", "o") == "foo.o".
//
static char *replace_suffix(const char *name, const char *suf)
{
    const char *slash = strrchr(name, '/');
    const char *base  = slash ? slash + 1 : name;
    const char *dot   = strrchr(base, '.');
    size_t stem       = dot ? (size_t)(dot - base) : strlen(base);

    char *out = malloc(stem + strlen(suf) + 2);
    if (!out) {
        error("out of memory");
        exit(1);
    }
    memcpy(out, base, stem);
    out[stem] = '.';
    strcpy(out + stem + 1, suf);
    return out;
}

//
// True if `a` and `b` name the same existing file (same device + inode).  Used
// to avoid clobbering a .S source on a case-insensitive filesystem, where the
// derived .s output resolves to the same file.  Returns false if either path
// cannot be stat()ed -- there is nothing to clobber yet.
//
static bool same_file(const char *a, const char *b)
{
    struct stat sa, sb;
    if (stat(a, &sa) != 0 || stat(b, &sb) != 0)
        return false;
    return sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

//
// Create a temporary file whose name ends in `.suf`, register it for cleanup,
// and return its (heap-allocated) path.  Uses mkstemps(3) rather than mkstemp(3)
// because the suffix is the point: every stage of the pipeline is a file whose
// extension says what is in it (and the assembler picks its input language by
// it).  The returned fd is closed immediately since the sub-tool reopens the
// path by name.
//
static char *make_temp(const char *suf)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !*dir)
        dir = "/tmp";

    size_t n   = strlen(dir) + strlen(suf) + sizeof("/vccXXXXXX.");
    char *path = malloc(n);
    if (!path) {
        error("out of memory");
        exit(1);
    }
    snprintf(path, n, "%s/vccXXXXXX.%s", dir, suf);

    int fd = mkstemps(path, (int)strlen(suf) + 1);
    if (fd < 0) {
        error("cannot create temporary file: %s", strerror(errno));
        exit(1);
    }
    close(fd);
    vec_push(&tmpfiles, path);
    return own(path);
}

//
// Find the running executable's own path: the OS knows it on macOS and Linux;
// elsewhere fall back to argv[0], searched on PATH when it has no '/'.  Returns
// a pointer into `buf` (PATH_MAX bytes) or to argv0, or NULL.
//
static const char *exe_path(const char *argv0, char *buf)
{
#if defined(__APPLE__)
    uint32_t size = PATH_MAX;
    if (_NSGetExecutablePath(buf, &size) == 0)
        return buf;
#elif defined(__linux__)
    ssize_t n = readlink("/proc/self/exe", buf, PATH_MAX - 1);
    if (n > 0) {
        buf[n] = '\0';
        return buf;
    }
#endif
    if (strchr(argv0, '/'))
        return argv0;

    const char *path = getenv("PATH");
    while (path && *path) {
        const char *colon = strchr(path, ':');
        size_t len        = colon ? (size_t)(colon - path) : strlen(path);
        snprintf(buf, PATH_MAX, "%.*s/%s", (int)len, len ? path : ".", argv0);
        if (access(buf, X_OK) == 0)
            return buf;
        path = colon ? colon + 1 : NULL;
    }
    return NULL;
}

//
// Set exe_dir to the directory holding the vcc executable, with symlinks
// resolved: a ~/bin/vcc linked to an installation still finds its passes.
// Left NULL when it cannot be determined; find_pass() reports that.
//
static void locate_self(const char *argv0)
{
    char buf[PATH_MAX], real[PATH_MAX];
    const char *path = exe_path(argv0, buf);

    if (!path || !realpath(path, real))
        return;
    char *slash = strrchr(real, '/');
    if (slash == real)
        slash[1] = '\0'; // "/vcc" lives in "/"
    else if (slash)
        *slash = '\0';
    exe_dir = own(strdup(real));
}

//
// Derive the target's data directory, <prefix>/share/vcc/<target>, where
// <prefix> is the parent of exe_dir.  Unknown exe_dir gives a relative
// "share/vcc/<target>" that will simply not be found.
//
static void locate_share(void)
{
    char prefix[PATH_MAX] = "";

    if (exe_dir) {
        snprintf(prefix, sizeof(prefix), "%s", exe_dir);
        char *slash = strrchr(prefix, '/');
        if (slash)
            *slash = '\0'; // /usr/local/bin -> /usr/local; /bin -> ""
    }
    size_t n  = strlen(prefix) + strlen(target->name) + sizeof("/share/vcc/");
    share_dir = malloc(n);
    if (!share_dir) {
        error("out of memory");
        exit(1);
    }
    snprintf(share_dir, n, "%s%sshare/vcc/%s", prefix, exe_dir ? "/" : "", target->name);
    own(share_dir);
}

//
// Locate one of our own passes.  Resolution order:
//   1. the per-tool environment override, if set (e.g. VCC_CPP);
//   2. <exe_dir>/<name>.
// Returns an owned path, or NULL (with an error reported) if not found.
//
static char *find_pass(const char *envvar, const char *name)
{
    const char *override = getenv(envvar);
    if (override && *override)
        return own(strdup(override));

    if (!exe_dir) {
        error("cannot find '%s': the directory %s runs from is unknown; set %s", name, progname,
              envvar);
        return NULL;
    }
    char *path = concat(exe_dir, "/");
    path       = concat(path, name);
    if (access(path, X_OK) != 0) {
        error("cannot find '%s'; set %s to its path", path, envvar);
        return NULL;
    }
    return path;
}

//
// Push the blank-separated words of `words` (if any) onto `av`.
//
static void push_words(struct vec *av, const char *words)
{
    if (!words)
        return;
    char *copy = own(strdup(words)), *save;
    for (char *w = strtok_r(copy, " \t", &save); w; w = strtok_r(NULL, " \t", &save))
        vec_push(av, w);
}

//
// True if an executable `name` is found on PATH.
//
static bool on_path(const char *name)
{
    const char *path = getenv("PATH");
    if (!path)
        return false;
    char *dirs = own(strdup(path)), *save;
    for (const char *d = strtok_r(dirs, ":", &save); d; d = strtok_r(NULL, ":", &save)) {
        char *file = concat(concat(d, "/"), name);
        if (access(file, X_OK) == 0)
            return true;
    }
    return false;
}

//
// True if the tool `path` is clang, ld.lld or wasm-ld rather than GNU binutils.
//
static bool is_llvm(const char *path)
{
    const char *base = strrchr(path, '/');
    base             = base ? base + 1 : path;
    return strncmp(base, "clang", 5) == 0 || strstr(base, "lld") != NULL ||
           strstr(base, "wasm-ld") != NULL;
}

//
// Locate a tool from another project, `tool` "as" or "ld", and push it onto the
// argument vector `av`.  Resolution order:
//   1. the environment override, if set (VCC_AS, VCC_LD), split into words at
//      blanks, so that it may carry arguments ("ld.lld -n");
//   2. the path found when vcc was configured, if any;
//   3. the GNU binutils <prefix>-<tool> on PATH, by each prefix in turn;
//   4. clang or ld.lld (or the target's own LLVM linker), where the target has a clang
//      triple;
//   5. the first prefix's name, which run() then reports missing.
// Returns true if the tool is clang or ld.lld; sets *chosen_configured when it is the
// configured one (if `chosen_configured` is not NULL).
//
static bool push_tool(struct vec *av, const char *envvar, const char *configured, const char *tool,
                      bool *chosen_configured)
{
    if (chosen_configured)
        *chosen_configured = false;
    const char *override = getenv(envvar);
    if (override && strspn(override, " \t") < strlen(override)) {
        size_t first = av->len;
        push_words(av, override);
        return is_llvm(av->data[first]);
    }
    if (configured && *configured) {
        vec_push(av, (char *)configured);
        if (chosen_configured)
            *chosen_configured = true;
        return is_llvm(configured);
    }
    if (target->arch == ARCH_BESM6) {
        vec_push(av, strcmp(tool, "as") == 0 ? "b6as" : "b6ld");
        return false;
    }
    if (target->arch == ARCH_HOSTED) {
        // One C compiler does both: <prefix>-gcc, the host's own, else clang.
        char *gcc = concat(target->prefixes, "-gcc");
        if (on_path(gcc)) {
            vec_push(av, gcc);
            return false;
        }
        if (strcmp(target->name, HOST_TARGET) == 0 && on_path("cc")) {
            vec_push(av, "cc");
            return false;
        }
        vec_push(av, "clang");
        return true;
    }
    char *prefixes = own(strdup(target->prefixes)), *save;
    char *first    = NULL;
    for (const char *p = strtok_r(prefixes, " ", &save); p; p = strtok_r(NULL, " ", &save)) {
        char *name = concat(concat(p, "-"), tool);
        if (!first)
            first = name;
        if (on_path(name)) {
            vec_push(av, name);
            return false;
        }
    }
    if (target->triple) {
        if (strcmp(tool, "as") == 0)
            vec_push(av, "clang");
        else
            vec_push(av, target->llvm_ld ? (char *)target->llvm_ld : "ld.lld");
        return true;
    }
    vec_push(av, first);
    return false;
}

//
// Spawn `tool` with argument vector `argv` (NULL-terminated) and wait for it.
// A tool name without a '/' is searched on PATH.  Returns the child's exit
// status: 0 on success, nonzero on failure.  With -v, the command line is echoed
// first.
//
static int run(const char *tool, char *const argv[])
{
    if (opt_v) {
        for (char *const *p = argv; *p; p++)
            printf("%s ", *p);
        putchar('\n');
    }
    // stdout is buffered: without the flush the child would inherit a copy of
    // whatever -v had put in the buffer.
    fflush(stdout);

    pid_t pid = fork();
    if (pid < 0) {
        error("cannot run '%s': %s", tool, strerror(errno));
        return 1;
    }
    if (pid == 0) {
        execvp(tool, argv);
        // Only reached if the exec failed.  _exit(), not exit(): this is a copy of
        // the parent, and letting it flush the parent's buffers or run the parent's
        // atexit() handlers would unlink the very temp files still in use.
        diag_prefix("error");
        fprintf(stderr, "cannot run '%s': %s\n", tool, strerror(errno));
        fflush(stderr);
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            error("wait failed: %s", strerror(errno));
            return 1;
        }
    }
    const char *slash = strrchr(tool, '/');
    const char *name  = slash ? slash + 1 : tool;
    if (WIFSIGNALED(status)) {
        error("'%s' was killed by signal %d", name, WTERMSIG(status));
        return 1;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        // A tool that fails has said why: 1 is an error in the input, 2 an
        // internal compiler error, 127 a tool that could not be run. Anything
        // else would leave the user without a message.
        int code = WEXITSTATUS(status);
        if (code == 1 || code == 2 || code == 127) {
            errflag = 1;
        } else {
            error("'%s' failed with exit status %d", name, code);
        }
        return code;
    }
    return 0;
}

//
// Run the preprocessor:
//     vcpp -t <target> -nostdinc [-D__ASSEMBLER__] [cppflags] [-I<share>/include] in out
// vcpp's own compiled-in include directory is switched off and the one beside
// this vcc passed instead, which keeps a relocated installation self-consistent.
// Line markers are kept (vparse locates every AST node by them, so the errors of
// vparse and vlower name the original file and line; both assemblers take them as
// comments or line markers).
// Returns 0 on success.
//
static int run_cpp(const char *in, const char *out, bool assembler)
{
    char *tool = find_pass("VCC_CPP", "vcpp");
    if (!tool)
        return 1;

    struct vec av = { 0 };
    vec_push(&av, tool);
    vec_push(&av, "-t");
    vec_push(&av, (char *)target->name);
    vec_push(&av, "-nostdinc");
    if (assembler)
        vec_push(&av, "-D__ASSEMBLER__");
    for (size_t i = 0; i < cppflags.len; i++)
        vec_push(&av, cppflags.data[i]);
    // vcpp wants the search directory glued to the flag: -Ipath, not -I path.
    // The standard directory comes after every user -I.
    if (!opt_nostdinc)
        vec_push(&av, concat(concat("-I", share_dir), "/include"));
    if (color_flag)
        vec_push(&av, color_flag);
    vec_push(&av, (char *)in);
    if (out) // else vcpp writes to standard output
        vec_push(&av, (char *)out);
    vec_push(&av, NULL);

    int rc = run(tool, av.data);
    vec_free(&av);
    return rc;
}

//
// Parse: vparse in out.  Returns 0 on success.
//
static int run_parse(const char *in, const char *out)
{
    char *tool = find_pass("VCC_PARSE", "vparse");
    if (!tool)
        return 1;
    struct vec av = { 0 };
    vec_push(&av, tool);
    if (max_errors_flag)
        vec_push(&av, max_errors_flag);
    if (no_show_source)
        vec_push(&av, "--no-show-source");
    if (color_flag)
        vec_push(&av, color_flag);
    vec_push(&av, (char *)in);
    vec_push(&av, (char *)out);
    vec_push(&av, NULL);
    int rc = run(tool, av.data);
    vec_free(&av);
    return rc;
}

//
// Check and lower for the target: vlower -t <target> in out.  Returns 0 on
// success.
//
static int run_lower(const char *in, const char *out)
{
    char *tool = find_pass("VCC_LOWER", "vlower");
    if (!tool)
        return 1;
    struct vec av = { 0 };
    vec_push(&av, tool);
    vec_push(&av, "-t");
    vec_push(&av, (char *)target->name);
    if (max_errors_flag)
        vec_push(&av, max_errors_flag);
    if (no_show_source)
        vec_push(&av, "--no-show-source");
    if (color_flag)
        vec_push(&av, color_flag);
    vec_push(&av, (char *)in);
    vec_push(&av, (char *)out);
    vec_push(&av, NULL);
    int rc = run(tool, av.data);
    vec_free(&av);
    return rc;
}

//
// Run the code generator: vgen<target> [--bemsh|--madlen] in out.  The dialect
// flag is selected by -Sbemsh/-Smadlen (BESM-6 only); for a plain -S it is
// omitted so the code generator uses its own default.  Returns 0 on success.
//
static int run_codegen(const char *in, const char *out)
{
    char *tool = find_pass("VCC_GEN", target->codegen);
    if (!tool)
        return 1;
    struct vec av = { 0 };
    vec_push(&av, tool);
    if (codegen_dialect)
        vec_push(&av, codegen_dialect);
    if (target->gen_flag)
        vec_push(&av, (char *)target->gen_flag);
    if (color_flag)
        vec_push(&av, color_flag);
    vec_push(&av, (char *)in);
    vec_push(&av, (char *)out);
    vec_push(&av, NULL);
    int rc = run(tool, av.data);
    vec_free(&av);
    return rc;
}

//
// Run the assembler, in -> out:
//     besm6:   b6as -X -o out in
//     riscv64: riscv64-unknown-elf-as -march=rv64imfd -mabi=lp64d -o out in
//     riscv32: riscv64-unknown-elf-as -march=rv32imfd -mabi=ilp32d -o out in
//     aarch64: aarch64-none-elf-as -o out in
//     arm32:   arm-none-eabi-as -mcpu=cortex-a15 -mfpu=vfpv3-d16 -mfloat-abi=hard -o out in
//     x86_64:  x86_64-elf-as --64 -o out in
//     avr:     avr-as -mmcu=atmega1280 -o out in
//     msp430:  msp430-elf-as -mcpu=msp430 -o out in
//     mmix:    mmix-knuth-mmixware-as -x -no-predefined-syms -o out in
// or with clang: clang --target=<triple> [flags] -c -o out in, e.g.
//     riscv64: clang --target=riscv64 -march=rv64imfd -mabi=lp64d -c -o out in
//     wasm32:  clang --target=wasm32 --no-default-config <features> -c -o out in
// (a clang given by VCC_AS for a target with no triple carries its own flags).
// A hosted target assembles with its C compiler:
//     x86_64-linux: cc -c -o out in
// Returns 0 on success.
//
static int run_as(const char *in, const char *out)
{
    struct vec av = { 0 };

    bool llvm = push_tool(&av, "VCC_AS", target->as_default, "as", NULL);
    if (target->arch == ARCH_BESM6) {
        vec_push(&av, "-X");
    } else if (target->arch == ARCH_HOSTED) {
        if (llvm)
            vec_push(&av, concat("--target=", target->triple));
        vec_push(&av, "-c");
    } else if (!llvm) {
        push_words(&av, target->as_flags);
    } else if (target->triple) {
        vec_push(&av, concat("--target=", target->triple));
        push_words(&av, target->clang_flags);
        vec_push(&av, "-c");
    }
    vec_push(&av, "-o");
    vec_push(&av, (char *)out);
    vec_push(&av, (char *)in);
    vec_push(&av, NULL);
    int rc = run(av.data[0], av.data);
    vec_free(&av);
    return rc;
}

//
// The object file for `src`: with -c, the -o name or foo.o in the current
// directory; when linking, a temporary, removed on exit (as gcc does).
//
static char *object_name(const char *src)
{
    if (!opt_c)
        return make_temp("o");
    return own(outfile ? strdup(outfile) : replace_suffix(src, "o"));
}

//
// Compile one source file through the pipeline up to the stage selected by the
// -E/-S/-c flags.  A .c file runs the full pipeline; a .S file is preprocessed
// assembly (cpp -> as); a .s file only needs assembling.  A produced object file
// is appended to `objects` so a later link step can pick it up.  Returns 0 on
// success.
//
static int compile_one(const char *src)
{
    char suf = opt_x ? opt_x : suffix_of(src);

    // A .s file only needs assembling; a .o file is already an object.
    if (suf == 's') {
        if (opt_E || opt_S)
            return 0;
        char *obj = object_name(src);
        int rc    = run_as(src, obj);
        vec_push(&objects, obj);
        return rc;
    }

    // A .S file is assembly that must be preprocessed first: cpp -> as.  Both
    // assemblers accept the resulting "# line" markers, so run_cpp's output feeds
    // straight into the assembler.
    if (suf == 'S') {
        // Where the preprocessed assembly goes depends on the stop stage.
        const char *sfile;
        if (opt_E)
            sfile = outfile; // NULL: standard output
        else if (opt_S)
            sfile = own(outfile ? strdup(outfile) : replace_suffix(src, "s"));
        else
            sfile = make_temp("s");

        // Guard against overwriting the source on a case-insensitive filesystem,
        // where replace_suffix("foo.S", "s") == "foo.s" names the same file.
        if ((opt_E || opt_S) && sfile && same_file(src, sfile)) {
            error("'%s': refusing to overwrite the input; use '-o'", src);
            return 1;
        }

        if (run_cpp(src, sfile, true) != 0)
            return 1;
        if (opt_E || opt_S)
            return 0;

        // Assemble the preprocessed output: .s -> .o
        char *obj = object_name(src);
        int rc    = run_as(sfile, obj);
        vec_push(&objects, obj);
        return rc;
    }

    if (suf != 'c') {
        error("'%s': unknown file type; name it .c, .S, .s, .o or .a, or use '-x'", src);
        return 1;
    }

    // Preprocess: .c -> .i
    const char *ifile;
    if (opt_E)
        ifile = outfile; // NULL: standard output
    else
        ifile = make_temp("i");
    if (run_cpp(src, ifile, false) != 0)
        return 1;
    if (opt_E)
        return 0;

    // Parse and lower: .i -> .ast -> .tac
    const char *astfile = make_temp("ast");
    if (run_parse(ifile, astfile) != 0)
        return 1;
    const char *tacfile = make_temp("tac");
    if (run_lower(astfile, tacfile) != 0)
        return 1;

    // Code generation: .tac -> .s (or .mad/.bemsh for the -Smadlen/-Sbemsh
    // dialects, so the derived name reflects the assembly dialect emitted --
    // the same extensions vgenbesm6 itself picks).
    const char *asmsuf = !codegen_dialect                           ? "s"
                         : strcmp(codegen_dialect, "--madlen") == 0 ? "mad"
                                                                    : "bemsh";
    const char *asmfile =
        opt_S ? own(outfile ? strdup(outfile) : replace_suffix(src, asmsuf)) : make_temp("s");
    if (run_codegen(tacfile, asmfile) != 0)
        return 1;
    if (opt_S)
        return 0;

    // Assemble: .s -> .o
    char *obj = object_name(src);
    int rc    = run_as(asmfile, obj);
    vec_push(&objects, obj);
    return rc;
}

//
// Link all collected objects into an executable:
//     besm6:   b6ld -X -e _start -o out -L<lib> <lib>/crt0.o objs ldflags -lc -lruntime
//     msp430:  msp430-elf-ld --gc-sections -T <script> -o out -L<lib> <lib>/crt0.o objs
//              ldflags -lc [libgcc.a]
//     mmix:    mmix-knuth-mmixware-ld --defsym=__.MMIX.start..text=0x100 -o out -L<lib>
//              <lib>/crt0.o objs ldflags -lc [libgcc.a]
//     wasm32:  wasm-ld --stack-first -z stack-size=1048576 -o out -L<lib> <lib>/crt0.o objs
//              ldflags -lc
//     wasm32-braam: wasm-ld --no-entry --import-memory --stack-first -z stack-size=131072
//              --gc-sections --initial-memory=<pages * 65536> -o out -L<lib> <lib>/crt0.o
//              objs ldflags -lc, then the braam section (braam_stamp)
//     others:  <prefix>-ld [flags] -T <script> -o out -L<lib> <lib>/crt0.o objs ldflags -lc,
//              e.g. riscv64-unknown-elf-ld -m elf32lriscv for riscv32, avr-ld -m avr51;
//              or ld.lld with no flags
// where <lib> is <share>/lib.  -nostdlib drops the -L, crt0.o and the implicit
// archives; the linker script (the qemu `virt` memory map) stays, unless
// -T names another.  MMIX takes the linker's default script.  A missing crt0.o is a fatal error.
// See README.md, "Linking".  Returns 0 on success.
//
//
// Link for a hosted target, with its C compiler:
//     cc [--target=<triple>] -no-pie -o out objs ldflags -L<lib> -lvcc
// The compiler adds the startup files, the C library and libgcc.  Not position
// independent on Linux: our code takes a function's address PC-relative, which a PIE
// cannot do for one in a shared library.  -nostdlib is passed on, and drops libvcc.a.
// On macOS, where every arm64 executable is a PIE, the code reaches what it does not
// define through the GOT.
//
static int link_hosted(const char *libdir)
{
    struct vec av = { 0 };
    if (push_tool(&av, "VCC_LD", target->ld_default, "ld", NULL))
        vec_push(&av, concat("--target=", target->triple));
    if (!target->pie)
        vec_push(&av, "-no-pie");
    if (opt_nostdlib)
        vec_push(&av, "-nostdlib");
    if (linkscript) {
        vec_push(&av, "-T");
        vec_push(&av, linkscript);
    }
    vec_push(&av, "-o");
    vec_push(&av, outfile ? outfile : (char *)"a.out");
    for (size_t i = 0; i < objects.len; i++)
        vec_push(&av, objects.data[i]);
    for (size_t i = 0; i < ldflags.len; i++)
        vec_push(&av, ldflags.data[i]);
    if (!opt_nostdlib) {
        char *lib = concat(libdir, "/libvcc.a");
        if (access(lib, R_OK) != 0) {
            error("'%s' not found; install the runtime, or use '-nostdlib'", lib);
            vec_free(&av);
            return 1;
        }
        vec_push(&av, concat("-L", libdir));
        vec_push(&av, "-lvcc");
    }
    vec_push(&av, NULL);

    int rc = run(av.data[0], av.data);
    vec_free(&av);
    return rc;
}

//
// The metadata Braam's exec reads, appended to a linked process: a custom section
// "braam" of five little-endian u32, magic, PROC_ABI, flags, the initial pages (those
// of --initial-memory) and the most the kernel allows; braam-core's tools/stamp.py
// does the same for its own programs.  An earlier "braam" section is dropped first.
// The numbers are libc/wasm32/braam/include/braam.h's.  Returns 0 on success.
//
static int braam_stamp(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        error("cannot read '%s': %s", path, strerror(errno));
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *data = malloc(size > 0 ? (size_t)size : 1);
    unsigned char *out  = malloc((size_t)size + 64);
    if (!data || !out || fread(data, 1, (size_t)size, f) != (size_t)size || size < 8 ||
        memcmp(data, "\0asm", 4) != 0) {
        fclose(f);
        free(data);
        free(out);
        error("'%s' is not a wasm module", path);
        return 1;
    }
    fclose(f);

    // Every section but a "braam" one.
    size_t n = 8, at = 8;
    memcpy(out, data, 8);
    while (at < (size_t)size) {
        size_t start = at, len = 0;
        unsigned shift   = 0;
        unsigned char id = data[at++];
        while (at < (size_t)size) {
            unsigned char b = data[at++];
            len |= (size_t)(b & 0x7f) << shift;
            shift += 7;
            if (!(b & 0x80))
                break;
        }
        size_t end = at + len < (size_t)size ? at + len : (size_t)size;
        bool braam = id == 0 && len >= 6 && data[at] == 5 && memcmp(data + at + 1, "braam", 5) == 0;
        if (!braam) {
            memcpy(out + n, data + start, end - start);
            n += end - start;
        }
        at = end;
    }

    // The section: id 0, its size, the name, five words.
    const unsigned long meta[5] = { 0x6d617262, 21, 0, braam_pages, 1600 };
    out[n++]                    = 0;
    out[n++]                    = 1 + 5 + 20;
    out[n++]                    = 5;
    memcpy(out + n, "braam", 5);
    n += 5;
    for (int i = 0; i < 5; i++)
        for (int k = 0; k < 4; k++)
            out[n++] = (unsigned char)(meta[i] >> (8 * k));

    f       = fopen(path, "wb");
    bool ok = f && fwrite(out, 1, n, f) == n;
    if (f && fclose(f) != 0)
        ok = false;
    free(data);
    free(out);
    if (!ok)
        error("cannot write '%s'", path);
    return ok ? 0 : 1;
}

static int link_objects(void)
{
    char *libdir = concat(share_dir, "/lib");
    if (target->arch == ARCH_HOSTED)
        return link_hosted(libdir);

    struct vec av = { 0 };
    bool configured;
    bool llvm = push_tool(&av, "VCC_LD", target->ld_default, "ld", &configured);
    switch (target->arch) {
    case ARCH_BESM6:
        vec_push(&av, "-X");
        vec_push(&av, "-e");
        vec_push(&av, "_start");
        break;
    case ARCH_CROSS: {
        if (!llvm)
            push_words(&av, configured ? target->ld_default_flags : target->ld_flags);
        else
            push_words(&av, target->llvm_ld_flags);
        if (target->ld_flag)
            vec_push(&av, (char *)target->ld_flag);
        if (target->braam) {
            char pages[48];
            snprintf(pages, sizeof pages, "--initial-memory=%lu", braam_pages * 65536UL);
            vec_push(&av, concat(pages, ""));
        }
        if (target->no_script && !linkscript)
            break;
        char *script = linkscript ? linkscript : concat(libdir, "/link.ld");
        if (access(script, R_OK) != 0) {
            error("linker script '%s' not found; use '-T'", script);
            vec_free(&av);
            return 1;
        }
        vec_push(&av, "-T");
        vec_push(&av, script);
        break;
    }
    case ARCH_HOSTED:
        break; // link_hosted
    }
    vec_push(&av, "-o");
    vec_push(&av, outfile ? outfile : (char *)"a.out");
    // The standard library dir and the crt0 startup object come before the
    // objects, crt0 first so its _start leads the text; -nostdlib skips both.
    if (!opt_nostdlib) {
        char *crt0 = concat(libdir, "/crt0.o");
        if (access(crt0, R_OK) != 0) {
            if (target->arch == ARCH_BESM6)
                error(
                    "'%s' not found: the BESM-6 crt0.o and libc.a come from v7besm and "
                    "must be installed into %s; or use '-nostdlib'",
                    crt0, libdir);
            else
                error("'%s' not found; install the runtime, or use '-nostdlib'", crt0);
            vec_free(&av);
            return 1;
        }
        vec_push(&av, concat("-L", libdir));
        vec_push(&av, crt0);
    }
    for (size_t i = 0; i < objects.len; i++)
        vec_push(&av, objects.data[i]);
    // User -L/-l flags follow the objects so user-named libraries resolve their
    // references (conventional link order).
    for (size_t i = 0; i < ldflags.len; i++)
        vec_push(&av, ldflags.data[i]);
    // The implicit C library, then (BESM-6) the compiler's helper archive.
    // libruntime.a is LAST, and the order is not cosmetic: b6ld scans an archive
    // once, in order, and libc calls the b$* helpers while no helper calls back
    // into libc.
    // On the MSP430 and MMIX, GCC's libgcc.a follows our libc.a for the helpers only GCC's
    // code calls; without it, objects of our own compiler still link.
    if (!opt_nostdlib) {
        vec_push(&av, "-lc");
        if (target->arch == ARCH_BESM6)
            vec_push(&av, "-lruntime");
        if (target->libgcc && *target->libgcc && access(target->libgcc, R_OK) == 0)
            vec_push(&av, (char *)target->libgcc);
    }
    vec_push(&av, NULL);

    int rc = run(av.data[0], av.data);
    vec_free(&av);
    if (rc == 0 && target->braam)
        rc = braam_stamp(outfile ? outfile : "a.out");
    return rc;
}

//
// Select the target by name; an unknown one is a usage error listing the valid
// names.
//
static const struct target *find_target(const char *name)
{
    for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); i++)
        if (strcmp(name, targets[i].name) == 0)
            return &targets[i];
    return NULL;
}

static void select_target(const char *name)
{
    target = find_target(name);
    if (target)
        return;
    error("unknown target '%s'", name);
    diag_prefix("note");
    fprintf(stderr, "the targets are");
    for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); i++)
        fprintf(stderr, " %s", targets[i].name);
    fprintf(stderr, "\n");
    exit(1);
}

static void usage(int status)
{
    printf("Usage:\n");
    printf("    %s [options] file...\n", progname);
    printf("Options:\n");
    printf("    -t, --target NAME  Target: x86_64-linux, aarch64-linux, riscv64-linux,\n");
    printf("                       aarch64-darwin (hosted), riscv64, riscv32, aarch64, arm32, x86_64, avr,\n");
    printf("                       msp430, mmix, besm6 (bare metal), wasm32 or wasm32-braam\n");
    printf("                       (a process of Braam); default %s\n",
           *HOST_TARGET ? HOST_TARGET : "riscv64");
    printf("    -c              Compile and assemble, but do not link\n");
    printf("    -S              Compile only; emit assembly (.s)\n");
    printf("    -Sbemsh         Like -S, but emit Bemsh-dialect assembly (besm6)\n");
    printf("    -Smadlen        Like -S, but emit Madlen-dialect assembly (besm6)\n");
    printf("    -E              Preprocess only; write to output or standard output\n");
    printf("    -P              With -E, no line markers\n");
    printf("    -x LANG         Input language: c, assembler-with-cpp, assembler or none\n");
    printf("    -o file         Set output file name\n");
    printf("    -O              Optimize (reserved; currently a no-op)\n");
    printf("    -g              Emit debug info (reserved; currently a no-op)\n");
    printf("    -v              Verbose: echo each sub-command\n");
    printf("    -Dname[=val]    Predefine a preprocessor macro\n");
    printf("    -Uname          Undefine a preprocessor macro\n");
    printf("    -Ipath          Add a header search directory\n");
    printf("    -Lpath          Add a library search directory (for the linker)\n");
    printf("    -lname          Link against library libname (for the linker)\n");
    printf("    -T file         Linker script instead of the standard one (not besm6)\n");
    printf(
        "    -nostdlib       Do not use the standard library dir, crt0.o or the implicit "
        "libraries\n");
    printf("    -nostdinc       Do not add the standard include directory\n");
    printf("    --initial-pages=N  wasm32-braam: the process's initial memory, in 64 KiB pages\n");
    printf("                    (default 4)\n");
    printf("    -fmax-errors=N  Stop after N errors (0: no limit; default 20)\n");
    printf("    -fno-diagnostics-show-caret  Don't show the source line under a diagnostic\n");
    printf("    -fdiagnostics-color[=WHEN]  Color diagnostics: always (the default WHEN), never\n");
    printf("                    or auto (when stderr is a terminal; the default)\n");
    printf("    -fno-diagnostics-color  Same as -fdiagnostics-color=never\n");
    printf("    -W..., -f..., -std=..., -pedantic, -pipe, -arch A, -isysroot D\n");
    printf("                    Accepted and ignored, for build systems made for GCC\n");
    printf(
        "Inputs are dispatched by suffix: .c (compile), "
        ".S (preprocess + assemble), .s (assemble), .o and .a (link).\n");
    exit(status);
}

int main(int argc, char *argv[])
{
    // Derive the diagnostic prefix from argv[0]'s basename (fallback "vcc").
    if (argc > 0 && argv[0] && argv[0][0]) {
        char *slash = strrchr(argv[0], '/');
        progname    = slash ? slash + 1 : argv[0];
    }

    atexit(cleanup);
    target = find_target(*HOST_TARGET ? HOST_TARGET : "riscv64");

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (arg[0] != '-' || arg[1] == '\0') {
            vec_push(&sources, arg);
            continue;
        }
        // Multi-character options that the single-letter switch would misread.
        if (strcmp(arg, "-nostdlib") == 0) {
            opt_nostdlib = true;
            continue;
        }
        if (strcmp(arg, "-nostdinc") == 0) {
            opt_nostdinc = true;
            continue;
        }
        if (strncmp(arg, "--initial-pages=", 16) == 0) {
            char *end;
            braam_pages = strtoul(arg + 16, &end, 10);
            if (*end || braam_pages == 0 || braam_pages > 1600) {
                error("invalid value in '%s': 1 to 1600 pages of 64 KiB", arg);
                exit(1);
            }
            continue;
        }
        if (strncmp(arg, "--target", 8) == 0) {
            if (arg[8] == '=') {
                select_target(arg + 9);
            } else if (arg[8] == '\0' && i + 1 < argc) {
                select_target(argv[++i]);
            } else if (arg[8] == '\0') {
                error("missing argument to '%s'", arg);
                exit(1);
            } else {
                error("unknown option '%s'", arg);
                exit(1);
            }
            continue;
        }
        // Options of GCC and clang that build systems pass (CMake among them):
        // accepted and ignored, those with a value taking it along.
        if (strcmp(arg, "-arch") == 0 || strcmp(arg, "-isysroot") == 0) {
            if (i + 1 < argc)
                i++;
            continue;
        }
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0)
            usage(0);
        if (strcmp(arg, "-x") == 0 && i + 1 >= argc) {
            error("missing argument to '%s'", arg);
            exit(1);
        }
        if (strcmp(arg, "-x") == 0) {
            const char *lang = argv[++i];
            if (strcmp(lang, "c") == 0)
                opt_x = 'c';
            else if (strcmp(lang, "assembler-with-cpp") == 0)
                opt_x = 'S';
            else if (strcmp(lang, "assembler") == 0)
                opt_x = 's';
            else if (strcmp(lang, "none") == 0)
                opt_x = 0;
            else {
                error("unknown language '%s'", lang);
                exit(1);
            }
            continue;
        }
        if (strcmp(arg, "-P") == 0) { // no line markers, for the preprocessor
            vec_push(&cppflags, arg);
            continue;
        }
        if (strncmp(arg, "-fmax-errors=", 13) == 0) {
            char *end;
            (void)strtoul(arg + 13, &end, 10);
            if (arg[13] == '\0' || *end) {
                error("invalid value in '%s'", arg);
                exit(1);
            }
            max_errors_flag = concat("--max-errors=", arg + 13);
            continue;
        }
        if (strcmp(arg, "-fno-diagnostics-show-caret") == 0) {
            no_show_source = 1;
            continue;
        }
        if (strcmp(arg, "-fdiagnostics-show-caret") == 0) {
            no_show_source = 0;
            continue;
        }
        if (strcmp(arg, "-fdiagnostics-color") == 0 ||
            strcmp(arg, "-fdiagnostics-color=always") == 0) {
            color      = 1;
            color_flag = "--color=always";
            continue;
        }
        if (strcmp(arg, "-fno-diagnostics-color") == 0 ||
            strcmp(arg, "-fdiagnostics-color=never") == 0) {
            color      = 0;
            color_flag = "--color=never";
            continue;
        }
        if (strcmp(arg, "-fdiagnostics-color=auto") == 0) {
            color      = -1;
            color_flag = NULL;
            continue;
        }
        if (strncmp(arg, "-fdiagnostics-color=", 20) == 0) {
            error("invalid value in '%s'", arg);
            exit(1);
        }
        if (arg[1] == 'W' || arg[1] == 'f' || arg[1] == 'w' || strncmp(arg, "-std=", 5) == 0 ||
            strncmp(arg, "-pedantic", 9) == 0 || strcmp(arg, "-pipe") == 0) {
            continue;
        }
        switch (arg[1]) {
        case 'c':
            opt_c = true;
            break;
        case 'S':
            opt_S = true;
            if (arg[2] == '\0')
                break; // plain -S: codegen default dialect
            if (strcmp(arg + 2, "bemsh") == 0)
                codegen_dialect = "--bemsh";
            else if (strcmp(arg + 2, "madlen") == 0)
                codegen_dialect = "--madlen";
            else {
                error("unknown option '%s'", arg);
                exit(1);
            }
            break;
        case 'E':
            opt_E = true;
            break;
        case 'O':
            opt_O = true;
            break;
        case 'g':
            opt_g = true;
            break;
        case 'v':
            opt_v = true;
            break;
        case 'o':
        case 't':
        case 'T':
            // Options with a value, glued (-ofile) or separate (-o file).
            if (arg[2] == '\0' && i + 1 >= argc) {
                error("missing argument to '%s'", arg);
                exit(1);
            }
            {
                char *val = arg[2] ? arg + 2 : argv[++i];
                if (arg[1] == 'o')
                    outfile = val;
                else if (arg[1] == 't')
                    select_target(val);
                else
                    linkscript = val;
            }
            break;
        case 'D':
        case 'U':
        case 'I':
            // Pass through to the preprocessor.  vcpp requires the value glued
            // to the flag (-DNAME), so fold the separated form (-D NAME) into
            // one token.
            if (arg[2]) {
                vec_push(&cppflags, arg);
            } else if (i + 1 < argc) {
                const char flag[3] = { '-', arg[1], '\0' };
                vec_push(&cppflags, concat(flag, argv[++i]));
            } else {
                error("missing argument to '%s'", arg);
                exit(1);
            }
            break;
        case 'L':
        case 'l':
            // Pass through to the linker.  Fold the separated form (-L dir) into
            // one glued token, matching the -D/-I handling above.
            if (arg[2]) {
                vec_push(&ldflags, arg);
            } else if (i + 1 < argc) {
                const char flag[3] = { '-', arg[1], '\0' };
                vec_push(&ldflags, concat(flag, argv[++i]));
            } else {
                error("missing argument to '%s'", arg);
                exit(1);
            }
            break;
        default:
            error("unknown option '%s'", arg);
            exit(1);
        }
    }

    if (sources.len == 0) {
        error("no input files");
        exit(1);
    }
    if ((opt_E || opt_S || opt_c) && outfile && sources.len > 1) {
        error("cannot specify '-o' with '-c', '-S' or '-E' and multiple input files");
        return 1;
    }
    if (codegen_dialect && target->arch != ARCH_BESM6) {
        error("'-S%s' requires target 'besm6'", codegen_dialect + 2);
        return 1;
    }
    if (linkscript && target->arch == ARCH_BESM6) {
        error("'-T' is not supported on target 'besm6'");
        return 1;
    }

    locate_self(argv[0]);
    locate_share();

    // Compile each source; a failure on one file skips the link step but does
    // not abort the remaining compilations.
    bool compiled_ok = true;
    for (size_t i = 0; i < sources.len; i++) {
        if (suffix_of(sources.data[i]) == 'o' || is_archive(sources.data[i])) {
            vec_push(&objects, sources.data[i]);
            continue;
        }
        if (compile_one(sources.data[i]) != 0)
            compiled_ok = false;
    }

    // Link unless we stopped early or a compile failed.
    if (compiled_ok && !opt_c && !opt_S && !opt_E && objects.len > 0)
        link_objects();

    vec_free(&sources);
    vec_free(&objects);
    vec_free(&cppflags);
    vec_free(&ldflags);
    return errflag ? 1 : 0;
}
