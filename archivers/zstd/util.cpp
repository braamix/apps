/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/*-****************************************
 *  Dependencies
 ******************************************/
#include "util.h" /* note : ensure that platform.h is included first ! */

/*-****************************************
 *  Internal Macros
 ******************************************/

/* CONTROL is almost like an assert(), but is never disabled.
 * It's designed for failures that may happen rarely,
 * but we don't want to maintain a specific error code path for them,
 * such as a malloc() returning NULL for example.
 * Braam: it recorded the status and exit()ed; here it records and the caller
 * returns, since there is no exit() to take.
 */
#define CONTROL(c)                                                               \
    {                                                                            \
        if (!(c)) {                                                              \
            UTIL_DISPLAYLEVEL(1, "Error : %s, %i : %s", __FILE__, __LINE__, #c); \
            zstd_fail(1);                                                        \
        }                                                                        \
    }

/* console log */
#define UTIL_DISPLAY(...) zstd_display(stderr, __VA_ARGS__)
#define UTIL_DISPLAYLEVEL(l, ...)      \
    {                                  \
        if (g_utilDisplayLevel >= l) { \
            UTIL_DISPLAY(__VA_ARGS__); \
        }                              \
    }

static int g_traceDepth = 0;
int g_traceFileStat     = 0;

#define UTIL_TRACE_CALL(...)                                         \
    {                                                                \
        if (g_traceFileStat) {                                       \
            UTIL_DISPLAY("Trace:FileStat: %*s> ", g_traceDepth, ""); \
            UTIL_DISPLAY(__VA_ARGS__);                               \
            UTIL_DISPLAY("\n");                                      \
            ++g_traceDepth;                                          \
        }                                                            \
    }

#define UTIL_TRACE_RET(ret)                                                     \
    {                                                                           \
        if (g_traceFileStat) {                                                  \
            --g_traceDepth;                                                     \
            UTIL_DISPLAY("Trace:FileStat: %*s< %d\n", g_traceDepth, "", (ret)); \
        }                                                                       \
    }

/* A modified version of realloc().
 * If UTIL_realloc() fails the original block is freed.
 */
UTIL_STATIC void *UTIL_realloc(void *ptr, size_t size)
{
    void *newptr = realloc(ptr, size);
    if (newptr)
        return newptr;
    free(ptr);
    return NULL;
}

/*-****************************************
 *  Console log
 ******************************************/
int g_utilDisplayLevel;

Task<int> UTIL_requireUserConfirmation(const char *prompt, const char *abortMsg,
                                       const char *acceptableLetters, int hasStdinInput)
{
    int ch, result;

    if (hasStdinInput) {
        UTIL_DISPLAY("stdin is an input - not proceeding.\n");
        co_return 1;
    }

    UTIL_DISPLAY("%s", prompt);
    co_await zstd_flush();
    ch     = co_await b_fgetc(stdin);
    result = 0;
    if (ch == EOF || strchr(acceptableLetters, ch) == NULL) {
        UTIL_DISPLAY("%s \n", abortMsg);
        result = 1;
    }
    /* flush the rest */
    while ((ch != EOF) && (ch != '\n'))
        ch = co_await b_fgetc(stdin);
    co_return result;
}

/*-*************************************
 *  Constants
 ***************************************/
#define KB                          *(1 << 10)
#define LIST_SIZE_INCREASE          (8 KB)
#define MAX_FILE_OF_FILE_NAMES_SIZE (1 << 20) * 50

/*-*************************************
 *  Functions
 ***************************************/

void UTIL_traceFileStat(void)
{
    g_traceFileStat = 1;
}

Task<int> UTIL_fstat(const int fd, const char *filename, stat_t *statbuf)
{
    int ret;
    UTIL_TRACE_CALL("UTIL_stat(%d, %s)", fd, filename);
    if (fd >= 0) {
        ret = !co_await b_fstat(fd, statbuf);
    } else {
        ret = !co_await b_stat(filename, statbuf);
    }
    UTIL_TRACE_RET(ret);
    co_return ret;
}

Task<int> UTIL_stat(const char *filename, stat_t *statbuf)
{
    co_return co_await UTIL_fstat(-1, filename, statbuf);
}

Task<int> UTIL_isFdRegularFile(int fd)
{
    stat_t statbuf;
    int ret;
    UTIL_TRACE_CALL("UTIL_isFdRegularFile(%d)", fd);
    ret = fd >= 0 && co_await UTIL_fstat(fd, "", &statbuf) && UTIL_isRegularFileStat(&statbuf);
    UTIL_TRACE_RET(ret);
    co_return ret;
}

Task<int> UTIL_isRegularFile(const char *infilename)
{
    stat_t statbuf;
    int ret;
    UTIL_TRACE_CALL("UTIL_isRegularFile(%s)", infilename);
    ret = co_await UTIL_stat(infilename, &statbuf) && UTIL_isRegularFileStat(&statbuf);
    UTIL_TRACE_RET(ret);
    co_return ret;
}

int UTIL_isRegularFileStat(const stat_t *statbuf)
{
    return S_ISREG(statbuf->st_mode) != 0;
}

/* There are no file permissions here, so upstream's chmod has nothing to do. */
int UTIL_chmod(char const *filename, const stat_t *statbuf, mode_t permissions)
{
    return UTIL_fchmod(-1, filename, statbuf, permissions);
}

int UTIL_fchmod(const int fd, char const *filename, const stat_t *statbuf, mode_t permissions)
{
    (void)fd;
    (void)filename;
    (void)statbuf;
    (void)permissions;
    return 0;
}

/* The store stamps a path when it is written and keeps no atime, so there is no
 * modification time to set back. */
int UTIL_utime(const char *filename, const stat_t *statbuf)
{
    (void)filename;
    (void)statbuf;
    return 0;
}

int UTIL_setFileStat(const char *filename, const stat_t *statbuf)
{
    return UTIL_setFDStat(-1, filename, statbuf);
}

/* Owner, group and permissions are all absent, so upstream's whole transfer --
 * group, then mode, then owner -- is a no-op reporting no errors. */
int UTIL_setFDStat(const int fd, const char *filename, const stat_t *statbuf)
{
    (void)fd;
    (void)filename;
    (void)statbuf;
    return 0;
}

Task<int> UTIL_isDirectory(const char *infilename)
{
    stat_t statbuf;
    int ret;
    UTIL_TRACE_CALL("UTIL_isDirectory(%s)", infilename);
    ret = co_await UTIL_stat(infilename, &statbuf) && UTIL_isDirectoryStat(&statbuf);
    UTIL_TRACE_RET(ret);
    co_return ret;
}

int UTIL_isDirectoryStat(const stat_t *statbuf)
{
    int ret;
    UTIL_TRACE_CALL("UTIL_isDirectoryStat()");
    ret = S_ISDIR(statbuf->st_mode) != 0;
    UTIL_TRACE_RET(ret);
    return ret;
}

int UTIL_compareStr(const void *p1, const void *p2)
{
    return strcmp(*(char *const *)p1, *(char *const *)p2);
}

Task<int> UTIL_isSameFile(const char *fName1, const char *fName2)
{
    int ret;
    assert(fName1 != NULL);
    assert(fName2 != NULL);
    UTIL_TRACE_CALL("UTIL_isSameFile(%s, %s)", fName1, fName2);
    {
        stat_t file1Stat;
        stat_t file2Stat;
        ret = co_await UTIL_stat(fName1, &file1Stat) && co_await UTIL_stat(fName2, &file2Stat) &&
              UTIL_isSameFileStat(fName1, fName2, &file1Stat, &file2Stat);
    }
    UTIL_TRACE_RET(ret);
    co_return ret;
}

/* st_ino is a hash of the path and st_dev is 1, so this is name identity the
 * long way round -- which is what upstream's Windows branch settles for too. */
int UTIL_isSameFileStat(const char *fName1, const char *fName2, const stat_t *file1Stat,
                        const stat_t *file2Stat)
{
    int ret;
    assert(fName1 != NULL);
    assert(fName2 != NULL);
    UTIL_TRACE_CALL("UTIL_isSameFileStat(%s, %s)", fName1, fName2);
    {
        ret = (file1Stat->st_dev == file2Stat->st_dev) && (file1Stat->st_ino == file2Stat->st_ino);
    }
    UTIL_TRACE_RET(ret);
    return ret;
}

/* UTIL_isFIFO : distinguish named pipes */
Task<int> UTIL_isFIFO(const char *infilename)
{
    UTIL_TRACE_CALL("UTIL_isFIFO(%s)", infilename);
    (void)infilename;
    UTIL_TRACE_RET(0);
    co_return 0;
}

/* There are no FIFOs: S_ISFIFO is 0 for every mode the store reports. */
int UTIL_isFIFOStat(const stat_t *statbuf)
{
    (void)statbuf;
    return 0;
}

/* process substitution */
int UTIL_isFileDescriptorPipe(const char *filename)
{
    UTIL_TRACE_CALL("UTIL_isFileDescriptorPipe(%s)", filename);
    /* Check if the filename is a /dev/fd/ path which indicates a file descriptor */
    if (filename[0] == '/' && strncmp(filename, "/dev/fd/", 8) == 0) {
        UTIL_TRACE_RET(1);
        return 1;
    }

    /* Check for alternative process substitution formats on different systems */
    if (filename[0] == '/' && strncmp(filename, "/proc/self/fd/", 14) == 0) {
        UTIL_TRACE_RET(1);
        return 1;
    }

    UTIL_TRACE_RET(0);
    return 0; /* Not recognized as a file descriptor pipe */
}

/* There are no block devices either. */
int UTIL_isBlockDevStat(const stat_t *statbuf)
{
    (void)statbuf;
    return 0;
}

Task<int> UTIL_isLink(const char *infilename)
{
    UTIL_TRACE_CALL("UTIL_isLink(%s)", infilename);
    {
        stat_t statbuf;
        int const r = co_await b_lstat(infilename, &statbuf);
        if (!r && S_ISLNK(statbuf.st_mode)) {
            UTIL_TRACE_RET(1);
            co_return 1;
        }
    }
    UTIL_TRACE_RET(0);
    co_return 0;
}

static int g_fakeStdinIsConsole  = 0;
static int g_fakeStderrIsConsole = 0;
static int g_fakeStdoutIsConsole = 0;

Task<int> UTIL_isConsole(FILE *file)
{
    int ret;
    UTIL_TRACE_CALL("UTIL_isConsole(%d)", b_fileno(file));
    if (file == stdin && g_fakeStdinIsConsole)
        ret = 1;
    else if (file == stderr && g_fakeStderrIsConsole)
        ret = 1;
    else if (file == stdout && g_fakeStdoutIsConsole)
        ret = 1;
    else
        ret = co_await b_isatty(b_fileno(file));
    UTIL_TRACE_RET(ret);
    co_return ret;
}

void UTIL_fakeStdinIsConsole(void)
{
    g_fakeStdinIsConsole = 1;
}
void UTIL_fakeStdoutIsConsole(void)
{
    g_fakeStdoutIsConsole = 1;
}
void UTIL_fakeStderrIsConsole(void)
{
    g_fakeStderrIsConsole = 1;
}

Task<U64> UTIL_getFileSize(const char *infilename)
{
    stat_t statbuf;
    UTIL_TRACE_CALL("UTIL_getFileSize(%s)", infilename);
    if (!co_await UTIL_stat(infilename, &statbuf)) {
        UTIL_TRACE_RET(-1);
        co_return UTIL_FILESIZE_UNKNOWN;
    }
    {
        U64 const size = UTIL_getFileSizeStat(&statbuf);
        UTIL_TRACE_RET((int)size);
        co_return size;
    }
}

U64 UTIL_getFileSizeStat(const stat_t *statbuf)
{
    if (!UTIL_isRegularFileStat(statbuf))
        return UTIL_FILESIZE_UNKNOWN;
    if (!S_ISREG(statbuf->st_mode))
        return UTIL_FILESIZE_UNKNOWN;
    return (U64)statbuf->st_size;
}

UTIL_HumanReadableSize_t UTIL_makeHumanReadableSize(U64 size)
{
    UTIL_HumanReadableSize_t hrs;

    if (g_utilDisplayLevel > 3) {
        /* In verbose mode, do not scale sizes down, except in the case of
         * values that exceed the integral precision of a double. */
        if (size >= (1ull << 53)) {
            hrs.value  = (double)size / (1ull << 20);
            hrs.suffix = " MiB";
            /* At worst, a double representation of a maximal size will be
             * accurate to better than tens of kilobytes. */
            hrs.precision = 2;
        } else {
            hrs.value     = (double)size;
            hrs.suffix    = " B";
            hrs.precision = 0;
        }
    } else {
        /* In regular mode, scale sizes down and use suffixes. */
        if (size >= (1ull << 60)) {
            hrs.value  = (double)size / (1ull << 60);
            hrs.suffix = " EiB";
        } else if (size >= (1ull << 50)) {
            hrs.value  = (double)size / (1ull << 50);
            hrs.suffix = " PiB";
        } else if (size >= (1ull << 40)) {
            hrs.value  = (double)size / (1ull << 40);
            hrs.suffix = " TiB";
        } else if (size >= (1ull << 30)) {
            hrs.value  = (double)size / (1ull << 30);
            hrs.suffix = " GiB";
        } else if (size >= (1ull << 20)) {
            hrs.value  = (double)size / (1ull << 20);
            hrs.suffix = " MiB";
        } else if (size >= (1ull << 10)) {
            hrs.value  = (double)size / (1ull << 10);
            hrs.suffix = " KiB";
        } else {
            hrs.value  = (double)size;
            hrs.suffix = " B";
        }

        if (hrs.value >= 100 || (U64)hrs.value == size) {
            hrs.precision = 0;
        } else if (hrs.value >= 10) {
            hrs.precision = 1;
        } else if (hrs.value > 1) {
            hrs.precision = 2;
        } else {
            hrs.precision = 3;
        }
    }

    return hrs;
}

Task<U64> UTIL_getTotalFileSize(const char *const *fileNamesTable, unsigned nbFiles)
{
    U64 total = 0;
    unsigned n;
    UTIL_TRACE_CALL("UTIL_getTotalFileSize(%u)", nbFiles);
    for (n = 0; n < nbFiles; n++) {
        U64 const size = co_await UTIL_getFileSize(fileNamesTable[n]);
        if (size == UTIL_FILESIZE_UNKNOWN) {
            UTIL_TRACE_RET(-1);
            co_return UTIL_FILESIZE_UNKNOWN;
        }
        total += size;
    }
    UTIL_TRACE_RET((int)total);
    co_return total;
}

/* Read the entire content of a file into a buffer with progressive resizing */
static Task<char *> UTIL_readFileContent(FILE *inFile, size_t *totalReadPtr)
{
    size_t bufSize   = 64 KB; /* Start with a reasonable buffer size */
    size_t totalRead = 0;
    size_t bytesRead = 0;
    char *buf        = (char *)malloc(bufSize);
    if (buf == NULL)
        co_return NULL;

    /* Read the file incrementally */
    while ((bytesRead = co_await zstd_fread(buf + totalRead, 1, bufSize - totalRead - 1, inFile)) >
           0) {
        totalRead += bytesRead;

        /* If buffer is nearly full, expand it */
        if (bufSize - totalRead < 1 KB) {
            if (bufSize >= MAX_FILE_OF_FILE_NAMES_SIZE) {
                /* Too large, abort */
                free(buf);
                co_return NULL;
            }

            {
                size_t newBufSize = bufSize * 2;
                if (newBufSize > MAX_FILE_OF_FILE_NAMES_SIZE)
                    newBufSize = MAX_FILE_OF_FILE_NAMES_SIZE;

                {
                    char *newBuf = (char *)realloc(buf, newBufSize);
                    if (newBuf == NULL) {
                        free(buf);
                        co_return NULL;
                    }

                    buf     = newBuf;
                    bufSize = newBufSize;
                }
            }
        }
    }

    /* Add null terminator to the end */
    buf[totalRead] = '\0';
    *totalReadPtr  = totalRead;

    co_return buf;
}

/* Process a buffer containing multiple lines and count the number of lines */
static size_t UTIL_processLines(char *buffer, size_t bufferSize)
{
    size_t lineCount = 0;
    size_t i         = 0;

    /* Convert newlines to null terminators and count lines */
    while (i < bufferSize) {
        if (buffer[i] == '\n') {
            buffer[i] = '\0'; /* Replace newlines with null terminators */
            lineCount++;
        }
        i++;
    }

    /* Count the last line if it doesn't end with a newline */
    if (bufferSize > 0 && (i == 0 || buffer[i - 1] != '\0')) {
        lineCount++;
    }

    return lineCount;
}

/* Create an array of pointers to the lines in a buffer */
static const char **UTIL_createLinePointers(char *buffer, size_t numLines, size_t bufferSize)
{
    size_t lineIndex                = 0;
    size_t pos                      = 0;
    void *const bufferPtrs          = malloc(numLines * sizeof(const char **));
    const char **const linePointers = (const char **)bufferPtrs;
    if (bufferPtrs == NULL)
        return NULL;

    while (lineIndex < numLines && pos < bufferSize) {
        size_t len                = 0;
        linePointers[lineIndex++] = buffer + pos;

        /* Find the next null terminator, being careful not to go past the buffer */
        while ((pos + len < bufferSize) && buffer[pos + len] != '\0') {
            len++;
        }

        /* Move past this string and its null terminator */
        pos += len;
        if (pos < bufferSize)
            pos++; /* Skip the null terminator if we're not at buffer end */
    }

    /* Verify we processed the expected number of lines */
    if (lineIndex != numLines) {
        /* Something went wrong - we didn't find as many lines as expected */
        free(bufferPtrs);
        return NULL;
    }

    return linePointers;
}

Task<FileNamesTable *> UTIL_createFileNamesTable_fromFileList(const char *fileList)
{
    stat_t statbuf;
    char *buffer      = NULL;
    size_t numLines   = 0;
    size_t bufferSize = 0;

    /* Check if the input is a valid file */
    if (!co_await UTIL_stat(fileList, &statbuf)) {
        co_return NULL;
    }

    /* Check if the input is a supported type */
    if (!UTIL_isRegularFileStat(&statbuf) && !UTIL_isFIFOStat(&statbuf) &&
        !UTIL_isFileDescriptorPipe(fileList)) {
        co_return NULL;
    }

    /* Open the input file */
    {
        FILE *const inFile = co_await b_fopen(fileList, "rb");
        if (inFile == NULL)
            co_return NULL;

        /* Read the file content */
        buffer = co_await UTIL_readFileContent(inFile, &bufferSize);
        co_await b_fclose(inFile);
    }

    if (buffer == NULL)
        co_return NULL;

    /* Process lines */
    numLines = UTIL_processLines(buffer, bufferSize);
    if (numLines == 0) {
        free(buffer);
        co_return NULL;
    }

    /* Create line pointers */
    {
        const char **linePointers = UTIL_createLinePointers(buffer, numLines, bufferSize);
        if (linePointers == NULL) {
            free(buffer);
            co_return NULL;
        }

        /* Create the final table */
        co_return UTIL_assembleFileNamesTable(linePointers, numLines, buffer);
    }
}

static FileNamesTable *UTIL_assembleFileNamesTable2(const char **filenames, size_t tableSize,
                                                    size_t tableCapacity, char *buf)
{
    FileNamesTable *const table = (FileNamesTable *)malloc(sizeof(*table));
    CONTROL(table != NULL);
    if (table == NULL)
        return NULL;
    table->fileNames     = filenames;
    table->buf           = buf;
    table->tableSize     = tableSize;
    table->tableCapacity = tableCapacity;
    return table;
}

FileNamesTable *UTIL_assembleFileNamesTable(const char **filenames, size_t tableSize, char *buf)
{
    return UTIL_assembleFileNamesTable2(filenames, tableSize, tableSize, buf);
}

void UTIL_freeFileNamesTable(FileNamesTable *table)
{
    if (table == NULL)
        return;
    free((void *)table->fileNames);
    free(table->buf);
    free(table);
}

FileNamesTable *UTIL_allocateFileNamesTable(size_t tableSize)
{
    const char **const fnTable = (const char **)malloc(tableSize * sizeof(*fnTable));
    FileNamesTable *fnt;
    if (fnTable == NULL)
        return NULL;
    fnt = UTIL_assembleFileNamesTable(fnTable, tableSize, NULL);
    if (fnt == NULL) {
        free((void *)fnTable);
        return NULL;
    }
    fnt->tableSize = 0; /* the table is empty */
    return fnt;
}

int UTIL_searchFileNamesTable(FileNamesTable *table, char const *name)
{
    size_t i;
    for (i = 0; i < table->tableSize; i++) {
        if (!strcmp(table->fileNames[i], name)) {
            return (int)i;
        }
    }
    return -1;
}

void UTIL_refFilename(FileNamesTable *fnt, const char *filename)
{
    assert(fnt->tableSize < fnt->tableCapacity);
    fnt->fileNames[fnt->tableSize] = filename;
    fnt->tableSize++;
}

static size_t getTotalTableSize(FileNamesTable *table)
{
    size_t fnb, totalSize = 0;
    for (fnb = 0; fnb < table->tableSize && table->fileNames[fnb]; ++fnb) {
        totalSize +=
            strlen(table->fileNames[fnb]) + 1; /* +1 to add '\0' at the end of each fileName */
    }
    return totalSize;
}

FileNamesTable *UTIL_mergeFileNamesTable(FileNamesTable *table1, FileNamesTable *table2)
{
    unsigned newTableIdx = 0;
    size_t pos           = 0;
    size_t newTotalTableSize;
    char *buf;

    FileNamesTable *const newTable = UTIL_assembleFileNamesTable(NULL, 0, NULL);
    CONTROL(newTable != NULL);
    if (newTable == NULL)
        return NULL;

    newTotalTableSize = getTotalTableSize(table1) + getTotalTableSize(table2);

    buf = (char *)calloc(newTotalTableSize, sizeof(*buf));
    CONTROL(buf != NULL);
    if (buf == NULL) {
        free(newTable);
        return NULL;
    }

    newTable->buf       = buf;
    newTable->tableSize = table1->tableSize + table2->tableSize;
    newTable->fileNames =
        (const char **)calloc(newTable->tableSize, sizeof(*(newTable->fileNames)));
    CONTROL(newTable->fileNames != NULL);
    if (newTable->fileNames == NULL) {
        UTIL_freeFileNamesTable(newTable);
        return NULL;
    }

    {
        unsigned idx1;
        for (idx1 = 0;
             (idx1 < table1->tableSize) && table1->fileNames[idx1] && (pos < newTotalTableSize);
             ++idx1, ++newTableIdx) {
            size_t const curLen = strlen(table1->fileNames[idx1]);
            memcpy(buf + pos, table1->fileNames[idx1], curLen);
            assert(newTableIdx <= newTable->tableSize);
            newTable->fileNames[newTableIdx] = buf + pos;
            pos += curLen + 1;
        }
    }

    {
        unsigned idx2;
        for (idx2 = 0;
             (idx2 < table2->tableSize) && table2->fileNames[idx2] && (pos < newTotalTableSize);
             ++idx2, ++newTableIdx) {
            size_t const curLen = strlen(table2->fileNames[idx2]);
            memcpy(buf + pos, table2->fileNames[idx2], curLen);
            assert(newTableIdx < newTable->tableSize);
            newTable->fileNames[newTableIdx] = buf + pos;
            pos += curLen + 1;
        }
    }
    assert(pos <= newTotalTableSize);
    newTable->tableSize = newTableIdx;

    UTIL_freeFileNamesTable(table1);
    UTIL_freeFileNamesTable(table2);

    return newTable;
}

/* Braam: b_opendir takes the whole listing in one syscall and the walk after it
 * does not block, so upstream's loop keeps its shape. The recursion is a task
 * calling a task; a directory tree deep enough to matter would cost native
 * stack, and the syscall at each level is what gives it back. */
static Task<int> UTIL_prepareFileList(const char *dirName, char **bufStart, size_t *pos,
                                      char **bufEnd, int followLinks)
{
    DIR *dir;
    struct dirent *entry;
    size_t dirLength;
    int nbFiles = 0;

    if (!(dir = co_await b_opendir(dirName))) {
        UTIL_DISPLAYLEVEL(1, "Cannot open directory '%s': %s\n", dirName, strerror(errno));
        co_return 0;
    }

    dirLength = strlen(dirName);
    errno     = 0;
    while ((entry = b_readdir(dir)) != NULL) {
        char *path;
        size_t fnameLength, pathLength;
        if (strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".") == 0)
            continue;
        fnameLength = strlen(entry->d_name);
        path        = (char *)malloc(dirLength + fnameLength + 2);
        if (!path) {
            b_closedir(dir);
            co_return 0;
        }
        memcpy(path, dirName, dirLength);

        path[dirLength] = '/';
        memcpy(path + dirLength + 1, entry->d_name, fnameLength);
        pathLength       = dirLength + 1 + fnameLength;
        path[pathLength] = 0;

        if (!followLinks && co_await UTIL_isLink(path)) {
            UTIL_DISPLAYLEVEL(2, "Warning : %s is a symbolic link, ignoring\n", path);
            free(path);
            continue;
        }

        if (co_await UTIL_isDirectory(path)) {
            nbFiles += co_await UTIL_prepareFileList(
                path, bufStart, pos, bufEnd,
                followLinks); /* Recursively call "UTIL_prepareFileList" with the new path. */
            if (*bufStart == NULL) {
                free(path);
                b_closedir(dir);
                co_return 0;
            }
        } else {
            if (*bufStart + *pos + pathLength >= *bufEnd) {
                ptrdiff_t newListSize = (*bufEnd - *bufStart) + LIST_SIZE_INCREASE;
                assert(newListSize >= 0);
                *bufStart = (char *)UTIL_realloc(*bufStart, (size_t)newListSize);
                if (*bufStart != NULL) {
                    *bufEnd = *bufStart + newListSize;
                } else {
                    free(path);
                    b_closedir(dir);
                    co_return 0;
                }
            }
            if (*bufStart + *pos + pathLength < *bufEnd) {
                memcpy(*bufStart + *pos, path, pathLength + 1); /* with final \0 */
                *pos += pathLength + 1;
                nbFiles++;
            }
        }
        free(path);
        errno = 0; /* clear errno after UTIL_isDirectory, UTIL_prepareFileList */
    }

    b_closedir(dir);
    co_return nbFiles;
}

