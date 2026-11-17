/*	$Id$	*/
/*
 * Copyright (c) 2003 Anders Magnusson (ragge@ludd.luth.se).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

# include "pass1.h"

#define	ccopy p1tcopy
#undef n_type
#define	n_type ptype
#undef n_df
#define n_df pdf

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#ifndef NO_C_BUILTINS

extern int dimfuncnt;
/*
 * replace an alloca function with direct allocation on stack.
 * return a destination temp node.
 */
static P1ND *
builtin_alloca(const struct bitable *bt, P1ND *a)
{
	P1ND *t, *u;

#ifdef notyet
	if (xnobuiltins)
		return NULL;
#endif

	t = tempnode(0, VOID|PTR, 0, 0);
	u = tempnode(regno(t), VOID|PTR, 0, 0);
	spalloc(t, a, SZCHAR);
	return u;
}

/*
 * Determine if a value is known to be constant at compile-time and
 * hence that PCC can perform constant-folding on expressions involving
 * that value.
 */
static P1ND *
builtin_constant_p(const struct bitable *bt, P1ND *a)
{
	P1ND *f;
	int isconst;

	p1walkf(a, putjops, 0);
	for (f = a; f->n_op == COMOP; f = f->n_right)
		;
	isconst = nncon(f);
	p1tfree(a);
	return bcon(isconst);
}

/*
 * Hint to the compiler whether this expression will evaluate true or false.
 * Just ignored for now.
 */
static P1ND *
builtin_expect(const struct bitable *bt, P1ND *a)
{
	P1ND *f;

	if (a && a->n_op == CM) {
		p1tfree(a->n_right);
		f = a->n_left;
		p1nfree(a);
		a = f;
	}

	return a;
}

/*
 * Run-time checks (pdp64 errata 144 and 145).  Three built-ins say that a value must be in range and what to do when it is not:
 *
 *	__builtin_chk(i, n)	i, if (unsigned long)i < (unsigned long)n: an index against a length (a negative i is a huge unsigned number)
 *	__builtin_chkz(p)	p, if p is not zero: a null check
 *	__builtin_chk2(i, lo, hi) i, if lo <= i < hi (signed): an index whose lower bound is not zero
 *
 * and otherwise a call of
 *
 *	void __chk_fail(int kind, const char *file, int line, long value, unsigned long limit);
 *
 * with kind 1, 2 or 3 as above (for kind 3 the limit is hi - lo, and the value is i - lo).  The check is written in trees (a compare, a branch over
 * a failure block that calls __chk_fail), so that it works on every target; when the pdp64 has its compare-and-trap, its trap-if-zero and its two-bound
 * check and the table of check sites, the back end turns the compare, the branch and the failure block into one instruction and writes the site into
 * the table instead of the file and line arguments.  Until then the call is the stub that the table will replace.
 */
static struct symtab *chkfailsp;
static int chk_lastlab;		/* the label after the failure block of the check that chkbuild built last */

#define	chkcm(x,y)	buildtree(COMOP, x, y)
#define	CHK_BOUNDS	1
#define	CHK_ZERO	2
#define	CHK_RANGE	3

/* the call of __chk_fail(kind, file, line, value, limit), value and limit consumed */
static P1ND *
chkcall(int kind, P1ND *val, P1ND *lim)
{
	P1ND *args;

	args = buildtree(CM, bcon(kind), cast(buildtree(ADDROF, strend(ftitle, CHAR), NULL), CHAR|PTR, 0));	/* the address of the file name */
	args = buildtree(CM, args, bcon(lineno));
	args = buildtree(CM, args, cast(val, LONG, 0));
	args = buildtree(CM, args, cast(lim, ULONG, 0));
	return doacall(chkfailsp, nametree(chkfailsp), args);
}

/*
 * x converted to type t, in a temporary that is assigned in *rn (the chain of the checks's set-up), unless it is a constant: a constant is used where it
 * stands, so that the code that runs when the check passes does not load it for the call that only a failure makes.
 */
static P1ND *
chktmp(P1ND *x, TWORD t, P1ND **rn)
{
	P1ND *tn;

	x = cast(x, t, 0);
	if (x->n_op == ICON)
		return x;
	tn = tempnode(0, t, 0, 0);
	*rn = *rn == NULL ? buildtree(ASSIGN, ccopy(tn), x) : chkcm(*rn, buildtree(ASSIGN, ccopy(tn), x));
	return tn;
}

/* tv holds the value, cond is true when the check passes: tv, cond and the call's operands are built by the caller from copies */
static P1ND *
chkbuild(int kind, P1ND *init, P1ND *tv, P1ND *cond, P1ND *val, P1ND *lim)
{
	P1ND *rn;
	int lok = getlab();

	chk_lastlab = lok;
	rn = init == NULL ? buildtree(CBRANCH, cond, bcon(lok)) : chkcm(init, buildtree(CBRANCH, cond, bcon(lok)));
	rn = chkcm(rn, chkcall(kind, val, lim));
	rn = chkcm(rn, nlabel(lok));
	return chkcm(rn, tv);
}

static P1ND *
builtin_chk(const struct bitable *bt, P1ND *a)
{
	P1ND *i, *n, *t, *tn, *rn = NULL, *cond, *val;

	if (a == NULL || a->n_op != CM)
		return bcon(0);
	i = a->n_left;
	n = a->n_right;
	p1nfree(a);
	t = chktmp(i, LONG, &rn);
	tn = chktmp(n, ULONG, &rn);
	cond = buildtree(LT, cast(ccopy(t), ULONG, 0), ccopy(tn));
	val = ccopy(t);
	return chkbuild(CHK_BOUNDS, rn, t, cond, val, tn);
}

static P1ND *
builtin_chkz(const struct bitable *bt, P1ND *a)
{
	P1ND *t, *rn, *cond, *val;

	if (a == NULL || a->n_op == CM)
		return bcon(0);
	t = tempnode(0, a->n_type, a->n_df, a->pss);
	rn = buildtree(ASSIGN, ccopy(t), a);
	cond = buildtree(NE, ccopy(t), bcon(0));
	val = ccopy(t);
	return chkbuild(CHK_ZERO, rn, t, cond, val, bcon(0));
}

static P1ND *
builtin_chk2(const struct bitable *bt, P1ND *a)
{
	P1ND *i, *lo, *hi, *t, *tl, *th, *rn = NULL, *cond, *val, *lim;

	if (a == NULL || a->n_op != CM || a->n_left->n_op != CM)
		return bcon(0);
	hi = a->n_right;
	lo = a->n_left->n_right;
	i = a->n_left->n_left;
	p1nfree(a->n_left);
	p1nfree(a);
	t = chktmp(i, LONG, &rn);
	tl = chktmp(lo, LONG, &rn);
	th = chktmp(hi, LONG, &rn);
	/* the copies of tl, th and t first, then the originals: buildtree frees the nodes it is given */
	cond = buildtree(LT, cast(buildtree(MINUS, ccopy(t), ccopy(tl)), ULONG, 0),
	    cast(buildtree(MINUS, ccopy(th), ccopy(tl)), ULONG, 0));
	val = buildtree(MINUS, ccopy(t), ccopy(tl));
	lim = buildtree(MINUS, th, tl);
	return chkbuild(CHK_RANGE, rn, t, cond, val, lim);
}


/* the subscript i of an array of n elements: i as a long, with the check of __builtin_chk(i, n) */
static P1ND *
chk_index(P1ND *i, int n)
{
	P1ND *t, *rn = NULL, *cond, *val, *lim;

	t = chktmp(i, LONG, &rn);
	cond = buildtree(LT, cast(ccopy(t), ULONG, 0), cast(bcon(n), ULONG, 0));
	val = ccopy(t);
	lim = cast(bcon(n), ULONG, 0);
	return chkbuild(CHK_BOUNDS, rn, t, cond, val, lim);
}

/*
 * -xchecks drops the checks that it can prove.  A subscript is provable when its range lies inside [0, n):
 *   - from the subscript alone: a constant, a mask (i & 7), an unsigned char or short, an unsigned remainder (u % 10), sums of such;
 *   - from a fact about a loop variable: in `for (i = c; i < K; i += s)` with c >= 0, s > 0 and K a constant, the body sees c <= i < K for as long as it does not
 *     change i; for an unsigned i the lower bound is the type's.  The fact holds only if the body never assigns i, never takes its address and has no label that a
 *     goto could enter by, and if i is a plain local (not static, not volatile) whose address is taken nowhere in the function.  The body is not known when
 *     the check is built, so such a check is built, noted, and the decision is made when the function is complete (chk_prune, called from pass 2): the
 *     branch of the check becomes a jump past its failure block, which is deleted.
 */
