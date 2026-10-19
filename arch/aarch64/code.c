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

/*
 *  Stuff for pass1.
 *
 * Calling convention (AAPCS64, LP64): integer and pointer arguments in x0-x7,
 * float and double in d0-d7, the rest in 8-byte slots at the bottom of the
 * caller's frame (written as [sp,#n]).  The result is in x0 or d0.
 * Structs follow the standard too (see sclass()): up to 16 bytes in one or two x
 * registers, one to four floats or doubles in as many v registers, anything
 * larger copied by the caller and passed by address, and a larger result written
 * through the address in x8.  Not done: 16-byte alignment of a register pair, and
 * a struct that does not fit the registers left is passed in memory.
 */

# include "pass1.h"
#include "outcap.h"
# include "peep.h"

#ifndef LANG_CXX
#undef NIL
#define	NIL NULL
#define	NODE P1ND
#define	nfree p1nfree
#define	ccopy p1tcopy
#define	tfree p1tfree
#undef n_type
#define n_type ptype
#undef n_qual
#define n_qual pqual
#undef n_df
#define n_df pdf
#else
#define	sss sap
#define ssdesc attr
#define pss n_ap
#endif

#define	NGPR	8		/* x0-x7 */
#define	NFPR	8		/* d0-d7 */

NODE *cstknode(TWORD, union dimfun *, struct ssdesc *);
#ifdef A64QUAD
void a64_tfinit(void);
#endif
static NODE *movtoreg(NODE *, int);
static NODE *movtomem(NODE *, int, int);

static int ngpr, nfpr, nrsp;	/* nrsp in bits */
#ifdef A64QUAD
static int lowering;		/* building the call for a long double operation */
static int ncallargs;		/* ... and this many of its arguments contain a call */

static int
hascall(NODE *p)
{
	int o = p->n_op;

	if (o == CALL || o == UCALL || o == STCALL || o == USTCALL || o == COMOP)
		return 1;
	if (coptype(o) == LTYPE)
		return 0;
	if (hascall(p->n_left))
		return 1;
	return coptype(o) == BITYPE && hascall(p->n_right);
}

static int
countcallargs(NODE *p)
{
	if (p == NIL)
		return 0;
	if (p->n_op == CM)
		return countcallargs(p->n_left) + countcallargs(p->n_right);
	return hascall(p);
}
#endif
static int stroffset;		/* temp holding the struct return pointer */
#ifdef MACHOABI
static int argidx, varidx;	/* the argument being placed; the first "..." one (Apple: those go on the stack) */
#endif

/*
 * Variable arguments: AAPCS64's va_list (Linux), as a one-element array of
 *	struct { void *__stack, *__gr_top, *__vr_top; int __gr_offs, __vr_offs; }
 * A variadic function stores x0-x7 and d0-d7 in a save area of its frame
 * (8 bytes each, then 16 each) and va_start points the list at it.
 */
#define	VAGRSZ	(8*8)
#define	VAVRSZ	(8*16)
static char *gr_top_n, *vr_top_n, *stack_n, *gr_offs_n, *vr_offs_n;
static int rsaoff;		/* the save area, bits above the frame pointer */
static int thisgpr, thisfpr, thisrsp;
static int vaneeds;
#define	VA_INT	1
#define	VA_FP	2
#define	VA_MEM	4
#define	VA_Q	8
#define	VA_NEG	16
static char *vaint, *vafp, *vamem, *vaq, *vaneg;

static NODE *movtomem(NODE *p, int off, int reg);
static NODE *movtoreg(NODE *p, int rno);

/*
 * Print out assembler segment name.
 */
void
setseg(int seg, char *name)
{
	switch (seg) {
#ifdef MACHOABI
	case PROG: name = ".text"; break;
	case DATA:
	case LDATA: name = ".data"; break;
#else
	case PROG: name = ".section .text"; break;
	case DATA:
	case LDATA: name = ".section .data"; break;
#endif
#ifdef MACHOABI
	case STRNG: name = ".cstring"; break;
	case RDATA: name = ".const_data"; break;
	case UDATA: break;
	case PICLDATA:
	case PICDATA: name = ".data"; break;
	case PICRDATA: name = ".const_data"; break;
#else
	case STRNG:
	case RDATA: name = ".section .rodata"; break;
	case UDATA: break;
	case PICLDATA:
	case PICDATA: name = ".section .data.rel.rw,\"aw\",@progbits"; break;
	case PICRDATA: name = ".section .data.rel.ro,\"aw\",@progbits"; break;
#endif
	case TLSDATA: name = ".section .tdata,\"awT\",@progbits"; break;
	case TLSUDATA: name = ".section .tbss,\"awT\",@nobits"; break;
	case CTORS: name = ".section\t.ctors,\"aw\",@progbits"; break;
	case DTORS: name = ".section\t.dtors,\"aw\",@progbits"; break;
	case NMSEG: 
		printf(PRTPREF "\t.section %s,\"a%c\",@progbits\n", name,
		    cftnsp ? 'x' : 'w');
		return;
	}
	printf(PRTPREF "\t%s\n", name);
}

/*
 * Define everything needed to print out some data (or text).
 * This means segment, alignment, visibility, etc.
 */
void
defloc(struct symtab *sp)
{
	char *name;

	name = getexname(sp);

	if (sp->sclass == EXTDEF) {
		printf(PRTPREF "\t.globl %s\n", name);
#ifndef MACHOABI
		if (ISFTN(sp->stype)) {
			printf(PRTPREF "\t.type %s,%%function\n", name);
		} else {
			printf(PRTPREF "\t.type %s,%%object\n", name);
			printf(PRTPREF "\t.size %s,%d\n", name,
			    (int)tsize(sp->stype, sp->sdf, sp->sss)/SZCHAR);
		}
#endif
	}
	if (sp->slevel == 0)
		printf(PRTPREF "%s:\n", name);
	else
		printf(PRTPREF LABFMT ":\n", sp->soffset);
}

static int
isfloat(TWORD t)
{
	return t == FLOAT || t == DOUBLE || t == LDOUBLE;
}

