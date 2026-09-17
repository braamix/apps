--- unittest ---
ok TestPrint.test_gh130163
ok TestPrint.test_print
ok TestPrint.test_print_flush
fail TestPy2MigrationHint.test_normal_string: AssertionError: "Missing parentheses in call to 'print'. Did you mean print(...)" not found in 'invalid syntax (<string>, line 1)'
fail TestPy2MigrationHint.test_string_in_loop_on_same_line: AssertionError: "Missing parentheses in call to 'print'. Did you mean print(...)" not found in 'invalid syntax (<string>, line 1)'
fail TestPy2MigrationHint.test_string_with_excessive_whitespace: AssertionError: "Missing parentheses in call to 'print'. Did you mean print(...)" not found in 'invalid syntax (<string>, line 1)'
fail TestPy2MigrationHint.test_string_with_leading_whitespace: AssertionError: "Missing parentheses in call to 'print'. Did you mean print(...)" not found in 'invalid syntax (<string>, line 2)'
fail TestPy2MigrationHint.test_string_with_semicolon: AssertionError: "Missing parentheses in call to 'print'. Did you mean print(...)" not found in 'invalid syntax (<string>, line 1)'
fail TestPy2MigrationHint.test_string_with_soft_space: AssertionError: "Missing parentheses in call to 'print'. Did you mean print(...)" not found in 'invalid syntax (<string>, line 1)'
--- ran 9 ok 3 fail 6 error 0 skip 0 ---
