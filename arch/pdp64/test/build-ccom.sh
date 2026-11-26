#!/bin/sh
#
# build-ccom.sh -- build the pdp64 ccom of this tree (as tools/build-pcc.sh of the pdp64 repository does, from the tracked files)
# into DIR (default ./build-pcc): DIR/ccom, the compiler for the pdp64 target (the 32-register prefix-coded encoding).
# usage: build-ccom.sh [DIR]
#
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
top=$(CDPATH= cd -- "$here/../../.." && pwd -P)
dir=${1:-build-pcc}
mkdir -p "$dir"
dir=$(CDPATH= cd -- "$dir" && pwd -P)
d=$dir/src
mkdir -p "$d"
(cd "$top" && git ls-files -z | tar --null -T - -cf -) | tar xf - -C "$d"
(cd "$d" && rm -f cc/ccom/*.o cc/ccom/*.lo cc/ccom/mkext cc/ccom/external.[ch] cc/ccom/*-ccom
 ./configure --target=pdp64-unknown-linux >cfg.log 2>&1
 cd cc/ccom && make CFLAGS="-g -O1 -std=gnu89 -w -fcommon" >mk.log 2>&1)
ln -sf "$(ls -t "$d"/cc/ccom/*-ccom | head -1)" "$dir/ccom"
echo "built $dir/ccom"
