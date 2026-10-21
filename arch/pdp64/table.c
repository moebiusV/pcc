/*	$Id$	*/
/*
 * pdp64 code tables.  One register class: every integer, pointer and floating-point value
 * lives in a general register; a value of a type narrower than 64 bits is kept extended
 * (see macdefs.h).  Instructions are two-address: "add AR,AL" is AL += AR, and the
 * result is the left register (RLEFT); the register allocator copies the left operand
 * first if it is still wanted.  Only quad operands can be in memory for the ALU
 * instructions, so the memory forms are for the 64-bit types.
 */

# include "pass2.h"
# include "pdp64-names.h"
# include "pdp64-limits.h"

#define	TW	(TINT|TUNSIGNED)		/* 32 bits */
#define	TSC	(TSHORT|TUSHORT|TCHAR|TUCHAR)	/* narrower */
#define	T64	(TLONG|TULONG|TLONGLONG|TULONGLONG|TPOINT)
#define	TFP	(TFLOAT|TDOUBLE|TLDOUBLE)
#define	TI32	(TW|TSC)			/* extended integers up to 32 bits */
#define	TANYI	(T64|TI32)
#define	TUS64	(TULONG|TULONGLONG|TPOINT)	/* unsigned 64 */
#define	TS64	(TLONG|TLONGLONG)

#define	XSL(c)	NEEDS(NREG(c, 1), NSL(c))
#define	XSLR(c)	NEEDS(NREG(c, 1), NSL(c), NSR(c))

#define	MQ	M_MOV_MOVQ
#define	ML	M_MOV_MOVL
#define	MW	M_MOV_MOVW
#define	MB	M_MOV_MOVB

/* a two-address operation: registers, and for 64-bit types memory, and a constant of an immediate's size */
#define	ALU(op, tw, mn) \
{ op,	INAREG|FOREFF,	SAREG, tw, SAREG, tw, 0, RLEFT, "\t" mn " AR,AL\nZY", }, \
{ op,	INAREG|FOREFF,	SAREG, tw, SIMM, TANY, 0, RLEFT, "\t" mn " AR,AL\nZY", },
#define	ALU64(op, tw, mn) \
{ op,	INAREG|FOREFF,	SAREG, tw, SOREG|SNAME, tw, 0, RLEFT, "\t" mn " AR,AL\nZY", },

/* the loads and stores of one size */
#define	LDST(ts, ld, st) \
{ UMUL,	INAREG, SANY, TANY, SOREG, ts, XSL(A), RESC1, "\t" ld " AL,A1\n", }, \
{ OPLTYPE, INAREG, SANY, TANY, SOREG|SNAME, ts, NEEDS(NREG(A, 1)), RESC1, "\t" ld " AR,A1\n", }, \
{ ASSIGN, FOREFF|INAREG, SOREG|SNAME, ts, SAREG, ts, 0, RDEST, "\t" st " AR,AL\n", }, \
{ ASSIGN, FOREFF|INAREG, SAREG, ts, SOREG|SNAME, ts, 0, RDEST, "\t" ld " AR,AL\n", },

/* conversion of a memory operand: the load takes the bytes of the (little-endian) low part */
#define	CVM(s, d, ld) \
{ SCONV, INAREG, SOREG|SNAME, s, SAREG, d, NEEDS(NREG(A, 1)), RESC1, "\t" ld " AL,A1\n", },

/* conversion of a register: the instruction that extends from the width of the result */
#define	CVR(s, d, mn) \
{ SCONV, INAREG, SAREG, s, SAREG, d, XSL(A), RESC1, "\t" mn " AL,A1\n", },

/* no instruction */
#define	CVN(s, d) \
{ SCONV, INAREG, SAREG, s, SAREG, d, 0, RLEFT, "", },

