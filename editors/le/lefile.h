// FILE, which is proc/file.h's File, and the four stdio calls the config
// parsers make of it.
//
// The config files -- syntax, keymap, colors, mainmenu, le.hlp, history2 --
// were read with stdio and are read with this. get(), put(), read() and
// write() are awaiters, so a buffer hit costs no coroutine frame; a refill is
// a syscall, which suspends and hands the native stack back.
//
// These are bytes and not runes. The grammars are byte-oriented -- a \ooo
// escape in a keymap names a byte, and a colour description is ASCII -- and
// File::get() would hand back a codepoint.
#pragma once

#include "proc/file.h"

typedef File FILE;

// One byte, or EOF at end of input.
Task<int> le_getc(FILE *f);

// One byte back, in front of what is buffered. ASCII only, which is all these
// parsers push back: File::unget takes a rune, and a byte at 0x80 or above
// would go back as two.
void le_ungetc(int c, FILE *f);

Task<void> le_putc(int c, FILE *f);
Task<void> le_puts(const char *s, FILE *f);

// Opens for reading, or for writing and truncating. Nothing here appends.
Task<FILE *> le_fopen(const char *path, bool write);
Task<void> le_fclose(FILE *f);
