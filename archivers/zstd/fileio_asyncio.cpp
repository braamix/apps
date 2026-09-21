/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "fileio_asyncio.h"

#include "fileio_common.h"
#include "platform.h"

/* **********************************************************************
 *  Sparse write
 ************************************************************************/

/** AIO_fwriteSparse() :
 *  @return : storedSkips,
 *            argument for next call to AIO_fwriteSparse() or AIO_fwriteSparseEnd() */
static Task<unsigned> AIO_fwriteSparse(FILE *file, const void *buffer, size_t bufferSize,
                                       const FIO_prefs_t *const prefs, unsigned storedSkips)
{
    const size_t *const bufferT =
        (const size_t *)buffer; /* Buffer is supposed malloc'ed, hence aligned on size_t */
    size_t bufferSizeT               = bufferSize / sizeof(size_t);
    const size_t *const bufferTEnd   = bufferT + bufferSizeT;
    const size_t *ptrT               = bufferT;
    static const size_t segmentSizeT = (32 KB) / sizeof(size_t); /* check every 32 KB */

    if (prefs->testMode)
        co_return 0; /* do not output anything in test mode */

    if (!prefs->sparseFileSupport) { /* normal write */
        size_t const sizeCheck = co_await zstd_fwrite(buffer, 1, bufferSize, file);
        if (sizeCheck != bufferSize) {
            EXM_ERROR(70, "Write error : cannot write block : %s", strerror(errno));
            co_return 0;
        }
        co_return 0;
    }

    /* avoid int overflow */
    if (storedSkips > 1 GB) {
        if (co_await LONG_SEEK(file, 1 GB, SEEK_CUR) != 0) {
            EXM_ERROR(91, "1 GB skip error (sparse file support)");
            co_return 0;
        }
        storedSkips -= 1 GB;
    }

    while (ptrT < bufferTEnd) {
        size_t nb0T;

        /* adjust last segment if < 32 KB */
        size_t seg0SizeT = segmentSizeT;
        if (seg0SizeT > bufferSizeT)
            seg0SizeT = bufferSizeT;
        bufferSizeT -= seg0SizeT;

        /* count leading zeroes */
        for (nb0T = 0; (nb0T < seg0SizeT) && (ptrT[nb0T] == 0); nb0T++)
            ;
        storedSkips += (unsigned)(nb0T * sizeof(size_t));

        if (nb0T != seg0SizeT) { /* not all 0s */
            size_t const nbNon0ST = seg0SizeT - nb0T;
            /* skip leading zeros */
            if (co_await LONG_SEEK(file, storedSkips, SEEK_CUR) != 0) {
                EXM_ERROR(92, "Sparse skip error ; try --no-sparse");
                co_return 0;
            }
            storedSkips = 0;
            /* write the rest */
            if (co_await zstd_fwrite(ptrT + nb0T, sizeof(size_t), nbNon0ST, file) != nbNon0ST) {
                EXM_ERROR(93, "Write error : cannot write block : %s", strerror(errno));
                co_return 0;
            }
        }
        ptrT += seg0SizeT;
    }

    {
        static size_t const maskT = sizeof(size_t) - 1;
        if (bufferSize & maskT) {
            /* size not multiple of sizeof(size_t) : implies end of block */
            const char *const restStart = (const char *)bufferTEnd;
            const char *restPtr         = restStart;
            const char *const restEnd   = (const char *)buffer + bufferSize;
            assert(restEnd > restStart && restEnd < restStart + sizeof(size_t));
            for (; (restPtr < restEnd) && (*restPtr == 0); restPtr++)
                ;
            storedSkips += (unsigned)(restPtr - restStart);
            if (restPtr != restEnd) {
                /* not all remaining bytes are 0 */
                size_t const restSize = (size_t)(restEnd - restPtr);
                if (co_await LONG_SEEK(file, storedSkips, SEEK_CUR) != 0) {
                    EXM_ERROR(92, "Sparse skip error ; try --no-sparse");
                    co_return 0;
                }
                if (co_await zstd_fwrite(restPtr, 1, restSize, file) != restSize) {
                    EXM_ERROR(95, "Write error : cannot write end of decoded block : %s",
                              strerror(errno));
                    co_return 0;
                }
                storedSkips = 0;
            }
        }
    }

    co_return storedSkips;
}

