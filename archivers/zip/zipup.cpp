// zipup.cpp — zipup.c, by Mark Adler. One entry: read it, compress it, and
// write it into the archive.
//
// This is where the port's coroutine boundary sits. Everything from zipup()
// down reaches a stream, so it is Task-returning; flush_outbuf, which
// trees.cpp calls from plain code, only appends, and defl_drain() writes what
// it left. read_buf keeps its indirection and its type gains a Task.

#include "crc32.h"
#include "kernel/alloc.h"
#include "revision.h"
#include "zip.h"

// Local functions
local int suffixes OF((char *, char *));
local Task<unsigned> file_read OF((char *buf, unsigned size));

// zip64 support 08/29/2003 R.Nausedat
local Task<zoff_t> filecompress OF((struct zlist far * z_entry, int *cmpr_method));

// Deflate "internal" global data (currently not in zip.h)
extern ulg window_size; // size of said window

Task<unsigned>(*read_buf) OF((char *buf, unsigned size)) = file_read;
// Current input function. Set to mem_read for in-memory compression

// Local data
local ulg crc;                // crc on uncompressed file data
local ftype ifile;            // file to compress
local char file_outbuf[1024]; // output buffer for compression to file

local zoff_t zisize; // input file size. global only for debugging
// If file_read detects binary it sets this flag - 12/16/04 EG
local int file_binary       = 0; // first buf
local int file_binary_final = 0; // for bzip2 for entire file.  assume text until find binary

// moved check to function 3/14/05 EG
Task<int> is_seekable(FILE *y)
{
    zoff_t pos;

    pos = co_await zftello(y);
    if (co_await zfseeko(y, pos, SEEK_SET)) {
        co_return 0;
    }

    co_return 1;
}

int percent(uzoff_t n, uzoff_t m)
{
    zoff_t p;

    // 2004-12-01 SMS.
    // Changed to do big-n test only for small zoff_t.
    // Changed big-n arithmetic to accomodate apparently negative values
    // when a small zoff_t value exceeds 2G.
    // Increased the reduction divisor from 256 to 512 to avoid the sign bit
    // in a reduced intermediate, allowing signed arithmetic for the final
    // result (which is no longer artificially limited to non-negative
    // values).
    // Note that right shifts must be on unsigned values to avoid undesired
    // sign extension.

    // Handle n = 0 case and account for int maybe being 16-bit.  12/28/2004 EG

#define PC_MAX_SAFE 0x007fffffL // 9 clear bits at high end.
#define PC_MAX_RND  0xffffff00L // 8 clear bits at low end.

    if (sizeof(uzoff_t) < 8) // Don't fiddle with big zoff_t.
    {
        if ((ulg)n > PC_MAX_SAFE) // Reduce large values.  (n > m)
        {
            if ((ulg)n < PC_MAX_RND)       // Divide n by 512 with rounding,
                n = ((ulg)n + 0x100) >> 9; // if boost won't overflow.
            else                           // Otherwise, use max value.
                n = PC_MAX_SAFE;

            if ((ulg)m < PC_MAX_RND)       // Divide m by 512 with rounding,
                m = ((ulg)m + 0x100) >> 9; // if boost won't overflow.
            else                           // Otherwise, use max value.
                m = PC_MAX_SAFE;
        }
    }
    if (n != 0)
        p = ((200 * ((zoff_t)n - (zoff_t)m) / (zoff_t)n) + 1) / 2;
    else
        p = 0;
    return (int)p; // Return (rounded) % reduction.
}

local int suffixes(char *a, char *s)
{
    int m;   // true if suffix matches so far
    char *p; // pointer into special
    char *q; // pointer into name a

    m = 1;
    q = a + strlen(a) - 1;
    for (p = s + strlen(s) - 1; p >= s; p--)
        if (*p == ':' || *p == ';') {
            if (m)
                return 1;
            else {
                m = 1;
                q = a + strlen(a) - 1;
            }
        } else {
            m = m && q >= a && case_map(*p) == case_map(*q);
            q--;
        }
    return m;
}

// Note: a zip "entry" includes a local header (which includes the file
// name), an encryption header if encrypting, the compressed data
// and possibly an extended local header.

