/*	$Id$	*/
 /*
 * Copyright (c) 2020 Puresoftware Ltd.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Machine-dependent defines for both passes.
 *
 * AArch64 (A64), LP64, little-endian.  Registers: x0-x15 and x19-x28 are
 * allocated (x0-x15 are clobbered by calls); x16 and x17 are scratch for the
 * code printed by the table (large offsets, symbol addresses, constants);
 * x29 is the frame pointer, x30 the link register, and sp the stack pointer.
 * d0-d31 hold float and double; d8-d15 are callee-saved.
 *
 * long double is double (ctype()), long long is long.
 */

/*
 * Convert (multi-)character constant to integer.
 */
#define makecc(val,i)	lastcon = (lastcon<<8)|((val<<24)>>24);

/*
 * Storage space requirements
 */
#define SZCHAR		8
#define SZBOOL		8
#define SZSHORT		16
#define SZINT		32
#define SZLONG		64
#define SZPOINT(t)	64
#define SZLONGLONG	64
#define SZFLOAT		32
#define SZDOUBLE	64
#ifdef A64QUAD
#define SZLDOUBLE	128	/* binary128: see AARCH64.md */
#else
#define SZLDOUBLE	64
#endif

/*
 * Alignment constraints
 */
#define ALCHAR		8
#define ALBOOL		8
#define ALSHORT		16
#define ALINT		32
#define ALLONG		64
#define ALPOINT		64
#define ALLONGLONG	64
#define ALFLOAT		32
#define ALDOUBLE	64
#ifdef A64QUAD
#define ALLDOUBLE	128
#else
#define ALLDOUBLE	64
#endif
#define ALSTACK		64
#ifdef A64QUAD
#define ALMAX		128
#else
#define ALMAX		64
#endif

/*
 * Min/max values.
 */
#define	MIN_CHAR	-128
#define	MAX_CHAR	127
#define	MAX_UCHAR	255
#define	MIN_SHORT	-32768
#define	MAX_SHORT	32767
#define	MAX_USHORT	65535
#define	MIN_INT		(-0x7fffffff-1)
#define	MAX_INT		0x7fffffff
#define	MAX_UNSIGNED	0xffffffffU
#define	MIN_LONG	0x8000000000000000LL
#define	MAX_LONG	0x7fffffffffffffffLL
#define	MAX_ULONG	0xffffffffffffffffULL
#define	MIN_LONGLONG	0x8000000000000000LL
#define	MAX_LONGLONG	0x7fffffffffffffffLL
#define	MAX_ULONGLONG	0xffffffffffffffffULL

/* Default char is signed (as on Apple's arm64; AAPCS64 proper has unsigned) */
#undef	CHAR_UNSIGNED
#define	BOOL_TYPE	UCHAR	/* what used to store _Bool */

/*
 * Use large-enough types.
 */
typedef	long long CONSZ;
typedef	unsigned long long U_CONSZ;
typedef long long OFFSZ;

#define CONFMT	"%lld"		/* format for printing constants */
#ifdef MACHOABI
#define LABFMT	"L%d"		/* format for printing labels (L is private in Mach-O) */
#define LOCLAB	"L"		/* ... and the same for labels made by name */
#define TEXTDIR	".text"
#else
#define LABFMT	".L%d"		/* format for printing labels */
#define LOCLAB	".L"
#define TEXTDIR	".section .text"
#endif
#define	STABLBL	".LL%d"		/* format for stab (debugging) labels */

#define	HASP2ALIGN		/* .align takes a power of two */
#undef	FIELDOPS		/* no bit-field instructions */
#define	TARGET_ENDIAN TARGET_LE	/* little-endian only */
#define	CC_DIV_0		/* division by zero is safe in the compiler */

#define ARGINIT		0	/* # bits above fp where arguments start; see PARAMMARK */
#define AUTOINIT	(16*8)	/* # bits above fp where automatics start (the frame record) */

/* Definitions mostly used in pass2 */

