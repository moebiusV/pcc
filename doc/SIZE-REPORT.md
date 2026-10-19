# Code size: stock PortableCC against this fork, clang, gcc, V7 dmr cc and 32V pcc

This report compares the code size of this fork of pcc with **stock PortableCC** (current `PortableCC/pcc`
master, commit 9a993a5f), with **clang** and **gcc** on the targets they share with pcc, and, for the two
old targets, with the **original compilers**: V7 dmr cc (the PDP-11 compiler of V7 Unix) and 32V pcc (the VAX compiler of 32V, itself
Johnson's pcc).  Ten C99 corpora (900 source files: lua 34, sqlite 1, zlib 15, lz4 5, cjson 2, bearssl 294, mbedtls 108, chibicc 8, c-testsuite 220, gcc-torture 213) are compiled for every back end that pcc and clang
have in common, with and without optimization, plus the V7 and 32V source trees that the earlier size
work used.  Everything is regenerated from the measurement files by `tools/corpus-report.py`.

Base: the fork contains upstream PortableCC commit `9a993a5f` (it was merged on 18 October 2026), which is also the
"stock" measured here, so stock and the fork differ only by the fork's own changes.  Where both implement the same
idea (cross-jumping on the intermediate code, the branch inversion that keeps operand order) both are kept when
the fork's adds something: its cross-jumping, behind `-xdmr`, is 1% smaller on amd64 and AArch64 on top of
upstream's and never larger on the 126 files tried.

## Summary

* **PDP-11, V7 libc and commands, against V7 dmr cc** (86 + 43 files both pcc builds compile): stock `-O` is
  **+16.5% / +8.8%** against V7 dmr cc; this fork's `-O` is **-3.2% / -4.2%** (and -16.9% / -11.9% against stock `-O`).
* **VAX, 32V libc and commands, against 32V pcc -O**: see the VAX section; stock `-O` is +9.0% / +3.4% and this
  fork's `-O` is -5.1% / -8.4%.
* **All C99 corpora, optimized, files every compiler compiled**: on amd64 ours is 9.4% smaller than stock, 49.1% larger than clang `-Os` and 59.7% larger than gcc `-Os`;
  on i386 3.5% smaller than stock, 32.6% larger than clang and 40.5% larger than gcc.
* **AArch64**: stock's back end compiles 271 of the 900 files, the fork 845.  Over the 263 files all three compiled, the
  fork is 17.3% smaller than stock and 13.4% larger than clang `-Os`; over every file the fork and clang both compiled it is 27.4% larger than clang.
* **The other back ends** (arm, m68k, mips, mips64, sparc64) compile only part of the corpus, in stock and in the
  fork alike, riscv64 and powerpc nothing: they are unfinished ports the fork did not work on.
* **Reliability**: files stock compiles and the fork does not: amd64 0, i386 0, aarch64 0, pdp11 2, vax 1; files the fork compiles and stock does not: amd64 0, i386 0, aarch64 574, pdp11 50, vax 35.  The corpus found three bugs in the fork that made the first set of these numbers worse (all fixed, see "What this run found").

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
* **PDP-11 and VAX** have no clang; gcc 14.2 is built as a cross-compiler for them (`pdp11-aout`, `vax-netbsdelf`, see
  `tools/corpus-gcc-cross.sh`; sizes come from the matching binutils).  pcc uses V7's `as` and `c2` (ported to the host) and `as32v`, a host
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
| aarch64 | 900 | 304 / 845 | 295 | 881,008 | 825,748 | 838,746 |  | -6.3% | -1.5% |  |
| amd64 | 900 | 846 / 846 | 831 | 3,367,445 | 3,363,857 | 2,645,796 | 2,646,867 | -0.1% | +27.1% | +27.1% |
| i386 | 900 | 847 / 847 | 831 | 2,223,628 | 2,219,596 | 2,584,198 | 2,426,591 | -0.2% | -14.1% | -8.5% |
| arm | 900 | 408 / 408 | 399 | 962,470 | 962,470 | 886,039 |  | +0.0% | +8.6% |  |
| powerpc | 900 | 0 / 0 | 0 | - | - |  |  | stock fails |  |  |
| m68k | 900 | 8 / 8 | 8 | 932 | 932 | 928 |  | +0.0% | +0.4% |  |
| mips | 900 | 571 / 574 | 558 | 2,144,759 | 2,141,563 | 1,708,698 |  | -0.1% | +25.3% |  |
| mips64 | 900 | 468 / 505 | 456 | 1,775,995 | 1,775,699 | 1,635,275 |  | -0.0% | +8.6% |  |
| riscv64 | 900 | 0 / 0 | 0 | - | - |  |  | stock fails |  |  |
| pdp11 | 900 | 800 / 814 | 796 | 1,267,018 | 1,610,502 |  | 1,426,696 | +27.1% |  | +11.6% |
| vax | 900 | 822 / 845 | 822 | 1,567,828 | 1,577,880 |  | 1,410,325 | +0.6% |  | +10.8% |
| sparc64 | 900 | 456 / 456 | 447 | 1,419,689 | 1,418,761 | 1,202,443 |  | -0.1% | +18.0% |  |

Optimized:

| target | files | compiled: stock / ours | common | stock | ours | clang | gcc | ours vs stock | ours vs clang | ours vs gcc |
|---|---|---|---|---|---|---|---|---|---|---|
| aarch64 | 900 | 271 / 845 | 263 | 759,583 | 627,918 | 553,663 |  | -17.3% | +13.4% |  |
| amd64 | 900 | 846 / 846 | 831 | 2,670,888 | 2,420,282 | 1,623,664 | 1,515,580 | -9.4% | +49.1% | +59.7% |
| i386 | 900 | 847 / 847 | 831 | 2,192,826 | 2,115,388 | 1,595,168 | 1,505,293 | -3.5% | +32.6% | +40.5% |
| arm | 900 | 489 / 489 | 479 | 950,237 | 916,429 | 641,538 |  | -3.6% | +42.8% |  |
| powerpc | 900 | 0 / 0 | 0 | - | - |  |  | stock fails |  |  |
| m68k | 900 | 121 / 121 | 121 | 4,687 | 4,687 | 2,347 |  | +0.0% | +99.7% |  |
| mips | 900 | 546 / 552 | 533 | 1,490,430 | 1,479,914 | 826,549 |  | -0.7% | +79.0% |  |
| mips64 | 900 | 442 / 449 | 433 | 1,292,406 | 1,291,697 | 943,191 |  | -0.1% | +36.9% |  |
| riscv64 | 900 | 0 / 0 | 0 | - | - |  |  | stock fails |  |  |
| pdp11 | 900 | 767 / 815 | 765 | 868,722 | 1,115,636 |  | 624,984 | +28.4% |  | +70.4% |
| vax | 900 | 808 / 842 | 807 | 1,099,276 | 1,047,516 |  | 805,953 | -4.7% |  | +28.8% |
| sparc64 | 900 | 276 / 275 | 268 | 32,071 | 31,947 | 14,884 |  | -0.4% | +114.6% |  |

"Common" is the number of files all compilers of that row compiled; the size columns sum those files only.  On the
targets without clang (PDP-11, VAX) the comparison is stock against ours.

### amd64, optimized, by corpus

| corpus | files | compiled: stock / ours | common | stock | ours | clang | gcc | ours vs stock | ours vs clang | ours vs gcc |
|---|---|---|---|---|---|---|---|---|---|---|
| lua | 34 | 34 / 34 | 34 | 237,051 | 204,843 | 164,118 | 157,421 | -13.6% | +24.8% | +30.1% |
| sqlite | 1 | 0 / 0 | 0 | - | - |  |  | stock fails |  |  |
| zlib | 15 | 15 / 15 | 12 | 98,076 | 90,503 | 56,515 | 51,187 | -7.7% | +60.1% | +76.8% |
| lz4 | 5 | 5 / 5 | 5 | 291,735 | 269,527 | 45,380 | 32,733 | -7.6% | +493.9% | +723.4% |
| cjson | 2 | 2 / 2 | 2 | 22,232 | 20,640 | 17,745 | 14,703 | -7.2% | +16.3% | +40.4% |
| bearssl | 294 | 294 / 294 | 294 | 299,463 | 281,240 | 218,622 | 202,004 | -6.1% | +28.6% | +39.2% |
| mbedtls | 108 | 101 / 101 | 101 | 925,829 | 845,976 | 557,531 | 542,316 | -8.6% | +51.7% | +56.0% |
| chibicc | 8 | 8 / 8 | 8 | 115,667 | 105,758 | 82,351 | 75,019 | -8.6% | +28.4% | +41.0% |
| c-testsuite | 220 | 215 / 215 | 215 | 43,250 | 39,726 | 30,680 | 27,492 | -8.1% | +29.5% | +44.5% |
| gcc-torture | 213 | 172 / 172 | 160 | 637,585 | 562,069 | 450,722 | 412,705 | -11.8% | +24.7% | +36.2% |

### i386, optimized, by corpus

| corpus | files | compiled: stock / ours | common | stock | ours | clang | gcc | ours vs stock | ours vs clang | ours vs gcc |
|---|---|---|---|---|---|---|---|---|---|---|
| lua | 34 | 34 / 34 | 34 | 189,844 | 180,884 | 162,803 | 153,121 | -4.7% | +11.1% | +18.1% |
| sqlite | 1 | 0 / 0 | 0 | - | - |  |  | stock fails |  |  |
| zlib | 15 | 15 / 15 | 12 | 69,561 | 66,066 | 50,545 | 45,339 | -5.0% | +30.7% | +45.7% |
| lz4 | 5 | 5 / 5 | 5 | 248,137 | 238,047 | 47,173 | 34,631 | -4.1% | +404.6% | +587.4% |
| cjson | 2 | 2 / 2 | 2 | 18,979 | 18,079 | 17,326 | 14,441 | -4.7% | +4.3% | +25.2% |
| bearssl | 294 | 294 / 294 | 294 | 291,876 | 289,904 | 229,481 | 224,982 | -0.7% | +26.3% | +28.9% |
| mbedtls | 108 | 101 / 101 | 101 | 750,647 | 727,464 | 532,770 | 532,904 | -3.1% | +36.5% | +36.5% |
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
| mbedtls | 108 | 15 / 104 | 15 | 7,818 | 7,462 | 6,978 |  | -4.6% | +6.9% |  |
| chibicc | 8 | 0 / 8 | 0 | - | - |  |  | stock fails |  |  |
| c-testsuite | 220 | 138 / 213 | 138 | 16,219 | 11,962 | 11,338 |  | -26.2% | +5.5% |  |
| gcc-torture | 213 | 59 / 170 | 51 | 730,647 | 604,711 | 531,769 |  | -17.2% | +13.7% |  |

Unoptimized, per corpus:

<details><summary>amd64</summary>

| corpus | files | compiled: stock / ours | common | stock | ours | clang | gcc | ours vs stock | ours vs clang | ours vs gcc |
|---|---|---|---|---|---|---|---|---|---|---|
| lua | 34 | 34 / 34 | 34 | 350,841 | 350,315 | 275,985 | 281,172 | -0.1% | +26.9% | +24.6% |
| sqlite | 1 | 0 / 0 | 0 | - | - |  |  | stock fails |  |  |
| zlib | 15 | 15 / 15 | 12 | 135,945 | 135,775 | 106,488 | 106,978 | -0.1% | +27.5% | +26.9% |
| lz4 | 5 | 5 / 5 | 5 | 95,294 | 95,110 | 74,719 | 75,721 | -0.2% | +27.3% | +25.6% |
| cjson | 2 | 2 / 2 | 2 | 40,849 | 40,733 | 32,740 | 30,341 | -0.3% | +24.4% | +34.3% |
| bearssl | 294 | 294 / 294 | 294 | 520,585 | 519,729 | 370,160 | 425,742 | -0.2% | +40.4% | +22.1% |
| mbedtls | 108 | 101 / 101 | 101 | 1,238,705 | 1,237,177 | 922,991 | 940,789 | -0.1% | +34.0% | +31.5% |
| chibicc | 8 | 8 / 8 | 8 | 164,981 | 164,905 | 111,726 | 114,544 | -0.0% | +47.6% | +44.0% |
| c-testsuite | 220 | 215 / 215 | 215 | 61,546 | 61,510 | 51,992 | 52,328 | -0.1% | +18.3% | +17.5% |
| gcc-torture | 213 | 172 / 172 | 160 | 758,699 | 758,603 | 698,995 | 619,252 | -0.0% | +8.5% | +22.5% |

</details>

<details><summary>i386</summary>

| corpus | files | compiled: stock / ours | common | stock | ours | clang | gcc | ours vs stock | ours vs clang | ours vs gcc |
|---|---|---|---|---|---|---|---|---|---|---|
| lua | 34 | 34 / 34 | 34 | 221,167 | 220,399 | 277,310 | 247,690 | -0.3% | -20.5% | -11.0% |
| sqlite | 1 | 0 / 0 | 0 | - | - |  |  | stock fails |  |  |
| zlib | 15 | 15 / 15 | 12 | 74,634 | 74,363 | 79,338 | 77,603 | -0.4% | -6.3% | -4.2% |
| lz4 | 5 | 5 / 5 | 5 | 59,720 | 59,478 | 72,972 | 63,851 | -0.4% | -18.5% | -6.8% |
| cjson | 2 | 2 / 2 | 2 | 25,025 | 24,941 | 31,241 | 25,974 | -0.3% | -20.2% | -4.0% |
| bearssl | 294 | 294 / 294 | 294 | 323,570 | 322,545 | 376,185 | 400,792 | -0.3% | -14.3% | -19.5% |
| mbedtls | 108 | 101 / 101 | 101 | 812,925 | 811,562 | 931,307 | 829,510 | -0.2% | -12.9% | -2.2% |
| chibicc | 8 | 8 / 8 | 8 | 98,762 | 98,638 | 105,691 | 100,878 | -0.1% | -6.7% | -2.2% |
| c-testsuite | 220 | 215 / 215 | 214 | 43,260 | 43,211 | 52,990 | 47,615 | -0.1% | -18.5% | -9.2% |
| gcc-torture | 213 | 173 / 173 | 161 | 564,565 | 564,459 | 657,164 | 632,678 | -0.0% | -14.1% | -10.8% |

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
| mbedtls | 108 | 15 / 104 | 15 | 8,786 | 8,530 | 8,366 |  | -2.9% | +2.0% |  |
| chibicc | 8 | 0 / 8 | 0 | - | - |  |  | stock fails |  |  |
| c-testsuite | 220 | 157 / 213 | 157 | 26,694 | 23,362 | 23,223 |  | -12.5% | +0.6% |  |
| gcc-torture | 213 | 73 / 170 | 64 | 838,465 | 787,421 | 801,750 |  | -6.1% | -1.8% |  |

</details>

### AArch64 against upstream pull request #47

PortableCC pull request #47 (hakanrw, open since February 2026) overhauls the old AArch64 back end; its code is
built here as a third compiler (`pr47`) beside stock and the fork, on the same files and with the same assembler.

| | compiled and assembled, `-O` | `-O0` |
|---|---|---|
| stock | 271 / 900 | 304 / 900 |
| PR #47 | 680 / 900 | 682 / 900 |
| the fork | 845 / 900 | 845 / 900 |

Sizes at `-O`, on the 261 files stock, PR #47, the fork and clang all compiled: stock 35,790, PR #47 42,614 (+19.1% against stock),
the fork 26,721 (-25.3% against stock); on the 664 files PR #47, the fork and clang all compiled, the fork is 37.1% smaller than PR #47
and 33.9% larger than clang `-Os` (PR #47: 112.9% larger than clang).

Why PR #47 stops (`-O`): 141 at the asm stage, 74 at the ccom stage.  Its indirect calls are written `mov lr,pc`, which is not an AArch64 instruction.

### Which back ends compile the corpus at all

Files (of 900) compiled and assembled, optimized, and the most common reason the rest stop:

| target | stock | ours | most common ours failure | most common stock failure |
|---|---|---|---|---|
| aarch64 | 271 | 845 | syntax error | compiler error: Coalesce: src class N, dst class N |
| amd64 | 846 | 846 | syntax error | syntax error |
| i386 | 847 | 847 | syntax error | syntax error |
| arm | 489 | 489 | major internal compiler error: <stdin>, line N | major internal compiler error: <stdin>, line N |
| powerpc | 0 | 0 | no ccom for ours/powerpc | no ccom for stock/powerpc |
| m68k | 121 | 121 | major internal compiler error: <stdin>, line N | major internal compiler error: <stdin>, line N |
| mips | 546 | 552 | compiler error: Cannot generate code, node Nx op F | compiler error: Cannot generate code, node Nx op F |
| mips64 | 442 | 449 | compiler error: Cannot generate code, node Nx op T | compiler error: Cannot generate code, node Nx op T |
| riscv64 | 0 | 0 | invalid function definition | invalid function definition |
| pdp11 | 767 | 815 | syntax error | major internal compiler error: <stdin>, line N |
| vax | 808 | 842 | syntax error | major internal compiler error: <stdin>, line N |
| sparc64 | 276 | 275 | compiler error: Cannot generate code, node Nx op T | compiler error: Cannot generate code, node Nx op T |

## PDP-11 against V7 dmr cc

Corpus: V7's `usr/src/libc` and `usr/src/cmd` (K&R C).  Measured with `tools/pccdiff.sh`, which compiles every file
with V7 dmr cc (ported to the host) and with pcc, assembles with V7 `as` and compares text plus
data.  "No `-O`" is `pcc -S` followed by V7's peephole `c2`; "`-O`" is ccom's passes, then `c2`.  Files both pcc
builds compiled:

| corpus | files | V7 dmr cc | stock, no `-O` | stock `-O` | ours, no `-O` | ours `-O` |
|---|---|---|---|---|---|---|
| libc | 86 | 15,062 | 23,560 (+56.4%) | 17,542 (+16.5%) | 21,364 (+41.8%) | **14,574 (-3.2%)** |
| commands | 43 | 84,346 | 112,044 (+32.8%) | 91,746 (+8.8%) | 102,984 (+22.1%) | **80,810 (-4.2%)** |

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
| libc (gen, stdio) | 81 | 13,784 | 12,536 | 20,532 (+49.0%) | 13,660 (+9.0%) | 20,672 (+50.0%) | **11,896 (-5.1%)** |
| commands | 28 | 12,076 | 11,348 | 15,832 (+31.1%) | 11,732 (+3.4%) | 15,936 (+32.0%) | **10,396 (-8.4%)** |

No-`-O` columns are against 32V pcc, `-O` columns against 32V pcc -O.

## What this run found

Running a new corpus on every back end turned up things the V7 and 32V sources never did.  The measurements were
taken first with the fork as it stood, and the problems below were fixed afterwards, one commit each; the tables
are the re-measurement with the fixes (only the fork's columns were recompiled, the others are unchanged).

**Found by the corpus and fixed:**

* **`cannot initialise function`** on 17 mbedTLS files (amd64, i386; 12 more on the PDP-11): the scanner's K&R
  initializer rule (`int a[] { 1, 2 };`, added for the V7 sources) read the `{` of a function that returns a
  pointer to an array, `T (*f(args))[N] { ... }`, as an initializer.  Commit "scan: a function definition that
  returns a pointer to an array is not a K&R initializer".
* **`compiler error: strmemb`** on 280 PDP-11 files under `-O` (BearSSL, mbedTLS): with `-xdmr` a word result
  is returned in `r0` directly, and the inliner reads "temporary 0" as "no return value" (r0 is register 0).
  Commit "ccom: -xdmr does not return through r0 inside a function that is being saved for inlining".
* **`no computed goto in inlined functions`** on 32 VAX files (and an internal error on the PDP-11): an inline
  function is saved as intermediate code, and the jump table of a dense `switch` (on the VAX the label list that
  `casel` adds) is not copied with it.  Inside such a function the switch is now compare code, so the function
  is still expanded at its call sites.  Commit "pdp11, vax: no jump table in a function saved for inlining".
* **`ccom` hung on the GNU `case LOW ... HIGH:` range with a huge range**, in stock and in the fork on amd64,
  i386, VAX and PDP-11: `case 1000000000000000000ULL ... 9999999999999999999ULL:` never finished (`gcccase()`
  added one switch entry per value and compared signed for an unsigned switch).  A long range is now one range
  test.  Stock still hangs on that file and is counted as a failure (a 90-second limit) for every target.

**What is still different from stock** (files stock compiles and the fork does not, optimized):

* amd64: 0
* i386: 0
* aarch64: 0
* pdp11: 2; 1 x "conversion between long long and floating point is not suppo"; 1 x "compiler error: long long operation N is not supported"
* vax: 1; 1 x "Assembler:"

**Other findings:**

* **`-xinline` makes `always_inline`-heavy code much bigger, in stock and in the fork.**  lz4's five files on amd64
  are 95,110 bytes unoptimized and 269,527 with `-O` for the fork (stock: 95,294 and 291,735), against 45,380 for clang `-Os` and 32,733 for
  gcc `-Os`.  lz4 builds many specializations from one function by passing constants to `LZ4_FORCE_INLINE` functions;
  the likely reason is that clang and gcc fold those constants after inlining and drop the dead branches, which pcc's
  inliner does not (not verified here).  This one corpus is why the lz4 row looks so bad; leaving it out changes no
  conclusion above.
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