int UTIL_isCompressedFile(const char *inputName, const char *extensionList[])
{
    const char *ext = UTIL_getFileExtension(inputName);
    while (*extensionList != NULL) {
        const int isCompressedExtension = strcmp(ext, *extensionList);
        if (isCompressedExtension == 0)
            return 1;
        ++extensionList;
    }
    return 0;
}

/*Utility function to get file extension from file */
const char *UTIL_getFileExtension(const char *infilename)
{
    const char *extension = strrchr(infilename, '.');
    if (!extension || extension == infilename)
        return "";
    return extension;
}

static int pathnameHas2Dots(const char *pathname)
{
    /* We need to figure out whether any ".." present in the path is a whole
     * path token, which is the case if it is bordered on both sides by either
     * the beginning/end of the path or by a directory separator.
     */
    const char *needle = pathname;
    while (1) {
        needle = strstr(needle, "..");

        if (needle == NULL) {
            return 0;
        }

        if ((needle == pathname || needle[-1] == PATH_SEP) &&
            (needle[2] == '\0' || needle[2] == PATH_SEP)) {
            return 1;
        }

        /* increment so we search for the next match */
        needle++;
    };
    return 0;
}

static int isFileNameValidForMirroredOutput(const char *filename)
{
    return !pathnameHas2Dots(filename);
}

