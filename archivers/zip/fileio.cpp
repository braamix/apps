// fileio.cpp — fileio.c, by Mark Adler. The half of it the OS decides.
//
// dostime, issymlnk and bfwrite's accounting are upstream's. The rest had to
// be rewritten: there is no localtime or mktime, no unlink or rename with an
// errno beside it, no chmod, and no mkstemp. What each became is in the
// README's table.
//
// The name lists, the pattern filter and the option parser are the other half
// of upstream's fileio.c and arrive with the whole create path.

#include "crc32.h"
#include "zip.h"

// Convert the date y/n/d and time h:m:s to a four byte DOS date and time.
// Note that this routine does not check for valid dates.
ulg dostime(int y, int n, int d, int h, int m, int s)
{
    return y < 1980 ? DOSTIME_MINIMUM /* dostime(1980, 1, 1, 0, 0, 0) */
                    : (((ulg)y - 1980) << 25) | ((ulg)n << 21) | ((ulg)d << 16) | ((ulg)h << 11) |
                          ((ulg)m << 5) | ((ulg)s >> 1);
}

// Return the Unix time t in DOS format, rounded up to the next two second
// boundary. Upstream reached localtime(); the offset is clock_now()'s here and
// civil() does the calendar.
ulg unix2dostime(time_t *t)
{
    return zdostime((u64)((*t + 1) & ~(time_t)1) * 1000);
}

// Return the Unix time from a DOS date and time. Upstream filled a struct tm
// and called mktime(); nothing inverts civil() here, so zdos2unix does.
time_t dos2unixtime(ulg dostime)
{
    return zdos2unix((u32)dostime);
}

// Return true if the attributes are those of a symbolic link.
int issymlnk(ulg a)
{
    return ((a >> 16) & S_IFMT) == S_IFLNK;
}

// The link's target, into buf. read_link answers the target as it was stored.
Task<extent> rdsymlnk(char *p, char *buf, extent n)
{
    Result<String> r = Err(Error::NoMemory);
    if (Task<Result<String>> t = read_link(Str(p, strlen(p))))
        r = co_await t;
    if (r.is_err())
        co_return 0;

    extent k = r.value().size() < n ? r.value().size() : n;
    memcpy(buf, r.value().data(), k);
    co_return k;
}

// Delete the file f.
Task<int> destroy(char *f)
{
    Result<void> r = Err(Error::NoMemory);
    if (Task<Result<void>> t = remove_path(Str(f, strlen(f)), false))
        r = co_await t;
    co_return r.is_ok() ? 0 : -1;
}

// Move the file s to the file d, or copy it if a move will not do.
Task<int> replace(char *d, char *s)
{
    Str dst(d, strlen(d)), src(s, strlen(s));

    // A rename keeps the mtime and a copy cannot, so it is worth trying first.
    // Err(Unsupported) is the store saying it cannot move this one, which is
    // an instruction rather than a failure — upstream read EXDEV the same way.
    Result<void> r = Err(Error::NoMemory);
    if (Task<Result<void>> t = rename_path(src, dst))
        r = co_await t;
    if (r.is_ok())
        co_return ZE_OK;
    if (r.error() != Error::Unsupported)
        co_return ZE_CREAT;

    if (Task<Result<void>> t = copy_file(src, dst))
        r = co_await t;
    if (r.is_err())
        co_return ZE_CREAT;
    if (Task<Result<void>> t = remove_path(src, false))
        r = co_await t;
    co_return ZE_OK;
}

// Upstream read and wrote a file's mode here. The filesystem keeps none, so
// what a new entry carries is synthesized in filetime() and these two do
// nothing: `zip` only ever used them to carry a mode from one file to another.
int getfileattr(char *f)
{
    return 0;
}

int setfileattr(char *f, int a)
{
    return 0;
}

// A name for the temporary archive. Upstream handed a "ziXXXXXX" template to
// mkstemp; there is none here, so the name carries proc_random() and the open
// that follows names SYS_O_EXCL, which is the same guarantee.
char *tempname(char *zip)
{
    char *t;

    if (tempath != NULL) {
        if ((t = (char *)malloc(strlen(tempath) + 20)) == NULL)
            return NULL;
        strcpy(t, tempath);
        if (lastchar(t) != '/')
            strcat(t, "/");
    } else {
        // Beside the archive, so the rename that replaces it is within one
        // directory and can be a rename rather than a copy.
        usize n = 0;
        for (usize i = strlen(zip); i > 0; i--)
            if (zip[i - 1] == '/') {
                n = i;
                break;
            }
        if ((t = (char *)malloc(n + 20)) == NULL)
            return NULL;
        memcpy(t, zip, n);
        t[n] = 0;
    }

    char stamp[10];
    u32 r = proc_random();
    for (int i = 0; i < 8; i++, r >>= 4)
        stamp[i] = "0123456789abcdef"[r & 0xf];
    stamp[8] = 0;

    strcat(t, "zi");
    strcat(t, stamp);
    return t;
}