/* bytes in a member of a floating-point aggregate */
static int
fpsize(TWORD t)
{
	return t == FLOAT ? 4 : t == LDOUBLE ? 16 : 8;
}

/* the stack slot of an argument: 8 bytes, 16 aligned and wide for a 128-bit long double */
static int
stkslot(TWORD t)
{
#ifdef A64QUAD
	if (t == LDOUBLE) {
		nrsp = (nrsp + 127) & ~127;
		return 128;
	}
#endif
	return SZLONG;
}

/*
 * A homogeneous floating-point aggregate: members all of one floating type, four
 * at most, counting the elements of arrays and of nested structs.
 */
static int
hfawalk(TWORD t, union dimfun *df, struct ssdesc *ss, TWORD *base, int *n)
{
	struct symtab *m;
	int n0, cnt;

	if (ISARY(t)) {
		cnt = df->ddim;
		if (cnt <= 0)
			return 0;
		n0 = *n;
		if (!hfawalk(DECREF(t), df + 1, ss, base, n))
			return 0;
		*n = n0 + (*n - n0) * cnt;
		return *n <= 4;
	}
	if (t == FLOAT || t == DOUBLE || t == LDOUBLE) {
#ifndef A64QUAD
		if (t == LDOUBLE)
			t = DOUBLE;		/* long double is a double here */
#endif
		if (*base == 0)
			*base = t;
		else if (*base != t)
			return 0;
		(*n)++;
		return *n <= 4;
	}
	if (t == STRTY) {
		for (m = strmemb(ss); m != NULL; m = m->snext) {
			if (m->sclass & FIELD)
				return 0;
			if (!hfawalk(m->stype, m->sdf, m->sss, base, n))
				return 0;
		}
		return *n > 0;
	}
	return 0;
}

/*
 * How a struct or union argument or result is passed: SC_REG, SC_HFA or SC_MEM.
 * *n is the number of registers (SC_REG: eight-byte words; SC_HFA: members), and
 * *base the member type of an SC_HFA.
 */
int
a64_sclass(TWORD t, union dimfun *df, struct ssdesc *ss, TWORD *base, int *n)
{
	long sz = (long)(tsize(t, df, ss) / SZCHAR);

	*base = 0;
	*n = 0;
	if (t == STRTY && hfawalk(t, df, ss, base, n))
		return SC_HFA;
	*base = 0;
	*n = (int)((sz + 7) / 8);
	return sz > 16 ? SC_MEM : SC_REG;
}

/*
 * code for the end of a function
 * deals with struct return here
 * The return value is the address of the struct, in the return register.
 */
void
efcode(void)
{
	struct symtab *sp;
	TWORD t;
	NODE *r, *l;

	sp = cftnsp;
	t = DECREF(sp->stype);
	if (t != STRTY && t != UNIONTY)
		return;

	{
		TWORD base;
		int n, cl, i, bsz;

		cl = a64_sclass(t, sp->sdf, sp->sss, &base, &n);
		if (cl == SC_REG) {
			/* x0 holds the address of the struct: x1 first, it is read through x0 */
			for (i = n - 1; i >= 0; i--) {
				r = block(REG, NIL, NIL, INCREF(LONG), 0, 0);
				regno(r) = X0;
				r = buildtree(UMUL, buildtree(PLUS, r, bcon(i)), NIL);
				ecomp(movtoreg(r, X0 + i));
			}
			return;
		}
		if (cl == SC_HFA) {
			bsz = fpsize(base);
			for (i = 0; i < n; i++) {
				r = block(REG, NIL, NIL, INCREF(base), 0, 0);
				regno(r) = X0;
				r = buildtree(UMUL, buildtree(PLUS, r, bcon(i)), NIL);
				ecomp(movtoreg(r, D0 + i));
			}
			return;
		}
	}

	r = block(REG, NIL, NIL, INCREF(t), sp->sdf, sp->sss);
	regno(r) = X0;
	r = buildtree(UMUL, r, NIL);
	l = tempnode(stroffset, INCREF(t), sp->sdf, sp->sss);
	l = buildtree(UMUL, l, NIL);
	ecomp(buildtree(ASSIGN, l, r));
	l = block(REG, NIL, NIL, LONG, 0, 0);
	regno(l) = X0;
	r = tempnode(stroffset, LONG, 0, 0);
	ecomp(buildtree(ASSIGN, l, r));
}

/*
 * A struct parameter, as AAPCS64 passes it: the words come in x registers (or v
 * registers for a floating-point aggregate) and are stored in the frame, or arrive
 * in the caller's argument area; a larger one arrives by address and is copied.
 */
static void
structparam(struct symtab *sp)
{
	TWORD base, ot;
	union dimfun *od;
	struct ssdesc *os;
	NODE *r, *q;
	int cl, n, i, off, bsz, sz;

	cl = a64_sclass(sp->stype, sp->sdf, sp->sss, &base, &n);
	sz = (int)(tsize(sp->stype, sp->sdf, sp->sss) / SZCHAR);
	sz = (sz + 7) & ~7;

	if (cl == SC_MEM) {
		/* the address of the caller's copy; this function gets a copy of its own */
		SETOFF(autooff, SZLONG);
		off = autooff;
		autooff += sz * SZCHAR;
		if (ngpr < NGPR) {
			r = block(REG, NIL, NIL, INCREF(sp->stype), sp->sdf, sp->sss);
			regno(r) = X0 + ngpr++;
		} else {
			ot = sp->stype; od = sp->sdf; os = sp->sss;
			sp->stype = LONG; sp->sdf = NULL; sp->sss = NULL;
			sp->soffset = PARAMMARK*SZCHAR + nrsp;
			nrsp += SZLONG;
			q = nametree(sp);
			sp->stype = ot; sp->sdf = od; sp->sss = os;
			r = ccast(q, INCREF(ot), 0, od, os);
		}
		sp->soffset = off;
		ecomp(buildtree(ASSIGN, nametree(sp), buildtree(UMUL, r, NIL)));
		return;
	}
	if ((cl == SC_REG && ngpr + n > NGPR) || (cl == SC_HFA && nfpr + n > NFPR)) {
		/* not enough registers left: the struct is in the caller's argument area */
		sp->soffset = PARAMMARK*SZCHAR + nrsp;
		nrsp += sz * SZCHAR;
		if (cl == SC_REG)
			ngpr = NGPR;
		else
			nfpr = NFPR;
		return;
	}
	SETOFF(autooff, SZLONG);
	off = autooff;
	autooff += sz * SZCHAR;
	bsz = fpsize(base);
	for (i = 0; i < n; i++) {
		if (cl == SC_REG) {
			r = block(REG, NIL, NIL, LONG, 0, 0);
			regno(r) = X0 + ngpr++;
			ecomp(movtomem(r, off + i * SZLONG, FPREG));
		} else {
			r = block(REG, NIL, NIL, base, 0, 0);
			regno(r) = D0 + nfpr++;
			ecomp(movtomem(r, off + i * bsz * SZCHAR, FPREG));
		}
	}
	sp->soffset = off;
}

