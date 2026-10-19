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

#include "pass1.h"

#ifndef LANG_CXX
#define	NODE P1ND
#define	ccopy p1tcopy
#define	tfree p1tfree
#define	nfree p1nfree
#define	fwalk p1fwalk
#define	talloc p1alloc
#undef n_type
#define n_type ptype
#undef n_qual
#define n_qual pqual
#undef n_df
#define n_df pdf
#else
#define sss sap
#define pss n_ap
#endif

#define IALLOC(sz)	(isinlining ? permalloc(sz) : tmpalloc(sz))

/*	this file contains code which is dependent on the target machine */

#ifdef MACHOABI
/*
 * A reference to a variable defined elsewhere goes through the GOT on Mach-O, as the linker
 * may put it in another dylib: the variable becomes *(&<sym@GOT>), where the pointer is
 * loaded with adrp sym@GOTPAGE / ldr [sym@GOTPAGEOFF] (local2.c recognizes the '@').
 */
static NODE *
picext(NODE *p)
{
	struct symtab *sp;
	NODE *q;
	char *c, *n;
	size_t len;

	c = getexname(p->n_sp);
	len = strlen(c) + 2;
	n = IALLOC(len);
	n[0] = '@';
	strlcpy(n + 1, c, len - 1);
	sp = IALLOC(sizeof(struct symtab));
	sp->sname = n;
	sp->sap = attr_new(ATTR_SONAME, 1);
	sp->sap->sarg(0) = n;
	sp->sclass = EXTERN;
	sp->sflags = sp->slevel = 0;
	q = block(NAME, NIL, NIL, INCREF(p->n_type), p->n_df, p->pss);
	q->n_sp = sp;
	q = block(UMUL, q, NIL, p->n_type, p->n_df, p->pss);
	q->n_sp = sp;
	nfree(p);
	return q;
}
#endif

/* clocal() is called to do local transformations on
 * an expression tree preparitory to its being
 * written out in intermediate code.
 *
 * the major essential job is rewriting the
 * automatic variables and arguments in terms of
 * REG and OREG nodes
 * conversion ops which are not necessary are also clobbered here
 * in addition, any special features (such as rewriting
 * exclusive or) are easily handled here as well
 */