struct chkfact { long sp; long lo, hi; int valid; long dep; int kind; };	/* kind 0: a loop's, 1: a guard's */
#define	BIG	(1L << 40)   /* sp: the number of the temporary that holds the variable (-1 none) */
struct chkpend { int lok, nf; struct chkfact *f[4]; };
#define	MAXFACTS	512
#define	MAXPEND		16384
static struct chkfact facts[MAXFACTS];
static int nfacts;
static struct chkfact *active[64];
static int nactive;
static struct chkpend pend[MAXPEND];
static int npend;
static long taken[1024];
static int ntaken;
static int scope[256], nscope;
static int nif, nifover;
static void chk_scope_push(void);
static void chk_scope_pop(void);
#define	NLAST	4096
static struct { int set; long v; } lastc[NLAST];
static int wcount[NLAST];	/* the writes to each temporary in the function so far */
extern int (*chk_elided_hook)(int);
extern void (*chk_reset_hook)(void);

static void
chk_reset(void)
{
	nfacts = nactive = npend = ntaken = nscope = 0;
	nif = nifover = 0;
	memset(lastc, 0, sizeof lastc);
	memset(wcount, 0, sizeof wcount);
}

/* called from buildtree: l is written, or has its address taken.  Only the locals that the compiler keeps in temporaries are followed: a variable in memory can be changed through a pointer. */
void
chk_note(int o, P1ND *l, P1ND *rr)
{
	int i;

	if (l == NULL || l->n_op != TEMP)
		return;
	if (o != ADDROF && (o == ASSIGN || o == PLUSEQ || o == MINUSEQ || o == MULEQ || o == DIVEQ || o == MODEQ || o == ANDEQ || o == OREQ ||
	    o == EREQ || o == LSEQ || o == RSEQ || o == INCR || o == DECR) && l->n_rval >= 0 && l->n_rval < NLAST) {
		wcount[l->n_rval]++;
		lastc[l->n_rval].set = o == ASSIGN && rr != NULL && rr->n_op == ICON && rr->n_sp == NULL;
		if (lastc[l->n_rval].set)
			lastc[l->n_rval].v = (long)glval(rr);
	}
	switch (o) {
	case ADDROF:
		if (ntaken < (int)(sizeof(taken)/sizeof(taken[0])))
			taken[ntaken++] = l->n_rval;
		/* FALLTHROUGH: an address taken inside the loop ends the fact as a write does */
	case ASSIGN: case PLUSEQ: case MINUSEQ: case MULEQ: case DIVEQ: case MODEQ:
	case ANDEQ: case OREQ: case EREQ: case LSEQ: case RSEQ: case INCR: case DECR:
		for (i = 0; i < nactive; i++)
			if (active[i]->sp == l->n_rval)
				active[i]->valid = 0;
		break;
	}
}

/* a case or default label: entered past the guards before the switch, not past a loop's test */
void
chk_caselabel(void)
{
	int i;

	for (i = 0; i < nactive; i++)
		if (active[i]->kind == 1)
			active[i]->valid = 0;
}

/* a user label inside a loop body: a goto may enter the body past the loop's test */
void
chk_userlabel(void)
{
	int i;

	for (i = 0; i < nactive; i++)
		active[i]->valid = 0;
}

static long
chk_var(P1ND *p)	/* the number of the temporary of an integer local, or -1 */
{
	if (p == NULL || p->n_op != TEMP || !ISINTEGER(p->n_type) || p->n_type > ULONGLONG)
		return -1;
	return p->n_rval;
}

static int
chk_const(P1ND *p, long *v)
{
	if (p != NULL && p->n_op == ICON && p->n_sp == NULL) {
		*v = (long)glval(p);
		return 1;
	}
	if (p != NULL && p->n_op == UMINUS && p->n_left->n_op == ICON && p->n_left->n_sp == NULL) {
		*v = -(long)glval(p->n_left);
		return 1;
	}
	return 0;
}

/*
 * The upper limit of a loop that is not a constant: a variable n in `i < n`.  A narrow type bounds it for good (an unsigned char is at most 255); a
 * variable of any other type bounds it if it is a local temporary that the function writes exactly once, with a constant, before the loop (chk_elided
 * checks the once at the end of the function; *dep is that variable).  Returns 1 and the largest value k of the variable.
 */
static int
chk_bound(P1ND *p, long *k, long *dep)
{
	TWORD ty;

	*dep = -1;
	if (chk_const(p, k))
		return 1;
	while (p != NULL && (p->n_op == SCONV || p->n_op == PCONV) && ISINTEGER(p->n_type) && ISINTEGER(p->n_left->n_type))
		p = p->n_left;
	if (p == NULL || chk_var(p) < 0)
		return 0;
	ty = p->n_type;
	if (ty == UCHAR) { *k = 255; return 1; }
	if (ty == USHORT) { *k = 65535; return 1; }
	if (ty == SHORT) { *k = 32767; return 1; }
	if (p->n_rval < NLAST && lastc[p->n_rval].set) {
		*k = lastc[p->n_rval].v;
		*dep = p->n_rval;
		return 1;
	}
	return 0;
}

static struct { long sp; long hi; int ne, haveinit, unsg; long c0; long dep; } forcand;

/* the init c0 of sp in `for (init; cond; ...)`: the last `sp = constant` of a chain of commas, unless something else writes sp */
static int
chk_init(P1ND *p, long sp, long *c0, int *ok)
{
	long v;

	if (p == NULL)
		return 0;
	if (p->n_op == COMOP) {
		chk_init(p->n_left, sp, c0, ok);
		return chk_init(p->n_right, sp, c0, ok);
	}
	if (p->n_op == ASSIGN && chk_var(p->n_left) == sp) {
		if (chk_const(p->n_right, &v)) {
			*c0 = v;
			*ok = 1;
		} else
			*ok = 0;
	}
	return *ok;
}

/* the first part of a for statement: its init and its condition (not evaluated yet); init is NULL for the form with a declaration */
void
chk_for_cond(P1ND *init, P1ND *cond)
{
	long sp = -1;
	long k, c0 = 0, hi = 0, dep = -1;
	int ne = 0, ok = 0, haveinit = 0;
	P1ND *l, *r;

	memset(&forcand, 0, sizeof forcand);
	if (!xchecks || cond == NULL || cond->n_op < EQ || cond->n_op > UGT)
		return;
	l = cond->n_left;
	r = cond->n_right;
	if ((sp = chk_var(l)) >= 0 && chk_bound(r, &k, &dep)) {
		switch (cond->n_op) {
		case LT: case ULT: hi = k; break;
		case LE: case ULE: hi = k + 1; break;
		case NE: hi = k; ne = 1; break;
		default: return;
		}
	} else if ((sp = chk_var(r)) >= 0 && chk_bound(l, &k, &dep)) {
		switch (cond->n_op) {
		case GT: case UGT: hi = k; break;
		case GE: case UGE: hi = k + 1; break;
		case NE: hi = k; ne = 1; break;
		default: return;
		}
	} else
		return;
	if (hi < 0)
		return;
	if (init != NULL)
		haveinit = chk_init(init, sp, &c0, &ok) && ok;
	else if (sp < NLAST && lastc[sp].set) {		/* the form with a declaration: its initializer was the last assignment */
		haveinit = 1;
		c0 = lastc[sp].v;
	}
	forcand.sp = sp;
	forcand.dep = dep;
	forcand.unsg = ISUNSIGNED(l->n_type);
	forcand.hi = hi;
	forcand.ne = ne;
	forcand.haveinit = haveinit;
	forcand.c0 = c0;
}

/* how sp changes in the third part of the for: >0 the step (it only increases), 0 it is not mentioned, -1 anything else */
static long
chk_step(P1ND *p, long sp)
{
	long v, a, b;

	if (p == NULL)
		return 0;
	switch (p->n_op) {
	case COMOP:
		a = chk_step(p->n_left, sp);
		b = chk_step(p->n_right, sp);
		if (a < 0 || b < 0 || (a > 0 && b > 0))
			return -1;
		return a > b ? a : b;
	case INCR:
		if (chk_var(p->n_left) == sp)
			return chk_const(p->n_right, &v) && v > 0 ? v : -1;
		return 0;
	case PLUSEQ:
		if (chk_var(p->n_left) == sp)
			return chk_const(p->n_right, &v) && v > 0 ? v : -1;
		return 0;
	case ASSIGN:
		if (chk_var(p->n_left) == sp) {
			if (p->n_right->n_op == PLUS && chk_var(p->n_right->n_left) == sp && chk_const(p->n_right->n_right, &v) && v > 0)
				return v;
			return -1;
		}
		return 0;
	case TEMP: case ICON:
		return 0;
	default:
		return -1;		/* a call, a store through a pointer, anything: not understood */
	}
}

