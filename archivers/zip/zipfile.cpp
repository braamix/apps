// zipfile.cpp — zipfile.c, by Mark Adler. The archive's headers.
//
// The write half: the local header, the data descriptor, the central header
// and the end record, with the Zip64 and Unicode-path extra fields they carry.
// Everything that reaches the archive goes through bfwrite, so put* are
// coroutines; the append_*_to_mem helpers build in memory and stay plain,
// which is why an allocation failure there records itself in zip_fatal for the
// caller to notice rather than ending the process where it stands.

#include "crc32.h"
#include "revision.h"
#include "zip.h"

// XXX start of zipfile.h
// Macros for converting integers in little-endian to machine format
#define SH(a)  ((ush)(((ush)(uch)(a)[0]) | (((ush)(uch)(a)[1]) << 8)))
#define LG(a)  ((ulg)SH(a) | ((ulg)SH((a) + 2) << 16))
#define LLG(a) ((zoff_t)LG(a) | ((zoff_t)LG((a) + 4) << 32))

// Macros for writing machine integers to little-endian format
#define PUTSH(a, f)                               \
    {                                             \
        co_await zfputc((char)((a) & 0xff), (f)); \
        co_await zfputc((char)((a) >> 8), (f));   \
    }
#define PUTLG(a, f) { PUTSH((a) & 0xffff, (f)) PUTSH((a) >> 16, (f)) }

#define PUTLLG(a, f) { PUTLG((a) & 0xffffffff, (f)) PUTLG((a) >> 32, (f)) }

// -- Structure of a ZIP file --

// Signatures for zip file information headers
#define LOCSIG    0x04034b50L
#define CENSIG    0x02014b50L
#define ENDSIG    0x06054b50L
#define EXTLOCSIG 0x08074b50L

// Offsets of values in headers
// local header
#define LOCVER 0  // version needed to extract
#define LOCFLG 2  // encrypt, deflate flags
#define LOCHOW 4  // compression method
#define LOCTIM 6  // last modified file time, DOS format
#define LOCDAT 8  // last modified file date, DOS format
#define LOCCRC 10 // uncompressed crc-32 for file
#define LOCSIZ 14 // compressed size in zip file
#define LOCLEN 18 // uncompressed size
#define LOCNAM 22 // length of filename
#define LOCEXT 24 // length of extra field

// extended local header (data descriptor) following file data (if bit 3 set)
// if Zip64 then all are 8 byte and not below - 11/1/03 EG
#define EXTCRC 0 // uncompressed crc-32 for file
#define EXTSIZ 4 // compressed size in zip file
#define EXTLEN 8 // uncompressed size

// central directory header
#define CENVEM 0  // version made by
#define CENVER 2  // version needed to extract
#define CENFLG 4  // encrypt, deflate flags
#define CENHOW 6  // compression method
#define CENTIM 8  // last modified file time, DOS format
#define CENDAT 10 // last modified file date, DOS format
#define CENCRC 12 // uncompressed crc-32 for file
#define CENSIZ 16 // compressed size in zip file
#define CENLEN 20 // uncompressed size
#define CENNAM 24 // length of filename
#define CENEXT 26 // length of extra field
#define CENCOM 28 // file comment length
#define CENDSK 30 // disk number start
#define CENATT 32 // internal file attributes
#define CENATX 34 // external file attributes
#define CENOFF 38 // relative offset of local header

// end of central directory record
#define ENDDSK 0  // number of this disk
#define ENDBEG 2  // number of the starting disk
#define ENDSUB 4  // entries on this disk
#define ENDTOT 6  // total number of entries
#define ENDSIZ 8  // size of entire central directory
#define ENDOFF 12 // offset of central on starting disk
#define ENDCOM 16 // length of zip file comment

// zip64 support 08/31/2003 R.Nausedat

// EOCDL_SIG used to detect Zip64 archive
#define ZIP64_EOCDL_SIG 0x07064b50
// EOCDL size is used in the empty archive check
#define ZIP64_EOCDL_OFS_SIZE 20

#define ZIP_UWORD16_MAX    0xFFFF     // border value
#define ZIP_UWORD32_MAX    0xFFFFFFFF // border value
#define ZIP_EF_HEADER_SIZE 4          // size of pre-header of extra fields

#define ZIP64_EXTCRC               0  // uncompressed crc-32 for file
#define ZIP64_EXTSIZ               4  // compressed size in zip file
#define ZIP64_EXTLEN               12 // uncompressed size
#define ZIP64_EOCD_SIG             0x06064b50
#define ZIP64_EOCD_OFS_SIZE        40
#define ZIP64_EOCD_OFS_CD_START    48
#define ZIP64_EOCDL_OFS_SIZE       20
#define ZIP64_EOCDL_OFS_EOCD_START 8
#define ZIP64_EOCDL_OFS_TOTALDISKS 16
#define ZIP64_MIN_VER              45 // min version to set in the CD extra records
#define ZIP64_CENTRAL_DIR_TAIL_SIZE \
    (56 - 8 - 4) // size of zip64 central dir tail, minus sig and size field bytes
#define ZIP64_CENTRAL_DIR_TAIL_SIG     0x06064B50L // zip64 central dir tail signature
#define ZIP64_CENTRAL_DIR_TAIL_END_SIG 0x07064B50L // zip64 end of cen dir locator signature
#define ZIP64_LARGE_FILE_HEAD_SIZE     32          // total size of zip64 extra field
#define ZIP64_EF_TAG                   0x0001      // ID for zip64 extra field
#define ZIP64_EFIELD_OFS_OSIZE ZIP_EF_HEADER_SIZE // zip64 extra field: offset to original file size
#define ZIP64_EFIELD_OFS_CSIZE \
    (ZIP64_EFIELD_OFS_OSIZE + 8) // zip64 extra field: offset to compressed file size
#define ZIP64_EFIELD_OFS_OFS \
    (ZIP64_EFIELD_OFS_CSIZE + 8) // zip64 extra field: offset to offset in archive
#define ZIP64_EFIELD_OFS_DISK \
    (ZIP64_EFIELD_OFS_OFS + 8) // zip64 extra field: offset to start disk #
// --------------------------------------------------------------------------------------------------------------------------
local int add_central_zip64_extra_field OF((struct zlist far *));
local int add_local_zip64_extra_field OF((struct zlist far *));
#define UTF8_PATH_EF_TAG 0x7075 // ID for Unicode path (up) extra field
local int add_Unicode_Path_local_extra_field OF((struct zlist far *));
local int add_Unicode_Path_cen_extra_field OF((struct zlist far *));

// New General Purpose Bit Flag bit 11 flags when entry path and
// comment are in UTF-8
#define UTF8_BIT (1 << 11)

// moved out of ZIP64_SUPPORT - 2/6/2005 EG
local void write_ushort_to_mem OF((ush, char *)); // little endian conversions
local void write_ulong_to_mem OF((ulg, char *));
local void write_int64_to_mem OF((uzoff_t, char *));
local void write_string_to_mem OF((char *, char *));

// added these self allocators - 2/6/2005 EG
local void append_ushort_to_mem OF((ush, char **, extent *, extent *));
local void append_ulong_to_mem OF((ulg, char **, extent *, extent *));
local void append_int64_to_mem OF((uzoff_t, char **, extent *, extent *));
local void append_string_to_mem OF((char *, int, char **, extent *, extent *));