#define DIR_DEFAULT_MODE 0755
static Task<mode_t> getDirMode(const char *dirName)
{
    stat_t st;
    if (!co_await UTIL_stat(dirName, &st)) {
        UTIL_DISPLAY("zstd: failed to get DIR stats %s: %s\n", dirName, strerror(errno));
        co_return DIR_DEFAULT_MODE;
    }
    if (!UTIL_isDirectoryStat(&st)) {
        UTIL_DISPLAY("zstd: expected directory: %s\n", dirName);
        co_return DIR_DEFAULT_MODE;
    }
    co_return st.st_mode;
}

static Task<int> makeDir(const char *dir, mode_t mode)
{
    int ret = co_await b_mkdir(dir, mode);
    if (ret != 0) {
        if (errno == EEXIST)
            co_return 0;
        UTIL_DISPLAY("zstd: failed to create DIR %s: %s\n", dir, strerror(errno));
    }
    co_return ret;
}

/* this function requires a mutable input string */
static void convertPathnameToDirName(char *pathname)
{
    size_t len = 0;
    char *pos  = NULL;
    /* get dir name from pathname similar to 'dirname()' */
    assert(pathname != NULL);

    /* remove trailing '/' chars */
    len = strlen(pathname);
    assert(len > 0);
    while (pathname[len] == PATH_SEP) {
        pathname[len] = '\0';
        len--;
    }
    if (len == 0)
        return;

    /* if input is a single file, return '.' instead. i.e.
     * "xyz/abc/file.txt" => "xyz/abc"
       "./file.txt"       => "."
       "file.txt"         => "."
     */
    pos = strrchr(pathname, PATH_SEP);
    if (pos == NULL) {
        pathname[0] = '.';
        pathname[1] = '\0';
    } else {
        *pos = '\0';
    }
}