/* after the third part, before the body: the fact about the loop variable, if the loop has the shape of one */
void
chk_for_push(P1ND *incr)
{
	long sp = forcand.sp;
	long step, lo;
	int pushed = 0;

	if (!xchecks)
		return;
	chk_scope_push();
	if (sp >= 0 && nfacts < MAXFACTS && nactive < 250 && nscope < 250) {
		step = chk_step(incr, sp);
		if (forcand.haveinit && step > 0 && forcand.c0 >= 0 && (!forcand.ne || (step == 1 && forcand.c0 <= forcand.hi)))
			lo = forcand.c0;
		else if ((forcand.unsg) && step >= 0 && !forcand.ne)
			lo = 0;
		else
			lo = -1;
		if (lo >= 0) {
			facts[nfacts].sp = sp;
			facts[nfacts].lo = lo;
			facts[nfacts].hi = forcand.hi;
			facts[nfacts].valid = 1;
			facts[nfacts].dep = forcand.dep;
			facts[nfacts].kind = 0;
			active[nactive++] = &facts[nfacts++];
			pushed = 1;
		}
	}
	memset(&forcand, 0, sizeof forcand);
}

/* after the body */
void
chk_for_pop(void)
{
	if (xchecks)
		chk_scope_pop();
}

/*
 * Guards.  The condition of an `if`, `while` or `for` tells what is known where it holds: in the body of `if (i < 10)`, in the else of `if (i >= 10)`, after
 * `if (i >= 10) return;`, in the body of `while (i < n)`.  The facts are of the same kind as a loop's (chkfact): they end when the variable is written,
 * and they are decided when the function is complete.  Early exit: if the then-part of an `if` without else never reaches its end, the negation of the
 * condition holds from there to the end of the enclosing statement; a scope is closed at the end of each compound statement and each controlled statement,
 * and a label that can be entered from elsewhere ends the guards (a case label ends those of before the switch).
 */
struct chkrec { struct { long sp, lo, hi; } f[8]; int n; };
static struct { int mark; struct chkrec neg; } ifstk[256];

static void
chk_scope_push(void)
{
	if (nscope < 256)
		scope[nscope++] = nactive;
}

static void
chk_scope_pop(void)
{
	if (nscope > 0)
		nactive = scope[--nscope];
}

static void
chk_addrec(struct chkrec *rc, long sp, long lo, long hi)
{
	if (rc->n < 8) {
		rc->f[rc->n].sp = sp;
		rc->f[rc->n].lo = lo;
		rc->f[rc->n].hi = hi;
		rc->n++;
	}
}

static int
chk_negop(int o)
{
	switch (o) {
	case EQ: return NE; case NE: return EQ;
	case LT: return GE; case GE: return LT; case LE: return GT; case GT: return LE;
	case ULT: return UGE; case UGE: return ULT; case ULE: return UGT; case UGT: return ULE;
	}
	return o;
}

static int
chk_flipop(int o)
{
	switch (o) {
	case LT: return GT; case GT: return LT; case LE: return GE; case GE: return LE;
	case ULT: return UGT; case UGT: return ULT; case ULE: return UGE; case UGE: return ULE;
	}
	return o;
}

/* what is known about the variables when the condition c has the value truth */
static void
chk_collect(P1ND *c, int truth, struct chkrec *rc)
{
	long k, sp, lo = -BIG, hi = BIG;
	int o;
	P1ND *l, *r;

	if (c == NULL)
		return;
	switch (c->n_op) {
	case NOT:
		chk_collect(c->n_left, !truth, rc);
		return;
	case ANDAND:
		if (truth) {
			chk_collect(c->n_left, 1, rc);
			chk_collect(c->n_right, 1, rc);
		}
		return;
	case OROR:
		if (!truth) {
			chk_collect(c->n_left, 0, rc);
			chk_collect(c->n_right, 0, rc);
		}
		return;
	case EQ: case NE: case LT: case LE: case GT: case GE: case ULT: case ULE: case UGT: case UGE:
		break;
	default:
		return;
	}
	o = c->n_op;
	l = c->n_left;
	r = c->n_right;
	if (!chk_const(r, &k)) {
		if (!chk_const(l, &k))
			return;
		l = r;
		o = chk_flipop(o);
	}
	if (!truth)
		o = chk_negop(o);
	/* an unsigned compare of a variable that was converted to the other signedness of its own size */
	if ((o == ULT || o == ULE || o == UGT || o == UGE) && l->n_op == SCONV && ISINTEGER(l->n_type) && ISINTEGER(l->n_left->n_type) &&
	    DEUNSIGN(l->n_type) == DEUNSIGN(l->n_left->n_type))
		l = l->n_left;
	if ((sp = chk_var(l)) < 0 || k < -BIG || k > BIG)
		return;
	switch (o) {
	case EQ: lo = k; hi = k + 1; break;
	case LT: hi = k; break;
	case LE: hi = k + 1; break;
	case GT: lo = k + 1; break;
	case GE: lo = k; break;
	case ULT: if (k < 0) return; lo = 0; hi = k; break;
	case ULE: if (k < 0) return; lo = 0; hi = k + 1; break;
	case UGT: if (k < 0 || !ISUNSIGNED(l->n_type)) return; lo = k + 1; break;
	case UGE: if (k < 0 || !ISUNSIGNED(l->n_type)) return; lo = k; break;
	default: return;
	}
	chk_addrec(rc, sp, lo, hi);
}

static void
chk_pushrec(struct chkrec *rc)
{
	int i;

	for (i = 0; i < rc->n && nfacts < MAXFACTS && nactive < 250; i++) {
		facts[nfacts].sp = rc->f[i].sp;
		facts[nfacts].lo = rc->f[i].lo;
		facts[nfacts].hi = rc->f[i].hi;
		facts[nfacts].valid = 1;
		facts[nfacts].dep = -1;
		facts[nfacts].kind = 1;
		active[nactive++] = &facts[nfacts++];
	}
}

/* while: the condition holds in the body */
void
chk_while_begin(P1ND *c)
{
	struct chkrec rc;

	if (!xchecks)
		return;
	chk_scope_push();
	rc.n = 0;
	chk_collect(c, 1, &rc);
	chk_pushrec(&rc);
}

void
chk_scope_end(void)
{
	if (xchecks)
		chk_scope_pop();
}

void
chk_if_begin(P1ND *c)
{
	struct chkrec rc;

	if (!xchecks)
		return;
	if (nif >= 256) {
		nifover++;
		return;
	}
	ifstk[nif].mark = nactive;
	ifstk[nif].neg.n = 0;
	chk_collect(c, 0, &ifstk[nif].neg);
	nif++;
	rc.n = 0;
	chk_collect(c, 1, &rc);
	chk_pushrec(&rc);
}

/* at the else */
void
chk_if_else(void)
{
	if (!xchecks || nifover || nif == 0)
		return;
	nactive = ifstk[nif - 1].mark;
	chk_pushrec(&ifstk[nif - 1].neg);
}

/* at the end of the statement; exits: the then-part of an if without else never reaches its end */
void
chk_if_end(int haselse, int exits)
{
	if (!xchecks)
		return;
	if (nifover) {
		nifover--;
		return;
	}
	if (nif == 0)
		return;
	nif--;
	nactive = ifstk[nif].mark;
	if (!haselse && exits)
		chk_pushrec(&ifstk[nif].neg);
}


/* the range [*lo, *hi) of an evaluated expression, when it is known; *used lists the facts that the answer depends on */
static int
chk_range(P1ND *p, long *lo, long *hi, struct chkfact **used, int *nused)
{
	long l1, h1, l2, h2, v;
	int i;

	switch (p->n_op) {
	case ICON:
		if (p->n_sp != NULL)
			return 0;
		*lo = (long)glval(p);
		*hi = *lo + 1;
		return 1;
	case TEMP:
		{
			long fl = -BIG, fh = BIG;
			int found = 0;

			for (i = nactive - 1; i >= 0; i--)
				if (active[i]->sp == p->n_rval && active[i]->valid && *nused < 4) {
					used[(*nused)++] = active[i];
					if (active[i]->lo > fl) fl = active[i]->lo;
					if (active[i]->hi < fh) fh = active[i]->hi;
					found = 1;
				}
			if (!found)
				break;
			if (ISUNSIGNED(p->n_type) && fl < 0)
				fl = 0;
			*lo = fl;
			*hi = fh;
			return fh > fl;
		}
	case SCONV: case PCONV:
		{
			if (!ISINTEGER(p->n_type) || !ISINTEGER(p->n_left->n_type))
				return 0;
			if (!chk_range(p->n_left, &l1, &h1, used, nused))
				return 0;
			if (l1 < 0 || h1 > 0x7fffffffL)
				return 0;
			*lo = l1;
			*hi = h1;
			return 1;
		}
		break;
	case PLUS: case MINUS:
		if (chk_range(p->n_left, &l1, &h1, used, nused) && chk_range(p->n_right, &l2, &h2, used, nused)) {
			if (p->n_op == PLUS) {
				*lo = l1 + l2;
				*hi = h1 + h2 - 1;
			} else {
				*lo = l1 - (h2 - 1);
				*hi = h1 - l2;
			}
			return *hi > *lo;
		}
		return 0;
	case AND:
		if (p->n_right->n_op == ICON && p->n_right->n_sp == NULL && (v = (long)glval(p->n_right)) >= 0) {
			*lo = 0;
			*hi = v + 1;
			return 1;
		}
		return 0;
	case MOD:
		if (p->n_right->n_op == ICON && p->n_right->n_sp == NULL && (v = (long)glval(p->n_right)) > 0 &&
		    chk_range(p->n_left, &l1, &h1, used, nused) && l1 >= 0) {
			*lo = 0;
			*hi = v;
			return 1;
		}
		return 0;
	}
	/* a variable or other expression of an unsigned small type: the type's range */
	if (p->n_type == UCHAR) {
		*lo = 0; *hi = 256;
		return 1;
	}
	if (p->n_type == USHORT) {
		*lo = 0; *hi = 65536;
		return 1;
	}
	return 0;
}

