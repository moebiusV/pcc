# pdp64: targets, options and optimizations

The pdp64 back end (`arch/pdp64`) compiles for three targets of one family:

| target | encoding | registers | `ccom` |
|---|---|---|---|
| `pdp64/r20` | revision 20 (the original) | r0-r15 | the 16-register build; the default; its output is the baseline and does not change |
| `pdp64/16` | prefix-coded, 16 registers | r0-r15 | the 16-register build |
| `pdp64/32` | prefix-coded, 32 registers | r0-r31 | the 32-register build (`-DPDP64_REGS32`) |

## Choosing the target

`ccom -m pdp64/16 file.i file.s` (the same spelling as `as64 -m`).  The register file is a table of the build (`mip/mkext.c` writes
`external.c` from `RSTATUS`), so the 32-register compiler is a second build of the same sources: `-DPDP64_REGS32` in `CFLAGS`.
`arch/pdp64/test/build-ccom.sh DIR` makes both (`DIR/ccom`, `DIR/ccom32`).  A `ccom` refuses a target it was not built for, with a message
(`-m pdp64/32` to the 16-register build and the other way round).  The 32-register build defaults to `pdp64/32`.

The assembler must be given the same `-m` (`as64 -m pdp64/32 ...`): the encoding is recorded in the object's `e_flags`.

Not done by this compiler: `cc/cc`, the pcc driver, is not built for pdp64 (no `os`/`mach` headers: `CPPMDADD` is missing), so every
tool calls `ccom` itself.  The driver's `-O` table (`ccomflgcheck`) knows the pdp64 flags for the day it is built.

## Registers and the ABI

Pass-2 numbers 0-11 are r0-r11, 12 is r15, 13 fp, 14 sp, 15 r12 (the scratch register, not allocated); in the 32-register build 16-31
are r16-r31.  r0-r7 and r16-r23 are clobbered by a call; r8-r11, r15 and r24-r31 are saved by the callee.  Arguments go in r0-r5, results
in r0 (r0:r1).  The split of r16-r31 is the draft of `docs/abi.md`: r24, r25, r26 are among the 16 hot registers of the 4-bit fields.
(A callee-saved register above r15 makes `pushm`/`popm` masks longer than 16 bits: a mask of r8-r15 is the one-parcel `pushm`/`popmret`, up
to r23 two parcels, beyond that three; `popmret` takes r8-r15 only.)

The encodings give one-parcel forms to the hot registers (3-bit: `r0 r1 r8 r2 sp r3 r9 r10`; 4-bit in pdp64/32 also `r11 r4 r15 r5 r24 r25 r26 r6`).
`PICKCOLOR` (`pdp64_pick` in `local2.c`) takes the hot registers first among the free ones (`-m hot=0` is the order of the register file; `-m hot=2` the file order
with the caller-saved first); a move that asks for a register (coalescing) still wins.  Measured: the order changes the corpus by at most
0.03%, because r0-r3 and r8-r10 are the registers the old order used first anyway.

## The `-x` flags of `ccom`

`ccom` flags found in `cc/ccom/main.c` and what they are on this back end.  The new targets turn on the first five by default; `-xno-NAME` turns one off
(`-m pdp64/16 -xno-dmr`).  The baseline `pdp64/r20` keeps taking its flags from the command line.

