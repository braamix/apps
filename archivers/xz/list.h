// SPDX-License-Identifier: 0BSD
#pragma once

#include "proc/io.h"

///////////////////////////////////////////////////////////////////////////////
//
/// \file       list.h
/// \brief      List information about .xz files
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

/// \brief      List information about the given .xz file
extern Task<void> list_file(const char *filename);

/// \brief      Show the totals after all files have been listed
extern Task<void> list_totals(void);
