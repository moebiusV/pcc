# Code size: stock PortableCC against this fork, clang, gcc, dmr's cc and 32V's cc

This report compares the code size of this fork of pcc with **stock PortableCC** (current
`PortableCC/pcc` master, 9a993a5f), with **clang** and **gcc** on the targets they share with pcc, and,
for the two old targets, with the **original compilers**: dmr's `cc` (V7, PDP-11) and 32V's `cc` (VAX,
itself Johnson's pcc).  It covers the C99 program **Lua 5.4.7** on every back end both pcc builds have in
common with clang, plus the V7 and 32V source trees the older size work used.

Base: this fork is built on PortableCC commit `c4893a9d`.  Upstream has moved by five commits since
(`9a993a5f`, the "stock" measured here); they touch `mip/reader.c` and `mip/match.c`, where this fork's
relational-canonicalization work also lives, so merging them is conflict resolution that has not been done.
The comparison therefore gives stock its five newest commits that the fork does not yet have.

Status: this first edition has **Lua** as its C99 corpus.  SQLite, zlib, lz4, cJSON, BearSSL, mbedTLS,
chibicc, c-testsuite and the GCC torture tests are fetched and wired into the same harness; their numbers
will be added when that run has finished.  Nothing below depends on them.

## Summary

* **PDP-11, V7 libc and commands, against dmr's `cc`** (files both pcc builds compile): stock `-O` is
  **+16.5% / +8.8%** larger than dmr; this fork's `-O` is **3.2% / 4.2% smaller** than dmr, and 16.9% /
  11.9% smaller than stock `-O`.  Stock rejects 3 libc files and 9 command files that this fork compiles.
* **VAX, 32V libc and commands, against 32V's `cc -O`**: stock `-O` is **+9.0% / +3.4%** larger; this
  fork's `-O` is **5.0% / 8.4% smaller** (81 libc and 28 command files that the 32V guest compiled and both
  pcc builds assembled).
* **Lua, amd64 and i386**: this fork's `-O` is 14% and 5% smaller than stock's `-O`; against clang `-Os`
  it is +25% (amd64) and +11% (i386), against gcc `-Os` +30% and +18%.
* **Lua, AArch64**: stock's AArch64 back end compiles 3 of Lua's 34 files; the fork compiles all 34, at
  +13% over clang `-Os` (+8% unoptimized).
* **The other back ends** (arm, m68k, mips, mips64, sparc64, riscv64, powerpc) compile few or none of
  Lua's files, in stock and in the fork alike.  They are ports that were never finished; the fork did not
  work on them, and their sizes are not meaningful.  powerpc and riscv64 produce nothing.

## What was measured

* **Size** is text plus data bytes of the assembled object (`size` on the host for ELF targets; the a.out
  header of V7 `as` and of `as32v` for the PDP-11 and VAX).  Clang and gcc are built **without unwind
  tables** (`-fno-asynchronous-unwind-tables -fno-unwind-tables`) and **without PIE** (`-fno-pic`; on x86
  also `-fcf-protection=none`), so they are compared with what pcc generates; pcc writes neither.  Without
  that, clang's `.eh_frame` alone adds 25 to 40% to a small file.
* **Compilers.**  *stock*: current PortableCC master, built for each target.  *ours*: this branch.
  Unoptimized means no flags.  Optimized means what `pcc -O` passes to `ccom` in each tree
  (`-xtemps -xdeljumps -xinline -xdce`, plus `-xssa` on Linux targets); for ours that is `-O -xdmr`, so
  also `-xdmr`.  Clang and gcc use `-O0` and `-Os`.
* **Only files every compiler in a row compiled are summed**, so each row compares the same code.  A file
  that fails to compile is a result of its own and is counted in the "compiled" columns; it is never
  silently dropped or averaged in.
* **Input.**  Each file is preprocessed once, with the host gcc against the musl 1.2.5 headers of that
  architecture, and the identical text goes to every compiler.  For the PDP-11 and VAX there are no musl
  headers; the i386 ones supply the typedefs.
