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
 *  Machine-dependent code-generation strategy (pass 2).
 */

#include <assert.h>
#include <string.h>

#include "pass2.h"

/*
 * Check size of offset in OREG.  Called by oregok() to see if an
 * OREG can be generated.  Any constant offset will do: adrput() prints
 * the ones that do not fit the instruction through a scratch register.
 */
int
notoff(TWORD ty, int r, CONSZ off, char *cp)
{
	if (cp && cp[0])
		return 1;	/* a symbol: needs adrp */
	return 0;
}

/*
 * Generate instructions for an OREG.  Called by swmatch().
 */
void
offstar(NODE *p, int shape)
{
	if (isreg(p))
		return; /* Is already OREG */

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
 * address is in a register by now, or will be (canon() runs before and
 * after the operands are generated).
 */
void
myormake(NODE *q)
{
	NODE *p;

	if (x2debug)
		printf("myormake(%p)\n", q);

	p = q->n_left;

	if (p->n_op == REG) {
		q->n_op = OREG;
		setlval(q, 0);
		q->n_rval = p->n_rval;
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
	/* Turns currently anything into OREG */
	if (shape & SOREG)
		return SROREG;
	return SRNOPE;
}

/*
 * Rewrite operations on binary operators (like +, -, etc...).
 * Called as a result of a failed table lookup.
 */
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
