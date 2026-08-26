// crypt.cpp — crypt.c, by Info-ZIP. Traditional PKZIP encryption.
//
// The three keys and the pseudo-random sequence are upstream's, unchanged.
// Two things are not: the random header is drawn with proc_random(), which is
// crypto.getRandomValues and better than the srand() upstream apologises for
// in a comment; and the write is a coroutine, because bfwrite is.

#include "crypt.h"

#include "crc32.h"
#include "proc/rt.h"
#include "ttyio.h"
#include "zip.h"

#define CRY_CRC_TAB CRC_32_TAB

local z_uint4 keys[3]; // keys defining the pseudo-random sequence

// Return the next byte in the pseudo-random sequence
int decrypt_byte(void)
{
    unsigned temp; /* POTENTIAL BUG:  temp*(temp^1) may overflow in an
                    * unpredictable manner on 16-bit systems; not a problem
                    * with any known compiler so far, though */

    temp = ((unsigned)keys[2] & 0xffff) | 2;
    return (int)(((temp * (temp ^ 1)) >> 8) & 0xff);
}

// Update the encryption keys with the next byte of plain text
int update_keys(int c)
{
    keys[0] = CRC32(keys[0], c, CRY_CRC_TAB);
    keys[1] = (keys[1] + (keys[0] & 0xff)) * 134775813L + 1;
    {
        int keyshift = (int)(keys[1] >> 24);
        keys[2]      = CRC32(keys[2], keyshift, CRY_CRC_TAB);
    }
    return c;
}

// Initialize the encryption keys and the random header according to the given
// password.
void init_keys(ZCONST char *passwd)
{
    keys[0] = 305419896L;
    keys[1] = 591751049L;
    keys[2] = 878082192L;
    while (*passwd != '\0') {
        update_keys((int)*passwd);
        passwd++;
    }
}

// Write encryption header to file zfile using the password passwd and the
// cyclic redundancy check crc.
Task<void> crypthead(ZCONST char *passwd, ulg crc)
{
    int n;                     // index in random header
    int t;                     // temporary
    int c;                     // random byte
    uch header[RAND_HEAD_LEN]; // random header

    // First generate RAND_HEAD_LEN-2 random bytes. Upstream encrypted the
    // output of rand() "to get less predictability, since rand() is often
    // poorly implemented"; proc_random() is the host's own generator, so the
    // bytes are taken as they come.
    init_keys(passwd);
    for (n = 0; n < RAND_HEAD_LEN - 2; n++) {
        c         = (int)(proc_random() & 0xff);
        header[n] = (uch)zencode(c, t);
    }
    // Encrypt random header (last two bytes is high word of crc)
    init_keys(passwd);
    for (n = 0; n < RAND_HEAD_LEN - 2; n++) {
        header[n] = (uch)zencode(header[n], t);
    }
    header[RAND_HEAD_LEN - 2] = (uch)zencode((int)(crc >> 16) & 0xff, t);
    header[RAND_HEAD_LEN - 1] = (uch)zencode((int)(crc >> 24) & 0xff, t);
    co_await bfwrite(header, 1, RAND_HEAD_LEN, BFWRITE_DATA);
}

// If requested, encrypt the data in buf, and in any case write it out. Returns
// what bfwrite returns.
//
// A bug has been found when encrypting large files that don't compress. See
// trees.c for the details and the fix.
Task<usize> zfwrite_crypt(zvoid *buf, extent item_size, extent nb)
{
    int t; // temporary

    if (key != (char *)NULL) { // key is the global password pointer
        ulg size;              // buffer size
        char *p = (char *)buf; // steps through buffer

        // Encrypt data in buffer
        for (size = item_size * (ulg)nb; size != 0; p++, size--) {
            *p = (char)zencode(*p, t);
        }
    }
    // Write the buffer out
    co_return co_await bfwrite(buf, item_size, nb, BFWRITE_DATA);
}

// ------------------------------------------------- what zipcloak asks for
//
// Upstream built these two under -DUTIL. Here they live beside the rest of
// crypt.c and --gc-sections keeps them out of zip's own binary.

