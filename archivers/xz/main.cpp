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

Task<i32> xz_main(Args args)
{
    io_init();
    message_init();
    hardware_init();

    int argc    = (int)args.size();
    char **argv = (char **)xmalloc((size_t)(argc + 1) * sizeof(char *));
    for (int i = 0; i < argc; ++i) {
        Str s   = args[(usize)i];
        argv[i] = (char *)xmalloc(s.size() + 1);
        memcpy(argv[i], s.data(), s.size());
        argv[i][s.size()] = '\0';
    }
    argv[argc] = NULL;
    tuklib_progname_init(argv);

    args_info ainfo;
    args_parse(&ainfo, argc, argv);
    if (xz_stop) {
        co_await xz_flush_diag();
        for (int i = 0; i < argc; ++i)
            free(argv[i]);
        free(argv);
        if (exit_status == E_ERROR)
            co_return 1;
        if (exit_status == E_WARNING)
            co_return 2;
        co_return 0;
    }
    if (xz_fatal) {
        for (int i = 0; i < argc; ++i)
            free(argv[i]);
        free(argv);
        co_return 1;
    }

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
    else
        message_set_files(ainfo.arg_count);

    if (opt_mode == MODE_COMPRESS) {
        if (opt_stdout || (ainfo.arg_count == 1 && strcmp(ainfo.arg_names[0], "-") == 0)) {
            if (is_tty_stdout())
                message_try_help();
        }
    }

    for (unsigned i = 0; i < ainfo.arg_count && !user_abort; ++i) {
        if (strcmp("-", ainfo.arg_names[i]) == 0) {
            if (opt_mode == MODE_COMPRESS) {
                if (is_tty_stdout())
                    continue;
            } else if (is_tty_stdin()) {
                continue;
            }
            if (ainfo.files_name == stdin_filename) {
                message_error(
                    _("Cannot read data from standard input when "
                      "reading filenames from standard input"));
                continue;
            }
            ainfo.arg_names[i] = (char *)stdin_filename;
        }

        if (opt_mode == MODE_LIST)
            co_await list_file(ainfo.arg_names[i]);
        else
            co_await coder_run(ainfo.arg_names[i]);
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

    for (int i = 0; i < argc; ++i)
        free(argv[i]);
    free(argv);

    if (es == E_ERROR)
        co_return 1;
    if (es == E_WARNING)
        co_return 2;
    co_return 0;
}
