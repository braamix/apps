#!/usr/bin/env python3
"""Convert FreeBSD gzip sources into Braam gzip.cpp body."""

import re
import sys
from pathlib import Path

UPSTREAM = Path("/Users/vak/Project/BSD/FreeBSD-github/usr.bin/gzip")
OUT = Path(__file__).resolve().parent.parent / "gzip_body.inc.cpp"

SKIP_INCLUDES = {
    "<err.h>",
    "<fts.h>",
    "<libgen.h>",
    "<getopt.h>",
    "<signal.h>",
    "<sys/param.h>",
    "<sys/time.h>",
    "<inttypes.h>",
}

TASK_VOID = {
    "handle_stdin",
    "handle_stdout",
    "handle_pathname",
    "handle_file",
    "handle_dir",
    "usage",
    "display_version",
    "display_license",
    "print_verbage",
    "print_test",
    "print_list_out",
    "prepend_gzip",
    "copymodes",
    "infile_newdata",
    "infile_set",
    "infile_clear",
    "setup_signals",
    "got_sigint",
    "got_siginfo",
    "print_ratio",
    "unlink_input",
    "print_list",
}

TASK_BOOL = {"io_pread", "parse_indexes"}

TASK_OFF_T = {
    "gz_compress",
    "gz_uncompress",
    "file_compress",
    "file_uncompress",
    "cat_fd",
    "unbzip2",
    "unxz",
    "unxz_len",
    "unzstd",
    "unlz",
    "unpack",
    "zuncompress",
}

TASK_INT = {"check_outfile"}

TASK_SSIZE = {"read_retry", "write_retry"}

def strip_gzip_c_includes_at_end(text: str) -> str:
    for macro, fname in (
        ("NO_BZIP2_SUPPORT", "unbzip2.c"),
        ("NO_COMPRESS_SUPPORT", "zuncompress.c"),
        ("NO_PACK_SUPPORT", "unpack.c"),
        ("NO_XZ_SUPPORT", "unxz.c"),
        ("NO_LZ_SUPPORT", "unlz.c"),
        ("NO_ZSTD_SUPPORT", "unzstd.c"),
    ):
        text = re.sub(
            rf"#ifndef {macro}\n#include \"{re.escape(fname)}\"\n#endif\n",
            "",
            text,
        )
    return text


def read_gzip_c() -> str:
    text = strip_gzip_c_includes_at_end((UPSTREAM / "gzip.c").read_text()) + "\n"
    for name in ("unbzip2.c", "unxz.c", "unzstd.c"):
        chunk = (UPSTREAM / name).read_text()
        chunk = re.sub(r"^#include.*\n", "", chunk, flags=re.M)
        text += chunk + "\n"
    return text


def strip_headers(text: str) -> str:
    lines = []
    in_include = False
    for line in text.splitlines():
        if line.startswith("#include"):
            if any(x in line for x in SKIP_INCLUDES):
                continue
            if "<fts.h>" in line or "<err.h>" in line:
                continue
            if "gzip.c" not in line and line.strip() == '#include "braam.h"':
                continue
        lines.append(line)
    return "\n".join(lines)


def remove_main(text: str) -> str:
    text = re.sub(
        r"int\s+main\s*\([^)]*\)\s*\{",
        "Task<i32>\ngzip_main(Args args)\n{",
        text,
        count=1,
    )
    return text


def drop_braced_function(text: str, pattern: str) -> str:
    m = re.search(pattern, text, flags=re.M)
    if not m:
        return text
    start = m.start()
    i = m.end() - 1
    depth = 0
    while i < len(text):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[:start] + text[i + 1 :]
        i += 1
    return text


def fix_task_decls(text: str) -> str:
    for fn in TASK_VOID:
        text = re.sub(
            rf"static\s+void\s+({fn})\s*\(",
            rf"static Task<void>\n\1(",
            text,
        )
        text = re.sub(rf"^void\s+({fn})\s*\(", rf"Task<void>\n\1(", text, flags=re.M)
    for fn in TASK_OFF_T:
        text = re.sub(
            rf"static\s+off_t\s+({fn})\s*\(",
            rf"static Task<off_t>\n\1(",
            text,
        )
        text = re.sub(rf"^off_t\s+({fn})\s*\(", rf"Task<off_t>\n\1(", text, flags=re.M)
    for fn in TASK_INT:
        text = re.sub(
            rf"static\s+int\s+({fn})\s*\(",
            rf"static Task<int>\n\1(",
            text,
        )
    for fn in TASK_SSIZE:
        text = re.sub(
            rf"static\s+ssize_t\s+({fn})\s*\(",
            rf"static Task<ssize_t>\n\1(",
            text,
        )
    for fn in TASK_BOOL:
        text = re.sub(
            rf"static\s+bool\s+({fn})\s*\(",
            rf"static Task<bool>\n\1(",
            text,
        )
    return text


