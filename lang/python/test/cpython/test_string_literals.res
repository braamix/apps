--- unittest ---
ok TestLiterals.test_eval_bytes_incomplete
fail TestLiterals.test_eval_bytes_invalid_escape: AssertionError: SyntaxWarning not triggered
fail TestLiterals.test_eval_bytes_invalid_octal_escape: AssertionError: SyntaxWarning not triggered
ok TestLiterals.test_eval_bytes_normal
fail TestLiterals.test_eval_bytes_raw: AssertionError: SyntaxError not raised
ok TestLiterals.test_eval_str_incomplete
fail TestLiterals.test_eval_str_invalid_escape: AssertionError: SyntaxWarning not triggered
fail TestLiterals.test_eval_str_invalid_octal_escape: AssertionError: SyntaxWarning not triggered
ok TestLiterals.test_eval_str_normal
ok TestLiterals.test_eval_str_raw
fail TestLiterals.test_eval_str_u: AssertionError: SyntaxError not raised
ok TestLiterals.test_file_iso_8859_1
ok TestLiterals.test_file_latin9
ok TestLiterals.test_file_latin_1
ok TestLiterals.test_file_utf8
ok TestLiterals.test_file_utf_8
ok TestLiterals.test_file_utf_8_error
fail TestLiterals.test_invalid_escape_locations_with_offset: AssertionError: 0 != 1
ok TestLiterals.test_template
ok TestLiterals.test_uppercase_prefixes
--- ran 20 ok 13 fail 7 error 0 skip 0 ---
