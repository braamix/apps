/*
 * Disk and drum images: a file of 48-bit words, addressed by word.
 *
 * Copyright (c) 2026, Serge Vakulenko
 *
 * A zone is 8 system words then 1024 of user data, and every word is one
 * 8-byte little-endian record; a transfer is therefore "seek to a word offset,
 * move n words".  Upstream said that with fseek/fread/fwrite on a `FILE *' held
 * in the UNIT.  On Braam a read is a coroutine and stdio's blocking half does
 * not exist, so the four calls below are what the machine asks for and what the
 * driver loop performs -- see machine.h for how a transfer is deferred out of
 * the instruction stream.
 *
 * A short read is not an error: it is a zone the image has never been written
 * that far into, which every caller already reports to the guest as its own
 * "unformatted" failure rather than as an I/O error.
 */
#ifndef BESM6_IMAGE_H
#define BESM6_IMAGE_H

typedef struct Image Image;

/* How an image came to be open, which the caller reports to the operator. */
enum {
    IMG_OPENED = 0, /* an existing image, read-write */
    IMG_RDONLY,     /* an existing image the store would not give up for writing */
    IMG_CREATED,    /* there was none, and `must_exist' did not insist */
};

/* `create' opens a fresh empty image, truncating one that is there.  NULL on
 * failure, with *why an SCPE_ code and errno as the store left it. */
Image *img_open(const char *path, int create, int must_exist, int roable, int *how, int *why);

/* Flushes and releases.  Nonzero on a write that could not be completed. */
int img_close(Image *m);

/* Words transferred, which is short of `n` at the end of the image. */
int img_read(Image *m, uint32 off, t_value *dst, int n);
int img_write(Image *m, uint32 off, const t_value *src, int n);

/* Appends at the end, which is what formatting a fresh image is. */
int img_append(Image *m, const t_value *src, int n);

/* Sticky, as ferror() was: set by a failed transfer and cleared by nothing. */
int img_error(Image *m);

/* Deletes an image that was rejected after being created. */
void img_remove(const char *path);

#endif /* BESM6_IMAGE_H */