/* pathname must be valid */
static const char *trimLeadingRootChar(const char *pathname)
{
    assert(pathname != NULL);
    if (pathname[0] == PATH_SEP)
        return pathname + 1;
    return pathname;
}

/* pathname must be valid */
static const char *trimLeadingCurrentDirConst(const char *pathname)
{
    assert(pathname != NULL);
    if ((pathname[0] == '.') && (pathname[1] == PATH_SEP))
        return pathname + 2;
    return pathname;
}

static char *trimLeadingCurrentDir(char *pathname)
{
    /* 'union charunion' can do const-cast without compiler warning */
    union charunion {
        char *chr;
        const char *cchr;
    } ptr;
    ptr.cchr = trimLeadingCurrentDirConst(pathname);
    return ptr.chr;
}

/* remove leading './' or '/' chars here */
static const char *trimPath(const char *pathname)
{
    return trimLeadingRootChar(trimLeadingCurrentDirConst(pathname));
}

static char *mallocAndJoin2Dir(const char *dir1, const char *dir2)
{
    assert(dir1 != NULL && dir2 != NULL);
    {
        const size_t dir1Size = strlen(dir1);
        const size_t dir2Size = strlen(dir2);
        char *outDirBuffer, *buffer;

        outDirBuffer = (char *)malloc(dir1Size + dir2Size + 2);
        CONTROL(outDirBuffer != NULL);
        if (outDirBuffer == NULL)
            return NULL;

        memcpy(outDirBuffer, dir1, dir1Size);
        outDirBuffer[dir1Size] = '\0';

        buffer = outDirBuffer + dir1Size;
        if (dir1Size > 0 && *(buffer - 1) != PATH_SEP) {
            *buffer = PATH_SEP;
            buffer++;
        }
        memcpy(buffer, dir2, dir2Size);
        buffer[dir2Size] = '\0';

        return outDirBuffer;
    }
}

