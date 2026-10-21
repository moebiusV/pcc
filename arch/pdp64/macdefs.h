/*	$Id$	*/
/*
 * Machine-dependent defines for both passes.
 *
 * pdp64: a 64-bit PDP-11-style machine (docs in ~/claude/pdp64/docs).  LP64, little-endian,
 * one register class: r0-r15 hold integers, pointers and floating point alike.
 *
 * The pass-2 register numbers are not the machine's:
 *	0-11	r0-r11	(r0-r7 are clobbered by a call, r8-r11 are saved by the callee)
 *	12	r15	(saved by the callee)
 *	13	r13, the frame pointer
 *	14	r14, the stack pointer
 *	15	r12, the scratch register of the code printed by the table (not allocated)
 *
 * long double is double, long long is long.
 *
 * A value of a type narrower than 64 bits is kept in a register extended to 64:
 * signed types (and "unsigned int") sign-extended from their own width, unsigned char
 * and unsigned short zero-extended.  "int" and "unsigned int" share one form, so a
 * conversion between them is free, and int to long needs no instruction.
 */

#define makecc(val,i)	lastcon = (lastcon<<8)|((val<<24)>>24);

#define SZCHAR		8
#define SZBOOL		8
#define SZSHORT		16
#define SZINT		32
#define SZLONG		64
#define SZPOINT(t)	64
#define SZLONGLONG	64
#define SZFLOAT		32
#define SZDOUBLE	64
#define SZLDOUBLE	64

#define ALCHAR		8
#define ALBOOL		8
#define ALSHORT		16
#define ALINT		32
#define ALLONG		64
#define ALPOINT		64
#define ALLONGLONG	64
#define ALFLOAT		32
#define ALDOUBLE	64
#define ALLDOUBLE	64
#define ALFTN		ALSHORT	/* instructions are parcels of 16 bits */
#define MYALIGN
#define MYINSTRING
#define ALSTACK		64
#define ALMAX		64

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

/* char is signed */
#undef	CHAR_UNSIGNED
#define	BOOL_TYPE	UCHAR	/* what used to store _Bool */

typedef	long long CONSZ;
typedef	unsigned long long U_CONSZ;
typedef long long OFFSZ;

#define CONFMT	"%lld"		/* format for printing constants */
#define LABFMT	".L%d"		/* format for printing labels */
#define LOCLAB	".L"
#define	STABLBL	".LL%d"		/* format for stab (debugging) labels */

#undef	FIELDOPS		/* no bit-field instructions used */
#define	TARGET_ENDIAN TARGET_LE	/* little-endian only */
#define	CC_DIV_0		/* division by zero is safe in the compiler */
#define STACK_DOWN		/* automatics are below the frame pointer */

/* ret address at 8(fp), the first stack argument at 16(fp) */
#define ARGINIT		128	/* # bits above fp where arguments start */
#define AUTOINIT	0	/* # bits below fp where automatics start */

#define BYTEOFF(x)	((x)&07)
#define wdal(k)		(BYTEOFF(k)==0)

#define STOARG(p)
#define STOFARG(p)
#define STOSTARG(p)
#define genfcall(a,b)	gencall(a,b)

/* How many 32-bit words does a value of this type take (used for stack allocation) */
#define	szty(t)	(t < LONG || t == FLOAT ? 1 : 2)

#define	R0	0
#define	R1	1
#define	R2	2
#define	R3	3
#define	R4	4
#define	R5	5
#define	R6	6
#define	R7	7
#define	R8	8
#define	R9	9
#define	R10	10
#define	R11	11
#define	R15	12
#define	FPREG	13
#define	STKREG	14
#define	SCRREG	15	/* r12 */

#define	MAXREGS	16

#define	RSTATUS	\
	SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG,	\
	SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG, SAREG|TEMPREG,	\
	SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG, SAREG|PERMREG,	\
	SAREG|PERMREG, 0, 0, 0,

/* no overlapping registers at all */
#define	ROVERLAP \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, \
	{ -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 }, { -1 },

#define PCLASS(p) (SAREG)

#define	NUMCLASS	1	/* highest number of reg classes used */

int COLORMAP(int c, int *r);
#define	GCLASS(x)	(CLASSA)
#define DECRA(x,y)	(((x) >> (y*8)) & 255)	/* decode encoded regs */
#define	ENCRD(x)	(x)			/* Encode dest reg in n_reg */
#define ENCRA1(x)	((x) << 8)		/* A1 */
#define ENCRA2(x)	((x) << 16)		/* A2 */
#define ENCRA(x,y)	((x) << (8+y*8))	/* encode regs in int */
#define	RETREG(x)	R0

#define	NARGREGS	6	/* r0-r5 carry the first six arguments */

/* Special shapes */
#define	SFUNCALL	(MAXSPECIAL+1)	/* struct assign after function call */
#define	SIMM		(MAXSPECIAL+2)	/* a constant an immediate holds (48 bits) */
#define	SSMALL		(MAXSPECIAL+3)	/* a constant in 12 bits: a displacement of the shortest form */
#define	SSH		(MAXSPECIAL+4)	/* a shift count 0..63 */
#define	SPCON		(MAXSPECIAL+5)	/* a positive constant */
#define	SSHP		(MAXSPECIAL+6)	/* a shift count 1..63 */
#define	SFPSP		(MAXSPECIAL+8)	/* the frame or the stack pointer (never clobbered) */
#define	SSYM		(MAXSPECIAL+7)	/* a constant with a symbol: an address */

#define	TARGET_FLT_EVAL_METHOD	0	/* all as their type */

/* floating point definitions */
#define USE_IEEEFP_32
#define FLT_PREFIX	IEEEFP_32
#define USE_IEEEFP_64
#define DBL_PREFIX	IEEEFP_64
#define LDBL_PREFIX	IEEEFP_64
#define DEFAULT_FPI_DEFS { &fpi_binary32, &fpi_binary64, &fpi_binary64 }

#define	COM	"\t; "

#define	TARGET_VALIST
#define	TARGET_STDARGS
#define	TARGET_BUILTINS							\
	{ "__builtin_stdarg_start", pdp64_builtin_stdarg_start,		\
						0, 2, 0, VOID },	\
	{ "__builtin_va_start", pdp64_builtin_stdarg_start,		\
						0, 2, 0, VOID },	\
	{ "__builtin_va_arg", pdp64_builtin_va_arg, BTNORVAL|BTNOPROTO,	\
							2, 0, 0 },	\
	{ "__builtin_va_end", pdp64_builtin_va_end, 0, 1, 0, VOID },	\
	{ "__builtin_va_copy", pdp64_builtin_va_copy, 0, 2, 0, VOID },
#ifdef LANG_CXX
#define P1ND struct node
#else
#define P1ND struct p1node
#endif
struct node;
struct bitable;
P1ND *pdp64_builtin_stdarg_start(const struct bitable *, P1ND *a);
P1ND *pdp64_builtin_va_arg(const struct bitable *, P1ND *a);
P1ND *pdp64_builtin_va_end(const struct bitable *, P1ND *a);
P1ND *pdp64_builtin_va_copy(const struct bitable *, P1ND *a);
#undef P1ND

#define	ATTR_MI_TARGET	ATTR_PDP64_STRRET

/* struct arguments and results: up to 16 bytes in one or two registers, else in memory */
#define	SC_REG	1
#define	SC_MEM	2
