--- unittest ---
ok GlobTests.test_escape
skip GlobTests.test_escape_windows: Win32 specific test
ok GlobTests.test_glob_broken_symlinks
ok GlobTests.test_glob_bytes_directory_with_trailing_slash
ok GlobTests.test_glob_directory_names
ok GlobTests.test_glob_directory_with_trailing_slash
ok GlobTests.test_glob_empty_pattern
ok GlobTests.test_glob_literal
skip GlobTests.test_glob_magic_in_drive: Win32 specific test
ok GlobTests.test_glob_many_open_files
skip GlobTests.test_glob_named_pipe: requires os.mkfifo()
ok GlobTests.test_glob_nested_directory
ok GlobTests.test_glob_non_directory
ok GlobTests.test_glob_one_directory
ok GlobTests.test_glob_symlinks
ok GlobTests.test_hidden_glob
ok GlobTests.test_recursive_glob
ok GlobTests.test_translate
ok GlobTests.test_translate_include_hidden
ok GlobTests.test_translate_matching
ok GlobTests.test_translate_recursive
ok GlobTests.test_translate_seps
--- ran 22 ok 19 fail 0 error 0 skip 3 ---