/*
 * code for the beginning of a function; a is an array of
 * indices in symtab for the arguments; n is the number
 */
void
bfcode(struct symtab **s, int cnt)
{
	struct symtab *sp;
	NODE *p, *r;
	TWORD sbase;
	int i, rno, sn;

	/* Always move register arguments to temps, to free the registers */
	ngpr = nfpr = 0;
	nrsp = ARGINIT;
	rsaoff = 0;
#ifndef MACHOABI
	if (cftnsp->sdf != NULL && cftnsp->sdf->dlst != 0 && pr_hasell(cftnsp->sdf->dlst)) {
		/* the save area: x0-x7, then d0-d7 in 16-byte slots */
		SETOFF(autooff, 128);
		rsaoff = autooff;
		autooff += (VAGRSZ + VAVRSZ) * SZCHAR;
		for (i = 0; i < 8; i++) {
			r = block(REG, NIL, NIL, LONG, 0, 0);
			regno(r) = X0 + i;
			ecomp(movtomem(r, rsaoff + i * SZLONG, FPREG));
#ifdef A64QUAD
			r = block(REG, NIL, NIL, LDOUBLE, 0, 0);	/* all 128 bits of v0-v7 */
#else
			r = block(REG, NIL, NIL, DOUBLE, 0, 0);
#endif
			regno(r) = D0 + i;
			ecomp(movtomem(r, rsaoff + VAGRSZ * SZCHAR + i * 128, FPREG));
		}
	}
#endif
	if ((cftnsp->stype == STRTY+FTN || cftnsp->stype == UNIONTY+FTN) &&
	    a64_sclass(DECREF(cftnsp->stype), cftnsp->sdf, cftnsp->sss, &sbase, &sn) == SC_MEM) {
		r = block(REG, NIL, NIL, LONG, 0, 0);
		regno(r) = X8;
		p = tempnode(0, r->n_type, r->n_df, r->pss);
		stroffset = regno(p);
		ecomp(buildtree(ASSIGN, p, r));
	}

	for (i = 0; i < cnt; i++) {
		sp = s[i];

		if (sp == NULL)
			continue;

		if (ISSOU(sp->stype)) {
			structparam(sp);
			continue;
		}
		if (isfloat(sp->stype)) {
			if (nfpr < NFPR)
				rno = D0 + nfpr++;
			else
				rno = -1;
		} else {
			if (ngpr < NGPR)
				rno = X0 + ngpr++;
			else
				rno = -1;
		}
		if (rno < 0) {
			int sl = stkslot(sp->stype);

			sp->soffset = PARAMMARK*SZCHAR + nrsp;
			nrsp += sl;
			continue;
		}
		r = block(REG, NIL, NIL, sp->stype, sp->sdf, sp->sss);
		regno(r) = rno;
		p = tempnode(0, sp->stype, sp->sdf, sp->sss);
		sp->soffset = regno(p);
		sp->sflags |= STNODE;
		ecomp(buildtree(ASSIGN, p, r));
	}
	thisgpr = ngpr;
	thisfpr = nfpr;
	thisrsp = nrsp;
}

/*
 * The assembly of the whole file is kept in memory so that it can be looked
 * at as a whole when the job is over (-xdmr): see peep.h.
 */
static int realout;
static char *obuf;
static size_t olen;

