--- unittest ---
ok FMATests.test_fma_infinities
ok FMATests.test_fma_nan_results
ok FMATests.test_fma_overflow
ok FMATests.test_fma_random
ok FMATests.test_fma_single_round
fail FMATests.test_fma_zero_result: AssertionError: False is not true : Expected a negative zero, got 0.0
ok IsCloseTests.test_asymmetry
ok IsCloseTests.test_decimals
ok IsCloseTests.test_eight_decimal_places
ok IsCloseTests.test_fractions
ok IsCloseTests.test_identical
ok IsCloseTests.test_identical_infinite
ok IsCloseTests.test_inf_ninf_nan
ok IsCloseTests.test_integers
ok IsCloseTests.test_near_zero
ok IsCloseTests.test_negative_tolerances
ok IsCloseTests.test_zero_tolerance
ok MathTests.testAcos
ok MathTests.testAcosh
ok MathTests.testAsin
ok MathTests.testAsinh
ok MathTests.testAtan
ok MathTests.testAtan2
ok MathTests.testAtanh
ok MathTests.testCbrt
error MathTests.testCeil: TypeError: 'BadDescr' object is not callable
ok MathTests.testConstants
ok MathTests.testCopysign
ok MathTests.testCos
ok MathTests.testCosh
ok MathTests.testDegrees
error MathTests.testDist: TypeError: must be real number, not FloatLike
ok MathTests.testExp
ok MathTests.testExp2
ok MathTests.testFabs
error MathTests.testFloor: TypeError: 'BadDescr' object is not callable
ok MathTests.testFmod
ok MathTests.testFrexp
error MathTests.testFsum: TypeError: must be real number, not FloatLike
ok MathTests.testGcd
ok MathTests.testHypot
ok MathTests.testHypotAccuracy
ok MathTests.testIsfinite
ok MathTests.testIsinf
ok MathTests.testIsnan
ok MathTests.testIsnormal
ok MathTests.testIssubnormal
ok MathTests.testLdexp
ok MathTests.testLdexp_denormal
error MathTests.testLog: OverflowError
error MathTests.testLog10: OverflowError
ok MathTests.testLog1p
error MathTests.testLog2: OverflowError
ok MathTests.testLog2Exact
ok MathTests.testModf
ok MathTests.testPow
ok MathTests.testRadians
error MathTests.testRemainder: ValueError: invalid hexadecimal floating-point string
ok MathTests.testSin
ok MathTests.testSinh
ok MathTests.testSqrt
error MathTests.testSumProd: ModuleNotFoundError: No module named 'test.test_iter'
ok MathTests.testTan
ok MathTests.testTanh
ok MathTests.testTanhSign
ok MathTests.test_exception_messages
ok MathTests.test_exceptions
ok MathTests.test_fmax
ok MathTests.test_fmax_nans
ok MathTests.test_fmin
ok MathTests.test_fmin_nans
ok MathTests.test_inf_constant
ok MathTests.test_input_exceptions
error MathTests.test_issue39871: ZeroDivisionError: division by zero
fail MathTests.test_lcm: AssertionError: TypeError not raised
skip MathTests.test_log_huge_integer: not enough memory for a bigmem test
ok MathTests.test_math_dist_leak
error MathTests.test_mtestfile: FileNotFoundError: [Errno 2] No such file or directory: '/tmp/mathdata/math_testcases.txt'
ok MathTests.test_nan_constant
ok MathTests.test_nextafter
ok MathTests.test_prod
ok MathTests.test_signbit
skip MathTests.test_sumprod_accuracy: implementation detail of CPython
skip MathTests.test_sumprod_extended_precision_accuracy: implementation detail of CPython
skip MathTests.test_sumprod_stress: resource 'cpu' is not enabled
error MathTests.test_testfile: FileNotFoundError: [Errno 2] No such file or directory: '/tmp/mathdata/cmath_testcases.txt'
fail MathTests.test_trunc: AssertionError: TypeError not raised
ok MathTests.test_ulp
--- ran 88 ok 69 fail 3 error 12 skip 4 ---
