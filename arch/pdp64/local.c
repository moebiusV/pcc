/*	$Id$	*/
/*
 *	pdp64: pass 1 machine-dependent code.
 */
#include "pass1.h"
#include <ctype.h>
#include <string.h>

#define NCHNAM	256

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

int pdp64_sclass(TWORD, union dimfun *, struct ssdesc *, int *);

/* clocal() is called to do local transformations on
 * an expression tree preparitory to its being
 * written out in intermediate code.
 *
 * the major essential job is rewriting the
 * automatic variables and arguments in terms of
 * REG and OREG nodes
 * conversion ops which are not necessary are also clobbered here
 */
NODE *
clocal(NODE *p)
{
	register struct symtab *q;
	register NODE *r, *l;
	register int o;
	register int m;
	TWORD t;

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
		}
		break;

	case SCONV:
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

		/* the same size: a load or store just takes the other type (the extension is the load's) */
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

		/* int and unsigned int are one form in a register */
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
		p->n_left->n_rval = RETREG(t);
		break;
	}
	return(p);
}

void
myp2tree(NODE *p)
{
	struct symtab *sp;

	if (p->n_op == STCALL || p->n_op == USTCALL) {
		/* pass 2 needs to know where the result comes back */
		struct attr *ap;
		int n, cl;

		/* the call's value is the address of the struct */
		cl = pdp64_sclass(ISPTR(p->n_type) ? DECREF(p->n_type) : p->n_type, NULL, p->pss, &n);
		p->n_ap = attr_add(p->n_ap, ap = attr_new(ATTR_PDP64_STRRET, 2));
		ap->iarg(0) = cl;
		ap->iarg(1) = n;
		return;
	}
	if (p->n_op != FCON)
		return;

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
	uerror("alloca and variable-length arrays are not supported on pdp64 yet");
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

/*
 * Make a name look like an external name in the local machine.  A name that is a register (r12, sp, fp)
 * cannot be a label or an operand in as64 (it is read as the register), so it is written in double quotes.
 * Mnemonics (add, swap, min, ...) are fine unquoted.
 */
char *
exname(char *p)
{
	static char buf[4][NCHNAM + 4];
	static int k;
	char *b;
	int c, q = 0;

	if (p == NULL)
		return "";
	if (p[0] == 'r' && isdigit((unsigned char)p[1])) {
		for (c = 1; isdigit((unsigned char)p[c]); c++)
			;
		q = p[c] == '\0';
	} else if (strcmp(p, "sp") == 0 || strcmp(p, "fp") == 0)
		q = 1;
	if (!q)
		return p;
	b = buf[k++ & 3];
	snprintf(b, NCHNAM + 4, "\"%s\"", p);
	return b;
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
	extern int lastloc;
	int al, ol;
	OFFSZ off;
	char *name;

	name = getexname(sp);
	off = tsize(sp->stype, sp->sdf, sp->sss);
	SETOFF(off,SZCHAR);
	off /= SZCHAR;
	al = talign(sp->stype, sp->sss)/SZCHAR;

	if (sp->sclass == STATIC) {
		ol = lastloc;
		locctr(UDATA, NULL);
		if (al > 1)
			printf(PRTPREF "\t.align %d\n", al);
		if (sp->slevel == 0)
			printf(PRTPREF "%s:\n", name);
		else
			printf(PRTPREF LABFMT ":\n", sp->soffset);
		printf(PRTPREF "\t.space %lld\n", off);
		locctr(ol, NULL);
		return;
	}
	if (sp->slevel == 0)
		printf(PRTPREF "\t.comm %s,%lld\n", name, off);
	else
		printf(PRTPREF "\t.comm " LABFMT ",%lld\n", sp->soffset, off);
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