/* called just before final exit */
/* flag is 1 if errors, 0 if none */
void
ejobcode(int flag)
{
	if (vaneeds & (VA_INT | VA_FP)) {
		/* x0 = the va_list; the address of the next argument comes back in x0 */
		int fp = 0;

		for (fp = 0; fp < 2; fp++) {
			if (!(vaneeds & (fp ? VA_FP : VA_INT)))
				continue;
			printf(PRTPREF "\t" TEXTDIR "\n\t.p2align 2\n%s:\n", exname(fp ? vafp : vaint));
			printf(PRTPREF "\tldr w1,[x0,#%d]\n", fp ? 28 : 24);
			printf(PRTPREF "\ttbz w1,#31," LOCLAB "%s_stk\n", fp ? vafp : vaint);
			printf(PRTPREF "\tadd w2,w1,#%d\n", fp ? 16 : 8);
			printf(PRTPREF "\tstr w2,[x0,#%d]\n", fp ? 28 : 24);
			printf(PRTPREF "\tldr x3,[x0,#%d]\n", fp ? 16 : 8);
			printf(PRTPREF "\tadd x0,x3,w1,sxtw\n\tret\n");
			printf(PRTPREF "" LOCLAB "%s_stk:\n\tldr x1,[x0]\n\tadd x2,x1,#8\n\tstr x2,[x0]\n\tmov x0,x1\n\tret\n", fp ? vafp : vaint);
		}
	}
	if (vaneeds & VA_MEM) {
		/*
		 * x0 = the va_list, x1 = size (at most 16): the address of a struct passed in registers
		 * x[n]..: all of it in the saved registers if they hold it, else all on the stack.
		 */
		printf(PRTPREF "\t" TEXTDIR "\n\t.p2align 2\n%s:\n", exname(vamem));
		printf(PRTPREF "\tadd x1,x1,#7\n\tand x1,x1,#-8\n\tldr w2,[x0,#24]\n");
		printf(PRTPREF "\ttbz w2,#31," LOCLAB "%s_stk\n\tadd w3,w2,w1\n\tcmp w3,#0\n\tb.gt " LOCLAB "%s_stk\n", vamem, vamem);
		printf(PRTPREF "\tstr w3,[x0,#24]\n\tldr x4,[x0,#8]\n\tadd x0,x4,w2,sxtw\n\tret\n");
		printf(PRTPREF "" LOCLAB "%s_stk:\n\tstr wzr,[x0,#24]\n\tldr x2,[x0]\n\tadd x3,x2,x1\n\tstr x3,[x0]\n\tmov x0,x2\n\tret\n", vamem);
	}
	if (vaneeds & VA_Q) {
		/* as __pcc_vafp, but on the stack a 128-bit value is 16-byte aligned and takes 16 bytes */
		printf(PRTPREF "\t" TEXTDIR "\n\t.p2align 2\n%s:\n", exname(vaq));
		printf(PRTPREF "\tldr w1,[x0,#28]\n\ttbz w1,#31," LOCLAB "%s_stk\n", vaq);
		printf(PRTPREF "\tadd w2,w1,#16\n\tstr w2,[x0,#28]\n\tldr x3,[x0,#16]\n\tadd x0,x3,w1,sxtw\n\tret\n");
		printf(PRTPREF "" LOCLAB "%s_stk:\n\tldr x1,[x0]\n\tadd x1,x1,#15\n\tand x1,x1,#-16\n", vaq);
		printf(PRTPREF "\tadd x2,x1,#16\n\tstr x2,[x0]\n\tmov x0,x1\n\tret\n");
	}
	if (vaneeds & VA_NEG) {
		/* -x for a 128-bit long double: flip the sign bit (libgcc has no __negtf2 for it) */
		printf(PRTPREF "\t" TEXTDIR "\n\t.p2align 2\n%s:\n", exname(vaneg));
		printf(PRTPREF "\tfmov x0,d0\n\tmov x1,v0.d[1]\n\teor x1,x1,#0x8000000000000000\n");
		printf(PRTPREF "\tfmov d0,x0\n\tmov v0.d[1],x1\n\tret\n");
	}
#ifndef MACHOABI
	printf(PRTPREF "\t.ident \"PCC: %s\"\n", VERSSTR);
#endif
	if (realout) {
		obuf = outcap_end(&olen);
		realout = 0;
		if (flag || getenv("A64_NOPEEP") != NULL)
			fputs(obuf, stdout);
		else
			a64_peep(obuf, stdout);
		free(obuf);
	}
}

void
bjobcode(void)
{
	struct symtab *sp;
	struct rstack *rp;
	NODE *p, *q;
	char *c;

	astypnames[INT] = astypnames[UNSIGNED] = "\t.long";
	astypnames[LONG] = astypnames[ULONG] = "\t.quad";

#ifdef MACHOABI
	/* Apple: typedef char *__builtin_va_list; the arguments are all in 8-byte slots on the stack */
	p = block(NAME, NIL, NIL, CHAR+PTR, 0, 0);
	c = addname("__builtin_va_list");
	p->n_sp = lookup(c, 0);
	defid(p, TYPEDEF);
	nfree(p);
#else
	/* typedef struct { void *__stack, *__gr_top, *__vr_top; int __gr_offs, __vr_offs; } __builtin_va_list[1]; */
	stack_n = addname("__stack");
	gr_top_n = addname("__gr_top");
	vr_top_n = addname("__vr_top");
	gr_offs_n = addname("__gr_offs");
	vr_offs_n = addname("__vr_offs");
	rp = bstruct(NULL, STNAME, NULL);
	p = block(NAME, NIL, NIL, VOID+PTR, 0, 0);
	soumemb(p, stack_n, 0);
	soumemb(p, gr_top_n, 0);
	soumemb(p, vr_top_n, 0);
	p->n_type = INT;
	p->n_ap = NULL;
	soumemb(p, gr_offs_n, 0);
	soumemb(p, vr_offs_n, 0);
	nfree(p);
	q = dclstruct(rp);
	c = addname("__builtin_va_list");
	p = block(LB, bdty(NAME, c), bcon(1), INT, 0, 0);
	p = tymerge(q, p);
	p->n_sp = lookup(c, 0);
	defid(p, TYPEDEF);
	nfree(q);
	nfree(p);
#endif

	/* the helpers va_arg calls, written into the file when used */
#define	MKN(vn, rn) \
	{ vn = addname(rn); sp = lookup(vn, SNORMAL); \
	  sp->sclass = USTATIC; sp->stype = FTN|VOID|(PTR<<TSHIFT); }
	MKN(vaint, "__pcc_vaint");
	MKN(vafp, "__pcc_vafp");
	MKN(vamem, "__pcc_vamem");
	MKN(vaq, "__pcc_vaq");
	MKN(vaneg, "__pcc_negtf");
#ifdef A64QUAD
	a64_tfinit();
#endif
	if (xdmr) {
		realout = outcap_begin();
	}
}

/*
 * Convert a plain argument to the register it goes in, or the slot it
 * goes to in memory.
 */
static NODE *
movtoreg(NODE *p, int rno)
{
	NODE *r;

	r = block(REG, NIL, NIL, p->n_type, p->n_df, p->pss);
	regno(r) = rno;
	return clocal(buildtree(ASSIGN, r, p));
}

static NODE *
movtomem(NODE *p, int off, int reg)
{
	struct symtab s;
	NODE *r, *l;

	s.stype = p->n_type;
	s.squal = 0;
	s.sdf = p->n_df;
	s.sss = p->pss;
	s.sap = p->n_ap;
	s.soffset = off;
	s.sclass = AUTO;

	l = block(REG, NIL, NIL, PTR+STRTY, 0, 0);
	slval(l, 0);
	regno(l) = reg;

	r = block(NAME, NIL, NIL, p->n_type, p->n_df, p->pss);
	r->n_sp = &s;
	r = stref(block(STREF, l, r, 0, 0, 0));

	return clocal(buildtree(ASSIGN, r, p));
}

