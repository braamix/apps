/*
 ****************************************************************************
 *
 *                   "DHRYSTONE" Benchmark Program
 *                   -----------------------------
 *
 *  Version:    C, Version 2.1
 *
 *  File:       dhry_1.cpp (part 2 of 4)
 *
 *  Date:       May 25, 1988
 *
 *  Author:     Reinhold P. Weicker
 *
 *  Braam:      printf became Buf, malloc became heap_new, scanf became the
 *              command line, and gettimeofday became proc_now. The measurement
 *              loop's body is untouched; what encloses it is entered a stretch
 *              at a time, so that a signal can reach the run.
 *
 ****************************************************************************
 */

#include "dhry.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "kernel/text.h"
#include "math/ftoa.h"
#include "proc/io.h"

/* Runs when the command line names no number. The clock counts whole
   milliseconds and the report wants two seconds, so this is set to take a few:
   fifty million measured 2.3 s, which leaves too little margin. */
#define NRUNS 100000000

#define DHRYSTONES_PER_DMIPS 1757

/* Global Variables: */

Rec_Pointer Ptr_Glob, Next_Ptr_Glob;
int Int_Glob;
Boolean Bool_Glob;
char Ch_1_Glob, Ch_2_Glob;
int Arr_1_Glob[50];
int Arr_2_Glob[50][50];

/* variables for time measurement: */

u32 Begin_Time, End_Time, User_Time;
float Microseconds, Dhrystones_Per_Second;

/* end of variables for time measurement */

namespace {

constexpr usize REPORT = 4096;

/* Proc_0's locals, copied out after the timer stops. The original printed them
   from main, which held the loop and the printfs both; here the loop is an
   ordinary function and the writing is a coroutine, so they travel. */
struct Local_Vars {
    One_Fifty Int_1_Loc;
    One_Fifty Int_2_Loc;
    One_Fifty Int_3_Loc;
    Enumeration Enum_Loc;
    Str_30 Str_1_Loc;
    Str_30 Str_2_Loc;
    int Number_Of_Runs;
};

Local_Vars Final_Loc;

/* Proc_0's local: the loop is entered a stretch at a time, and a local would
   not survive between two. */
Str_30 Str_1_Loc;

/* A run stopped by ^C. */
bool Interrupted;

/* printf's %-w.df: the conversion braam::math's, the padding Buf's. */
void put_fixed(Buf<REPORT> &b, float v, i32 decimals, usize width)
{
    Buf<32> t;
    put_f64(t, double(v), decimals, 'f');
    b.put_left(t.str(), width);
}

} // namespace

/* Proc_0's initialisations, which the original ran before starting the timer.
   Nought if the two records could not be had. */
int main_init(void)
{
    /* Initializations */

    Next_Ptr_Glob = heap_new<Rec_Type>();
    Ptr_Glob      = heap_new<Rec_Type>();
    if (Next_Ptr_Glob == 0 || Ptr_Glob == 0)
        return 0;

    Ptr_Glob->Ptr_Comp                = Next_Ptr_Glob;
    Ptr_Glob->Discr                   = Ident_1;
    Ptr_Glob->variant.var_1.Enum_Comp = Ident_3;
    Ptr_Glob->variant.var_1.Int_Comp  = 40;
    strcpy(Ptr_Glob->variant.var_1.Str_Comp, "DHRYSTONE PROGRAM, SOME STRING");
    strcpy(Str_1_Loc, "DHRYSTONE PROGRAM, 1'ST STRING");

    Arr_2_Glob[8][7] = 10;
    /* Was missing in published program. Without this statement,    */
    /* Arr_2_Glob [8][7] would have an undefined value.             */
    /* Warning: With 16-Bit processors and Number_Of_Runs > 32000,  */
    /* overflow may occur for this array element.                   */

    Final_Loc.Number_Of_Runs = 0;
    return 1;
}

/* noinline: proc_main is a coroutine, and inlining this into it would move the
   benchmark's locals into a heap-allocated frame. */
__attribute__((noinline)) void main_body(int Runs)
/*****/

