/*	$Id$	*/
/*
 *  Machine-dependent code-generation strategy (pass 2).
 */

#include <assert.h>
#include <string.h>

#include "pass2.h"

/*
 * Check size of offset in OREG.  Called by oregok() to see if an
 * OREG can be generated.  A base register with a 44-bit displacement is
 * the longest form; a symbol cannot be added to a register.
 */
int
notoff(TWORD ty, int r, CONSZ off, char *cp)
{
	if (cp && cp[0])
		return 1;
	if (off >= (1LL << 43) || off < -(1LL << 43))
		return 1;
	return 0;
}

/*
 * An address as base + index * scale + constant, found by looking through the sums:  (p + 24) + (i << 2),
 * 24 + (p + i) and p + (i << 2) + 24 are all  24(p)(i*4).  Nothing is changed here; offstar() makes
 * registers of what is not one and myormake() builds the OREG.  Returns 0 if the address is not of this
 * form.  b, i are the nodes of the base and of the index (or its operand), sh the shift (0 to 3).
 */
struct adr {
	NODE *b, *i;
	int sh;
	CONSZ c;
	int nt;
};

static int
adrscan(NODE *p, struct adr *a)
{
	NODE *x;

	if (p->n_op == PLUS)
		return adrscan(p->n_left, a) && adrscan(p->n_right, a);
	if (p->n_op == MINUS && p->n_right->n_op == ICON && p->n_right->n_name[0] == '\0') {
		a->c -= getlval(p->n_right);
		return adrscan(p->n_left, a);
	}
	if (p->n_op == ICON && p->n_name[0] == '\0') {
		a->c += getlval(p);
		return 1;
	}
	if (++a->nt > 3)
		return 0;
	x = p;
	if (x->n_op == SCONV && x->n_type == LONG && x->n_left->n_type == INT)
		x = x->n_left;			/* a pointless conversion: ints are kept sign-extended */
	if (x->n_op == LS && x->n_right->n_op == ICON && getlval(x->n_right) >= 1 && getlval(x->n_right) <= 3) {
		NODE *l = x->n_left;

		if (a->i != NULL)
			return 0;
		/* (i + k) << s is i << s with k << s added: the constant joins the displacement */
		while ((l->n_op == PLUS || l->n_op == MINUS) && l->n_right->n_op == ICON && l->n_right->n_name[0] == '\0') {
			a->c += (l->n_op == PLUS ? 1 : -1) * (getlval(l->n_right) << getlval(x->n_right));
			l = l->n_left;
		}
		a->i = l;
		a->sh = (int)getlval(x->n_right);
		return 1;
	}
	if (a->b == NULL) {
		a->b = p;
		return 1;
	}
	if (a->i == NULL) {
		a->i = p;
		a->sh = 0;
		return 1;
	}
	return 0;
}

static int
adrform(NODE *p, struct adr *a)
{
	a->b = a->i = NULL;
	a->sh = 0;
	a->c = 0;
	a->nt = 0;
	if (!adrscan(p, a))
		return 0;
	if (a->b == NULL && a->i != NULL && a->sh == 0) {
		a->b = a->i;
		a->i = NULL;
	}
	if (a->b == NULL)
		return 0;
	if (a->i == NULL && a->b->n_op == ICON)
		return 0;		/* a symbol: a memory operand of its own (NAME), not through a register */
	if (a->c >= (1LL << 36) || a->c < -(1LL << 36))
		return 0;
	return 1;
}

/*
 * Generate instructions for an OREG.  Called by swmatch().  d(Rb)(Ri*s) is an operand of the machine.
 */
void
offstar(NODE *p, int shape)
{
	struct adr a;

	if (isreg(p))
		return; /* Is already OREG */

	if (xdce && adrform(p, &a)) {
		if (a.b != NULL && !isreg(a.b))
			(void)geninsn(a.b, INAREG);
		if (a.i != NULL && !isreg(a.i))
			(void)geninsn(a.i, INAREG);
		return;
	}

	if ((p->n_op == PLUS || p->n_op == MINUS) &&
	    p->n_right->n_op == ICON && p->n_right->n_name[0] == '\0') {
		if (isreg(p->n_left) == 0)
			(void)geninsn(p->n_left, INAREG);
		/* Converted in ormake() */
		return;
	}
	(void)geninsn(p, INAREG);
}

/*
 * Called when a UMUL could not be turned into an OREG by oregok(): the
 * address is in a register by now, or will be.
 */
void
myormake(NODE *q)
{
	NODE *p;
	struct adr a;

	p = q->n_left;

	if (p->n_op == REG) {
		q->n_op = OREG;
		setlval(q, 0);
		q->n_rval = p->n_rval;
		q->n_name = "";
		tfree(p);
		return;
	}
	if (xdce && adrform(p, &a) && a.b->n_op == REG && (a.i == NULL || a.i->n_op == REG)) {
		q->n_op = OREG;
		setlval(q, a.c);
		if (a.i == NULL)
			q->n_rval = regno(a.b);
		else
			q->n_rval = R2PACK(regno(a.b), regno(a.i), a.sh);
		q->n_name = "";
		tfree(p);
	}
}

/*
 * Shape matches for UMUL.  Cooperates with offstar().
 */
int
shumul(NODE *p, int shape)
{
	if (shape & SOREG)
		return SROREG;
	return SRNOPE;
}

int
setbin(NODE *p)
{
	return 0;
}

/* setup for assignment operator */
int
setasg(NODE *p, int cookie)
{
	return 0;
}

/* setup for unary operator */
int
setuni(NODE *p, int cookie)
{
	return 0;
}

/*
 * Set evaluation order of a binary node if it differs from default.
 */
int
setorder(NODE *p)
{
	return 0;
}

/*
 * set registers in calling conventions live.
 */
int *
livecall(NODE *p)
{
	static int r[2*8+1];
	NODE *q;
	int cr = 0;

	if (optype(p->n_op) != BITYPE)
		return r[0] = -1, r;

	for (q = p->n_right; q->n_op == CM; q = q->n_left) {
		if (q->n_right->n_op == ASSIGN &&
		    q->n_right->n_left->n_op == REG)
			r[cr++] = regno(q->n_right->n_left);
	}
	if (q->n_op == ASSIGN && q->n_left->n_op == REG)
		r[cr++] = regno(q->n_left);
	r[cr++] = -1;
	return r;
}

/*
 * Signal whether the instruction is acceptable for this target.
 */
int
acceptable(struct optab *op)
{
	return 1;
}