/*
 * Evaluate p into a temp before anything is assigned to an argument
 * register or slot (delcomop() in pass2 hoists the left side of the
 * COMOP ahead of the call), so a call inside an argument cannot
 * clobber the arguments already placed.
 */
static NODE *
hoist(NODE *p)
{
	NODE *q, *r;

	if (coptype(p->n_op) == LTYPE)
		return p;
#ifdef A64QUAD
	/*
	 * A library call made for a long double operation can be made after the tree has been cleaned of
	 * comma operators (a condition x becomes x != 0 late), so it makes none it can do without: an
	 * argument needs a temporary only if another argument has a call that would clobber its register.
	 */
	if (lowering && ncallargs - hascall(p) <= 0)
		return p;
#endif
	q = tempnode(0, p->n_type, p->n_df, p->pss);
	r = ccopy(q);
	return buildtree(COMOP, buildtree(ASSIGN, q, p), r);
}

static NODE *structstack(NODE *p);

/*
 * A struct argument.  The words go to registers when there are enough left, a
 * larger struct is copied to a temporary in our frame and its address passed like
 * an integer; the rest goes in the argument area.  Everything that reads the
 * struct is hoisted ahead of the call so that no register is clobbered first.
 */
static NODE *
structarg(NODE *p)
{
	NODE *t, *tc, *cp, *a, *rv, *w;
	TWORD base;
	int cl, n, i, bsz, ok;

	cl = a64_sclass(p->n_type, p->n_df, p->pss, &base, &n);

#ifdef MACHOABI
	if (argidx++ >= varidx && cl != SC_MEM)
		return structstack(p);		/* by value, in 8-byte slots */
	if (argidx > varidx && cl == SC_MEM) {
		/* the copy is ours; its address goes in the next stack slot */
		t = cstknode(p->n_type, p->n_df, p->pss);
		tc = ccopy(t);
		cp = buildtree(ASSIGN, t, buildtree(UMUL, p->n_left, NIL));
		a = buildtree(COMOP, cp, buildtree(ADDROF, tc, NIL));
		nfree(p);
		rv = movtomem(a, nrsp, STKREG);
		nrsp += SZLONG;
		return rv;
	}
#endif
	if (cl == SC_MEM) {
		/* the copy is ours; the callee gets its address */
		t = cstknode(p->n_type, p->n_df, p->pss);
		tc = ccopy(t);
		cp = buildtree(ASSIGN, t, buildtree(UMUL, p->n_left, NIL));
		a = buildtree(COMOP, cp, buildtree(ADDROF, tc, NIL));
		nfree(p);
		if (ngpr < NGPR)
			return movtoreg(a, X0 + ngpr++);
		rv = movtomem(a, nrsp, STKREG);
		nrsp += SZLONG;
		return rv;
	}
	ok = cl == SC_REG ? ngpr + n <= NGPR : nfpr + n <= NFPR;
	if (!ok) {
		if (cl == SC_REG)
			ngpr = NGPR;
		else
			nfpr = NFPR;
		return structstack(p);
	}

	/* the address of the struct, kept in a temporary for all the loads */
	bsz = cl == SC_HFA ? fpsize(base) : 8;
	t = tempnode(0, p->n_left->n_type, p->n_left->n_df, p->n_left->pss);
	tc = ccopy(t);
	cp = buildtree(ASSIGN, t, p->n_left);	/* the node is not cast: a call must keep its struct type */
	rv = NULL;
	for (i = 0; i < n; i++) {
		w = buildtree(UMUL, buildtree(PLUS,
		    cast(ccopy(tc), INCREF(cl == SC_HFA ? base : LONG), 0), bcon(i)), NIL);
		if (i == 0)
			w = buildtree(COMOP, cp, w);
		w = cl == SC_REG ? movtoreg(w, X0 + ngpr++) : movtoreg(w, D0 + nfpr++);
		rv = rv == NULL ? w : block(CM, rv, w, INT, 0, 0);
	}
	tfree(tc);
	nfree(p);
	return rv;
}

/* A struct that does not go in registers is copied into the argument area. */
static NODE *
structstack(NODE *p)
{
	NODE *q;
	struct symtab s;
	NODE *l, *t;

	q = buildtree(UMUL, p->n_left, NIL);

	s.stype = p->n_type;
	s.squal = 0;
	s.sdf = p->n_df;
	s.sss = p->pss;
	s.sap = NULL;
	s.soffset = nrsp;
	s.sclass = AUTO;

	nrsp += (int)(tsize(p->n_type, p->n_df, p->pss) +
	    SZLONG - 1) & ~(SZLONG - 1);

	l = block(REG, NIL, NIL, PTR+STRTY, 0, 0);
	slval(l, 0);
	regno(l) = STKREG;

	t = block(NAME, NIL, NIL, p->n_type, p->n_df, p->pss);
	t->n_sp = &s;
	t = stref(block(STREF, l, t, 0, 0, 0));

	t = buildtree(ASSIGN, t, q);
	nfree(p);
	p = t->n_left;
	nfree(t);
	return p;
}

/*
 * Convert each argument into an assignment to its register or its slot.
 * Also convert arguments < INT to int (default argument promotions).
 */
