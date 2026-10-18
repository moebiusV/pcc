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
 * Where this differs from the standard: a struct argument is always passed
 * in memory in the argument area, and a struct result always through a
 * pointer passed in x8 (the standard passes small ones in x0/x1).
 */

# include "pass1.h"
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

static int ngpr, nfpr, nrsp;	/* nrsp in bits */
static int stroffset;		/* temp holding the struct return pointer */

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
static char *vaint, *vafp, *vamem;

static NODE *movtomem(NODE *p, int off, int reg);
static NODE *movtoreg(NODE *p, int rno);

/*
 * Print out assembler segment name.
 */
void
setseg(int seg, char *name)
{
	switch (seg) {
	case PROG: name = ".section .text"; break;
	case DATA:
	case LDATA: name = ".section .data"; break;
	case STRNG:
	case RDATA: name = ".section .rodata"; break;
	case UDATA: break;
	case PICLDATA:
	case PICDATA: name = ".section .data.rel.rw,\"aw\",@progbits"; break;
	case PICRDATA: name = ".section .data.rel.ro,\"aw\",@progbits"; break;
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
		if (ISFTN(sp->stype)) {
			printf(PRTPREF "\t.type %s,%%function\n", name);
		} else {
			printf(PRTPREF "\t.type %s,%%object\n", name);
			printf(PRTPREF "\t.size %s,%d\n", name,
			    (int)tsize(sp->stype, sp->sdf, sp->sss)/SZCHAR);
		}
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
 * code for the beginning of a function; a is an array of
 * indices in symtab for the arguments; n is the number
 */
void
bfcode(struct symtab **s, int cnt)
{
	struct symtab *sp;
	NODE *p, *r;
	int i, rno;

	/* Always move register arguments to temps, to free the registers */
	ngpr = nfpr = 0;
	nrsp = ARGINIT;
	rsaoff = 0;
	if (cftnsp->sdf != NULL && cftnsp->sdf->dlst != 0 && pr_hasell(cftnsp->sdf->dlst)) {
		/* the save area: x0-x7, then d0-d7 in 16-byte slots */
		SETOFF(autooff, 128);
		rsaoff = autooff;
		autooff += (VAGRSZ + VAVRSZ) * SZCHAR;
		for (i = 0; i < 8; i++) {
			r = block(REG, NIL, NIL, LONG, 0, 0);
			regno(r) = X0 + i;
			ecomp(movtomem(r, rsaoff + i * SZLONG, FPREG));
			r = block(REG, NIL, NIL, DOUBLE, 0, 0);
			regno(r) = D0 + i;
			ecomp(movtomem(r, rsaoff + VAGRSZ * SZCHAR + i * 128, FPREG));
		}
	}
	if (cftnsp->stype == STRTY+FTN || cftnsp->stype == UNIONTY+FTN) {
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
			sp->soffset = PARAMMARK*SZCHAR + nrsp;
			nrsp += (int)(tsize(sp->stype, sp->sdf, sp->sss) +
			    SZLONG - 1) & ~(SZLONG - 1);
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
			sp->soffset = PARAMMARK*SZCHAR + nrsp;
			nrsp += SZLONG;
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
static FILE *realout;
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
			printf(PRTPREF "\t.section .text\n\t.p2align 2\n%s:\n", fp ? vafp : vaint);
			printf(PRTPREF "\tldr w1,[x0,#%d]\n", fp ? 28 : 24);
			printf(PRTPREF "\ttbz w1,#31,.L%s_stk\n", fp ? vafp : vaint);
			printf(PRTPREF "\tadd w2,w1,#%d\n", fp ? 16 : 8);
			printf(PRTPREF "\tstr w2,[x0,#%d]\n", fp ? 28 : 24);
			printf(PRTPREF "\tldr x3,[x0,#%d]\n", fp ? 16 : 8);
			printf(PRTPREF "\tadd x0,x3,w1,sxtw\n\tret\n");
			printf(PRTPREF ".L%s_stk:\n\tldr x1,[x0]\n\tadd x2,x1,#8\n\tstr x2,[x0]\n\tmov x0,x1\n\tret\n", fp ? vafp : vaint);
		}
	}
	if (vaneeds & VA_MEM) {
		/* x0 = the va_list, x1 = size: a struct is in the stack arguments */
		printf(PRTPREF "\t.section .text\n\t.p2align 2\n%s:\n", vamem);
		printf(PRTPREF "\tldr x2,[x0]\n\tadd x1,x1,#7\n\tand x1,x1,#-8\n\tadd x1,x2,x1\n");
		printf(PRTPREF "\tstr x1,[x0]\n\tmov x0,x2\n\tret\n");
	}
	printf(PRTPREF "\t.ident \"PCC: %s\"\n", VERSSTR);
	if (realout != NULL) {
		fclose(stdout);
		stdout = realout;
		realout = NULL;
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

	/* the helpers va_arg calls, written into the file when used */
#define	MKN(vn, rn) \
	{ vn = addname(rn); sp = lookup(vn, SNORMAL); \
	  sp->sclass = USTATIC; sp->stype = FTN|VOID|(PTR<<TSHIFT); }
	MKN(vaint, "__pcc_vaint");
	MKN(vafp, "__pcc_vafp");
	MKN(vamem, "__pcc_vamem");
	if (xdmr) {
		realout = stdout;
		if ((stdout = open_memstream(&obuf, &olen)) == NULL)
			stdout = realout, realout = NULL;
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
	q = tempnode(0, p->n_type, p->n_df, p->pss);
	r = ccopy(q);
	return buildtree(COMOP, buildtree(ASSIGN, q, p), r);
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

	if (p->n_op == STARG) {
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
	r = nrsp;
	nrsp += SZLONG;
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
mkvacall(char *fun, NODE *a, int typ, int withsize)
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
	r = ccast(r, INCREF(dp->n_type), 0, dp->n_df, dp->pss);
	return buildtree(UMUL, r, NIL);
}

NODE *
aarch64_builtin_va_arg(const struct bitable *bt, NODE *a)
{
	NODE *r, *dp = a->n_right;
	TWORD t = dp->n_type;

	if (t == FLOAT || t == DOUBLE || t == LDOUBLE)
		r = mkvacall(vafp, a, VA_FP, 0);
	else if (ISSOU(t))
		r = mkvacall(vamem, a, VA_MEM, 1);
	else
		r = mkvacall(vaint, a, VA_INT, 0);
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
