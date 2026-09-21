#!/usr/bin/env python3
"""Post-process gzip_body.inc.cpp for Braam."""

import re
from pathlib import Path

BODY = Path(__file__).resolve().parent.parent / "gzip_body.inc.cpp"

AWAIT_FUNCS = [
    "handle_stdin",
    "handle_stdout",
    "handle_pathname",
    "handle_file",
    "handle_dir",
    "file_compress",
    "file_uncompress",
    "gz_compress",
    "gz_uncompress",
    "cat_fd",
    "check_outfile",
    "unbzip2",
    "unxz",
    "unzstd",
    "unxz_len",
    "parse_indexes",
    "unlz",
    "unpack",
    "zuncompress_fd",
    "usage",
    "display_version",
    "display_license",
    "print_verbage",
    "print_test",
    "print_list",
    "print_list_out",
    "print_ratio",
    "read_retry",
    "write_retry",
    "gzip_pread",
    "io_pread",
    "unlink_input",
]


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


FORWARD_DECLS = """
static Task<off_t> gz_compress(int, int, off_t *, const char *, uint32_t);
static Task<off_t> gz_uncompress(int, int, char *, size_t, off_t *, const char *);
static Task<off_t> file_compress(char *, char *, size_t);
static Task<off_t> file_uncompress(char *, char *, size_t);
static Task<off_t> cat_fd(unsigned char *, size_t, off_t *, int);
static Task<int> check_outfile(const char *);
static Task<void> handle_file(char *, struct stat *);
static Task<void> handle_dir(char *);
static Task<void> print_list_out(off_t, off_t, const char *);
static Task<void> print_verbage(const char *, const char *, off_t, off_t);
static Task<void> print_test(const char *, int);
static Task<void> unlink_input(const char *, const struct stat *);
"""


