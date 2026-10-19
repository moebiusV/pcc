# Portable C Compiler

This repository is the development branch of Anders Magnusson's (ragge's) pcc compiler, which is his
modernized and updated version of the original Steve Johnson portable C compiler from Version 7 Unix.

The major things worked on in this fork, each described under [What This Fork Changes](#what-this-fork-changes):

- **Bug fixes** in shared code and in the back ends, found by running generated programs on real systems and
  emulators and by differential fuzzing against the host compiler (soft-float comparisons, shifts, `dce()`,
  `switch`/`casel`, PDP-11 long arithmetic, amd64/i386 unsigned conversions, and more).
- **`-xdmr`**: size optimizations modelled on Dennis Ritchie's V7 compiler, behind one flag, for the PDP-11,
  VAX, i386, amd64 and AArch64.
- **Dead-code elimination (`-xdce`)** and shared flow-graph passes: loop rotation, cross-jumping and tail
  merging, spill-slot sharing, and a better instruction matcher.
- **The old peepholes inside `ccom`**: the V7 `c2` for the PDP-11 and a C99 port of 32V's `c2` for the VAX,
  with x86 and AArch64 peepholes of the same kind.
- **PDP-11 and VAX back ends** brought to the size of the original Unix compilers or below, including 64-bit
  `long long` on both.
- **A new AAPCS64 (LP64) AArch64 back end** that replaces the ARM32-derived one, working on Linux and the BSDs.
- **Use as the code generator of other compilers** (the IMP72 bootstrap): pass 2 reading textual IR on amd64,
  i386 and AArch64, with the portability fixes that needed (musl, hosts without x87 long double, the i386
  two-pass build).
- **Measurement**: [doc/SIZE-REPORT.md](doc/SIZE-REPORT.md) compares stock pcc, this fork, clang, gcc, V7 dmr
  cc and 32V pcc on the same sources.

## What Is In This Tree

The repository is organized around the traditional PCC split between a
C front end, machine-independent compiler support, and target-specific
back ends:

- `cc/cc` - compiler driver installed as `pcc`, `pcpp`, and `p++`
- `cc/cpp` - C preprocessor
- `cc/ccom` - C compiler front end and first-pass code generation
- `cc/cxxcom` - C++ front-end sources
- `mip` - machine-independent pass 2 support, instruction matching,
  register allocation, and optimizer code
- `arch` - target back ends, including amd64, i386, i86, aarch64, arm,
  m68k, mips, mips64, powerpc, riscv, sparc64, vax, pdp10, pdp11,
  pdp7, nova, hppa, and m16c
- `os` - target operating-system configuration headers
- `common` - shared compatibility, Unicode, string-to-floating-point,
  and soft-float support
- `f77` - historical Fortran driver and front-end sources; present in
  the distribution tree but not built by the top-level `all` target

The configured operating-system targets include the major modern POSIX
operating systems, such as macOS, the BSD family, Linux, Android, and
several bare or older-system target combinations.

## Building

The checked-in `configure` script is generated, so a normal build does
not require Autoconf.

```sh
./configure
make
```

For a native install:

```sh
./configure --prefix=/usr/local --enable-native
make
make install
```

For a cross compiler, pass a target triplet:

```sh
./configure --target=vax-dec-bsd --prefix=/usr/local
make
make install
```

Useful configuration options include:

- `--target=<triplet>` - select the target CPU/OS/ABI
- `--enable-native` - build as a native compiler even when the target
  alias would otherwise make it look like a cross compiler
- `--enable-twopass` - build separate first and second compiler passes
- `--disable-gcc-compat` - turn off GCC-compatibility code paths
- `--enable-tls` - enable thread-local storage support when the target
  supports it
- `--with-incdir=<path>` and `--with-libdir=<path>` - set default
  system include and library search paths
- `--with-assembler=<path>` and `--with-linker=<path>` - set the
  assembler and linker used by the driver
- `--enable-multiarch=yes|no|auto|<triplet>` - control Linux multiarch
  path handling

Build tools normally needed are a C compiler, `make`, `yacc`/`byacc`
or compatible, and `lex`/`flex`.  Regenerating `configure`
additionally requires Autoconf.

The preprocessor has a small test target:

```sh
make -C cc/cpp test
```

Run it after configuring and building; the test target uses the
ngenerated Makefiles and the freshly built preprocessor.

## How This Differs From The Historic PCC

The original PCC was valued for being compact and retargetable, but it
was a compiler for a much smaller C and Unix world.  The current PCC
project is a long-running effort to keep that model while updating the
compiler for newer systems and ABIs.

Notable changes versus the original PCC include:

- **Autoconf and cross-build support.**  The build system understands
  host and target triples, native versus cross builds, alternate
  assemblers/linkers, sysroot-style paths, and Linux multiarch include
  directories.
- **Many more back ends.**  Alongside traditional targets such as VAX
  PDP-11 and PDP-10, this tree contains support for many new targets
  such as amd64, i386, ARM, AArch64, MIPS, MIPS64, PowerPC, SPARC64,
  and RISC-V.
- **Modernized driver behavior.**  The driver accepts many familiar
  Unix/GCC options, handles `.c`, `.i`, `.s`, `.S`, and object inputs,
  and installs `pcc`, `pcpp`, and `p++` front ends.
- **A substantially rewritten preprocessor.** The preprocessor has
  been dramatically modernized.
- **C99 and GNU compatibility work.**  The compiler has C99-oriented
  parsing and type handling, GNU statement-expression support,
  selected attributes and builtins, `__GNUC__` compatibility
  definitions when enabled, GCC-style inline assembly work, stack
  protector hooks, and many libc-facing builtin stubs.
- **Better aggregate and ABI handling.**  Improved structures, unions,
  bit-fields, arrays, function prototypes, hidden structure return
  arguments, register arguments, stack arguments, varargs, and ABI
  classification correct across targets.
- **Floating-point and complex improvements.**  The tree includes
  soft-float support, long double handling, `__float128`-related
  compatibility work, complex type fixes, IEEE64 long double options,
  and newer builtin support such as `__builtin_isinf_sign`.
- **Reworked internals.**  The pass 1 type model, prototype storage,
  argument handling, instruction needs, shape matching, liveness
  analysis, and register allocation have all been refactored over the
  years.

## What This Fork Changes

This tree is a fork of [PortableCC/pcc](https://github.com/PortableCC/pcc) (`origin/master`).  The work is about
code **size** and **correctness**: with `-O`, the fork's PDP-11 and VAX output is smaller than the original
Unix compilers' (V7 dmr cc, 32V pcc -O) on the V7 and 32V sources, and 4% to 17% smaller than stock pcc's
on amd64, i386 and AArch64 -- still 14% to 49% larger than clang `-Os`.  How the numbers were measured, and how stock and fork compare on every target both clang and pcc
reach, is in **[doc/SIZE-REPORT.md](doc/SIZE-REPORT.md)**.

Read the numbers with **[doc/SIZE-NOTES.md](doc/SIZE-NOTES.md)**: a smaller size is only a better compiler if both
compilers produced correct code for the same valid program.  Stock pcc on the PDP-11 has no 64-bit `long long`
(it computes with 32 bits) and gets `unsigned long >> n` wrong, so on files that use them its code is smaller
because it is wrong; on amd64 and i386 the fork's advantage is its optimizations (without `-xdmr` the fork is
stock's size); and against gcc on the PDP-11 a few dozen crypto functions are compared on code that is undefined
on a 16-bit `int`.

Everything that is a code-size trick for a specific old compiler sits behind **`-xdmr`**, which `-O` turns on
for the PDP-11 and AArch64 (the other targets accept it as a flag).  Without `-xdmr` the generated code
differs from stock only through the bug fixes below.

### Dead-code elimination and the shared optimizer

- **`-xdce`** (`mip/regs.c`, `cc/ccom`): dead assignments and unreachable blocks are removed on the flow
  graph; reads of `const` integer objects with a constant initializer fold to the constant; a
  discarded conversion around an unused `?:` arm is dropped.  `dce()` now relinks the flow graph when it
  deletes a one-statement block (before, a live loop assignment could be deleted).
- **Passes on the tree form, for every target** (`mip/optim2.c`, run under `-xdmr`): loop rotation
  (test at the bottom), cross-jumping and tail merging, `deljumps` that inverts a relation without swapping
  its operands, and spill-slot sharing sized from the temporaries' real types.
- **Pass-1 rewrites** (`-xdmr`): `x op= e` and `x++` through a pointer evaluate the pointer once; a `?:` used
  as a condition branches from each arm; `a[i] op= v` computes the address once; the arms of a `?:` statement
  are evaluated for effect only; a dense `switch` is a jump table on the PDP-11 and `casel` on the VAX.
- **The old peepholes live inside `ccom`** (`cc2/`, `common/c2run.h`): the V7 `c2` for the PDP-11 and a C99
  port of 32V's `c2` for the VAX, so the optimization needs no separate pass-2 program.  Under `-xdmr`
  the x86 targets get `common/x86peep.h` (redundant tests, loop rotation) and AArch64 gets `arch/aarch64/peep.h`.
  The instruction matcher prefers a register that is already in place (`mip/match.c`).

### Back ends

- **PDP-11.**  Brought to V7 dmr cc's size or below on V7's libc and commands: deferred and auto-increment
  addressing (`*p++`, `*name`, `(rN)+`), byte loads and compares without widening, in-place updates through
  pointers, `sob`-friendly loops, unsigned divide by a constant as `clr r0; div`, smaller divide helpers
  written into the file that uses them.  **`long long`** is a 64-bit memory object operated on by helper
  calls (`arch/pdp11/ll.c`, `lowerll.c`).
- **VAX.**  32V `as` syntax and `$name` operands, `casel`, `movz`/`cvt`/`incl`/`decl`/`sob` forms, `bbs`/`bbc`
  bit tests, parameters read from their slots, long long helpers (`__muldi3` ...), a C99 `c2`.
- **AArch64.**  A new AAPCS64 LP64 back end replaces the ARM32-derived one (the old one does not
  compile most programs): paired register saves, pre/post-indexed frames, `cbz`/`tbz`, `cset`, extended
  and scaled index operands, `va_list` and struct passing as AAPCS64 defines them, and a text optimizer under
  `-xdmr`.  It is tested on an emulator and against clang's code (`tools/arm64run.py`, `tools/arm64abi.sh` in
  the toolchain repo).  Open: `alloca`, VLAs, jump tables, a 128-bit `long double`, Mach-O (Apple Silicon).
- **i386 / amd64.**  Shortest forms for constants and addresses, `leal sym(,%reg,scale)`, byte/short compares
  without widening.

### Bug fixes found along the way (shared code unless noted)

Found by running the generated programs (the V7 and 32V command sets under their own systems and an
emulator) and by differential fuzzing against the host compiler with sanitizers (`tools/exprfuzz.py`):

- `soft_cmp` ordered negative and zero floating constants wrongly;
- a shift by zero lost the node's type, and merged constant left shifts went past the type's width;
- amd64/i386: `int / int` and `int % int` retyped across an unsigned conversion; `(long)(unsigned << c)`;
- `dce()` deleted a live loop assignment after removing a one-statement block;
- a `switch` on a variable that is live across a dense `casel`;
- PDP-11: a long left shift printed the wrong register pair; a long load clobbered its base register; long `&`,
  `|`, `^` claimed condition codes; `a = b = 0` on longs; `sizeof` is unsigned; unsigned `>>` on longs;
- compiler crashes on `register x >>= n` and `**p++ = 0`;
- `x++` as a statement folded through the placeholder of a rewritten `op=`.

### Tests and tools

The test suites, emulator and simulator harnesses, the fuzzer and the size scoreboards are in the toolchain
repository this compiler is built for (`facetlang/pdp11/v7unix-toolchain`: `tools/pcctest-all.sh`,
`vaxtest-all.sh`, `x86test-all.sh`, `a64test-all.sh`, `arm64test-all.sh`, `pccdiff.sh`, `vaxdiff.py`,
`corpus.py`).  Each change above has a regression test that runs on the real system or an emulator of it.

## Licensing

The sources are available under a mix of permissive BSD and ISC style
licenses. See `LICENSE.md` for an aggregate audit and the main license
texts.  Individual file headers remain authoritative.