local void write_ushort_to_mem(OFT(ush) usValue, OFT(char *) pPtr)
{
    *pPtr++ = ((char)(usValue) & 0xff);
    *pPtr   = ((char)(usValue >> 8) & 0xff);
}

local void write_ulong_to_mem(ulg uValue, char *pPtr)
{
    write_ushort_to_mem((ush)(uValue & 0xffff), pPtr);
    write_ushort_to_mem((ush)((uValue >> 16) & 0xffff), pPtr + 2);
}

local void write_int64_to_mem(uzoff_t l64Value, char *pPtr)
{
    write_ulong_to_mem((ulg)(l64Value & 0xffffffff), pPtr);
    write_ulong_to_mem((ulg)((l64Value >> 32) & 0xffffffff), pPtr + 4);
}

// Write a string to memory
local void write_string_to_mem(char *strValue, char *pPtr)
{
    if (strValue != NULL) {
        int ssize = strlen(strValue);
        int i;

        for (i = 0; i < ssize; i++) {
            *(pPtr + i) = *(strValue + i);
        }
    }
}

// same as above but allocate memory as needed and keep track of current end
// using offset - 2/6/05 EG

local void append_ushort_to_mem(OFT(ush) usValue, OFT(char **) pPtr, OFT(extent *) offset,
                                OFT(extent *) blocksize)
{
    if (zip_fatal)
        return;

    if (*pPtr == NULL) {
        // malloc a 1K block
        (*blocksize) = 1024;
        *pPtr        = (char *)malloc(*blocksize);
        if (*pPtr == NULL) {
            {
                zip_fail(ZE_MEM, "append_ushort_to_mem");
                return;
            }
        }
    }
    // if (*offset) + 2 > (*blocksize) - 1
    else if ((*offset) > (*blocksize) - (1 + 2)) {
        // realloc a bigger block in 1 K increments
        (*blocksize) += 1024;
        *pPtr = (char *)realloc(*pPtr, (extent)*blocksize);
        if (*pPtr == NULL) {
            {
                zip_fail(ZE_MEM, "append_ushort_to_mem");
                return;
            }
        }
    }
    write_ushort_to_mem(usValue, (*pPtr) + (*offset));
    (*offset) += 2;
}

local void append_ulong_to_mem(ulg uValue, char **pPtr, extent *offset, extent *blocksize)
{
    if (zip_fatal)
        return;

    if (*pPtr == NULL) {
        // malloc a 1K block
        (*blocksize) = 1024;
        *pPtr        = (char *)malloc(*blocksize);
        if (*pPtr == NULL) {
            {
                zip_fail(ZE_MEM, "append_ulong_to_mem");
                return;
            }
        }
    } else if ((*offset) > (*blocksize) - (1 + 4)) {
        // realloc a bigger block in 1 K increments
        (*blocksize) += 1024;
        *pPtr = (char *)realloc(*pPtr, *blocksize);
        if (*pPtr == NULL) {
            {
                zip_fail(ZE_MEM, "append_ulong_to_mem");
                return;
            }
        }
    }
    write_ulong_to_mem(uValue, (*pPtr) + (*offset));
    (*offset) += 4;
}

local void append_int64_to_mem(uzoff_t l64Value, char **pPtr, extent *offset, extent *blocksize)
{
    if (zip_fatal)
        return;

    if (*pPtr == NULL) {
        // malloc a 1K block
        (*blocksize) = 1024;
        *pPtr        = (char *)malloc(*blocksize);
        if (*pPtr == NULL) {
            {
                zip_fail(ZE_MEM, "append_int64_to_mem");
                return;
            }
        }
    } else if ((*offset) > (*blocksize) - (1 + 8)) {
        // realloc a bigger block in 1 K increments
        (*blocksize) += 1024;
        *pPtr = (char *)realloc(*pPtr, *blocksize);
        if (*pPtr == NULL) {
            {
                zip_fail(ZE_MEM, "append_int64_to_mem");
                return;
            }
        }
    }
    write_int64_to_mem(l64Value, (*pPtr) + (*offset));
    (*offset) += 8;
}

// Append a string to the memory block.
local void append_string_to_mem(char *strValue, int strLength, char **pPtr, extent *offset,
                                extent *blocksize)
{
    if (zip_fatal)
        return;

    if (strValue != NULL) {
        unsigned bsize = 1024;
        unsigned ssize = strLength;
        unsigned i;

        if (ssize > bsize) {
            bsize = ssize;
        }
        if (*pPtr == NULL) {
            // malloc a 1K block
            (*blocksize) = bsize;
            *pPtr        = (char *)malloc(*blocksize);
            if (*pPtr == NULL) {
                {
                    zip_fail(ZE_MEM, "append_string_to_mem");
                    return;
                }
            }
        } else if ((*offset) + ssize > (*blocksize) - 1) {
            // realloc a bigger block in 1 K increments
            (*blocksize) += bsize;
            *pPtr = (char *)realloc(*pPtr, *blocksize);
            if (*pPtr == NULL) {
                {
                    zip_fail(ZE_MEM, "append_string_to_mem");
                    return;
                }
            }
        }
        for (i = 0; i < ssize; i++) {
            *(*pPtr + *offset + i) = *(strValue + i);
        }
        (*offset) += ssize;
    }
}

// ----------------------------------------------------

// zip64 support 08/31/2003 R.Nausedat
// moved out of zip64 support 10/22/05

// Searches pExtra for extra field with specified tag.
// If it finds one it returns a pointer to it, else NULL.
// Renamed and made generic.  10/3/03
char *get_extra_field(OFT(ush) tag, OFT(char *) pExtra, OFT(unsigned) iExtraLen)
{
    char *pTemp;
    ush usBlockTag;
    ush usBlockSize;

    if (pExtra == NULL)
        return NULL;

    for (pTemp = pExtra; pTemp < pExtra + iExtraLen - ZIP_EF_HEADER_SIZE;) {
        usBlockTag  = SH(pTemp);     // get tag
        usBlockSize = SH(pTemp + 2); // get field data size
        if (usBlockTag == tag)
            return pTemp;
        pTemp += (usBlockSize + ZIP_EF_HEADER_SIZE);
    }
    return NULL;
}

// copy_nondup_extra_fields
//
// Copy any extra fields in old that are not in new to new.
// Returns the new extra fields block and newLen is new length.
char *copy_nondup_extra_fields(char *oldExtra, unsigned oldExtraLen, char *newExtra,
                               unsigned newExtraLen, unsigned *newLen)
{
    char *returnExtra  = NULL;
    ush returnExtraLen = 0;
    char *tempExtra;
    char *pTemp;
    ush tag;
    ush blocksize;

    if (oldExtra == NULL) {
        // no old extra fields so return copy of newExtra
        if (newExtra == NULL || newExtraLen == 0) {
            *newLen = 0;
            return NULL;
        } else {
            if ((returnExtra = (char *)malloc(newExtraLen)) == NULL) {
                zip_fail(ZE_MEM, "extra field copy");
                return NULL;
            }
            memcpy(returnExtra, newExtra, newExtraLen);
            returnExtraLen = newExtraLen;
            *newLen        = returnExtraLen;
            return returnExtra;
        }
    }

    // allocate block large enough for all extra fields
    if ((tempExtra = (char *)malloc(0xFFFF)) == NULL) {
        zip_fail(ZE_MEM, "extra field copy");
        return NULL;
    }

    // look for each old extra field in new block
    for (pTemp = oldExtra; pTemp < oldExtra + oldExtraLen;) {
        tag       = SH(pTemp);     // get tag
        blocksize = SH(pTemp + 2); // get field data size
        if (get_extra_field(tag, newExtra, newExtraLen) == NULL) {
            // tag not in new block so add it
            memcpy(tempExtra + returnExtraLen, pTemp, blocksize + 4);
            returnExtraLen += blocksize + 4;
        }
        pTemp += blocksize + 4;
    }

    // copy all extra fields from new block
    memcpy(tempExtra + returnExtraLen, newExtra, newExtraLen);
    returnExtraLen += newExtraLen;

    // copy tempExtra to returnExtra
    if ((returnExtra = (char *)malloc(returnExtraLen)) == NULL) {
        zip_fail(ZE_MEM, "extra field copy");
        return NULL;
    }
    memcpy(returnExtra, tempExtra, returnExtraLen);
    free(tempExtra);

    *newLen = returnExtraLen;
    return returnExtra;
}

