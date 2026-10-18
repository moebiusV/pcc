/*      $Id$    */
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

# include "pass2.h"

#define	TW	(TINT|TUNSIGNED)		/* 32-bit, in a w register */
#define	TSC	(TSHORT|TUSHORT|TCHAR|TUCHAR)	/* narrow: the low bits count */
#define	T32	(TW|TSC)
#define	T64	(TLONG|TULONG|TLONGLONG|TULONGLONG|TPOINT)
#define	TFP	(TFLOAT|TDOUBLE|TLDOUBLE)

#define	XSL(c)	NEEDS(NREG(c, 1), NSL(c))
#define	XSLR(c)	NEEDS(NREG(c, 1), NSL(c), NSR(c))

/*
 * Printing: A1 and AL are registers named by the width of the node's type
 * (w0 for int, x0 for long and pointers, s0 for float, d0 for double);
 * I1 and IL are the 32-bit names (w0, s0) and U1, UL, UR the 64-bit ones
 * (x0, d0) whatever the type.  Every template that touches memory starts
 * with ZP, which loads x16 for an address that does not fit the instruction.
 */

/* a three-register operation and its immediate forms */
#define	RRR(op, ts, mn) \
{ op,	INAREG, SAREG, ts, SAREG, ts, XSL(A), RESC1, "\t" mn " A1,AL,AR\n", },
#define	RRC(op, ts, mn, sh) \
{ op,	INAREG, SAREG, ts, sh, TANY, XSL(A), RESC1, "\t" mn " A1,AL,#CR\n", },

/* the loads and stores of one size */
#define	LDST(ts, ld, st) \
{ UMUL,	INAREG, SANY, TANY, SOREG, ts, XSL(A), RESC1, "ZP\t" ld " A1,AL\n", }, \
{ OPLTYPE, INAREG, SANY, TANY, SOREG|SNAME, ts, XSL(A), RESC1, "ZP\t" ld " A1,AL\n", }, \
{ ASSIGN, FOREFF|INAREG, SOREG|SNAME, ts, SAREG, ts, 0, RDEST, "ZP\t" st " AR,AL\n", }, \
{ ASSIGN, FOREFF|INAREG, SAREG, ts, SOREG|SNAME, ts, 0, RDEST, "ZP\t" ld " AL,AR\n", },

