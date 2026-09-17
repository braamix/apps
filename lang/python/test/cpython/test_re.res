--- unittest ---
skip DebugTests.test_atomic_group: implementation detail of CPython
skip DebugTests.test_debug_charset_bitmap: implementation detail of CPython
skip DebugTests.test_debug_flag: implementation detail of CPython
skip DebugTests.test_possesive_repeat: implementation detail of CPython
skip DebugTests.test_possesive_repeat_one: implementation detail of CPython
ok ExternalTests.test_re_benchmarks
ok ExternalTests.test_re_tests
skip ImplementationTest.test_case_helpers: implementation detail of CPython
skip ImplementationTest.test_dealloc: implementation detail of CPython
skip ImplementationTest.test_disallow_instantiation: implementation detail of CPython
skip ImplementationTest.test_immutable: implementation detail of CPython
ok ImplementationTest.test_overlap_table
skip ImplementationTest.test_repeat_minmax_overflow_maxrepeat: implementation detail of CPython
ok ImplementationTest.test_signedness
skip ImplementationTest.test_sre_template_invalid_group_index: implementation detail of CPython
ok PatternReprTests.test_bytes
ok PatternReprTests.test_flags_repr
ok PatternReprTests.test_inline_flags
ok PatternReprTests.test_locale
ok PatternReprTests.test_long_pattern
ok PatternReprTests.test_multiple_flags
ok PatternReprTests.test_quotes
ok PatternReprTests.test_single_flag
ok PatternReprTests.test_unicode_flag
ok PatternReprTests.test_unknown_flags
ok PatternReprTests.test_without_flags
ok ReTests.test_ASSERT_NOT_mark_bug
ok ReTests.test_MARK_PUSH_macro_bug
ok ReTests.test_MIN_REPEAT_ONE_mark_bug
ok ReTests.test_MIN_UNTIL_mark_bug
ok ReTests.test_REPEAT_ONE_mark_bug
ok ReTests.test_anyall
ok ReTests.test_ascii_and_unicode_flag
ok ReTests.test_atomic_grouping
ok ReTests.test_backref_group_name_in_exception
ok ReTests.test_basic_re_sub
ok ReTests.test_big_codesize
ok ReTests.test_bigcharset
ok ReTests.test_branching
ok ReTests.test_bug_113254
ok ReTests.test_bug_114660
ok ReTests.test_bug_117612
ok ReTests.test_bug_1661
ok ReTests.test_bug_16688
ok ReTests.test_bug_20998
ok ReTests.test_bug_2537
ok ReTests.test_bug_29444
ok ReTests.test_bug_34294
ok ReTests.test_bug_3629
ok ReTests.test_bug_40736
ok ReTests.test_bug_418626
ok ReTests.test_bug_448951
ok ReTests.test_bug_449000
ok ReTests.test_bug_449964
ok ReTests.test_bug_527371
ok ReTests.test_bug_581080
ok ReTests.test_bug_612074
ok ReTests.test_bug_6509
ok ReTests.test_bug_6561
ok ReTests.test_bug_725106
ok ReTests.test_bug_725149
ok ReTests.test_bug_764548
ok ReTests.test_bug_817234
ok ReTests.test_bug_926075
ok ReTests.test_bug_931848
ok ReTests.test_bug_gh100061
ok ReTests.test_bug_gh101955
ok ReTests.test_bug_gh140797
ok ReTests.test_bug_gh91616
ok ReTests.test_bytes_str_mixing
ok ReTests.test_category
ok ReTests.test_character_set_any
ok ReTests.test_character_set_errors
ok ReTests.test_character_set_none
ok ReTests.test_comments
ok ReTests.test_compile
ok ReTests.test_constants
ok ReTests.test_copying
ok ReTests.test_dollar_matches_twice
ok ReTests.test_empty_array
ok ReTests.test_enum
ok ReTests.test_error
ok ReTests.test_error_is_PatternError_alias
ok ReTests.test_expand
ok ReTests.test_fail
ok ReTests.test_findall_atomic_grouping
ok ReTests.test_findall_possessive_quantifiers
ok ReTests.test_finditer
ok ReTests.test_flags
ok ReTests.test_fullmatch_atomic_grouping
ok ReTests.test_fullmatch_possessive_quantifiers
ok ReTests.test_getattr
ok ReTests.test_group
ok ReTests.test_group_name_in_exception
ok ReTests.test_groupdict
ok ReTests.test_ignore_case
ok ReTests.test_ignore_case_range
ok ReTests.test_ignore_case_set
ok ReTests.test_ignore_spaces
ok ReTests.test_inline_flags
ok ReTests.test_issue17998
fail ReTests.test_keep_buffer: AssertionError: BufferError not raised
ok ReTests.test_keyword_parameters
skip ReTests.test_large_search: not enough memory for a bigmem test
skip ReTests.test_large_subn: not enough memory for a bigmem test
skip ReTests.test_locale_caching: test needs en_US.iso88591 locale
skip ReTests.test_locale_compiled: test needs en_US.iso88591 locale
ok ReTests.test_locale_flag
ok ReTests.test_locale_ignorecase_negated_set
ok ReTests.test_look_behind_overflow
ok ReTests.test_lookahead
ok ReTests.test_lookbehind
ok ReTests.test_match_getitem
ok ReTests.test_match_repr
skip ReTests.test_memory_leaks: requires debug build
ok ReTests.test_misc_errors
ok ReTests.test_misuse_flags
ok ReTests.test_multiple_repeat
ok ReTests.test_named_unicode_escapes
ok ReTests.test_not_literal
ok ReTests.test_nothing_to_repeat
ok ReTests.test_other_escapes
ok ReTests.test_pattern_compare
ok ReTests.test_pattern_compare_bytes
error ReTests.test_pickling: ModuleNotFoundError: No module named 'pickle'
ok ReTests.test_possessive_quantifiers
ok ReTests.test_prefixmatch_getitem
ok ReTests.test_property_escapes
ok ReTests.test_qualified_re_split
ok ReTests.test_qualified_re_sub
ok ReTests.test_re_escape
ok ReTests.test_re_escape_bytes
ok ReTests.test_re_escape_non_ascii
ok ReTests.test_re_escape_non_ascii_bytes
ok ReTests.test_re_findall
ok ReTests.test_re_fullmatch
ok ReTests.test_re_groupref
ok ReTests.test_re_groupref_exists
ok ReTests.test_re_groupref_exists_errors
ok ReTests.test_re_groupref_exists_validation_bug
ok ReTests.test_re_groupref_overflow
ok ReTests.test_re_match
ok ReTests.test_re_split
ok ReTests.test_re_subn
skip ReTests.test_regression_gh94675: test requires multiprocessing
ok ReTests.test_repeat_minmax
ok ReTests.test_repeat_minmax_overflow
ok ReTests.test_scanner
ok ReTests.test_scoped_flags
skip ReTests.test_search_anchor_at_beginning: resource 'cpu' is not enabled
ok ReTests.test_search_coverage
ok ReTests.test_search_dot_unicode
ok ReTests.test_search_star_plus
ok ReTests.test_set_operations
ok ReTests.test_special_escapes
ok ReTests.test_sre_byte_class_literals
ok ReTests.test_sre_byte_literals
ok ReTests.test_sre_character_class_literals
ok ReTests.test_sre_character_literals
ok ReTests.test_stack_overflow
ok ReTests.test_sub_template_numeric_escape
ok ReTests.test_symbolic_groups
ok ReTests.test_symbolic_groups_errors
ok ReTests.test_symbolic_refs
ok ReTests.test_symbolic_refs_errors
ok ReTests.test_unlimited_zero_width_repeat
ok ReTests.test_weakref
ok ReTests.test_word_boundaries
ok ReTests.test_zerowidth
ok TestModule.test_deprecated__version__
--- ran 170 ok 150 fail 1 error 1 skip 18 ---
