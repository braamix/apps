/*
 ****************************************************************************
 *
 *                   "DHRYSTONE" Benchmark Program
 *                   -----------------------------
 *
 *  File:       dhry_lib.cpp (part 4 of 4)
 *
 *  The C library routines the benchmark calls. Braam has no libc, so they are
 *  written out here.
 *
 *  A translation unit of their own on purpose: strcpy and strcmp are called
 *  from inside the measurement loop, and a cross-unit call is what they were
 *  when they came from libc. "Understanding Variations in Dhrystone
 *  Performance" measured the two at a quarter to a third of the score, so how
 *  they are reached is part of what is being measured.
 *
 *  strlen is not the benchmark's: it is what Str reaches for when the report
 *  prints a Str_30.
 *
 ****************************************************************************
 */

#include "dhry.h"

char *strcpy(char *Dst, const char *Src)
{
    char *Ptr_Loc = Dst;

    while ((*Ptr_Loc++ = *Src++) != '\0')
        ;
    return Dst;
}

int strcmp(const char *Str_1, const char *Str_2)
{
    while (*Str_1 != '\0' && *Str_1 == *Str_2) {
        Str_1++;
        Str_2++;
    }
    return int((unsigned char)*Str_1) - int((unsigned char)*Str_2);
}

__SIZE_TYPE__ strlen(const char *Str)
{
    const char *Ptr_Loc = Str;

    while (*Ptr_Loc != '\0')
        Ptr_Loc++;
    return (__SIZE_TYPE__)(Ptr_Loc - Str);
}