struct optab table[] = {
/* First entry must be an empty entry */
{ -1, FOREFF, SANY, TANY, SANY, TANY, 0, 0, "", },

/* PCONVs are usually not necessary */
{ PCONV,	INAREG,
	SAREG,	T64,
	SAREG,	T64,
		0,	RLEFT,
		"", },

{ PCONV,	INAREG,
	SAREG,	TINT,
	SAREG,	TPOINT,
		XSL(A),	RESC1,
		"	sxtw A1,IL\n", },

{ PCONV,	INAREG,
	SAREG,	TUNSIGNED,
	SAREG,	TPOINT,
		XSL(A),	RESC1,
		"	mov I1,IL\n", },

{ PCONV,	INAREG,
	SAREG,	TCHAR,
	SAREG,	TPOINT,
		XSL(A),	RESC1,
		"	sxtb A1,IL\n", },

{ PCONV,	INAREG,
	SAREG,	TUCHAR,
	SAREG,	TPOINT,
		XSL(A),	RESC1,
		"	uxtb I1,IL\n", },

{ PCONV,	INAREG,
	SAREG,	TSHORT,
	SAREG,	TPOINT,
		XSL(A),	RESC1,
		"	sxth A1,IL\n", },

{ PCONV,	INAREG,
	SAREG,	TUSHORT,
	SAREG,	TPOINT,
		XSL(A),	RESC1,
		"	uxth I1,IL\n", },

/*
 * Integer conversions.  A value in a register is only right in the bits
 * of its type, so a conversion to a smaller or equal type does nothing
 * and one to a larger type extends from the smaller.
 */
{ SCONV,	INAREG,
	SAREG,	T64,
	SAREG,	T64|T32,
		0,	RLEFT,
		"", },

{ SCONV,	INAREG,
	SAREG,	TW,
	SAREG,	T32,
		0,	RLEFT,
		"", },

{ SCONV,	INAREG,
	SAREG,	TSHORT|TUSHORT,
	SAREG,	TSHORT|TUSHORT|TCHAR|TUCHAR,
		0,	RLEFT,
		"", },

{ SCONV,	INAREG,
	SAREG,	TCHAR|TUCHAR,
	SAREG,	TCHAR|TUCHAR,
		0,	RLEFT,
		"", },

/* from memory, extending as it loads */
{ SCONV,	INAREG,
	SOREG|SNAME,	TCHAR,
	SAREG,		TSHORT|TUSHORT|TW|T64,
		XSL(A),	RESC1,
		"ZP	ldrsb A1,AL\n", },

{ SCONV,	INAREG,
	SOREG|SNAME,	TUCHAR,
	SAREG,		TSHORT|TUSHORT|TW|T64,
		XSL(A),	RESC1,
		"ZP	ldrb I1,AL\n", },

{ SCONV,	INAREG,
	SOREG|SNAME,	TSHORT,
	SAREG,		TW|T64,
		XSL(A),	RESC1,
		"ZP	ldrsh A1,AL\n", },

{ SCONV,	INAREG,
	SOREG|SNAME,	TUSHORT,
	SAREG,		TW|T64,
		XSL(A),	RESC1,
		"ZP	ldrh I1,AL\n", },

{ SCONV,	INAREG,
	SOREG|SNAME,	TINT,
	SAREG,		T64,
		XSL(A),	RESC1,
		"ZP	ldrsw A1,AL\n", },

{ SCONV,	INAREG,
	SOREG|SNAME,	TUNSIGNED,
	SAREG,		T64,
		XSL(A),	RESC1,
		"ZP	ldr I1,AL\n", },

/* from a register */
{ SCONV,	INAREG,
	SAREG,	TCHAR,
	SAREG,	TSHORT|TUSHORT|TW|T64,
		XSL(A),	RESC1,
		"	sxtb A1,IL\n", },

{ SCONV,	INAREG,
	SAREG,	TUCHAR,
	SAREG,	TSHORT|TUSHORT|TW|T64,
		XSL(A),	RESC1,
		"	uxtb I1,IL\n", },

{ SCONV,	INAREG,
	SAREG,	TSHORT,
	SAREG,	TW|T64,
		XSL(A),	RESC1,
		"	sxth A1,IL\n", },

{ SCONV,	INAREG,
	SAREG,	TUSHORT,
	SAREG,	TW|T64,
		XSL(A),	RESC1,
		"	uxth I1,IL\n", },

{ SCONV,	INAREG,
	SAREG,	TINT,
	SAREG,	T64,
		XSL(A),	RESC1,
		"	sxtw A1,IL\n", },

{ SCONV,	INAREG,
	SAREG,	TUNSIGNED,
	SAREG,	T64,
		XSL(A),	RESC1,
		"	mov I1,IL\n", },

/*
 * Floating point conversions.  Types smaller than int are converted
 * to int first (clocal).
 */
{ SCONV,	INBREG,
	SBREG,	TFLOAT,
	SBREG,	TFLOAT,
		0,	RLEFT,
		"", },

{ SCONV,	INBREG,
	SBREG,	TDOUBLE|TLDOUBLE,
	SBREG,	TDOUBLE|TLDOUBLE,
		0,	RLEFT,
		"", },

{ SCONV,	INBREG,
	SBREG,	TFLOAT,
	SBREG,	TDOUBLE|TLDOUBLE,
		XSL(B),	RESC1,
		"	fcvt A1,AL\n", },

{ SCONV,	INBREG,
	SBREG,	TDOUBLE|TLDOUBLE,
	SBREG,	TFLOAT,
		XSL(B),	RESC1,
		"	fcvt A1,AL\n", },

{ SCONV,	INBREG,
	SAREG,	TINT|TLONG,
	SBREG,	TFP,
		NEEDS(NREG(B, 1)),	RESC1,
		"	scvtf A1,AL\n", },

{ SCONV,	INBREG,
	SAREG,	TUNSIGNED|TULONG,
	SBREG,	TFP,
		NEEDS(NREG(B, 1)),	RESC1,
		"	ucvtf A1,AL\n", },

{ SCONV,	INAREG,
	SBREG,	TFP,
	SAREG,	TINT|TLONG,
		NEEDS(NREG(A, 1)),	RESC1,
		"	fcvtzs A1,AL\n", },

{ SCONV,	INAREG,
	SBREG,	TFP,
	SAREG,	TUNSIGNED|TULONG,
		NEEDS(NREG(A, 1)),	RESC1,
		"	fcvtzu A1,AL\n", },

/*
 * Subroutine calls.
 */
{ CALL,		FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	0,
		"	bl CL\n", },

{ UCALL,	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	0,
		"	bl CL\n", },

{ CALL,		INAREG,
	SCON,	TANY,
	SAREG,	T64|T32,
		XSL(A),	RESC1,	/* should be 0 */
		"	bl CL\n", },

{ UCALL,	INAREG,
	SCON,	TANY,
	SAREG,	T64|T32,
		XSL(A),	RESC1,	/* should be 0 */
		"	bl CL\n", },

{ CALL,		INBREG,
	SCON,	TANY,
	SBREG,	TFP,
		XSL(B),	RESC1,	/* should be 0 */
		"	bl CL\n", },

{ UCALL,	INBREG,
	SCON,	TANY,
	SBREG,	TFP,
		XSL(B),	RESC1,	/* should be 0 */
		"	bl CL\n", },

{ CALL,		FOREFF,
	SAREG,	TANY,
	SANY,	TANY,
		0,	0,
		"	blr UL\n", },

{ UCALL,	FOREFF,
	SAREG,	TANY,
	SANY,	TANY,
		0,	0,
		"	blr UL\n", },

{ CALL,		INAREG,
	SAREG,	TANY,
	SAREG,	T64|T32,
		XSL(A),	RESC1,	/* should be 0 */
		"	blr UL\n", },

{ UCALL,	INAREG,
	SAREG,	TANY,
	SAREG,	T64|T32,
		XSL(A),	RESC1,	/* should be 0 */
		"	blr UL\n", },

{ CALL,		INBREG,
	SAREG,	TANY,
	SBREG,	TFP,
		XSL(B),	RESC1,	/* should be 0 */
		"	blr UL\n", },

{ UCALL,	INBREG,
	SAREG,	TANY,
	SBREG,	TFP,
		XSL(B),	RESC1,	/* should be 0 */
		"	blr UL\n", },

/* struct return: x8 gets the address of the space for the result */
{ USTCALL,	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	0,
		"ZB	bl CL\n", },

{ USTCALL,	INAREG,
	SCON,	TANY,
	SANY,	TANY,
		XSL(A),	RESC1,	/* should be 0 */
		"ZB	bl CL\n", },

{ STCALL,	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	0,
		"ZB	bl CL\n", },

{ STCALL,	INAREG,
	SCON,	TANY,
	SANY,	TANY,
		XSL(A),	RESC1,	/* should be 0 */
		"ZB	bl CL\n", },

/*
 * Arithmetic.
 */
RRR(PLUS, T32, "add")
RRR(PLUS, T64, "add")
RRC(PLUS, T32, "add", SIMM12)
RRC(PLUS, T64, "add", SIMM12)

{ PLUS,		INAREG,
	SAREG,	T32,
	SNIMM12,	TANY,
		XSL(A),	RESC1,
		"	sub A1,AL,#ZN\n", },

{ PLUS,		INAREG,
	SAREG,	T64,
	SNIMM12,	TANY,
		XSL(A),	RESC1,
		"	sub A1,AL,#ZN\n", },

/* an address and a constant: frame, symbol, or one too big for an add */
{ PLUS,		INAREG,
	SAREG,	T64,
	SCON,	TANY,
		NEEDS(NREG(A, 1), NSL(A)),	RESC1,
		"ZA", },

{ PLUS,		INBREG,
	SBREG,	TFP,
	SBREG,	TFP,
		XSL(B),	RESC1,
		"	fadd A1,AL,AR\n", },

RRR(MINUS, T32, "sub")
RRR(MINUS, T64, "sub")
RRC(MINUS, T32, "sub", SIMM12)
RRC(MINUS, T64, "sub", SIMM12)

{ MINUS,	INAREG,
	SAREG,	T32,
	SNIMM12,	TANY,
		XSL(A),	RESC1,
		"	add A1,AL,#ZN\n", },

{ MINUS,	INAREG,
	SAREG,	T64,
	SNIMM12,	TANY,
		XSL(A),	RESC1,
		"	add A1,AL,#ZN\n", },

{ MINUS,	INBREG,
	SBREG,	TFP,
	SBREG,	TFP,
		XSL(B),	RESC1,
		"	fsub A1,AL,AR\n", },

RRR(MUL, T32, "mul")
RRR(MUL, T64, "mul")

{ MUL,		INBREG,
	SBREG,	TFP,
	SBREG,	TFP,
		XSL(B),	RESC1,
		"	fmul A1,AL,AR\n", },

/*
 * Division is signed only when both operands are signed; one unsigned
 * operand makes it unsigned (folded constants can keep different types).
 */
RRR(DIV, TINT, "sdiv")
RRR(DIV, TLONG|TLONGLONG, "sdiv")
{ DIV,		INAREG,
	SAREG,	TUNSIGNED,
	SAREG,	TW,
		XSL(A),	RESC1,
		"	udiv A1,AL,AR\n", },

{ DIV,		INAREG,
	SAREG,	TINT,
	SAREG,	TUNSIGNED,
		XSL(A),	RESC1,
		"	udiv A1,AL,AR\n", },

{ DIV,		INAREG,
	SAREG,	TULONG|TULONGLONG|TPOINT,
	SAREG,	T64,
		XSL(A),	RESC1,
		"	udiv A1,AL,AR\n", },

{ DIV,		INAREG,
	SAREG,	T64,
	SAREG,	TULONG|TULONGLONG|TPOINT,
		XSL(A),	RESC1,
		"	udiv A1,AL,AR\n", },

{ DIV,		INBREG,
	SBREG,	TFP,
	SBREG,	TFP,
		XSL(B),	RESC1,
		"	fdiv A1,AL,AR\n", },

{ MOD,		INAREG,
	SAREG,	TINT|TLONG|TLONGLONG,
	SAREG,	TINT|TLONG|TLONGLONG,
		NEEDS(NREG(A, 2)),	RESC1,
		"	sdiv A2,AL,AR\n	msub A1,A2,AR,AL\n", },

{ MOD,		INAREG,
	SAREG,	TUNSIGNED,
	SAREG,	TW,
		NEEDS(NREG(A, 2)),	RESC1,
		"	udiv A2,AL,AR\n	msub A1,A2,AR,AL\n", },

{ MOD,		INAREG,
	SAREG,	TINT,
	SAREG,	TUNSIGNED,
		NEEDS(NREG(A, 2)),	RESC1,
		"	udiv A2,AL,AR\n	msub A1,A2,AR,AL\n", },

{ MOD,		INAREG,
	SAREG,	TULONG|TULONGLONG|TPOINT,
	SAREG,	T64,
		NEEDS(NREG(A, 2)),	RESC1,
		"	udiv A2,AL,AR\n	msub A1,A2,AR,AL\n", },

{ MOD,		INAREG,
	SAREG,	T64,
	SAREG,	TULONG|TULONGLONG|TPOINT,
		NEEDS(NREG(A, 2)),	RESC1,
		"	udiv A2,AL,AR\n	msub A1,A2,AR,AL\n", },

/* AND/OR/ER */
RRR(AND, T32, "and")
RRR(AND, T64, "and")
RRC(AND, T32, "and", SLOGIMM)
RRC(AND, T64, "and", SLOGIMM64)
RRR(OR, T32, "orr")
RRR(OR, T64, "orr")
RRC(OR, T32, "orr", SLOGIMM)
RRC(OR, T64, "orr", SLOGIMM64)
RRR(ER, T32, "eor")
RRR(ER, T64, "eor")
RRC(ER, T32, "eor", SLOGIMM)
RRC(ER, T64, "eor", SLOGIMM64)

/* the immediate logical forms print their operand in hex */

/*
 * Shifts.  The count register is named in the width of the value.
 */
{ LS,		INAREG,
	SAREG,	T32,
	SAREG,	TANY,
		XSL(A),	RESC1,
		"	lsl A1,AL,IR\n", },

{ LS,		INAREG,
	SAREG,	T64,
	SAREG,	TANY,
		XSL(A),	RESC1,
		"	lsl A1,AL,UR\n", },

{ LS,		INAREG,
	SAREG,	T32,
	SSH32,	TANY,
		XSL(A),	RESC1,
		"	lsl A1,AL,#CR\n", },

{ LS,		INAREG,
	SAREG,	T64,
	SSH64,	TANY,
		XSL(A),	RESC1,
		"	lsl A1,AL,#CR\n", },

{ RS,		INAREG,
	SAREG,	TINT,
	SAREG,	TANY,
		XSL(A),	RESC1,
		"	asr A1,AL,IR\n", },

{ RS,		INAREG,
	SAREG,	TUNSIGNED,
	SAREG,	TANY,
		XSL(A),	RESC1,
		"	lsr A1,AL,IR\n", },

{ RS,		INAREG,
	SAREG,	TLONG|TLONGLONG,
	SAREG,	TANY,
		XSL(A),	RESC1,
		"	asr A1,AL,UR\n", },

{ RS,		INAREG,
	SAREG,	TULONG|TULONGLONG|TPOINT,
	SAREG,	TANY,
		XSL(A),	RESC1,
		"	lsr A1,AL,UR\n", },

{ RS,		INAREG,
	SAREG,	TINT,
	SSH32,	TANY,
		XSL(A),	RESC1,
		"	asr A1,AL,#CR\n", },

{ RS,		INAREG,
	SAREG,	TUNSIGNED,
	SSH32,	TANY,
		XSL(A),	RESC1,
		"	lsr A1,AL,#CR\n", },

{ RS,		INAREG,
	SAREG,	TLONG|TLONGLONG,
	SSH64,	TANY,
		XSL(A),	RESC1,
		"	asr A1,AL,#CR\n", },

{ RS,		INAREG,
	SAREG,	TULONG|TULONGLONG|TPOINT,
	SSH64,	TANY,
		XSL(A),	RESC1,
		"	lsr A1,AL,#CR\n", },

/*
 * Unary operators.
 */
{ UMINUS,	INAREG,
	SAREG,	T32|T64,
	SAREG,	T32|T64,
		XSL(A),	RESC1,
		"	neg A1,AL\n", },

{ UMINUS,	INBREG,
	SBREG,	TFP,
	SBREG,	TFP,
		XSL(B),	RESC1,
		"	fneg A1,AL\n", },

{ COMPL,	INAREG,
	SAREG,	T32|T64,
	SANY,	TANY,
		XSL(A),	RESC1,
		"	mvn A1,AL\n", },

/*
 * Indirection (loads) and assignment, by size.
 */
LDST(T64, "ldr", "str")
LDST(TW, "ldr", "str")
LDST(TSHORT|TUSHORT, "ldrh", "strh")
LDST(TCHAR|TUCHAR, "ldrb", "strb")

{ UMUL,		INBREG,
	SANY,	TANY,
	SOREG,	TFP,
		XSL(B),	RESC1,
		"ZP	ldr A1,AL\n", },

{ OPLTYPE,	INBREG,
	SANY,	TANY,
	SOREG|SNAME,	TFP,
		XSL(B),	RESC1,
		"ZP	ldr A1,AL\n", },

{ ASSIGN,	FOREFF|INBREG,
	SOREG|SNAME,	TFP,
	SBREG,		TFP,
		0,	RDEST,
		"ZP	str AR,AL\n", },

{ ASSIGN,	FOREFF|INBREG,
	SBREG,		TFP,
	SOREG|SNAME,	TFP,
		0,	RDEST,
		"ZP	ldr AL,AR\n", },

{ ASSIGN,	FOREFF|INBREG,
	SBREG,	TFP,
	SBREG,	TFP,
		0,	RDEST,
		"	fmov AL,AR\n", },

/* zero to memory */
{ ASSIGN,	FOREFF,
	SOREG|SNAME,	T64,
	SZERO,		TANY,
		0,	0,
		"ZP	str xzr,AL\n", },

{ ASSIGN,	FOREFF,
	SOREG|SNAME,	TW,
	SZERO,		TANY,
		0,	0,
		"ZP	str wzr,AL\n", },

{ ASSIGN,	FOREFF,
	SOREG|SNAME,	TSHORT|TUSHORT,
	SZERO,		TANY,
		0,	0,
		"ZP	strh wzr,AL\n", },

{ ASSIGN,	FOREFF,
	SOREG|SNAME,	TCHAR|TUCHAR,
	SZERO,		TANY,
		0,	0,
		"ZP	strb wzr,AL\n", },

/* register to register, constant to register */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,	T32|T64,
	SAREG,	T32|T64,
		0,	RDEST,
		"	mov AL,AR\n", },

