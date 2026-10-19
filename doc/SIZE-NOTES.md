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
| fork against gcc, **PDP-11 and VAX**, ordinary functions | real, small (median function 4% larger) |
| fork against gcc, **PDP-11 and VAX**, a few dozen crypto functions | **not comparable**: the sources assume a 32-bit `int`, which is undefined behaviour on a 16-bit one, and gcc deletes the code |

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

How much of the corpus this touches (PDP-11, optimized, `tools/size-gap/llcount.py` and `llsplit.py`, files both
compilers compile):

| files | count | stock | fork | fork / stock |
|---|---|---|---|---|
| whose output calls a real 64-bit helper | 89 | 350,924 | 401,678 | 1.14 |
| that do not | 676 | 517,798 | 483,074 | **0.93** |
| all | 765 | 868,722 | 884,752 | 1.02 |

So the "fork is 2% larger than stock on the PDP-11" in the report comes entirely from the 12% of files that use
64-bit integers (mostly the crypto code in bearssl and mbedtls, and Lua's `lua_Integer`), where stock is not
computing what the program says.  On the other 676 files the fork's optimizations win, as they do on the V7 sources
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

## 3. The PDP-11 against gcc: ordinary code is close, a few functions are not comparable

gcc 14.2 is built as a cross compiler for the PDP-11 (`tools/corpus-gcc-cross.sh`).  Per function
(`tools/size-gap/fnscan.py`, the 120 PDP-11 files where the fork is larger than gcc, 755 functions): the median
function with at least eight instructions is **4% larger** in the fork than in gcc.  But 30 functions carry 70% of
the excess, almost all in mbedtls:

| function | gcc instructions | fork instructions |
|---|---|---|
| `mbedtls_internal_ripemd160_process` | 236 | 4,621 |
| `mbedtls_internal_sha1_process` | 2,515 | 6,596 |
| `mbedtls_internal_sha256_process` | 1,553 | 4,601 |
| `mbedtls_des3_crypt_ecb` | 7 | 773 |

These are not an optimizer difference.  The corpus is preprocessed against the i386 C library headers, in which
`uint32_t` is `unsigned int`.  On the PDP-11 that is 16 bits.  The code then does things like `x >> 24` on a 16-bit
value, which C leaves undefined, and gcc, which is allowed to, deletes the computation that depends on it.  The
fork emits a shift by 24 on a 16-bit register, which the hardware defines but the programmer did not mean.  Neither
output means what the author wrote, so their sizes cannot be compared.  (The corpus should use `unsigned long` for
`uint32_t` on the PDP-11; that is on the list of things to fix in the harness.)

What is a real difference in those functions, and is not an artifact:

* **Small `memcpy` calls.**  mbedtls reads unaligned words with `memcpy(&v, p, 4)`.  gcc turns that into two loads;
  the fork makes a call (pushes, `jsr`, pops).
* **Reads of `static const` objects through a byte pointer.**  mbedtls decides byte order at run time from
  `static const uint16_t detector = 0x100`, read as `*(unsigned char *)&detector`.  gcc folds the value and
  removes the other branch; the fork folds a plain read of such an object but not this one.

Both are now done in the fork (`-xdce`): `memcpy` of 1 to 4 bytes (16 where unaligned loads work) is copied in place, and a
braced `static const` scalar and its first byte are folded.  They took the fork from 4.4% to 1.8% larger than stock on
the PDP-11 C99 corpora, and from 37.6% to 34.2% larger than gcc.

## 4. The memcpy and const-folding optimizations, on every target

Both optimizations (`-xdce`, in the shared front end: a `memcpy` of a constant number of bytes is copied in
place, and reads of `static const` integer objects are folded, including the first byte through a char pointer)
are not specific to the PDP-11.  Measured on the whole C99 corpus, optimized, against stock, before and after
(`doc/SIZE-REPORT.md`; the rest of the compiler did not change in between):

| target | fork vs stock before | after |
|---|---|---|
| amd64 | -9.4% | **-10.3%** |
| i386 | -3.5% | **-4.6%** |
| VAX | -4.7% | **-6.8%** |
| PDP-11 | +4.4% | **+1.8%** |
| AArch64 | -17.3% | -17.3% (unchanged: the compiler inlines the call, but this corpus does not reach the idiom there) |

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