/* decided in pass 2 when the function is complete: is the check with failure-ok label lok provable? */
int
chk_elided(int lok)
{
	int i, j, k;

	for (i = 0; i < npend; i++) {
		if (pend[i].lok != lok)
			continue;
		for (j = 0; j < pend[i].nf; j++) {
			struct chkfact *f = pend[i].f[j];

			if (!f->valid)
				return 0;
			if (f->dep >= 0) {
				if (wcount[f->dep] != 1)
					return 0;
				for (k = 0; k < ntaken; k++)
					if (taken[k] == f->dep)
						return 0;
			}
			for (k = 0; k < ntaken; k++)
				if (taken[k] == f->sp)
					return 0;
		}
		return 1;
	}
	return 0;
}

/*
 * -xchecks: the subscript i of an array of known size n is checked as __builtin_chk(i, n) does (the call of eve() for a[i] and i[a]).  Not checked: an array
 * whose size is not known or is dynamic, an array of one element or none (the trailing `char name[1]` that stands for a longer array), a constant
 * subscript that is in range, an array that is a function parameter (it is a pointer), and a[i] under & (&a[n], one past the end, is a valid address).
 */
void
chk_subscript(P1ND **a, P1ND **b)
{
	P1ND *x = *a, *i = *b, *t;
	int n, swap = 0;

	if (!ISARY(x->n_type) && ISARY(i->n_type)) {		/* i[a] */
		t = x; x = i; i = t;
		swap = 1;
	}
	if (!ISARY(x->n_type) || x->n_df == NULL || !ISINTEGER(i->n_type))
		return;
	n = x->n_df->ddim;
	if (n <= 1)
		return;
	if (i->n_op == ICON && i->n_sp == NULL && glval(i) >= 0 && glval(i) < n)
		return;
	{
		long lo, hi;
		struct chkfact *used[4];
		int nused = 0;

		if (chk_range(i, &lo, &hi, used, &nused) && lo >= 0 && hi <= n) {
			if (nused == 0)
				return;			/* provable from the subscript alone */
			i = chk_index(i, n);		/* provable if the loop's fact holds: decided when the function is complete */
			if (npend < MAXPEND) {
				pend[npend].lok = chk_lastlab;
				pend[npend].nf = nused;
				memcpy(pend[npend].f, used, nused * sizeof used[0]);
				npend++;
			}
			goto done;
		}
	}
	i = chk_index(i, n);
done:
	if (swap) {
		*a = i;
		*b = x;
	} else
		*b = i;
}

/*
 * Take integer absolute value.
 * Simply does: ((((x)>>(8*sizeof(x)-1))^(x))-((x)>>(8*sizeof(x)-1)))
 */
static P1ND *
builtin_abs(const struct bitable *bt, P1ND *a)
{
	P1ND *p, *q, *r, *t, *t2, *t3;
	int tmp1, tmp2, shift;

	if (a->n_type != INT)
		a = cast(a, INT, 0);

	if (a->n_op == ICON) {
		if (glval(a) < 0)
			slval(a, -glval(a));
		p = a;
	} else {
		t = tempnode(0, a->n_type, a->n_df, a->pss);
		tmp1 = regno(t);
		p = buildtree(ASSIGN, t, a);

		t = tempnode(tmp1, a->n_type, a->n_df, a->pss);
		shift = (int)tsize(a->n_type, a->n_df, a->pss) - 1;
		q = buildtree(RS, t, bcon(shift));

		t2 = tempnode(0, a->n_type, a->n_df, a->pss);
		tmp2 = regno(t2);
		q = buildtree(ASSIGN, t2, q);

		t = tempnode(tmp1, a->n_type, a->n_df, a->pss);
		t2 = tempnode(tmp2, a->n_type, a->n_df, a->pss);
		t3 = tempnode(tmp2, a->n_type, a->n_df, a->pss);
		r = buildtree(MINUS, buildtree(ER, t, t2), t3);

		p = buildtree(COMOP, p, buildtree(COMOP, q, r));
	}

	return p;
}

#define	cmop(x,y) buildtree(COMOP, x, y)
#define	lblnod(l) nlabel(l)

#ifndef TARGET_BSWAP
static P1ND *
builtin_bswap16(const struct bitable *bt, P1ND *a)
{
	P1ND *f, *t1, *t2;

	t1 = buildtree(LS, buildtree(AND, ccopy(a), bcon(255)), bcon(8));
	t2 = buildtree(AND, buildtree(RS, a, bcon(8)), bcon(255));
	f = buildtree(OR, t1, t2);
	return f;
}

static P1ND *
builtin_bswap32(const struct bitable *bt, P1ND *a)
{
	P1ND *f, *t1, *t2, *t3, *t4;

	t1 = buildtree(LS, buildtree(AND, ccopy(a), bcon(255)), bcon(24));
	t2 = buildtree(LS, buildtree(AND, ccopy(a), bcon(255 << 8)), bcon(8));
	t3 = buildtree(AND, buildtree(RS, ccopy(a), bcon(8)), bcon(255 << 8));
	t4 = buildtree(AND, buildtree(RS, a, bcon(24)), bcon(255));
	f = buildtree(OR, buildtree(OR, t1, t2), buildtree(OR, t3, t4));
	return f;
}

static P1ND *
builtin_bswap64(const struct bitable *bt, P1ND *a)
{
	P1ND *f, *t1, *t2, *t3, *t4, *t5, *t6, *t7, *t8;

#define	XBCON(x) xbcon(x, NULL, ctype(ULONGLONG))
	t1 = buildtree(LS, buildtree(AND, ccopy(a), XBCON(255)), bcon(56));
	t2 = buildtree(LS, buildtree(AND, ccopy(a), XBCON(255 << 8)), bcon(40));
	t3 = buildtree(LS, buildtree(AND, ccopy(a), XBCON(255 << 16)), bcon(24));
	t4 = buildtree(LS, buildtree(AND, ccopy(a), XBCON(255 << 24)), bcon(8));
	t5 = buildtree(AND, buildtree(RS, ccopy(a), bcon(8)), XBCON(255 << 24));
	t6 = buildtree(AND, buildtree(RS, ccopy(a), bcon(24)), XBCON(255 << 16));
	t7 = buildtree(AND, buildtree(RS, ccopy(a), bcon(40)), XBCON(255 << 8));
	t8 = buildtree(AND, buildtree(RS, a, bcon(56)), XBCON(255));
	f = buildtree(OR,
	    buildtree(OR, buildtree(OR, t1, t2), buildtree(OR, t3, t4)),
	    buildtree(OR, buildtree(OR, t5, t6), buildtree(OR, t7, t8)));
	return f;
#undef XBCON
}

#endif

#ifndef TARGET_CXZ
/*
 * Find number of beginning 0's in a word of type t.
 * t should be deunsigned.
 */
static P1ND *
builtin_cxz(P1ND *a, TWORD t, int isclz)
{
	P1ND *t101, *t102;
	P1ND *rn, *p;
	int l15, l16, l17;
	int sz;

	t = ctype(t);
	sz = (int)tsize(t, 0, 0);

	t101 = tempnode(0, INT, 0, 0);
	t102 = tempnode(0, t, 0, 0);
	l15 = getlab();
	l16 = getlab();
	l17 = getlab();
	rn = buildtree(ASSIGN, ccopy(t102), a);
	rn = cmop(rn, buildtree(ASSIGN, ccopy(t101), bcon(0)));
	rn = cmop(rn, lblnod(l16));

	p = buildtree(CBRANCH, buildtree(GE, ccopy(t101), bcon(sz)), bcon(l15));
	rn = cmop(rn, p);
	if (isclz) {
		p = buildtree(CBRANCH,
		    buildtree(GE, ccopy(t102), bcon(0)), bcon(l17));
	} else {
		p = buildtree(CBRANCH,
		    buildtree(EQ, buildtree(AND, ccopy(t102), bcon(1)),
		    bcon(0)), bcon(l17));
	}
	rn = cmop(rn, p);

	rn = cmop(rn, block(GOTO, bcon(l15), NULL, INT, 0, 0));

	rn = cmop(rn, lblnod(l17));
	rn = cmop(rn, buildtree(isclz ? LSEQ : RSEQ , t102, bcon(1)));

	rn = cmop(rn, buildtree(INCR, ccopy(t101), bcon(1)));

	rn = cmop(rn, block(GOTO, bcon(l16), NULL, INT, 0, 0));
	rn = cmop(rn, lblnod(l15));
	return cmop(rn, t101);
}