static Task<void> AIO_fwriteSparseEnd(const FIO_prefs_t *const prefs, FILE *file,
                                      unsigned storedSkips)
{
    if (prefs->testMode)
        assert(storedSkips == 0);
    if (storedSkips > 0) {
        assert(prefs->sparseFileSupport > 0); /* storedSkips>0 implies sparse support is enabled */
        (void)prefs; /* assert can be disabled, in which case prefs becomes unused */
        if (co_await LONG_SEEK(file, storedSkips - 1, SEEK_CUR) != 0) {
            EXM_ERROR(69, "Final skip error (sparse file support)");
            co_return;
        }
        /* last zero must be explicitly written,
         * so that skipped ones get implicitly translated as zero by FS */
        {
            const char lastZeroByte[1] = { 0 };
            if (co_await zstd_fwrite(lastZeroByte, 1, 1, file) != 1) {
                EXM_ERROR(69, "Write error : cannot write last zero : %s", strerror(errno));
                co_return;
            }
        }
    }
}

/* **********************************************************************
 *  AsyncIO functionality
 ************************************************************************/

/* AIO_supported:
 * Returns 1 if AsyncIO is supported on the system, 0 otherwise. */
int AIO_supported(void)
{
    return 0;
}

/* ***********************************
 *  Generic IoPool implementation
 *************************************/

static IOJob_t *AIO_IOPool_createIoJob(IOPoolCtx_t *ctx, size_t bufferSize)
{
    IOJob_t *const job = (IOJob_t *)malloc(sizeof(IOJob_t));
    void *const buffer = malloc(bufferSize);
    if (!job || !buffer) {
        EXM_ERROR(101, "Allocation error : not enough memory");
        free(job);
        free(buffer);
        return NULL;
    }
    job->buffer         = buffer;
    job->bufferSize     = bufferSize;
    job->usedBufferSize = 0;
    job->file           = NULL;
    job->ctx            = ctx;
    job->offset         = 0;
    return job;
}

/* AIO_IOPool_init:
 * Allocates and sets and a new I/O pool including its included availableJobs. */
static void AIO_IOPool_init(IOPoolCtx_t *ctx, const FIO_prefs_t *prefs,
                            Task<void> (*poolFunction)(void *), size_t bufferSize)
{
    int i;
    ctx->prefs              = prefs;
    ctx->poolFunction       = poolFunction;
    ctx->totalIoJobs        = 2;
    ctx->availableJobsCount = ctx->totalIoJobs;
    for (i = 0; i < ctx->availableJobsCount; i++) {
        ctx->availableJobs[i] = AIO_IOPool_createIoJob(ctx, bufferSize);
    }
    ctx->jobBufferSize = bufferSize;
    ctx->file          = NULL;
}

/* AIO_IOPool_releaseIoJob:
 * Releases an acquired job back to the pool. Doesn't execute the job. */
static void AIO_IOPool_releaseIoJob(IOJob_t *job)
{
    IOPoolCtx_t *const ctx = (IOPoolCtx_t *)job->ctx;
    assert(ctx->availableJobsCount < ctx->totalIoJobs);
    ctx->availableJobs[ctx->availableJobsCount++] = job;
}

/* AIO_IOPool_free:
 * Release a previously allocated IO pool. Makes sure all jobs are released. */
static void AIO_IOPool_destroy(IOPoolCtx_t *ctx)
{
    int i;
    assert(ctx->file == NULL);
    for (i = 0; i < ctx->availableJobsCount; i++) {
        IOJob_t *job = (IOJob_t *)ctx->availableJobs[i];
        if (job == NULL)
            continue;
        free(job->buffer);
        free(job);
    }
}

/* AIO_IOPool_acquireJob:
 * Returns an available io job to be used for a future io. */
static IOJob_t *AIO_IOPool_acquireJob(IOPoolCtx_t *ctx)
{
    IOJob_t *job;
    assert(ctx->file != NULL || ctx->prefs->testMode);
    assert(ctx->availableJobsCount > 0);
    job = (IOJob_t *)ctx->availableJobs[--ctx->availableJobsCount];
    if (job == NULL)
        return NULL;
    job->usedBufferSize = 0;
    job->file           = ctx->file;
    job->offset         = 0;
    return job;
}