/* this function will return NULL if input srcFileName is not valid name for mirrored output path */
char *UTIL_createMirroredDestDirName(const char *srcFileName, const char *outDirRootName)
{
    char *pathname = NULL;
    if (!isFileNameValidForMirroredOutput(srcFileName))
        return NULL;

    pathname = mallocAndJoin2Dir(outDirRootName, trimPath(srcFileName));
    if (pathname == NULL)
        return NULL;

    convertPathnameToDirName(pathname);
    return pathname;
}

static Task<int> mirrorSrcDir(char *srcDirName, const char *outDirName)
{
    mode_t srcMode;
    int status   = 0;
    char *newDir = mallocAndJoin2Dir(outDirName, trimPath(srcDirName));
    if (!newDir)
        co_return -ENOMEM;

    srcMode = co_await getDirMode(srcDirName);
    status  = co_await makeDir(newDir, srcMode);
    free(newDir);
    co_return status;
}

static Task<int> mirrorSrcDirRecursive(char *srcDirName, const char *outDirName)
{
    int status = 0;
    char *pp   = trimLeadingCurrentDir(srcDirName);
    char *sp   = NULL;

    while ((sp = strchr(pp, PATH_SEP)) != NULL) {
        if (sp != pp) {
            *sp    = '\0';
            status = co_await mirrorSrcDir(srcDirName, outDirName);
            if (status != 0)
                co_return status;
            *sp = PATH_SEP;
        }
        pp = sp + 1;
    }
    status = co_await mirrorSrcDir(srcDirName, outDirName);
    co_return status;
}