// The latest format is
// 1 byte     Version of Unicode Path Extra Field
// 4 bytes    Name Field CRC32 Checksum
// variable   UTF-8 Version Of Name

// adds a zip64 extra field to the data the cextra member of zlist points to. If
// there is already a zip64 extra field present delete it first.
local int add_central_zip64_extra_field(struct zlist far *pZipListEntry)
{
    char *pExtraFieldPtr;
    char *pTemp;
    ush usTemp;
    ush efsize = 0;
    ush esize;
    ush oldefsize;
    extent len;
    int used_zip64 = 0;

    // get length of ef based on which fields exceed limits
    // AppNote says:
    // The order of the fields in the ZIP64 extended
    // information record is fixed, but the fields will
    // only appear if the corresponding Local or Central
    // directory record field is set to 0xFFFF or 0xFFFFFFFF.
    efsize = ZIP_EF_HEADER_SIZE; // type + size
    if (pZipListEntry->len > ZIP_UWORD32_MAX || force_zip64 == 1) {
        // compressed size
        efsize += 8;
        used_zip64 = 1;
    }
    if (pZipListEntry->siz > ZIP_UWORD32_MAX) {
        // uncompressed size
        efsize += 8;
        used_zip64 = 1;
    }
    if (pZipListEntry->off > ZIP_UWORD32_MAX) {
        // offset
        efsize += 8;
        used_zip64 = 1;
    }
    if (pZipListEntry->dsk > ZIP_UWORD16_MAX) {
        // disk number
        efsize += 4;
        used_zip64 = 1;
    }

    if (used_zip64 && force_zip64 == 0) {
        zipwarn("Large entry support disabled using -fz- but needed", "");
        return ZE_BIG;
    }

    // malloc zip64 extra field?
    if (pZipListEntry->cextra == NULL) {
        if (efsize == ZIP_EF_HEADER_SIZE) {
            return ZE_OK;
        }
        if ((pExtraFieldPtr = pZipListEntry->cextra = (char *)malloc(efsize)) == NULL) {
            return ZE_MEM;
        }
        pZipListEntry->cext = efsize;
    } else {
        // check if we have a "large file" extra field ...
        pExtraFieldPtr = get_extra_field(ZIP64_EF_TAG, pZipListEntry->cextra, pZipListEntry->cext);
        if (pExtraFieldPtr == NULL) {
            // ... we don't, so re-malloc enough memory for the old extra data plus
            // the size of the zip64 extra field
            if ((pExtraFieldPtr = (char *)malloc(efsize + pZipListEntry->cext)) == NULL) {
                return ZE_MEM;
            }
            // move the old extra field
            memmove(pExtraFieldPtr, pZipListEntry->cextra, pZipListEntry->cext);
            free(pZipListEntry->cextra);
            pZipListEntry->cextra = pExtraFieldPtr;
            pExtraFieldPtr += pZipListEntry->cext;
            pZipListEntry->cext += efsize;
        } else {
            // ... we have. sort out the existing zip64 extra field and remove it from
            // pZipListEntry->cextra, re-malloc enough memory for the old extra data
            // left plus the size of the zip64 extra field
            usTemp = SH(pExtraFieldPtr + 2);
            // if pZipListEntry->cextra == pExtraFieldPtr and pZipListEntry->cext == usTemp + efsize
            // we should have only one extra field, and this is a zip64 extra field. as some
            // zip tools seem to require fixed zip64 extra fields we have to check if
            // usTemp + ZIP_EF_HEADER_SIZE is equal to ZIP64_LARGE_FILE_HEAD_SIZE. if it
            // isn't, we free the old extra field and allocate memory for a new one
            if (pZipListEntry->cext == (extent)(usTemp + ZIP_EF_HEADER_SIZE)) {
                // just Zip64 extra field in extra field
                if (pZipListEntry->cext != efsize) {
                    // wrong size
                    if ((pExtraFieldPtr = (char *)malloc(efsize)) == NULL) {
                        return ZE_MEM;
                    }
                    free(pZipListEntry->cextra);
                    pZipListEntry->cextra = pExtraFieldPtr;
                    pZipListEntry->cext   = efsize;
                }
            } else {
                // get the old Zip64 extra field out and add new
                oldefsize = usTemp + ZIP_EF_HEADER_SIZE;
                if ((pTemp = (char *)malloc(pZipListEntry->cext - oldefsize + efsize)) == NULL) {
                    return ZE_MEM;
                }
                len = (extent)(pExtraFieldPtr - pZipListEntry->cextra);
                memcpy(pTemp, pZipListEntry->cextra, len);
                memcpy(pTemp + len, pExtraFieldPtr + oldefsize,
                       pZipListEntry->cext - oldefsize - len);
                pZipListEntry->cext -= oldefsize;
                pExtraFieldPtr = pTemp + pZipListEntry->cext;
                pZipListEntry->cext += efsize;
                free(pZipListEntry->cextra);
                pZipListEntry->cextra = pTemp;
            }
        }
    }

    // set zip64 extra field members
    write_ushort_to_mem(ZIP64_EF_TAG, pExtraFieldPtr);
    write_ushort_to_mem((ush)(efsize - ZIP_EF_HEADER_SIZE), pExtraFieldPtr + 2);
    esize = ZIP_EF_HEADER_SIZE;
    if (pZipListEntry->len > ZIP_UWORD32_MAX || force_zip64 == 1) {
        write_int64_to_mem(pZipListEntry->len, pExtraFieldPtr + esize);
        esize += 8;
    }
    if (pZipListEntry->siz > ZIP_UWORD32_MAX) {
        write_int64_to_mem(pZipListEntry->siz, pExtraFieldPtr + esize);
        esize += 8;
    }
    if (pZipListEntry->off > ZIP_UWORD32_MAX) {
        write_int64_to_mem(pZipListEntry->off, pExtraFieldPtr + esize);
        esize += 8;
    }
    if (pZipListEntry->dsk > ZIP_UWORD16_MAX) {
        write_ulong_to_mem(pZipListEntry->dsk, pExtraFieldPtr + esize);
    }

    // un' wech
    return ZE_OK;
}

