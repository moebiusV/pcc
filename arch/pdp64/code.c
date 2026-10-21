/*	$Id$	*/
/*
 *  Stuff for pass1.
 *
 * Calling convention (pdp64 ABI version 1, ~/claude/pdp64/docs/abi.md): the first six
 * arguments (integers, pointers, floating point alike) in r0-r5, the rest in 8-byte
 * slots at 0(sp), 8(sp), ... when the call is made (the frame has an outgoing argument
 * area for them, so nothing is pushed or popped around a call).  The result is in r0
 * (r0:r1 for a struct of up to 16 bytes); a larger struct result is written through a
 * hidden first argument, a pointer in r0, which is returned in r0.  A struct argument of
 * up to 16 bytes takes one or two argument registers (all of them or none), a larger
 * one is copied into the stack slots.
 *
 * A function with "..." makes the register arguments contiguous with the stack
 * arguments: its prologue takes the return address off the stack, pushes r5..r0 and the
 * return address again (see local2.c).  va_list is a char * walking the slots.
 */

# include "pass1.h"
# include <stdlib.h>

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

#define	NGPR	NARGREGS

NODE *cstknode(TWORD, union dimfun *, struct ssdesc *);

static int ngpr, nrsp;		/* argument registers used; bits of stack arguments */
static int stroffset;		/* temp holding the struct return pointer */
int pdp64_varfunc;		/* the function being compiled has "..." (read by local2.c) */
static int thisgpr, thisrsp;	/* what the named parameters of this function use */

static NODE *movtoreg(NODE *, int);
static NODE *movtomem(NODE *, int, int);

/*
 * Print out assembler segment name.
 */
int rodata_in_text = 1;

