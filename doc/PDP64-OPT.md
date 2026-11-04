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
| `-xssa` | SSA form (placement of phi functions, renaming), `removephi` | correct since the `pp_avail` fix of this phase (Lua miscompiled with it, below); **slower and bigger on every program, both targets: off by default** (section "`-xssa` against the timing model") |
| `-xscp` | simple constant propagation after SSA | needs `-xssa`; fails on two corpus files (`bad xasm node type`); off |
| `-xccp` | conditional constant propagation (`mip/optim2.c` `ccp`, written in this phase: the flag was parsed and never read) | on for the new targets; off elsewhere unless asked |
| `-xtailcall` | tail calls through a register or memory and from the middle of a function (pdp64 `peep.h`; was parsed and never read) | on for the new targets |
| `-xgnu89`, `-xgnu99`, `-xuchar` | language dialect, `char` unsigned | not used by the project |
| `-xc2`, `-xllib`, `-xllall`, `-xnoframeless` | pdp11 options | no effect here |
| `-xabs` | absolute addresses (amd64) | no effect here |
| `-xnoregvars` | do not pin `register` variables | used by the compiler itself on retry |

There is no flag named for Johnson, and per the author's instruction his optimizations are part of `-xdmr`: see "The `-xdmr` family" below for the split between machine-independent
and back-end specific and the port of the VAX back end's tricks.  Johnson's pcc proper (Sethi-Ullman ordering `-Zu`, the table matcher, graph-coloring allocation) is always on; `-xno-dmr -xno-temps -xno-deljumps -xno-inline -xno-dce`
leaves little more than that.  The tail call of the peephole (`call f ; ret` to `jmp f`) and the cbCC fusion are part of `-xdmr`.  On the new targets every optimization of this document has an `-xno-NAME`
(`-xno-autoinc`, `-xno-sob`, `-xno-rmw`, `-xno-bic`, `-xno-field`, `-xno-setcc`, `-xno-minmax`, `-xno-mullea`, `-xno-divp2`, `-xno-divmagic`, `-xno-jtab`, `-xno-tailcall`, `-xno-ccp`, and the older ones: `-xno-fuse`, `-xno-avail`, `-xno-arrange`, `-xno-thread`, `-xno-leafold`, `-xno-switch`), the same names as `-m no-NAME`.

## Target options (`-m`)

`-m no-NAME` switches off an optimization of the new targets (they exist for the ablation table); `-m hot=N`, `-m swleaf=N`, `-m jtmin=N` set a parameter.  The same names work as `-xno-NAME`.

| option | what |
|---|---|
| `no-fuse` or `no-movcall`, `no-popmret`, `no-popret`, `no-mov2` | the four fused instructions |
| `no-arrange` | moving a register move down to its call |
| `no-avail` | values known in a basic block: redundant `clr`/`movq #k`/`lea sym`/loads, stores forwarded to loads |
| `no-thread` | jump threading of a condition made into a value |
| `no-leafold` | `movq a,b ; add #k,b` to `lea k(a),b` |
| `no-switch` | switch dispatch by halves |
| `no-autoinc`, `no-sob`, `no-rmw`, `no-bic`, `no-field`, `no-setcc`, `no-minmax`, `no-mullea`, `no-divp2`, `no-divmagic`, `no-jtab`, `no-tail` | the passes of this phase (below); `-xno-tailcall` is `no-tail` |
| `hoist` | (off by default) spill reloads out of leaf loops |
| `jtmin=N` | smallest number of cases for a jump table (8) |

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

## The `-xdmr` family: Ritchie's and Johnson's optimizations

The author's rule: the Johnson ("sj", the pcc that was ported to the VAX) optimizations belong with `-xdmr`, which therefore enables both Ritchie's (dmr-cc, c2) and Johnson's.
On `pdp64/16` and `pdp64/32` `-xdmr` is on by default and every optimization below has its own `-xno-NAME` switch (`-xno-autoinc`, `-xno-sob`, ...; the same names as `-m no-NAME`
of the ablation tables; `-xno-dmr` takes the text passes and the code-shape changes away together).  Where an optimization lives decides which back ends have it:

* **MI** (machine-independent: `cc/ccom/*`, `mip/*`): written once; a back end that is built gets it, and it changes the output of a back end only if it is switched on for it.
* **BE** (back-end specific: `arch/NAME`): instruction selection, addressing modes, fused or short forms.  Those of pdp64 are in `arch/pdp64` and only for `pdp64/16` and `pdp64/32`.

### Ritchie (dmr-cc, the PDP-11 C compiler, and c2)