struct optab table[] = {
/* First entry must be an empty entry */
{ -1, FOREFF, SANY, TANY, SANY, TANY, 0, 0, "", },

/* PCONVs: a signed or narrow integer is extended already */
{ PCONV,	INAREG,
	SAREG,	TUNSIGNED,
	SAREG,	TPOINT,
		XSL(A),	RESC1,
		"	" M_MOVZ_MOVZL " AL,A1\n", },

{ PCONV,	INAREG,
	SAREG,	TANYI|TPOINT,
	SAREG,	TANYI|TPOINT,
		0,	RLEFT,
		"", },

/*
 * Integer conversions.  From memory the load does it.  A register is converted by
 * extending from the width of the result, unless the value is within the result's range
 * already.
 */
CVM(TCHAR|TUCHAR|TSHORT|TUSHORT|TW|T64, TCHAR, MB)
CVM(TCHAR|TUCHAR|TSHORT|TUSHORT|TW|T64, TUCHAR, M_MOVZ_MOVZB)
CVM(TSHORT|TUSHORT|TW|T64, TSHORT, MW)
CVM(TSHORT|TUSHORT|TW|T64, TUSHORT, M_MOVZ_MOVZW)
CVM(TW|T64, TW, ML)
CVM(TCHAR, TSHORT|TW|T64, MB)
CVM(TUCHAR, TSHORT|TUSHORT|TW|T64, M_MOVZ_MOVZB)
CVM(TSHORT, TW|T64, MW)
CVM(TUSHORT, TW|T64, M_MOVZ_MOVZW)
CVM(TINT, T64, ML)
CVM(TUNSIGNED, T64, M_MOVZ_MOVZL)

/* from a register: nothing */
CVN(T64, T64)
CVN(TW, TW)
CVN(TINT, T64)
CVN(TCHAR, TSHORT|TW|T64)
CVN(TUCHAR, TSHORT|TUSHORT|TW|T64)
CVN(TSHORT, TW|T64)
CVN(TUSHORT, TW|T64)

/* ... and the rest */
CVR(TANYI, TCHAR, MB)
CVR(TANYI, TUCHAR, M_MOVZ_MOVZB)
CVR(TANYI, TSHORT, MW)
CVR(TANYI, TUSHORT, M_MOVZ_MOVZW)
CVR(T64|TSHORT|TUSHORT|TCHAR|TUCHAR, TW, ML)
CVR(TUNSIGNED, T64, M_MOVZ_MOVZL)

/*
 * Floating point.  A double is the 64 bits of a register, a float the low 32 bits sign-extended.
 * The conversions go through the double for what has no direct instruction.
 */
#define	FADD(op, d, s) \
{ op,	INAREG|FOREFF,	SAREG, TDOUBLE|TLDOUBLE, SAREG|SOREG|SNAME, TDOUBLE|TLDOUBLE, 0, RLEFT, "\t" d " AR,AL\n", }, \
{ op,	INAREG|FOREFF,	SAREG, TFLOAT, SAREG, TFLOAT, 0, RLEFT, "\t" s " AR,AL\n", },

FADD(PLUS, M_ALU_FADDD, M_FESC_FADDS)
FADD(MINUS, M_ALU_FSUBD, M_FESC_FSUBS)
FADD(MUL, M_ALU_FMULD, M_FESC_FMULS)

{ DIV,		INAREG|FOREFF,
	SAREG,	TDOUBLE|TLDOUBLE,
	SAREG,	TDOUBLE|TLDOUBLE,
		0,	RLEFT,
		"	" M_FESC_FDIVD " AR,AL\n", },

{ DIV,		INAREG|FOREFF,
	SAREG,	TFLOAT,
	SAREG,	TFLOAT,
		0,	RLEFT,
		"	" M_FESC_FDIVS " AR,AL\n", },

{ UMINUS,	INAREG,
	SAREG,	TDOUBLE|TLDOUBLE,
	SANY,	TANY,
		XSL(A),	RESC1,
		"	" M_FESC_FNEG " AL,A1\n", },

/* a float is negated by flipping bit 31 and the extension of it: 33 sign bits */
{ UMINUS,	INAREG|FOREFF,
	SAREG,	TFLOAT,
	SANY,	TANY,
		0,	RLEFT,
		"	" M_ALU_XOR " #-2147483648,AL\n", },

{ OPLOG,	FORCC,
	SAREG,	TFLOAT|TDOUBLE|TLDOUBLE,
	SAREG,	TFLOAT|TDOUBLE|TLDOUBLE,
		0,	RESCC,
		"ZF", },

/* float <-> double */
{ SCONV,	INAREG,
	SAREG,	TFLOAT,
	SAREG,	TDOUBLE|TLDOUBLE,
		XSL(A),	RESC1,
		"	" M_FESC_FCVTSD " AL,A1\n", },

{ SCONV,	INAREG,
	SAREG,	TDOUBLE|TLDOUBLE,
	SAREG,	TFLOAT,
		XSL(A),	RESC1,
		"	" M_FESC_FCVTDS " AL,A1\n", },

{ SCONV,	INAREG,
	SAREG,	TDOUBLE|TLDOUBLE,
	SAREG,	TDOUBLE|TLDOUBLE,
		0,	RLEFT,
		"", },

/* integer to floating point */
{ SCONV,	INAREG,
	SAREG,	TINT,
	SAREG,	TFLOAT,
		XSL(A),	RESC1,
		"	" M_FESC_FCVTLS " AL,A1\n", },

{ SCONV,	INAREG,
	SAREG,	TINT|TS64,
	SAREG,	TDOUBLE|TLDOUBLE,
		XSL(A),	RESC1,
		"	" M_FESC_FCVTQD " AL,A1\n", },

{ SCONV,	INAREG,
	SAREG,	TS64,
	SAREG,	TFLOAT,
		XSL(A),	RESC1,
		"	" M_FESC_FCVTQD " AL,A1\n	" M_FESC_FCVTDS " A1,A1\n", },

{ SCONV,	INAREG,
	SAREG,	TUNSIGNED,
	SAREG,	TDOUBLE|TLDOUBLE,
		XSL(A),	RESC1,
		"	" M_MOVZ_MOVZL " AL,A1\n	" M_FESC_FCVTQD " A1,A1\n", },

{ SCONV,	INAREG,
	SAREG,	TUNSIGNED,
	SAREG,	TFLOAT,
		XSL(A),	RESC1,
		"	" M_MOVZ_MOVZL " AL,A1\n	" M_FESC_FCVTQD " A1,A1\n	" M_FESC_FCVTDS " A1,A1\n", },

{ SCONV,	INAREG,
	SAREG,	TUS64,
	SAREG,	TDOUBLE|TLDOUBLE,
		XSL(A),	RESC1,
		"	fcvtqud AL,A1\n", },

{ SCONV,	INAREG,
	SAREG,	TUS64,
	SAREG,	TFLOAT,
		XSL(A),	RESC1,
		"	fcvtqud AL,A1\n	" M_FESC_FCVTDS " A1,A1\n", },

/* floating point to integer: truncating */
{ SCONV,	INAREG,
	SAREG,	TFLOAT,
	SAREG,	TINT,
		XSL(A),	RESC1,
		"	" M_FESC_FCVTSL " AL,A1\n", },

{ SCONV,	INAREG,
	SAREG,	TDOUBLE|TLDOUBLE,
	SAREG,	TINT|TS64,
		XSL(A),	RESC1,
		"	" M_FESC_FCVTDQ " AL,A1\n", },

{ SCONV,	INAREG,
	SAREG,	TFLOAT,
	SAREG,	TS64,
		XSL(A),	RESC1,
		"	" M_FESC_FCVTSD " AL,A1\n	" M_FESC_FCVTDQ " A1,A1\n", },

{ SCONV,	INAREG,
	SAREG,	TDOUBLE|TLDOUBLE,
	SAREG,	TUNSIGNED,
	XSL(A),	RESC1,
		"	" M_FESC_FCVTDQ " AL,A1\n	" ML " A1,A1\n", },

{ SCONV,	INAREG,
	SAREG,	TFLOAT,
	SAREG,	TUNSIGNED,
		XSL(A),	RESC1,
		"	" M_FESC_FCVTSD " AL,A1\n	" M_FESC_FCVTDQ " A1,A1\n	" ML " A1,A1\n", },

{ SCONV,	INAREG,
	SAREG,	TDOUBLE|TLDOUBLE,
	SAREG,	TUS64,
		XSL(A),	RESC1,
		"	fcvtdqu AL,A1\n", },

{ SCONV,	INAREG,
	SAREG,	TFLOAT,
	SAREG,	TUS64,
		XSL(A),	RESC1,
		"	" M_FESC_FCVTSD " AL,A1\n	fcvtdqu A1,A1\n", },

/* +0.0 */
{ FCON,		INAREG,
	SANY,	TFLOAT|TDOUBLE|TLDOUBLE,
	SANY,	TFLOAT|TDOUBLE|TLDOUBLE,
		NEEDS(NREG(A, 1)),	RESC1,
		"	" M_SOP_CLR " A1\n", },

/*
 * Subroutine calls.
 */
{ CALL,		FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	0,
		"	" M_SOP_CALL " ZC\n", },

{ UCALL,	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	0,
		"	" M_SOP_CALL " ZC\n", },

{ CALL,		INAREG,
	SCON,	TANY,
	SAREG,	TANYI|T64|TFP,
		XSL(A),	RESC1,	/* should be 0 */
		"	" M_SOP_CALL " ZC\n", },

{ UCALL,	INAREG,
	SCON,	TANY,
	SAREG,	TANYI|T64|TFP,
		XSL(A),	RESC1,	/* should be 0 */
		"	" M_SOP_CALL " ZC\n", },

/* through a pointer in memory */
{ CALL,		FOREFF,
	SOREG|SNAME,	T64,
	SANY,	TANY,
		0,	0,
		"	" M_XFER_CALLI " AL\n", },

{ UCALL,	FOREFF,
	SOREG|SNAME,	T64,
	SANY,	TANY,
		0,	0,
		"	" M_XFER_CALLI " AL\n", },

{ CALL,		INAREG,
	SOREG|SNAME,	T64,
	SAREG,	TANYI|T64|TFP,
		XSL(A),	RESC1,	/* should be 0 */
		"	" M_XFER_CALLI " AL\n", },

{ UCALL,	INAREG,
	SOREG|SNAME,	T64,
	SAREG,	TANYI|T64|TFP,
		XSL(A),	RESC1,	/* should be 0 */
		"	" M_XFER_CALLI " AL\n", },

/* through a pointer in a register: call to the address */
{ CALL,		FOREFF,
	SAREG,	TANY,
	SANY,	TANY,
		0,	0,
		"	" M_SOP_CALL " (AL)\n", },

{ UCALL,	FOREFF,
	SAREG,	TANY,
	SANY,	TANY,
		0,	0,
		"	" M_SOP_CALL " (AL)\n", },

{ CALL,		INAREG,
	SAREG,	TANY,
	SAREG,	TANYI|T64|TFP,
		XSL(A),	RESC1,	/* should be 0 */
		"	" M_SOP_CALL " (AL)\n", },

{ UCALL,	INAREG,
	SAREG,	TANY,
	SAREG,	TANYI|T64|TFP,
		XSL(A),	RESC1,	/* should be 0 */
		"	" M_SOP_CALL " (AL)\n", },

/* struct return: r0 is the address of the result, or the result is in r0:r1 (ZR) */
{ USTCALL,	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	0,
		"	" M_SOP_CALL " ZC\n", },

{ USTCALL,	INAREG,
	SCON,	TANY,
	SANY,	TANY,
		XSL(A),	RESC1,	/* should be 0 */
		"	" M_SOP_CALL " ZC\nZR", },

{ USTCALL,	INAREG,
	SAREG,	TANY,
	SANY,	TANY,
		XSL(A),	RESC1,	/* should be 0 */
		"	" M_SOP_CALL " (AL)\nZR", },

{ STCALL,	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	0,
		"	" M_SOP_CALL " ZC\n", },

{ STCALL,	INAREG,
	SCON,	TANY,
	SANY,	TANY,
		XSL(A),	RESC1,	/* should be 0 */
		"	" M_SOP_CALL " ZC\nZR", },

{ STCALL,	INAREG,
	SAREG,	TANY,
	SANY,	TANY,
		XSL(A),	RESC1,	/* should be 0 */
		"	" M_SOP_CALL " (AL)\nZR", },

/*
 * Arithmetic.  A result of 32 bits is extended again (ZY/ZX) unless -mnowrap.
 */
/* an address and a constant: a three-address lea */
{ PLUS,		INAREG,
	SAREG,	T64,
	SSMALL,	TANY,
		XSL(A),	RESC1,
		"ZA", },

{ PLUS,		INAREG,
	SAREG,	TW,
	SSMALL,	TANY,
		XSL(A),	RESC1,
		"ZAZX", },

{ PLUS,		INAREG,
	SFPSP,	T64,
	SIMM,	TANY,
		NEEDS(NREG(A, 1)),	RESC1,
		"ZA", },

ALU(PLUS, T64, M_ALU_ADD)
ALU(PLUS, TW, M_ALU_ADD)
ALU64(PLUS, T64, M_ALU_ADD)

/* an address of a symbol and a register */
{ PLUS,		INAREG,
	SAREG,	T64,
	SSYM,	TANY,
		NEEDS(NREG(A, 1)),	RESC1,
		"ZN", },

{ MINUS,	INAREG,
	SAREG,	T64,
	SSMALL,	TANY,
		XSL(A),	RESC1,
		"ZaZX", },

{ MINUS,	INAREG,
	SAREG,	TW,
	SSMALL,	TANY,
		XSL(A),	RESC1,
		"ZaZX", },

{ MINUS,	INAREG,
	SFPSP,	T64,
	SIMM,	TANY,
		NEEDS(NREG(A, 1)),	RESC1,
		"Za", },

ALU(MINUS, T64, M_ALU_SUB)
ALU(MINUS, TW, M_ALU_SUB)
ALU64(MINUS, T64, M_ALU_SUB)

ALU(MUL, T64, M_ALU_MUL)
ALU(MUL, TW, M_ALU_MUL)
ALU64(MUL, T64, M_ALU_MUL)

ALU(AND, T64, M_ALU_AND)
ALU(AND, TI32, M_ALU_AND)
ALU64(AND, T64, M_ALU_AND)
ALU(OR, T64, M_ALU_BIS)
ALU(OR, TI32, M_ALU_BIS)
ALU64(OR, T64, M_ALU_BIS)
ALU(ER, T64, M_ALU_XOR)
ALU(ER, TI32, M_ALU_XOR)
ALU64(ER, T64, M_ALU_XOR)

/*
 * Division is signed only when both operands are signed; one unsigned
 * operand makes it unsigned (folded constants can keep different types).
 */
{ DIV,		INAREG,
	SAREG,	TINT|TS64,
	SAREG,	TINT|TS64,
		XSL(A),	RESC1,
		"	" M_AESC_DIV " AL,AR,A1\n", },

{ DIV,		INAREG,
	SAREG,	TUS64,
	SAREG,	T64|TW,
		XSL(A),	RESC1,
		"	" M_AESC_DIVU " AL,AR,A1\n", },

{ DIV,		INAREG,
	SAREG,	TS64|TW,
	SAREG,	TUS64,
		XSL(A),	RESC1,
		"	" M_AESC_DIVU " AL,AR,A1\n", },

/* unsigned int: the operands zero-extended, the quotient extended again */
{ DIV,		INAREG,
	SAREG,	TW,
	SAREG,	TUNSIGNED,
		NEEDS(NREG(A, 2)),	RESC1,
		"	" M_MOVZ_MOVZL " AL,A1\n	" M_MOVZ_MOVZL " AR,A2\n	" M_AESC_DIVU " A1,A2,A1\n	" ML " A1,A1\n", },

{ DIV,		INAREG,
	SAREG,	TUNSIGNED,
	SAREG,	TINT,
		NEEDS(NREG(A, 2)),	RESC1,
		"	" M_MOVZ_MOVZL " AL,A1\n	" M_MOVZ_MOVZL " AR,A2\n	" M_AESC_DIVU " A1,A2,A1\n	" ML " A1,A1\n", },

{ MOD,		INAREG,
	SAREG,	TINT|TS64,
	SAREG,	TINT|TS64,
		XSL(A),	RESC1,
		"	" M_AESC_MOD " AL,AR,A1\n", },

{ MOD,		INAREG,
	SAREG,	TUS64,
	SAREG,	T64|TW,
		XSL(A),	RESC1,
		"	" M_AESC_MODU " AL,AR,A1\n", },

{ MOD,		INAREG,
	SAREG,	TS64|TW,
	SAREG,	TUS64,
		XSL(A),	RESC1,
		"	" M_AESC_MODU " AL,AR,A1\n", },

{ MOD,		INAREG,
	SAREG,	TW,
	SAREG,	TUNSIGNED,
		NEEDS(NREG(A, 2)),	RESC1,
		"	" M_MOVZ_MOVZL " AL,A1\n	" M_MOVZ_MOVZL " AR,A2\n	" M_AESC_MODU " A1,A2,A1\n	" ML " A1,A1\n", },

{ MOD,		INAREG,
	SAREG,	TUNSIGNED,
	SAREG,	TINT,
		NEEDS(NREG(A, 2)),	RESC1,
		"	" M_MOVZ_MOVZL " AL,A1\n	" M_MOVZ_MOVZL " AR,A2\n	" M_AESC_MODU " A1,A2,A1\n	" ML " A1,A1\n", },

/*
 * Shifts.  The count is a register or a constant.
 */
{ LS,		INAREG|FOREFF,
	SAREG,	T64|TW,
	SSH,	TANY,
		0,	RLEFT,
		"	" M_ALU_SHL " AR,AL\nZY", },

{ LS,		INAREG|FOREFF,
	SAREG,	T64|TW,
	SAREG,	TANYI,
		0,	RLEFT,
		"	" M_ALU_SHL " AR,AL\nZY", },

{ RS,		INAREG|FOREFF,
	SAREG,	TINT|TS64,
	SSH,	TANY,
		0,	RLEFT,
		"	" M_ALU_SAR " AR,AL\n", },

{ RS,		INAREG|FOREFF,
	SAREG,	TINT|TS64,
	SAREG,	TANYI,
		0,	RLEFT,
		"	" M_ALU_SAR " AR,AL\n", },

{ RS,		INAREG|FOREFF,
	SAREG,	TUS64,
	SSH,	TANY,
		0,	RLEFT,
		"	" M_ALU_SHR " AR,AL\n", },

{ RS,		INAREG|FOREFF,
	SAREG,	TUS64,
	SAREG,	TANYI,
		0,	RLEFT,
		"	" M_ALU_SHR " AR,AL\n", },

/* unsigned int: zero-extend first; a count of 0 would leave it so, hence ZY unless the count is at least 1 */
{ RS,		INAREG|FOREFF,
	SAREG,	TUNSIGNED,
	SSHP,	TANY,
		0,	RLEFT,
		"	" M_MOVZ_MOVZL " AL,AL\n	" M_ALU_SHR " AR,AL\n", },

{ RS,		INAREG|FOREFF,
	SAREG,	TUNSIGNED,
	SAREG,	TANYI,
		0,	RLEFT,
		"	" M_MOVZ_MOVZL " AL,AL\n	" M_ALU_SHR " AR,AL\n	" ML " AL,AL\n", },

/*
 * Unary operators.
 */
{ UMINUS,	INAREG|FOREFF,
	SAREG,	T64|TW,
	SANY,	TANY,
		0,	RLEFT,
		"	" M_SOP_NEG " AL\nZY", },

{ COMPL,	INAREG|FOREFF,
	SAREG,	T64|TI32,
	SANY,	TANY,
		0,	RLEFT,
		"	" M_SOP_COM " AL\n", },

/*
 * Indirection (loads) and assignment, by size.
 */
LDST(T64|TFP&~TFLOAT, MQ, MQ)
LDST(TINT|TUNSIGNED|TFLOAT, ML, ML)
LDST(TSHORT, MW, MW)
LDST(TCHAR, MB, MB)

/* the zero-extending loads have no store of their own */
{ UMUL,		INAREG,
	SANY,	TANY,
	SOREG,	TUSHORT,
		XSL(A),	RESC1,
		"	" M_MOVZ_MOVZW " AL,A1\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SOREG|SNAME,	TUSHORT,
		NEEDS(NREG(A, 1)),	RESC1,
		"	" M_MOVZ_MOVZW " AR,A1\n", },

{ ASSIGN,	FOREFF|INAREG,
	SOREG|SNAME,	TUSHORT,
	SAREG,		TUSHORT,
		0,	RDEST,
		"	" MW " AR,AL\n", },

{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TUSHORT,
	SOREG|SNAME,	TUSHORT,
		0,	RDEST,
		"	" M_MOVZ_MOVZW " AR,AL\n", },

{ UMUL,		INAREG,
	SANY,	TANY,
	SOREG,	TUCHAR,
		XSL(A),	RESC1,
		"	" M_MOVZ_MOVZB " AL,A1\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SOREG|SNAME,	TUCHAR,
		NEEDS(NREG(A, 1)),	RESC1,
		"	" M_MOVZ_MOVZB " AR,A1\n", },

{ ASSIGN,	FOREFF|INAREG,
	SOREG|SNAME,	TUCHAR,
	SAREG,		TUCHAR,
		0,	RDEST,
		"	" MB " AR,AL\n", },

{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TUCHAR,
	SOREG|SNAME,	TUCHAR,
		0,	RDEST,
		"	" M_MOVZ_MOVZB " AR,AL\n", },

/* zero to memory: 64 bits at a time */
{ ASSIGN,	FOREFF,
	SOREG|SNAME,	T64,
	SZERO,		TANY,
		0,	0,
		"	" M_SOP_CLR " AL\n", },

/* register to register, constant to register */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,	TANYI|T64|TFP,
	SAREG,	TANYI|T64|TFP,
		0,	RDEST,
		"	" MQ " AR,AL\n", },