/* One stretch of the measurement loop; the body is the original's. The timer */
/* belongs to the whole run and is started outside.                           */
{
    One_Fifty Int_1_Loc;
    One_Fifty Int_2_Loc;
    One_Fifty Int_3_Loc;
    char Ch_Index;
    Enumeration Enum_Loc;
    Str_30 Str_2_Loc;
    int Run_Index;
    int Number_Of_Runs = Runs;

    for (Run_Index = 1; Run_Index <= Number_Of_Runs; ++Run_Index) {
        Proc_5();
        Proc_4();
        /* Ch_1_Glob == 'A', Ch_2_Glob == 'B', Bool_Glob == true */
        Int_1_Loc = 2;
        Int_2_Loc = 3;
        strcpy(Str_2_Loc, "DHRYSTONE PROGRAM, 2'ND STRING");
        Enum_Loc  = Ident_2;
        Bool_Glob = !Func_2(Str_1_Loc, Str_2_Loc);
        /* Bool_Glob == 1 */
        while (Int_1_Loc < Int_2_Loc) /* loop body executed once */
        {
            Int_3_Loc = 5 * Int_1_Loc - Int_2_Loc;
            /* Int_3_Loc == 7 */
            Proc_7(Int_1_Loc, Int_2_Loc, &Int_3_Loc);
            /* Int_3_Loc == 7 */
            Int_1_Loc += 1;
        } /* while */
        /* Int_1_Loc == 3, Int_2_Loc == 3, Int_3_Loc == 7 */
        Proc_8(Arr_1_Glob, Arr_2_Glob, Int_1_Loc, Int_3_Loc);
        /* Int_Glob == 5 */
        Proc_1(Ptr_Glob);
        for (Ch_Index = 'A'; Ch_Index <= Ch_2_Glob; ++Ch_Index)
        /* loop body executed twice */
        {
            if (Enum_Loc == Func_1(Ch_Index, 'C'))
            /* then, not executed */
            {
                Proc_6(Ident_1, &Enum_Loc);
                strcpy(Str_2_Loc, "DHRYSTONE PROGRAM, 3'RD STRING");
                Int_2_Loc = Run_Index;
                Int_Glob  = Run_Index;
            }
        }
        /* Int_1_Loc == 3, Int_2_Loc == 3, Int_3_Loc == 7 */
        Int_2_Loc = Int_2_Loc * Int_1_Loc;
        Int_1_Loc = Int_2_Loc / Int_3_Loc;
        Int_2_Loc = 7 * (Int_2_Loc - Int_3_Loc) - Int_1_Loc;
        /* Int_1_Loc == 1, Int_2_Loc == 13, Int_3_Loc == 7 */
        Proc_2(&Int_1_Loc);
        /* Int_1_Loc == 5 */

    } /* loop "for Run_Index" */

    /* Out of the frame, so that the report can be written from a coroutine.
       The count accumulates: the report divides by the runs that were made. */
    Final_Loc.Int_1_Loc = Int_1_Loc;
    Final_Loc.Int_2_Loc = Int_2_Loc;
    Final_Loc.Int_3_Loc = Int_3_Loc;
    Final_Loc.Enum_Loc  = Enum_Loc;
    Final_Loc.Number_Of_Runs += Number_Of_Runs;
    strcpy(Final_Loc.Str_1_Loc, Str_1_Loc);
    strcpy(Final_Loc.Str_2_Loc, Str_2_Loc);
}