| optimization | where | on pdp64 |
|---|---|---|
| `register` variables are kept in registers (`pftn.c`) | MI | on (`-xdmr`) |
| `x op= e`, `x++` compute the address once; a variable changed in a condition (`++n > 1`); `c ? a : b` as a condition; a value-less `?:` statement; constant statements dropped (`trees.c`, `cgram.y`) | MI | on (`-xdmr`) |
| `postincfold` (`t = x; x = x + k` folded into the statement) | MI | on (`-xdmr`, `-xtemps`) |
| c2's jump optimizer: jumps to jumps, dead code, loop rotation, cross-jumping, tail merging (`deljumps`, `mip/optim2.c`) | MI (2BSD c2's algorithm) | on (`-xdeljumps`) |
| no frame pointer when nothing needs it; frameless leaves (`arch/pdp64/code.c`) | BE | on |
| the text peephole of c2 (`arch/pdp64/peep.h`: dead instructions, copy propagation, codes reuse, `cbCC`, `call;ret` to `jmp`) | BE | on, part of `-xdmr` |
| autoincrement / autodecrement for `*p++`, `*--p` (pdp11 `order.c`, `inctree`) | BE (pdp11, vax; on pdp64 a text pass) | on, `-xno-autoinc` |
| `sob` for `while (--n)`, `do ... while (--n)` | BE (pdp11 c2) | on, `-xno-sob` |
| `inc`, `dec`, `neg`, `com`, `add`... directly on a memory operand | BE | on, `-xno-rmw` |

### Johnson (pcc as ported to the VAX)

