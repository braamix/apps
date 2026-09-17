--- unittest ---
ok CAutoFileTests.testAttributes
ok CAutoFileTests.testBlksize
ok CAutoFileTests.testErrnoOnClose
ok CAutoFileTests.testErrnoOnClosedFileno
ok CAutoFileTests.testErrnoOnClosedIsatty
ok CAutoFileTests.testErrnoOnClosedRead
ok CAutoFileTests.testErrnoOnClosedReadable
ok CAutoFileTests.testErrnoOnClosedReadall
ok CAutoFileTests.testErrnoOnClosedReadinto
ok CAutoFileTests.testErrnoOnClosedSeek
ok CAutoFileTests.testErrnoOnClosedSeekable
ok CAutoFileTests.testErrnoOnClosedTell
ok CAutoFileTests.testErrnoOnClosedTruncate
ok CAutoFileTests.testErrnoOnClosedWritable
ok CAutoFileTests.testErrnoOnClosedWrite
ok CAutoFileTests.testErrors
ok CAutoFileTests.testFinalizing
ok CAutoFileTests.testMethods
error CAutoFileTests.testOpenDirFD: IsADirectoryError: [Errno 21] Is a directory: '.'
ok CAutoFileTests.testOpendir
ok CAutoFileTests.testReadintoByteArray
ok CAutoFileTests.testRecursiveRepr
ok CAutoFileTests.testRepr
error CAutoFileTests.testReprNoCloseFD: PermissionError: [Errno 13] Permission denied: '@test_28_tmpæ'
ok CAutoFileTests.testSeekTell
ok CAutoFileTests.testWeakRefs
ok CAutoFileTests.testWritelinesError
ok CAutoFileTests.testWritelinesList
ok CAutoFileTests.testWritelinesUserList
ok CAutoFileTests.test_none_args
ok CAutoFileTests.test_reject
error CAutoFileTests.test_subclass_repr: PermissionError: [Errno 13] Permission denied: '@test_28_tmpæ'
skip CAutoFileTests.test_syscalls_read: strace not found
ok COtherFileTests.testAbles
ok COtherFileTests.testAppend
ok COtherFileTests.testBadModeArgument
ok COtherFileTests.testBooleanFd
ok COtherFileTests.testBytesOpen
ok COtherFileTests.testConstructorHandlesNULChars
ok COtherFileTests.testInvalidFd
skip COtherFileTests.testInvalidFd_overflow: implementation detail of CPython
ok COtherFileTests.testInvalidInit
ok COtherFileTests.testInvalidModeStrings
ok COtherFileTests.testModeStrings
ok COtherFileTests.testTruncate
ok COtherFileTests.testTruncateOnWindows
fail COtherFileTests.testUnclosedFDOnException: AssertionError: MyException not raised
ok COtherFileTests.testUnicodeOpen
ok COtherFileTests.testUtf8BytesOpen
ok COtherFileTests.testWarnings
ok COtherFileTests.test_open_code
ok PyAutoFileTests.testAttributes
ok PyAutoFileTests.testBlksize
ok PyAutoFileTests.testErrnoOnClose
ok PyAutoFileTests.testErrnoOnClosedFileno
ok PyAutoFileTests.testErrnoOnClosedIsatty
ok PyAutoFileTests.testErrnoOnClosedRead
ok PyAutoFileTests.testErrnoOnClosedReadable
ok PyAutoFileTests.testErrnoOnClosedReadall
ok PyAutoFileTests.testErrnoOnClosedReadinto
ok PyAutoFileTests.testErrnoOnClosedSeek
ok PyAutoFileTests.testErrnoOnClosedSeekable
ok PyAutoFileTests.testErrnoOnClosedTell
ok PyAutoFileTests.testErrnoOnClosedTruncate
ok PyAutoFileTests.testErrnoOnClosedWritable
ok PyAutoFileTests.testErrnoOnClosedWrite
ok PyAutoFileTests.testErrors
ok PyAutoFileTests.testMethods
error PyAutoFileTests.testOpenDirFD: IsADirectoryError: [Errno 21] Is a directory: '.'
ok PyAutoFileTests.testOpendir
ok PyAutoFileTests.testReadintoByteArray
ok PyAutoFileTests.testRecursiveRepr
ok PyAutoFileTests.testRepr
error PyAutoFileTests.testReprNoCloseFD: PermissionError: [Errno 13] Permission denied: '@test_28_tmpæ'
ok PyAutoFileTests.testSeekTell
ok PyAutoFileTests.testWeakRefs
ok PyAutoFileTests.testWritelinesError
ok PyAutoFileTests.testWritelinesList
ok PyAutoFileTests.testWritelinesUserList
ok PyAutoFileTests.test_none_args
ok PyAutoFileTests.test_reject
error PyAutoFileTests.test_subclass_repr: PermissionError: [Errno 13] Permission denied: '@test_28_tmpæ'
skip PyAutoFileTests.test_syscalls_read: strace not found
ok PyOtherFileTests.testAbles
ok PyOtherFileTests.testAppend
ok PyOtherFileTests.testBadModeArgument
ok PyOtherFileTests.testBooleanFd
ok PyOtherFileTests.testBytesOpen
ok PyOtherFileTests.testConstructorHandlesNULChars
ok PyOtherFileTests.testInvalidFd
ok PyOtherFileTests.testInvalidInit
ok PyOtherFileTests.testInvalidModeStrings
ok PyOtherFileTests.testModeStrings
ok PyOtherFileTests.testTruncate
ok PyOtherFileTests.testTruncateOnWindows
ok PyOtherFileTests.testUnclosedFDOnException
ok PyOtherFileTests.testUnicodeOpen
ok PyOtherFileTests.testUtf8BytesOpen
ok PyOtherFileTests.testWarnings
ok PyOtherFileTests.test_open_code
--- ran 100 ok 90 fail 1 error 6 skip 3 ---