NODE *
clocal(NODE *p)
{

	register struct symtab *q;
	register NODE *r, *l;
	register int o;
	register int m;
	TWORD t;

#ifdef PCC_DEBUG
	if (xdebug) {
		printf("clocal: %p\n", p);
		fwalk(p, eprint, 0);
	}
#endif
	switch( o = p->n_op ){

	case NAME:
		if ((q = p->n_sp) == NULL)
			return p; /* Nothing to care about */

		switch (q->sclass) {

		case PARAM:
		case AUTO:
			/* fake up a structure reference */
			r = block(REG, NIL, NIL, PTR+STRTY, 0, 0);
			slval(r, 0);
			r->n_rval = FPREG;
			p = stref(block(STREF, r, p, 0, 0, 0));
			break;

		case REGISTER:
			p->n_op = REG;
			slval(p, 0);
			p->n_rval = q->soffset;
			break;
#ifdef MACHOABI
		case EXTERN:
			if (statinit || blevel == 0 || ISFTN(p->n_type) ||
			    q->sname[0] == '@' || strncmp(q->sname, "__builtin", 9) == 0 ||
			    strncmp(q->sname, "__pcc_", 6) == 0)
				break;
			p = picext(p);
			break;
#endif
		}
		break;

	case SCONV:
		/* Special-case shifts */
		if (p->n_type == LONG && (l = p->n_left)->n_op == LS && 
		    l->n_type == INT && l->n_left->n_type == INT &&
		    l->n_right->n_op == ICON) {
			/* a signed int operand only: unsigned shifts wrap */
			p->n_left = l->n_left;
			p = buildtree(LS, p, l->n_right);
			nfree(l);
			break;
		}

		l = p->n_left;

		/* Float conversions may need extra casts */
		if (p->n_type == FLOAT || p->n_type == DOUBLE ||
		    p->n_type == LDOUBLE) {
			if (l->n_type < INT || l->n_type == BOOL) {
				p->n_left = block(SCONV, l, NIL,
				    ISUNSIGNED(l->n_type) ? UNSIGNED : INT,
				    l->n_df, l->pss);
				break;
			}
		}

		if (p->n_type == l->n_type) {
			nfree(p);
			return l;
		}

		if ((p->n_type & TMASK) == 0 && (l->n_type & TMASK) == 0 &&
		    tsize(p->n_type, p->n_df, p->pss) ==
		    tsize(l->n_type, l->n_df, l->pss)) {
			if (p->n_type != FLOAT && p->n_type != DOUBLE &&
			    l->n_type != FLOAT && l->n_type != DOUBLE &&
			    l->n_type != LDOUBLE && p->n_type != LDOUBLE) {
				if (l->n_op == NAME || l->n_op == UMUL ||
				    l->n_op == TEMP) {
					l->n_type = p->n_type;
					nfree(p);
					return l;
				}
			}
		}

		if (DEUNSIGN(p->n_type) == INT && DEUNSIGN(l->n_type) == INT &&
		    coptype(l->n_op) == BITYPE && l->n_op != COMOP &&
		    l->n_op != QUEST && l->n_op != ASSIGN && l->n_op != RS &&
		    l->n_op != DIV && l->n_op != MOD) {
			l->n_type = p->n_type;
			nfree(p);
			return l;
		}

		o = l->n_op;
		m = p->n_type;

		if (o == ICON) {
			/* if named constant and pointer, allow cast 
			   to long/ulong */
			if (!nncon(l) && (l->n_type & TMASK) &&
			    (m == LONG || m == ULONG)) {
				l->n_type = m;
				l->pss = 0;
				return nfree(p);
			}

			if (ISPTR(l->n_type) && !nncon(l))
				break; /* cannot convert named pointers */

			if (l->n_sp == 0) {
				p->n_type = ULONG;
				concast(l, m);
			} else if (m != LONG && m != ULONG)
				break;

			l->n_type = m;
			l->pss = 0;
			nfree(p);
			return l;
		} else if (l->n_op == FCON)
			cerror("SCONV FCON");
		if ((p->n_type == CHAR || p->n_type == UCHAR ||
		    p->n_type == SHORT || p->n_type == USHORT) &&
		    (l->n_type == FLOAT || l->n_type == DOUBLE ||
		    l->n_type == LDOUBLE)) {
			p = block(SCONV, p, NIL, p->n_type, p->n_df, p->pss);
			p->n_left->n_type = INT;
			return p;
		}
		break;

	case MOD:
	case DIV:
		if (o == DIV && p->n_type != CHAR && p->n_type != SHORT)
			break;
		if (o == MOD && p->n_type != CHAR && p->n_type != SHORT)
			break;
		/* make it an int division by inserting conversions */
#ifdef LANG_CXX
		p->n_left = makety(p->n_left, INT, 0, 0, 0);
		p->n_right = makety(p->n_right, INT, 0, 0, 0);
		p = makety(p, p->n_type, 0, 0, 0);
#else
		p->n_left = makety(p->n_left, tdint);
		p->n_right = makety(p->n_right, tdint);
		p = makety(p, mkqtyp(p->n_type));
#endif
		p->n_left->n_type = INT;
		break;

	case FORCE:
		/* put return value in return reg */
		p->n_op = ASSIGN;
		p->n_right = p->n_left;
		p->n_left = block(REG, NIL, NIL, p->n_type, 0, 0);
		t = p->n_type;
		if (ISITY(t))
			t = t - (FIMAG-FLOAT);
		p->n_left->n_rval = p->n_left->n_type == BOOL ? 
		    RETREG(CHAR) : RETREG(t);
		break;
	}
#ifdef PCC_DEBUG
	if (xdebug) {
		printf("clocal end: %p\n", p);
		fwalk(p, eprint, 0);
	}
#endif
	return(p);
}

int a64_sclass(TWORD, union dimfun *, struct ssdesc *, TWORD *, int *);

