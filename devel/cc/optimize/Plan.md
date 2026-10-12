# Plan: the open items after the MSP430 `sort` work

Status after `241f7a5`: on MSP430, `sort` takes 32 686 cycles and 312 bytes; GCC `-O2`
takes 32 845 and 284. Three items are left. They are independent, and each step below
ends with `ctest -j8` green.

1. The inner loop's guard, which GCC proves always true (≈ 190 cycles on MSP430).
2. The negative offset `q - c`, which the in-place step leaves behind, is not folded into
   the load or store on AArch64, RISC-V and AVR.
3. A constant pointer step goes through a register on AArch64 and RISC-V
   (`mov x10,#1; add x4,x4,x10,lsl #2`).

Items 2 and 3 share a root: on these backends a constant `ADD_PTR` index is
materialized like a variable one. So the backend work starts there.

---

## A. Constant `ADD_PTR` as an immediate (items 2 and 3, AArch64 and RISC-V)

### A1. AArch64: select `add`/`sub` with an immediate

- `backend/aarch64/instr.c` `gen_add_ptr`: when the index is a constant, compute
  `off = index * scale`, wrapped to 64 bits.
  - If `|off|` takes an arithmetic immediate (`arith_imm` in `peephole.c`: 12 bits,
    optionally `lsl #12`; move it to `a64_ir.c` or duplicate the test), emit
    `add d, p, #off` or `sub d, p, #-off`.
  - Otherwise, emit `gen_li` and a plain `add`.
  - Skip the `sxtw` and the `mul`/shift path entirely.
- The peephole's `fold_address` already folds `add/sub t, p, #k` into the next load or
  store's `[p, #off]` when `t` dies there, and `fits_ldst` accepts `-256…255`
  (`ldur`/`stur`). So `add x1,x4,#-4; str w2,[x1]` should become `str w2,[x4,#-4]`
  with no new peephole code. Check this on the sort inner loop.
- Tests:
  - Update the `backend/aarch64/test/ptr_tests.cpp` goldens for the constant index
    (expect many golden changes; review the diff by hand).
  - Add a golden for the bubble-sort inner loop:
    `ldr w3,[x4]; add x4,x4,#4; ldr w2,[x4]; … str w2,[x4,#-4]; str w3,[x4]`.
    Even better would be the post-indexed `ldr w3,[x4],#4`, which is a separate,
    optional peephole rule (`ldr r,[p]` followed by `add p,p,#size` with `p` not read
    in between gives `ldr r,[p],#size`), the analogue of MSP430's `@rN+`.
- Run the interop and book tests: they compare against clang, so the meaning cannot
  silently change.

### A2. RISC-V: `addi`, and folding it into the access

- `backend/riscv/instr.c` `gen_add_ptr`:
  - When the index is a constant and `off = index * scale` fits 12 bits, emit
    `addi d, p, off`.
  - Otherwise, use `li` + `add`, with no shift and no `mul`.
  - Keep the `pair_half` path (`long long` index on RV32) for a variable index only.
- `backend/riscv/peephole.c`: new rule in `rewrite`. Given `addi t, p, k` followed by a
  load or store whose base is `t`, where `t` dies there (`last_read`, and `t` is not
  the stored value) and `off + k` fits 12 bits: rewrite the access to `off+k(p)` and
  delete the `addi`. This mirrors AArch64's `fold_address`; `reads_as_base` already
  exists for the check.
- Tests:
  - Golden updates in `backend/riscv/test/codegen_tests.cpp`.
  - A new golden for the sort inner loop: `lw a3,0(a4); addi a4,a4,4; lw a2,0(a4); …
    sw a2,-4(a4)`.
  - A peephole unit test in `peephole_tests.cpp` on hand-built IR, including the
    negative case where `t` is live after the access.
- Run on RV64 and RV32 (`riscv-tests`, `riscv32-tests`).

### A3. Check the other backends

ARM32 and x86-64 already fold the offset (`[r6,#-4]`, `-4(%r8)`). Confirm that their
constant `ADD_PTR` is an immediate too: grep the sort output for a register-loaded
constant step. Only fix it if it isn't.