def replace_io(text: str) -> str:
    text = drop_braced_function(text, r"static\s+ssize_t\s+read_retry\s*\(")
    text = drop_braced_function(text, r"static\s+Task<ssize_t>\s+read_retry\s*\(")
    text = drop_braced_function(text, r"static\s+ssize_t\s+write_retry\s*\(")
    text = drop_braced_function(text, r"static\s+Task<ssize_t>\s+write_retry\s*\(")

    text = re.sub(r"\bpread\s*\(", "co_await gzip_pread(", text)
    text = re.sub(r"\bread\s*\(", "co_await read_retry(", text)
    text = re.sub(r"\bwrite\s*\(", "co_await write_retry(", text)

    text = text.replace("getprogname()", "gzip_progname(args[0])")
    text = text.replace("setup_signals();", "/* signals via sig_catch */")

    # copymodes stub
    text = re.sub(
        r"static Task<void>\ncopymodes\([^)]+\)\s*\{[^}]*(?:\{[^}]*\}[^}]*)*\}",
        "static Task<void>\ncopymodes(int fd, const struct stat *sbp, const char *file)\n{\n\t(void)fd;\n\t(void)sbp;\n\t(void)file;\n\tco_return;\n}",
        text,
        flags=re.S,
    )

    text = re.sub(r"\bexit\s*\(\s*exit_value\s*\)", "co_return exit_value", text)
    text = re.sub(r"\bexit\s*\(\s*0\s*\)", "co_return 0", text)
    text = re.sub(r"\bexit\s*\(\s*2\s*\)", "co_return 2", text)
    text = re.sub(r"\b_exit\s*\(\s*2\s*\)", "co_return 130", text)

    text = re.sub(r"\bopen\s*\(", "co_await b_open(", text)
    text = re.sub(r"\bclose\s*\(", "co_await b_close(", text)
    text = re.sub(r"\bunlink\s*\(", "co_await b_unlink(", text)
    text = re.sub(r"\blseek\s*\(", "co_await b_lseek(", text)
    text = re.sub(r"\bstat\s*\(", "co_await b_stat(", text)
    text = re.sub(r"\blstat\s*\(", "co_await b_lstat(", text)
    text = re.sub(r"\bfstat\s*\(", "co_await b_fstat(", text)
    text = re.sub(r"\bisatty\s*\(", "co_await b_isatty(", text)
    text = re.sub(r"\bdup\s*\(", "co_await b_dup(", text)

    text = re.sub(r"\bfprintf\s*\(", "co_await b_fprintf(", text)
    text = re.sub(r"\bprintf\s*\(", "co_await b_printf(", text)
    text = re.sub(r"\bfflush\s*\(", "co_await b_fflush(", text)
    text = re.sub(r"\bfgets\s*\(", "co_await b_fgets(", text)

    text = text.replace("time(NULL)", "ztime(NULL)")
    text = re.sub(
        r"char \*date = ctime\(&ts\);",
        "char datebuf[32]; struct tm tm; gmtime_r(&ts, &tm); strftime(datebuf, sizeof datebuf, \"%b %e %H:%M\", &tm); char *date = datebuf;",
        text,
    )

    text = text.replace("basename(file)", "path_basename({file, strlen(file)})")
    return text


def fix_main_args(text: str) -> str:
    # Replace argc/argv with Args
    insert = """
\tstatic char prog_storage[256];
\tStr prog = args.size() > 0 ? args[0] : Str("gzip");
\t{
\t\tusize pn = prog.size();
\t\tif (pn >= sizeof prog_storage) pn = sizeof prog_storage - 1;
\t\tmemcpy(prog_storage, prog.data(), pn);
\t\tprog_storage[pn] = '\\0';
\t}
\tconst char *progname = prog_storage;
\tif (strcmp(progname, "gunzip") == 0)
\t\tdflag = 1;
\telse if (strcmp(progname, "zcat") == 0 || strcmp(progname, "gzcat") == 0)
\t\tdflag = cflag = 1;

\tVec<Str> argvec;
\tfor (usize i = 0; i < args.size(); i++)
\t\tif (!argvec.push(args[i])) { co_return 2; }
"""
    text = re.sub(
        r"Task<i32>\ngzip_main\(Args args\)\n\{",
        "Task<i32>\ngzip_main(Args args)\n{" + insert,
        text,
        count=1,
    )
    return text


def main():
    text = read_gzip_c()
    text = strip_headers(text)
    text = remove_main(text)
    text = fix_task_decls(text)
    text = replace_io(text)

    header = """// Generated from FreeBSD gzip — do not edit by hand; run tools/convert.py
#include "braam.h"
#include "kernel/args.h"
#include "kernel/vec.h"

#include <bzlib.h>
#include <lzma.h>
#include <zstd.h>

"""
    OUT.write_text(header + text)
    print("Wrote", OUT)


if __name__ == "__main__":
    main()