void
myp2tree(NODE *p)
{
	struct symtab *sp;

	if (p->n_op == STCALL || p->n_op == USTCALL) {
		/* pass 2 needs to know where the result comes back */
		struct attr *ap;
		TWORD base;
		int n, cl;

		/* the call's value is the address of the struct */
		cl = a64_sclass(ISPTR(p->n_type) ? DECREF(p->n_type) : p->n_type, NULL, p->pss, &base, &n);
		p->n_ap = attr_add(p->n_ap, ap = attr_new(ATTR_AARCH64_STRRET, 3));
		ap->iarg(0) = cl;
		ap->iarg(1) = (int)base;
		ap->iarg(2) = n;
		return;
	}
	if (p->n_op != FCON)
		return;

	{
		/* Do not lose negative zeros: +0.0 is made with fmov */
		long long ll[2];
		short ss;
#ifdef LANG_CXX
		memcpy(ll, &p->n_dcon, sizeof(ll));
#else
		memcpy(ll, p->n_scon, sizeof(ll));
#endif
		memcpy(&ss, &ll[1], sizeof(ss));
		if (ll[0] == 0 && ss == 0)
			return;
	}

	sp = IALLOC(sizeof(struct symtab));
	sp->sclass = STATIC;
	sp->sss = NULL;
	sp->sap = NULL;
	sp->slevel = 1; /* fake numeric label */
	sp->soffset = getlab();
	sp->sflags = 0;
	sp->stype = p->n_type;
	sp->squal = (CON >> TSHIFT);
	sp->sname = NULL;

	locctr(DATA, sp);
	defloc(sp);
	inval(0, tsize(sp->stype, sp->sdf, sp->sss), p);

	p->n_op = NAME;
	slval(p, 0);
	p->n_sp = sp;
}

/*
 * Convert ADDROF NAME to ICON?
 */
int
andable(NODE *p)
{
	return 1;
}

/*
 * Return 1 if a variable of type type is OK to put in register.
 */
int
cisreg(TWORD t)
{
	return 1;
}

/*
 * Allocate off bits on the stack.  p is a tree that when evaluated
 * is the multiply count for off, t is a storeable node where to write
 * the allocated address.
 */
void
spalloc(NODE *t, NODE *p, OFFSZ off)
{
	uerror("alloca and variable-length arrays are not supported on aarch64");
}

/*
 * print out a constant node, may be associated with a label.
 * Do not free the node after use.
 * off is bit offset from the beginning of the aggregate
 * fsz is the number of bits this is referring to
 */
int
ninval(CONSZ off, int fsz, NODE *p)
{
	return 0;
}

#define NCHNAM	256

/* make a name look like an external name in the local machine */
char *
exname(char *p)
{
#ifdef MACHOABI
	static char text[NCHNAM + 2];

	if (p == NULL)
		return "";
	text[0] = '_';
	strncpy(text + 1, p, NCHNAM);
	text[NCHNAM + 1] = '\0';
	return text;
#else
	return (p == NULL ? "" : p);
#endif
}

/*
 * map types which are not defined on the local machine
 */
TWORD
ctype(TWORD type)
{
	switch (BTYPE(type)) {
	case LONGLONG:
		MODTYPE(type,LONG);
		break;

	case ULONGLONG:
		MODTYPE(type,ULONG);
		break;

	case LDOUBLE:
		MODTYPE(type,DOUBLE);
		break;
	}
	return (type);
}

void
calldec(NODE *p, NODE *q) 
{
}

void
extdec(struct symtab *q)
{
}

/* make a common declaration for id, if reasonable */
void
defzero(struct symtab *sp)
{
	int al;
	OFFSZ off;
	char *name;

	name = getexname(sp);
	off = tsize(sp->stype, sp->sdf, sp->sss);
	SETOFF(off,SZCHAR);
	off /= SZCHAR;
	al = talign(sp->stype, sp->sss)/SZCHAR;

#ifdef MACHOABI
	{
		int lg = 0;

		while ((1 << lg) < al)
			lg++;
		al = lg;		/* Mach-O wants the alignment as a power of two */
	}
	if (sp->sclass == STATIC) {
		if (sp->slevel == 0)
			printf(PRTPREF "\t.lcomm %s,0%llo,%d\n", name, off, al);
		else
			printf(PRTPREF "\t.lcomm " LABFMT ",0%llo,%d\n", sp->soffset, off, al);
		return;
	}
#else
	if (sp->sclass == STATIC) {
		if (sp->slevel == 0) {
			printf(PRTPREF "\t.local %s\n", name);
		} else
			printf(PRTPREF "\t.local " LABFMT "\n", sp->soffset);
	}
#endif
	if (sp->slevel == 0) {
		printf(PRTPREF "\t.comm %s,0%llo,%d\n", name, off, al);
	} else
		printf(PRTPREF "\t.comm " LABFMT ",0%llo,%d\n", sp->soffset, off, al);
}

/*
 * Give target the opportunity of handling pragmas.
 */
int
mypragma(char *str)
{
	return 0;
}

/*
 * Called when a identifier has been declared.
 */
void
fixdef(struct symtab *sp)
{
}

void
pass1_lastchance(struct interpass *ip)
{
}
