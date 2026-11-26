#!/bin/sh
#
# pdp64test.sh -- compile a C file with the pdp64 ccom, assemble it with as64, link it with a tiny driver and run it
# under sim64: prints what `int test()` returns.  tools/pdp64test.sh of the pdp64 repository, with the target:
#
# usage: [PDP64CCOM=ccom] [PDP64TOOLS=dir-of-as64-ld64-sim64] pdp64test.sh [ccom flags] x.c
# There is one target, pdp64; as64 (no options needed), ld64 and sim64 are run as: as64 -o x.o x.s; ld64 -o x.x crt.o main.o t.o; sim64 x.x
# PDP64CPP is the preprocessor command (default gcc -E -P -undef -D__pcc__ -D__LP64__ -D_LP64).
#
set -eu
ccom=${PDP64CCOM:?PDP64CCOM is the ccom to test}
T=${PDP64TOOLS:?PDP64TOOLS is the directory of as64, ld64 and sim64}
cpp=${PDP64CPP:-gcc -E -P -undef -D__pcc__ -D__LP64__ -D_LP64}
flags=
while [ $# -gt 1 ]; do
	flags="$flags $1"
	shift
done
src=$1
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
$cpp "$src" > "$tmp/t.i"
$ccom $flags "$tmp/t.i" "$tmp/t.s"
cat > "$tmp/main.c" <<'EOT'
int test(void);
long write(int, char *, long);
int main(void)
{
	char b[24];
	char *p = b + 24;
	long v = test();
	int neg = v < 0;
	unsigned long u = neg ? -(unsigned long)v : (unsigned long)v;
	*--p = '\n';
	do { *--p = '0' + (int)(u % 10); u /= 10; } while (u);
	if (neg) *--p = '-';
	write(1, p, b + 24 - p);
	return 0;
}
EOT
nomemcpy=0; grep -q "^memcpy:" "$tmp/t.s" && nomemcpy=1
cat > "$tmp/crt.s" <<'EOT'
	.text
	.globl	_start
_start:	call	main
	trap	#0
	.globl	write
write:	trap	#1
	ret
	.globl	memcpy
memcpy:	movq	r0, r3
	mcpy	r0, r1, r2
	movq	r3, r0
	ret
	.globl	memmove
memmove: movq	r0, r3
	mmov	r0, r1, r2
	movq	r3, r0
	ret
	.globl	memset
memset:	movq	r0, r3
	mset	r0, r1, r2
	movq	r3, r0
	ret
EOT
$ccom "$tmp/main.c" "$tmp/main.s" 2>/dev/null || { $cpp "$tmp/main.c" > "$tmp/main.i"; $ccom $flags "$tmp/main.i" "$tmp/main.s"; }
if [ $nomemcpy = 1 ]; then awk '/^\t.globl\tmemcpy/{skip=6} skip>0{skip--; next} {print}' "$tmp/crt.s" > "$tmp/crt2.s" && mv "$tmp/crt2.s" "$tmp/crt.s"; fi
for f in t main crt; do "$T/as64" -o "$tmp/$f.o" "$tmp/$f.s"; done
"$T/ld64" -o "$tmp/t.x" "$tmp/crt.o" "$tmp/main.o" "$tmp/t.o"
"$T/sim64" "$tmp/t.x"