---

## B. AVR: the negative offset (item 2, AVR)

AVR's `ldd`/`std` take a displacement of 0…63 only, so `q - 2` cannot fold into an
access. The swap path now costs `movw; subi; sbci; movw` where the old two-pointer
form cost a `movw`. On the other hand, the non-swap path saved a copy. Which form wins
has to be measured, not guessed.

### B1. Measure first

- qemu has no cycle count, and neither simavr nor simulavr is installed. Two options:
  - Install simavr (Homebrew `simavr`), which reports cycles, and add a
    `scripts/bench_avr.sh` modelled on `scripts/bench_msp430.sh`, reusing
    `bench/msp430/*.c`, which are target-neutral C.
  - Without a simulator, count cycles by hand from the AVR datasheet timings over the
    inner loop's two paths (swap / no swap), weighted by the sort's swap ratio. That
    ratio is about 1/2 on random data and can be counted with a host build of `sort.c`.
- Compare three forms, all built from the same TAC with the step-in-place rewrite
  forced on or off:
  - the two-pointer form (in-place step off);
  - the current form;
  - the `st -Z` form of B2.

### B2. Store through a pre-decrement (if B1 shows it pays)

A store to `q - 2` followed by a store to `q`, with `q` in `Z`, is `st -Z, hi;
st -Z, lo; std Z+2, …; std Z+3, …`. Equivalently, with `Z` left at `q - 2`, the later
accesses take displacement +2. Do this in the AVR peephole, or in selection when an
`ADD_PTR(q, -c)` feeds only a store and `q` is in `Z`. It is fiddly because `Z` must
end up equal to `q` again, or the register allocator's view of `q` breaks. So prefer
selection, with `q` kept out of `Z` as today.

### B3. Otherwise: a target switch

If B1 shows the in-place step costs AVR cycles, add `Target.neg_disp` (bool: a load or
store reaches `p - c` for a small `c` at no cost) to `semantic/target.h`/`target.c`:
true for MSP430, x86-64, ARM32, AArch64 and RISC-V; false for AVR (and BESM-6, where it
does not matter). `step_pointer` in `optimize/ivsr.c` then refuses when a pending read
of `q` would need the `- c` and the target says no. Add a test in
`optimize/test/loop_tests.cpp` under `TargetGuard t("avr")` that the bubble sort keeps
two pointers.

Not part of this item, but visible in the same loop: AVR reloads `Z` from `r16` before
every access (`movw r30, r16`), because the pointer is allocated outside X/Y/Z. Note it
in `docs/Avr_Backend.md` as the next AVR opportunity; don't fix it here.

---

## C. Redundant branch elimination (item 1, all loop-optimizing targets)

### What has to be proved

After IVSR, the bubble sort's outer loop is

```
pre:   %4 = n - 1;  if !(%4 > 0) goto exit
       e = ADD_PTR(v, %4, s)              ; end pointer
head:  q = v
       if !(e >u v) goto latch            ; the inner guard
       … inner loop …
latch: e = ADD_PTR(e, -1, s)
       if (e >u v) goto head
exit:
```

The inner guard `e >u v` holds on both edges into `head`:
- **Back edge:** it is the latch's own test, taken true, and neither `e` nor `v`
  changes between that test and the guard.
- **Entry edge:** `%4 > 0` holds there, and `e = v + %4*s` with `s > 0`. A valid
  pointer does not wrap, so `e >u v`.

GCC removes the guard this way, and the same pattern comes from every rotated loop
nested in another whose bound it shares.

### C1. A conditions pass: `optimize/conditions.c`

A forward dataflow over `OptCfg`, shaped like CSE's available expressions, with an
optimistic meet (an unvisited predecessor is left out) and intersection at merges.

- **Facts:** comparisons `a op b` known true, with `a`/`b` private names or constants
  and `op` one of the six signed or four unsigned relations, plus `==`/`!=`. Keep them
  as a small list per block; the sets are tiny.
