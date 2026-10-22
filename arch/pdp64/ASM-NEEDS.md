# What the pdp64 back end would emit if as64 had it

Nothing the compiler emits today is missing from `as64`.  Still not assembled (only these three groups):

* `MESC` read-modify-write on memory: `addm rs, mem`, `subm`, `bicm`, `bism` (rm <- rm op reg).  Would be
  emitted for `x op= y` and `x |= mask` on a 64-bit object in memory (now: load, op, store = 3 instructions,
  10-14 bytes; with them one 4-6 byte instruction), via the shared FINDMOPS path (`ASSIGN(x, op(x, e))`).
  Quad operands only.  Not used yet.
* `ATOM` (`cas`, `swap`, `xadd`, ... and the `_db` forms): `__sync_*` / `__atomic_*` builtins are not
  implemented in the back end yet, so nothing asks for them.
* `trapl`: not a C construct.

Assumptions the back end makes about instructions that exist, which the ISA text does not spell out
(please confirm or tell me the right form):

* `clr mem`, `inc mem`, `tst mem` act on a quad (the table uses them for 64-bit objects only).
* `call (rN)` / `jmp (rN)` transfer to the address in rN (the effective address, as the PDP-11's JMP);
  `calli d(rN)` / `calli sym` call through the pointer stored there.
* `fcvtdq` truncates toward zero (C's double -> long long), `fcvtsl` does the same for float -> int32 and
  sign-extends, `fcvtls` is int32 -> float, `fcvtqd` int64 -> double; the `fcvtdqu` / `fcvtqud` pair is
  unsigned 64-bit.  `fneg rs, rd` and the binary64 forms only; a float is negated with `xor #-2147483648`.
* `div ra, rb, rd` is ra / rb (the first source is the dividend).
* After `fcmp rd, rs`: N = rd < rs, Z = equal, V = unordered, C clear (errata 14).  The back end then uses
  `bmi`/`bpl`/`bgt`/`bge`/`blt`/`ble` so that an unordered pair makes <, <=, >, >= false.