// Add Zip64 extra field to local header
// 10/5/03 EG
local int add_local_zip64_extra_field(struct zlist far *pZEntry)
{
    char *pZ64Extra;
    char *pOldZ64Extra;
    char *pOldTemp;
    char *pTemp;
    ush newEFSize;
    ush usTemp;
    ush blocksize;
    ush Z64LocalLen = ZIP_EF_HEADER_SIZE + // tag + EF Data Len
                      8 +                  // original uncompressed length of file
                      8;                   // compressed size of file

    // malloc zip64 extra field?
    // after the below pZ64Extra should point to start of Zip64 extra field
    if (pZEntry->ext == 0 || pZEntry->extra == NULL) {
        // get new extra field
        pZ64Extra = pZEntry->extra = (char *)malloc(Z64LocalLen);
        if (pZEntry->extra == NULL) {
            {
                zip_fail(ZE_MEM, "Zip64 local extra field");
                return ZE_MEM;
            }
        }
        pZEntry->ext = Z64LocalLen;
    } else {
        // check if we have a Zip64 extra field ...
        pOldZ64Extra = get_extra_field(ZIP64_EF_TAG, pZEntry->extra, pZEntry->ext);
        if (pOldZ64Extra == NULL) {
            // ... we don't, so re-malloc enough memory for the old extra data plus
            // the size of the zip64 extra field
            pZ64Extra = (char *)malloc(Z64LocalLen + pZEntry->ext);
            if (pZ64Extra == NULL) {
                zip_fail(ZE_MEM, "Zip64 Extra Field");
                return ZE_MEM;
            }
            // move old extra field and update pointer and length
            memmove(pZ64Extra, pZEntry->extra, pZEntry->ext);
            free(pZEntry->extra);
            pZEntry->extra = pZ64Extra;
            pZ64Extra += pZEntry->ext;
            pZEntry->ext += Z64LocalLen;
        } else {
            // ... we have. Sort out the existing zip64 extra field and remove it
            // from pZEntry->extra, re-malloc enough memory for the old extra data
            // left plus the size of the zip64 extra field
            blocksize = SH(pOldZ64Extra + 2);
            // If the right length then go with it, else get rid of it and add a new extra field
            // to existing block.
            if (blocksize == Z64LocalLen - ZIP_EF_HEADER_SIZE) {
                // looks good
                pZ64Extra = pOldZ64Extra;
            } else {
                newEFSize = pZEntry->ext - (blocksize + ZIP_EF_HEADER_SIZE) + Z64LocalLen;
                pZ64Extra = (char *)malloc(newEFSize);
                if (pZ64Extra == NULL) {
                    zip_fail(ZE_MEM, "Zip64 Extra Field");
                    return ZE_MEM;
                }
                // move all before Zip64 EF
                usTemp = (extent)(pOldZ64Extra - pZEntry->extra);
                pTemp  = pZ64Extra;
                memcpy(pTemp, pZEntry->extra, usTemp);
                // move all after old Zip64 EF
                pTemp    = pZ64Extra + usTemp;
                pOldTemp = pOldZ64Extra + ZIP_EF_HEADER_SIZE + blocksize;
                usTemp   = pZEntry->ext - usTemp - blocksize;
                memcpy(pTemp, pOldTemp, usTemp);
                // replace extra fields
                pZEntry->ext = newEFSize;
                free(pZEntry->extra);
                pZEntry->extra = pZ64Extra;
                pZ64Extra      = pTemp + usTemp;
            }
        }
    }
    // set/update zip64 extra field members
    write_ushort_to_mem(ZIP64_EF_TAG, pZ64Extra);
    write_ushort_to_mem((ush)(Z64LocalLen - ZIP_EF_HEADER_SIZE), pZ64Extra + 2);
    write_int64_to_mem(pZEntry->len, pZ64Extra + 2 + 2);
    write_int64_to_mem(pZEntry->siz, pZ64Extra + 2 + 2 + 8);

    return ZE_OK;
}

// Add UTF-8 path extra field
// 10/11/05
local int add_Unicode_Path_local_extra_field(struct zlist far *pZEntry)
{
    char *pUExtra;
    char *pOldUExtra;
    char *pOldTemp;
    char *pTemp;
    ush newEFSize;
    ush usTemp;
    ush ULen = strlen(pZEntry->uname);
    ush blocksize;
    ulg chksum    = CRCVAL_INITIAL;
    ush ULocalLen = ZIP_EF_HEADER_SIZE + // tag + EF Data Len
                    1 +                  // version
                    4 +                  // iname chksum
                    ULen;                // UTF-8 path

    // malloc Unicode Path extra field?
    // after the below pUExtra should point to start of Unicode Path extra field
    if (pZEntry->ext == 0 || pZEntry->extra == NULL) {
        // get new extra field
        pUExtra = pZEntry->extra = (char *)malloc(ULocalLen);
        if (pZEntry->extra == NULL) {
            {
                zip_fail(ZE_MEM, "UTF-8 Path local extra field");
                return ZE_MEM;
            }
        }
        pZEntry->ext = ULocalLen;
    } else {
        // check if we have a Unicode Path extra field ...
        pOldUExtra = get_extra_field(UTF8_PATH_EF_TAG, pZEntry->extra, pZEntry->ext);
        if (pOldUExtra == NULL) {
            // ... we don't, so re-malloc enough memory for the old extra data plus
            // the size of the UTF-8 Path extra field
            pUExtra = (char *)malloc(ULocalLen + pZEntry->ext);
            if (pUExtra == NULL) {
                zip_fail(ZE_MEM, "UTF-8 Path Extra Field");
                return ZE_MEM;
            }
            // move old extra field and update pointer and length
            memmove(pUExtra, pZEntry->extra, pZEntry->ext);
            free(pZEntry->extra);
            pZEntry->extra = pUExtra;
            pUExtra += pZEntry->ext;
            pZEntry->ext += ULocalLen;
        } else {
            // ... we have. Sort out the existing UTF-8 Path extra field and remove it
            // from pZEntry->extra, re-malloc enough memory for the old extra data
            // left plus the size of the UTF-8 Path extra field
            blocksize = SH(pOldUExtra + 2);
            // If the right length then go with it, else get rid of it and add a new extra field
            // to existing block.
            if (blocksize == ULocalLen - ZIP_EF_HEADER_SIZE) {
                // looks good
                pUExtra = pOldUExtra;
            } else {
                newEFSize = pZEntry->ext - (blocksize + ZIP_EF_HEADER_SIZE) + ULocalLen;
                pUExtra   = (char *)malloc(newEFSize);
                if (pUExtra == NULL) {
                    zip_fail(ZE_MEM, "UTF-8 Path Extra Field");
                    return ZE_MEM;
                }
                // move all before UTF-8 Path EF
                usTemp = (extent)(pOldUExtra - pZEntry->extra);
                pTemp  = pUExtra;
                memcpy(pTemp, pZEntry->extra, usTemp);
                // move all after old UTF-8 Path EF
                pTemp    = pUExtra + usTemp;
                pOldTemp = pOldUExtra + ZIP_EF_HEADER_SIZE + blocksize;
                usTemp   = pZEntry->ext - usTemp - blocksize;
                memcpy(pTemp, pOldTemp, usTemp);
                // replace extra fields
                pZEntry->ext = newEFSize;
                free(pZEntry->extra);
                pZEntry->extra = pUExtra;
                pUExtra        = pTemp + usTemp;
            }
        }
    }

    // Compute the Adler-16 checksum of iname
    // chksum = adler16(ADLERVAL_INITIAL,
    // (uch *)(pZEntry->iname), strlen(pZEntry->iname));

#define inameLocal (pZEntry->iname)

    chksum = crc32(chksum, (uch *)(inameLocal), strlen(inameLocal));

#undef inameLocal

    // set/update UTF-8 Path extra field members
    // tag header
    write_ushort_to_mem(UTF8_PATH_EF_TAG, pUExtra);
    // data size
    write_ushort_to_mem((ush)(ULocalLen - ZIP_EF_HEADER_SIZE), pUExtra + 2);
    // version
    *(pUExtra + 2 + 2) = 1;
    // iname chksum
    write_ulong_to_mem(chksum, pUExtra + 2 + 2 + 1);
    // UTF-8 path
    write_string_to_mem(pZEntry->uname, pUExtra + 2 + 2 + 1 + 4);

    return ZE_OK;
}

