/*
 ****************************************************************************
 *
 *                   "DHRYSTONE" Benchmark Program
 *                   -----------------------------
 *
 *  Version:    C, Version 2.1
 *
 *  File:       dhry.h (part 1 of 4)
 *
 *  Date:       May 25, 1988
 *
 *  Author:     Reinhold P. Weicker
 *                      Siemens AG, AUT E 51
 *                      Postfach 3220
 *                      8520 Erlangen
 *                      Germany (West)
 *
 *              Original Version (in Ada) published in
 *              "Communications of the ACM" vol. 27., no. 10 (Oct. 1984),
 *              pp. 1013 - 1030, together with the statistics
 *              on which the distribution of statements etc. is based.
 *
 *              In this C version, the following C library functions are used:
 *              - strcpy, strcmp (inside the measurement loop)
 *              - printf, scanf (outside the measurement loop)
 *              In addition, Berkeley UNIX system calls "times ()" or "time ()"
 *              are used for execution time measurement. For measurements
 *              on other systems, these calls have to be changed.
 *
 ***************************************************************************
 *
 *  Compilation model and measurement (IMPORTANT):
 *
 *  This Braam version of Dhrystone consists of four files:
 *  - dhry.h (this file, containing global definitions)
 *  - dhry_1.cpp (containing the code corresponding to Ada package Pack_1)
 *  - dhry_2.cpp (containing the code corresponding to Ada package Pack_2)
 *  - dhry_lib.cpp (strcpy and strcmp, which came from the C library)
 *
 *  The following "ground rules" apply for measurements:
 *  - Separate compilation
 *  - No procedure merging
 *  - Otherwise, compiler optimizations are allowed but should be indicated
 *  - Default results are those without register declarations
 *  See the companion paper "Rationale for Dhrystone Version 2" for a more
 *  detailed discussion of these ground rules.
 *
 *  README.md carries the rest of the original commentary: the history, the
 *  statement distribution the benchmark is built on, and what this port
 *  changed.
 *
 ***************************************************************************
 */

/* Compiler and system dependent definitions: */

typedef enum { Ident_1, Ident_2, Ident_3, Ident_4, Ident_5 } Enumeration;

/* General definitions: */

/* The original defined true and false here. C++ has them as keywords, and
   Boolean is int, so the built-in literals convert to the same 1 and 0. */

typedef int One_Thirty;
typedef int One_Fifty;
typedef char Capital_Letter;
typedef int Boolean;
typedef char Str_30[31];
typedef int Arr_1_Dim[50];
typedef int Arr_2_Dim[50][50];

typedef struct record {
    struct record *Ptr_Comp;
    Enumeration Discr;
    union {
        struct {
            Enumeration Enum_Comp;
            int Int_Comp;
            char Str_Comp[31];
        } var_1;
        struct {
            Enumeration E_Comp_2;
            char Str_2_Comp[31];
        } var_2;
        struct {
            char Ch_1_Comp;
            char Ch_2_Comp;
        } var_3;
    } variant;
} Rec_Type, *Rec_Pointer;

/* Forward declaration necessary since Enumeration may not simply be int */
void Proc_1(Rec_Pointer);
void Proc_2(One_Fifty *);
void Proc_3(Rec_Pointer *);
void Proc_4(void);
void Proc_5(void);
void Proc_6(Enumeration, Enumeration *);
void Proc_7(One_Fifty, One_Fifty, One_Fifty *);
void Proc_8(Arr_1_Dim, Arr_2_Dim, int, int);
Enumeration Func_1(Capital_Letter, Capital_Letter);
Boolean Func_2(Str_30, Str_30);
Boolean Func_3(Enumeration);

/* The two library routines the benchmark calls. Braam has no C library, so
   dhry_lib.cpp supplies them; a translation unit of their own keeps the call
   a call, as it was when it went to libc.

   extern "C" because the compiler emits its own call to strlen, by the C name,
   wherever it expands __builtin_strlen. */
extern "C" {

char *strcpy(char *Dst, const char *Src);
int strcmp(const char *Str_1, const char *Str_2);

/* Not the benchmark's: Str reaches for it when the report prints a Str_30. */
__SIZE_TYPE__ strlen(const char *Str);

} // extern "C"