def main():
    text = BODY.read_text()

    text = re.sub(
        r"static Task<void>\ncheck_siginfo\(void\)\s*\{[\s\S]*?\n\}\n\n",
        "",
        text,
        count=1,
    )
    text = re.sub(
        r"static void\ncheck_siginfo\(void\)\s*\{[\s\S]*?\n\}\n\n",
        "",
        text,
        count=1,
    )
    text = re.sub(r"static void\nco_await print_list\(", "static Task<void>\nprint_list(", text, count=1)
    text = re.sub(
        r"case FT_Z:\s*\n\s*if \(\(in = zdopen\(STDIN_FILENO\)\)[\s\S]*?fclose\(in\);\s*\n\s*break;",
        """case FT_Z:
\t\tusize = co_await zuncompress_fd(STDIN_FILENO, STDOUT_FILENO,
\t\t    (char *)fourbytes, sizeof fourbytes, &gsize);
\t\tbreak;""",
        text,
        count=1,
    )
    text = text.replace(
        "\treturn in_size == -1 ? -1 : size;",
        "\tco_return in_size == -1 ? -1 : size;",
    )
    text = re.sub(r"static Task<void>\nusage\s*\(", "Task<void>\nusage(", text, count=1)
    text = re.sub(r"static Task<void>\ndisplay_license\s*\(", "Task<void>\ndisplay_license(", text, count=1)
    text = re.sub(r"static Task<void>\ndisplay_version\s*\(", "Task<void>\ndisplay_version(", text, count=1)
    text = text.replace("static Task<void>\nprint_list", "Task<void>\nprint_list")
    text = text.replace("static Task<void>\nusage", "Task<void>\nusage")
    text = text.replace("static Task<void>\ndisplay_license", "Task<void>\ndisplay_license")
    text = text.replace("static Task<void>\ndisplay_version", "Task<void>\ndisplay_version")
    text = re.sub(r"\tint error;\n", "\t/* int error */;\n", text, count=1)
    text = re.sub(
        r"#ifndef NO_COMPRESS_SUPPORT\n\tFILE \*in;\n#endif\n",
        "",
        text,
        count=1,
    )

    text = re.sub(
        r"(static Task<void>\nhandle_stdin\(void\)[\s\S]*?case FT_Z:)\s*\{[\s\S]*?break;\s*\}",
        r"""\1 {
\t\tusize = co_await zuncompress_fd(STDIN_FILENO, STDOUT_FILENO,
\t\t    (char *)fourbytes, sizeof fourbytes, &gsize);
\t\tbreak;
\t}""",
        text,
        count=1,
    )

    text = re.sub(
        r"static\s+void\s+maybe_err\(const char \*fmt, \.\.\.\).*?"
        r"/\* compress input to output",
        "/* compress input to output",
        text,
        count=1,
        flags=re.S,
    )

    text = re.sub(r"static time_t ztime.*?^\}\n\n", "", text, count=1, flags=re.S | re.M)
    text = re.sub(
        r"Task<i32>\ngzip_main\(Args args\).*?^\tco_return exit_value;\n\}",
        "",
        text,
        count=1,
        flags=re.S | re.M,
    )
    text = re.sub(
        r"static const struct option longopts\[\] = \{.*?\};\n\n",
        "",
        text,
        flags=re.S,
    )

    text = re.sub(
        r"typedef struct \{\s*const char\s*\*zipped;[\s\S]*?\} suffixes_t;\n",
        "",
        text,
        count=1,
    )
    text = text.replace("static suffixes_t suffixes[]", "suffixes_t suffixes[]")
    text = re.sub(
        r"static\s+int\s+cflag;[\s\S]*?static\s+const char \*infile;[^\n]*\n\n",
        "",
        text,
        count=1,
    )
    text = text.replace(
        "#define NUM_SUFFIXES (nitems(suffixes))",
        "const int gzip_num_suffixes = (int)(sizeof(suffixes) / sizeof(suffixes[0]));\n#define NUM_SUFFIXES gzip_num_suffixes",
    )

    text = re.sub(r"\berrx\s*\(", "maybe_errx(", text)
    text = re.sub(r"\bwarn\s*\(", "maybe_warn(", text)
    text = re.sub(r"\bwarnx\s*\(", "maybe_warnx(", text)
    text = text.replace("co_await b_fclose(stdin);", "/* stdin */")
    text = re.sub(r"__printflike\([^)]+\)\s*", "", text)
    text = re.sub(r"__dead2\s*", "", text)

    text = text.replace(
        "path_basename({file, strlen(file)})",
        "path_basename(Str(file, strlen(file))).data()",
    )

    old_dir = r"static Task<void>\nhandle_dir\(char \*dir\)\s*\{[\s\S]*?\n\}"
    new_dir = """static Task<void>
handle_dir(char *dir)
{
\tDIR *d = co_await b_opendir(dir);
\tif (!d) {
\t\tmaybe_warn("couldn't opendir %s", dir);
\t\tco_return;
\t}
\tfor (;;) {
\t\tstruct dirent *de = b_readdir(d);
\t\tif (!de)
\t\t\tbreak;
\t\tif (de->d_name[0] == '.')
\t\t\tcontinue;
\t\tchar path[PATH_MAX];
\t\tif ((size_t)snprintf(path, sizeof path, "%s/%s", dir, de->d_name) >= sizeof path)
\t\t\tcontinue;
\t\tstruct stat sb;
\t\tif (co_await b_lstat(path, &sb) != 0)
\t\t\tcontinue;
\t\tif (S_ISDIR(sb.st_mode)) {
\t\t\tif (rflag)
\t\t\t\tco_await handle_dir(path);
\t\t} else if (S_ISREG(sb.st_mode))
\t\t\tco_await handle_file(path, &sb);
\t}
\tb_closedir(d);
}"""
    text = re.sub(old_dir, new_dir, text, count=1)

    text = re.sub(
        r"static Task<void>\ncopymodes\(int fd, const struct stat \*sbp, const char \*file\)\s*\{[\s\S]*?\n\}\n\n/\* what sort",
        "static Task<void>\ncopymodes(int fd, const struct stat *sbp, const char *file)\n{\n\t(void)fd;\n\t(void)sbp;\n\t(void)file;\n\tco_return;\n}\n\n/* what sort",
        text,
        count=1,
    )

    text = re.sub(
        r"case FT_Z: \{[\s\S]*?break;\n\t\}",
        """case FT_Z: {
\t\tif (lflag) {
\t\t\tmaybe_warnx("no -l with Lempel-Ziv files");
\t\t\tgoto lose;
\t\t}
\t\tsize = co_await zuncompress_fd(fd, zfd, (char *)fourbytes,
\t\t    sizeof fourbytes, NULL);
\t\tbreak;
\t}""",
        text,
        count=1,
    )

    for fn in AWAIT_FUNCS:
        text = re.sub(rf"(?<![.\w]){fn}\s*\(", f"co_await {fn}(", text)

    text = text.replace("co_await co_await ", "co_await ")

    # Function definitions must not lead with co_await.
    text = re.sub(r"(static Task<[^>]+>\n)co_await (\w+)\(", r"\1\2(", text)
    text = re.sub(r"(^Task<[^>]+>\n)co_await (\w+)\(", r"\1\2(", text, flags=re.M)

    text = text.replace("\n\treturn;\n", "\n\tco_return;\n")
    text = text.replace("\n\treturn (-1);\n", "\n\tco_return (-1);\n")
    text = text.replace("\n\treturn -1;\n", "\n\tco_return -1;\n")
    text = re.sub(r"co_return 0;", "co_return;", text)
    text = re.sub(r"co_return 2;", "/* fatal */", text)
    text = text.replace("fclose(stdin);", "/* stdin */")
    text = text.replace("gzip_progname(args[0])", 'gzip_progname(Str(""))')

    text = re.sub(r"static Task<off_t>\nunbzip2", "Task<off_t>\nunbzip2", text, count=1)
    text = re.sub(r"static Task<off_t>\nunxz\b", "Task<off_t>\nunxz", text, count=1)
    text = re.sub(r"static Task<off_t>\nunxz_len", "Task<off_t>\nunxz_len", text, count=1)
    text = re.sub(r"static Task<off_t>\nunzstd", "Task<off_t>\nunzstd", text, count=1)
    text = re.sub(r"static Task<off_t>\nunlz", "Task<off_t>\nunlz", text, count=1)
    text = re.sub(r"static Task<off_t>\nunpack", "Task<off_t>\nunpack", text, count=1)

    text = text.replace("= malloc(", "= (char *)malloc(")

    # co_return in coroutines; keep plain return in sync helpers.
    text = re.sub(
        r"(static const suffixes_t \*\ncheck_suffix\([^{]+\{)([\s\S]*?)(\n\})",
        lambda m: m.group(1) + re.sub(r"\tco_return ", "\treturn ", m.group(2)) + m.group(3),
        text,
        count=1,
    )
    text = re.sub(
        r"(static enum filetype\nfile_gettype\([^{]+\{)([\s\S]*?)(\n\})",
        lambda m: m.group(1) + re.sub(r"\tco_return ", "\treturn ", m.group(2)) + m.group(3),
        text,
        count=1,
    )
    text = re.sub(
        r"(static bool\nlz_st_is_char\([^{]+\{)([\s\S]*?)(\n\})",
        lambda m: m.group(1) + re.sub(r"\tco_return ", "\treturn ", m.group(2)) + m.group(3),
        text,
        count=1,
    )
    for pat in (
        "\treturn (",
        "\treturn -1",
        "\treturn in_tot",
        "\treturn ok",
        "\treturn size",
        "\treturn (size)",
        "\treturn ret",
        "\treturn bytes_out",
        "\treturn (bytes_out + prelen)",
        "\treturn (-1)",
        "\treturn bout",
        "\treturn rv",
        "\treturn (bytes_out)",
        "\treturn (out_tot)",
        "\treturn (in_tot)",
        "\treturn (gcode)",
        "\treturn (unpackd",
        "\treturn sz - left",
        "\treturn;",
        "\treturn true",
        "\treturn false",
        "\treturn res",
        "\treturn 0",
    ):
        text = text.replace(pat, pat.replace("return", "co_return", 1))
    text = re.sub(r"(?<![\w])return \(-1\);", "co_return (-1);", text)
    text = text.replace("co_co_return", "co_return")
    text = re.sub(
        r"(static Task<bool>\nparse_indexes\([^{]+\{)([\s\S]*?)(\n\})",
        lambda m: m.group(1)
        + re.sub(r"\treturn (true|false);", lambda r: "\tco_return " + r.group(1) + ";", m.group(2))
        + m.group(3),
        text,
        count=1,
    )
    text = re.sub(
        r"(static Task<bool>\nio_pread\([^{]+\{)([\s\S]*?)(\n\})",
        lambda m: m.group(1)
        + re.sub(r"\treturn (true|false);", lambda r: "\tco_return " + r.group(1) + ";", m.group(2))
        + m.group(3),
        text,
        count=1,
    )
    text = re.sub(
        r"\} state = GZSTATE_MAGIC0;",
        "};\n\tint state = GZSTATE_MAGIC0;",
        text,
        count=1,
    )
    text = re.sub(r"\bstate\+\+;", "state = (int)state + 1;", text)

    text = re.sub(
        r"static Task<void>\ngot_sigint[\s\S]*?static Task<void>\ninfile_clear\(void\)\s*\{[\s\S]*?\n\}\n\n",
        "",
        text,
        count=1,
    )
    text = text.replace("co_return NULL;", "return NULL;")
    text = re.sub(
        r"(static Task<void>\nunlink_input\([^{]+\{[\s\S]*?)\n\treturn;\n",
        r"\1\n\tco_return;\n",
        text,
        count=1,
    )
    text = re.sub(
        r"(static Task<off_t>\nfile_compress\([^{]+\{[\s\S]*?)\n\treturn \(",
        r"\1\n\tco_return (",
        text,
        count=1,
    )
    text = re.sub(
        r"(static Task<off_t>\nfile_compress\([^{]+\{[\s\S]*?)\n\treturn -1;",
        r"\1\n\tco_return -1;",
        text,
    )

    text = text.replace("#include <zstd.h>\n\n", "#include <zstd.h>\n" + FORWARD_DECLS + "\n", 1)

    text = re.sub(r"static Task<void>\nhandle_stdin", "Task<void>\nhandle_stdin", text, count=1)
    text = re.sub(r"static Task<void>\nhandle_stdout", "Task<void>\nhandle_stdout", text, count=1)
    text = re.sub(r"static Task<void>\nhandle_pathname", "Task<void>\nhandle_pathname", text, count=1)

    for pat in (
        r"static Task<ssize_t>\s+read_retry\s*\(",
        r"static Task<ssize_t>\s+write_retry\s*\(",
        r"static ssize_t\s+read_retry\s*\(",
        r"static ssize_t\s+write_retry\s*\(",
    ):
        text = drop_braced_function(text, pat)

    BODY.write_text(text)
    print("fixed", BODY)


if __name__ == "__main__":
    main()