// Close the split disk_number is on and give it its final name. Splits arrive
// with the update path; nothing calls this yet.
Task<int> close_split(ulg disk_number, FILE *tempfile, char *temp_name)
{
    co_return ZE_OK;
}

// bfwrite
// Does the fwrite but also counts bytes and does splits.
Task<usize> bfwrite(ZCONST void *buffer, usize size, usize count, int mode)
{
    usize bytes_written = 0;
    usize r;
    usize b                     = size * count;
    uzoff_t bytes_left_in_split = 0;
    usize bytes_to_write        = b;

    // --------------------------------
    // local header
    if (mode == BFWRITE_LOCALHEADER) {
        // writing local header - reset entry data count
        bytes_this_entry = 0;
        // save start of local header so we can rewrite later
        current_local_file   = y;
        current_local_disk   = current_disk;
        current_local_offset = bytes_this_split;
    }

    if (split_size == 0)
        bytes_left_in_split = bytes_to_write;
    else
        bytes_left_in_split = split_size - bytes_this_split;

    if (bytes_to_write > bytes_left_in_split) {
        if (mode == BFWRITE_HEADER || mode == BFWRITE_LOCALHEADER ||
            mode == BFWRITE_CENTRALHEADER) {
            // if can't write entire header save for next split
            bytes_to_write = 0;
        } else {
            // normal data so fill the split
            bytes_to_write = (usize)bytes_left_in_split;
        }
    }

    // --------------------------------
    // central header
    if (mode == BFWRITE_CENTRALHEADER) {
        // set start disk for CD
        if (cd_start_disk == (ulg)-1) {
            cd_start_disk   = current_disk;
            cd_start_offset = bytes_this_split;
        }
        cd_entries_this_disk++;
        total_cd_entries++;
    }

    // --------------------------------
    if (bytes_to_write > 0) {
        // write out the bytes for this split
        r = co_await zwrite(buffer, size, bytes_to_write, y);
        bytes_written += r;
        bytes_to_write = b - r;
        bytes_this_split += r;
        if (mode == BFWRITE_DATA)
            // if data descriptor do not include in count
            bytes_this_entry += r;
    } else {
        bytes_to_write = b;
    }

    // Upstream closed the split here and opened the next. Splits arrive with
    // the update path; until then the only way to be short is a failed write.
    if (bytes_to_write > 0)
        ZIPERR(ZE_WRITE, "write error on zip file")

    co_return bytes_written;
}

// Reduce all path components to MSDOS upper case 8.3 style names.
char *msname(char *n)
{
    int c; // current character
    int f; // characters in current component
    char *p;
    char *q;

    p = q = n;
    f     = 0;
    while ((c = (unsigned char)*POSTINCSTR(p)) != 0)
        if (c == ' ' || c == ':' || c == '"' || c == '*' || c == '+' || c == ',' || c == ';' ||
            c == '<' || c == '=' || c == '>' || c == '?' || c == '[' || c == ']' || c == '|')
            continue; // char is discarded
        else if (c == '/') {
            *POSTINCSTR(q) = (char)c;
            f              = 0; // new component
        } else if (c == '.') {
            if (f == 0)
                continue; // leading dots are discarded
            else if (f < 9) {
                *POSTINCSTR(q) = (char)c;
                f              = 9; // now in file type
            } else
                f = 12; // now just excess characters
        } else if (f < 12 && f != 8) {
            f += CLEN(p); // do until end of name or type
            *POSTINCSTR(q) = (char)(to_up(c));
        }
    *q = 0;
    return n;
}

// Return a pointer to the start of the last path component. For a directory
// name terminated by the character in c, the return value is an empty string.
char *last(char *p, int c)
{
    char *t;

    if ((t = strrchr(p, c)) != NULL)
        return t + 1;
    else
        return p;
}