Task<int> zipup(struct zlist far *z)
{
    iztimes f_utim; // UNIX GMT timestamps, filled by filetime()
    ulg tim;        // time returned by filetime()
    ulg a = 0L;     // attributes returned by filetime()
    char *b;        // malloc'ed file buffer
    extent k = 0;   // result of zread
    int l    = 0;   // true if this file is a symbolic link
    int m;          // method for this entry

    zoff_t o = 0, p;       // offsets in zip file
    zoff_t q = (zoff_t)-3; // size returned by filetime
    uzoff_t uq;            // unsigned q
    zoff_t s = 0;          // size of compressed data

    int r;            // temporary variable
    int isdir;        // set for a directory name
    int set_type = 0; // set if file type (ascii/binary) unknown
    zoff_t last_o;    // used to detect wrap around

    ush tempext      = 0; // temp copies of extra fields
    ush tempcext     = 0;
    char *tempextra  = NULL;
    char *tempcextra = NULL;

    z->nam = strlen(z->iname);
    isdir  = z->iname[z->nam - 1] == (char)0x2f; // ascii[(unsigned)('/')]

    file_binary       = -1; // not set, set after first read
    file_binary_final = 0;  // not set, set after first read

    tim = co_await filetime(z->name, &a, &q, &f_utim);
    if (tim == 0 || q == (zoff_t)-3)
        co_return ZE_OPEN;

    // q is set to -1 if the input file is a device, -2 for a volume label
    if (q == (zoff_t)-2) {
        isdir = 1;
        q     = 0;
    } else if (isdir != ((a & MSDOS_DIR_ATTR) != 0)) {
        // don't overwrite a directory with a file and vice-versa
        co_return ZE_MISS;
    }
    // reset dot_count for each file
    if (!display_globaldots)
        dot_count = -1;

    // display uncompressed size
    uq = ((uzoff_t)q > (uzoff_t)-3) ? 0 : (uzoff_t)q;
    if (noisy && display_usize) {
        co_await zfprintf(mesg, " (");
        co_await DisplayNumString(mesg, uq);
        co_await zfprintf(mesg, ")");
        mesg_line_started = 1;
        co_await zfflush(mesg);
    }
    if (logall && display_usize) {
        co_await zfprintf(logfile, " (");
        co_await DisplayNumString(logfile, uq);
        co_await zfprintf(logfile, ")");
        logfile_line_started = 1;
        co_await zfflush(logfile);
    }

    // initial z->len so if error later have something
    z->len = uq;

    z->att = (ush)UNKNOWN; // will be changed later
    z->atx = 0;            // may be changed by set_extra_field()

    // Free the old extra fields which are probably obsolete
    // Should probably read these and keep any we don't update.  12/30/04 EG
    if (extra_fields == 2) {
        // If keeping extra fields, make copy before clearing for set_extra_field()
        // A better approach is to modify the port code, but maybe later
        if (z->ext) {
            if ((tempextra = (char *)malloc(z->ext)) == NULL) {
                ZIPERR(ZE_MEM, "extra fields copy");
            }
            memcpy(tempextra, z->extra, z->ext);
            tempext = z->ext;
        }
        if (z->cext) {
            if ((tempcextra = (char *)malloc(z->cext)) == NULL) {
                ZIPERR(ZE_MEM, "extra fields copy");
            }
            memcpy(tempcextra, z->cextra, z->cext);
            tempcext = z->cext;
        }
    }
    if (z->ext) {
        free((zvoid *)(z->extra));
    }
    if (z->cext && z->extra != z->cextra) {
        free((zvoid *)(z->cextra));
    }
    z->extra = z->cextra = NULL;
    z->ext = z->cext = 0;

    window_size = 0L;

    // Select method based on the suffix and the global method
    m = special != NULL && suffixes(z->name, special) ? STORE : method;

    // For now force deflate if using descriptors.  Instead zip and unzip
    // could check bytes read against compressed size in each data descriptor
    // found and skip over any that don't match.  This is how at least one
    // other zipper does it.  To be added later.  Until then it
    // probably doesn't hurt to force deflation when streaming.  12/30/04 EG

    // Now is a good time.  For now allow storing for testing.  12/16/05 EG
    // By release need to force deflation based on reports some inflate
    // streamed data to find the end of the data
    // Need to handle bzip2

    // Open file to zip up unless it is stdin
    if (strcmp(z->name, "-") == 0) {
        ifile  = (ftype)zstdin;
        z->tim = tim;
    } else {
        if (extra_fields) {
            // create extra field and change z->att and z->atx if desired
            co_await set_extra_field(z, &f_utim);

            // For now allow store for testing
        }
        l = issymlnk(a);
        if (l) {
            ifile = fbad;
            m     = STORE;
        } else if (isdir) { // directory
            ifile = fbad;
            m     = STORE;
            q     = 0;
        } else {
            if ((ifile = co_await zopen(z->name, fhow)) == fbad)
                co_return ZE_OPEN;
        }

        z->tim = tim;

    } // strcmp(z->name, "-") == 0

    if (extra_fields == 2) {
        unsigned len;
        char *p;

        // step through old extra fields and copy over any not already
        // in new extra fields
        p = copy_nondup_extra_fields(tempextra, tempext, z->extra, z->ext, &len);
        free(z->extra);
        z->ext   = len;
        z->extra = p;
        p        = copy_nondup_extra_fields(tempcextra, tempcext, z->cextra, z->cext, &len);
        free(z->cextra);
        z->cext   = len;
        z->cextra = p;

        if (tempext)
            free(tempextra);
        if (tempcext)
            free(tempcextra);
    }

    if (q == 0)
        m = STORE;
    if (m == BEST)
        m = DEFLATE;

    // Do not create STORED files with extended local headers if the
    // input size is not known, because such files could not be extracted.
    // So if the zip file is not seekable and the input file is not
    // on disk, obey the -0 option by forcing deflation with stored block.
    // Note however that using "zip -0" as filter is not very useful...
    // ??? to be done.

    // An alternative used by others is to allow storing but on reading do
    // a second check when a signature is found.  This is simply to check
    // the compressed size to the bytes read since the start of the file data.
    // If this is the right signature then the compressed size should match
    // the size of the compressed data to that point.  If not look for the
    // next signature.  We should do this.  12/31/04 EG
    //
    // For reading and testing we should do this, but should not write
    // stored streamed data unless for testing as finding the end of
    // streamed deflated data can be done by inflating.  6/26/06 EG

    // Fill in header information and write local header to zip file.
    // This header will later be re-written since compressed length and
    // crc are not yet known.

    // (Assume ext, cext, com, and zname already filled in.)
    z->vem = (ush)(dosify ? 20 : OS_CODE + Z_MAJORVER * 10 + Z_MINORVER);

    z->ver = (ush)(m == STORE ? 10 : 20); // Need PKUNZIP 2.0 except for store
    z->crc = 0;                           // to be updated later
    // Assume first that we will need an extended local header:
    if (isdir)
        // If dir then q = 0 and extended header not needed
        z->flg = 0;
    else
        z->flg = 8; // to be updated later
#if CRYPT
    if (!isdir && key != NULL) {
        z->flg |= 1;
        // Since we do not yet know the crc here, we pretend that the crc
        // is the modification time:
        z->crc = z->tim << 16;
        // More than pretend.  File is encrypted using crypt header with that.
    }
#endif // CRYPT
    z->lflg = z->flg;
    z->how  = (ush)m;                                 // may be changed later
    z->siz  = (zoff_t)(m == STORE && q >= 0 ? q : 0); // will be changed later
    z->len  = (zoff_t)(q != -1L ? q : 0);             // may be changed later
    if (z->att == (ush)UNKNOWN) {
        z->att   = BINARY; // set sensible value in header
        set_type = 1;
    }
    // Attributes from filetime(), flag bits from set_extra_field():
    z->atx = dosify ? a & 0xff : a | (z->atx & 0x0000ff00);

    if ((r = co_await putlocal(z, PUTLOCAL_WRITE)) != ZE_OK) {
        if (ifile != fbad)
            co_await zclose(ifile);
        co_return r;
    }

    // now get split information set by bfwrite()
    z->off = current_local_offset;

    // disk local header was written to
    z->dsk = current_local_disk;

    tempzn += 4 + LOCHEAD + z->nam + z->ext;

#if CRYPT
    if (!isdir && key != NULL) {
        crypthead(key, z->crc);
        z->siz += RAND_HEAD_LEN; // to be updated later
        tempzn += RAND_HEAD_LEN;
    }
#endif // CRYPT
    if (zferror(y)) {
        if (ifile != fbad)
            co_await zclose(ifile);
        ZIPERR(ZE_WRITE, "unexpected error on zip file");
    }

    last_o = o;
    o      = co_await zftello(y); // for debugging only, ftell can fail on pipes
    if (zferror(y))
        y->clear_err();

    if (o != -1 && last_o > o) {
        co_await zfprintf(mesg, "last %s o %s\n", zip_fzofft(last_o, NULL, NULL),
                          zip_fzofft(o, NULL, NULL));
        ZIPERR(ZE_BIG, "seek wrap - zip file too big to write");
    }

    // Write stored or deflated file to zip file
    zisize = 0L;
    crc    = CRCVAL_INITIAL;

    if (isdir) {
        // nothing to write
    } else if (m != STORE) {
        if (set_type)
            z->att = (ush)UNKNOWN;
        // ... is finally set in file compression routine
        {
            s = co_await filecompress(z, &m);
        }
        if (z->att == (ush)BINARY && translate_eol && file_binary) {
            if (translate_eol == 1)
                co_await zipwarn("has binary so -l ignored", "");
            else
                co_await zipwarn("has binary so -ll ignored", "");
        } else if (z->att == (ush)BINARY && translate_eol) {
            if (translate_eol == 1)
                co_await zipwarn("-l used on binary file - corrupted?", "");
            else
                co_await zipwarn("-ll used on binary file - corrupted?", "");
        }
    } else {
        if ((b = (char *)malloc(SBSZ)) == NULL)
            co_return ZE_MEM;

        if (l) {
            k = co_await rdsymlnk(z->name, b, SBSZ);
            // compute crc first because zfwrite will alter the buffer b points to !!
            crc = crc32(crc, (uch *)b, k);
            if (co_await zfwrite(b, 1, k) != k) {
                free((zvoid *)b);
                co_return ZE_TEMP;
            }
            zisize = k;

        } else {
            while ((k = co_await file_read(b, SBSZ)) > 0 && k != (extent)EOF) {
                if (co_await zfwrite(b, 1, k) != k) {
                    if (ifile != fbad)
                        co_await zclose(ifile);
                    free((zvoid *)b);
                    co_return ZE_TEMP;
                }
                if (!display_globaldots) {
                    if (dot_size > 0) {
                        // initial space
                        if (noisy && dot_count == -1) {
                            co_await zfputc(' ', mesg);
                            co_await zfflush(mesg);
                            dot_count++;
                        }
                        dot_count++;
                        if (dot_size <= (dot_count + 1) * SBSZ)
                            dot_count = 0;
                    }
                    if ((verbose || noisy) && dot_size && !dot_count) {
                        co_await zfputc('.', mesg);
                        co_await zfflush(mesg);
                        mesg_line_started = 1;
                    }
                }
            }
        }
        free((zvoid *)b);
        s = zisize;
    }
    // A ^C reaches the run where it parks, which is the read below; zread()
    // records ZE_ABORT and the entry ends here rather than being written out
    // as a short one. Upstream's handler ended the process from inside the
    // signal, which nothing here can do.
    if (zip_fatal != ZE_OK) {
        if (ifile != fbad)
            co_await zclose(ifile);
        co_return zip_fatal;
    }

    if (ifile != fbad && zerr(ifile)) {
        co_await zipwarn("could not read input file: ", z->oname);
    }
    if (ifile != fbad)
        co_await zclose(ifile);

    tempzn += s;
    p = tempzn; // save for future fseek()

    // Check input size (but not in VMS -- variable record lengths mess it up)
    // and not on MSDOS -- diet in TSR mode reports an incorrect file size)
    if (!translate_eol && q != -1L && zisize != q) {
        Trace((mesg, " i=%lu, q=%lu ", zisize, q));
        co_await zipwarn(" file size changed while zipping ", z->name);
    }

    if (isdir) {
        // A directory
        z->siz = 0;
        z->len = 0;
        z->how = STORE;
        z->ver = 10;
        // never encrypt directory so don't need extended local header
        z->flg &= ~8;
        z->lflg &= ~8;
    } else {
        // Try to rewrite the local header with correct information
        z->crc = crc;
        z->siz = s;
#if CRYPT
        if (!isdir && key != NULL)
            z->siz += RAND_HEAD_LEN;
#endif // CRYPT
        z->len = zisize;
        // if can seek back to local header
        if (use_descriptors || co_await zfseeko(y, z->off, SEEK_SET)) {
            if (z->how != (ush)m)
                error("can't rewrite method");
            if (m == STORE && q < 0)
                ZIPERR(ZE_PARMS, "zip -0 not supported for I/O on pipes or devices");
            if ((r = co_await putextended(z)) != ZE_OK)
                co_return r;
            // if Zip64 and not seekable then Zip64 data descriptor
            tempzn += (zip64_entry ? 24L : 16L);
            z->flg = z->lflg; // if z->flg modified by deflate
        } else {
            // ftell() not as useful across splits
            if (bytes_this_entry != (uzoff_t)(key ? s + 12 : s)) {
                co_await zfprintf(mesg, " s=%s, actual=%s ", zip_fzofft(s, NULL, NULL),
                                  zip_fzofft(bytes_this_entry, NULL, NULL));
                error("incorrect compressed size");
            }
            z->how = (ush)m;
            switch (m) {
            case STORE:
                z->ver = 10;
                break;
            // Need PKUNZIP 2.0 for DEFLATE
            case DEFLATE:
                z->ver = 20;
                break;
            }
            // The encryption header needs the crc, but we don't have it
            // for a new file.  The file time is used instead and the encryption
            // header then used to encrypt the data.  The AppNote standard only
            // can be applied to a file that the crc is known, so that means
            // either an existing entry in an archive or get the crc before
            // creating the encryption header and then encrypt the data.
            if ((z->flg & 1) == 0) {
                // not encrypting so don't need extended local header
                z->flg &= ~8;
            }
            // deflate may have set compression level bit markers in z->flg,
            // and we can't think of any reason central and local flags should
            // be different.
            z->lflg = z->flg;

            // If not using descriptors, back up and rewrite local header.
            if (split_method == 1 && current_local_file != y) {
                if (co_await zfseeko(current_local_file, z->off, SEEK_SET))
                    co_return ZE_READ;
            }

            // if local header in another split, putlocal will close it
            if ((r = co_await putlocal(z, PUTLOCAL_REWRITE)) != ZE_OK)
                co_return r;

            if (co_await zfseeko(y, bytes_this_split, SEEK_SET))
                co_return ZE_READ;

            if ((z->flg & 1) != 0) {
                // encrypted file, extended header still required
                if ((r = co_await putextended(z)) != ZE_OK)
                    co_return r;
                if (zip64_entry)
                    tempzn += 24L;
                else
                    tempzn += 16L;
            }
        }
    } // isdir
    // Free the local extra field which is no longer needed
    if (z->ext) {
        if (z->extra != z->cextra) {
            free((zvoid *)(z->extra));
            z->extra = NULL;
        }
        z->ext = 0;
    }

    // Display statistics
    if (noisy) {
        if (verbose) {
            co_await zfprintf(mesg, "\t(in=%s) (out=%s)", zip_fzofft(zisize, NULL, "u"),
                              zip_fzofft(s, NULL, "u"));
        }
        if (m == DEFLATE)
            co_await zfprintf(mesg, " (deflated %d%%)\n", percent(zisize, s));
        else
            co_await zfprintf(mesg, " (stored 0%%)\n");
        mesg_line_started = 0;
        co_await zfflush(mesg);
    }
    if (logall) {
        if (m == DEFLATE)
            co_await zfprintf(logfile, " (deflated %d%%)\n", percent(zisize, s));
        else
            co_await zfprintf(logfile, " (stored 0%%)\n");
        logfile_line_started = 0;
        co_await zfflush(logfile);
    }

    co_return ZE_OK;
}