static P1ND *
builtin_clz(const struct bitable *bt, P1ND *a)
{
	return builtin_cxz(a, INT, 1);
}

static P1ND *
builtin_clzl(const struct bitable *bt, P1ND *a)
{
	return builtin_cxz(a, LONG, 1);
}

static P1ND *
builtin_clzll(const struct bitable *bt, P1ND *a)
{
	return builtin_cxz(a, LONGLONG, 1);
}

static P1ND *
builtin_ctz(const struct bitable *bt, P1ND *a)
{
	return builtin_cxz(a, INT, 0);
}

static P1ND *
builtin_ctzl(const struct bitable *bt, P1ND *a)
{
	return builtin_cxz(a, LONG, 0);
}

static P1ND *
builtin_ctzll(const struct bitable *bt, P1ND *a)
{
	return builtin_cxz(a, LONGLONG, 0);
}
#endif

#ifndef TARGET_ERA
static P1ND *
builtin_era(const struct bitable *bt, P1ND *a)
{
	return a;	/* Just pass through */
}
#endif

#ifndef TARGET_FFS
/*
 * Find number of beginning 0's in a word of type t.
 * t should be deunsigned.
 */
static P1ND *
builtin_ff(P1ND *a, TWORD t)
{
	P1ND *t101, *t102;
	P1ND *rn, *p;
	int l15, l16, l17;
	int sz;

	t = ctype(t);
	sz = (int)tsize(t, 0, 0)+1;

	t101 = tempnode(0, INT, 0, 0);
	t102 = tempnode(0, t, 0, 0);
	l15 = getlab();
	l16 = getlab();
	l17 = getlab();
	rn = buildtree(ASSIGN, ccopy(t101), bcon(0));
	rn = cmop(rn, buildtree(ASSIGN, ccopy(t102), a));

	p = buildtree(CBRANCH, buildtree(EQ, ccopy(t102), bcon(0)), bcon(l15));
	rn = cmop(rn, p);

	rn = cmop(rn, buildtree(INCR, ccopy(t101), bcon(1)));

	rn = cmop(rn, lblnod(l16));

	p = buildtree(CBRANCH, buildtree(GE, ccopy(t101), bcon(sz)), bcon(l15));
	rn = cmop(rn, p);

	p = buildtree(CBRANCH,
	    buildtree(EQ, buildtree(AND, ccopy(t102), bcon(1)),
	    bcon(0)), bcon(l17));
	rn = cmop(rn, p);

	rn = cmop(rn, block(GOTO, bcon(l15), NULL, INT, 0, 0));

	rn = cmop(rn, lblnod(l17));
	rn = cmop(rn, buildtree(RSEQ, t102, bcon(1)));

	rn = cmop(rn, buildtree(INCR, ccopy(t101), bcon(1)));

	rn = cmop(rn, block(GOTO, bcon(l16), NULL, INT, 0, 0));
	rn = cmop(rn, lblnod(l15));
	return cmop(rn, t101);
}

static P1ND *
builtin_ffs(const struct bitable *bt, P1ND *a)
{
	return builtin_ff(a, INT);
}

static P1ND *
builtin_ffsl(const struct bitable *bt, P1ND *a)
{
	return builtin_ff(a, LONG);
}

static P1ND *
builtin_ffsll(const struct bitable *bt, P1ND *a)
{
	return builtin_ff(a, LONGLONG);
}
#endif

static P1ND *
builtin_popcnt(P1ND *a, TWORD t)
{
	P1ND *t101, *t102;
	P1ND *rn, *p;
	int l15, l16, l17;

	t101 = tempnode(0, INT, 0, 0); /* counter for set bits */
	t102 = tempnode(0, t, 0, 0); /* input data */
	l15 = getlab();
	l16 = getlab();
	l17 = getlab();
	rn = buildtree(ASSIGN, ccopy(t101), bcon(0)); /* bit counter set to 0 */
	p = buildtree(ASSIGN, ccopy(t102), a); /* input data */
	rn = cmop(rn, p);

	rn = cmop(rn, lblnod(l15)); /* place a label here */

	p = buildtree(CBRANCH,
	    buildtree(EQ, buildtree(AND, ccopy(t102), bcon(1)),
	    bcon(0)), bcon(l16)); /* if (t103 & 1) == 0, goto l16 */
	rn = cmop(rn, p);

	rn = cmop(rn, buildtree(INCR, ccopy(t101), bcon(1))); /* bit counter plus 1 */

	rn = cmop(rn, lblnod(l16)); /* label */

	rn = cmop(rn, buildtree(RSEQ, t102, bcon(1))); /* right-shift input data 1 position */

	p = buildtree(CBRANCH, buildtree(EQ, ccopy(t102), bcon(0)), bcon(l17)); /* if all bits unset, goto l17 */
	rn = cmop(rn, p);

	rn = cmop(rn, block(GOTO, bcon(l15), NULL, INT, 0, 0)); /* go to the beginning of loop */

	rn = cmop(rn, lblnod(l17)); /* label, loop exit here */
	return cmop(rn, t101);
}

static P1ND *
builtin_popcount(const struct bitable *bt, P1ND *a)
{
	return builtin_popcnt(a, UNSIGNED);
}

static P1ND *
builtin_popcountl(const struct bitable *bt, P1ND *a)
{
	return builtin_popcnt(a, ULONG);
}

static P1ND *
builtin_popcountll(const struct bitable *bt, P1ND *a)
{
	return builtin_popcnt(a, ULONGLONG);
}

/*
 * Get size of object, if possible.
 * 0 = whole object, 1 == closest object.  Return -1 if not available.
 * 2 == max of rem object, 3 == min of rem obj.  Return 0 if not available.
 */
static P1ND *
builtin_object_size(const struct bitable *bt, P1ND *a)
{
	CONSZ v = icons(a->n_right);
	int r;

	if (v < 0 || v > 3)
		uerror("arg2 must be between 0 and 3");
	r = v < 2 ? -1 : 0;

	a = p1nfree(a);
#ifdef notyet
	if (ISPTR(a->n_type)) {
		a = buildtree(UMUL, a, 0);
		a = optloop(a);
		a = doszof(a);
	}
#else
	p1walkf(a, putjops, 0); /* if ?: exists */
	p1tfree(a);
#endif
	return xbcon(r, NULL, bt->rt);
}

#ifndef TARGET_STDARGS
static P1ND *
builtin_stdarg_start(const struct bitable *bt, P1ND *a)
{
	P1ND *p, *q;
	int sz;

	/* must first deal with argument size; use int size */
	p = a->n_right;
	if (p->n_type < INT) {
		sz = (int)(SZINT/tsize(p->n_type, p->n_df, p->pss));
	} else
		sz = 1;

	/* do the real job */
	p = buildtree(ADDROF, p, NULL); /* address of last arg */
#ifdef STACK_DOWN
	p = optim(buildtree(PLUS, p, bcon(sz))); /* add one to it (next arg) */
#else
	p = optim(buildtree(MINUS, p, bcon(sz))); /* add one to it (next arg) */
#endif
	q = block(NAME, NULL, NULL, PTR+VOID, 0, 0); /* create cast node */
	q = buildtree(CAST, q, p); /* cast to void * (for assignment) */
	p = q->n_right;
	p1nfree(q->n_left);
	p1nfree(q);
	p = buildtree(ASSIGN, a->n_left, p); /* assign to ap */
	p1nfree(a);
	return p;
}

static P1ND *
builtin_va_arg(const struct bitable *bt, P1ND *a)
{
	P1ND *p, *q, *r, *rv;
	int sz, nodnum;

	/* create a copy to a temp node of current ap */
	p = ccopy(a->n_left);
	q = tempnode(0, p->n_type, p->n_df, p->pss);
	nodnum = regno(q);
	rv = buildtree(ASSIGN, q, p);

	r = a->n_right;
	sz = (int)tsize(r->n_type, r->n_df, r->pss);
#ifdef MYVAARGSZ
	SETOFF(sz, MYVAARGSZ);
#endif
	sz /= SZCHAR;
	/* add one to ap */
#ifdef STACK_DOWN
	rv = buildtree(COMOP, rv , buildtree(PLUSEQ, a->n_left, bcon(sz)));
#else
	ecomp(buildtree(MINUSEQ, a->n_left, bcon(sz)));
#endif

	p1nfree(a->n_right);
	p1nfree(a);
	r = tempnode(nodnum, INCREF(r->n_type), r->n_df, r->pss);
	return buildtree(COMOP, rv, buildtree(UMUL, r, NULL));

}

static P1ND *
builtin_va_end(const struct bitable *bt, P1ND *a)
{
	return a; /* may have side effects */
}