static Task<void> makeMirroredDestDirsWithSameSrcDirMode(char **srcDirNames, unsigned nbFile,
                                                         const char *outDirName)
{
    unsigned int i = 0;
    for (i = 0; i < nbFile; i++)
        co_await mirrorSrcDirRecursive(srcDirNames[i], outDirName);
}

static int firstIsParentOrSameDirOfSecond(const char *firstDir, const char *secondDir)
{
    size_t firstDirLen = strlen(firstDir), secondDirLen = strlen(secondDir);
    return firstDirLen <= secondDirLen &&
           (secondDir[firstDirLen] == PATH_SEP || secondDir[firstDirLen] == '\0') &&
           0 == strncmp(firstDir, secondDir, firstDirLen);
}

static int compareDir(const void *pathname1, const void *pathname2)
{
    /* sort it after remove the leading '/'  or './'*/
    const char *s1 = trimPath(*(char *const *)pathname1);
    const char *s2 = trimPath(*(char *const *)pathname2);
    return strcmp(s1, s2);
}

static Task<void> makeUniqueMirroredDestDirs(char **srcDirNames, unsigned nbFile,
                                             const char *outDirName)
{
    unsigned int i = 0, uniqueDirNr = 0;
    char **uniqueDirNames = NULL;

    if (nbFile == 0)
        co_return;

    uniqueDirNames = (char **)malloc(nbFile * sizeof(char *));
    CONTROL(uniqueDirNames != NULL);
    if (uniqueDirNames == NULL)
        co_return;

    /* if dirs is "a/b/c" and "a/b/c/d", we only need call:
     * we just need "a/b/c/d" */
    qsort((void *)srcDirNames, nbFile, sizeof(char *), compareDir);

    uniqueDirNr                     = 1;
    uniqueDirNames[uniqueDirNr - 1] = srcDirNames[0];
    for (i = 1; i < nbFile; i++) {
        char *prevDirName = srcDirNames[i - 1];
        char *currDirName = srcDirNames[i];

        /* note: we always compare trimmed path, i.e.:
         * src dir of "./foo" and "/foo" will be both saved into:
         * "outDirName/foo/" */
        if (!firstIsParentOrSameDirOfSecond(trimPath(prevDirName), trimPath(currDirName)))
            uniqueDirNr++;

        /* we need to maintain original src dir name instead of trimmed
         * dir, so we can retrieve the original src dir's mode_t */
        uniqueDirNames[uniqueDirNr - 1] = currDirName;
    }

    co_await makeMirroredDestDirsWithSameSrcDirMode(uniqueDirNames, uniqueDirNr, outDirName);

    free(uniqueDirNames);
}