static NODE *
argput(NODE *p)
{
	NODE *q;
	int r, nreg;

	if (p->n_op == CM) {
		p->n_left = argput(p->n_left);
		p->n_right = argput(p->n_right);
		return p;
	}

	if (p->n_op == STARG)
		return structarg(p);

#ifdef MACHOABI
	/* Apple: the arguments of the "..." all go on the stack, 8 bytes each */
	if (argidx++ >= varidx) {
		r = nrsp;
		nrsp += SZLONG;
		if (p->n_type < INT || p->n_type == BOOL)
			p = cast(p, INT, 0);
		return movtomem(hoist(p), r, STKREG);
	}
#endif
	nreg = isfloat(p->n_type) ? nfpr : ngpr;
	if (nreg < NGPR) {		/* NGPR == NFPR */
		if (isfloat(p->n_type))
			r = D0 + nfpr++;
		else
			r = X0 + ngpr++;
		if (p->n_type < INT || p->n_type == BOOL)
			p = cast(p, INT, 0);
		return movtoreg(hoist(p), r);
	}
	{
		int sl = stkslot(p->n_type);

		r = nrsp;
		nrsp += sl;
	}
	if (p->n_type < INT || p->n_type == BOOL)
		p = cast(p, INT, 0);
	return movtomem(hoist(p), r, STKREG);
}

/*
 * Sort arglist so that struct copies end up first (they use scratch
 * registers), and register assignments last.
 */
static int
argsort(NODE *p)
{
	NODE *q;
	int rv = 0;

	if (p->n_op != CM)
		return rv;
	if (p->n_right->n_op == CM) {
		q = p->n_right->n_left;
		p->n_right->n_left = p->n_left;
		p->n_left = p->n_right;
		p->n_right = p->n_left->n_right;
		p->n_left->n_right = q;
	}
	if (p->n_right->n_op == ASSIGN && p->n_right->n_left->n_op == REG) {
		if (p->n_left->n_op == CM &&
		    p->n_left->n_right->n_op == STASG) {
			q = p->n_left->n_right;
			p->n_left->n_right = p->n_right;
			p->n_right = q;
			rv = 1;
		} else if (p->n_left->n_op == STASG) {
			q = p->n_left;
			p->n_left = p->n_right;
			p->n_right = q;
			rv = 1;
		}
	}
	return rv | argsort(p->n_left);
}

/*
 * Called with a function call with arguments as argument.
 * This is done early in buildtree() and only done once.
 * Returns p.
 */
NODE *
funcode(NODE *p)
{
	ngpr = nfpr = nrsp = 0;
#ifdef A64QUAD
	ncallargs = countcallargs(p->n_right);
#endif
#ifdef MACHOABI
	argidx = 0;
	varidx = 0x7fffffff;
	if (p->n_left->n_df != NULL && p->n_left->n_df->dlst != 0 &&
	    (argidx = pr_ellidx(p->n_left->n_df->dlst)) >= 0)
		varidx = argidx;
	argidx = 0;
#endif

	p->n_right = argput(p->n_right);
	while (argsort(p->n_right))
		;
	return p;
}

/* fix up type of field p */
void
fldty(struct symtab *p)
{
}

/*
 * XXX - fix genswitch.
 */
int
mygenswitch(int num, TWORD type, struct swents **p, int n)
{
	return 0;
}

/*
 * Return return as given by a.
 */
NODE *
builtin_return_address(const struct bitable *bt, NODE *a)
{
	int nframes;
	NODE *f;

	nframes = (int)glval(a);
	tfree(a);

	f = block(REG, NIL, NIL, PTR+VOID, 0, 0);
	regno(f) = FPREG;

	while (nframes--)
		f = block(UMUL, f, NIL, PTR+VOID, 0, 0);

	f = block(PLUS, f, bcon(8), INCREF(PTR+VOID), 0, 0);
	f = buildtree(UMUL, f, NIL);

	return f;
}

/*
 * Return frame as given by a.
 */
NODE *
builtin_frame_address(const struct bitable *bt, NODE *a)
{
	int nframes;
	NODE *f;

	nframes = (int)glval(a);
	tfree(a);

	f = block(REG, NIL, NIL, PTR+VOID, 0, 0);
	regno(f) = FPREG;

	while (nframes--)
		f = block(UMUL, f, NIL, PTR+VOID, 0, 0);

	return f;
}

/*
 * Return "canonical frame address".
 */
NODE *
builtin_cfa(const struct bitable *bt, NODE *a)
{
	NODE *f;

	f = block(REG, NIL, NIL, PTR+VOID, 0, 0);
	regno(f) = FPREG;
	return block(PLUS, f, bcon(16), INCREF(PTR+VOID), 0, 0);
}

#ifdef A64QUAD
/*
 * A 128-bit long double has no instructions: every operation on one is a call to the library
 * (libgcc's __addtf3 and the rest), made here, in pass 1, so the rest of the compiler only sees
 * calls, loads, stores and moves.  Comparisons use libgcc's convention: the call returns a value
 * whose sign says the order, and an unordered pair answers so that the C comparison is false, which
 * also makes the inverted test that cbranch() builds true for a NaN.
 */
static struct tfent {
	char *name;
	TWORD rt;
	struct symtab *sp;
} tfents[] = {
	{ "__addtf3", LDOUBLE }, { "__subtf3", LDOUBLE }, { "__multf3", LDOUBLE }, { "__divtf3", LDOUBLE },
	{ "__eqtf2", INT }, { "__netf2", INT }, { "__lttf2", INT }, { "__letf2", INT },
	{ "__gttf2", INT }, { "__getf2", INT },
	{ "__extendsftf2", LDOUBLE }, { "__extenddftf2", LDOUBLE }, { "__trunctfsf2", FLOAT }, { "__trunctfdf2", DOUBLE },
	{ "__floatsitf", LDOUBLE }, { "__floatunsitf", LDOUBLE }, { "__floatditf", LDOUBLE }, { "__floatunditf", LDOUBLE },
	{ "__fixtfsi", INT }, { "__fixtfdi", LONG }, { "__fixunstfsi", UNSIGNED }, { "__fixunstfdi", ULONG },
};
#define	NTF	(sizeof(tfents) / sizeof(tfents[0]))
enum { TF_ADD, TF_SUB, TF_MUL, TF_DIV, TF_EQ, TF_NE, TF_LT, TF_LE, TF_GT, TF_GE,
    TF_EXTSF, TF_EXTDF, TF_TRSF, TF_TRDF, TF_FLSI, TF_FLUSI, TF_FLDI, TF_FLUDI,
    TF_FXSI, TF_FXDI, TF_FXUSI, TF_FXUDI };