static P1ND *
builtin_va_copy(const struct bitable *bt, P1ND *a)
{
	P1ND *f;

	f = buildtree(ASSIGN, a->n_left, a->n_right);
	p1nfree(a);
	return f;
}
#endif /* TARGET_STDARGS */

/*
 * For unimplemented "builtin" functions, try to invoke the
 * non-builtin name
 */
static P1ND *
binhelp(P1ND *a, TWORD rt, char *n)
{
	P1ND *f = block(NAME, NULL, NULL, INT, 0, 0);
	int oblvl = blevel;

	blevel = 0;
	f->n_sp = lookup(addname(n), SNORMAL);
	blevel = oblvl;
	if (f->n_sp->sclass == SNULL) {
		f->n_sp->sclass = EXTERN;
		f->n_sp->stype = INCREF(rt)+(FTN-PTR);
		f->n_sp->sdf = permalloc(sizeof(union dimfun));
		dimfuncnt++;
		f->n_sp->sdf->dlst = 0;
	}
	f->n_type = f->n_sp->stype;
	f = clocal(f);
	return buildtree(CALL, f, a);
}

static P1ND *
builtin_unimp(const struct bitable *bt, P1ND *a)
{
	return binhelp(a, bt->rt, &bt->name[10]);
}

#if 0
static P1ND *
builtin_unimp_f(P1ND *f, P1ND *a, TWORD rt)
{
	return binhelp(f, a, rt, f->n_sp->sname);
}
#endif

static P1ND *
builtin_isinf_sign(const struct bitable *bt, P1ND *a)
{
	if (a->n_type < FLOAT || a->n_type > DOUBLE) {
		uerror("non-floating point argument");
		return bcon(0);
	}
	if (a->n_type != DOUBLE)
		a = cast(a, DOUBLE, 0);
	return binhelp(a, INT, "__isinf_sign");
}

#ifndef TARGET_PREFETCH
static P1ND *
builtin_prefetch(const struct bitable *bt, P1ND *a)
{
	p1tfree(a);
	return bcon(0);
}
#endif

/*
 * check if compatible types.
 * XXX - all enum are considered equal types
 */
static P1ND *
builtin_tc(const struct bitable *bt, P1ND *a)
{
	P1ND *p;

	if (a == NULL || a->n_op != CM ||
	    a->n_left->n_op != TYPE || a->n_right->n_op != TYPE)
		uerror("bad %s arg", bt->name);
	
	p = bcon(a->n_left->n_type == a->n_right->n_type);
	p1nfree(a->n_left);
	p1nfree(a->n_right);
	p1nfree(a);
	return p;
}

static void
putinlbl(P1ND *p, void *arg)
{
	if (p->n_op == COMOP && p->n_left->n_op == GOTO) {
		int v = (int)glval(p->n_left->n_left);
		send_passt(IP_DEFLAB, v+1);
	}
}

/*
 * Similar to ?:
 */
static P1ND *
builtin_ce(const struct bitable *bt, P1ND *a)
{
	P1ND *p;

	if (a == NULL || a->n_op != CM ||
	    a->n_left->n_op != CM || a->n_left->n_left->n_op == CM)
		uerror("bad %s arg", bt->name);
	if (nncon(a->n_left->n_left) == 0)
		uerror("arg not constant");
	if (glval(a->n_left->n_left)) {
		p = a->n_left->n_right;
		a->n_left->n_op = UMUL; /* for p1tfree() */
		p1walkf(a->n_right, putinlbl, 0);
	} else {
		p = a->n_right;
		a->n_op = UMUL; /* for p1tfree() */
		p1walkf(a->n_left->n_right, putinlbl, 0);
	}
	p1tfree(a);
	return p;
	
}

static P1ND *
builtin_classify_type(const struct bitable *bt, P1ND *a)
{
	TWORD t = a->n_type;
	int rv;

	if (t == BOOL)
		rv = 4;
	else if (t == CHAR || t == UCHAR)
		rv = 2;
	else if (t <= ULONGLONG)
		rv = 1;
	else if (t == STRTY)
		rv = 12;
	else if (t == UNIONTY)
		rv = 13;
	else if (ISPTR(t))
		rv = 5;
	else if (ISFTY(t))
		rv = 8;
	else if (ISFTN(t))
		rv = 10;
	else if (ISCTY(t))
		rv = 9;
	else
		rv = -1;

	p1tfree(a);
	return bcon(rv);
}


#ifndef TARGET_ISMATH
/*
 * Handle the builtin macros for the math functions is*
 * To get something that is be somewhat generic assume that 
 * isnan() is a real function and that cast of a NaN type 
 * to double will still be a NaN.
 */
static P1ND *
mtisnan(P1ND *p)
{

	return binhelp(cast(ccopy(p), DOUBLE, 0), INT, "isnan");
}

static TWORD
mtcheck(P1ND *p)
{
	TWORD t1 = p->n_left->n_type, t2 = p->n_right->n_type;

	if ((t1 >= FLOAT && t1 <= LDOUBLE) ||
	    (t2 >= FLOAT && t2 <= LDOUBLE))
		return MAX(t1, t2);
	return 0;
}

static P1ND *
builtin_isunordered(const struct bitable *bt, P1ND *a)
{
	P1ND *p;

	if (mtcheck(a) == 0)
		return bcon(0);

	p = buildtree(OROR, mtisnan(a->n_left), mtisnan(a->n_right));
	p1tfree(a);
	return p;
}
static P1ND *
builtin_isany(P1ND *a, TWORD rt, int cmpt)
{
	P1ND *p, *q;
	TWORD t;

	if ((t = mtcheck(a)) == 0)
		return bcon(0);
	p = buildtree(OROR, mtisnan(a->n_left), mtisnan(a->n_right));
	p = buildtree(NOT, p, NULL);
	q = buildtree(cmpt, cast(ccopy(a->n_left), t, 0),
	    cast(ccopy(a->n_right), t, 0));
	p = buildtree(ANDAND, p, q);
	p1tfree(a);
	return p;
}
static P1ND *
builtin_isgreater(const struct bitable *bt, P1ND *a)
{
	return builtin_isany(a, bt->rt, GT);
}
static P1ND *
builtin_isgreaterequal(const struct bitable *bt, P1ND *a)
{
	return builtin_isany(a, bt->rt, GE);
}
static P1ND *
builtin_isless(const struct bitable *bt, P1ND *a)
{
	return builtin_isany(a, bt->rt, LT);
}
static P1ND *
builtin_islessequal(const struct bitable *bt, P1ND *a)
{
	return builtin_isany(a, bt->rt, LE);
}
static P1ND *
builtin_islessgreater(const struct bitable *bt, P1ND *a)
{
	P1ND *p, *q, *r;
	TWORD t;

	if ((t = mtcheck(a)) == 0)
		return bcon(0);
	p = buildtree(OROR, mtisnan(a->n_left), mtisnan(a->n_right));
	p = buildtree(NOT, p, NULL);
	q = buildtree(GT, cast(ccopy(a->n_left), t, 0),
	    cast(ccopy(a->n_right), t, 0));
	r = buildtree(LT, cast(ccopy(a->n_left), t, 0),
	    cast(ccopy(a->n_right), t, 0));
	q = buildtree(OROR, q, r);
	p = buildtree(ANDAND, p, q);
	p1tfree(a);
	return p;
}
#endif

#ifndef TARGET_SIGNBIT
static P1ND *
builtin_signbit(const struct bitable *bt, P1ND *a)
{
	return binhelp(a, INT, "__signbitd");
}
static P1ND *
builtin_signbitf(const struct bitable *bt, P1ND *a)
{
	return binhelp(a, INT, "__signbitf");
}
static P1ND *
builtin_signbitl(const struct bitable *bt, P1ND *a)
{
	return binhelp(a, INT, "__signbitl");
}
#endif

static P1ND *
builtin_huge_val(const struct bitable *bt, P1ND *a)
{
	P1ND *f = block(FCON, NULL, NULL, bt->rt, NULL, 0);

	f->n_scon = sfallo();
	soft_huge_val(f->n_scon);
	return f;
}

/*
 * Return NANs, if reasonable.
 */
static P1ND *
builtin_nanx(const struct bitable *bt, P1ND *a)
{

	if (a == NULL || a->n_op == CM) {
		uerror("%s bad argument", bt->name);
		a = bcon(0);
	} else if (a->n_op == STRING && *a->n_name == '\0') {
		p1nfree(a);
		a = block(FCON, NULL, NULL, bt->rt, NULL, 0);
		a->n_scon = sfallo();
		soft_nan(a->n_scon, NULL);
	} else
		a = binhelp(eve(a), bt->rt, &bt->name[10]);
	return a;
}

#ifndef NO_COMPLEX
static P1ND *
builtin_cir(const struct bitable *bt, P1ND *a)
{
	char *n;

	if (a == NULL || a->n_op == CM) {
		uerror("wrong argument count to %s", bt->name);
		return bcon(0);
	}

	n = addname(bt->name[1] == 'r' ? "__real" : "__imag");
	return cast(structref(a, DOT, n), bt->rt, 0);
}