static Task<void> makeMirroredDestDirs(char **srcFileNames, unsigned nbFile, const char *outDirName)
{
    unsigned int i = 0;
    for (i = 0; i < nbFile; ++i)
        convertPathnameToDirName(srcFileNames[i]);
    co_await makeUniqueMirroredDestDirs(srcFileNames, nbFile, outDirName);
}

Task<void> UTIL_mirrorSourceFilesDirectories(const char **inFileNames, unsigned int nbFile,
                                             const char *outDirName)
{
    unsigned int i = 0, validFilenamesNr = 0;
    char **srcFileNames = (char **)malloc(nbFile * sizeof(char *));
    CONTROL(srcFileNames != NULL);
    if (srcFileNames == NULL)
        co_return;

    /* check input filenames is valid */
    for (i = 0; i < nbFile; ++i) {
        if (isFileNameValidForMirroredOutput(inFileNames[i])) {
            char *fname = STRDUP(inFileNames[i]);
            CONTROL(fname != NULL);
            if (fname == NULL)
                break;
            srcFileNames[validFilenamesNr++] = fname;
        }
    }

    if (validFilenamesNr > 0) {
        co_await makeDir(outDirName, DIR_DEFAULT_MODE);
        co_await makeMirroredDestDirs(srcFileNames, validFilenamesNr, outDirName);
    }

    for (i = 0; i < validFilenamesNr; i++)
        free(srcFileNames[i]);
    free(srcFileNames);
}