#define BYTEOFF(x)	((x)&07)
#define wdal(k)		(BYTEOFF(k)==0)

#define STOARG(p)
#define STOFARG(p)
#define STOSTARG(p)

#define genfcall(a,b)	gencall(a,b)

/* How many 32-bit words does a value of this type take (used for stack allocation) */
#ifdef A64QUAD
#define	szty(t)	(t == LDOUBLE ? 4 : t < LONG || t == FLOAT ? 1 : 2)
#else
#define	szty(t)	(t < LONG || t == FLOAT ? 1 : 2)
#endif

/*
 * The registers.  Numbers are the pass-2 register numbers, not the machine's:
 *	0-15	x0-x15
 *	16-25	x19-x28 (callee-saved)
 *	26	x29, the frame pointer
 *	27	sp
 *	28-35	d0-d7
 *	36-43	d8-d15 (callee-saved)
 *	44-58	d16-d30 (d31 is the floating point scratch register)
 */
#define	X0	0
#define	X1	1
#define	X2	2
#define	X3	3
#define	X4	4
#define	X5	5
#define	X6	6
#define	X7	7
#define	X8	8
#define	X9	9
#define	X10	10
#define	X11	11
#define	X12	12
#define	X13	13
#define	X14	14
#define	X15	15
#define	X19	16
#define	X28	25
#define	FPREG	26
#define	STKREG	27
#define	D0	28
#define	D7	35
#define	D8	36
#define	D15	43
#define	D16	44
#define	D30	58

#define	MAXREGS	59

/*
 * d8-d15 are callee-saved, but only their low 64 bits: a 128-bit long double kept there would
 * lose the rest in any call, so with A64QUAD no floating-point register survives a call.
 */
#ifdef A64QUAD
#define	DPERM	TEMPREG
#else
#define	DPERM	PERMREG
#endif

#define	RSTATUS	\
	SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG,	\
	SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG,	\
	SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG,	\
	SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG,	\
	SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG,	\
	SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG,	\
	SAREG|PERMREG, SAREG|PERMREG, 0, 0,				\
	SBREG|TEMPREG, SBREG|TEMPREG, SBREG|TEMPREG, SBREG|TEMPREG,	\
	SBREG|TEMPREG, SBREG|TEMPREG, SBREG|TEMPREG, SBREG|TEMPREG,	\
	SBREG|DPERM, SBREG|DPERM, SBREG|DPERM, SBREG|DPERM,		\
	SBREG|DPERM, SBREG|DPERM, SBREG|DPERM, SBREG|DPERM,		\
	SBREG|TEMPREG, SBREG|TEMPREG, SBREG|TEMPREG, SBREG|TEMPREG,	\
	SBREG|TEMPREG, SBREG|TEMPREG, SBREG|TEMPREG, SBREG|TEMPREG,	\
	SBREG|TEMPREG, SBREG|TEMPREG, SBREG|TEMPREG, SBREG|TEMPREG,	\
	SBREG|TEMPREG, SBREG|TEMPREG, SBREG|TEMPREG,

/* no overlapping registers at all */
#define	ROVERLAP \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
	{ -1 }, { -1 }, { -1 },

/* Return a register class based on the type of the node */
#ifdef A64QUAD
#define PCLASS(p) (p->n_type == FLOAT || p->n_type == DOUBLE || p->n_type == LDOUBLE ? SBREG : SAREG)
#else
#define PCLASS(p) (p->n_type == FLOAT || p->n_type == DOUBLE ? SBREG : SAREG)
#endif

#define	NUMCLASS	2	/* highest number of reg classes used */