/* AIO_IOPool_setFile:
 * Sets the destination file for future files in the pool.
 * Requires release of all otherwise acquired jobs. */
static void AIO_IOPool_setFile(IOPoolCtx_t *ctx, FILE *file)
{
    assert(ctx != NULL);
    assert(ctx->availableJobsCount == ctx->totalIoJobs);
    ctx->file = file;
}

static FILE *AIO_IOPool_getFile(const IOPoolCtx_t *ctx)
{
    return ctx->file;
}

/* AIO_IOPool_enqueueJob:
 * Executes an io job. With no thread to hand it to, `enqueue` is the call.
 * The queued job shouldn't be used directly after queueing it. */
static Task<void> AIO_IOPool_enqueueJob(IOJob_t *job)
{
    IOPoolCtx_t *const ctx = (IOPoolCtx_t *)job->ctx;
    co_await ctx->poolFunction(job);
}

/* ***********************************
 *  WritePool implementation
 *************************************/

/* AIO_WritePool_acquireJob:
 * Returns an available write job to be used for a future write. */
IOJob_t *AIO_WritePool_acquireJob(WritePoolCtx_t *ctx)
{
    return AIO_IOPool_acquireJob(&ctx->base);
}

/* AIO_WritePool_enqueueAndReacquireWriteJob:
 * Queues a write job for execution and acquires a new one.
 * After execution `job`'s pointed value would change to the newly acquired job.
 * Make sure to set `usedBufferSize` to the wanted length before call.
 * The queued job shouldn't be used directly after queueing it. */
Task<void> AIO_WritePool_enqueueAndReacquireWriteJob(IOJob_t **job)
{
    IOPoolCtx_t *const ctx = (IOPoolCtx_t *)(*job)->ctx;
    co_await AIO_IOPool_enqueueJob(*job);
    *job = AIO_IOPool_acquireJob(ctx);
}

/* AIO_WritePool_sparseWriteEnd:
 * Ends sparse writes to the current file. */
Task<void> AIO_WritePool_sparseWriteEnd(WritePoolCtx_t *ctx)
{
    assert(ctx != NULL);
    if (ctx->base.file != NULL)
        co_await AIO_fwriteSparseEnd(ctx->base.prefs, ctx->base.file, ctx->storedSkips);
    ctx->storedSkips = 0;
}

/* AIO_WritePool_setFile:
 * Sets the destination file for future writes in the pool.
 * Requires release of all otherwise acquired jobs.
 * Also requires ending of sparse write if a previous file was used in sparse mode. */
void AIO_WritePool_setFile(WritePoolCtx_t *ctx, FILE *file)
{
    AIO_IOPool_setFile(&ctx->base, file);
    assert(ctx->storedSkips == 0);
}

/* AIO_WritePool_getFile:
 * Returns the file the writePool is currently set to write to. */
FILE *AIO_WritePool_getFile(const WritePoolCtx_t *ctx)
{
    return AIO_IOPool_getFile(&ctx->base);
}

/* AIO_WritePool_releaseIoJob:
 * Releases an acquired job back to the pool. Doesn't execute the job. */
void AIO_WritePool_releaseIoJob(IOJob_t *job)
{
    AIO_IOPool_releaseIoJob(job);
}

/* AIO_WritePool_closeFile:
 * Ends sparse write and closes the writePool's current file and sets the file to NULL.
 * Requires release of all otherwise acquired jobs.  */
Task<int> AIO_WritePool_closeFile(WritePoolCtx_t *ctx)
{
    FILE *const dstFile = ctx->base.file;
    assert(dstFile != NULL || ctx->base.prefs->testMode != 0);
    co_await AIO_WritePool_sparseWriteEnd(ctx);
    AIO_IOPool_setFile(&ctx->base, NULL);
    if (dstFile == NULL)
        co_return 0;
    co_return co_await b_fclose(dstFile);
}

/* AIO_WritePool_executeWriteJob:
 * Executes a write job. */
