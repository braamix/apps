// crypt.h — traditional PKZIP encryption, by Info-ZIP.
//
// The encryption code is not copyrighted and is in the public domain; the
// notice -v prints says so and where it came from.
//
// unzip's __G macro layer, which threaded a globals struct through every call,
// is the identity for zip and is written out.
#pragma once

#include "zip.h"

#define CRYPT 1

#define CR_MAJORVER     2
#define CR_MINORVER     91
#define CR_BETA_VER     ""
#define CR_VERSION_DATE "05 Jan 2007" // last public release date

#define RAND_HEAD_LEN 12 // length of the encryption random header

int decrypt_byte(void);
int update_keys(int c);
void init_keys(ZCONST char *passwd);

// zencode(c, t) is the encrypted byte; t is scratch the caller declares.
#define zencode(c, t) (t = decrypt_byte(), update_keys(c), t ^ (c))

// The header the archive carries before an encrypted entry.
Task<void> crypthead OF((ZCONST char *, ulg));

// zip.h's zfwrite goes through this instead once encryption is built in: the
// buffer is encrypted in place and then handed to bfwrite.
Task<usize> zfwrite_crypt OF((zvoid *, extent, extent));

// zipcloak's two: one entry encrypted, one decrypted.
Task<int> zipcloak OF((struct zlist far *, ZCONST char *));
Task<int> zipbare OF((struct zlist far *, ZCONST char *));

// decrypt_byte for the reading half.
#define zdecode(c) update_keys(c ^= decrypt_byte())

#undef zfwrite
#define zfwrite(b, s, c) zfwrite_crypt(b, s, c)