local int add_Unicode_Path_cen_extra_field(struct zlist far *pZEntry)
{
    char *pUExtra;
    char *pOldUExtra;
    char *pOldTemp;
    char *pTemp;
    ush newEFSize;
    ush usTemp;
    ush ULen = strlen(pZEntry->uname);
    ush blocksize;
    ulg chksum  = CRCVAL_INITIAL;
    ush UCenLen = ZIP_EF_HEADER_SIZE + // tag + EF Data Len
                  1 +                  // version
                  4 +                  // checksum
                  ULen;                // UTF-8 path

    // malloc Unicode Path extra field?
    // after the below pUExtra should point to start of Unicode Path extra field
    if (pZEntry->cext == 0 || pZEntry->cextra == NULL) {
        // get new extra field
        pUExtra = pZEntry->cextra = (char *)malloc(UCenLen);
        if (pZEntry->cextra == NULL) {
            {
                zip_fail(ZE_MEM, "UTF-8 Path cen extra field");
                return ZE_MEM;
            }
        }
        pZEntry->cext = UCenLen;
    } else {
        // check if we have a Unicode Path extra field ...
        pOldUExtra = get_extra_field(UTF8_PATH_EF_TAG, pZEntry->cextra, pZEntry->cext);
        if (pOldUExtra == NULL) {
            // ... we don't, so re-malloc enough memory for the old extra data plus
            // the size of the UTF-8 Path extra field
            pUExtra = (char *)malloc(UCenLen + pZEntry->cext);
            if (pUExtra == NULL) {
                zip_fail(ZE_MEM, "UTF-8 Path Extra Field");
                return ZE_MEM;
            }
            // move old extra field and update pointer and length
            memmove(pUExtra, pZEntry->cextra, pZEntry->cext);
            free(pZEntry->cextra);
            pZEntry->cextra = pUExtra;
            pUExtra += pZEntry->cext;
            pZEntry->cext += UCenLen;
        } else {
            // ... we have. Sort out the existing UTF-8 Path extra field and remove it
            // from pZEntry->extra, re-malloc enough memory for the old extra data
            // left plus the size of the UTF-8 Path extra field
            blocksize = SH(pOldUExtra + 2);
            // If the right length then go with it, else get rid of it and add a new extra field
            // to existing block.
            if (blocksize == UCenLen - ZIP_EF_HEADER_SIZE) {
                // looks good
                pUExtra = pOldUExtra;
            } else {
                newEFSize = pZEntry->cext - (blocksize + ZIP_EF_HEADER_SIZE) + UCenLen;
                pUExtra   = (char *)malloc(newEFSize);
                if (pUExtra == NULL) {
                    zip_fail(ZE_MEM, "UTF-8 Path Extra Field");
                    return ZE_MEM;
                }
                // move all before UTF-8 Path EF
                usTemp = (extent)(pOldUExtra - pZEntry->cextra);
                pTemp  = pUExtra;
                memcpy(pTemp, pZEntry->cextra, usTemp);
                // move all after old UTF-8 Path EF
                pTemp    = pUExtra + usTemp;
                pOldTemp = pOldUExtra + ZIP_EF_HEADER_SIZE + blocksize;
                usTemp   = pZEntry->cext - usTemp - blocksize;
                memcpy(pTemp, pOldTemp, usTemp);
                // replace extra fields
                pZEntry->cext = newEFSize;
                free(pZEntry->cextra);
                pZEntry->cextra = pUExtra;
                pUExtra         = pTemp + usTemp;
            }
        }
    }

    // Compute the CRC-32 checksum of iname
#define inameLocal (pZEntry->iname)

    chksum = crc32(chksum, (uch *)(inameLocal), strlen(inameLocal));

#undef inameLocal

    // Compute the Adler-16 checksum of iname
    // chksum = adler16(ADLERVAL_INITIAL,
    // (uch *)(pZEntry->iname), strlen(pZEntry->iname));

    // set/update UTF-8 Path extra field members
    // tag header
    write_ushort_to_mem(UTF8_PATH_EF_TAG, pUExtra);
    // data size
    write_ushort_to_mem((ush)(UCenLen - ZIP_EF_HEADER_SIZE), pUExtra + 2);
    // version
    *(pUExtra + 2 + 2) = 1;
    // iname checksum
    write_ulong_to_mem(chksum, pUExtra + 2 + 2 + 1);
    // UTF-8 path
    write_string_to_mem(pZEntry->uname, pUExtra + 2 + 2 + 1 + 4);

    return ZE_OK;
}

zoff_t ffile_size OF((FILE *));

// 2004-12-06 SMS.
// ffile_size() returns reliable file size or EOF.
// May be used to detect large files in a small-file program.

