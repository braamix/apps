/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef UTIL_H_MODULE
#define UTIL_H_MODULE

/*-****************************************
 *  Dependencies
 ******************************************/
#include "platform.h"

/* Braam: what reaches the store is Group B, so it returns a Task. What reads a
 * stat_t already in hand does not and stays a plain function. */

#define UTIL_fseek b_fseeko

/*-*************************************************
 *  Sleep & priority functions
 *  Neither exists here; upstream's `unknown platform` answer.
 ***************************************************/
#define UTIL_sleep(s)
#define UTIL_sleepMilli(milli)
#define SET_REALTIME_PRIORITY

/*-****************************************
 *  Compiler specifics
 ******************************************/
#define UTIL_STATIC static __attribute__((unused))

/*-****************************************
 *  Console log
 ******************************************/
extern int g_utilDisplayLevel;

/**
 * Displays a message prompt and returns success (0) if first character from stdin
 * matches any from acceptableLetters. Otherwise, returns failure (1) and displays abortMsg.
 * If any of the inputs are stdin itself, then automatically return failure (1).
 */
Task<int> UTIL_requireUserConfirmation(const char *prompt, const char *abortMsg,
                                       const char *acceptableLetters, int hasStdinInput);

/*-****************************************
 *  File functions
 ******************************************/
typedef struct stat stat_t;

#define PATH_SEP  '/'
#define STRDUP(s) strdup(s)

/**
 * Calls platform's equivalent of stat() on filename and writes info to statbuf.
 * Returns success (1) or failure (0).
 *
 * UTIL_fstat() is like UTIL_stat() but takes an optional fd that refers to the
 * file in question. It turns out that this can be meaningfully faster. If fd is
 * -1, behaves just like UTIL_stat() (i.e., falls back to using the filename).
 */
Task<int> UTIL_stat(const char *filename, stat_t *statbuf);
Task<int> UTIL_fstat(const int fd, const char *filename, stat_t *statbuf);

/**
 * There are no file permissions and no owners here, and the store stamps a path
 * rather than an open file: upstream's metadata transfer has nothing to carry,
 * so these are no-ops that report success and do not block.
 */
int UTIL_setFileStat(const char *filename, const stat_t *statbuf);
int UTIL_setFDStat(const int fd, const char *filename, const stat_t *statbuf);
int UTIL_utime(const char *filename, const stat_t *statbuf);
int UTIL_chmod(char const *filename, const stat_t *statbuf, mode_t permissions);
int UTIL_fchmod(const int fd, char const *filename, const stat_t *statbuf, mode_t permissions);

/*
 * These helpers operate on a pre-populated stat_t, i.e., the result of
 * calling one of the above functions.
 */

int UTIL_isRegularFileStat(const stat_t *statbuf);
int UTIL_isDirectoryStat(const stat_t *statbuf);
int UTIL_isFIFOStat(const stat_t *statbuf);
int UTIL_isBlockDevStat(const stat_t *statbuf);
U64 UTIL_getFileSizeStat(const stat_t *statbuf);

/*
 * In the absence of a pre-existing stat result on the file in question, these
 * functions will do a stat() call internally and then use that result to
 * compute the needed information.
 */

Task<int> UTIL_isFdRegularFile(int fd);
Task<int> UTIL_isRegularFile(const char *infilename);
Task<int> UTIL_isDirectory(const char *infilename);
Task<int> UTIL_isSameFile(const char *file1, const char *file2);
int UTIL_isSameFileStat(const char *file1, const char *file2, const stat_t *file1Stat,
                        const stat_t *file2Stat);
int UTIL_isCompressedFile(const char *infilename, const char *extensionList[]);
Task<int> UTIL_isLink(const char *infilename);
Task<int> UTIL_isFIFO(const char *infilename);
int UTIL_isFileDescriptorPipe(const char *filename);

/**
 * Returns with the given file descriptor is a console.
 * Allows faking whether stdin/stdout/stderr is a console
 * using UTIL_fake*IsConsole().
 */
Task<int> UTIL_isConsole(FILE *file);

/**
 * Pretends that stdin/stdout/stderr is a console for testing.
 */
void UTIL_fakeStdinIsConsole(void);
void UTIL_fakeStdoutIsConsole(void);
void UTIL_fakeStderrIsConsole(void);

/**
 * Emit traces for functions that read, or modify file metadata.
 */
void UTIL_traceFileStat(void);

#define UTIL_FILESIZE_UNKNOWN ((U64)(-1))
Task<U64> UTIL_getFileSize(const char *infilename);
Task<U64> UTIL_getTotalFileSize(const char *const *fileNamesTable, unsigned nbFiles);

