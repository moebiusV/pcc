# Reading the size report: what the differences mean

[SIZE-REPORT.md](SIZE-REPORT.md) puts numbers side by side.  A number is only worth comparing if both compilers
produced **correct code for the same, valid program**.  Where one of them did not, a smaller size can mean a
missing feature, not a better compiler.  This note says where that happens, shows it, and says where the
differences are real.  Every figure here can be regenerated with `tools/corpus.py` and `tools/size-gap/` in the
toolchain repository (named under each section).

The short version:

| comparison | what the size difference is |
|---|---|
| fork against stock pcc, **amd64 and i386** | real: the fork's optimizations.  Without `-xdmr` the fork is stock's size. |
| fork against stock pcc, **PDP-11**, files with no 64-bit integers | real: the fork is 5% smaller |
| fork against stock pcc, **PDP-11**, files with `long long` / `uint64_t` | **not comparable**: stock does not implement 64-bit integers there and silently computes with 32 bits |
| fork against gcc/clang, **amd64, i386, AArch64** | real: they are mature optimizers |
| fork against gcc, **PDP-11 and VAX** | real: 16.6% larger on the PDP-11 (median function 3%), 24% on the VAX; the PDP-11 corpus now uses 16-bit-`int` headers so that gcc is not compared on undefined behaviour |

## 1. Stock pcc on the PDP-11 does not implement 64-bit integers, so its code is smaller and wrong

On the PDP-11 an `int` is 16 bits and a `long` 32.  Stock pcc has no `long long` there: it compiles it as a 32-bit
`long`.  There is no diagnostic except an incidental "shift larger than type".  For

```c
typedef unsigned long long u64;
u64 add(u64 a, u64 b) { return a + b; }
unsigned hi(u64 a) { return (unsigned)(a >> 32); }
```

stock pcc produces

```
_add:   mov 4(r5),r0 ; mov 6(r5),r1 ; mov 10(r5),r2 ; mov 12(r5),r3
        add r2,r0 ; add r3,r1 ; adc r0                    <- a 32-bit add; the other 32 bits are never touched
_hi:    mov $-40,r2 ; ashc r2,r0 ; mov r1,r0              <- "shift larger than type": a shift of 32 bits by 32
```

(`sizeof(long long)` is 4.)  The fork has a real 64-bit `long long`: it is an eight-byte object in memory and every
operation is a call to a helper (`arch/pdp11/lowerll.c`, `ll.c`).  That is bigger, and it is correct.

A second case: `ashc` shifts arithmetically, so stock pcc's `unsigned long x; x >> 3` copies the sign bit into the
top three bits.  It compiles to `ashc $-3,r0` and is wrong for any `x` with the top bit set.  The fork emits the
same `ashc` followed by a mask (`bic $-20000,r0`), which is correct; for a variable count it calls a helper.