{ ASSIGN,	FOREFF|INAREG,
	SAREG,	TANYI|T64|TFP,
	SCON,	TANY,
		0,	RDEST,
		"Zk", },

/* struct assignment: the right side is a pointer to the source */
{ STASG,	INAREG|FOREFF,
	SOREG|SNAME,	TANY,
	SAREG,		TPTRTO|TANY,
		NEEDS(NREG(A, 3)),	RDEST,
		"ZSFZT", },

/*
 * Comparisons.  cbgen() prints the branch.
 */
{ OPLOG,	FORCC,
	SAREG,	TANYI|T64,
	SZERO,	TANY,
		0,	RESCC,
		"	" M_SOP_TST " AL\n", },

{ OPLOG,	FORCC,
	SAREG,	TANYI|T64,
	SAREG,	TANYI|T64,
		0,	RESCC,
		"	" M_ALU_CMP " AL,AR\n", },

{ OPLOG,	FORCC,
	SAREG,	TANYI|T64,
	SIMM,	TANY,
		0,	RESCC,
		"	" M_ALU_CMP " AL,AR\n", },

{ OPLOG,	FORCC,
	SAREG,	T64,
	SOREG|SNAME,	T64,
		0,	RESCC,
		"	" M_ALU_CMP " AL,AR\n", },

