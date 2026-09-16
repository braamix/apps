--- unittest ---
error PEP3131Test.test_invalid: ModuleNotFoundError: No module named 'test.tokenizedata'
fail PEP3131Test.test_non_bmp_normalized: AssertionError: 'Unicode' not found in ['self', '𝔘𝔫𝔦𝔠𝔬𝔡𝔢']
error PEP3131Test.test_valid: AttributeError: 'type' object has no attribute 'μ'
--- ran 3 ok 0 fail 1 error 2 skip 0 ---