void
a64_tfinit(void)
{
	unsigned i;
	struct symtab *sp;

	/* private symbols, so that a program declaring __addtf3 itself does not clash with them */
	for (i = 0; i < NTF; i++) {
		sp = permalloc(sizeof(struct symtab));
		memset(sp, 0, sizeof(struct symtab));
		sp->sname = tfents[i].name;
		sp->sap = attr_new(ATTR_SONAME, 1);
		sp->sap->sarg(0) = tfents[i].name;
		sp->sclass = EXTERN;
		sp->sflags = sp->slevel = 0;
		sp->stype = FTN|tfents[i].rt;
		tfents[i].sp = sp;
	}
}

static NODE *
tfcall(int which, NODE *args)
{
	NODE *f = block(NAME, NIL, NIL, INT, 0, 0);

	f->n_sp = tfents[which].sp;
	f->n_type = f->n_sp->stype;
	f = clocal(f);
	lowering = 1;
	f = buildtree(CALL, f, args);
	lowering = 0;
	return f;
}

static NODE *
tfzero(void)
{
	return ccast(bcon(0), LDOUBLE, 0, NULL, NULL);
}

/* the node p has a 128-bit long double operation: the call that does it, or NULL if it has none */
NODE *
a64_ldlower(NODE *p)
{
	NODE *l, *r, *t, *a, *v;
	int o = p->n_op, w;
	TWORD lt, pt = p->n_type;

	switch (o) {
	case PLUS: case MINUS: case MUL: case DIV:
		if (pt != LDOUBLE)
			return NULL;
		w = o == PLUS ? TF_ADD : o == MINUS ? TF_SUB : o == MUL ? TF_MUL : TF_DIV;
		l = p->n_left;
		r = p->n_right;
		nfree(p);
		return tfcall(w, buildtree(CM, l, r));

	case UMINUS:
		if (pt != LDOUBLE)
			return NULL;
		l = p->n_left;
		nfree(p);
		vaneeds |= VA_NEG;
		t = block(NAME, NIL, NIL, INT, 0, 0);
		t->n_sp = lookup(vaneg, SNORMAL);
		t->n_sp->stype = FTN|LDOUBLE;
		t->n_type = t->n_sp->stype;
		return buildtree(CALL, clocal(t), l);

	case EQ: case NE: case LT: case LE: case GT: case GE:
		if (p->n_left->n_type != LDOUBLE)
			return NULL;
		w = o == EQ ? TF_EQ : o == NE ? TF_NE : o == LT ? TF_LT : o == LE ? TF_LE : o == GT ? TF_GT : TF_GE;
		l = p->n_left;
		r = p->n_right;
		p->n_left = tfcall(w, buildtree(CM, l, r));
		p->n_right = bcon(0);
		return p;

	case NOT:
		if (p->n_left->n_type != LDOUBLE)
			return NULL;
		l = p->n_left;
		nfree(p);
		return buildtree(EQ, l, tfzero());

	case SCONV:
		l = p->n_left;
		lt = l->n_type;
		if (pt == LDOUBLE && lt != LDOUBLE) {
			if (lt < INT || lt == BOOL) {		/* through int */
				l = block(SCONV, l, NIL, ISUNSIGNED(lt) ? UNSIGNED : INT, 0, 0);
				lt = l->n_type;
			}
			w = lt == FLOAT ? TF_EXTSF : lt == DOUBLE ? TF_EXTDF : lt == INT ? TF_FLSI :
			    lt == UNSIGNED ? TF_FLUSI : lt == LONG ? TF_FLDI : lt == ULONG ? TF_FLUDI : -1;
			if (w < 0)
				return NULL;
			nfree(p);
			return tfcall(w, l);
		}
		if (lt == LDOUBLE && pt != LDOUBLE) {
			if (pt == BOOL) {
				nfree(p);
				return block(SCONV, buildtree(NE, l, tfzero()), NIL, BOOL, 0, 0);
			}
			if (pt < INT) {			/* through int */
				nfree(p);
				return block(SCONV, block(SCONV, l, NIL, INT, 0, 0), NIL, pt, 0, 0);
			}
			w = pt == FLOAT ? TF_TRSF : pt == DOUBLE ? TF_TRDF : pt == INT ? TF_FXSI :
			    pt == UNSIGNED ? TF_FXUSI : pt == LONG ? TF_FXDI : pt == ULONG ? TF_FXUDI : -1;
			if (w < 0)
				return NULL;
			nfree(p);
			return tfcall(w, l);
		}
		return NULL;

	case PLUSEQ: case MINUSEQ: case MULEQ: case DIVEQ:
		if (pt != LDOUBLE)
			return NULL;
		/* *(t = &l) = *t op r: l is evaluated once */
		l = p->n_left;
		r = p->n_right;
		nfree(p);
		t = tempnode(0, INCREF(LDOUBLE), 0, 0);
		a = buildtree(ASSIGN, ccopy(t), buildtree(ADDROF, l, NIL));
		v = buildtree(o == PLUSEQ ? PLUS : o == MINUSEQ ? MINUS : o == MULEQ ? MUL : DIV,
		    buildtree(UMUL, ccopy(t), NIL), r);
		return buildtree(COMOP, a, buildtree(ASSIGN, buildtree(UMUL, t, NIL), v));

	case INCR: case DECR:
		if (pt != LDOUBLE)
			return NULL;
		/* t = &l, v = *t, *t = v +- r, v */
		l = p->n_left;
		r = p->n_right;
		nfree(p);
		t = tempnode(0, INCREF(LDOUBLE), 0, 0);
		v = tempnode(0, LDOUBLE, 0, 0);
		a = buildtree(ASSIGN, ccopy(t), buildtree(ADDROF, l, NIL));
		a = buildtree(COMOP, a, buildtree(ASSIGN, ccopy(v), buildtree(UMUL, ccopy(t), NIL)));
		a = buildtree(COMOP, a, buildtree(ASSIGN, buildtree(UMUL, t, NIL),
		    buildtree(o == INCR ? PLUS : MINUS, ccopy(v), ccast(r, LDOUBLE, 0, NULL, NULL))));
		return buildtree(COMOP, a, v);
	}
	return NULL;
}
#endif

