/*
 * Disk and drum images: a file of 48-bit words, addressed by word.
 *
 * Copyright (c) 2026, Serge Vakulenko
 *
 * Upstream said this with fseek/fread/fwrite on a FILE * in the UNIT; here it
 * is what the machine asks for and the driver performs (machine.h).
 */
#ifndef BESM6_IMAGE_H
#define BESM6_IMAGE_H

typedef struct Image Image;

/* How an image came to be open; the caller reports it. */
enum {
    IMG_OPENED = 0, /* an existing image, read-write */
    IMG_RDONLY,     /* an existing image the store would not give up for writing */
    IMG_CREATED,    /* there was none, and `must_exist' did not insist */
};

/* `create' truncates.  NULL on failure, *why an SCPE_ code. */
Image *img_open(const char *path, int create, int must_exist, int roable, int *how, int *why);

/* Nonzero on a write that could not be completed. */
int img_close(Image *m);

/* Words transferred; short of `n' past the end, which is an unformatted zone
 * and not an error -- every caller reports it to the guest as its own. */
int img_read(Image *m, uint32 off, t_value *dst, int n);
int img_write(Image *m, uint32 off, const t_value *src, int n);

/* Appends: what formatting a fresh image is. */
int img_append(Image *m, const t_value *src, int n);

/* Sticky, as ferror() was. */
int img_error(Image *m);

/* Deletes one rejected after being created. */
void img_remove(const char *path);

#endif /* BESM6_IMAGE_H */