The VAX back end of this tree (`arch/vax`: `table.c`, `local.c`, `local2.c`, `order.c`, `code.c`, `macdefs.h`), read as it was before the project's `-xdmr` additions (the table of 33c978a9),
plus what it relies on in `mip/` and `cc/ccom`.  The VAX has autoincrement `(rN)+`, `-(rN)`; `ashl`/`ashq`; `extv`/`extzv`/`insv`; `movc3`/`cmpc3`/`locc`; index mode `base[rI]`; `bitl`; `bicl`; `incl`/`decl`; `casel`; `calls`/`ret`.
(32V's separate peephole `cc2` has `sobgtr`, `aoblss`, `acbl`, `jbs`/`jbc`.)

**Back-end specific, and what pdp64 has:**

| # | VAX trick | pdp64 | status |
|---|---|---|---|
| 1 | autoincrement/decrement operands for `*p++`, `*--p` (table `STARREG`, `order.c`) | `(rN)+`, `-(rN)`, and strided `(rN)+d`, `(rN)-d` | **ported** (text pass `pp_autoinc`, also from a copy of the pointer and for any step): `-xno-autoinc` |
| 2 | `sobgtr`/`aoblss`/`acbl` loop closers | `sob` | **ported**: `dec r; cbne r,#0` and the rotated `while (n--)` (needs n dead afterwards and unused in the body): `-xno-sob` |
| 3 | `extv`/`extzv`/`insv` for bit fields and `(x << a) >> b` | `extb`, `extzb`, `insb` | **ported**: shift pairs (with the 32-bit extension between), shift-and-mask, mask-shift-or: `-xno-field` |
| 4 | `ashl` for both directions | `shl`, `shr`, `sar` | n/a |
| 5 | `movc3` for struct assignment and constant `memcpy`; `cmpc3`, `locc`, `skpc` | `mcpy`, `mmov`, `mset`, `mcmp`, `mscn` | struct assignment is `mcpy` already; constant `memcpy` in place is MI (`-xdce`); loops to `mset`/`mscn`: not done |
| 6 | index mode `name[rI]`, `base(rB)[rI]` scaled by the operand size | `d(Rb)(Rx*s)` | already (`adrform` in `order.c`) |
| 7 | `movaq`/`moval` with an index for `&a[i]` | `lea d(Rb)(Rx*s)` | already; `mul #5`, `#9` and `3x+k` are `lea` too: `-xno-mullea` |
| 8 | codes of the previous instruction reused instead of `tstl` | codes written by ALU ops, not by moves | already (peephole) |
| 9 | `bitl`/`bitw`/`bitb` for `(x & m) == 0`; 32V's `jbs`/`jbc` | `bit`, `and` | `and #m; beq` is the same two instructions; `bit` where the value is needed again: not done |
| 10 | `bicl` for `x & ~y` (`AND` made `~R & L` in `optim2`) | `bic` | **ported**: `-xno-bic` |
| 11 | `incl`, `decl`, `mnegl`, `mcoml` | `inc`, `dec`, `neg`, `com` | already |
| 12 | `incl`/`addl2`... on memory (`memincdec`) | `inc M`, `addm`, `subm`, `bicm`, `bism` | **ported**: `-xno-rmw` |
| 13 | `cvtbl`/`movzbl` loads that widen | `movb`/`movzb` | already |
| 14 | `casel` jump table for a dense `switch` (`code.c`) | `jmpi (Rt)(Rx*8)` through a table of 8-byte addresses | **ported** (table after the function's `ret`, a bounds compare first): `-xno-jtab`, `-m jtmin=N` |
| 15 | `calls`/`ret` with the register mask; arguments on the stack; `parmprop` (parameters read in place) | `call`/`ret`, arguments in r0-r5, `pushm`/`popm`/`popmret`, `movcall` | different ABI; the fused instructions of phase 3 are the analogs |
| 16 | `tstl` against zero for compares | `tst`, `cbCC rd,#0` | already |
| 17 | rotates and byte swaps (`rotl`, `insv`) | `rori`, `bswap` | already (phase 3, `-xdce`) |
| 18 | `ediv`/`emul` | `mulh`, `div` | divisions by constants: shifts and a multiply (below) |

Not on the VAX, added in the same spirit: `set cc` for a comparison used as a value, `min`/`max`/`abs`, signed division by powers of two and by constants as shifts and multiplies, pointer difference as a shift, `lea` for small multipliers.

**Machine-independent Johnson (always on, no flag), checked on pdp64 and left alone:** the table-driven matcher with costs (`mip/match.c`); Sethi-Ullman ordering (`-Zu`, `mip/order.c`);
`OREG` formation, constant offsets into displacements (`mip/reader.c` `oregok`/`ormake` and the back end's `offstar`); constant folding, `x * 2^k` to a shift, unsigned `x / 2^k`, `x % 2^k` to shift and mask
(`cc/ccom/optim.c`, `trees.c`); canonicalization (constants to the right); graph-colouring allocation with coalescing (`mip/regs.c`); `char` compares without widening.
They are not under any `-x` flag, so nothing to enable; the one gap found is that Johnson's `optim.c` does the unsigned powers of two and leaves signed division to the hardware, which is what the `divp2` pass of the new targets (BE) fills.
Machine-independent changes made in this phase: `ccp` (`mip/optim2.c`, off unless `-xccp`; compiled and run on amd64 with `-xccp` over seven of the test programs, built (not run, no simulator) for vax), the `iscasel` test accepts the pdp64 table jump.
The default output of the other back ends is unchanged (their flags never set `xccp`).

## `-xccp` and `-xtailcall`: what they were, and what they are now

Both flags were found in `cc/ccom/main.c` (and `cxxcom`) as options that set `xccp` / `xtailcall` and are **read by nothing**: `git log -S` shows `5ca51a27 "Add opt flag xccp (conditional constant propagation)"` and
`a3ee00b9 "Add -xtailcall"`, each only the flag and its `extern`; the only constant propagation in `mip/optim2.c` is `simple_cp` (`-xscp`, after SSA), whose comment says "should be replaced with conditional constant propagation someday".
So the answer to "why no effect on pdp64" is that no back end and no pass had ever implemented them.  Now:

* **`-xccp`** (MI, `mip/optim2.c`, `ccp()`): conditional constant propagation (Wegman and Zadeck's idea without SSA) on the temporaries.  A lattice per temporary (unseen, constant, varying) at the start of each flow-graph block;
  a block is visited only when an executable edge reaches it, a `CBRANCH` with a constant condition makes only one edge executable; then each use of a constant temporary becomes the constant, the trees are folded
  (integers and plain-number pointers, every width), a constant `CBRANCH` becomes a `GOTO` or goes away, and `deljumps` removes what is left.  Left alone: a function that calls `setjmp`/`vfork`, has a computed goto, a switch dispatch or a jump table;
  trees that assign inside an expression or contain inline assembler are not looked into.  It is off unless asked for (`-xccp`) on every other back end (checked: amd64 compiled with `-xccp` and the test programs run; vax compiled, not run); on for `pdp64/16` and `/32`.
  Effect on the corpus (ablation table): 1,908 bytes (0.2%) and 788 instructions; on the six programs the instructions executed change by less than 0.1%.
  The pass finds little because `-xtemps -xdce -xinline` and pass 1's folding already did most of it: what is left is constants through joins and loops after inlining.
* **`-xtailcall`** (BE for pdp64): the peephole already turned `call f ; ret` into `jmp f` (part of `-xdmr`, independent of the flag).  The flag now covers the cases it missed: `call (rN) ; ret` to `jmp (rN)` and `calli M ; ret` to `jmpi M`
  (indirect calls: function pointers, method tables, PLT-free external calls; the register or the address must survive the epilogue's `pop`s: caller-saved or no epilogue, and nothing based on sp or fp), and a `call` followed by a `ret` in the
  middle of a function (the early `return f(x)` of an `if`), which the old test refused because only the final `ret` knew the size of the outgoing-argument area.  Not tail calls, correctly: a call with arguments on the stack
  (the frame holds them), a function in which the address of a frame slot is taken (`lea k(sp)`, a copy of sp or fp: the frame goes away before the jump).  The fused `popmret` does not interact: the epilogue's `popm` stays and the `ret` is replaced
  by the `jmp` (`popm` plus `jmp` is two instructions where `popmret` was one, but the call that would have been executed first is gone).  `-xno-tailcall` switches the extension off.  Corpus effect: 268 bytes (ablation table).


### The passes added in this phase (new targets; each `-xno-NAME` / `-m no-NAME`)

All of them are passes of `peep.h` over the text of a function after its other rewrites (`pp_modes`, before the fusions), except `divp2`, `divmagic` and `jtab`, which rewrite trees (`local2.c`, `code.c`).
Counts are over the 841-file corpus, pdp64/16, each alone switched off from the default (the ablation table below).

* **`autoinc`** (`pp_autoinc`; Johnson's STARREG, Ritchie's `inctree`): `movb (r1),r2 ; inc r1` is `movb (r1)+,r2`; `dec r1 ; movb (r1),r2` is `movb -(r1),r2`; a copy of the pointer made for the old value
  (`movq r1,r3 ; inc r1 ; movb (r3),r3`, which is how the compiler writes `*s++`) is folded the same way; the stride is the operand size by default and any step up to 2000 otherwise (`(r1)+16`, `(r1)-24`).
  The pair is two bytes plus two or four, the one instruction four: no larger, one instruction (and one issue slot) less per element.
* **`sob`**: `dec r ; cbne r,#0,L` becomes `sob r,L` (six bytes to four); `while (n--)`, as the rotated loop `br T ; B: body ; T: movq n,c ; dec n ; cbne c,#0,B`, becomes `cbeq n,#0,X ; B: body ; T: sob n,B ; X:` when the body does not
  use n, n and the copy are dead after the loop and nothing else enters the loop.
* **`rmw`**: `movq M,r ; inc r ; movq r,M` is `inc M`; with `add`, `sub`, `bic`, `bis` of a register it is `addm`, `subm`, `bicm`, `bism` (six bytes to four, three instructions to one; not in a function with a volatile access;
  the indexed mode is left out of `addm` because the ISA text says `MESC` has no room for it).  64-bit values only.
* **`bic`**: `com y ; and y,x` (y dead) is `bic y,x`.
* **`field`**: `shl #a,r ; [movzl r,r] ; shr #b,r` (or `movl` and `sar`) is `extzb`/`extb r,#b-a,#w,r`; `[movzl] ; shr #p,r ; and #2^w-1,r` is `extzb r,#p,#w,r`; `and #~(m<<p),d ; [and #m,s] ; [shl #p,s] ; bis s,d` is `insb s,#p,#w,d`.
  Taken only when the old sequence was at least as long as the six bytes of the new instruction.
* **`setcc`**: `cbX a,b,L ; movq #1,r ; br J|ret ; L: clr r ; J:` (a comparison used as a value) is `cmp a,b ; set cc,r [; ret]`: ten bytes to six, and no branch to mispredict.
* **`minmax`**: `cbX a,b,L ; movq a,b ; L:` is `max`/`min`/`maxu`/`minu a,b,b` and `cbge a,#0,L ; neg a ; L:` is `abs a,a`.
* **`mullea`**: `mul #5|9,r` is `lea (r)(r*4|8),r`, and with a following `add #k,r`, `mul #3` too (`lea k(r)(r*2),r`): the same four bytes, one cycle of latency instead of three.
* **`divp2`** (tree rewrite): signed `x / 2^k` is `(x + ((x >> 63) >>> (64-k))) >> k` and `x % 2^k` is `x - ((x/2^k) << k)`, the difference of two pointers is a shift (exact: no rounding needed); a dividend that is a pure expression of temporaries is first put in a new temporary.
  Larger by two to six bytes; a `div` is 20 cycles in the model and the shifts are 4 or 5.
* **`divmagic`** (tree rewrite): signed `int` `x / c` and `x % c` for a constant that is not a power of two: a 64-bit multiply by `m = floor(2^k/c) + 1` (`k = 31 + ceil(log2 c)`), an arithmetic shift, and the sign bit added; checked for every `c` up to 3000 and
  the corners of the range.  Larger by six to ten bytes; three cycles of multiply and two of shift and add instead of 20.  (The 64-bit and unsigned cases need `mulh`/`mulhu`, which the tree has no operator for: not done.)
* **`jtab`** (`code.c` `jumptable`): a switch of at least `-m jtmin=N` (8) cases that fill at least half of their range is a bounds compare (one unsigned compare against the range, to the default) and `lea table,r12 ; jmpi (r12)(rI*8)` through
  a table of eight-byte addresses written after the function's `ret` (so the function stays one region for the peephole); not in a function saved for inlining (its labels are renumbered when it is copied), not with `-xssa` (`removephi` does not know a computed jump).
  The `casel` of the VAX with word offsets would need a relocation the assembler does not have.  The 83-case switch of lua's interpreter was a search by halves (phase 3).
* **`-xtailcall`, `-xccp`**: above.
* **`-m hoist`** (off by default; `pp_hoist`): the load of a frame slot that nothing in a call-free loop changes is made once before the loop, into a caller-saved register the loop does not name (live-range splitting by hand: the allocator has none).  Correct on the tests (`test/c/hoist.c`;
  an early version miscompiled `-xno-temps` code because its own rewrite also renamed the new load, found by the harness), but it finds almost nothing: over the corpus it changes a few loops and the result is 44 bytes and 21 instructions *more*, because the in-loop load becomes a register move that
  nothing removes (the region optimizer cannot be run again after it: its renaming mishandled the new shape), and the hot loops that reload from the frame (zlib's `longest_match`, lz4's compressor) have every register named in the loop, which is why they spilled.  Kept as an option, off.
* **Scaled index through a conversion** (`order.c`, `adrscan`): `x[u + 1]` with an `unsigned long` (or `unsigned long long`) index `u` is `movl 4(r0)(r1*4),r0`; the conversion between `long` and `unsigned long` that stood between the shift and the sum hid the pattern, and the index was
  shifted and added by hand (`inc ; shl #2 ; movl (r0)(r1)`).  bs_ec's multiply-accumulate loop, half of its instructions, was written that way.  Not an option name of its own (it is the addressing selection).

## Left out, and why

* **Linker relaxation of calls and addresses.**  An external `call` is six bytes and `lea sym,r` of a data symbol six: 15,000 and 12,800 times in the corpus, 17% of its bytes.  The
  encoding has the four-byte forms (a `call` reaches +-2 MB); the assembler cannot know the distance.  The next step is `ld64`.
* **Loop-invariant motion** of `lea sym`/constants out of loops, **strength reduction** of induction variables, **real live-range splitting**: not done.  The allocator has no splitting and the register
  pressure is the limit in the hot loops (zlib's `longest_match`, lz4's compressor: every caller- and callee-saved register is named in the loop), so hoisting needs a spare register that is not there; `-m hoist`
  (above) is the one case that was cheap.  `-xssa` is the nearest thing there is and it loses (section below).
* **Division by a constant for 64-bit and unsigned operands** (needs `mulh`/`mulhu` as tree operators), **loops to `mset`/`mscn`**, **the unaligned `memcpy` idiom as `movuw`/`movul`/`movuq`** (lz4's `LZ4_read16/32`
  copies bytes through the stack and executes 9 instructions per read: 10% of lz4; the unaligned-load hook `UNALIGNED_LOADS_OK` is for targets that load at any address, and pdp64 aborts on a misaligned access): not done.
* **Load CSE across blocks** and **store elimination**: only inside a basic block (3.).

## Tests

`arch/pdp64/test/pdp64test.sh [-m target] [ccom flags] x.c` compiles, assembles (`as64 -m`), links, runs under `sim64` and prints what `int test()` returns;
`pdp64test-all.sh -m TARGET` runs the project's tests (`a64tests` of the v7unix-toolchain, `tests/c` of the pdp64 repository) and `test/c` (new targets) under the flag sets
of the original harness (r20) or the defaults, a few optimizations off in turn, `-xno-tailcall`, `-xccp` and `-xssa` (new targets).  The tests of this phase are in `test/c`: `tailcall`, `ccp`, `autoinc`, `modes`, `fields`, `setcc`, `minmax`, `mullea`, `divp2`, `divmagic`, `jtab`, `hoist`, `index`, `avail_chase`.  `PDP64CCOM`, `PDP64TOOLS` (the directory of `as64 ld64 sim64`).

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

## Phase 4 measurements

Compiler: `pcc-enc` branch `enc-prefix` (this document's commit); `as64`, `ld64`, `sim64` of `pdp64-enc` (phase 2), timing model `sim64 --timing` of `pdp64-enc-tm` (branch `enc-tm`).  Scripts and logs: the session's scratchpad `p4/`.

### The six programs

Same programs and method as above (static bytes of instructions of the units / instructions executed / bytes executed; output compared with the host build; all 18 runs print the host's output).

| program | pdp64/r20, old compiler | pdp64/16 | pdp64/32 |
|---|---|---|---|
| lua | 95,874 / 233.25 M / 886.2 M | 82,004 / 137.68 M / 386.5 M | 84,264 / 128.35 M / 390.7 M |
| lz4 | 20,542 / 139.66 M / 398.5 M | 18,064 / 133.18 M / 357.9 M | 18,398 / 124.31 M / 337.9 M |
| zlib | 34,926 / 172.93 M / 491.3 M | 29,642 / 155.85 M / 422.2 M | 30,670 / 140.91 M / 400.6 M |
| bs_hash | 10,456 / 65.30 M / 195.8 M | 9,262 / 63.53 M / 184.3 M | 9,606 / 63.51 M / 187.8 M |
| bs_sym | 14,444 / 73.62 M / 196.0 M | 12,766 / 72.04 M / 182.2 M | 13,742 / 69.42 M / 208.9 M |
| bs_ec | 13,088 / 71.05 M / 197.4 M | 11,056 / 47.53 M / 140.3 M | 11,630 / 46.42 M / 165.4 M |
| total | 189,330 / 755.82 M / 2365.2 M | 162,794 / 609.81 M / 1673.4 M | 168,310 / 572.92 M / 1691.2 M |

Against pdp64/r20: pdp64/16 static -14.0%, instructions -19.3%, bytes executed -29.2%; pdp64/32 -11.1%, -24.2%, -28.5%.  (Phase 3: 166,032 / 647.24 M / 1761.1 M and 172,022 / 611.49 M / 1785.8 M.)

Dynamic ablation, pdp64/16, the six programs (each option off alone; the default is the final build: 162,794 / 609.81 M / 1673.4 M):

| setting | static bytes | instructions | bytes executed | instructions vs default | bytes executed vs default |
|---|---:|---:|---:|---:|---:|
| `-xno-autoinc` | 164,230 | 639.16 M | 1725.2 M | +4.81% | +3.09% |
| `-xno-sob` | 162,808 | 610.60 M | 1674.2 M | +0.13% | +0.05% |
| `-xno-rmw` | 162,916 | 610.40 M | 1674.2 M | +0.10% | +0.05% |
| `-xno-field` | 162,898 | 611.57 M | 1675.1 M | +0.29% | +0.10% |
| `-xno-setcc` | 162,982 | 610.23 M | 1674.3 M | +0.07% | +0.05% |
| `-xno-minmax` | 162,832 | 609.82 M | 1673.5 M | +0.00% | +0.00% |
| `-xno-jtab` | 163,900 | 614.08 M | 1703.9 M | +0.70% | +1.82% |
| `-xno-divmagic` | 162,760 | 609.81 M | 1673.4 M | -0.00% | -0.00% |

### Static ablation, pdp64/16, 841 files (bytes of instructions by `as64 --stats`)

| setting | bytes | vs default | instructions | vs default |
|---|---:|---:|---:|---:|
| default (all on) | 943,506 |  | 301,885 |  |
| `-xno-autoinc` | 944,632 | +0.12% | 302,993 | +0.37% |
| `-xno-rmw` | 944,078 | +0.06% | 302,555 | +0.22% |
| `-xno-bic` | 943,652 | +0.02% | 301,958 | +0.02% |
| `-xno-sob` | 943,558 | +0.01% | 301,951 | +0.02% |
| `-xno-field` | 943,680 | +0.02% | 302,386 | +0.17% |
| `-xno-setcc` | 944,462 | +0.10% | 302,558 | +0.22% |
| `-xno-minmax` | 943,602 | +0.01% | 301,933 | +0.02% |
| `-xno-mullea` | 943,506 | +0.00% | 301,885 | +0.00% |
| `-xno-divp2` | 943,408 | -0.01% | 301,812 | -0.02% |
| `-xno-divmagic` | 943,358 | -0.02% | 301,834 | -0.02% |
| `-xno-jtab` | 948,226 | +0.50% | 302,991 | +0.37% |
| `-xno-tailcall` | 943,774 | +0.03% | 302,018 | +0.04% |
| `-xno-ccp` | 945,414 | +0.20% | 302,673 | +0.26% |
| `-m hoist` | 943,550 | +0.00% | 301,906 | +0.01% |
| `-xssa` | 953,312 | +1.04% | 304,477 | +0.86% |

This table is of the build at `fd0b3d6a` (before the unsigned-index change, which makes the default smaller still: below).  Phase 3's default was 953,370 bytes.

Final pdp64/16 corpus: 841 of 841 files compile and assemble, 941,166 bytes, 300,823 instructions.

Final pdp64/32 corpus: 841 of 841 files compile and assemble, 973,436 bytes, 296,072 instructions.


### `-xssa` against the timing model

Before the benchmark: **`-xssa` miscompiled Lua** (reported by the timing agent: an unmapped access at `0x30` in `luaL_checkversion_`).  Bisected by recompiling the Lua units one by one with `-xssa` into the working build:
only `lapi.c`; with `-xno-dmr` or `-m no-avail` it ran; the cause was in `pp_avail` (phase 3, block-local value availability), not in SSA: after a register copy `movq r1,r0` the availability key of r0 still read "the load from (r0)", although r0 had just been
overwritten, so the next `movq (r0),r0` (the second step of `*(*ci)`, in `index2value`) was deleted as redundant.  The SSA code shape (copies instead of `lea`) is what exposed it; in principle any code could (`test/c/avail_chase.c`
is the pointer chase that goes with it; the old compiler happens to pass it).  Fixed in `pp_avail` (a key that names the destination register as base is dropped when the destination is written by a copy).  With the fix all six programs run correctly with `-xssa` on both targets.


Cycles with `-xssa` against without, pdp64/16, all six programs (positive = `-xssa` slower), per model (the models are the timing model's: issue width x fetch bytes per cycle, in-order, load 3 cycles, multiply 3, divide 20, bimodal predictor, BTB 64; `i2c` adds 16 KB caches):

| program | instructions executed (default -> `-xssa`) | 1-issue, fetch 4 B/cycle | 1-issue, fetch 16 | 2-issue, fetch 8 | 2-issue, fetch 16 | 4-issue, fetch 16 | 4-issue, fetch 64 | 2-issue, fetch 16, 16 KB I- and D-caches |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| lua | 137.9 M -> 148.6 M | +3.91% | +4.03% | +3.55% | +3.78% | +2.63% | +2.39% | +3.56% |
| lz4 | 133.2 M -> 141.5 M | +2.35% | +2.91% | +4.50% | +4.45% | +5.88% | +5.87% | +3.42% |
| zlib | 155.9 M -> 165.1 M | +4.06% | +4.25% | +4.43% | +4.38% | +4.33% | +4.33% | +2.23% |
| bs_hash | 63.5 M -> 64.7 M | +1.36% | +1.36% | +1.35% | +1.34% | +0.31% | +0.30% | +1.34% |
| bs_sym | 72.3 M -> 73.5 M | +1.64% | +1.64% | +1.90% | +1.92% | +1.32% | +1.31% | +1.91% |
| bs_ec | 68.9 M -> 71.4 M | +5.08% | +5.08% | +5.87% | +5.87% | +5.97% | +5.96% | +5.87% |
| total | | +3.24% | +3.44% | +3.84% | +3.87% | +3.75% | +3.69% | +2.90% |

Share of the cycles spent waiting for instructions (fetch-wait, `fe`) and waiting for operands or units (execute-wait, `ex`), default build, per model (%):

| program | i1w4 | i1w16 | i2w8 | i2w16 | i4w16 | i4w64 | i2c |
|---|---|---|---|---|---|---|---|
| lua | 8 / 24 | 7 / 24 | 8 / 29 | 8 / 29 | 8 / 31 | 8 / 31 | 8 / 33 |
| lz4 | 4 / 26 | 3 / 26 | 2 / 34 | 2 / 34 | 2 / 35 | 2 / 35 | 2 / 43 |
| zlib | 2 / 30 | 1 / 30 | 2 / 39 | 2 / 39 | 2 / 43 | 2 / 43 | 1 / 68 |
| bs_hash | 0 / 23 | 0 / 23 | 0 / 30 | 0 / 30 | 0 / 32 | 0 / 32 | 0 / 30 |
| bs_sym | 0 / 27 | 0 / 27 | 0 / 32 | 0 / 32 | 0 / 34 | 0 / 34 | 0 / 33 |
| bs_ec | 2 / 21 | 2 / 21 | 3 / 26 | 3 / 26 | 3 / 28 | 3 / 28 | 3 / 26 |

Cycles with `-xssa` against without, pdp64/32, three programs (lz4, bs_hash, bs_ec; the others were not run for lack of time) (positive = `-xssa` slower), per model (the models are the timing model's: issue width x fetch bytes per cycle, in-order, load 3 cycles, multiply 3, divide 20, bimodal predictor, BTB 64; `i2c` adds 16 KB caches):

| program | instructions executed (default -> `-xssa`) | 1-issue, fetch 4 B/cycle | 1-issue, fetch 16 | 2-issue, fetch 8 | 2-issue, fetch 16 | 4-issue, fetch 16 | 4-issue, fetch 64 | 2-issue, fetch 16, 16 KB I- and D-caches |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| lz4 | 124.3 M -> 130.0 M | +5.42% | +5.49% | +6.04% | +5.79% | +4.99% | +5.21% | +4.98% |
| bs_hash | 63.5 M -> 64.3 M | +1.00% | +0.99% | +1.21% | +1.22% | +0.10% | +0.09% | +1.25% |
| bs_ec | 67.0 M -> 69.3 M | +3.91% | +3.71% | +6.66% | +6.17% | +5.69% | +5.98% | +6.17% |
| total | | +3.97% | +3.95% | +5.07% | +4.82% | +4.03% | +4.21% | +4.46% |

Share of the cycles spent waiting for instructions (fetch-wait, `fe`) and waiting for operands or units (execute-wait, `ex`), default build, per model (%):

| program | i1w4 | i1w16 | i2w8 | i2w16 | i4w16 | i4w64 | i2c |
|---|---|---|---|---|---|---|---|
| lz4 | 2 / 22 | 2 / 22 | 2 / 30 | 2 / 30 | 2 / 31 | 2 / 31 | 1 / 41 |
| bs_hash | 0 / 23 | 0 / 23 | 0 / 30 | 0 / 30 | 0 / 31 | 0 / 31 | 0 / 30 |
| bs_ec | 2 / 19 | 2 / 19 | 3 / 24 | 3 / 24 | 3 / 26 | 3 / 26 | 3 / 24 |

Reading, in the author's terms (the decision rule: more bytes for fewer instructions executed is worth it if the loop is execute-bound, not if it is fetch-bound):

* The fetch/execute gap.  At 4 bytes of fetch per cycle and issue width 1 the instructions of these programs average 2.7 bytes, so a window holds 1.5 instructions and the front end is just ahead of a scalar back end;
  fetch-wait is 0-8% of the cycles (lua 6-8%, which has the most taken branches; zlib, bs_* 0-2%) and is nearly gone from 8 bytes on.  Execute-wait is 21-43% (68% for zlib with caches on): load-to-use (a load is 3 cycles, and a compare or an address follows it), the multiplier,
  the divider.  Widening the fetch from 4 to 16 bytes a cycle at one issue slot changes the cycles by -0.3% (0.0 to -0.7% per program), from 16 to 64 at four slots by -0.03%; going from one issue slot to two takes 16% off (13 to 22%), from two to four 2.9% more.
  (The timing agent's sweep, which includes 2 bytes a cycle, finds the knee there: its totals for the six programs on /16 are 1078 M cycles at 2 bytes, 968 M at 4, 964 M at 16: below 4 bytes a cycle the programs are fetch-bound, from 4 up they are not.)
  So a few bytes more in an instruction cost nothing in these programs, and an instruction fewer in the loop saves a cycle only if it was on the critical path or used an issue slot that mattered.
* **`-xssa` does not pay.**  Where phi functions are taken out it leaves copies (`movq r3,r6 ; inc r6` where the allocator had `lea 1(r3),r6`; bs_ec's multiply-accumulate loop, 51% of its instructions, shows exactly that), so it executes **more** instructions
  (+5.2% on pdp64/16 over the six programs, +4.7% on /32) and more bytes (+6.1%, +7.7%), is 2.0% larger statically, and costs 3-4% in cycles on every model, 0.3% to 6.0% per program, with no program and no model where it wins (the closest is
  bs_hash at +0.3% with 4-issue and wide fetch).  Both targets: **off by default**.  The size-for-speed bargain does not exist here: the code is larger and slower.
* The passes of this phase that trade size for speed (`divp2`, `divmagic`, `jtab`) were measured on micro-benchmarks (below) and, for `jtab`, on Lua.


### Micro-benchmarks (pdp64/16, default build against the option off)

A loop of 200,000 to 300,000 iterations of the idiom; cycles on three models (issue width x fetch bytes: 1x8, 2x16, 4x32), the option on against off.  Instructions are on the 1x8 model's run.

| idiom | option off | instructions on / off | bytes executed on / off | cycles 1x8 | cycles 2x16 | cycles 4x32 |
|---|---|---|---|---|---|---|
| switch | `-xno-jtab` | 5,100,011 / 4,800,011 | 15,000,030 / 14,700,030 | +0.4% | -4.5% | -4.0% |
| div10 | `-xno-divmagic` | 4,599,986 / 2,799,995 | 13,199,960 / 7,599,988 | -30.0% | -37.5% | -37.5% |
| div8 | `-xno-divp2` | 4,000,010 / 1,800,010 | 10,000,028 / 4,800,028 | -53.2% | -64.4% | -66.7% |
| ptrdiff | `-xno-divp2` | 5,700,011 / 5,700,011 | 16,800,030 / 17,400,030 | -32.4% | -56.2% | -58.1% |
| cpy | `-xno-autoinc` | 1,236,107 / 2,874,507 | 4,943,618 / 7,401,218 | -44.3% | -33.3% | -20.0% |
| cpy | `-xno-sob` | 1,236,107 / 2,056,507 | 4,943,618 / 5,765,218 | -28.5% | -20.0% | -0.1% |
| sum | `-xno-autoinc` | 1,236,909 / 2,056,109 | 4,124,418 / 4,943,618 | -28.4% | -19.9% | -24.9% |
| cmpval | `-xno-setcc` | 6,900,011 / 6,900,011 | 18,000,030 / 18,000,030 | -13.8% | -4.0% | -4.0% |
| minmax | `-xno-minmax` | 6,300,011 / 6,590,646 | 17,400,030 / 17,981,300 | -11.2% | -15.9% | -16.9% |
| field | `-xno-field` | 7,000,011 / 8,600,011 | 20,400,030 / 22,800,030 | -19.3% | -14.6% | -15.8% |
| rmw | `-xno-rmw` | 6,000,012 / 7,200,012 | 19,200,040 / 22,200,040 | -16.1% | -14.8% | -11.5% |

The sign is the change in cycles caused by having the option on.  `jtab` on a ten-case switch executes 6% more instructions and takes +0.4% cycles at one issue slot, -4.5% at two and -4.0% at four; on Lua's interpreter (83 cases) it executes 3.0% fewer instructions
and takes 3.4% to 5.0% fewer cycles over the models while making the code 806 bytes smaller (halves of compares were four bytes a case).  `divp2` and `divmagic` make the code larger and the loops 30-67% faster; every other pass is smaller or equal in bytes and faster.


### Verification

* pcc's test harness (`arch/pdp64/test/pdp64test-all.sh`; 86 programs from the project plus 18 of this directory): pdp64/r20 602 of 602, pdp64/16 954 of 954, pdp64/32 954 of 954 (flag sets: the defaults, each of `-xno-dmr -xno-temps -xno-deljumps -xno-inline -xno-dce` alone, `-xssa`, `-xno-tailcall`, `-xccp`).
* The r20 output is byte-identical to the compiler before this phase on all 841 corpus files (checked by comparing the assembly, after every change).
* All 841 corpus files compile and assemble with `as64 -m pdp64/16` and `-m pdp64/32`.
* The six programs run and print the host's output on pdp64/r20, pdp64/16 and pdp64/32 (18 of 18), and with `-xssa` on /16 and /32 (12 of 12).
* Other back ends: the machine-independent changes are `ccp` (only with `-xccp`, which no other target sets), a one-line change to the test for a `casel`-like XASM, and an `extern`.  Checked: amd64 `ccom` built from the tree, compiled the test programs with `-xtemps -xdeljumps -xdce -xdmr` with and without `-xccp`, linked with gcc, all returned 0;
  the vax `ccom` builds and compiles with `-xccp` (no VAX simulator here to run it); the default output of other targets does not change because no flag they set reaches the new code.

