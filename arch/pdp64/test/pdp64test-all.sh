#!/bin/sh
#
# pdp64test-all.sh -- the pdp64 test programs (first line "/* expect: N */") with a ccom, for a target, under several flag sets.
#
# usage: PDP64CCOM=ccom PDP64TOOLS=dir [PDP64TESTS=dirs] pdp64test-all.sh [-m target] ["flags" ...]
# Tests: $PDP64TESTS (default: the a64tests of the v7unix-toolchain and tests/c of the pdp64 repository, space separated).
# Flag sets: for pdp64/r20 plain and the optimizing ones of the original harness; for pdp64/16 and pdp64/32 the target's
# defaults (all optimizations), then each optimization off in turn, and -xssa.
#
set -u
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
mode=
if [ "${1:-}" = -m ]; then mode=$2; shift 2; fi
# the tests of this directory (test/c) are for the new targets: pdp64/r20 keeps a bug they would find (a TEMP made unsigned: local.c)
case "${mode:-pdp64/r20}" in
pdp64/r20) : ${PDP64TESTS:="$HOME/claude/facetlang/pdp11/v7unix-toolchain/tools/a64tests $HOME/claude/pdp64/tests/c"} ;;
*) : ${PDP64TESTS:="$HOME/claude/facetlang/pdp11/v7unix-toolchain/tools/a64tests $HOME/claude/pdp64/tests/c $here/c"} ;;
esac
if [ $# -eq 0 ]; then
	case "${mode:-pdp64/r20}" in
	pdp64/r20) set -- "" "-xdmr" "-xtemps -xdeljumps -xinline -xdce" "-xdmr -xtemps -xdeljumps -xinline -xdce" "-xdmr -xtemps -xdeljumps -xinline -xdce -xssa" "-xdmr -xtemps -xdeljumps -xinline -xdce -xabs" "-xdmr -xtemps -xdeljumps -xinline -xdce -xccp" ;;
	*) set -- "" "-xno-dmr" "-xno-temps" "-xno-deljumps" "-xno-inline" "-xno-dce" "-xssa" "-xno-tailcall" "-xccp" ;;
	esac
fi
pass=0; fail=0
for flags in "$@"; do
	for d in $PDP64TESTS; do
	for t in "$d"/*.c; do
		want=$(sed -n '1s/.*expect: \(-\{0,1\}[0-9]*\).*/\1/p' "$t")
		got=$("$here/pdp64test.sh" ${mode:+-m $mode} $flags "$t" 2>&1 | tail -1)
		if [ "$got" = "$want" ]; then pass=$((pass+1))
		else fail=$((fail+1)); echo "FAIL [$mode $flags] $(basename "$t"): want $want, got $got"; fi
	done
	done
done
echo "pdp64test-all [${mode:-default}]: $pass passed, $fail failed"
[ $fail -eq 0 ]