Task<int> putlocal(struct zlist far *z, int rewrite)
{
    // If any of compressed size (siz), uncompressed size (len), offset(off), or
    // disk number (dsk) is larger than can fit in the below standard fields then a
    // Zip64 flag value is stored and a Zip64 extra field is created.
    // Only siz and len are in the local header while all can be in the central
    // directory header.
    //
    // For the local header if the extra field is created must store both
    // uncompressed and compressed sizes.
    //
    // This assumes that for large entries the compressed size won't need a
    // Zip64 extra field if the uncompressed size did not.  This assumption should
    // only fail for a large file of nearly totally uncompressable data.
    //
    // If streaming stdin in and use_descriptors is set then always create a Zip64
    // extra field flagging the data descriptor as being in Zip64 format.  This is
    // needed as don't know if need Zip64 or not when need to set Zip64 flag in
    // local header.
    //
    // If rewrite is set then don't count bytes written for splits
    char *block      = NULL;   // mem block to write to
    extent offset    = 0;      // offset into block
    extent blocksize = 0;      // size of block
    ush nam          = z->nam; // size of name to write to header
    int use_uname    = 0;      // write uname to header
    int streaming_in = 0;      // streaming stdin
    int was_zip64    = 0;

    // If input is stdin then streaming stdin.  No problem with that.
    //
    // The problem is updating the local header data in the output once the sizes
    // and crc are known.  If the output is not seekable, then need data descriptors
    // and also need to assume Zip64 will be needed as don't know yet.  Even if the
    // output is seekable, if the input is streamed need to write the Zip64 extra field
    // before writing the data or there won't be room for it later if we need it.
    streaming_in = (strcmp(z->name, "-") == 0);

    if (!rewrite) {
        zip64_entry = 0;
        // initial local header
        if (z->siz > ZIP_UWORD32_MAX || z->len > ZIP_UWORD32_MAX || force_zip64 == 1 ||
            (force_zip64 != 0 && streaming_in)) {
            // assume Zip64
            if (force_zip64 == 0) {
                co_await zipwarn("Entry too big:", z->oname);
                ZIPERR(ZE_BIG, "Large entry support disabled with -fz- but needed");
            }
            zip64_entry = 1; // header of this entry has a field needing Zip64
            if (z->ver < ZIP64_MIN_VER)
                z->ver = ZIP64_MIN_VER;
            was_zip64 = 1;
        }
    } else {
        // rewrite
        was_zip64   = zip64_entry;
        zip64_entry = 0;
        if (z->siz > ZIP_UWORD32_MAX || z->len > ZIP_UWORD32_MAX || force_zip64 == 1 ||
            (force_zip64 != 0 && streaming_in)) {
            // Zip64 entry
            zip64_entry = 1;
        }
        if (force_zip64 == 0 && zip64_entry) {
            // tried to force into standard entry but needed Zip64 entry
            co_await zipwarn("Entry too big:", z->oname);
            ZIPERR(ZE_BIG, "Large entry support disabled with -fz- but entry needs");
        }
        // Normally for a large archive if the input file is less than 4 GB then
        // the compressed or stored version should be less than 4 GB.  If this
        // assumption is wrong this catches it.  This is a problem even if not
        // streaming as the Zip64 extra field was not written and now there's no
        // room for it.
        if (was_zip64 == 0 && zip64_entry == 1) {
            // guessed wrong and need Zip64
            co_await zipwarn("Entry too big:", z->oname);
            if (force_zip64 == 0) {
                ZIPERR(ZE_BIG, "Compressed/stored entry unexpectedly large - do not use -fz-");
            } else {
                ZIPERR(ZE_BIG, "Poor compression resulted in unexpectedly large entry - try -fz");
            }
        }
        if (zip64_entry) {
            // Zip64 entry still
            // this archive needs Zip64 (version 4.5 unzipper)
            zip64_archive = 1;
            if (z->ver < ZIP64_MIN_VER)
                z->ver = ZIP64_MIN_VER;
        } else {
            // it turns out we do not need Zip64
            zip64_entry = 0;
        }
        if (was_zip64 && zip64_entry != 1) {
            z->ver = 20;
        }
    }

    // Instead of writing to the file as we go, to do splits we have to write it
    // to memory and see if it will fit before writing the entire local header.
    // If the local header doesn't fit we need to save it for the next disk.

    if (zip64_entry || was_zip64)
        // update extra field
        add_local_zip64_extra_field(z);

    if (z->uname) {
        // need UTF-8 name
        if (utf8_force || using_utf8) {
            z->lflg |= UTF8_BIT;
            z->flg |= UTF8_BIT;
        }
        if (z->flg & UTF8_BIT) {
            // If this flag is set, then restore UTF-8 as path name
            use_uname = 1;
            nam       = strlen(z->uname);
        } else {
            // use extra field
            add_Unicode_Path_local_extra_field(z);
        }
    } else {
        // clear UTF-8 bit as not needed
        z->flg &= ~UTF8_BIT;
        z->lflg &= ~UTF8_BIT;
    }

    append_ulong_to_mem(LOCSIG, &block, &offset, &blocksize);   // local file header signature
    append_ushort_to_mem(z->ver, &block, &offset, &blocksize);  // version needed to extract
    append_ushort_to_mem(z->lflg, &block, &offset, &blocksize); // general purpose bit flag
    append_ushort_to_mem(z->how, &block, &offset, &blocksize);  // compression method
    append_ulong_to_mem(z->tim, &block, &offset, &blocksize);   // last mod file date time
    append_ulong_to_mem(z->crc, &block, &offset, &blocksize);   // crc-32
                                                                // changes 10/5/03 EG
    if (zip64_entry) {
        append_ulong_to_mem(0xFFFFFFFF, &block, &offset, &blocksize); // compressed size
        append_ulong_to_mem(0xFFFFFFFF, &block, &offset, &blocksize); // uncompressed size
    } else {
        append_ulong_to_mem((ulg)z->siz, &block, &offset, &blocksize); // compressed size
        append_ulong_to_mem((ulg)z->len, &block, &offset, &blocksize); // uncompressed size
    }
    append_ushort_to_mem(nam, &block, &offset, &blocksize); // file name length

    append_ushort_to_mem(z->ext, &block, &offset, &blocksize); // extra field length

    if (use_uname) {
        // path is UTF-8
        append_string_to_mem(z->uname, nam, &block, &offset, &blocksize);
    } else
        append_string_to_mem(z->iname, z->nam, &block, &offset, &blocksize); // file name
    if (z->ext) {
        append_string_to_mem(z->extra, z->ext, &block, &offset, &blocksize); // extra field
    }

    // write the header
    if (rewrite == PUTLOCAL_REWRITE) {
        // use fwrite as seeked back and not extending the archive
        // also if split_method 1 write to file with local header
        if (split_method == 1) {
            if (co_await zwrite(block, 1, offset, current_local_file) != offset) {
                free(block);
                co_return ZE_TEMP;
            }
            // now can close the split if local header on previous split
            if (current_local_disk != current_disk) {
                co_await close_split(current_local_disk, current_local_file,
                                     current_local_tempname);
                current_local_file = NULL;
                free(current_local_tempname);
            }
        } else {
            // not doing splits
            if (co_await zwrite(block, 1, offset, y) != offset) {
                free(block);
                co_return ZE_TEMP;
            }
        }
    } else {
        // do same if archive not split or split_method 2 with descriptors
        // use bfwrite which counts bytes for splits
        if (co_await bfwrite(block, 1, offset, BFWRITE_LOCALHEADER) != offset) {
            free(block);
            co_return ZE_TEMP;
        }
    }
    free(block);
    co_return ZE_OK;
}

