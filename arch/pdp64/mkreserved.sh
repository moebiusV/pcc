#!/bin/sh
# Make pdp64-reserved.h, the names a C symbol cannot have in as64 assembly (mnemonics, registers), from pdp64-names.h.
# Run it after ~/claude/pdp64/tools/export-pcc-headers.sh has written a new pdp64-names.h.
cd "$(dirname "$0")"
{
	echo '/* made by mkreserved.sh from pdp64-names.h -- do not edit.  Sorted words that exprot() writes in quotes. */'
	sed -n 's/^#define [A-Z0-9_]* *"\([a-z0-9_]*\)".*/\1/p' pdp64-names.h | sort -u | sed 's/.*/	"&",/'
} > pdp64-reserved.h
