// SPDX-License-Identifier: 0BSD
#include "kernel/args.h"
#include "private.h"

enum exit_status_type exit_status = E_SUCCESS;

static bool no_warn = false;

extern void set_exit_status(enum exit_status_type new_status)
{
    if (exit_status != E_ERROR)
        exit_status = new_status;
}

extern void set_exit_no_warn(void)
{
    no_warn = true;
}

static void copy_path(char *dst, usize cap, Str s)
{
    if (cap == 0)
        return;
    usize n = s.size();
    if (n >= cap)
        n = cap - 1;
    memcpy(dst, s.data(), n);
    dst[n] = '\0';
}

static Task<const char *> read_name(args_info *args)
{
    static char *name  = NULL;
    static size_t size = 256;

    if (name == NULL)
        name = (char *)xmalloc(size);

    size_t pos = 0;
    while (!user_abort) {
        int c = co_await b_fgetc(args->files_file);
        if (c == EOF) {
            if (b_ferror(args->files_file)) {
                message_error(_("%s: Error reading filenames: %s"),
                              tuklib_mask_nonprint(args->files_name), strerror(errno));
                break;
            }
            break;
        }
        if (c == (unsigned char)args->files_delim) {
            if (pos == 0)
                continue;
            name[pos] = '\0';
            co_return name;
        }
        name[pos++] = (char)c;
        if (pos == size) {
            size *= 2;
            name = (char *)xrealloc(name, size);
        }
    }
    co_return NULL;
}

static Task<void> run_file(Str name, args_info *ainfo)
{
    if (name == Str("-")) {
        if (opt_mode == MODE_COMPRESS) {
            if (is_tty_stdout())
                co_return;
        } else if (is_tty_stdin()) {
            co_return;
        }
        if (ainfo->files_name == stdin_filename) {
            message_error(
                _("Cannot read data from standard input when "
                  "reading filenames from standard input"));
            co_return;
        }
        name = Str(stdin_filename);
    }

    char path[512];
    copy_path(path, sizeof path, name);
    if (path[0] == '\0' && name != Str(stdin_filename)) {
        message_error(_("Empty filename, skipping"));
        co_return;
    }

    if (opt_mode == MODE_LIST)
        co_await list_file(path);
    else
        co_await coder_run(path);
}

Task<i32> xz_main(Args args)
{
    io_init();
    message_init();
    hardware_init();

    args_info ainfo;
    args_parse(&ainfo, args);
    if (xz_stop) {
        co_await xz_flush_diag();
        if (exit_status == E_ERROR)
            co_return 1;
        if (exit_status == E_WARNING)
            co_return 2;
        co_return 0;
    }
    if (xz_fatal)
        co_return 1;

    if (ainfo.files_name != NULL && ainfo.files_name != stdin_filename &&
        ainfo.files_file == NULL) {
        const char *mode = ainfo.files_delim == '\n' ? "r" : "rb";
        ainfo.files_file = co_await b_fopen(ainfo.files_name, mode);
        if (ainfo.files_file == NULL)
            message_fatal(_("%s: %s"), tuklib_mask_nonprint(ainfo.files_name), strerror(errno));
    }

    if (opt_mode != MODE_LIST && opt_robot)
        message_fatal(
            _("Compression and decompression with --robot "
              "are not supported yet."));

    if (ainfo.files_name != NULL)
        message_set_files(0);
    else if (ainfo.stdin_only)
        message_set_files(1);
    else
        message_set_files(ainfo.file_count);

    if (opt_mode == MODE_COMPRESS) {
        const bool stdin_path =
            ainfo.stdin_only ||
            (ainfo.file_count == 1 && ainfo.args[ainfo.file_index] == Str("-"));
        if (opt_stdout || stdin_path) {
            if (is_tty_stdout())
                message_try_help();
        }
    }

    if (ainfo.stdin_only) {
        if (!user_abort)
            co_await run_file(Str("-"), &ainfo);
    } else {
        for (unsigned i = 0; i < ainfo.file_count && !user_abort; ++i)
            co_await run_file(ainfo.args[ainfo.file_index + i], &ainfo);
    }

    if (ainfo.files_name != NULL) {
        while (true) {
            const char *name = co_await read_name(&ainfo);
            if (name == NULL)
                break;
            if (opt_mode == MODE_LIST)
                co_await list_file(name);
            else
                co_await coder_run(name);
        }
        if (ainfo.files_name != stdin_filename) {
            co_await b_fclose(ainfo.files_file);
            free(ainfo.files_name);
        }
    }

#ifdef HAVE_DECODERS
    if (opt_mode == MODE_LIST)
        co_await list_totals();
#endif

    enum exit_status_type es = exit_status;
    if (es == E_WARNING && no_warn)
        es = E_SUCCESS;

    if (es == E_ERROR)
        co_return 1;
    if (es == E_WARNING)
        co_return 2;
    co_return 0;
}