Task<FileNamesTable *> UTIL_createExpandedFNT(const char *const *inputNames, size_t nbIfns,
                                              int followLinks)
{
    unsigned nbFiles;
    char *buf    = (char *)malloc(LIST_SIZE_INCREASE);
    char *bufend = buf + LIST_SIZE_INCREASE;

    if (!buf)
        co_return NULL;

    {
        size_t ifnNb, pos;
        for (ifnNb = 0, pos = 0, nbFiles = 0; ifnNb < nbIfns; ifnNb++) {
            if (!co_await UTIL_isDirectory(inputNames[ifnNb])) {
                size_t const len = strlen(inputNames[ifnNb]);
                if (buf + pos + len >= bufend) {
                    ptrdiff_t newListSize = (bufend - buf) + LIST_SIZE_INCREASE;
                    assert(newListSize >= 0);
                    buf = (char *)UTIL_realloc(buf, (size_t)newListSize);
                    if (!buf)
                        co_return NULL;
                    bufend = buf + newListSize;
                }
                if (buf + pos + len < bufend) {
                    memcpy(buf + pos, inputNames[ifnNb], len + 1); /* including final \0 */
                    pos += len + 1;
                    nbFiles++;
                }
            } else {
                nbFiles += (unsigned)co_await UTIL_prepareFileList(inputNames[ifnNb], &buf, &pos,
                                                                   &bufend, followLinks);
                if (buf == NULL)
                    co_return NULL;
            }
        }
    }

    /* note : even if nbFiles==0, function returns a valid, though empty, FileNamesTable* object */

    {
        size_t ifnNb, pos;
        size_t const fntCapacity =
            nbFiles + 1; /* minimum 1, allows adding one reference, typically stdin */
        const char **const fileNamesTable =
            (const char **)malloc(fntCapacity * sizeof(*fileNamesTable));
        if (!fileNamesTable) {
            free(buf);
            co_return NULL;
        }

        for (ifnNb = 0, pos = 0; ifnNb < nbFiles; ifnNb++) {
            fileNamesTable[ifnNb] = buf + pos;
            if (buf + pos > bufend) {
                free(buf);
                free((void *)fileNamesTable);
                co_return NULL;
            }
            pos += strlen(fileNamesTable[ifnNb]) + 1;
        }
        co_return UTIL_assembleFileNamesTable2(fileNamesTable, nbFiles, fntCapacity, buf);
    }
}

Task<void> UTIL_expandFNT(FileNamesTable **fnt, int followLinks)
{
    FileNamesTable *const newFNT =
        co_await UTIL_createExpandedFNT((*fnt)->fileNames, (*fnt)->tableSize, followLinks);
    CONTROL(newFNT != NULL);
    if (newFNT == NULL)
        co_return;
    UTIL_freeFileNamesTable(*fnt);
    *fnt = newFNT;
}

FileNamesTable *UTIL_createFNT_fromROTable(const char **filenames, size_t nbFilenames)
{
    size_t const sizeof_FNTable   = nbFilenames * sizeof(*filenames);
    const char **const newFNTable = (const char **)malloc(sizeof_FNTable);
    if (newFNTable == NULL)
        return NULL;
    memcpy((void *)newFNTable, filenames,
           sizeof_FNTable); /* void* : mitigate a Visual compiler bug or limitation */
    return UTIL_assembleFileNamesTable(newFNTable, nbFilenames, NULL);
}

/*-****************************************
 *  count the number of cores
 ******************************************/

/* One worker per process: there are no threads to count. */
int UTIL_countCores(int logical)
{
    (void)logical;
    return 1;
}

int UTIL_countPhysicalCores(void)
{
    return UTIL_countCores(0);
}

int UTIL_countLogicalCores(void)
{
    return UTIL_countCores(1);
}