static Task<void> AIO_WritePool_executeWriteJob(void *opaque)
{
    IOJob_t *const job        = (IOJob_t *)opaque;
    WritePoolCtx_t *const ctx = (WritePoolCtx_t *)job->ctx;
    ctx->storedSkips = co_await AIO_fwriteSparse(job->file, job->buffer, job->usedBufferSize,
                                                 ctx->base.prefs, ctx->storedSkips);
    AIO_IOPool_releaseIoJob(job);
}

/* AIO_WritePool_create:
 * Allocates and sets and a new write pool including its included jobs. */
WritePoolCtx_t *AIO_WritePool_create(const FIO_prefs_t *prefs, size_t bufferSize)
{
    WritePoolCtx_t *const ctx = (WritePoolCtx_t *)malloc(sizeof(WritePoolCtx_t));
    if (!ctx) {
        EXM_ERROR(100, "Allocation error : not enough memory");
        return NULL;
    }
    AIO_IOPool_init(&ctx->base, prefs, AIO_WritePool_executeWriteJob, bufferSize);
    ctx->storedSkips = 0;
    return ctx;
}

/* AIO_WritePool_free:
 * Frees and releases a writePool and its resources. Closes destination file if needs to. */
Task<void> AIO_WritePool_free(WritePoolCtx_t *ctx)
{
    if (ctx == NULL)
        co_return;
    if (AIO_WritePool_getFile(ctx))
        co_await AIO_WritePool_closeFile(ctx);
    AIO_IOPool_destroy(&ctx->base);
    assert(ctx->storedSkips == 0);
    free(ctx);
}

/* AIO_WritePool_setAsync:
 * Nothing is asynchronous here. */
void AIO_WritePool_setAsync(WritePoolCtx_t *ctx, int async)
{
    (void)ctx;
    (void)async;
}

/* ***********************************
 *  ReadPool implementation
 *************************************/
static void AIO_ReadPool_releaseAllCompletedJobs(ReadPoolCtx_t *ctx)
{
    int i;
    for (i = 0; i < ctx->completedJobsCount; i++) {
        IOJob_t *job = (IOJob_t *)ctx->completedJobs[i];
        AIO_IOPool_releaseIoJob(job);
    }
    ctx->completedJobsCount = 0;
}

static void AIO_ReadPool_addJobToCompleted(IOJob_t *job)
{
    ReadPoolCtx_t *const ctx = (ReadPoolCtx_t *)job->ctx;
    assert(ctx->completedJobsCount < MAX_IO_JOBS);
    ctx->completedJobs[ctx->completedJobsCount++] = job;
}

/* AIO_ReadPool_findNextWaitingOffsetCompletedJob:
 * Looks through the completed jobs for a job matching the waitingOnOffset and returns it,
 * if job wasn't found returns NULL. */
static IOJob_t *AIO_ReadPool_findNextWaitingOffsetCompletedJob(ReadPoolCtx_t *ctx)
{
    IOJob_t *job = NULL;
    int i;
    /* This implementation goes through all completed jobs and looks for the one matching the next
     * offset. While not strictly needed for a single threaded reader implementation (as in such a
     * case we could expect reads to be completed in order) this implementation was chosen as it
     * better fits other asyncio interfaces (such as io_uring) that do not provide promises
     * regarding order of completion. */
    for (i = 0; i < ctx->completedJobsCount; i++) {
        job = (IOJob_t *)ctx->completedJobs[i];
        if (job->offset == ctx->waitingOnOffset) {
            ctx->completedJobs[i] = ctx->completedJobs[--ctx->completedJobsCount];
            return job;
        }
    }
    return NULL;
}

/* AIO_ReadPool_getNextCompletedJob:
 * Returns a completed IOJob_t for the next read in line based on waitingOnOffset and advances
 * waitingOnOffset. */
static IOJob_t *AIO_ReadPool_getNextCompletedJob(ReadPoolCtx_t *ctx)
{
    IOJob_t *job = AIO_ReadPool_findNextWaitingOffsetCompletedJob(ctx);

    if (job) {
        assert(job->offset == ctx->waitingOnOffset);
        ctx->waitingOnOffset += job->usedBufferSize;
    }

    return job;
}

/* AIO_ReadPool_executeReadJob:
 * Executes a read job. */