{ ASSIGN,	FOREFF|INAREG,
	SAREG,	T32|T64,
	SCON,	TANY,
		0,	RDEST,
		"Zk", },

/* struct assignment: the right side is a pointer to the source */
{ STASG,	INAREG|FOREFF,
	SOREG|SNAME,	TANY,
	SAREG,		TPTRTO|TANY,
		NEEDS(NREG(A, 2)),	RDEST,
		"ZS", },

/*
 * Comparisons.  cbgen() prints the branch.
 */
{ OPLOG,	FORCC,
	SAREG,	T64|TW,
	SAREG,	T64|TW,
		0,	RESCC,
		"ZV	cmp AL,AR\n", },

{ OPLOG,	FORCC,
	SAREG,	T64|TW,
	SIMM12,	TANY,
		0,	RESCC,
		"ZV	cmp AL,#CR\n", },

{ OPLOG,	FORCC,
	SAREG,	T64|TW,
	SNIMM12,	TANY,
		0,	RESCC,
		"ZV	cmn AL,#ZN\n", },

{ OPLOG,	FORCC,
	SBREG,	TFP,
	SBREG,	TFP,
		0,	RESCC,
		"ZF	fcmp AL,AR\n", },

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
		"	b LL\n", },

{ GOTO,		FOREFF,
	SAREG,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	br UL\n", },

/*
 * Convert LTYPE to reg.
 */
{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SCON,	T32|T64,
		NEEDS(NREG(A, 1)),	RESC1,
		"ZK", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SAREG,	T32|T64,
		NEEDS(NREG(A, 1)),	RESC1,
		"	mov A1,AL\n", },

{ OPLTYPE,	INBREG,
	SANY,	TANY,
	SBREG,	TFP,
		NEEDS(NREG(B, 1)),	RESC1,
		"	fmov A1,AL\n", },

/* 0.0 */
{ FCON,		INBREG,
	SANY,	TFLOAT,
	SANY,	TFLOAT,
		NEEDS(NREG(B, 1)),	RESC1,
		"	fmov A1,wzr\n", },

{ FCON,		INBREG,
	SANY,	TDOUBLE|TLDOUBLE,
	SANY,	TDOUBLE|TLDOUBLE,
		NEEDS(NREG(B, 1)),	RESC1,
		"	fmov A1,xzr\n", },

{ ADDROF,	INAREG,
	SNAME,	TANY,
	SANY,	TANY,
		NEEDS(NREG(A, 1)), RESC1,
		"ZG", },

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