Task<int> zipcloak(struct zlist far *z, ZCONST char *passwd)
{
    int c;                    // input byte
    int res;                  // result code
    zoff_t n;                 // holds offset and counts size
    int t;                    // temporary
    struct zlist far *localz; // local header
    uch buf[1024];            // write buffer
    int b;                    // bytes in buffer

    // Set encrypted bit, clear extended local header bit and write local
    // header to output file
    if ((n = (zoff_t) co_await zftello(y)) == (zoff_t)-1L)
        co_return ZE_TEMP;

    // assume this archive is one disk and the file is open

    // read the local header
    res = co_await readlocal(&localz, z);

    // update disk and offset
    z->dsk = 0;
    z->off = n;

    // Set encryption and unset any extended local header
    z->flg |= 1, z->flg &= ~8;
    localz->lflg |= 1, localz->lflg &= ~8;

    // Add size of encryption header
    localz->siz += RAND_HEAD_LEN;
    z->siz = localz->siz;

    // Put the local header
    if ((res = co_await putlocal(localz, PUTLOCAL_WRITE)) != ZE_OK)
        co_return res;

    // Initialize keys with password and write random header
    co_await crypthead(passwd, localz->crc);

    // Encrypt data
    b = 0;
    for (n = z->siz - RAND_HEAD_LEN; n; n--) {
        if ((c = co_await zfgetc(in_file)) == EOF) {
            co_return zferror(in_file) ? ZE_READ : ZE_EOF;
        }
        buf[b] = (uch)zencode(c, t);
        b++;
        if (b >= 1024) {
            // write the buffer
            co_await bfwrite(buf, 1, b, BFWRITE_DATA);
            b = 0;
        }
    }
    if (b) {
        // write the buffer
        co_await bfwrite(buf, 1, b, BFWRITE_DATA);
        b = 0;
    }

    // Since we seek to the start of each local header can skip
    // reading any extended local header
    // if ((flag & 8) != 0 && zfseeko(in_file, 16L, SEEK_CUR)) {
    // co_return ferror(in_file) ? ZE_READ : ZE_EOF;
    // }
    // if (fflush(y) == EOF) co_return ZE_TEMP;

    // Update number of bytes written to output file
    tempzn += (4 + LOCHEAD) + localz->nam + localz->ext + localz->siz;

    // Free local header
    if (localz->ext)
        free(localz->extra);
    if (localz->nam)
        free(localz->iname);
    if (localz->nam)
        free(localz->name);
    if (localz->uname)
        free(localz->uname);
    free(localz);

    co_return ZE_OK;
}

Task<int> zipbare(struct zlist far *z, ZCONST char *passwd)
{
    int c1; // last input byte
    // all file offset and size now zoff_t - 8/28/04 EG
    zoff_t size;              // size of input data
    struct zlist far *localz; // local header
    uch buf[1024];            // write buffer
    int b;                    // bytes in buffer
    zoff_t n;
    int r;   // size of encryption header
    int res; // co_return code

    // Save position
    if ((n = (zoff_t) co_await zftello(y)) == (zoff_t)-1L)
        co_return ZE_TEMP;

    // Read local header
    res = co_await readlocal(&localz, z);

    // Update disk and offset
    z->dsk = 0;
    z->off = n;

    // Initialize keys with password
    init_keys(passwd);

    // Decrypt encryption header, save last two bytes
    c1 = 0;
    for (r = RAND_HEAD_LEN; r; r--) {
        if ((c1 = co_await zfgetc(in_file)) == EOF) {
            co_return zferror(in_file) ? ZE_READ : ZE_EOF;
        }
        Trace((stdout, " (%02x)", c1));
        zdecode(c1);
        Trace((stdout, " %02x", c1));
    }
    Trace((stdout, "\n"));

    // If last two bytes of header don't match crc (or file time in the
    // case of an extended local header), back up and just copy. For
    // pkzip 2.0, the check has been reduced to one byte only.
    if ((ush)c1 != (z->flg & 8 ? (ush)z->tim >> 8 : (ush)(z->crc >> 24))) {
        if (co_await zfseeko(in_file, n, SEEK_SET)) {
            co_return zferror(in_file) ? ZE_READ : ZE_EOF;
        }
        if ((res = co_await zipcopy(z)) != ZE_OK) {
            ziperr(res, "was copying an entry");
        }
        co_return ZE_MISS;
    }

    z->siz -= RAND_HEAD_LEN;
    localz->siz = z->siz;

    localz->flg = z->flg &= ~9;
    z->lflg     = localz->lflg &= ~9;

    if ((res = co_await putlocal(localz, PUTLOCAL_WRITE)) != ZE_OK)
        co_return res;

    // Decrypt data
    b = 0;
    for (size = z->siz; size; size--) {
        if ((c1 = co_await zfgetc(in_file)) == EOF) {
            co_return zferror(in_file) ? ZE_READ : ZE_EOF;
        }
        zdecode(c1);
        buf[b] = c1;
        b++;
        if (b >= 1024) {
            // write the buffer
            co_await bfwrite(buf, 1, b, BFWRITE_DATA);
            b = 0;
        }
    }
    if (b) {
        // write the buffer
        co_await bfwrite(buf, 1, b, BFWRITE_DATA);
        b = 0;
    }
    // Since we seek to the start of each local header can skip
    // reading any extended local header

    // Update number of bytes written to output file
    tempzn += (4 + LOCHEAD) + localz->nam + localz->ext + localz->siz;

    // Free local header
    if (localz->ext)
        free(localz->extra);
    if (localz->nam)
        free(localz->iname);
    if (localz->nam)
        free(localz->name);
    if (localz->uname)
        free(localz->uname);
    free(localz);

    co_return ZE_OK;
}