Task<int> putextended(struct zlist far *z)
{
    // write to mem block then write to file 3/10/2005
    char *block      = NULL; // mem block to write to
    extent offset    = 0;    // offset into block
    extent blocksize = 0;    // size of block

    append_ulong_to_mem(EXTLOCSIG, &block, &offset, &blocksize); // extended local signature
    append_ulong_to_mem(z->crc, &block, &offset, &blocksize);    // crc-32
    if (zip64_entry) {
        // use Zip64 entries
        append_int64_to_mem(z->siz, &block, &offset, &blocksize); // compressed size
        append_int64_to_mem(z->len, &block, &offset, &blocksize); // uncompressed size
        // This is rather klugy as the AppNote handles this poorly.  Typically
        // we don't know at this point if we are writing a Zip64 archive or not,
        // unless a file has needed Zip64.  This is particularly annoying here
        // when deciding the size of the data descriptor (extended local header)
        // fields as the appnote says the uncompressed and compressed sizes
        // should be 8 bytes if the archive is Zip64 and 4 bytes if not.
        //
        // One interpretation is the version of the archive is determined from
        // the Version Needed To Extract field in the Zip64 End Of Central Directory
        // record and so either an archive should start as Zip64 and write all data
        // descriptors with 8-byte fields or store everything until all the files
        // are processed and then write everything to the archive as changing the
        // sizes of the data descriptors is messy and just not feasible when
        // streaming to standard output.  This is not easily workable and others
        // use the different interpretation below.
        //
        // This was the old thought:
        // We always write a standard data descriptor.  If the file has a large
        // uncompressed or compressed size we set the field to the max field
        // value, which we are defining as flagging the field as having a Zip64
        // value that doesn't fit.  As the CRC happens before the variable size
        // fields the CRC is still valid and can be used to check the file.  We
        // always use deflate if streaming so signatures should not appear in
        // the data and all local header signatures should be valid, allowing a
        // streaming unzip to find entries by local header signatures, if max size
        // values in the data descriptor sizes ignore them, and extract the file and
        // check it using the CRC.  If not streaming the central directory is available
        // so just use those values which are correct.
        //
        // After discussions with other groups this is the current thinking:
        //
        // Apparent industry interpretation for data descriptors:
        // Data descriptor size is determined for each entry.  If the local header
        // version needed to extract is 45 or higher then the entry can use Zip64
        // data descriptors but more checking is needed.  If Zip64 extra field is
        // present then assume data descriptor is Zip64 and local version needed
        // to extract should be 45 or higher.  If standard data descriptor then
        // local size fields are set to 0 and correct sizes are in standard data descriptor.
        // If Zip64 data descriptor then local sizes are set to -1, Zip64 extra field
        // sizes are set to 0, and the correct sizes are in the Zip64 data descriptor.
        //
        // So do this:
        // If an entry is standard and the archive is updatable then seek back and
        // update the local header.  No change.
        //
        // If an entry is zip64 and the archive is updatable assume the Zip64 extra
        // field was created and update it.  No change.
        //
        // If data descriptors are needed then assume the archive is Zip64.  This is
        // a change and means if ZIP64_SUPPORT is enabled that any non-updatable archive
        // will be in Zip64 format and use Zip64 data descriptors.  This should be
        // compatible with other zippers that depend on the current (though not perfect)
        // AppNote description.
        //
        // If anyone has some ideas on this I'd like to hear them.
        //
        // 3/20/05 EG
        //
        // Only assume need Zip64 if the input size is unknown.  If the input size is
        // known we can assume Zip64 if the input is larger than 4 GB and assume not
        // otherwise.  If the output is seekable we still need to create the Zip64
        // extra field if the input size is unknown so we can seek back and update it.
        // 12/28/05 EG
        // Updated 5/21/06 EG
    } else {
        // for encryption
        append_ulong_to_mem((ulg)z->siz, &block, &offset, &blocksize); // compressed size
        append_ulong_to_mem((ulg)z->len, &block, &offset, &blocksize); // uncompressed size
    }
    // write the header
    if (co_await bfwrite(block, 1, offset, BFWRITE_HEADER) != offset) {
        free(block);
        co_return ZE_TEMP;
    }
    free(block);
    co_return ZE_OK;
}

Task<int> putcentral(struct zlist far *z)
{
    // If any of compressed size (siz), uncompressed size (len), offset(off), or
    // disk number (dsk) is larger than can fit in the below standard fields then a
    // Zip64 flag value is stored and a Zip64 extra field is created.
    // Only siz and len are in the local header while all are in the central directory
    // header.
    //
    // For the central directory header just store the fields required.  All previous fields
    // must be stored though.  So can store none (no extra field), just uncompressed size
    // (len), len then siz, len then siz then off, or len then siz then off then dsk, in
    // those orders.  10/6/03 EG

    // write to mem block then write to file 3/10/2005 EG
    char *block      = NULL;   // mem block to write to
    extent offset    = 0;      // offset into block
    extent blocksize = 0;      // size of block
    uzoff_t off      = 0;      // offset to start of local header
    ush nam          = z->nam; // size of name to write to header
    int use_uname    = 0;      // write uname to header

    int iRes;

    if (z->uname) {
        if (utf8_force) {
            z->flg |= UTF8_BIT;
        }
        if (z->flg & UTF8_BIT) {
            // If this flag is set, then restore UTF-8 as path name
            use_uname = 1;
            nam       = strlen(z->uname);
        } else {
            add_Unicode_Path_cen_extra_field(z);
        }
    } else {
        // clear UTF-8 bit as not needed
        z->flg &= ~UTF8_BIT;
        z->lflg &= ~UTF8_BIT;
    }

    off = z->off;

    if (z->siz > ZIP_UWORD32_MAX || z->len > ZIP_UWORD32_MAX || z->off > ZIP_UWORD32_MAX ||
        z->dsk > ZIP_UWORD16_MAX || (force_zip64 == 1)) {
        iRes = add_central_zip64_extra_field(z);
        if (iRes != ZE_OK)
            co_return iRes;
    }

    append_ulong_to_mem(CENSIG, &block, &offset, &blocksize);  // central file header signature
    append_ushort_to_mem(z->vem, &block, &offset, &blocksize); // version made by
    append_ushort_to_mem(z->ver, &block, &offset, &blocksize); // version needed to extract
    append_ushort_to_mem(z->flg, &block, &offset, &blocksize); // general purpose bit flag
    append_ushort_to_mem(z->how, &block, &offset, &blocksize); // compression method
    append_ulong_to_mem(z->tim, &block, &offset, &blocksize);  // last mod file date time
    append_ulong_to_mem(z->crc, &block, &offset, &blocksize);  // crc-32
    if (z->siz > ZIP_UWORD32_MAX) {
        // instead of z->siz
        append_ulong_to_mem(ZIP_UWORD32_MAX, &block, &offset, &blocksize); // compressed size
    } else {
        append_ulong_to_mem((ulg)z->siz, &block, &offset, &blocksize); // compressed size
    }
    // if forcing Zip64 just force first ef field
    if (z->len > ZIP_UWORD32_MAX || (force_zip64 == 1)) {
        // instead of z->len
        append_ulong_to_mem(ZIP_UWORD32_MAX, &block, &offset, &blocksize); // uncompressed size
    } else {
        append_ulong_to_mem((ulg)z->len, &block, &offset, &blocksize); // uncompressed size
    }
    append_ushort_to_mem(nam, &block, &offset, &blocksize);     // file name length
    append_ushort_to_mem(z->cext, &block, &offset, &blocksize); // extra field length
    append_ushort_to_mem(z->com, &block, &offset, &blocksize);  // file comment length

    if (z->dsk > ZIP_UWORD16_MAX) {
        // instead of z->dsk
        append_ushort_to_mem((ush)ZIP_UWORD16_MAX, &block, &offset, &blocksize); // Zip64 flag
    } else {
        append_ushort_to_mem((ush)z->dsk, &block, &offset, &blocksize); // disk number start
    }
    append_ushort_to_mem(z->att, &block, &offset, &blocksize); // internal file attributes
    append_ulong_to_mem(z->atx, &block, &offset, &blocksize);  // external file attributes
    if (off > ZIP_UWORD32_MAX) {
        // instead of z->off
        append_ulong_to_mem(ZIP_UWORD32_MAX, &block, &offset, &blocksize); // Zip64 flag
    } else {
        append_ulong_to_mem((ulg)off, &block, &offset, &blocksize); // offset of local header
    }

    if (use_uname) {
        // path is UTF-8
        append_string_to_mem(z->uname, nam, &block, &offset, &blocksize);
    } else
        append_string_to_mem(z->iname, z->nam, &block, &offset, &blocksize);

    if (z->cext) {
        append_string_to_mem(z->cextra, z->cext, &block, &offset, &blocksize);
    }
    if (z->com) {
        append_string_to_mem(z->comment, z->com, &block, &offset, &blocksize);
    }

    // write the header
    if (co_await bfwrite(block, 1, offset, BFWRITE_CENTRALHEADER) != offset) {
        free(block);
        co_return ZE_TEMP;
    }
    free(block);

    co_return ZE_OK;
}

// Write the end of central directory data to file y.  Return an error code
// in the ZE_ class.