#endif

/*
 * Target defines, to implement target versions of the generic builtins
 */
#ifndef TARGET_MEMCMP
#define	builtin_memcmp builtin_unimp
#endif
#ifndef TARGET_MEMCPY
#define	builtin_memcpy builtin_unimp
#endif
#ifndef TARGET_MEMPCPY
#define	builtin_mempcpy builtin_unimp
#endif
#ifndef TARGET_MEMSET
#define	builtin_memset builtin_unimp
#endif

/* Reasonable type of size_t */
#ifndef SIZET
#if SZINT == SZSHORT
#define	SIZET UNSIGNED
#elif SZLONG > SZINT
#define SIZET ULONG
#else
#define SIZET UNSIGNED
#endif
#endif

static TWORD memcpyt[] = { VOID|PTR, VOID|PTR, SIZET, INT };
static TWORD memsett[] = { VOID|PTR, INT, SIZET, INT };
static TWORD allocat[] = { SIZET };
static TWORD expectt[] = { LONG, LONG };
static TWORD chkt[] = { LONG, ULONG };
static TWORD chkzt[] = { VOID|PTR };
static TWORD chk2t[] = { LONG, LONG, LONG };
static TWORD strcmpt[] = { CHAR|PTR, CHAR|PTR };
static TWORD strcpyt[] = { CHAR|PTR, CHAR|PTR, INT };
static TWORD strncpyt[] = { CHAR|PTR, CHAR|PTR, SIZET, INT };
static TWORD strchrt[] = { CHAR|PTR, INT };
static TWORD strcspnt[] = { CHAR|PTR, CHAR|PTR };
static TWORD strspnt[] = { CHAR|PTR, CHAR|PTR };
static TWORD strpbrkt[] = { CHAR|PTR, CHAR|PTR };
static TWORD nant[] = { CHAR|PTR };
static TWORD bitt[] = { UNSIGNED };
static TWORD bsw16t[] = { USHORT };
static TWORD bitlt[] = { ULONG };
static TWORD bitllt[] = { ULONGLONG };
static TWORD abst[] = { INT };
static TWORD fmaxft[] = { FLOAT, FLOAT };
static TWORD fmaxt[] = { DOUBLE, DOUBLE };
static TWORD fmaxlt[] = { LDOUBLE, LDOUBLE };
static TWORD scalbnft[] = { FLOAT, INT };
static TWORD scalbnt[] = { DOUBLE, INT };
static TWORD scalbnlt[] = { LDOUBLE, INT };

static const struct bitable bitable[] = {
	/* gnu universe only */
	{ "alloca", builtin_alloca, BTGNUONLY, 1, allocat, VOID|PTR },

#ifndef NO_COMPLEX
	/* builtins for complex operations */
	{ "crealf", builtin_cir, BTNOPROTO, 1, 0, FLOAT },
	{ "creal", builtin_cir, BTNOPROTO, 1, 0, DOUBLE },
	{ "creall", builtin_cir, BTNOPROTO, 1, 0, LDOUBLE },
	{ "cimagf", builtin_cir, BTNOPROTO, 1, 0, FLOAT },
	{ "cimag", builtin_cir, BTNOPROTO, 1, 0, DOUBLE },
	{ "cimagl", builtin_cir, BTNOPROTO, 1, 0, LDOUBLE },
#endif
	/* always existing builtins */
	{ "__builtin___memcpy_chk", builtin_unimp, 0, 4, memcpyt, VOID|PTR },
	{ "__builtin___mempcpy_chk", builtin_unimp, 0, 4, memcpyt, VOID|PTR },
	{ "__builtin___memmove_chk", builtin_unimp, 0, 4, memcpyt, VOID|PTR },
	{ "__builtin___memset_chk", builtin_unimp, 0, 4, memsett, VOID|PTR },

	{ "__builtin___strcat_chk", builtin_unimp, 0, 3, strcpyt, CHAR|PTR },
	{ "__builtin___strcpy_chk", builtin_unimp, 0, 3, strcpyt, CHAR|PTR },
	{ "__builtin___stpcpy_chk", builtin_unimp, 0, 3, strcpyt, CHAR|PTR },
	{ "__builtin___strncat_chk", builtin_unimp, 0, 4, strncpyt,CHAR|PTR },
	{ "__builtin___strncpy_chk", builtin_unimp, 0, 4, strncpyt,CHAR|PTR },
	{ "__builtin___stpncpy_chk", builtin_unimp, 0, 4, strncpyt,CHAR|PTR },

	{ "__builtin___printf_chk", builtin_unimp, BTNOPROTO, -1, 0, INT },
	{ "__builtin___fprintf_chk", builtin_unimp, BTNOPROTO, -1, 0, INT },
	{ "__builtin___sprintf_chk", builtin_unimp, BTNOPROTO, -1, 0, INT },
	{ "__builtin___snprintf_chk", builtin_unimp, BTNOPROTO, -1, 0, INT },
	{ "__builtin___vprintf_chk", builtin_unimp, BTNOPROTO, -1, 0, INT },
	{ "__builtin___vfprintf_chk", builtin_unimp, BTNOPROTO, -1, 0, INT },
	{ "__builtin___vsprintf_chk", builtin_unimp, BTNOPROTO, -1, 0, INT },
	{ "__builtin___vsnprintf_chk", builtin_unimp, BTNOPROTO, -1, 0, INT },

	{ "__builtin_alloca", builtin_alloca, 0, 1, allocat, VOID|PTR },
	{ "__builtin_abs", builtin_abs, 0, 1, abst, INT },
	{ "__builtin_bswap16", builtin_bswap16, 0, 1, bsw16t, USHORT },
	{ "__builtin_bswap32", builtin_bswap32, 0, 1, bitt, UNSIGNED },
	{ "__builtin_bswap64", builtin_bswap64, 0, 1, bitllt, ULONGLONG },
	{ "__builtin_choose_expr", builtin_ce, BTNOPROTO|BTNORVAL, 0, 0, 0 },
	{ "__builtin_clz", builtin_clz, 0, 1, bitt, INT },
	{ "__builtin_clzl", builtin_clzl, 0, 1, bitlt, INT },
	{ "__builtin_clzll", builtin_clzll, 0, 1, bitllt, INT },
	{ "__builtin_ctz", builtin_ctz, 0, 1, bitt, INT },
	{ "__builtin_ctzl", builtin_ctzl, 0, 1, bitlt, INT },
	{ "__builtin_ctzll", builtin_ctzll, 0, 1, bitllt, INT },
	{ "__builtin_extract_return_addr", builtin_era, 0, 1, memcpyt, VOID|PTR },
	{ "__builtin_ffs", builtin_ffs, 0, 1, bitt, INT },
	{ "__builtin_ffsl", builtin_ffsl, 0, 1, bitlt, INT },
	{ "__builtin_ffsll", builtin_ffsll, 0, 1, bitllt, INT },
	{ "__builtin_popcount", builtin_popcount, 0, 1, bitt, UNSIGNED },
	{ "__builtin_popcountl", builtin_popcountl, 0, 1, bitlt, ULONG },
	{ "__builtin_popcountll", builtin_popcountll, 0, 1, bitllt, ULONGLONG },

	{ "__builtin_classify_type", builtin_classify_type, 0, 1, 0, INT },
	{ "__builtin_constant_p", builtin_constant_p, 0, 1, 0, INT },
	{ "__builtin_copysignf", builtin_unimp, 0, 2, fmaxft, FLOAT },
	{ "__builtin_copysign", builtin_unimp, 0, 2, fmaxt, DOUBLE },
	{ "__builtin_copysignl", builtin_unimp, 0, 2, fmaxlt, LDOUBLE },
	{ "__builtin_expect", builtin_expect, 0, 2, expectt, LONG },
	{ "__builtin_chk", builtin_chk, 0, 2, chkt, LONG },
	{ "__builtin_chkz", builtin_chkz, 0, 1, chkzt, VOID|PTR },
	{ "__builtin_chk2", builtin_chk2, 0, 3, chk2t, LONG },
	{ "__builtin_memcmp", builtin_memcmp, 0, 3, memcpyt, INT },
	{ "__builtin_memcpy", builtin_memcpy, 0, 3, memcpyt, VOID|PTR },
	{ "__builtin_mempcpy", builtin_mempcpy, 0, 3, memcpyt, VOID|PTR },
	{ "__builtin_memset", builtin_memset, 0, 3, memsett, VOID|PTR },
	{ "__builtin_fabsf", builtin_unimp, 0, 1, fmaxft, FLOAT },
	{ "__builtin_fabs", builtin_unimp, 0, 1, fmaxt, DOUBLE },
	{ "__builtin_fabsl", builtin_unimp, 0, 1, fmaxlt, LDOUBLE },
	{ "__builtin_fmaxf", builtin_unimp, 0, 2, fmaxft, FLOAT },
	{ "__builtin_fmax", builtin_unimp, 0, 2, fmaxt, DOUBLE },
	{ "__builtin_fmaxl", builtin_unimp, 0, 2, fmaxlt, LDOUBLE },
	{ "__builtin_huge_valf", builtin_huge_val, 0, 0, 0, FLOAT },
	{ "__builtin_huge_val", builtin_huge_val, 0, 0, 0, DOUBLE },
	{ "__builtin_huge_vall", builtin_huge_val, 0, 0, 0, LDOUBLE },
	{ "__builtin_inff", builtin_huge_val, 0, 0, 0, FLOAT },
	{ "__builtin_inf", builtin_huge_val, 0, 0, 0, DOUBLE },
	{ "__builtin_infl", builtin_huge_val, 0, 0, 0, LDOUBLE },
	{ "__builtin_isgreater", builtin_isgreater, 0, 2, NULL, INT },
	{ "__builtin_isgreaterequal", builtin_isgreaterequal, 0, 2, NULL, INT },
	{ "__builtin_isinf_sign", builtin_isinf_sign, BTNOPROTO, 1, NULL, INT },
	{ "__builtin_isinff", builtin_unimp, 0, 1, fmaxft, INT },
	{ "__builtin_isinf", builtin_unimp, 0, 1, fmaxt, INT },
	{ "__builtin_isinfl", builtin_unimp, 0, 1, fmaxlt, INT },
	{ "__builtin_isless", builtin_isless, 0, 2, NULL, INT },
	{ "__builtin_islessequal", builtin_islessequal, 0, 2, NULL, INT },
	{ "__builtin_islessgreater", builtin_islessgreater, 0, 2, NULL, INT },
	{ "__builtin_isnanf", builtin_unimp, 0, 1, fmaxft, INT },
	{ "__builtin_isnan", builtin_unimp, 0, 1, fmaxt, INT },
	{ "__builtin_isnanl", builtin_unimp, 0, 1, fmaxlt, INT },
	{ "__builtin_isunordered", builtin_isunordered, 0, 2, NULL, INT },
	{ "__builtin_logbf", builtin_unimp, 0, 1, fmaxft, FLOAT },
	{ "__builtin_logb", builtin_unimp, 0, 1, fmaxt, DOUBLE },
	{ "__builtin_logbl", builtin_unimp, 0, 1, fmaxlt, LDOUBLE },
	{ "__builtin_nanf", builtin_nanx, BTNOEVE, 1, nant, FLOAT },
	{ "__builtin_nan", builtin_nanx, BTNOEVE, 1, nant, DOUBLE },
	{ "__builtin_nanl", builtin_nanx, BTNOEVE, 1, nant, LDOUBLE },
	{ "__builtin_object_size", builtin_object_size, BTNOPROTO, 2, memsett, SIZET },
	{ "__builtin_prefetch", builtin_prefetch, 0, 1, memsett, VOID },
	{ "__builtin_scalbnf", builtin_unimp, 0, 2, scalbnft, FLOAT },
	{ "__builtin_scalbn", builtin_unimp, 0, 2, scalbnt, DOUBLE },
	{ "__builtin_scalbnl", builtin_unimp, 0, 2, scalbnlt, LDOUBLE },

	{ "__builtin_signbitf", builtin_signbitf, 0, 1, scalbnft, INT },
	{ "__builtin_signbit", builtin_signbit, 0, 1, scalbnt, INT },
	{ "__builtin_signbitl", builtin_signbitl, 0, 1, scalbnlt, INT },

	{ "__builtin_strcmp", builtin_unimp, 0, 2, strcmpt, INT },
	{ "__builtin_strcpy", builtin_unimp, 0, 2, strcpyt, CHAR|PTR },
	{ "__builtin_stpcpy", builtin_unimp, 0, 2, strcpyt, CHAR|PTR },
	{ "__builtin_strchr", builtin_unimp, 0, 2, strchrt, CHAR|PTR },
	{ "__builtin_strlen", builtin_unimp, 0, 1, strcmpt, SIZET },
	{ "__builtin_strrchr", builtin_unimp, 0, 2, strchrt, CHAR|PTR },
	{ "__builtin_strncpy", builtin_unimp, 0, 3, strncpyt, CHAR|PTR },
	{ "__builtin_strncat", builtin_unimp, 0, 3, strncpyt, CHAR|PTR },
	{ "__builtin_strcspn", builtin_unimp, 0, 2, strcspnt, SIZET },
	{ "__builtin_strspn", builtin_unimp, 0, 2, strspnt, SIZET },
	{ "__builtin_strstr", builtin_unimp, 0, 2, strcmpt, CHAR|PTR },
	{ "__builtin_strpbrk", builtin_unimp, 0, 2, strpbrkt, CHAR|PTR },
	{ "__builtin_types_compatible_p", builtin_tc, BTNOPROTO|BTNOEVE, 2, 0, INT },
#ifndef TARGET_STDARGS
	{ "__builtin_stdarg_start", builtin_stdarg_start, 0, 2, 0, VOID },
	{ "__builtin_va_start", builtin_stdarg_start, 0, 2, 0, VOID },
	{ "__builtin_va_arg", builtin_va_arg, BTNORVAL|BTNOPROTO, 2, 0, 0 },
	{ "__builtin_va_end", builtin_va_end, 0, 1, 0, VOID },
	{ "__builtin_va_copy", builtin_va_copy, 0, 2, 0, VOID },
#endif
	{ "__builtin_dwarf_cfa", builtin_cfa, 0, 0, 0, VOID|PTR },
	{ "__builtin_frame_address",
	    builtin_frame_address, 0, 1, bitt, VOID|PTR },
	{ "__builtin_return_address",
	    builtin_return_address, 0, 1, bitt, VOID|PTR },
#ifdef TARGET_BUILTINS
	TARGET_BUILTINS
#endif
};