namespace {

/* Every printf of the original, in order, into one buffer. */
void report(Buf<REPORT> &b)
{
    b.put("Execution ends\n");
    b.put("\n");
    b.put("Final values of the variables used in the benchmark:\n");
    b.put("\n");
    b.put("Int_Glob:            ").put(Int_Glob).put('\n');
    b.put("        should be:   ").put(5).put('\n');
    b.put("Bool_Glob:           ").put(Bool_Glob).put('\n');
    b.put("        should be:   ").put(1).put('\n');
    b.put("Ch_1_Glob:           ").put(Ch_1_Glob).put('\n');
    b.put("        should be:   ").put('A').put('\n');
    b.put("Ch_2_Glob:           ").put(Ch_2_Glob).put('\n');
    b.put("        should be:   ").put('B').put('\n');
    b.put("Arr_1_Glob[8]:       ").put(Arr_1_Glob[8]).put('\n');
    b.put("        should be:   ").put(7).put('\n');
    b.put("Arr_2_Glob[8][7]:    ").put(Arr_2_Glob[8][7]).put('\n');
    b.put("        should be:   Number_Of_Runs + 10\n");
    b.put("Ptr_Glob->\n");
    b.put("  Ptr_Comp:          ").put_hex(proc_addr(Ptr_Glob->Ptr_Comp)).put('\n');
    b.put("        should be:   (implementation-dependent)\n");
    b.put("  Discr:             ").put(int(Ptr_Glob->Discr)).put('\n');
    b.put("        should be:   ").put(0).put('\n');
    b.put("  Enum_Comp:         ").put(int(Ptr_Glob->variant.var_1.Enum_Comp)).put('\n');
    b.put("        should be:   ").put(2).put('\n');
    b.put("  Int_Comp:          ").put(Ptr_Glob->variant.var_1.Int_Comp).put('\n');
    b.put("        should be:   ").put(17).put('\n');
    b.put("  Str_Comp:          ").put(Ptr_Glob->variant.var_1.Str_Comp).put('\n');
    b.put("        should be:   DHRYSTONE PROGRAM, SOME STRING\n");
    b.put("Next_Ptr_Glob->\n");
    b.put("  Ptr_Comp:          ").put_hex(proc_addr(Next_Ptr_Glob->Ptr_Comp)).put('\n');
    b.put("        should be:   (implementation-dependent), same as above\n");
    b.put("  Discr:             ").put(int(Next_Ptr_Glob->Discr)).put('\n');
    b.put("        should be:   ").put(0).put('\n');
    b.put("  Enum_Comp:         ").put(int(Next_Ptr_Glob->variant.var_1.Enum_Comp)).put('\n');
    b.put("        should be:   ").put(1).put('\n');
    b.put("  Int_Comp:          ").put(Next_Ptr_Glob->variant.var_1.Int_Comp).put('\n');
    b.put("        should be:   ").put(18).put('\n');
    b.put("  Str_Comp:          ").put(Next_Ptr_Glob->variant.var_1.Str_Comp).put('\n');
    b.put("        should be:   DHRYSTONE PROGRAM, SOME STRING\n");
    b.put("Int_1_Loc:           ").put(Final_Loc.Int_1_Loc).put('\n');
    b.put("        should be:   ").put(5).put('\n');
    b.put("Int_2_Loc:           ").put(Final_Loc.Int_2_Loc).put('\n');
    b.put("        should be:   ").put(13).put('\n');
    b.put("Int_3_Loc:           ").put(Final_Loc.Int_3_Loc).put('\n');
    b.put("        should be:   ").put(7).put('\n');
    b.put("Enum_Loc:            ").put(int(Final_Loc.Enum_Loc)).put('\n');
    b.put("        should be:   ").put(1).put('\n');
    b.put("Str_1_Loc:           ").put(Final_Loc.Str_1_Loc).put('\n');
    b.put("        should be:   DHRYSTONE PROGRAM, 1'ST STRING\n");
    b.put("Str_2_Loc:           ").put(Final_Loc.Str_2_Loc).put('\n');
    b.put("        should be:   DHRYSTONE PROGRAM, 2'ND STRING\n");
    b.put("\n");

    if (Interrupted) {
        b.put("Interrupted after ").put(u32(Final_Loc.Number_Of_Runs)).put(" runs\n");
        b.put("\n");
    }

    User_Time = End_Time - Begin_Time;

    /* Measurements should last at least 2 seconds */
    if (User_Time < 2000) {
        b.put("Measured time too small to obtain meaningful results\n");
        b.put("Please increase number of runs\n");
        b.put("\n");
    } else {
        /* The clock counts milliseconds where gettimeofday counted
           microseconds; the two scale factors move by a thousand. */
        Microseconds          = (float)User_Time * 1000 / (float)Final_Loc.Number_Of_Runs;
        Dhrystones_Per_Second = (float)Final_Loc.Number_Of_Runs * 1000 / (float)User_Time;

        b.put("Nanoseconds for one run through Dhrystone: ");
        put_fixed(b, Microseconds * 1000, 1, 7);
        b.put(" \n");
        b.put("            Million Dhrystones per Second: ");
        put_fixed(b, Dhrystones_Per_Second / 1000000, 3, 7);
        b.put(" \n");
        b.put("                                    DMIPS: ");
        put_fixed(b, Dhrystones_Per_Second / DHRYSTONES_PER_DMIPS, 1, 7);
        b.put(" \n");
        b.put("\n");
    }
}

} // namespace