static NODE *
mkstkref(int off, TWORD typ)
{
	NODE *p;

	p = block(REG, NIL, NIL, PTR|typ, 0, 0);
	regno(p) = FPREG;
	return buildtree(PLUS, p, bcon(off/SZCHAR));
}

NODE *
aarch64_builtin_stdarg_start(const struct bitable *bt, NODE *a)
{
	NODE *p, *r;

	p = a->n_left;
#ifdef MACHOABI
	/* built by hand: the list is a plain char * here, and the compare of pointer types has nothing to say */
	r = block(ASSIGN, p, cast(mkstkref(PARAMMARK*SZCHAR + thisrsp, VOID), p->n_type, 0), p->n_type, p->n_df, p->pss);
	tfree(a->n_right);
	nfree(a);
	return r;
#endif
	r = buildtree(ASSIGN, structref(ccopy(p), STREF, stack_n),
	    mkstkref(PARAMMARK*SZCHAR + thisrsp, VOID));
	r = buildtree(COMOP, r, buildtree(ASSIGN, structref(ccopy(p), STREF, gr_top_n),
	    mkstkref(rsaoff + VAGRSZ*SZCHAR, VOID)));
	r = buildtree(COMOP, r, buildtree(ASSIGN, structref(ccopy(p), STREF, vr_top_n),
	    mkstkref(rsaoff + (VAGRSZ + VAVRSZ)*SZCHAR, VOID)));
	r = buildtree(COMOP, r, buildtree(ASSIGN, structref(ccopy(p), STREF, gr_offs_n),
	    bcon(-(8 - thisgpr) * 8)));
	r = buildtree(COMOP, r, buildtree(ASSIGN, structref(ccopy(p), STREF, vr_offs_n),
	    bcon(-(8 - thisfpr) * 16)));
	tfree(a);
	return r;
}

static NODE *
mkvacall(char *fun, NODE *a, int typ, int withsize, int byref)
{
	NODE *r, *f = block(NAME, NIL, NIL, INT, 0, 0);
	NODE *ap = a->n_left;
	NODE *dp = a->n_right;
	OFFSZ sz = tsize(dp->n_type, dp->n_df, dp->pss);

	f->n_sp = lookup(fun, SNORMAL);
	vaneeds |= typ;
	f->n_type = f->n_sp->stype;
	f = clocal(f);
	r = buildtree(CALL, f, withsize ?
	    buildtree(CM, ccopy(ap), bcon((sz + SZCHAR - 1) / SZCHAR)) : ccopy(ap));
	if (byref) {
		/* the slot holds the address of the struct */
		r = ccast(r, INCREF(INCREF(dp->n_type)), 0, dp->n_df, dp->pss);
		r = buildtree(UMUL, r, NIL);
	} else
		r = ccast(r, INCREF(dp->n_type), 0, dp->n_df, dp->pss);
	return buildtree(UMUL, r, NIL);
}

NODE *
aarch64_builtin_va_arg(const struct bitable *bt, NODE *a)
{
	NODE *r, *dp = a->n_right;
	TWORD t = dp->n_type;

#ifdef MACHOABI
	{
		/* *(T *)((ap += slot) - slot), the slot a multiple of 8 bytes; a big struct is behind a pointer */
		NODE *ap = a->n_left, *tp, *pv;
		OFFSZ sz = tsize(t, dp->n_df, dp->pss) / SZCHAR;
		int big = ISSOU(t) && sz > 16;
		int slot = big ? 8 : (int)((sz + 7) & ~7);

		tp = tempnode(0, PTR+VOID, 0, 0);	/* void *: converts to any pointer without a word */
		pv = ccopy(tp);
		r = buildtree(COMOP, buildtree(ASSIGN, tp, ccast(ccopy(ap), CHAR+PTR, 0, NULL, NULL)),
		    buildtree(COMOP, buildtree(ASSIGN, ccopy(ap), ccast(buildtree(PLUS, ccast(ccopy(pv), CHAR+PTR, 0, NULL, NULL), bcon(slot)), CHAR+PTR, 0, NULL, NULL)),
		    big ? buildtree(UMUL, ccast(buildtree(UMUL, ccast(pv, INCREF(INCREF(t)), 0, dp->n_df, dp->pss), NIL),
			INCREF(t), 0, dp->n_df, dp->pss), NIL)
			: buildtree(UMUL, ccast(pv, INCREF(t), 0, dp->n_df, dp->pss), NIL)));
		tfree(a);
		return r;
	}
#endif
	if (t == LDOUBLE)
		r = mkvacall(vaq, a, VA_Q, 0, 0);
	else if (t == FLOAT || t == DOUBLE)
		r = mkvacall(vafp, a, VA_FP, 0, 0);
	else if (ISSOU(t)) {
		TWORD base;
		int n, cl;

		cl = a64_sclass(t, dp->n_df, dp->pss, &base, &n);
		if (cl == SC_HFA) {
			uerror("va_arg of a struct of floating-point members is not supported");
			r = bcon(0);
		} else if (cl == SC_MEM)
			r = mkvacall(vaint, a, VA_INT, 0, 1);
		else
			r = mkvacall(vamem, a, VA_MEM, 1, 0);
	} else
		r = mkvacall(vaint, a, VA_INT, 0, 0);
	tfree(a);
	return r;
}

NODE *
aarch64_builtin_va_end(const struct bitable *bt, NODE *a)
{
	tfree(a);
	return bcon(0);
}

NODE *
aarch64_builtin_va_copy(const struct bitable *bt, NODE *a)
{
	NODE *f;

	f = buildtree(ASSIGN, buildtree(UMUL, a->n_left, NIL),
	    buildtree(UMUL, a->n_right, NIL));
	nfree(a);
	return f;
}