| flag | what it is | status on pdp64 |
|---|---|---|
| `-xdmr` | "Dennis Ritchie" size optimizations: no frame pointer when nothing needs it, `register` variables in registers, `x op= e` addressed once, and the text peephole `peep.h` (which keeps the file's assembly in memory and rewrites it) | on; **the peephole and everything below that lives in it need it** |
| `-xtemps` | automatic scalars become temporaries (registers) in pass 2; the flow graph, `cseblock`, `fwdtemps`, spill sharing | on |
| `-xdeljumps` | `deljumps`: jumps to jumps, dead code, label cleanup, loop rotation (`looprot`), cross-jumping and tail merging | on |
| `-xinline` | size-limited inlining of static functions | on |
| `-xdce` | dead code elimination and the small idioms (constant `memcpy` in place, `const` folding, rotates and byte swaps recognized, zeroing loops, one-site static functions) | on |
| `-xssa` | SSA form (placement of phi functions, renaming), `removephi` | correct (all tests and the corpus), but the corpus is 1.1% **bigger**: off by default |
| `-xscp` | simple constant propagation after SSA | needs `-xssa`; fails on two corpus files (`bad xasm node type`); off |
| `-xccp`, `-xtailcall` | parsed, never read by any pass | no effect |
| `-xgnu89`, `-xgnu99`, `-xuchar` | language dialect, `char` unsigned | not used by the project |
| `-xc2`, `-xllib`, `-xllall`, `-xnoframeless` | pdp11 options | no effect here |
| `-xabs` | absolute addresses (amd64) | no effect here |
| `-xnoregvars` | do not pin `register` variables | used by the compiler itself on retry |

There is no flag named for Johnson: his pcc is what remains when these are off (Sethi-Ullman ordering `-Zu`, the table matcher, graph-coloring
allocation); `-xno-dmr -xno-temps -xno-deljumps -xno-inline -xno-dce` is that compiler.  The tail call of the peephole (`call f ; ret` to `jmp f`)
and the cbCC fusion are part of `-xdmr`.

## Target options (`-m`)

`-m no-NAME` switches off an optimization of the new targets (they exist for the ablation table); `-m hot=N`, `-m swleaf=N` set a parameter.

| option | what |
|---|---|
| `no-fuse` or `no-movcall`, `no-popmret`, `no-popret`, `no-mov2` | the four fused instructions |
| `no-arrange` | moving a register move down to its call |
| `no-avail` | values known in a basic block: redundant `clr`/`movq #k`/`lea sym`/loads, stores forwarded to loads |
| `no-thread` | jump threading of a condition made into a value |
| `no-leafold` | `movq a,b ; add #k,b` to `lea k(a),b` |
| `no-switch` | switch dispatch by halves |

## What the compiler does for the new encodings

All of it is in `arch/pdp64` and only for `pdp64/16` and `pdp64/32` (the baseline's output is byte-identical to the compiler before).
Numbers are the 841-file corpus compiled for pdp64/16 and assembled by `as64 -m pdp64/16`; the ablation table below gives each switched off alone.

1. **Fusion** (`pp_fuse` in `peep.h`; the assembler never fuses): `movq a,b ; call f` as `movcall a,b,f`; `movq a,b ; movq c,d` as `mov2` when all four are in the 3-bit hot table;
   `pop r ; ret` as `popret r`; `popm #m ; ret` as `popmret #m` when m is a mask of r8-r15.  A run of moves before a call is matched by dynamic programming
   (each fused pair is two bytes, so the most pairs win).  The compiler also arranges for pairs: a register move earlier in the block, past instructions that
   neither read its destination nor write its source or destination, is moved next to the call (`no-arrange`).
2. **A call reads only the argument registers it passes.**  `lastcall` counts the registers of the argument tree and `zzzcode G` writes `; args:N` before the call; the peephole's liveness
   took every call to read r0-r5, which kept a dead copy into an argument register alive before every call without arguments of that kind.
3. **Values known in a basic block** (`pp_avail`): a register that already holds the constant, the address of the symbol, or the loaded value
   makes the instruction redundant, or a register move (a `lea` of a symbol is six bytes); a store followed by a load of the same place (the same address text,
   nothing stored or called in between, same width) takes the stored register (`movl r,M ; movl M,s` is `movl r,s`).  A write to a register, a store, a call or a label
   that something refers to ends what depends on it; a function with a volatile access (`; vol`, written by `eoftn`) keeps all its loads.
4. **Jump threading** of a condition made into a value and tested (`x = a < b; if (x)`, the joins of `&&` `||` `?:`): the way in whose outcome is known jumps
   to the target, the test and the constant go (`pp_thread`).
5. **`pushm`/`popm` understood** by the liveness model.  They were barriers, which made every register live before a function's epilogue
   and so kept dead instructions alive.
6. **`lea` for `movq a,b ; add #k,b`** when the codes are dead.
7. **Switch dispatch by halves** (`mygenswitch`): cgram.y's switch is a chain of compares, n for the last case; with runs of six or fewer compared in turn after a
   search by halves, an interpreter's opcode switch (lua's `luaV_execute`, 83 cases) costs a few compares.  No jump table: it costs eight bytes a case.
8. **The conversion of a `TEMP`** to the unsigned type of the same width is made (`local.c`): `char c = -3; return (unsigned char)c;` gave -3 with `-xtemps` because the
   register was retyped as if it were memory.  This bug is in the baseline too; `pdp64/r20` keeps its output, so it keeps the bug.
9. **Hot registers**, see above.

## Left out, and why

* **Linker relaxation of calls and addresses.**  An external `call` is six bytes and `lea sym,r` of a data symbol six: 15,000 and 12,800 times in the corpus, 17% of its bytes.  The
  encoding has the four-byte forms (a `call` reaches +-2 MB); the assembler cannot know the distance.  The next step is `ld64`.
* **A jump table for dense switches** (eight bytes an entry against four for a compare).
* **Loop-invariant motion** of `lea sym`/constants out of loops, **strength reduction**, **live-range splitting**: not done.  `-xssa` is the nearest thing there is and it makes the code larger.
* **Load CSE across blocks** and **store elimination**: only inside a basic block (3.).

## Tests

`arch/pdp64/test/pdp64test.sh [-m target] [ccom flags] x.c` compiles, assembles (`as64 -m`), links, runs under `sim64` and prints what `int test()` returns;
`pdp64test-all.sh -m TARGET` runs the project's tests (`a64tests` of the v7unix-toolchain, `tests/c` of the pdp64 repository) and `test/c` (new targets) under the flag sets
of the original harness (r20) or the defaults, each optimization off in turn and `-xssa` (new targets).  `PDP64CCOM`, `PDP64TOOLS` (the directory of `as64 ld64 sim64`).

## Measurements

The 841-file corpus of `tools/pdp64size.py` (preprocessed once; sources in `~/claude/corpora`), compiled by `ccom`, assembled by the phase-1 `as64`, bytes of instructions
(`as64 --stats`; for `pdp64/r20` the length walk of `pdp64size.py`).  One file, `gcc.c-torture/execute/memcpy-a2.c`, is 27% of the total (700 tiny functions of byte-compare loops);
the second column leaves it out.

| | bytes | without memcpy-a2 |
|---|---:|---:|
| pdp64/r20, this compiler = the old one (output identical, 841 files) | 1,083,870 | |
| the same old code, assembled as pdp64/16 (no fusing) | 1,016,238 | |
| the Python model, A16 (no fusion) / C16 (with the four fused pairs) | 1,013,598 / 979,192 | |
| **pdp64/16, this compiler** | **953,370** | 695,254 |
| pdp64/32, the same code assembled as pdp64/32 (old compiler, 16 registers used) | 1,060,674 | |
| the Python model, C32 | 1,007,172 | |
| **pdp64/32, this compiler** | **987,472** | 726,284 |

Cumulative path for pdp64/16 (each line adds one change; bytes):

| step | bytes |
|---|---:|
| old compiler, assembled as pdp64/16 | 1,016,238 |
| + fusion (movcall 9,721, popmret 2,261, popret 2,601, mov2 4,358) | 978,086 |
| + a call reads only the registers it passes | 965,296 |
| + values known in a block, threading, pushm/popm in the liveness model | 955,682 |
| + arranging moves for movcall, stores forwarded to loads | 951,734 |
| + `lea` for `movq`+`add`, register order, labels nothing refers to | 950,800 |
| + switch dispatch by halves (runs of six) | 953,370 |

Ablation, pdp64/16, each alone off from the default (`ablate.sh`):

| setting | bytes | vs default | instructions | bytes without memcpy-a2 | vs default |
|---|---:|---:|---:|---:|---:|
| default (all on) | 953,370 | | 306,963 | 695,254 | |
| `-xno-dmr` | 1,197,772 | +25.64% | 402,617 | 886,408 | +27.49% |
| `-xno-temps` | 1,591,184 | +66.90% | 548,923 | 1,238,852 | +78.19% |
| `-xno-deljumps` | 1,010,808 | +6.02% | 333,881 | 752,690 | +8.26% |
| `-xno-inline` | 956,812 | +0.36% | 308,025 | 698,696 | +0.50% |
| `-xno-dce` | 1,107,884 | +16.21% | 373,272 | 849,768 | +22.22% |
| plus `-xssa` | 957,568 | +0.44% | 308,154 | 699,452 | +0.60% |
| plus `-xssa -xscp` (839 files) | 958,262 | +0.51% | 308,148 | 700,146 | +0.70% |
| `-m no-fuse` (all four) | 993,044 | +4.16% | 326,655 | 730,832 | +5.12% |
| `-m no-movcall` | 974,822 | +2.25% | 317,574 | 716,706 | +3.09% |
| `-m no-popmret` | 957,892 | +0.47% | 309,224 | 699,776 | +0.65% |
| `-m no-popret` | 958,572 | +0.55% | 309,564 | 696,360 | +0.16% |
| `-m no-mov2` | 958,842 | +0.57% | 309,688 | 700,726 | +0.79% |
| `-m no-arrange` | 956,290 | +0.31% | 308,403 | 698,174 | +0.42% |
| `-m no-avail` | 960,676 | +0.77% | 309,621 | 702,560 | +1.05% |
| `-m no-thread` | 953,986 | +0.06% | 307,224 | 695,870 | +0.09% |
| `-m no-leafold` | 953,598 | +0.02% | 307,172 | 695,482 | +0.03% |
| `-m no-switch` | 950,800 | -0.27% | 306,187 | 692,684 | -0.37% |
| `-m hot=0` / `hot=2` | 953,378 | +0.00% | 306,967 | 695,262 | +0.00% |
| the five `-x` all off | 2,356,392 | +147.16% | 697,512 | 1,868,880 | +168.81% |

pdp64/32: default 987,472 (302,180 instructions); `no-fuse` 1,027,696 (+4.07%); `no-avail` 994,704 (+0.73%); `no-switch` 984,532 (-0.30%); `hot=0` 987,776 (+0.03%); five `-x` off 2,734,152.
Fused pairs in the corpus: pdp64/16 movcall 10,785, popmret 2,261, popret 2,601, mov2 4,045 (19,692 pairs, 39,674 bytes = 4.16%, two bytes a pair); pdp64/32 11,539, 1,614, 2,581, 4,073.
After the pass there is no adjacent `pop r ; ret`, `popm ; ret` or `movq r,r ; call f` left in the pdp64/16 output; 1,334 pairs of adjacent register moves are not fused because a register is not in the 3-bit table.
The popmret of pdp64/32 is fewer because a mask with r24-r31 does not fit it.

The switch dispatch costs 0.27% of the corpus and pays on execution (below).

Six programs (lua, lz4, zlib, bearssl hash, symmetric ciphers, elliptic curves; the stand-in libc of the earlier study) built from the same preprocessed sources with each compiler, run under the phase-2 `sim64`
(`-P`), output compared with the host build: all twelve runs of pdp64/16 and pdp64/32 print the host's output, and so do the six of pdp64/r20.
Static = bytes of instructions of the units; instructions and bytes executed from the profile.

| program | pdp64/r20, old compiler: static / instructions / bytes executed | pdp64/16 | pdp64/32 |
|---|---|---|---|
| lua | 95,874 / 233.25 M / 886.2 M | 83,274 / 148.75 M / 425.3 M | 85,850 / 141.72 M / 436.4 M |
| lz4 | 20,542 / 139.66 M / 398.5 M | 18,280 / 134.04 M / 359.2 M | 18,652 / 125.16 M / 339.6 M |
| zlib | 34,926 / 172.93 M / 491.3 M | 30,480 / 157.36 M / 423.7 M | 31,504 / 142.42 M / 402.1 M |
| bs_hash | 10,456 / 65.30 M / 195.8 M | 9,406 / 63.56 M / 184.3 M | 9,800 / 63.54 M / 187.8 M |
| bs_sym | 14,444 / 73.62 M / 196.0 M | 12,960 / 72.93 M / 183.5 M | 13,974 / 70.31 M / 210.5 M |
| bs_ec | 13,088 / 71.05 M / 197.4 M | 11,632 / 70.60 M / 185.0 M | 12,242 / 68.34 M / 209.4 M |
| total | 189,330 / 755.82 M / 2365.2 M | 166,032 / 647.24 M / 1761.1 M | 172,022 / 611.49 M / 1785.8 M |

pdp64/16 against the baseline: static -12.3%, instructions -14.4%, bytes executed -25.5%; pdp64/32: -9.1%, -19.1%, -24.5%.
lua alone, switch dispatch: 225.1 M instructions and 724.8 M bytes without it, 148.7 M and 425.3 M with it (pdp64/16).