{ OPLOG,	FORCC,
	SANY,	TANY,
	SANY,	TANY,
		NEEDS(NREWRITE),	0,
		"diediedie!", },

/*
 * Jumps.
 */
{ GOTO,		FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	" M_BRANCH_BR " LL\n", },

{ GOTO,		FOREFF,
	SAREG,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	" M_SOP_JMP " (AL)\n", },

/*
 * Convert LTYPE to reg.
 */
{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SCON,	TANYI|T64|TFP,
		NEEDS(NREG(A, 1)),	RESC1,
		"ZK", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SAREG,	TANYI|T64|TFP,
		NEEDS(NREG(A, 1)),	RESC1,
		"	" MQ " AL,A1\n", },

{ ADDROF,	INAREG,
	SNAME,	TANY,
	SANY,	TANY,
		NEEDS(NREG(A, 1)), RESC1,
		"	" M_LEA_LEA " AL,A1\n", },

# define DF(x) FORREW,SANY,TANY,SANY,TANY,NEEDS(NREWRITE),x,""

{ UMUL, DF( UMUL ), },

{ ASSIGN, DF(ASSIGN), },

{ STASG, DF(STASG), },

{ FLD, DF(FLD), },

{ OPLEAF, DF(NAME), },

{ OPUNARY, DF(UMINUS), },

{ OPANY, DF(BITYPE), },

{ FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	0,	FREE,	"help; I'm in trouble\n" },
};

int tablesize = sizeof(table)/sizeof(table[0]);