Task<i32> proc_main(Args args)
{
    if (args.size() > 2) {
        co_await write_all(SYS_STDERR, "usage: dhrystone [runs]\n");
        co_return 2;
    }

    int Number_Of_Runs = NRUNS;
    if (args.size() == 2) {
        Option<u32> n = parse_u32(args[1]);
        if (!n.has_value() || n.value() == 0 || n.value() > 0x7fffffff) {
            co_await errln("dhrystone", args[1], Error::Invalid);
            co_return 2;
        }
        Number_Of_Runs = int(n.value());
    }

    /* Written before the run, because the loop itself writes nothing. */
    Buf<128> head;
    head.put("\n");
    head.put("Dhrystone Benchmark, Version 2.1 (Language: C)\n");
    head.put("\n");
    head.put("Execution starts, ").put(Number_Of_Runs).put(" runs through Dhrystone\n");
    if ((co_await write_all(SYS_STDOUT, head.str())).is_err())
        co_return 1;

    if (!main_init()) {
        co_await errln("dhrystone", "Rec_Type", Error::NoMemory);
        co_return 1;
    }

    /* The loop cannot suspend, so a signal reaches this program only between
       two stretches of it. A fixed count of them rather than a fixed length
       bounds the cost: 64 syscalls at 34-45 us, inside the measured interval
       and under 3 ms of a run of seconds. */
    constexpr int CHUNKS = 64;
    int step             = Number_Of_Runs / CHUNKS;
    if (step < 1)
        step = 1;

    if (Task<Result<void>> t = sig_catch(SIG_INT))
        co_await t;

    /***************/
    /* Start timer */
    /***************/

    Begin_Time = proc_now();

    for (int done = 0; done < Number_Of_Runs;) {
        int runs = Number_Of_Runs - done < step ? Number_Of_Runs - done : step;
        main_body(runs);
        done += runs;
        if (done >= Number_Of_Runs)
            break;
        if (Task<Result<void>> t = sleep_for(0))
            co_await t;
        if (sig_take(SIG_INT)) {
            Interrupted = true;
            break;
        }
    }

    /**************/
    /* Stop timer */
    /**************/

    End_Time = proc_now();

    /* Four kilobytes is a coroutine frame's worth many times over. */
    Buf<REPORT> *b = heap_new<Buf<REPORT>>();
    if (b == 0) {
        co_await errln("dhrystone", "report", Error::NoMemory);
        co_return 1;
    }
    report(*b);
    Result<void> w = co_await write_all(SYS_STDOUT, b->str());
    heap_delete(b);
    if (w.is_err())
        co_return w.error() == Error::Cancelled ? 130 : 1;

    co_return Interrupted ? 130 : 0;
}

void Proc_1(Rec_Pointer Ptr_Val_Par)
/******************/
/* executed once */
{
    Rec_Pointer Next_Record = Ptr_Val_Par->Ptr_Comp;
    /* == Ptr_Glob_Next */
    /* Local variable, initialized with Ptr_Val_Par->Ptr_Comp,    */
    /* corresponds to "rename" in Ada, "with" in Pascal           */

    *Ptr_Val_Par->Ptr_Comp              = *Ptr_Glob;
    Ptr_Val_Par->variant.var_1.Int_Comp = 5;
    Next_Record->variant.var_1.Int_Comp = Ptr_Val_Par->variant.var_1.Int_Comp;
    Next_Record->Ptr_Comp               = Ptr_Val_Par->Ptr_Comp;
    Proc_3(&Next_Record->Ptr_Comp);
    /* Ptr_Val_Par->Ptr_Comp->Ptr_Comp
                        == Ptr_Glob->Ptr_Comp */
    if (Next_Record->Discr == Ident_1)
    /* then, executed */
    {
        Next_Record->variant.var_1.Int_Comp = 6;
        Proc_6(Ptr_Val_Par->variant.var_1.Enum_Comp, &Next_Record->variant.var_1.Enum_Comp);
        Next_Record->Ptr_Comp = Ptr_Glob->Ptr_Comp;
        Proc_7(Next_Record->variant.var_1.Int_Comp, 10, &Next_Record->variant.var_1.Int_Comp);
    } else /* not executed */
        *Ptr_Val_Par = *Ptr_Val_Par->Ptr_Comp;
} /* Proc_1 */

void Proc_2(One_Fifty *Int_Par_Ref)
/******************/
/* executed once */
/* *Int_Par_Ref == 1, becomes 4 */
{
    One_Fifty Int_Loc;
    Enumeration Enum_Loc;

    Int_Loc = *Int_Par_Ref + 10;
    do /* executed once */
        if (Ch_1_Glob == 'A')
        /* then, executed */
        {
            Int_Loc -= 1;
            *Int_Par_Ref = Int_Loc - Int_Glob;
            Enum_Loc     = Ident_1;
        } /* if */
    while (Enum_Loc != Ident_1); /* true */
} /* Proc_2 */

void Proc_3(Rec_Pointer *Ptr_Ref_Par)
/******************/
/* executed once */
/* Ptr_Ref_Par becomes Ptr_Glob */
{
    if (Ptr_Glob != 0)
        /* then, executed */
        *Ptr_Ref_Par = Ptr_Glob->Ptr_Comp;
    Proc_7(10, Int_Glob, &Ptr_Glob->variant.var_1.Int_Comp);
} /* Proc_3 */

void Proc_4() /* without parameters */
/*******/
/* executed once */
{
    Boolean Bool_Loc;

    Bool_Loc  = Ch_1_Glob == 'A';
    Bool_Glob = Bool_Loc | Bool_Glob;
    Ch_2_Glob = 'B';
} /* Proc_4 */

void Proc_5() /* without parameters */
/*******/
/* executed once */
{
    Ch_1_Glob = 'A';
    Bool_Glob = false;
} /* Proc_5 */