* **PDP-11 and VAX** have no clang or gcc.  They use V7's `as` and `c2` (ported to the host) and `as32v`,
  a host port of the 32V assembler (`modern/usr/src/cmd/as32v` in the toolchain repository), which gives
  the same size as the real 32V assembler running in an emulated VAX on all 112 sources with a unique
  name that were compared (three more share a name between libc and cmd, which the comparison could not
  tell apart, and are left out).  Optimized output goes through the target's peephole `c2`.  For the VAX the symbols
  are renamed to at most 8 characters first, because 32V's `as` keeps only 8, and stock's GNU-syntax output
  is rewritten to the old spelling (`%r0` to `r0`, `.L12` to `L12`, a leading `_`); neither changes an
  instruction.

## Lua on the 64- and 32-bit targets

34 source files, same input to every compiler.

### Unoptimized

| target | files | compiled: stock / ours | common | stock | ours | clang -O0 | gcc -O0 | ours vs stock | ours vs clang |
|---|---|---|---|---|---|---|---|---|---|
| amd64 | 34 | 34 / 34 | 34 | 350,841 | 350,352 | 275,985 | 281,172 | -0.1% | +27% |
| i386 | 34 | 34 / 34 | 34 | 221,167 | 220,420 | 277,310 | 247,690 | -0.3% | -21% |
| aarch64 | 34 | 2 / 34 | 2 | 340 | 340 | 340 | | | |
| aarch64, all 34 files ours compiled | 34 | n/a / 34 | | | 316,884 | 292,510 | | | +8% |

### Optimized

| target | files | compiled: stock / ours | common | stock `-O` | ours `-O -xdmr` | clang -Os | gcc -Os | ours vs stock | ours vs clang | ours vs gcc |
|---|---|---|---|---|---|---|---|---|---|---|
| amd64 | 34 | 34 / 34 | 34 | 237,051 | 204,858 | 164,118 | 157,421 | **-13.6%** | +25% | +30% |
| i386 | 34 | 34 / 34 | 34 | 189,844 | 180,896 | 162,803 | 153,121 | **-4.7%** | +11% | +18% |
| aarch64 | 34 | 3 / 34 | 3 | 588 | 656 | 656 | | | | |
| aarch64, all 34 files ours compiled | 34 | n/a / 34 | | | 200,312 | 176,603 | | | +13% | |

Reading this:

* Unoptimized, stock and the fork are within 0.4% of each other on x86: the fork's gains are in the
  optimizer, as intended.  Unoptimized pcc on i386 is smaller than clang `-O0` and gcc `-O0`, because those
  keep every variable in memory with verbose prologues.
* Optimized, the fork removes 14% (amd64) and 5% (i386) from stock.  It does not reach clang or gcc:
  25 to 30% larger on amd64, 11 to 18% on i386.  An earlier study of AArch64 libc found where that goes: no
  tail calls (a `bl f; ret` pair keeps a frame), a global's `adrp` address recomputed for each access
  instead of held in a register, and no conditional-select or `subs`-fused compares.  I have not repeated
  that study for x86.
* AArch64: stock's back end (ARM32-derived) compiles 3 of 34 files and stops with internal errors
  ("op 76 slipped through", "Coalesce: src class 3, dst class 1").  The AArch64 back end in this fork is a
  new AAPCS64 LP64 implementation; it compiles all of Lua and its output is 13% over clang `-Os`.
  The 2 and 3 files the common column has for aarch64 are too small to say anything about size.

### Which back ends compile Lua at all

Number of Lua files (of 34) compiled and assembled, optimized.