static Task<void> AIO_ReadPool_executeReadJob(void *opaque)
{
    IOJob_t *const job       = (IOJob_t *)opaque;
    ReadPoolCtx_t *const ctx = (ReadPoolCtx_t *)job->ctx;
    if (ctx->reachedEof) {
        job->usedBufferSize = 0;
        AIO_ReadPool_addJobToCompleted(job);
        co_return;
    }
    job->usedBufferSize = co_await zstd_fread(job->buffer, 1, job->bufferSize, job->file);
    if (job->usedBufferSize < job->bufferSize) {
        if (b_ferror(job->file)) {
            EXM_ERROR(37, "Read error");
        } else if (b_feof(job->file)) {
            ctx->reachedEof = 1;
        } else {
            EXM_ERROR(37, "Unexpected short read");
        }
    }
    AIO_ReadPool_addJobToCompleted(job);
}

static Task<void> AIO_ReadPool_enqueueRead(ReadPoolCtx_t *ctx)
{
    IOJob_t *const job = AIO_IOPool_acquireJob(&ctx->base);
    if (job == NULL)
        co_return;
    job->offset = ctx->nextReadOffset;
    ctx->nextReadOffset += job->bufferSize;
    co_await AIO_IOPool_enqueueJob(job);
}

static Task<void> AIO_ReadPool_startReading(ReadPoolCtx_t *ctx)
{
    while (ctx->base.availableJobsCount) {
        co_await AIO_ReadPool_enqueueRead(ctx);
    }
}

/* AIO_ReadPool_setFile:
 * Sets the source file for future read in the pool. Initiates reading immediately if file is not
 * NULL. */
Task<void> AIO_ReadPool_setFile(ReadPoolCtx_t *ctx, FILE *file)
{
    assert(ctx != NULL);
    AIO_ReadPool_releaseAllCompletedJobs(ctx);
    if (ctx->currentJobHeld) {
        AIO_IOPool_releaseIoJob((IOJob_t *)ctx->currentJobHeld);
        ctx->currentJobHeld = NULL;
    }
    AIO_IOPool_setFile(&ctx->base, file);
    ctx->nextReadOffset  = 0;
    ctx->waitingOnOffset = 0;
    ctx->srcBuffer       = ctx->coalesceBuffer;
    ctx->srcBufferLoaded = 0;
    ctx->reachedEof      = 0;
    if (file != NULL)
        co_await AIO_ReadPool_startReading(ctx);
}

/* AIO_ReadPool_create:
 * Allocates and sets and a new readPool including its included jobs.
 * bufferSize should be set to the maximal buffer we want to read at a time, will also be used
 * as our basic read size. */
ReadPoolCtx_t *AIO_ReadPool_create(const FIO_prefs_t *prefs, size_t bufferSize)
{
    ReadPoolCtx_t *const ctx = (ReadPoolCtx_t *)malloc(sizeof(ReadPoolCtx_t));
    if (!ctx) {
        EXM_ERROR(100, "Allocation error : not enough memory");
        return NULL;
    }
    AIO_IOPool_init(&ctx->base, prefs, AIO_ReadPool_executeReadJob, bufferSize);

    ctx->coalesceBuffer = (U8 *)malloc(bufferSize * 2);
    if (!ctx->coalesceBuffer) {
        EXM_ERROR(100, "Allocation error : not enough memory");
        free(ctx);
        return NULL;
    }
    ctx->srcBuffer          = ctx->coalesceBuffer;
    ctx->srcBufferLoaded    = 0;
    ctx->completedJobsCount = 0;
    ctx->currentJobHeld     = NULL;

    return ctx;
}

/* AIO_ReadPool_free:
 * Frees and releases a readPool and its resources. Closes source file. */
Task<void> AIO_ReadPool_free(ReadPoolCtx_t *ctx)
{
    if (ctx == NULL)
        co_return;
    if (AIO_ReadPool_getFile(ctx))
        co_await AIO_ReadPool_closeFile(ctx);
    AIO_IOPool_destroy(&ctx->base);
    free(ctx->coalesceBuffer);
    free(ctx);
}

/* AIO_ReadPool_consumeBytes:
 * Consumes byes from srcBuffer's beginning and updates srcBufferLoaded accordingly. */