/*
 * Check and cast arguments for builtins.
 */
static int
acnt(P1ND *a, int narg, TWORD *tp)
{
	P1ND *q;
	TWORD t;

	if (a == NULL)
		return narg;
	for (; a->n_op == CM; a = a->n_left, narg--) {
		if (tp == NULL)
			continue;
		q = a->n_right;
		t = ctype(tp[narg-1]);
		if (q->n_type == t)
			continue;
		a->n_right = ccast(q, t, 0, NULL, 0);
	}

	/* Last arg is ugly to deal with */
	if (narg == 1 && tp != NULL && a->n_type != tp[0]) {
		q = p1alloc();
		*q = *a;
		q = ccast(q, ctype(tp[0]), 0, NULL, 0);
		*a = *q;
		p1nfree(q);
	}
	return narg != 1;
}

P1ND *
builtin_check(struct symtab *sp, P1ND *a)
{
	const struct bitable *bt;

	if (sp->soffset < 0 ||
	    sp->soffset >= (int)(sizeof(bitable)/sizeof(bitable[0])))
		cerror("builtin_check");

	bt = &bitable[sp->soffset];
	if ((bt->flags & BTNOEVE) == 0 && a != NULL)
		a = eve(a);
	if (((bt->flags & BTNOPROTO) == 0) && acnt(a, bt->narg, bt->tp)) {
		uerror("wrong argument count to %s", bt->name);
		return bcon(0);
	}
	return (*bt->fun)(bt, a);
}

/*
 * Put all builtin functions into the global symbol table.
 */
void
builtin_init(void)
{
	const struct bitable *bt;
	P1ND *p = block(TYPE, 0, 0, 0, 0, 0);
	struct symtab *sp;
	int i, d_debug;

	d_debug = ddebug;
	ddebug = 0;
	for (i = 0; i < (int)(sizeof(bitable)/sizeof(bitable[0])); i++) {
		bt = &bitable[i];
		if ((bt->flags & BTGNUONLY) && xgnu99 == 0 && xgnu89 == 0)
			continue; /* not in c99 universe, at least for now */
		sp = lookup(addname(bt->name), 0);
		if (bt->rt == 0 && (bt->flags & BTNORVAL) == 0)
			cerror("function '%s' has no return type", bt->name);
		p->n_type = INCREF(bt->rt) + (FTN-PTR);
		p->n_df = memset(permalloc(sizeof(union dimfun)), 0,
		    sizeof(union dimfun));
		dimfuncnt++;
		p->n_sp = sp;
		defid(p, EXTERN);
		sp->soffset = i;
		sp->sflags |= SBUILTIN;
	}
	chk_elided_hook = chk_elided;
	chk_reset_hook = chk_reset;
	/* the failure routine of the run-time checks: void __chk_fail(int, const char *, int, long, unsigned long), declared without a prototype */
	chkfailsp = lookup(addname("__chk_fail"), 0);
	p->n_type = INCREF(VOID) + (FTN-PTR);
	p->n_df = memset(permalloc(sizeof(union dimfun)), 0, sizeof(union dimfun));
	dimfuncnt++;
	p->n_sp = chkfailsp;
	defid(p, EXTERN);
	p1nfree(p);
	ddebug = d_debug;
}
#endif