/**
 * Take @size in bytes,
 * prepare the components to pretty-print it in a scaled way.
 * The components in the returned struct should be passed in
 * precision, value, suffix order to a "%.*f%s" format string.
 * Output policy is sensible to @g_utilDisplayLevel,
 * for verbose mode (@g_utilDisplayLevel >= 4),
 * does not scale down.
 */
typedef struct {
    double value;
    int precision;
    const char *suffix;
} UTIL_HumanReadableSize_t;

UTIL_HumanReadableSize_t UTIL_makeHumanReadableSize(U64 size);

int UTIL_compareStr(const void *p1, const void *p2);
const char *UTIL_getFileExtension(const char *infilename);
Task<void> UTIL_mirrorSourceFilesDirectories(const char **fileNamesTable, unsigned int nbFiles,
                                             const char *outDirName);
char *UTIL_createMirroredDestDirName(const char *srcFileName, const char *outDirRootName);

/*-****************************************
 *  Lists of Filenames
 ******************************************/

typedef struct {
    const char **fileNames;
    char *buf;        /* fileNames are stored in this buffer (or are read-only) */
    size_t tableSize; /* nb of fileNames */
    size_t tableCapacity;
} FileNamesTable;

/*! UTIL_createFileNamesTable_fromFileList() :
 *  read filenames from @inputFileName, and store them into returned object.
 * @return : a FileNamesTable*, or NULL in case of error (ex: @inputFileName doesn't exist).
 *  Note: inputFileSize must be less than 50MB
 */
Task<FileNamesTable *> UTIL_createFileNamesTable_fromFileList(const char *inputFileName);

/*! UTIL_assembleFileNamesTable() :
 *  This function takes ownership of its arguments, @filenames and @buf,
 *  and store them inside the created object.
 * @return : resulting FileNamesTable* object, or NULL if allocation fails.
 */
FileNamesTable *UTIL_assembleFileNamesTable(const char **filenames, size_t tableSize, char *buf);

/*! UTIL_freeFileNamesTable() :
 *  This function is compatible with NULL argument and never fails.
 */
void UTIL_freeFileNamesTable(FileNamesTable *table);

/*! UTIL_mergeFileNamesTable():
 * @return : FileNamesTable*, concatenation of @table1 and @table2
 *  note: @table1 and @table2 are consumed (freed) by this operation
 */
FileNamesTable *UTIL_mergeFileNamesTable(FileNamesTable *table1, FileNamesTable *table2);

/*! UTIL_expandFNT() :
 *  read names from @fnt, and expand those corresponding to directories
 *  update @fnt, now containing only file names,
 *  note : in case of error, @fnt[0] is NULL
 */
Task<void> UTIL_expandFNT(FileNamesTable **fnt, int followLinks);

/*! UTIL_createFNT_fromROTable() :
 *  copy the @filenames pointer table inside the returned object.
 *  The names themselves are still stored in their original buffer, which must outlive the object.
 * @return : a FileNamesTable* object,
 *        or NULL in case of error
 */
FileNamesTable *UTIL_createFNT_fromROTable(const char **filenames, size_t nbFilenames);

/*! UTIL_allocateFileNamesTable() :
 *  Allocates a table of const char*, to insert read-only names later on.
 *  The created FileNamesTable* doesn't hold a buffer.
 * @return : FileNamesTable*, or NULL, if allocation fails.
 */
FileNamesTable *UTIL_allocateFileNamesTable(size_t tableSize);

/*! UTIL_searchFileNamesTable() :
 *  Searched through entries in FileNamesTable for a specific name.
 * @return : index of entry if found or -1 if not found
 */
int UTIL_searchFileNamesTable(FileNamesTable *table, char const *name);

/*! UTIL_refFilename() :
 *  Add a reference to read-only name into @fnt table.
 *  As @filename is only referenced, its lifetime must outlive @fnt.
 *  Internal table must be large enough to reference a new member,
 *  otherwise its UB (protected by an `assert()`).
 */
void UTIL_refFilename(FileNamesTable *fnt, const char *filename);

/* b_opendir and b_mkdir are here, so both directory features are. */
#define UTIL_HAS_CREATEFILELIST
#define UTIL_HAS_MIRRORFILELIST

/*! UTIL_createExpandedFNT() :
 *  read names from @filenames, and expand those corresponding to directories.
 *  links are followed or not depending on @followLinks directive.
 * @return : an expanded FileNamesTable*, where each name is a file
 *        or NULL in case of error
 */
Task<FileNamesTable *> UTIL_createExpandedFNT(const char *const *filenames, size_t nbFilenames,
                                              int followLinks);

/*-****************************************
 *  System
 ******************************************/

int UTIL_countCores(int logical);

int UTIL_countPhysicalCores(void);

int UTIL_countLogicalCores(void);

#endif /* UTIL_H_MODULE */