int COLORMAP(int c, int *r);
#define	GCLASS(x)	(x < 28 ? CLASSA : CLASSB)
#define DECRA(x,y)	(((x) >> (y*8)) & 255)	/* decode encoded regs */
#define	ENCRD(x)	(x)			/* Encode dest reg in n_reg */
#define ENCRA1(x)	((x) << 8)		/* A1 */
#define ENCRA2(x)	((x) << 16)		/* A2 */
#define ENCRA(x,y)	((x) << (8+y*8))	/* encode regs in int */
#ifdef A64QUAD
#define	RETREG(x)	(x == FLOAT || x == DOUBLE || x == LDOUBLE ? D0 : X0)
#else
#define	RETREG(x)	(x == FLOAT || x == DOUBLE ? D0 : X0)
#endif

/*
 * Stack-passed parameters are addressed from the frame pointer at
 * PARAMMARK + their offset above the caller's stack pointer; the real
 * distance is only known once the frame is laid out, so it is added where
 * the address is printed (local2.c, fpoff()).
 */
#define	PARAMMARK	0x1000000

/* Special shapes */
#define	SIMM12		(MAXSPECIAL+1)	/* 0..4095, add/sub/cmp immediate */
#define	SNIMM12		(MAXSPECIAL+2)	/* -4095..-1, sub/add/cmn immediate */
#define	SLOGIMM		(MAXSPECIAL+3)	/* encodable and/orr/eor immediate */
#define	SLOGIMM64	(MAXSPECIAL+4)	/* the same, 64-bit */
#define	SSH32		(MAXSPECIAL+5)	/* shift count 0..31 */
#define	SSH64		(MAXSPECIAL+6)	/* shift count 0..63 */
#define	SFUNCALL	(MAXSPECIAL+7)	/* struct assign after function call */

#define	TARGET_FLT_EVAL_METHOD	0	/* all as their type */

/* floating point definitions */
#define USE_IEEEFP_32
#define FLT_PREFIX	IEEEFP_32
#define USE_IEEEFP_64
#define DBL_PREFIX	IEEEFP_64
#if defined(A64QUAD) && !defined(MACHOABI)
#define USE_IEEEFP_128
#define LDBL_PREFIX	IEEEFP_128
#define DEFAULT_FPI_DEFS { &fpi_binary32, &fpi_binary64, &fpi_binary128 }
#else
#define LDBL_PREFIX	IEEEFP_64
#define DEFAULT_FPI_DEFS { &fpi_binary32, &fpi_binary64, &fpi_binary64 }
#endif

#define	COM	"\t// "

#define	TARGET_VALIST
#define	TARGET_STDARGS
#define	TARGET_BUILTINS							\
	{ "__builtin_stdarg_start", aarch64_builtin_stdarg_start,	\
						0, 2, 0, VOID },	\
	{ "__builtin_va_start", aarch64_builtin_stdarg_start,		\
						0, 2, 0, VOID },	\
	{ "__builtin_va_arg", aarch64_builtin_va_arg, BTNORVAL|BTNOPROTO,\
							2, 0, 0 },	\
	{ "__builtin_va_end", aarch64_builtin_va_end, 0, 1, 0, VOID },	\
	{ "__builtin_va_copy", aarch64_builtin_va_copy, 0, 2, 0, VOID },
#ifdef LANG_CXX
#define P1ND struct node
#else
#define P1ND struct p1node
#endif
struct node;
struct bitable;
P1ND *aarch64_builtin_stdarg_start(const struct bitable *, P1ND *a);
P1ND *aarch64_builtin_va_arg(const struct bitable *, P1ND *a);
P1ND *aarch64_builtin_va_end(const struct bitable *, P1ND *a);
P1ND *aarch64_builtin_va_copy(const struct bitable *, P1ND *a);
#undef P1ND

/* the class of a struct a call returns (see code.c): set by myp2tree() for pass 2 */
#define	ATTR_MI_TARGET	ATTR_AARCH64_STRRET

/* struct arguments and results (AAPCS64) */
#define	SC_REG	1	/* up to 16 bytes: in one or two x registers */
#define	SC_HFA	2	/* one to four floats or doubles: in as many v registers */
#define	SC_MEM	3	/* larger: an argument is passed by address, a result goes through x8 */
