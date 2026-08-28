// What configure probed for. There is one target here, so it is written out
// rather than generated: no mmap, no locking, no locale, always UTF-8.
#pragma once

#define VERSION "1.16.8"

#define USE_MULTIBYTE_CHARS 1
#define DISABLE_FILE_LOCKS  1

// Deliberately absent, and each of them steers an #ifdef upstream:
//   HAVE_MMAP HAVE_UNISTD_H HAVE_DIRENT_H HAVE_ALLOCA_H HAVE_LANGINFO_CODESET
//   HAVE_SYS_IOCTL_H HAVE_SYS_MMAN_H HAVE_SYS_MOUNT_H HAVE_SYS_PARAM_H
//   HAVE_SYS_TIMES_H HAVE_TIMES HAVE_FCHMOD HAVE_FTRUNCATE HAVE_STRCOLL
//   HAVE_LINUX_TIOCL_H WITH_MOUSE USE_LEGACY_TERM