How much of the corpus this touches (PDP-11, optimized, `tools/size-gap/llcount.py` and `llsplit.py`, the files stock,
the fork and gcc all compile, with the PDP-11's own headers -- section 3):

| files | count | stock | fork | fork / stock |
|---|---|---|---|---|
| whose output calls a real 64-bit helper | 72 | 297,890 | 322,482 | 1.08 |
| that do not | 607 | 506,140 | 465,526 | **0.92** |
| all | 679 | 804,030 | 788,008 | 0.98 |

So what is left of "the fork is larger than stock on the PDP-11" comes from the 11% of files that use
64-bit integers (mostly the crypto code in bearssl and mbedtls, and Lua's `lua_Integer`), where stock is not
computing what the program says.  On the other 607 files the fork's optimizations win, as they do on the V7 sources
(where the fork is 3% to 4% smaller than the original V7 compiler).

An earlier version of the report showed the fork 28% larger than stock.  That was a different problem, now fixed:
each file that used an unsigned 32-bit shift or a 64-bit operation carried its own copy of the helper
(about 690 bytes), even a file whose only user was an unused `static inline` function.  Constant shifts are now
inline and a helper is written only if a function that is written out calls it.

## 2. The same check on amd64 and i386: the difference is the optimizations

Here there is no missing feature to blame.  The headers match the target, stock and fork compile the same files
(846 and 847 of 900), and the fork's own bug fixes touch rare constructs.  To see how much of the difference is the
optimizations, `tools/size-gap/x86exp.py` compiles a sample three ways -- stock, the fork without `-xdmr`, and the
fork with it (72 files each, optimized):

| | stock | fork without `-xdmr` | fork with `-xdmr` |
|---|---|---|---|
| amd64 | 599,577 | 596,845 (-0.5%) | 553,466 (**-7.7%**) |
| i386 | 482,173 | 475,613 (-1.4%) | 463,799 (**-3.8%**) |

Without `-xdmr` the fork is the stock compiler (the half percent is the shared flags and a few fixes); the rest is
the optimizations.  Over the whole corpus the fork is smaller than stock on 612 of 831 amd64 files and larger on 14.
Stock does have bugs on these targets as well (the list is in
`UPSTREAM-PLAN.md` in the toolchain repository, Wave 1: `soft_cmp`, shifts of
zero, an unsigned conversion, large `case` ranges, `(a != b) == 0`), but they involve rare constructs and do not
move the totals.

The gap to gcc and clang (amd64: +49% and +60%, i386: +33% and +41%) is real.  They are mature optimizers with a
global middle end; pcc has none, and nothing about the input makes the comparison unfair.

## 3. The PDP-11 against gcc

gcc 14.2 is built as a cross compiler for the PDP-11 (`tools/corpus-gcc-cross.sh`).

**The corpus used to be unfair to this comparison.**  It was preprocessed against the i386 C library headers, in
which `uint32_t` is `unsigned int` -- 16 bits on the PDP-11.  The crypto sources then did things like `x >> 24` on a
16-bit value, which C leaves undefined, and gcc, which may, deleted the code that depended on it (a function of 800
instructions came out as 7).  The fork emitted a shift that means nothing.  Neither output meant what the author
wrote.  The PDP-11 half of the corpus now has its own headers (`tools/corpus-pdp11-sysroot.sh`: `int` 16 bits,
`uint32_t` is `unsigned long`, `INT_MAX` is 32767, pointers 16 bits), and **every row of the report is summed over
the files all of its compilers compiled** (stock, the fork, and gcc or clang where the target has them), so no
compiler is credited for a file another could not compile.

Measured that way (PDP-11, optimized): the **fair set** is the 618 files that stock, the fork and gcc all compile and
that stock gets right (the fork's output calls no 64-bit helper; section 1):

| | stock | fork | gcc | fork / gcc |
|---|---|---|---|---|
| fair set (618 files) | 506,162 | 466,252 (-7.9%) | 399,986 | **1.166** |

(`tools/corpus.py` marks the files that need a 64-bit helper; `tools/size-gap/llsplit.py` and `llcount.py` give the
split.)  So the fork's `-O -xdmr` is **not** within 5% of gcc on the PDP-11: it is 16.6% larger, and 7.9% smaller than
stock.  Typical code is much closer: per function (`tools/size-gap/fnscan.py`, 597 functions in the 91 files where the fork
is larger) the median function with at least eight instructions is 3% larger, but the excess is concentrated -- 30
functions carry 62% of it.  Over the other targets, on their fair sets (every compiler in the row compiled the
file): VAX +23.6% (fork 8.6% smaller than stock), i386 +36.0% (6.7% smaller), amd64 +54.5% (12.3% smaller).

What explained part of the gap, found and fixed:

* **Local arrays zeroed one byte at a time.**  `unsigned char x[64] = { 0 };` was 64 `clrb` instructions in the fork
  and a loop or a `memset` in gcc: 4,346 `clr`/`clrb` in runs of eight or more, in 25 files of the corpus, about
  17 KB, 13% of the gap at that time.  It is now a loop (`-xdce`), and the PDP-11 gap on the same files went from
  20% to 16.6%.
* `memcpy` of a constant few bytes and reads of `static const` objects (section 4).

What is left is spread over many functions that have not been examined one by one; the largest are
`mbedtls_internal_sha256_process` (4,669 instructions against gcc's 3,860), `br_aes_ct_bitslice_encrypt` (857
against 333) and `crc32_z` (892 against 473).

Nothing in these numbers is a defect of the measurement any longer: both compilers see the same valid program with
the same types.

## 4. The memcpy and const-folding optimizations, on every target

Both optimizations (`-xdce`, in the shared front end: a `memcpy` of a constant number of bytes is copied in
place, and reads of `static const` integer objects are folded, including the first byte through a char pointer)
are not specific to the PDP-11.  Measured on the whole C99 corpus, optimized, against stock, before and after
(`doc/SIZE-REPORT.md`; the rest of the compiler did not change in between):

| target | before both | after memcpy and const folding | after array zeroing too |
|---|---|---|---|
| amd64 | -9.4% | -10.3% | **-12.3%** |
| i386 | -3.5% | -4.6% | **-6.7%** |
| VAX | -4.7% | -6.8% | **-8.6%** |
| PDP-11 (fair set) | +4.4% | +1.8% | **-7.9%** (the set changed: 64-bit files left out) |
| AArch64 | -17.3% | -17.3% | -17.3% (unchanged: the idioms do not occur in this corpus there) |

So on every measured target where the idiom occurs the optimization helped, by one to two points on x86 and the
VAX and more on the PDP-11, where a call costs the most.  Targets that cannot load an unaligned word copy bytes
(up to 4); targets that can (amd64, i386, VAX, AArch64) copy words (up to 16 bytes).

## 5. What to take from the report

* Compare the fork with stock on amd64, i386 and AArch64, and on the PDP-11 and VAX for files without 64-bit
  types; those are optimizer differences.
* Do not read the PDP-11 total for stock as a baseline on files with 64-bit integers.
* Compare with gcc and clang for what they are -- mature optimizers -- and, on the PDP-11 and VAX, discount the
  functions that depend on a 32-bit `int`.
* The V7 and 32V tables, where the original compilers are the baseline, compare programs written for those
  machines and are not affected by any of this.