| target | stock | ours | what stops the rest |
|---|---|---|---|
| amd64, i386 | 34 | 34 | |
| aarch64 | 3 | 34 | stock: the old back end |
| mips | 8 | 9 | "Cannot generate code, node FUNARG" (unfinished argument passing) |
| mips64 | 4 | 6 | same |
| arm | 5 | 6 | "op 75 slipped through" |
| m68k | 2 | 2 | assembler syntax pcc prints for it is rejected |
| sparc64 | 2 | 2 | "Cannot generate code, node TEMP"; long double |
| riscv64 | 0 | 0 | every file: "invalid function definition" |
| powerpc | 0 | 0 | the back end does not build: `sz =  = p2autooff` in `arch/powerpc/local2.c` (ELF ABI path), in stock too |

The typo arrived in August 2023 (commit b4f178cb); the ELF path has not compiled since.  None of these ports is part of what this fork worked on; they are
listed so nobody reads their sizes as a result.

## PDP-11 against dmr's `cc`

Corpus: V7's `usr/src/libc` and `usr/src/cmd` (K&R C).  Measured with `tools/pccdiff.sh`, which compiles
every file with dmr's `cc` (the V7 compiler, ported to the host) and with pcc, assembles with V7 `as` and
compares text plus data.  "Unoptimized" is `pcc -S` followed by V7's peephole `c2`; "optimized" is `pcc -O`
(ccom's passes, then `c2`).  Files both pcc builds compiled:

| corpus | files | dmr `cc` | stock, no `-O` | stock `-O` | ours, no `-O` | ours `-O` |
|---|---|---|---|---|---|---|
| libc | 86 | 15,062 | 23,560 (+56.4%) | 17,542 (+16.5%) | 21,364 (+41.8%) | **14,574 (-3.2%)** |
| commands | 43 | 84,346 | 112,044 (+32.8%) | 91,746 (+8.8%) | 102,984 (+22.1%) | **80,812 (-4.2%)** |

Over all files each compiles: libc 89 files (stock 86), dmr 17,356, ours `-O` 16,838 (-3.0%); commands 52
files (stock 43), dmr 98,102, ours `-O` 94,174 (-4.0%).  Ours versus stock `-O` on the common files:
-16.9% (libc) and -11.9% (commands).

One file of 89 is larger than dmr's (`qsort`, +12 bytes), and that one is more correct: dmr's `clr r0; div`
for an unsigned divide by a variable is wrong for divisors of 32768 and up (shown by running dmr's own
output); pcc calls a 34-byte helper that is right.  `BIGGER-THAN-ORIGINAL.md` in the toolchain repository
lists every file that is larger and why.

## VAX against 32V's `cc` and `cc -O`

Corpus: 32V's `libc/gen`, `libc/stdio` and `cmd` sources.  The reference sizes come from the real 32V
operating system running under SIMH (`tools/vaxdiff.py`): its `cc` and `cc -O`.  That compiler is
Johnson's pcc, so this is also the comparison with the original pcc.  The guest produced a full set of sizes for
115 of the 229 sources (for the other 105 it returned an incomplete set, for example no `cc -O` size; I did
not investigate why); stock and ours were compiled and assembled on the host.  Files with a unique name that all of them handled:

| corpus | files | 32V `cc` | 32V `cc -O` | stock `-O` | ours `-O -xdmr` |
|---|---|---|---|---|---|
| libc (gen, stdio) | 81 | 13,784 | 12,536 | 13,660 (+9.0%) | **11,908 (-5.0%)** |
| commands | 28 | 12,076 | 11,348 | 11,732 (+3.4%) | **10,396 (-8.4%)** |

Percentages are against 32V's `cc -O`.  Unoptimized (raw `ccom` output, no `c2`), stock and ours are 36,364
and 36,832 bytes on the 109 files, +40.6% and +42.4% over 32V's unoptimized `cc` (25,860); `-O` is where the
fork's work shows.

## Lua on the PDP-11 and the VAX

No clang or gcc here, so the comparison is stock against ours.  Lua is a C99 program on 32-bit machines;
on the 16-bit PDP-11 it is a stress test, not a use case.

| target | config | compiled: stock / ours | common | stock | ours | ours vs stock |
|---|---|---|---|---|---|---|
| VAX | unoptimized | 27 / 34 | 27 | 160,532 | 162,548 | +1.3% |
| VAX | optimized | 22 / 32 | 21 | 79,204 | 72,784 | **-8.1%** |
| PDP-11 | unoptimized | 25 / 27 | 23 | 126,704 | 138,510 | +9.3% |
| PDP-11 | optimized | 18 / 26 | 17 | 60,730 | 62,518 | +2.9% |

The PDP-11 sizes are higher for the fork because it is correct where stock is not: stock's PDP-11
`long long` is not 64 bits (`sizeof(long long)` is not 8 there), while the fork's is, implemented as an
eight-byte object operated on by helper calls.  Lua's integers are 64-bit, so the fork emits the calls and
stock silently computes with a narrower type.  The fork rejects, with a diagnostic, the three files that
convert `long long` to floating point, which are not implemented.  On the same input the fork compiles
more files than stock in both optimized configurations (VAX 32 against 22, PDP-11 26 against 18).

## What this exercise found

Running a new corpus on every back end turned up things the V7 and 32V sources never did:

* **ccom hung on the GNU `case LOW ... HIGH:` range with a huge range.**  A 16-line program
  (`case 1000000000000000000ULL ... 9999999999999999999ULL:`) never finished, in stock and in the fork
  (tested on amd64, i386, VAX and PDP-11, all four hung): `gcccase()` added one switch entry per value
  (and compared signed for an unsigned switch).  It is in the GCC torture suite.  The fork now keeps a
  long range as one range test (commit "ccom: GNU case ranges ..."); stock still hangs.  The measurements
  above were taken with the build before that fix, and the torture-suite files with case ranges are
  re-measured separately (below, once finished).
* musl's `struct timespec` padding (`int :8*(sizeof(time_t)-sizeof(long))`) is an error on a 16-bit int
  once `long long` is 64 bits; removed from the PDP-11 input for both compilers.
* The AArch64 varargs the first measurement needed (`va_list`, `va_arg`, `va_copy`) did not exist; they do
  now (in the AArch64 back end commit; tests `a64va1`, `a64va2`).
* A build-system trap, for anyone doing this: the fork's tree tracks a prebuilt `pdp11-bsd-ccom`, which a
  naive "pick the `*-ccom` in the directory" build mistakes for the compiler of any target that sorts after
  `p`.  The first set of riscv64, sparc64 and VAX numbers came from that mistake and were thrown away.

## Reproducing

All of the harness is in the toolchain repository (`facetlang/pdp11/v7unix-toolchain/tools`):

```
corpus-build.sh                  # stock (git archive of origin/master) and ours, every target, plus c2 for PDP-11 and VAX
corpus-gcc-cross.sh              # optional: binutils and a C-only GCC for vax-netbsdelf and pdp11-aout
corpus.py --only lua -j 3        # incremental, resumable; --arch to restrict; --full for every torture test
pccdiff.sh -v orig/usr/src/libc  # PDP-11 against dmr's cc (PCC=... for stock)
vaxdiff.py --ccom CCOM ...       # VAX against the 32V guest
```

Corpora are fetched into `~/claude/corpora`: Lua 5.4.7, SQLite 3.46.1 (amalgamation), zlib 1.3.1, lz4 1.9.4,
cJSON 1.7.18, BearSSL, mbedTLS 3.6.2, chibicc, c-testsuite, GCC's `gcc.c-torture/execute`, and the musl
1.2.5 headers for each architecture.

Known limits of this edition: one C99 corpus; the 32V reference covers 115 of 229 files; GCC's own VAX and
PDP-11 back ends are not in the tables yet (the cross compilers are still being built); the
PDP-11 "no `-O`" column of the V7 table is `pcc -S` followed by V7's `c2` (as `pccdiff.sh` defines it), while
the unoptimized columns elsewhere are raw `ccom` output.
