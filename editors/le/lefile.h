// FILE, which is proc/file.h's File.
//
// The config files -- syntax, keymap, colors, mainmenu, le.hlp, history2 --
// were read with stdio and are read with this. get(), put(), read() and
// write() are awaiters, so a buffer hit costs no coroutine frame; a refill is
// a syscall, which suspends and hands the native stack back.
#pragma once

#include "proc/file.h"

typedef File FILE;
