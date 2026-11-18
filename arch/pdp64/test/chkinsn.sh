#!/bin/sh
# chkinsn.sh -- the checks of -xchecks as instructions (errata 152): the assembly has `ctcc` or `cteq` and a `.chk` line for each check that is not proved, none for a proved
# one, no call of __chk_fail; and as64 accepts it.  usage: PDP64CCOM=ccom PDP64TOOLS=dir chkinsn.sh
here=$(dirname "$0"); ccom=${PDP64CCOM:-ccom}; tools=${PDP64TOOLS:-build}; w=${TMPDIR:-/tmp}/chkinsn.$$; fail=0
cat > $w.c <<'C'
long a[10];
long b[5000];
long prov(void) { long s = 0; int i; for (i = 0; i < 10; i++) s += a[i]; return s; }
long unprov(unsigned long i) { return a[i]; }
long wide(long i) { return b[i]; }
char *nz(char *p) { return __builtin_chkz(p); }
C
"$ccom" -xchecks $w.c $w.s || { echo "FAIL chkinsn: ccom"; exit 1; }
n=$(grep -c 'ctcc' $w.s); z=$(grep -c 'cteq' $w.s); c=$(grep -c '\.chk' $w.s); f=$(grep -c '__chk_fail' $w.s)
[ "$n" = 2 ] && [ "$z" = 1 ] && [ "$c" = 3 ] && [ "$f" = 0 ] || { echo "FAIL chkinsn: ctcc $n (want 2), cteq $z (want 1), .chk $c (want 3), __chk_fail $f (want 0)"; fail=1; }
awk '/^prov:/{p=1;next} /^[a-z]+:/{p=0} p' $w.s | grep -q 'ctcc' && { echo "FAIL chkinsn: a proved check was kept"; fail=1; }
grep -q 'ctcc r[0-9]*,#10' $w.s || { echo "FAIL chkinsn: the 12-bit immediate form"; fail=1; }
grep -q 'ctcc r[0-9]*,r[0-9]*' $w.s || { echo "FAIL chkinsn: the register form for a bound of 5000"; fail=1; }
"$tools/as64" -o $w.o $w.s || { echo "FAIL chkinsn: as64"; fail=1; }
[ $fail = 0 ] && echo "ok   chkinsn"
rm -f $w.c $w.s $w.o
exit $fail