void
setseg(int seg, char *name)
{
	switch (seg) {
	case PROG: name = ".text"; break;
	case STRNG:
	case RDATA:
		/* read-only data next to the code: the references to it are in the same section */
		name = rodata_in_text ? ".text" : ".data"; break;
	case DATA:
	case LDATA:
	case PICLDATA:
	case PICDATA:
	case PICRDATA:
	case TLSDATA:
	case CTORS:
	case DTORS:
	case NMSEG: name = ".data"; break;
	case UDATA:
	case TLSUDATA: name = ".bss"; break;
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

	if (sp->sclass == EXTDEF)
		printf(PRTPREF "\t.globl %s\n", name);
	if (sp->slevel == 0)
		printf(PRTPREF "%s:\n", name);
	else
		printf(PRTPREF LABFMT ":\n", sp->soffset);
}

/*
 * Print out a string of characters as bytes (as64 reads a ; in a string as a comment).
 */
void
instring(struct symtab *sp)
{
	unsigned short sh[2];
	char *s;
	int val, cnt = 0;
	TWORD t;
	NODE *p;

	locctr(STRNG, sp);
	defloc(sp);

	t = BTYPE(sp->stype);
	s = sp->sname;
	if (t == ctype(USHORT)) {
		/* convert to UTF-16 */
		p = xbcon(0, NULL, t);
		while (*s) {
			cp2u16(u82cp(&s), sh);
			if ((glval(p) = sh[0]))
				inval(0, SZSHORT, p);
			if ((glval(p) = sh[1]))
				inval(0, SZSHORT, p);
		}
		slval(p, 0);
		inval(0, SZSHORT, p);
		nfree(p);
	} else if (t == ctype(UNSIGNED) || t == ctype(INT)) {
		/* convert to UTF-32 */
		p = xbcon(0, NULL, t);
		while (*s) {
			slval(p, u82cp(&s));
			inval(0, SZINT, p);
		}
		slval(p, 0);
		inval(0, SZINT, p);
		nfree(p);
	} else if (t == CHAR || t == UCHAR) {
		while (*s) {
			if (cnt++ == 0)
				printf(PRTPREF "\t.byte ");
			if (*s == '\\')
				val = esccon(&s);
			else
				val = *s++;
			printf("%d", val & 0377);
			if (cnt > 15) {
				cnt = 0;
				printf("\n");
			} else
				printf(",");
		}
		printf("%s0\n", cnt ? "" : PRTPREF "\t.byte ");
	} else
		cerror("instring %ld", t);
}

/* alignment: .even for a function or a halfword, else .align */
void
defalign(int al)
{
	if (al <= ALCHAR)
		return;
	if (al == ALSHORT)
		printf(PRTPREF "\t.even\n");
	else
		printf(PRTPREF "\t.align %d\n", al / ALCHAR);
}

static int
isfloat(TWORD t)
{
	return t == FLOAT || t == DOUBLE || t == LDOUBLE;
}

/*
 * How a struct or union argument or result is passed: SC_REG (up to 16 bytes, in n
 * registers) or SC_MEM.
 */
int
pdp64_sclass(TWORD t, union dimfun *df, struct ssdesc *ss, int *n)
{
	long sz = (long)(tsize(t, df, ss) / SZCHAR);

	*n = (int)((sz + 7) / 8);
	return sz > 16 ? SC_MEM : SC_REG;
}

/*
 * code for the end of a function
 * deals with struct return here
 */
void
efcode(void)
{
	struct symtab *sp;
	TWORD t;
	NODE *r, *l;
	int n, cl, i;

	sp = cftnsp;
	t = DECREF(sp->stype);
	if (t != STRTY && t != UNIONTY)
		return;

	cl = pdp64_sclass(t, sp->sdf, sp->sss, &n);
	if (cl == SC_REG) {
		/* r0 holds the address of the struct: r1 first, it is read through r0 */
		for (i = n - 1; i >= 0; i--) {
			r = block(REG, NIL, NIL, INCREF(LONG), 0, 0);
			regno(r) = R0;
			r = buildtree(UMUL, buildtree(PLUS, r, bcon(i)), NIL);
			ecomp(movtoreg(r, R0 + i));
		}
		return;
	}

	/* copy to the caller's space, and return its address */
	r = block(REG, NIL, NIL, INCREF(t), sp->sdf, sp->sss);
	regno(r) = R0;
	r = buildtree(UMUL, r, NIL);
	l = tempnode(stroffset, INCREF(t), sp->sdf, sp->sss);
	l = buildtree(UMUL, l, NIL);
	ecomp(buildtree(ASSIGN, l, r));
	l = block(REG, NIL, NIL, LONG, 0, 0);
	regno(l) = R0;
	r = tempnode(stroffset, LONG, 0, 0);
	ecomp(buildtree(ASSIGN, l, r));
}

/*
 * A struct parameter: up to 16 bytes arrive in registers (stored in the frame), a larger
 * one is in the caller's argument area and is used where it is.
 */
static void
structparam(struct symtab *sp)
{
	NODE *r;
	int cl, n, i, off, sz;

	cl = pdp64_sclass(sp->stype, sp->sdf, sp->sss, &n);
	sz = (int)(tsize(sp->stype, sp->sdf, sp->sss) / SZCHAR);
	sz = (sz + 7) & ~7;

	if (cl == SC_MEM || ngpr + n > NGPR) {
		/* in the caller's argument area */
		sp->soffset = nrsp;
		nrsp += sz * SZCHAR;
		if (cl == SC_REG)
			ngpr = NGPR;
		return;
	}
	SETOFF(autooff, SZLONG);
	autooff += sz * SZCHAR;
	off = autooff;
	for (i = 0; i < n; i++) {
		r = block(REG, NIL, NIL, LONG, 0, 0);
		regno(r) = R0 + ngpr++;
		ecomp(movtomem(r, -(off - i * SZLONG), FPREG));
	}
	sp->soffset = -off;
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
	int i, rno, n;

	ngpr = 0;
	pdp64_varfunc = cftnsp->sdf != NULL && cftnsp->sdf->dlst != 0 &&
	    pr_hasell(cftnsp->sdf->dlst);
	/* the saved registers of a variadic function sit between the frame and the stack arguments */
	nrsp = ARGINIT + (pdp64_varfunc ? 6 * SZLONG : 0);
	thisrsp = nrsp;

	if ((cftnsp->stype == STRTY+FTN || cftnsp->stype == UNIONTY+FTN) &&
	    pdp64_sclass(DECREF(cftnsp->stype), cftnsp->sdf, cftnsp->sss, &n) == SC_MEM) {
		r = block(REG, NIL, NIL, LONG, 0, 0);
		regno(r) = R0 + ngpr++;
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
		if (ngpr < NGPR)
			rno = R0 + ngpr++;
		else
			rno = -1;
		if (rno < 0) {
			sp->soffset = nrsp;
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
	thisrsp = nrsp;
}

/* called just before final exit */
/* flag is 1 if errors, 0 if none */
void
ejobcode(int flag)
{
}

void
bjobcode(void)
{
	NODE *p;
	char *c;

	astypnames[CHAR] = astypnames[UCHAR] = "\t.byte";
	astypnames[SHORT] = astypnames[USHORT] = "\t.word";
	astypnames[INT] = astypnames[UNSIGNED] = "\t.long";
	astypnames[LONG] = astypnames[ULONG] = "\t.quad";
	astypnames[LONGLONG] = astypnames[ULONGLONG] = "\t.quad";

	/* typedef char *__builtin_va_list; the arguments are in 8-byte slots */
	p = block(NAME, NIL, NIL, CHAR+PTR, 0, 0);
	c = addname("__builtin_va_list");
	p->n_sp = lookup(c, 0);
	defid(p, TYPEDEF);
	nfree(p);
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

static NODE *structstack(NODE *p);

/*
 * A struct argument.  Up to 16 bytes go to registers when there are enough left,
 * else (and when larger) it is copied into the argument area.  Everything that reads
 * the struct is hoisted ahead of the call so that no register is clobbered first.
 */
static NODE *
structarg(NODE *p)
{
	NODE *t, *tc, *cp, *w, *rv;
	int cl, n, i;

	cl = pdp64_sclass(p->n_type, p->n_df, p->pss, &n);
	if (cl == SC_MEM || ngpr + n > NGPR) {
		if (cl == SC_REG)
			ngpr = NGPR;
		return structstack(p);
	}

	/* the address of the struct, kept in a temporary for all the loads */
	t = tempnode(0, p->n_left->n_type, p->n_left->n_df, p->n_left->pss);
	tc = ccopy(t);
	cp = buildtree(ASSIGN, t, p->n_left);	/* the node is not cast: a call must keep its struct type */
	rv = NULL;
	for (i = 0; i < n; i++) {
		w = buildtree(UMUL, buildtree(PLUS,
		    cast(ccopy(tc), INCREF(LONG), 0), bcon(i)), NIL);
		if (i == 0)
			w = buildtree(COMOP, cp, w);
		w = movtoreg(w, R0 + ngpr++);
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
	int r;

	if (p->n_op == CM) {
		p->n_left = argput(p->n_left);
		p->n_right = argput(p->n_right);
		return p;
	}

	if (p->n_op == STARG)
		return structarg(p);

	if (ngpr < NGPR) {
		r = R0 + ngpr++;
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
	NODE *l, *h;
	int n;

	ngpr = nrsp = 0;
	h = NULL;
	/* A struct result of more than 16 bytes: the address of the space for it is argument 0 */
	if ((l = p->n_left)->n_type == INCREF(FTN)+STRTY ||
	    l->n_type == INCREF(FTN)+UNIONTY) {
		if (pdp64_sclass(BTYPE(l->n_type), l->n_df, l->pss, &n) == SC_MEM) {
			NODE *t = cstknode(BTYPE(l->n_type), l->n_df, l->pss);

			h = movtoreg(buildtree(ADDROF, t, NIL), R0 + ngpr++);
		}
	}

	p->n_right = argput(p->n_right);
	if (h != NULL)
		p->n_right = block(CM, h, p->n_right, INT, 0, 0);

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

/*
 * va_start: the first slot after the named parameters.  The six register slots are
 * at 16(fp) (see the prologue), the stack arguments follow them.
 */
NODE *
pdp64_builtin_stdarg_start(const struct bitable *bt, NODE *a)
{
	NODE *p, *r, *f;

	p = a->n_left;
	f = block(REG, NIL, NIL, PTR|VOID, 0, 0);
	regno(f) = FPREG;
	f = buildtree(PLUS, f, bcon(16 + thisgpr * 8 + (thisrsp - ARGINIT - 6 * SZLONG) / SZCHAR));
	r = block(ASSIGN, p, cast(f, p->n_type, 0), p->n_type, p->n_df, p->pss);
	tfree(a->n_right);
	nfree(a);
	return r;
}

/* *(T *)((ap += slot) - slot), the slot a multiple of 8 bytes */
NODE *
pdp64_builtin_va_arg(const struct bitable *bt, NODE *a)
{
	NODE *r, *dp = a->n_right;
	NODE *ap = a->n_left, *tp, *pv;
	TWORD t = dp->n_type;
	OFFSZ sz = tsize(t, dp->n_df, dp->pss) / SZCHAR;
	int slot = (int)((sz + 7) & ~7);

	tp = tempnode(0, PTR+VOID, 0, 0);	/* void *: converts to any pointer without a word */
	pv = ccopy(tp);
	r = buildtree(COMOP, buildtree(ASSIGN, tp, ccast(ccopy(ap), CHAR+PTR, 0, NULL, NULL)),
	    buildtree(COMOP, buildtree(ASSIGN, ccopy(ap), ccast(buildtree(PLUS, ccast(ccopy(pv), CHAR+PTR, 0, NULL, NULL), bcon(slot)), CHAR+PTR, 0, NULL, NULL)),
	    buildtree(UMUL, ccast(pv, INCREF(t), 0, dp->n_df, dp->pss), NIL)));
	tfree(a);
	return r;
}

NODE *
pdp64_builtin_va_end(const struct bitable *bt, NODE *a)
{
	tfree(a);
	return bcon(0);
}

NODE *
pdp64_builtin_va_copy(const struct bitable *bt, NODE *a)
{
	NODE *f;

	f = buildtree(ASSIGN, buildtree(UMUL, a->n_left, NIL),
	    buildtree(UMUL, a->n_right, NIL));
	nfree(a);
	return f;
}