Task<int> putend(OFT(uzoff_t) n, OFT(uzoff_t) s, OFT(uzoff_t) c, OFT(extent) m, OFT(char *) z)
{
    ush vem; // version made by
    int iNeedZip64 = 0;

    char *block      = NULL; // mem block to write to
    extent offset    = 0;    // offset into block
    extent blocksize = 0;    // size of block

    // we have to create a zip64 archive if we have more than 64k - 1 entries,
    // if the CD is > 4 GB or if the offset to the CD > 4 GB. even if the CD start
    // is < 4 GB and CD start + CD size > 4GB we do not need a zip64 archive since
    // the offset entry in the CD tail is still valid.  [note that there are other
    // reasons for needing a Zip64 archive though, such as an uncompressed
    // size > 4 GB for an entry but the entry compresses below 4 GB, so the archive
    // is Zip64 but the CD does not need Zip64.]
    // order of the zip/zip64 records in a zip64 archive:
    // central directory
    // zip64 end of central directory record
    // zip64 end of central directory locator
    // end of central directory record

    // check zip64_archive instead of force_zip64 3/19/05

    zip64_eocd_disk   = current_disk;
    zip64_eocd_offset = bytes_this_split;

    if (n > ZIP_UWORD16_MAX || s > ZIP_UWORD32_MAX || c > ZIP_UWORD32_MAX || zip64_archive) {
        ++iNeedZip64;
        // write zip64 central dir tail:
        //
        // 4 bytes   zip64 end of central dir signature (0x06064b50)
        append_ulong_to_mem((ulg)ZIP64_CENTRAL_DIR_TAIL_SIG, &block, &offset, &blocksize);
        // 8 bytes   size of zip64 end of central directory record
        // a fixed size unless the end zip64 extensible data sector is used. - 3/19/05 EG
        // also note that AppNote 6.2 creates version 2 of this record for
        // central directory encryption - 3/19/05 EG
        append_int64_to_mem((zoff_t)ZIP64_CENTRAL_DIR_TAIL_SIZE, &block, &offset, &blocksize);

        // 2 bytes   version made by
        vem = OS_CODE + Z_MAJORVER * 10 + Z_MINORVER;
        append_ushort_to_mem(vem, &block, &offset, &blocksize);

        // APPNOTE says that zip64 archives should have at least version 4.5
        // in the "version needed to extract" field
        // 2 bytes   version needed to extract
        append_ushort_to_mem(ZIP64_MIN_VER, &block, &offset, &blocksize);

        // 4 bytes   number of this disk
        append_ulong_to_mem(current_disk, &block, &offset, &blocksize);
        // 4 bytes   number of the disk with the start of the central directory
        append_ulong_to_mem(cd_start_disk, &block, &offset, &blocksize);
        // 8 bytes   total number of entries in the central directory on this disk
        append_int64_to_mem(cd_entries_this_disk, &block, &offset, &blocksize);
        // 8 bytes   total number of entries in the central directory
        append_int64_to_mem(n, &block, &offset, &blocksize);
        // 8 bytes   size of the central directory
        append_int64_to_mem(s, &block, &offset, &blocksize);
        // 8 bytes   offset of start of central directory with respect to the starting disk number
        append_int64_to_mem(cd_start_offset, &block, &offset, &blocksize);
        // zip64 extensible data sector    (variable size), we don't use it...

        // write zip64 end of central directory locator:
        //
        // 4 bytes   zip64 end of central dir locator  signature (0x07064b50)
        append_ulong_to_mem(ZIP64_CENTRAL_DIR_TAIL_END_SIG, &block, &offset, &blocksize);
        // 4 bytes   number of the disk with the start of the zip64 end of central directory
        append_ulong_to_mem(zip64_eocd_disk, &block, &offset, &blocksize);
        // 8 bytes   relative offset of the zip64 end of central directory record, that is
        // offset of CD + CD size
        append_int64_to_mem(zip64_eocd_offset, &block, &offset, &blocksize);
        // PUTLLG(l64Temp, f);
        // 4 bytes   total number of disks
        append_ulong_to_mem(current_disk + 1, &block, &offset, &blocksize);
    }

    // end of central dir signature
    append_ulong_to_mem(ENDSIG, &block, &offset, &blocksize);
    // mv archives to come :)
    // for now use n for all
    // 2 bytes    number of this disk
    if (current_disk < 0xFFFF)
        append_ushort_to_mem((ush)current_disk, &block, &offset, &blocksize);
    else
        append_ushort_to_mem((ush)0xFFFF, &block, &offset, &blocksize);
    // 2 bytes    number of the disk with the start of the central directory
    if (cd_start_disk == (ulg)-1)
        cd_start_disk = 0;
    if (cd_start_disk < 0xFFFF)
        append_ushort_to_mem((ush)cd_start_disk, &block, &offset, &blocksize);
    else
        append_ushort_to_mem((ush)0xFFFF, &block, &offset, &blocksize);
    // 2 bytes    total number of entries in the central directory on this disk
    if (cd_entries_this_disk < 0xFFFF)
        append_ushort_to_mem((ush)cd_entries_this_disk, &block, &offset, &blocksize);
    else
        append_ushort_to_mem((ush)0xFFFF, &block, &offset, &blocksize);
    // 2 bytes    total number of entries in the central directory
    if (total_cd_entries < 0xFFFF)
        append_ushort_to_mem((ush)total_cd_entries, &block, &offset, &blocksize);
    else
        append_ushort_to_mem((ush)0xFFFF, &block, &offset, &blocksize);
    if (s > ZIP_UWORD32_MAX)
        // instead of s
        append_ulong_to_mem(ZIP_UWORD32_MAX, &block, &offset, &blocksize);
    else
        // 4 bytes    size of the central directory
        append_ulong_to_mem((ulg)s, &block, &offset, &blocksize);
    if (force_zip64 == 1 || cd_start_offset > ZIP_UWORD32_MAX)
        // instead of cd_start_offset
        append_ulong_to_mem(ZIP_UWORD32_MAX, &block, &offset, &blocksize);
    else
        // 4 bytes    offset of start of central directory with respect to the starting disk number
        append_ulong_to_mem((ulg)cd_start_offset, &block, &offset, &blocksize);

    // size of comment
    append_ushort_to_mem((ush)m, &block, &offset, &blocksize);
    // Write the comment, if any
    if (m) {
        // PKWare defines the archive comment to be ASCII only so no OEM conversion
        append_string_to_mem(z, m, &block, &offset, &blocksize);
    }

    // write the block
    if (co_await bfwrite(block, 1, offset, BFWRITE_HEADER) != offset) {
        free(block);
        co_return ZE_TEMP;
    }
    free(block);

#ifdef HANDLE_AMIGA_SFX
    if (amiga_sfx_offset && zipbeg /* -J zeroes this */) {
        s = co_await zftello(y);
        while (s & 3)
            s++, co_await zfputc(0, f); // final marker must be longword aligned
        PUTLG(0xF2030000 /* 1010 in Motorola byte order */, f);
        c = (s - amiga_sfx_offset - 4) / 4; // size of archive part in longwords
        if (co_await zfseeko(y, amiga_sfx_offset, SEEK_SET) != 0)
            co_return ZE_TEMP;
        c = ((c >> 24) & 0xFF) | ((c >> 8) & 0xFF00) | ((c & 0xFF00) << 8) |
            ((c & 0xFF) << 24); // invert byte order
        PUTLG(c, y);
        co_await zfseeko(y, 0, SEEK_END); // just in case
    }
#endif

    co_return ZE_OK;
} // end function putend()

// Note: a zip "entry" includes a local header (which includes the file
// name), an encryption header if encrypting, the compressed data
// and possibly an extended local header.
