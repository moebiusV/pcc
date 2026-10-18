# Code size: stock PortableCC against this fork, clang, gcc, V7 dmr cc and 32V pcc

This report compares the code size of this fork of pcc with **stock PortableCC** (current `PortableCC/pcc`
master, commit 9a993a5f), with **clang** and **gcc** on the targets they share with pcc, and, for the two
old targets, with the **original compilers**: V7 dmr cc (the PDP-11 compiler of V7 Unix) and 32V pcc (the VAX compiler of 32V, itself
Johnson's pcc).  Ten C99 corpora (900 source files: lua 34, sqlite 1, zlib 15, lz4 5, cjson 2, bearssl 294, mbedtls 108, chibicc 8, c-testsuite 220, gcc-torture 213) are compiled for every back end that pcc and clang
have in common, with and without optimization, plus the V7 and 32V source trees that the earlier size
work used.  Everything is regenerated from the measurement files by `tools/corpus-report.py`.

Base: this fork is built on PortableCC commit `c4893a9d`.  Upstream has moved by five commits since (`9a993a5f`,
the "stock" measured here); they touch `mip/reader.c` and `mip/match.c`, where this fork's relational-
canonicalization work also lives, so merging them is conflict resolution that has not been done.  The
comparison therefore gives stock five newer commits that the fork does not yet have.

## Summary

* **PDP-11, V7 libc and commands, against V7 dmr cc** (86 + 43 files both pcc builds compile): stock `-O` is
  **+16.5% / +8.8%** against V7 dmr cc; this fork's `-O` is **-3.2% / -4.2%** (and -16.9% / -11.9% against stock `-O`).
* **VAX, 32V libc and commands, against 32V pcc -O**: see the VAX section; stock `-O` is +9.0% / +3.4% and this
  fork's `-O` is -5.0% / -8.4%.
* **All C99 corpora, optimized, files every compiler compiled**: on amd64 ours is 9.2% smaller than stock, 49.1% larger than clang `-Os` and 60.2% larger than gcc `-Os`;
  on i386 3.6% smaller than stock, 32.6% larger than clang and 41.4% larger than gcc.
* **AArch64**: stock's back end compiles 271 of the 900 files, the fork 829.  Over the 262 files all three compiled, the
  fork is 17.2% smaller than stock and 13.6% larger than clang `-Os`; over every file the fork and clang both compiled it is 27.3% larger than clang.
* **The other back ends** (arm, m68k, mips, mips64, sparc64) compile only part of the corpus, in stock and in the
  fork alike, riscv64 and powerpc nothing: they are unfinished ports the fork did not work on.
* **Where the fork compiles less than stock** (found by this run, not fixed): see "What this run found".

## What was measured

* **Size** is text plus data bytes of the assembled object (`size` on the host for ELF targets; the a.out header of
  V7 `as` and of `as32v` for the PDP-11 and VAX).  Clang and gcc are built **without unwind tables**
  (`-fno-asynchronous-unwind-tables -fno-unwind-tables`) and **without PIE** (`-fno-pic`; on x86 also
  `-fcf-protection=none`), so they are compared with what pcc generates; pcc writes neither.  Without that,
  clang's `.eh_frame` alone adds 25 to 40% to a small file.
* **Compilers.**  *stock*: current PortableCC master built for each target.  *ours*: this branch.  Unoptimized means
  no flags.  Optimized means what `pcc -O` passes to `ccom` in each tree (`-xtemps -xdeljumps -xinline -xdce`, plus
  `-xssa` on Linux targets); for ours that is `-O -xdmr`, so also `-xdmr`.  Clang and gcc use `-O0` and `-Os`.
* **Only files every compiler in a row compiled are summed**, so each row compares the same code.  A file that fails
  to compile is a result of its own and is counted in the "compiled" columns; it is never dropped silently or
  averaged in.  Compiles are cut off after 90 seconds (a compiler still running then counts as failed).
* **Input.**  Each file is preprocessed once with the host gcc against the musl 1.2.5 headers of that architecture, and
  the identical text goes to every compiler.  The PDP-11 and VAX have no musl headers; the i386 ones supply the
  typedefs (and musl's `struct timespec` padding field is removed for the 16-bit PDP-11, where it cannot fit).
* **PDP-11 and VAX** have no clang or gcc.  They use V7's `as` and `c2` (ported to the host) and `as32v`, a host
  port of the 32V assembler (`modern/usr/src/cmd/as32v` in the toolchain repository) that gives the same size as the
  real 32V assembler in an emulated VAX on all 112 sources compared with unique names.  Optimized output goes
  through the target's peephole `c2` (stock: V7's `c2` and the 32V-derived `c2vax`; ours: `cc2.pdp11.c` and
  `cc2.vax.c`, which are this fork's versions).  For the VAX the symbols are renamed to at most 8 characters first,
  because 32V's `as` keeps only 8, and stock's GNU-syntax output is rewritten to the old spelling (`%r0` to `r0`,
  `.L12` to `L12`, a leading `_`); neither changes an instruction.

## C99 corpora on the 32- and 64-bit targets

### All 900 C99 files together

Unoptimized:

| target | files | compiled: stock / ours | common | stock | ours | clang | gcc | ours vs stock | ours vs clang | ours vs gcc |
|---|---|---|---|---|---|---|---|---|---|---|
| aarch64 | 900 | 304 / 829 | 294 | 875,579 | 821,103 | 834,159 |  | -6.2% | -1.6% |  |
| amd64 | 900 | 846 / 829 | 814 | 2,908,977 | 2,905,970 | 2,294,355 | 2,296,414 | -0.1% | +26.7% | +26.5% |
| i386 | 900 | 847 / 830 | 814 | 1,926,901 | 1,923,329 | 2,228,247 | 2,117,141 | -0.2% | -13.7% | -9.2% |
| arm | 900 | 408 / 406 | 397 | 937,470 | 937,470 | 861,395 |  | +0.0% | +8.8% |  |
| powerpc | 900 | 0 / 0 | 0 | - | - |  |  | stock fails |  |  |
| m68k | 900 | 8 / 8 | 8 | 932 | 932 | 928 |  | +0.0% | +0.4% |  |
| mips | 900 | 571 / 574 | 558 | 2,144,759 | 2,141,563 | 1,708,698 |  | -0.1% | +25.3% |  |
| mips64 | 900 | 468 / 505 | 456 | 1,775,995 | 1,775,699 | 1,635,275 |  | -0.0% | +8.6% |  |
| riscv64 | 900 | 0 / 0 | 0 | - | - |  |  | stock fails |  |  |
| pdp11 | 900 | 800 / 800 | 782 | 1,082,902 | 1,413,812 |  |  | +30.6% |  |  |
| vax | 900 | 822 / 828 | 805 | 1,258,360 | 1,270,592 |  |  | +1.0% |  |  |
| sparc64 | 900 | 456 / 456 | 447 | 1,419,689 | 1,418,761 | 1,202,443 |  | -0.1% | +18.0% |  |

Optimized:

| target | files | compiled: stock / ours | common | stock | ours | clang | gcc | ours vs stock | ours vs clang | ours vs gcc |
|---|---|---|---|---|---|---|---|---|---|---|
| aarch64 | 900 | 271 / 829 | 262 | 754,994 | 624,881 | 550,292 |  | -17.2% | +13.6% |  |
| amd64 | 900 | 846 / 829 | 814 | 2,319,431 | 2,107,173 | 1,413,433 | 1,315,718 | -9.2% | +49.1% | +60.2% |
| i386 | 900 | 847 / 830 | 814 | 1,925,560 | 1,856,300 | 1,400,001 | 1,312,756 | -3.6% | +32.6% | +41.4% |
| arm | 900 | 489 / 487 | 476 | 917,400 | 883,592 | 614,621 |  | -3.7% | +43.8% |  |
| powerpc | 900 | 0 / 0 | 0 | - | - |  |  | stock fails |  |  |
| m68k | 900 | 121 / 121 | 121 | 4,687 | 4,687 | 2,347 |  | +0.0% | +99.7% |  |
| mips | 900 | 546 / 551 | 533 | 1,490,430 | 1,479,930 | 826,549 |  | -0.7% | +79.0% |  |
| mips64 | 900 | 442 / 449 | 433 | 1,292,406 | 1,291,713 | 943,191 |  | -0.1% | +37.0% |  |
| riscv64 | 900 | 0 / 0 | 0 | - | - |  |  | stock fails |  |  |
| pdp11 | 900 | 767 / 503 | 471 | 486,560 | 552,768 |  |  | +13.6% |  |  |
| vax | 900 | 808 / 807 | 776 | 832,180 | 791,216 |  |  | -4.9% |  |  |
| sparc64 | 900 | 276 / 275 | 268 | 32,071 | 31,947 | 14,884 |  | -0.4% | +114.6% |  |

"Common" is the number of files all compilers of that row compiled; the size columns sum those files only.  On the
targets without clang (PDP-11, VAX) the comparison is stock against ours.

### amd64, optimized, by corpus

| corpus | files | compiled: stock / ours | common | stock | ours | clang | gcc | ours vs stock | ours vs clang | ours vs gcc |
|---|---|---|---|---|---|---|---|---|---|---|
| lua | 34 | 34 / 34 | 34 | 237,051 | 204,858 | 164,118 | 157,421 | -13.6% | +24.8% | +30.1% |
| sqlite | 1 | 0 / 0 | 0 | - | - |  |  | stock fails |  |  |
| zlib | 15 | 15 / 15 | 12 | 98,076 | 90,543 | 56,515 | 51,187 | -7.7% | +60.2% | +76.9% |
| lz4 | 5 | 5 / 5 | 5 | 291,735 | 269,627 | 45,380 | 32,733 | -7.6% | +494.2% | +723.7% |
| cjson | 2 | 2 / 2 | 2 | 22,232 | 20,592 | 17,745 | 14,703 | -7.4% | +16.0% | +40.1% |
| bearssl | 294 | 294 / 294 | 294 | 299,463 | 281,240 | 218,622 | 202,004 | -6.1% | +28.6% | +39.2% |
| mbedtls | 108 | 101 / 84 | 84 | 574,372 | 532,752 | 347,300 | 342,454 | -7.2% | +53.4% | +55.6% |
| chibicc | 8 | 8 / 8 | 8 | 115,667 | 105,762 | 82,351 | 75,019 | -8.6% | +28.4% | +41.0% |
| c-testsuite | 220 | 215 / 215 | 215 | 43,250 | 39,726 | 30,680 | 27,492 | -8.1% | +29.5% | +44.5% |
| gcc-torture | 213 | 172 / 172 | 160 | 637,585 | 562,073 | 450,722 | 412,705 | -11.8% | +24.7% | +36.2% |

### i386, optimized, by corpus

| corpus | files | compiled: stock / ours | common | stock | ours | clang | gcc | ours vs stock | ours vs clang | ours vs gcc |
|---|---|---|---|---|---|---|---|---|---|---|
| lua | 34 | 34 / 34 | 34 | 189,844 | 180,896 | 162,803 | 153,121 | -4.7% | +11.1% | +18.1% |
| sqlite | 1 | 0 / 0 | 0 | - | - |  |  | stock fails |  |  |
| zlib | 15 | 15 / 15 | 12 | 69,561 | 66,094 | 50,545 | 45,339 | -5.0% | +30.8% | +45.8% |
| lz4 | 5 | 5 / 5 | 5 | 248,137 | 238,107 | 47,173 | 34,631 | -4.0% | +404.8% | +587.6% |
| cjson | 2 | 2 / 2 | 2 | 18,979 | 18,023 | 17,326 | 14,441 | -5.0% | +4.0% | +24.8% |
| bearssl | 294 | 294 / 294 | 294 | 291,876 | 289,915 | 229,481 | 224,982 | -0.7% | +26.3% | +28.9% |
| mbedtls | 108 | 101 / 84 | 84 | 483,381 | 468,321 | 337,603 | 340,367 | -3.1% | +38.7% | +37.6% |
| chibicc | 8 | 8 / 8 | 8 | 87,154 | 84,548 | 77,541 | 70,348 | -3.0% | +9.0% | +20.2% |
| c-testsuite | 220 | 215 / 215 | 214 | 36,142 | 35,304 | 29,779 | 27,162 | -2.3% | +18.6% | +30.0% |
| gcc-torture | 213 | 173 / 173 | 161 | 500,486 | 475,092 | 447,750 | 402,365 | -5.1% | +6.1% | +18.1% |

### aarch64, optimized, by corpus

| corpus | files | compiled: stock / ours | common | stock | ours | clang | gcc | ours vs stock | ours vs clang | ours vs gcc |
|---|---|---|---|---|---|---|---|---|---|---|
| lua | 34 | 3 / 34 | 3 | 588 | 656 | 656 |  | +11.6% | +0.0% |  |
| sqlite | 1 | 0 / 0 | 0 | - | - |  |  | stock fails |  |  |
| zlib | 15 | 1 / 15 | 1 | 76 | 56 | 36 |  | -26.3% | +55.6% |  |
| lz4 | 5 | 0 / 5 | 0 | - | - |  |  | stock fails |  |  |
| cjson | 2 | 0 / 2 | 0 | - | - |  |  | stock fails |  |  |
| bearssl | 294 | 55 / 294 | 55 | 4,235 | 3,071 | 2,886 |  | -27.5% | +6.4% |  |
| mbedtls | 108 | 15 / 87 | 14 | 3,229 | 3,689 | 3,607 |  | +14.2% | +2.3% |  |
| chibicc | 8 | 0 / 8 | 0 | - | - |  |  | stock fails |  |  |
| c-testsuite | 220 | 138 / 214 | 138 | 16,219 | 12,074 | 11,338 |  | -25.6% | +6.5% |  |
| gcc-torture | 213 | 59 / 170 | 51 | 730,647 | 605,335 | 531,769 |  | -17.2% | +13.8% |  |

Unoptimized, per corpus:

<details><summary>amd64</summary>

| corpus | files | compiled: stock / ours | common | stock | ours | clang | gcc | ours vs stock | ours vs clang | ours vs gcc |
|---|---|---|---|---|---|---|---|---|---|---|
| lua | 34 | 34 / 34 | 34 | 350,841 | 350,352 | 275,985 | 281,172 | -0.1% | +26.9% | +24.6% |
| sqlite | 1 | 0 / 0 | 0 | - | - |  |  | stock fails |  |  |
| zlib | 15 | 15 / 15 | 12 | 135,945 | 135,907 | 106,488 | 106,978 | -0.0% | +27.6% | +27.0% |
| lz4 | 5 | 5 / 5 | 5 | 95,294 | 95,110 | 74,719 | 75,721 | -0.2% | +27.3% | +25.6% |
| cjson | 2 | 2 / 2 | 2 | 40,849 | 40,733 | 32,740 | 30,341 | -0.3% | +24.4% | +34.3% |
| bearssl | 294 | 294 / 294 | 294 | 520,585 | 519,729 | 370,160 | 425,742 | -0.2% | +40.4% | +22.1% |
| mbedtls | 108 | 101 / 84 | 84 | 780,237 | 779,102 | 571,550 | 590,336 | -0.1% | +36.3% | +32.0% |
| chibicc | 8 | 8 / 8 | 8 | 164,981 | 164,905 | 111,726 | 114,544 | -0.0% | +47.6% | +44.0% |
| c-testsuite | 220 | 215 / 215 | 215 | 61,546 | 61,517 | 51,992 | 52,328 | -0.0% | +18.3% | +17.6% |
| gcc-torture | 213 | 172 / 172 | 160 | 758,699 | 758,615 | 698,995 | 619,252 | -0.0% | +8.5% | +22.5% |

</details>

<details><summary>i386</summary>

| corpus | files | compiled: stock / ours | common | stock | ours | clang | gcc | ours vs stock | ours vs clang | ours vs gcc |
|---|---|---|---|---|---|---|---|---|---|---|
| lua | 34 | 34 / 34 | 34 | 221,167 | 220,420 | 277,310 | 247,690 | -0.3% | -20.5% | -11.0% |
| sqlite | 1 | 0 / 0 | 0 | - | - |  |  | stock fails |  |  |
| zlib | 15 | 15 / 15 | 12 | 74,634 | 74,475 | 79,338 | 77,603 | -0.2% | -6.1% | -4.0% |
| lz4 | 5 | 5 / 5 | 5 | 59,720 | 59,478 | 72,972 | 63,851 | -0.4% | -18.5% | -6.8% |
| cjson | 2 | 2 / 2 | 2 | 25,025 | 24,941 | 31,241 | 25,974 | -0.3% | -20.2% | -4.0% |
| bearssl | 294 | 294 / 294 | 294 | 323,570 | 322,545 | 376,185 | 400,792 | -0.3% | -14.3% | -19.5% |
| mbedtls | 108 | 101 / 84 | 84 | 516,198 | 515,144 | 575,356 | 520,060 | -0.2% | -10.5% | -0.9% |
| chibicc | 8 | 8 / 8 | 8 | 98,762 | 98,638 | 105,691 | 100,878 | -0.1% | -6.7% | -2.2% |
| c-testsuite | 220 | 215 / 215 | 214 | 43,260 | 43,217 | 52,990 | 47,615 | -0.1% | -18.4% | -9.2% |
| gcc-torture | 213 | 173 / 173 | 161 | 564,565 | 564,471 | 657,164 | 632,678 | -0.0% | -14.1% | -10.8% |

</details>

<details><summary>aarch64</summary>

| corpus | files | compiled: stock / ours | common | stock | ours | clang | gcc | ours vs stock | ours vs clang | ours vs gcc |
|---|---|---|---|---|---|---|---|---|---|---|
| lua | 34 | 2 / 34 | 2 | 340 | 340 | 340 |  | +0.0% | +0.0% |  |
| sqlite | 1 | 0 / 0 | 0 | - | - |  |  | stock fails |  |  |
| zlib | 15 | 1 / 15 | 1 | 822 | 698 | 802 |  | -15.1% | -13.0% |  |
| lz4 | 5 | 0 / 5 | 0 | - | - |  |  | stock fails |  |  |
| cjson | 2 | 0 / 2 | 0 | - | - |  |  | stock fails |  |  |
| bearssl | 294 | 56 / 294 | 56 | 5,901 | 5,397 | 4,265 |  | -8.5% | +26.5% |  |
| mbedtls | 108 | 15 / 87 | 14 | 3,357 | 3,845 | 3,779 |  | +14.5% | +1.7% |  |
| chibicc | 8 | 0 / 8 | 0 | - | - |  |  | stock fails |  |  |
| c-testsuite | 220 | 157 / 214 | 157 | 26,694 | 23,362 | 23,223 |  | -12.5% | +0.6% |  |
| gcc-torture | 213 | 73 / 170 | 64 | 838,465 | 787,461 | 801,750 |  | -6.1% | -1.8% |  |

</details>

### Which back ends compile the corpus at all

Files (of 900) compiled and assembled, optimized, and the most common reason the rest stop:

| target | stock | ours | most common ours failure | most common stock failure |
|---|---|---|---|---|
| aarch64 | 271 | 829 | cannot initialise function | compiler error: Coalesce: src class N, dst class N |
| amd64 | 846 | 829 | cannot initialise function | syntax error |
| i386 | 847 | 830 | cannot initialise function | syntax error |
| arm | 489 | 487 | major internal compiler error: <stdin>, line N | major internal compiler error: <stdin>, line N |
| powerpc | 0 | 0 | no ccom for ours/powerpc | no ccom for stock/powerpc |
| m68k | 121 | 121 | major internal compiler error: <stdin>, line N | major internal compiler error: <stdin>, line N |
| mips | 546 | 551 | compiler error: Cannot generate code, node Nx op F | compiler error: Cannot generate code, node Nx op F |
| mips64 | 442 | 449 | compiler error: Cannot generate code, node Nx op T | compiler error: Cannot generate code, node Nx op T |
| riscv64 | 0 | 0 | invalid function definition | invalid function definition |
| pdp11 | 767 | 503 | compiler error: strmemb | major internal compiler error: <stdin>, line N |
| vax | 808 | 807 | no computed goto in inlined functions | major internal compiler error: <stdin>, line N |
| sparc64 | 276 | 275 | compiler error: Cannot generate code, node Nx op T | compiler error: Cannot generate code, node Nx op T |

## PDP-11 against V7 dmr cc

Corpus: V7's `usr/src/libc` and `usr/src/cmd` (K&R C).  Measured with `tools/pccdiff.sh`, which compiles every file
with V7 dmr cc (ported to the host) and with pcc, assembles with V7 `as` and compares text plus
data.  "No `-O`" is `pcc -S` followed by V7's peephole `c2`; "`-O`" is ccom's passes, then `c2`.  Files both pcc
builds compiled:

| corpus | files | V7 dmr cc | stock, no `-O` | stock `-O` | ours, no `-O` | ours `-O` |
|---|---|---|---|---|---|---|
| libc | 86 | 15,062 | 23,560 (+56.4%) | 17,542 (+16.5%) | 21,364 (+41.8%) | **14,574 (-3.2%)** |
| commands | 43 | 84,346 | 112,044 (+32.8%) | 91,746 (+8.8%) | 102,984 (+22.1%) | **80,812 (-4.2%)** |

Over all files each compiles: libc 89 files for ours (stock 86), commands 52 (stock 43).  Stock rejects files that
the fork compiles; the fork rejects none that stock compiles.

One libc file is larger than V7 dmr cc's (`qsort`) and that one is more correct: dmr cc's `clr r0; div` for an unsigned
divide by a variable is wrong for divisors of 32768 and up (shown by running its own output); pcc calls a
helper that is right.  `BIGGER-THAN-ORIGINAL.md` in the toolchain repository lists every larger file and why.

## VAX against 32V pcc

Corpus: 32V's `libc/gen`, `libc/stdio` and `cmd` sources.  The reference sizes come from the real 32V operating
system under SIMH (`tools/vaxdiff.py`): its `cc` and `cc -O`, here called 32V pcc and 32V pcc -O.  That compiler is Johnson's pcc, so this is also the
comparison with the original pcc.  The guest produced a full set of sizes for 115 of the 229 sources (for the other
ones it returned an incomplete set, for example no `cc -O` size; I did not investigate why).  Files with a unique
name that every row handled:

| corpus | files | 32V pcc | 32V pcc -O | stock no `-O` | stock `-O` | ours no `-O` | ours `-O` |
|---|---|---|---|---|---|---|---|
| libc (gen, stdio) | 81 | 13,784 | 12,536 | 20,532 (+49.0%) | 13,660 (+9.0%) | 20,860 (+51.3%) | **11,908 (-5.0%)** |
| commands | 28 | 12,076 | 11,348 | 15,832 (+31.1%) | 11,732 (+3.4%) | 15,972 (+32.3%) | **10,396 (-8.4%)** |

No-`-O` columns are against 32V pcc, `-O` columns against 32V pcc -O.

## What this run found

Running a new corpus on every back end turned up things the V7 and 32V sources never did.  None of these was
fixed to make the numbers look better.  The one exception is the case-range hang below, which was fixed because it
hangs the compiler; every measurement here uses the compiler with that fix.

* **amd64, `-O`: 17 files that stock compiles and the fork does not.**  17 x "cannot initialise function"
* **pdp11, `-O`: 296 files that stock compiles and the fork does not.**  280 x "compiler error: strmemb"; 12 x "cannot initialise function"; 1 x "timeout"; 1 x "no computed goto in inlined functions"
* **vax, `-O`: 32 files that stock compiles and the fork does not.**  32 x "no computed goto in inlined functions"

  * `cannot initialise function` is the scanner's K&R initializer rule (`int a[] { 1, 2 };`, added for the V7
    sources) misreading a function definition that returns a pointer to an array, `T (*f(args))[N] { ... }`:
    at file scope a `{` after a `]` is taken for an initializer.  Introduced by the second commit of the series
    ("pdp11: dmr-cc style addressing ...", `cc/ccom/scan.l`); found by bisection; stock parses it.
  * `compiler error: strmemb` is an internal error in pass 1 under `-O` on the PDP-11 (the same file compiles
    without `-O`); not yet analysed.
  * `no computed goto in inlined functions` is the VAX `casel` switch (a computed jump) making the inliner
    refuse a function, as an error instead of simply not inlining it.

* **`-xinline` makes `always_inline`-heavy code much bigger, in stock and in the fork.**  lz4's five files on amd64
  are 95,110 bytes unoptimized and 269,627 with `-O` for the fork (stock: 95,294 and 291,735), against 45,380 for clang `-Os` and 32,733 for
  gcc `-Os`.  lz4 builds many specializations from one function by passing constants to `LZ4_FORCE_INLINE` functions;
  the likely reason is that clang and gcc fold those constants after inlining and drop the dead branches, which pcc's
  inliner does not (not verified here).  This one corpus is why the lz4 row looks so bad; leaving it out changes no
  conclusion above.
* **ccom hung on the GNU `case LOW ... HIGH:` range with a huge range.**  `case 1000000000000000000ULL ...
  9999999999999999999ULL:` never finished, in stock and in the fork on amd64, i386, VAX and PDP-11: `gcccase()`
  added one switch entry per value (and compared signed for an unsigned switch).  The fork now keeps a long range
  as one range test and compiles that file (commit "ccom: GNU case ranges ..."); stock still hangs and is counted as
  a failure (a 90-second limit) on that file for every target.
* **Stock's powerpc back end does not build** (`sz =  = p2autooff` in `arch/powerpc/local2.c`, since August 2023)
  and riscv64 compiles nothing: every file stops with "invalid function definition".
* SQLite's amalgamation does not compile with any pcc here: it needs the `__atomic_*` builtins and `__ATOMIC_*`
  macros, which pcc does not have.
* A build-system trap, for anyone repeating this: the fork's tree tracks a prebuilt `pdp11-bsd-ccom`, which a naive
  "pick the `*-ccom` in the directory" mistakes for the compiler of any target that sorts after `p`; the first
  riscv64, sparc64 and VAX numbers came from that mistake and were thrown away.

## Reproducing

All of the harness is in the toolchain repository (`facetlang/pdp11/v7unix-toolchain/tools`):

```
corpus-build.sh                  # stock (git archive of origin/master) and ours, every target, plus c2 for PDP-11 and VAX
corpus.py -j 2                  # incremental, resumable; --arch to restrict; --full for every torture test
corpus-report.py                # this file
pccdiff.sh -v orig/usr/src/libc  # PDP-11 against V7 dmr cc (PCC=... for stock)
vaxdiff.py --ccom CCOM ...       # VAX against the 32V guest
corpus-gcc-cross.sh              # optional: binutils and a C-only GCC for vax-netbsdelf and pdp11-aout
```

Corpora live in `~/claude/corpora`: Lua 5.4.7, SQLite 3.46.1 (amalgamation), zlib 1.3.1, lz4 1.9.4, cJSON 1.7.18,
BearSSL, mbedTLS 3.6.2, chibicc, c-testsuite, every eighth test of GCC's `gcc.c-torture/execute`, and the musl 1.2.5
headers for each architecture.

Limits: GCC's own VAX and PDP-11 back ends are not in the tables (the cross compilers were not finished); the 32V
reference covers 115 of the sources; the PDP-11 "no `-O`" column of the V7 table is `pcc -S` followed by V7's `c2`
(as `pccdiff.sh` defines it) while the unoptimized columns elsewhere are raw `ccom` output; the PDP-11 C99 numbers
mix correctness differences (the fork has a 64-bit `long long`, stock does not) with code quality.