- **Edge facts:** a block ending in `jump_if_zero c` / `jump_if_not_zero c`, where
  `c = a op b` is computed in the same block with neither operand redefined after it,
  gives `a op b` on the true edge and its negation on the false edge. The meet is per
  edge, not per block, so `OptPreds` needs the edge's kind (taken / fall-through); the
  CFG already records it (see the `[cfg] … -[cond-taken]->` trace).
- **Kill:** a definition of `x` kills every fact that mentions `x`. Private names
  only: a global or address-taken operand is never a fact, which avoids any alias
  question. Reuse `collect_alias_sets`.
- **Derivation through `ADD_PTR`:** at `p = ADD_PTR(b, x, s)` with `s > 0`:
  - a fact `x > 0`, or a constant `x > 0`, adds `p >u b`;
  - a fact `x < 0`, or a constant `x < 0`, adds `p <u b`.

  This is valid because pointer arithmetic out of the object is undefined. Only do it
  where `p` and `b` are pointers. The same rule turns `e = ADD_PTR(e, -1, s)` into
  nothing, since `e` is redefined; that is right, as the latch's own test supplies the
  fact.
- **Implication when testing a fact:** identical; mirrored (`a > b` ⇔ `b < a`);
  weakened (`>` ⇒ `>=`, `!=`); `a > b` ⇒ `a != b`. Nothing transitive; it isn't needed
  here and is costly.
- **Use:** at a conditional jump whose condition `c = a op b` (same block) is implied
  true, or implied false, by the block's in-facts plus the facts generated before it:
  - replace the condition by the constant `1` or `0`;
  - constant folding then turns the jump into an unconditional one or deletes it;
  - unreachable-code elimination removes the dead arm;
  - dead-store elimination removes `c`.

### C2. Wiring

- `OptFlags.conditions` (default on), with a `--no-conditions` CLI flag in
  `translator/main.c` next to `--no-ivsr`. Off where `target_config->no_loop_opt`
  (BESM-6), following the earlier decision that every new pass leaves BESM-6 output
  unchanged.
- **Placement:** run it next to `reduce_induction_variables` in
  `optimize/optimize.c`, at the scalar fixed point, so it sees IVSR's end pointer.
  Report a change only when it rewrote a condition, so the loop converges.

### C3. Tests: `optimize/test/conditions_tests.cpp`, added to `optimizer-tests`

- The bubble sort: no conditional jump on the inner guard is left (count
  `jump_if_zero`).
- An `if (x > 0) { if (x > 0) … }` duplicate disappears; `if (x > 0) { x = g(); if
  (x > 0) … }` keeps both (a kill).
- A global operand keeps the test (`if (g > 0) { h(); if (g > 0) … }`).
- The `ADD_PTR` derivation, for a positive and for an unknown index.
- Off on BESM-6 (`TargetGuard t("besm6")`).

Then every backend's run tests and the book suite, which compare with clang/GCC. The
goldens of every non-BESM-6 backend may move wherever a nested rotated loop appears;
review the diffs by hand.

### C4. Measure

Run `scripts/bench_msp430.sh`; expect `sort` at about 32.5k cycles. Check that no other
bench gets slower.

---

## Order and checks

1. **A1, A2, A3.** Small and contained. Afterwards, compare the AArch64 and RISC-V sort
   inner loops with clang's.
2. **C.** Afterwards, run the MSP430 bench and the full ctest.
3. **B.** Measure first; then B2 or B3, or neither if the current form wins.

After each step:
- Save the BESM-6 libc assembly (`build/libc/besm6/*.madlen`, `*.bemsh`) and compare
  it after the build; it must be byte-identical.
- Update `docs/TAC_Optimization.md` (a section for the conditions pass),
  `docs/Aarch64_Backend.md`, `docs/Riscv_Backend.md`, `docs/Avr_Backend.md`, and the
  MSP430 cost table in `docs/Msp430_Backend.md`.
- Add the new pass to CLAUDE.md's optimizer pass list.
