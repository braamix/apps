--- unittest ---
ok Assertions.test_almost
ok Assertions.test_equality
ok Assertions.test_identity
ok Assertions.test_membership
ok Assertions.test_ordering
ok Assertions.test_sequences
ok Assertions.test_truth
ok Assertions.test_types
ok Fixtures.test_a
ok Fixtures.test_b
ok Inherited.test_almost
ok Inherited.test_equality
ok Inherited.test_identity
ok Inherited.test_membership
ok Inherited.test_ordering
ok Inherited.test_own
ok Inherited.test_sequences
ok Inherited.test_truth
ok Inherited.test_types
fail Outcomes.test_subtest_failure_is_reported: 1 subtests: i=2: AssertionError: 2 != 1
error Outcomes.test_this_one_errors: KeyError: 'not an assertion'
fail Outcomes.test_this_one_fails: AssertionError: 1 != 2
ok Outcomes.test_this_one_is_expected_to_fail (expected failure)
ok Outcomes.test_this_one_is_not_skipped
skip Outcomes.test_this_one_is_skipped_by_decorator: skipIf said so
skip Outcomes.test_this_one_skips: deliberate
ok Raising.test_call_form
ok Raising.test_call_form_lets_the_wrong_one_through
ok Raising.test_call_form_that_raises_nothing_is_a_failure
ok Raising.test_context_manager
ok Raising.test_exception_is_kept
ok Raising.test_not_raised_is_a_failure
ok Raising.test_regex_anchored
ok Raising.test_regex_escape
ok Raising.test_regex_is_re
ok Raising.test_regex_plain
ok Raising.test_regex_that_does_not_match
ok Raising.test_tuple_of_types
ok Raising.test_wrong_type_passes_through
skip SkippedWhole.test_one: the whole class
skip SkippedWhole.test_two: the whole class
ok SubTests.test_all_good
ok SubTests.test_skip_inside_is_not_a_failure
skip Support.test_docstrings_are_skipped: test requires docstrings
ok Support.test_get_attribute
ok Support.test_never_and_always_equal
skip Support.test_refcounts_are_skipped: the collector does not count references
ok Wrapped.test_the_expected_failure_really_ran
ok Wrapped.test_the_unskipped_one_really_ran
--- ran 49 ok 40 fail 2 error 1 skip 6 ---