local Task<unsigned> file_read(char *buf, unsigned size)
{
    unsigned len;
    char *b;
    zoff_t isize_prev; // Previous zisize.  Used for overflow check.

    if (translate_eol == 0) {
        len = co_await zread(ifile, buf, size);
        if (len == (unsigned)EOF || len == 0)
            co_return len;
    } else if (translate_eol == 1) {
        // translate_eol == 1
        // Transform LF to CR LF
        size >>= 1;
        b    = buf + size;
        size = len = co_await zread(ifile, b, size);
        if (len == (unsigned)EOF || len == 0)
            co_return len;

        // check buf for binary - 12/16/04
        if (file_binary == -1) {
            // first read
            file_binary = is_text_buf(b, size) ? 0 : 1;
        }

        if (file_binary != 1) {
            {
                do {
                    if ((*buf++ = *b++) == '\n')
                        *(buf - 1) = CR, *buf++ = LF, len++;
                } while (--size != 0);
            }
            buf -= len;
        } else { // do not translate binary
            memcpy(buf, b, size);
        }

    } else {
        // translate_eol == 2
        // Transform CR LF to LF and suppress final ^Z
        b    = buf;
        size = len = co_await zread(ifile, buf, size - 1);
        if (len == (unsigned)EOF || len == 0)
            co_return len;

        // check buf for binary - 12/16/04
        if (file_binary == -1) {
            // first read
            file_binary = is_text_buf(b, size) ? 0 : 1;
        }

        if (file_binary != 1) {
            buf[len] = '\n'; // I should check if next char is really a \n
            {
                do {
                    if ((*buf++ = *b++) == CR && *b == LF)
                        buf--, len--;
                } while (--size != 0);
            }
            if (len == 0) {
                co_await zread(ifile, buf, 1);
                len = 1; // keep single \r if EOF
            } else {
                buf -= len;
                if (buf[len - 1] == CTRLZ)
                    len--; // suppress final ^Z
            }
        }
    }
    crc = crc32(crc, (uch *)buf, len);
    // 2005-05-23 SMS.
    // Increment file size.  A small-file program reading a large file may
    // cause zisize to overflow, so complain (and abort) if it goes
    // negative or wraps around.  Awful things happen later otherwise.
    isize_prev = zisize;
    zisize += (ulg)len;
    if (zisize < isize_prev) {
        ZIPERR(ZE_BIG, "overflow in byte count");
    }
    co_return len;
}