void AIO_ReadPool_consumeBytes(ReadPoolCtx_t *ctx, size_t n)
{
    assert(n <= ctx->srcBufferLoaded);
    ctx->srcBufferLoaded -= n;
    ctx->srcBuffer += n;
}

/* AIO_ReadPool_releaseCurrentlyHeldAndGetNext:
 * Release the current held job and get the next one, returns NULL if no next job available. */
static Task<IOJob_t *> AIO_ReadPool_releaseCurrentHeldAndGetNext(ReadPoolCtx_t *ctx)
{
    if (ctx->currentJobHeld) {
        AIO_IOPool_releaseIoJob((IOJob_t *)ctx->currentJobHeld);
        ctx->currentJobHeld = NULL;
        co_await AIO_ReadPool_enqueueRead(ctx);
    }
    ctx->currentJobHeld = AIO_ReadPool_getNextCompletedJob(ctx);
    co_return (IOJob_t *) ctx->currentJobHeld;
}

/* AIO_ReadPool_fillBuffer:
 * Tries to fill the buffer with at least n or jobBufferSize bytes (whichever is smaller).
 * Returns if srcBuffer has at least the expected number of bytes loaded or if we've reached the end
 * of the file. Return value is the number of bytes added to the buffer. Note that srcBuffer might
 * have up to 2 times jobBufferSize bytes. */
Task<size_t> AIO_ReadPool_fillBuffer(ReadPoolCtx_t *ctx, size_t n)
{
    IOJob_t *job;
    int useCoalesce = 0;
    if (n > ctx->base.jobBufferSize)
        n = ctx->base.jobBufferSize;

    /* We are good, don't read anything */
    if (ctx->srcBufferLoaded >= n)
        co_return 0;

    /* We still have bytes loaded, but not enough to satisfy caller. We need to get the next job
     * and coalesce the remaining bytes with the next job's buffer */
    if (ctx->srcBufferLoaded > 0) {
        useCoalesce = 1;
        memcpy(ctx->coalesceBuffer, ctx->srcBuffer, ctx->srcBufferLoaded);
        ctx->srcBuffer = ctx->coalesceBuffer;
    }

    /* Read the next chunk */
    job = co_await AIO_ReadPool_releaseCurrentHeldAndGetNext(ctx);
    if (!job)
        co_return 0;
    if (useCoalesce) {
        assert(ctx->srcBufferLoaded + job->usedBufferSize <= 2 * ctx->base.jobBufferSize);
        memcpy(ctx->coalesceBuffer + ctx->srcBufferLoaded, job->buffer, job->usedBufferSize);
        ctx->srcBufferLoaded += job->usedBufferSize;
    } else {
        ctx->srcBuffer       = (U8 *)job->buffer;
        ctx->srcBufferLoaded = job->usedBufferSize;
    }
    co_return job->usedBufferSize;
}

/* AIO_ReadPool_consumeAndRefill:
 * Consumes the current buffer and refills it with bufferSize bytes. */
Task<size_t> AIO_ReadPool_consumeAndRefill(ReadPoolCtx_t *ctx)
{
    AIO_ReadPool_consumeBytes(ctx, ctx->srcBufferLoaded);
    co_return co_await AIO_ReadPool_fillBuffer(ctx, ctx->base.jobBufferSize);
}

/* AIO_ReadPool_getFile:
 * Returns the current file set for the read pool. */
FILE *AIO_ReadPool_getFile(const ReadPoolCtx_t *ctx)
{
    return AIO_IOPool_getFile(&ctx->base);
}

/* AIO_ReadPool_closeFile:
 * Closes the current set file and resets state. */
Task<int> AIO_ReadPool_closeFile(ReadPoolCtx_t *ctx)
{
    FILE *const file = AIO_ReadPool_getFile(ctx);
    co_await AIO_ReadPool_setFile(ctx, NULL);
    co_return co_await b_fclose(file);
}

/* AIO_ReadPool_setAsync:
 * Nothing is asynchronous here. */
void AIO_ReadPool_setAsync(ReadPoolCtx_t *ctx, int async)
{
    (void)ctx;
    (void)async;
}