// Flush the current output buffer. Upstream wrote it to the archive here;
// trees.cpp calls this from plain code, and a write is a co_await, so it goes
// into a String that defl_drain() empties at each block boundary. The sink
// therefore holds one block at most.
void flush_outbuf(char *o_buf, unsigned *o_idx)
{
    if (*o_idx != 0) {
        if (defl_sink == NULL || !defl_sink->append(Str(o_buf, *o_idx)))
            zip_fail(ZE_MEM, "deflate output");
    }
    *o_idx = 0;
}

// What flush_outbuf left, into the archive. Called once per deflate block.
Task<int> defl_drain(void)
{
    if (defl_sink == NULL || defl_sink->empty())
        co_return ZE_OK;
    usize n = defl_sink->size();
    if (co_await zfwrite(defl_sink->data(), 1, n) != n)
        ZIPERR(ZE_WRITE, "write error on zip file")
    defl_sink->clear();
    co_return ZE_OK;
}

// Return true if the zip file can be seeked. This is used to check if
// the local header can be re-rewritten. This function always returns
// true for in-memory compression.
// IN assertion: the local header has already been written (ftell() > 0).
int seekable()
{
    return fseekable(y);
}

// Compression to archive file.
local Task<zoff_t> filecompress(struct zlist far *z_entry, int *cmpr_method)
{
    // Where trees.cpp emits. Built on first use and never torn down: a String
    // has a destructor and a global may not, and the kernel drops the process.
    if (defl_sink == NULL) {
        defl_sink = heap_new<String>();
        if (defl_sink == NULL || !defl_sink->reserve(2 * CBSZ))
            ZIPERR(ZE_MEM, "deflate output")
    }
    defl_sink->clear();

    // Set the defaults for file compression.
    read_buf = file_read;

    // Initialize deflate's internals and execute file compression.
    bi_init(file_outbuf, sizeof(file_outbuf), TRUE);
    ct_init(&z_entry->att, cmpr_method);
    co_await lm_init(level, &z_entry->flg);
    co_return co_await deflate();
}
