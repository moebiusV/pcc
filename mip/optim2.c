/*	$Id$	*/
/*
 * Copyright (c) 2004 Anders Magnusson (ragge@ludd.luth.se).
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include "pass2.h"

#include <string.h>
#include <stdlib.h>

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#define	BDEBUG(x)	if (b2debug) printf x

#define	mktemp(n, t)	mklnode(TEMP, 0, n, t)

#define	CHADD(bb,c)	{ if (bb->ch[0] == 0) bb->ch[0] = c; \
			  else if (bb->ch[1] == 0) bb->ch[1] = c; \
			  else comperr("triple cfnodes"); }
#define	FORCH(cn, chp)	\
	for (cn = &chp[0]; cn < &chp[2] && cn[0]; cn++)

/* main switch for new things not yet ready for all-day use */
/* #define ENABLE_NEW */


static int dfsnum;

static void postincfold(struct p2env *);
static void retfold(struct p2env *);
static void fwdtemps(struct p2env *);
static void cseblock(struct p2env *);
#define	ISFTY(x)	((x) >= FLOAT && (x) <= LDOUBLE)
/*
 * Enables tail merging (comjump/xjump) inside deljumps.  Only set for
 * the deljumps runs that happen before register allocation: after
 * ngenregs the trees carry register assignments (n_reg/n_su), and two
 * structurally equal trees may still use different scratch registers,
 * so merging them would be wrong there.
 */
static int tailmerge;

void saveip(struct interpass *ip);
void deljumps(struct p2env *);
void optdump(struct interpass *ip);
void printip(struct interpass *pole);

static struct varinfo defsites;

void bblocks_build(struct p2env *);
void cfg_build(struct p2env *);
void cfg_dfs(struct basicblock *bb, unsigned int parent, 
	     struct bblockinfo *bbinfo);
void dominators(struct p2env *);
struct basicblock *
ancestorwithlowestsemi(struct basicblock *bblock, struct bblockinfo *bbinfo);
void computeDF(struct p2env *, struct basicblock *bblock);
void printDF(struct p2env *p2e);
void findTemps(struct interpass *ip);
void placePhiFunctions(struct p2env *);
void renamevar(struct p2env *p2e,struct basicblock *bblock);
void removephi(struct p2env *p2e);
void simple_cp(struct p2env *p2e);
void remunreach(struct p2env *);
static void liveanal(struct p2env *p2e);
static void printip2(struct interpass *);

/* create "proper" basic blocks, add labels where needed (so bblocks have labels) */
/* run before bb generate */
static void add_labels(struct p2env*) ;

/* Perform trace scheduling, try to get rid of gotos as much as possible */
void TraceSchedule(struct p2env*) ;

#ifdef ENABLE_NEW
static void do_cse(struct p2env* p2e) ;
#endif

/* Walk the complete set, performing a function on each node. 
 * if type is given, apply function on only that type */
void WalkAll(struct p2env* p2e, void (*f) (NODE*, void*), void* arg, int type) ;

void BBLiveDead(struct basicblock* bblock, int what, unsigned int variable) ;

/* Fill the live/dead code */
void LiveDead(struct p2env* p2e, int what, unsigned int variable) ;

#ifdef PCC_DEBUG
void printflowdiagram(struct p2env *, char *);
#endif

void
optimize(struct p2env *p2e)
{
	struct interpass *ipole = &p2e->ipole;

	if (b2debug) {
		printf("initial links\n");
		printip(ipole);
	}

	postincfold(p2e);
	if (xdeljumps) {
		tailmerge = 1;
		deljumps(p2e); /* Delete redundant jumps and dead code */
		tailmerge = 0;
	}
	retfold(p2e);
	cseblock(p2e);
	fwdtemps(p2e);

	if (xssa)
		add_labels(p2e) ;
#ifdef ENABLE_NEW
	do_cse(p2e);
#endif

#ifdef PCC_DEBUG
	if (b2debug) {
		printf("links after deljumps\n");
		printip(ipole);
	}
#endif
	if (xssa || xtemps) {
		bblocks_build(p2e);
		BDEBUG(("Calling cfg_build\n"));
		cfg_build(p2e);
	
#ifdef PCC_DEBUG
		printflowdiagram(p2e, "first");
#endif
	}
	if (xssa) {
		BDEBUG(("Calling liveanal\n"));
		liveanal(p2e);
		BDEBUG(("Calling dominators\n"));
		dominators(p2e);
		BDEBUG(("Calling computeDF\n"));
		computeDF(p2e, DLIST_NEXT(&p2e->bblocks, bbelem));

		if (b2debug) {
			printDF(p2e);
		}

		BDEBUG(("Calling placePhiFunctions\n"));

		placePhiFunctions(p2e);

		BDEBUG(("Calling renamevar\n"));

		renamevar(p2e,DLIST_NEXT(&p2e->bblocks, bbelem));

		BDEBUG(("Calling removephi\n"));

#ifdef PCC_DEBUG
		printflowdiagram(p2e, "ssa");
#endif

		removephi(p2e);

		/* Simple constant propagation */
		if (xscp)
			simple_cp(p2e);

		BDEBUG(("Calling remunreach\n"));
/*		remunreach(p2e); */
		
		/*
		 * Recalculate basic blocks and cfg that was destroyed
		 * by removephi
		 */
		/* first, clean up all what deljumps should have done, and more */

		/* TODO: add the basic blocks done by the ssa code by hand. 
		 * The trace scheduler should not change the order in
		 * which blocks are executed or what data is calculated.
		 * Thus, the BBlock order should remain correct.
		 */

#ifdef ENABLE_NEW
		bblocks_build(p2e);
		BDEBUG(("Calling cfg_build\n"));
		cfg_build(p2e);

		TraceSchedule(p2e);
#ifdef PCC_DEBUG
		printflowdiagram(p2e, "sched_trace");

		if (b2debug) {
			printf("after tracesched\n");
			printip(ipole);
			fflush(stdout) ;
		}
#endif
#endif

		/* Now, clean up the gotos we do not need any longer */
		if (xdeljumps) {
			tailmerge = 1;
			deljumps(p2e); /* Delete jumps and dead code */
			tailmerge = 0;
		}

		bblocks_build(p2e);
		BDEBUG(("Calling cfg_build\n"));
		cfg_build(p2e);

#ifdef PCC_DEBUG
		printflowdiagram(p2e, "no_phi");

		if (b2debug) {
			printf("new tree\n");
			printip(ipole);
		}
#endif
	}
	myoptim(ipole);
}

/*
 * Delete unused labels, excess of labels, gotos to gotos.
 * This routine can be made much more efficient.
 *
 * Layout of the statement list here (_must_ look this way!):
 *	PROLOG
 *	LABEL	- states beginning of function argument moves
 *	...code to save/move arguments
 *	LABEL	- states beginning of execution code
 *	...code + labels in function in function
 *	EPILOG
 *
 * This version of deljumps is based on the c2 implementation
 * that were included in 2BSD.
 */
#define	LABEL 1
#define	JBR	2
#define	CBR	3
#define	STMT	4
#define	EROU	5
struct dlnod {
	int op;
	struct interpass *dlip;
	struct dlnod *forw;
	struct dlnod *back;
	struct dlnod *ref;
	int labno;
	int refc;
};

#ifdef DLJDEBUG
static void
dumplink(struct dlnod *dl)
{
	printf("dumplink %p\n", dl);
	for (; dl; dl = dl->forw) {
		if (dl->op == STMT) {
			printf("STMT(%p)\n", dl);
			fwalk(dl->dlip->ip_node, e2print, 0);
		} else if (dl->op == EROU) {
			printf("EROU(%p)\n", dl);
		} else {
			static char *str[] = { 0, "LABEL", "JBR", "CBR" };
			printf("%s(%p) %d refc %d ref %p\n", str[dl->op], 
			    dl, dl->labno, dl->refc, dl->ref);
		}
	}
	printf("end dumplink\n");
}
#endif

/*
 * Create the linked list that we can work on.
 */
static void
listsetup(struct interpass *ipole, struct dlnod *dl)
{
	struct interpass *ip = DLIST_NEXT(ipole, qelem);
	struct interpass *nip;
	struct dlnod *p, *lastp;
	NODE *q;

	lastp = dl;
	while (ip->type != IP_DEFLAB)
		ip = DLIST_NEXT(ip,qelem);
	ip = DLIST_NEXT(ip,qelem);
	while (ip->type != IP_DEFLAB)
		ip = DLIST_NEXT(ip,qelem);
	/* Now ip is at the beginning */
	for (;;) {
		ip = DLIST_NEXT(ip,qelem);
		if (ip == ipole)
			break;
		p = tmpalloc(sizeof(struct dlnod));
		p->labno = 0;
		p->dlip = ip;
		switch (ip->type) {
		case IP_DEFLAB:
			p->op = LABEL;
			p->labno = ip->ip_lbl;
			break;

		case IP_NODE:
			q = ip->ip_node;
			switch (q->n_op) {
			case GOTO:
				if (q->n_left->n_op == ICON) {
					p->op = JBR;
					p->labno = (int)getlval(q->n_left);
				} else 
					p->op = STMT;
				break;
			case CBRANCH:
				p->op = CBR;
				p->labno = (int)getlval(q->n_right);
				break;
			case ASSIGN:
				/* remove ASSIGN to self for regs */
				if (q->n_left->n_op == REG && 
				    q->n_right->n_op == REG &&
				    regno(q->n_left) == regno(q->n_right)) {
					nip = DLIST_PREV(ip, qelem);
					tfree(q);
					DLIST_REMOVE(ip, qelem);
					ip = nip;
					continue;
				}
				/* FALLTHROUGH */
			default:
				p->op = STMT;
				break;
			}
			break;

		case IP_ASM:
			p->op = STMT;
			break;

		case IP_EPILOG:
			p->op = EROU;
			break;

		default:
			comperr("listsetup: bad ip node %d", ip->type);
		}
		p->forw = 0;
		p->back = lastp;
		lastp->forw = p;
		lastp = p;
		p->ref = 0;
	}
}

/*
 * Traverse to the first statement behind a label.
 */
static struct dlnod *
nonlab(struct dlnod *p)
{
	while (p && p->op==LABEL)
		p = p->forw;
	return(p);
}

static void
iprem(struct dlnod *p)
{
	if (p->dlip->type == IP_NODE)
		tfree(p->dlip->ip_node);
	DLIST_REMOVE(p->dlip, qelem);
}

static void
decref(struct dlnod *p)
{
	if (--p->refc <= 0) {
		iprem(p);
		p->back->forw = p->forw;
		p->forw->back = p->back;
	}
}

/*
 * Change a label number for jump/branch instruction to labno.
 */
static void
setlab(struct dlnod *p, int labno)
{
	p->labno = labno;
	if (p->op == JBR)
		setlval(p->dlip->ip_node->n_left, labno);
	else if (p->op == CBR) {
		setlval(p->dlip->ip_node->n_right, labno);
		p->dlip->ip_node->n_left->n_label = labno;
	} else
		comperr("setlab bad op %d", p->op);
}

/*
 * See if a label is used outside of us.
 */
static int
inuse(struct p2env *p2e, int lbl)
{
	int *l;

	for (l = p2e->epp->ip_labels; *l; l++)
		if (*l == lbl)
			return 1;
	return 0;
}

/*
 * Label reference counting and removal of unused labels.
 */
#define	LABHS 127
static void
refcount(struct p2env *p2e, struct dlnod *dl)
{
	struct dlnod *p, *lp;
	struct dlnod *labhash[LABHS];
	struct dlnod **hp, *tp;

	/* Clear label hash */
	for (hp = labhash; hp < &labhash[LABHS];)
		*hp++ = 0;
	/* Enter labels into hash.  Later overwrites earlier */
	for (p = dl->forw; p!=0; p = p->forw) {
		if (p->op==LABEL) {
			labhash[p->labno % LABHS] = p;
			p->refc = 0;
			if (inuse(p2e, p->labno))
				p->refc = 1000; /* never remove */
		}
	}
	/* search for jumps to labels and fill in reference */
	for (p = dl->forw; p!=0; p = p->forw) {
		if (p->op==JBR || p->op==CBR) {
			p->ref = 0;
			lp = labhash[p->labno % LABHS];
			if (lp==0 || p->labno!=lp->labno)
			    for (lp = dl->forw; lp!=0; lp = lp->forw) {
				if (lp->op==LABEL && p->labno==lp->labno)
					break;
			    }
			if (lp) {
				tp = nonlab(lp)->back;
				if (tp!=lp) {
					setlab(p, tp->labno);
					lp = tp;
				}
				p->ref = lp;
				lp->refc++;
			}
		}
	}
	for (p = dl->forw; p!=0; p = p->forw)
		if (p->op==LABEL && p->refc==0 && (lp = nonlab(p))->op)
			decref(p);
}

static int nchange;

static struct dlnod *
codemove(struct dlnod *p)
{
	struct dlnod *p1, *p2, *p3;
#ifdef notyet
	struct dlnod *t, *tl;
	int n;
#endif

	p1 = p;
	if (p1->op!=JBR || (p2 = p1->ref)==0)
		return(p1);
	while (p2->op == LABEL)
		if ((p2 = p2->back) == 0)
			return(p1);
	if (p2->op!=JBR)
		goto ivloop;
	if (p1==p2)
		return(p1);
	p2 = p2->forw;
	p3 = p1->ref;
	while (p3) {
		if (p3->op==JBR) {
			if (p1==p3 || p1->forw==p3 || p1->back==p3)
				return(p1);
			nchange++;
			p1->back->forw = p2;
			p1->dlip->qelem.q_back->qelem.q_forw = p2->dlip;

			p1->forw->back = p3;
			p1->dlip->qelem.q_forw->qelem.q_back = p3->dlip;


			p2->back->forw = p3->forw;
			p2->dlip->qelem.q_back->qelem.q_forw = p3->forw->dlip;

			p3->forw->back = p2->back;
			p3->dlip->qelem.q_forw->qelem.q_back = p2->back->dlip;

			p2->back = p1->back;
			p2->dlip->qelem.q_back = p1->dlip->qelem.q_back;

			p3->forw = p1->forw;
			p3->dlip->qelem.q_forw = p1->forw->dlip;

			decref(p1->ref);
			if (p1->dlip->type == IP_NODE)
				tfree(p1->dlip->ip_node);

			return(p2);
		} else
			p3 = p3->forw;
	}
	return(p1);

ivloop:
	if (p1->forw->op!=LABEL)
		return(p1);
	return(p1);

#ifdef notyet
	p3 = p2 = p2->forw;
	n = 16;
	do {
		if ((p3 = p3->forw) == 0 || p3==p1 || --n==0)
			return(p1);
	} while (p3->op!=CBR || p3->labno!=p1->forw->labno);
	do 
		if ((p1 = p1->back) == 0)
			return(p);
	while (p1!=p3);
	p1 = p;
	tl = insertl(p1);
	p3->subop = revbr[p3->subop];
	decref(p3->ref);
		p2->back->forw = p1;
	p3->forw->back = p1;
	p1->back->forw = p2;
	p1->forw->back = p3;
	t = p1->back;
	p1->back = p2->back;
	p2->back = t;
	t = p1->forw;
	p1->forw = p3->forw;
	p3->forw = t;
	p2 = insertl(p1->forw);
	p3->labno = p2->labno;
	p3->ref = p2;
	decref(tl);
	if (tl->refc<=0)
		nrlab--;
	nchange++;
	return(p3);
#endif
}


/* the inverse of a relation, or -1; operands stay where they are */
static int
invrel(int op)
{
	switch (op) {
	case EQ: return NE;
	case NE: return EQ;
	case LE: return GT;
	case LT: return GE;
	case GE: return LT;
	case GT: return LE;
	case ULE: return UGT;
	case ULT: return UGE;
	case UGE: return ULT;
	case UGT: return ULE;
	}
	return -1;
}

/*
 * Loop rotation (-xdmr), as c2 of dmr-cc and of 32V does it:
 *
 *	L1:	T		(a few statements)		jbr L1
 *		cbr X					Lb:	B
 *		B				=>	L1:	T
 *		jbr L1						cbr' Lb
 *	X:						X:
 *
 * One branch per iteration instead of two, the same number of nodes.  Anything
 * that jumps to L1 (a "continue") still finds the test there.
 */
/* the interpass before the first node, for when the list head is the position */
static struct interpass *headip;

static void
dl_move(struct dlnod *n, struct dlnod *pos)	/* n goes after pos, in both lists */
{
	n->back->forw = n->forw;
	if (n->forw)
		n->forw->back = n->back;
	DLIST_REMOVE(n->dlip, qelem);
	n->forw = pos->forw;
	n->back = pos;
	if (pos->forw)
		pos->forw->back = n;
	pos->forw = n;
	DLIST_INSERT_AFTER(pos->dlip ? pos->dlip : headip, n->dlip, qelem);
}

static int
isfloatrel(NODE *c)
{
	TWORD t = BTYPE(c->n_left->n_type);

	return t == FLOAT || t == DOUBLE || t == LDOUBLE;
}

/* labels made by looprot must outlive deljumps' working storage */
static struct interpass *rotpool;
static int rotleft;

static int
looprot(struct p2env *p2e, struct dlnod *dl)
{
	struct dlnod *j, *l1, *first, *pred, *c, *t, *x, *q, *b1, *bl, *lb, *pos, *nx;
	struct interpass *lip;
	int n, found, newlab;

	for (j = dl->forw; j != 0; j = j->forw) {
		if (j->op != JBR || (l1 = j->ref) == 0 || l1->op != LABEL)
			continue;
		if ((x = j->forw) == 0 || x->op != LABEL)
			continue;
		/* the labels before the test, and what comes before them */
		for (first = l1; first->back->op == LABEL; first = first->back)
			;
		pred = first->back;
		headip = DLIST_PREV(first->dlip, qelem);
		if (pred == j || pred->op == EROU)
			continue;
		/* the test: a few statements and a conditional branch out of the loop */
		for (t = l1->forw, n = 0; t && t->op == STMT && n < 16; t = t->forw, n++)
			;
		if (t == 0 || t->op != CBR || t->ref == 0 || t == j)
			continue;
		c = t;
		found = 0;
		for (q = x; q && q->op == LABEL; q = q->forw)
			if (q->labno == c->labno)
				found = 1;
		if (!found)
			continue;
		if (isfloatrel(c->dlip->ip_node->n_left))
			continue;
		if ((b1 = c->forw) == j)
			continue;	/* an empty body */
		/* the test must come before the jump: walk down to it */
		for (q = b1, found = 0; q && q->op != EROU; q = q->forw)
			if (q == j) {
				found = 1;
				break;
			}
		if (!found)
			continue;
		if (rotleft <= 0)
			return 0;
		bl = j->back;

		/* 1. the test, with its labels, after the body */
		pos = bl;
		for (q = first;; q = nx) {
			nx = q->forw;
			dl_move(q, pos);
			pos = q;
			if (q == c)
				break;
		}
		/* 2. a new label at the head of the body, and the test branches to it */
		newlab = getlab2();
		lip = rotpool + --rotleft;
		lip->type = IP_DEFLAB;
		lip->ip_lbl = newlab;
		lb = tmpalloc(sizeof(struct dlnod));
		memset(lb, 0, sizeof(*lb));
		lb->op = LABEL;
		lb->labno = newlab;
		lb->dlip = lip;
		lb->refc = 1;
		setlab(c, newlab);
		c->ref = lb;
		c->dlip->ip_node->n_left->n_op =
		    invrel(c->dlip->ip_node->n_left->n_op);
		/* 3. the jump to the test goes in front of the body (if flow can reach it) */
		if (pred->op == JBR) {
			/* nothing falls in: the jump is not needed */
			j->back->forw = j->forw;
			j->forw->back = j->back;
			iprem(j);
			pos = pred;
		} else {
			dl_move(j, pred);
			pos = j;
		}
		/* the new label, after it */
		lb->forw = pos->forw;
		lb->back = pos;
		if (pos->forw)
			pos->forw->back = lb;
		pos->forw = lb;
		DLIST_INSERT_AFTER(pos->dlip ? pos->dlip : headip, lip, qelem);
		return 1;
	}
	return 0;
}

/*
 * Cross-jumping (-xdmr), c2's xjump and comjump: when the statement before a
 * jump is the same as the statement before the place it jumps to (or before
 * another jump to the same place), the first copy goes, and the jump goes to
 * the second:
 *
 *		X					jbr	Lx
 *		jbr	L		=>	...
 *	...							Lx:	X
 *		X					L:
 *	L:
 *
 * Statements are compared as trees.
 */
static int
nodeeq(NODE *a, NODE *b)
{
	if (a->n_op != b->n_op || a->n_type != b->n_type)
		return 0;
	switch (a->n_op) {
	case NAME: case ICON:
		return strcmp(a->n_name, b->n_name) == 0 &&
		    getlval(a) == getlval(b);
	case REG: case TEMP:
		return regno(a) == regno(b);
	case OREG:
		return a->n_rval == b->n_rval && getlval(a) == getlval(b) &&
		    strcmp(a->n_name, b->n_name) == 0;
	case STASG: case STARG: case STCALL: case USTCALL:
		return 0;
	}
	switch (optype(a->n_op)) {
	case UTYPE:
		return nodeeq(a->n_left, b->n_left);
	case BITYPE:
		return nodeeq(a->n_left, b->n_left) &&
		    nodeeq(a->n_right, b->n_right);
	}
	return 0;
}

static int
isstmt(struct dlnod *p)
{
	return p->op == STMT && p->dlip != 0 && p->dlip->type == IP_NODE;
}

/* the label in front of p: the one already there, or a new one */
static struct dlnod *
labbefore(struct dlnod *p)
{
	struct dlnod *lb;
	struct interpass *lip;

	if (p->back->op == LABEL)
		return p->back;
	lip = rotpool + --rotleft;
	lip->type = IP_DEFLAB;
	lip->ip_lbl = getlab2();
	lb = tmpalloc(sizeof(struct dlnod));
	memset(lb, 0, sizeof(*lb));
	lb->op = LABEL;
	lb->labno = lip->ip_lbl;
	lb->dlip = lip;
	lb->back = p->back;
	lb->forw = p;
	p->back->forw = lb;
	p->back = lb;
	DLIST_INSERT_BEFORE(p->dlip, lip, qelem);
	return lb;
}

/* j jumps to a place that is also reached after e: merge the common tails */
static int
mergetails(struct dlnod *j, struct dlnod *e)
{
	struct dlnod *p1, *p2, *lb;
	int n = 0;

	for (;;) {
		p1 = j->back;
		for (p2 = e->back; p2 && p2->op == LABEL; p2 = p2->back)
			;
		if (p2 == 0 || !isstmt(p1) || !isstmt(p2) || p1 == p2 ||
		    !nodeeq(p1->dlip->ip_node, p2->dlip->ip_node))
			return n;
		if (rotleft <= 0 && p2->back->op != LABEL)
			return n;
		lb = labbefore(p2);
		/* p1 goes; the jump goes to the second copy */
		p1->back->forw = j;
		j->back = p1->back;
		iprem(p1);
		setlab(j, lb->labno);
		j->ref = lb;
		e = lb;
		n++;
	}
}

static int
dmrtails(struct dlnod *dl)
{
	struct dlnod *j, *k;
	int n = 0;

	for (j = dl->forw; j != 0; j = j->forw) {
		if (j->op != JBR || j->ref == 0)
			continue;
		/* the code that falls into the target */
		n += mergetails(j, j->ref);
		/* other jumps to the same place */
		if (j->ref->refc > 1)
			for (k = j->forw; k != 0; k = k->forw)
				if (k->op == JBR && k->ref == j->ref)
					n += mergetails(k, j);
	}
	return n;
}

/*
 * Post-increment through a pointer that lives in memory (-xdmr, targets with
 * deferred addressing: MEM_DEFERRED).  pass 1 splits  *p++ = c  into
 *	t = p;  p = p + 1;  *t = c;
 * which costs a register and an extra instruction.  The increment can follow the
 * use, which then goes through the pointer itself, as dmr-cc has it:
 *	*p = c;  p = p + 1;		movb r4,*_p ; inc _p
 * Done when t has no other use, the statement that uses it has no call (the
 * callee could look at p), no branch, and does not mention p.
 */
#ifndef MEM_DEFERRED
#define MEM_DEFERRED 0
#endif

static int
isptrvar(NODE *x)	/* a pointer held in memory, read twice without effects */
{
	NODE *a;

	if (!ISPTR(x->n_type) || ISVOL(x->n_qual))
		return 0;
	if (x->n_op == NAME)
		return 1;
	/* a variable of the frame, already an OREG off the frame pointer */
	if (x->n_op == OREG)
		return regno(x) == FPREG && !R2TEST(x->n_rval) && x->n_name[0] == '\0';
	if (x->n_op != UMUL)
		return 0;
	/*
	 * A variable of the frame, *(fp+k).  Through any other register the
	 * deferred form  *(rN)  costs an index word, no better than loading it.
	 */
	a = x->n_left;
	if (a->n_op == PLUS && a->n_right->n_op == ICON && a->n_right->n_name[0] == '\0')
		a = a->n_left;
	else if (a->n_op == MINUS && a->n_right->n_op == ICON && a->n_right->n_name[0] == '\0')
		a = a->n_left;
	return a->n_op == REG && regno(a) == FPREG;
}

static int cnt_t, cnt_call;

static void
cntwalk(NODE *p, void *arg)
{
	if (p->n_op == TEMP && regno(p) == cnt_t)
		cnt_call++;
}

static void
callwalk(NODE *p, void *arg)
{
	if (callop(p->n_op))
		cnt_t = -1;
}

static int
mentions(NODE *p, NODE *x)	/* is x, as a tree, somewhere in p? */
{
	if (nodeeq(p, x))
		return 1;
	switch (optype(p->n_op)) {
	case UTYPE: return mentions(p->n_left, x);
	case BITYPE: return mentions(p->n_left, x) || mentions(p->n_right, x);
	}
	return 0;
}

/* replace UMUL(TEMP t) by UMUL(copy of x); returns the number of uses seen */
static int
subst(NODE *p, int t, NODE *x)
{
	int n = 0;

	if (p->n_op == UMUL && p->n_left->n_op == TEMP && regno(p->n_left) == t) {
		nfree(p->n_left);
		p->n_left = tcopy(x);
		return 1;
	}
	switch (optype(p->n_op)) {
	case UTYPE: n += subst(p->n_left, t, x); break;
	case BITYPE: n += subst(p->n_left, t, x); n += subst(p->n_right, t, x); break;
	}
	return n;
}

static int
tempuses(struct p2env *p2e, int t)
{
	struct interpass *ip;

	cnt_t = t;
	cnt_call = 0;
	DLIST_FOREACH(ip, &p2e->ipole, qelem)
		if (ip->type == IP_NODE)
			walkf(ip->ip_node, cntwalk, 0);
	return cnt_call;
}

/* ip1: t = x;  ip2: x = x +- k */
static int
incpair(struct interpass *ip1, struct interpass *ip2, int *tp, NODE **xp)
{
	NODE *n1, *n2, *r;

	if (ip1->type != IP_NODE || ip2->type != IP_NODE)
		return 0;
	n1 = ip1->ip_node;
	n2 = ip2->ip_node;
	if (n1->n_op != ASSIGN || n1->n_left->n_op != TEMP ||
	    !ISPTR(n1->n_left->n_type) || !isptrvar(n1->n_right))
		return 0;
	if (n2->n_op != ASSIGN || !nodeeq(n2->n_left, n1->n_right))
		return 0;
	r = n2->n_right;
	if ((r->n_op != PLUS && r->n_op != MINUS) || r->n_right->n_op != ICON ||
	    r->n_right->n_name[0] != '\0' || !nodeeq(r->n_left, n1->n_right))
		return 0;
	*tp = regno(n1->n_left);
	*xp = n1->n_right;
	return 1;
}

/*
 * Does control go from ip, the statement after a copy, straight to the
 * epilogue: a jump to a label that is followed only by labels and the epilogue,
 * or no statement at all?  Then nothing but the return value is live.
 */
static int
toepilog(struct p2env *p2e, struct interpass *ip)
{
	struct interpass *q, *e = &p2e->ipole;
	NODE *g;

	q = ip;
	if (ip != e && ip->type == IP_NODE && (g = ip->ip_node)->n_op == GOTO) {
		if (g->n_left->n_op != ICON || g->n_left->n_name[0] != '\0')
			return 0;
		for (q = DLIST_NEXT(e, qelem); q != e; q = DLIST_NEXT(q, qelem))
			if (q->type == IP_DEFLAB && q->ip_lbl == getlval(g->n_left))
				break;
	}
	while (q != e && q->type == IP_DEFLAB)
		q = DLIST_NEXT(q, qelem);
	return q != e && q->type == IP_EPILOG;
}

/*
 * (-xdmr) "r = E; ret = r" just before the return, where r is a register and
 * dies there: "ret = E".  The last update of a register variable that is also
 * the result (getw: i |= getc(f) << 8; return i;) is computed in the result
 * register, as dmr-cc does (bis r3,r0), not in r3 and then copied (bis r0,r3;
 * mov r3,r0).
 */
static void
retfold(struct p2env *p2e)
{
	struct interpass *ip, *pip, *next;
	NODE *s1, *s2;
	TWORD t;

	if (!xdmr)
		return;
	for (ip = DLIST_NEXT(&p2e->ipole, qelem); ip != &p2e->ipole; ip = next) {
		next = DLIST_NEXT(ip, qelem);
		if (ip->type != IP_NODE || (s2 = ip->ip_node)->n_op != ASSIGN ||
		    s2->n_left->n_op != REG || s2->n_right->n_op != REG)
			continue;
		pip = DLIST_PREV(ip, qelem);
		if (pip == &p2e->ipole || pip->type != IP_NODE ||
		    (s1 = pip->ip_node)->n_op != ASSIGN || s1->n_left->n_op != REG)
			continue;
		t = s2->n_right->n_type;
		if (regno(s1->n_left) != regno(s2->n_right) ||
		    regno(s2->n_left) == regno(s2->n_right) ||
		    s1->n_left->n_type != t || s2->n_left->n_type != t ||
		    !(t == INT || t == UNSIGNED || ISPTR(t)))
			continue;
		if (!toepilog(p2e, next))
			continue;
		tfree(s1->n_left);
		s1->n_left = s2->n_left;
		nfree(s2->n_right);
		nfree(s2);
		DLIST_REMOVE(ip, qelem);
	}
}

/*
 * (-xdce) The same bit operation on temporaries and constants in the statements of a straight run of code
 * (no label between, branches allowed), with nothing assigned to the temporaries in between: computed once
 * into a new temporary.  A chain of  (alg & ~0xff) == A || (alg & ~0xff) == B || ...  computes the and once.
 * Only and, or, xor, right shift and complement: sums and left shifts are what addresses are made of, and
 * an address mode is free.
 */
struct csecand {
	NODE **slot;		/* where the first one is, to be replaced by the temporary */
	NODE *copy;		/* what it is (the slot changes) */
	struct interpass *ip;
	int temp;
	int nld;		/* memory reads in it: a store or a call ends it */
};
#define	NCSE	24
#define	NCSEPOOL	512
static struct csecand csepool[NCSEPOOL];
static int ncsepool;
static struct csecand *csecands[NCSE];	/* those that hold at this point */
static int ncse;
/* every copy made, to be freed at the end */
static NODE *cseall[512];
static int ncseall;

static int
cseok(NODE *p, int *nld)
{
	switch (p->n_op) {
	case TEMP:
		return !ISFTY(p->n_type) && p->n_type != LONGLONG && p->n_type != ULONGLONG;
	case ICON:
		return p->n_name[0] == '\0';
	case NAME:
		(*nld)++;
		return !ISVOL(p->n_qual) && !ISFTY(p->n_type) && p->n_type != LONGLONG && p->n_type != ULONGLONG &&
		    p->n_type != STRTY && p->n_type != UNIONTY;
	case UMUL:
		(*nld)++;
		return !ISVOL(p->n_qual) && !ISFTY(p->n_type) && p->n_type != LONGLONG && p->n_type != ULONGLONG &&
		    p->n_type != STRTY && p->n_type != UNIONTY && cseok(p->n_left, nld);
	case ADDROF:
		return p->n_left->n_op == NAME && p->n_left->n_name[0] != '\0';
	case FLD:
		return (p->n_left->n_op == UMUL || p->n_left->n_op == NAME) && cseok(p->n_left, nld);
	case AND: case OR: case ER: case RS: case LS: case PLUS: case MINUS: case MUL:
		return cseok(p->n_left, nld) && cseok(p->n_right, nld);
	case COMPL: case SCONV: case PCONV:
		return cseok(p->n_left, nld);
	}
	return 0;
}

/* is it worth a temporary: a bit operation, a field, or a chain of two reads */
static int
cseroot(NODE *p, int *nld)
{
	*nld = 0;
	if (p->n_type == LONGLONG || p->n_type == ULONGLONG || ISFTY(p->n_type))
		return 0;
	if (p->n_op == AND || p->n_op == OR || p->n_op == ER || p->n_op == RS || p->n_op == COMPL || p->n_op == FLD)
		return cseok(p, nld);
	if ((p->n_op == UMUL || p->n_op == SCONV || p->n_op == PCONV) && cseok(p, nld))
		return *nld >= 2;
	return 0;
}

static int
cseeq(NODE *a, NODE *b)
{
	if (a->n_op != b->n_op || a->n_type != b->n_type)
		return 0;
	switch (a->n_op) {
	case NAME: case ICON:
		return strcmp(a->n_name, b->n_name) == 0 && getlval(a) == getlval(b);
	case TEMP:
		return regno(a) == regno(b);
	case FLD:
		if (a->n_rval != b->n_rval)
			return 0;
		break;
	}
	switch (optype(a->n_op)) {
	case LTYPE:
		return 0;
	case UTYPE:
		return cseeq(a->n_left, b->n_left);
	}
	return cseeq(a->n_left, b->n_left) && cseeq(a->n_right, b->n_right);
}

static int
csetemp(NODE *p, int t)
{
	if (p->n_op == TEMP && regno(p) == t)
		return 1;
	if (optype(p->n_op) == LTYPE)
		return 0;
	if (csetemp(p->n_left, t))
		return 1;
	return optype(p->n_op) == BITYPE && csetemp(p->n_right, t);
}

/* the temporaries a statement assigns; *memw is set if it writes memory or calls */
static void
csewrites(NODE *p, int *w, int *nw, int *memw)
{
	if (asgop(p->n_op)) {
		if (p->n_left->n_op == TEMP) {
			if (*nw < 16)
				w[(*nw)++] = regno(p->n_left);
		} else if (p->n_left->n_op != REG)
			*memw = 1;
	}
	if (callop(p->n_op) || p->n_op == STASG || p->n_op == STARG || p->n_op == XASM)
		*memw = 1;
	if (optype(p->n_op) == LTYPE)
		return;
	csewrites(p->n_left, w, nw, memw);
	if (optype(p->n_op) == BITYPE)
		csewrites(p->n_right, w, nw, memw);
}

static int csememw;	/* the statement being looked at writes memory or calls: no memory reads are shared in it */

static void
csewalk(struct p2env *p2e, NODE **pp, NODE *parent, struct interpass *ip, struct csecand **newc, int *nnew)
{
	NODE *p = *pp;
	int i, nld;

	if (parent != NULL && cseroot(p, &nld) && !(csememw && nld)) {
		for (i = 0; i < ncse; i++) {
			struct csecand *c = csecands[i];

			if (cseeq(c->copy, p)) {
				NODE *t, *e;
				struct interpass *nip;

				if (c->temp < 0) {
					c->temp = p2e->epp->ip_tmpnum++;
					e = tcopy(c->copy);
					t = mktemp(c->temp, e->n_type);
					nip = ipnode(mkbinode(ASSIGN, t, e, e->n_type));
					DLIST_INSERT_BEFORE(c->ip, nip, qelem);
					tfree(*c->slot);
					*c->slot = mktemp(c->temp, c->copy->n_type);
				}
				tfree(p);
				*pp = mktemp(c->temp, c->copy->n_type);
				return;
			}
		}
		if (*nnew < NCSE && ncseall < 512 && ncsepool < NCSEPOOL) {
			struct csecand *c = &csepool[ncsepool++];

			c->slot = pp;
			c->copy = cseall[ncseall++] = tcopy(p);
			c->ip = ip;
			c->temp = -1;
			c->nld = nld;
			newc[(*nnew)++] = c;
		}
		return;
	}
	if (optype(p->n_op) == LTYPE)
		return;
	csewalk(p2e, &p->n_left, p, ip, newc, nnew);
	if (optype(p->n_op) == BITYPE)
		csewalk(p2e, &p->n_right, p, ip, newc, nnew);
}

/* the label a GOTO or CBRANCH goes to, or -1 */
static int
cseTarget(NODE *p)
{
	NODE *t = p->n_op == GOTO ? p->n_left : p->n_op == CBRANCH ? p->n_right : NULL;

	if (t != NULL && t->n_op == ICON && t->n_name[0] == '\0')
		return (int)getlval(t);
	return -1;
}

static int cselo, csehi, *cserefs;
static struct { int n; struct csecand *c[NCSE]; } *csesaved;	/* what holds at the jump to a label, by label */

static void
cserefcount(NODE *p, void *arg)
{
	if (p->n_op == ICON && p->n_name[0] == '\0' && getlval(p) >= cselo && getlval(p) < csehi)
		cserefs[getlval(p) - cselo]++;
}

static void
cseblock(struct p2env *p2e)
{
	struct interpass *ip;
	struct csecand *newc[NCSE];
	int nnew, i, j, w[16], nw, memw, lbl, uncond = 0, indirect = 0;

	if (!xdce || !xtemps)
		return;
	ncse = ncsepool = 0;
	cselo = p2e->ipp->ip_lblnum;
	csehi = p2e->epp->ip_lblnum + 1;
	cserefs = tmpcalloc((csehi - cselo + 1) * sizeof(int));
	csesaved = tmpcalloc((csehi - cselo + 1) * sizeof(*csesaved));
	/* a label that one jump leads to, and no other way in: the state at that jump holds there */
	DLIST_FOREACH(ip, &p2e->ipole, qelem) {
		if (ip->type == IP_ASM)
			indirect = 1;
		else if (ip->type == IP_NODE) {
			if (ip->ip_node->n_op == GOTO && cseTarget(ip->ip_node) < 0)
				indirect = 1;
			walkf(ip->ip_node, cserefcount, 0);
		}
	}
	DLIST_FOREACH(ip, &p2e->ipole, qelem) {
		if (ip->type == IP_DEFLAB) {
			lbl = ip->ip_lbl - cselo;
			if (!indirect && uncond && lbl >= 0 && lbl <= csehi - cselo && cserefs[lbl] == 1 &&
			    csesaved[lbl].n > 0) {
				ncse = csesaved[lbl].n - 1;
				for (i = 0; i < ncse; i++)
					csecands[i] = csesaved[lbl].c[i];
			} else
				ncse = 0;
			uncond = 0;
			continue;
		}
		if (ip->type != IP_NODE) {
			ncse = 0;	/* an asm, the ends of the function */
			uncond = 0;
			continue;
		}
		nnew = 0;
		nw = 0;
		memw = 0;
		csewrites(ip->ip_node, w, &nw, &memw);
		csememw = memw;
		csewalk(p2e, &ip->ip_node, NULL, ip, newc, &nnew);
		for (i = 0; i < ncse; ) {
			int kill = 0;

			for (j = 0; j < nw; j++)
				if (csetemp(csecands[i]->copy, w[j]))
					kill = 1;
			if (memw && csecands[i]->nld)
				kill = 1;
			if (kill)
				csecands[i] = csecands[--ncse];
			else
				i++;
		}
		for (i = 0; i < nnew && ncse < NCSE; i++) {
			int kill = 0;

			for (j = 0; j < nw; j++)
				if (csetemp(newc[i]->copy, w[j]))
					kill = 1;
			if (memw && newc[i]->nld)
				kill = 1;
			if (!kill)
				csecands[ncse++] = newc[i];
		}
		uncond = ip->ip_node->n_op == GOTO;
		if ((lbl = cseTarget(ip->ip_node)) >= 0 && lbl - cselo >= 0 && lbl - cselo <= csehi - cselo &&
		    csesaved[lbl - cselo].n == 0) {
			csesaved[lbl - cselo].n = ncse + 1;
			for (i = 0; i < ncse; i++)
				csesaved[lbl - cselo].c[i] = csecands[i];
		}
	}
	ncse = 0;
	while (ncseall > 0)
		tfree(cseall[--ncseall]);
}

/*
 * (-xdce) A temporary with one assignment and one use, the use in the next statement, and a value that is
 * only a copy of another temporary or that plus a constant: the use takes the value itself.  An inlined
 * function's pointer parameter is  t = p + 4;  r = *t;  and  *(p + 4)  is an address mode, not a register
 * and an add.
 */
static int *fw_occ, *fw_def, fw_n;

static void
fw_count(NODE *p, void *arg)
{
	if (p->n_op == TEMP && regno(p) >= 0 && regno(p) < fw_n)
		fw_occ[regno(p)]++;
	if ((asgop(p->n_op)) && p->n_left->n_op == TEMP &&
	    regno(p->n_left) >= 0 && regno(p->n_left) < fw_n)
		fw_def[regno(p->n_left)]++;
}

static int
fw_writes(NODE *p, int a)
{
	if ((asgop(p->n_op)) && p->n_left->n_op == TEMP &&
	    regno(p->n_left) == a)
		return 1;
	if (optype(p->n_op) == LTYPE)
		return 0;
	if (fw_writes(p->n_left, a))
		return 1;
	return optype(p->n_op) == BITYPE && fw_writes(p->n_right, a);
}

static NODE **
fw_find(NODE **pp, int t)
{
	NODE *p = *pp, **q;

	if (p->n_op == TEMP && regno(p) == t)
		return pp;
	if (optype(p->n_op) == LTYPE)
		return NULL;
	if ((q = fw_find(&p->n_left, t)) != NULL)
		return q;
	return optype(p->n_op) == BITYPE ? fw_find(&p->n_right, t) : NULL;
}

static void
fwdtemps(struct p2env *p2e)
{
	struct interpass *ip, *nx, *next;
	NODE *s, *r, *c, **slot;
	int a, t;

	if (!xdce || !xtemps)
		return;
	fw_n = p2e->epp->ip_tmpnum + 1;
	fw_occ = tmpcalloc(fw_n * sizeof(int));
	fw_def = tmpcalloc(fw_n * sizeof(int));
	DLIST_FOREACH(ip, &p2e->ipole, qelem)
		if (ip->type == IP_NODE)
			walkf(ip->ip_node, fw_count, 0);
	for (ip = DLIST_NEXT(&p2e->ipole, qelem); ip != &p2e->ipole; ip = next) {
		next = DLIST_NEXT(ip, qelem);
		if (ip->type != IP_NODE || (s = ip->ip_node)->n_op != ASSIGN || s->n_left->n_op != TEMP)
			continue;
		t = regno(s->n_left);
		if (t < 0 || t >= fw_n || fw_def[t] != 1 || fw_occ[t] != 2)
			continue;
		r = s->n_right;
		if (r->n_op == PLUS && r->n_right->n_op == ICON && r->n_right->n_name[0] == '\0')
			r = r->n_left;
		if (r->n_op != TEMP || r->n_type != s->n_left->n_type || regno(r) == t)
			continue;
		a = regno(r);
		if (s->n_right->n_type != s->n_left->n_type)
			continue;
		if (!ISPTR(s->n_left->n_type))
			continue;
		if (next == &p2e->ipole || next->type != IP_NODE)
			continue;
		nx = next;
		if (fw_writes(nx->ip_node, a) || (slot = fw_find(&nx->ip_node, t)) == NULL)
			continue;
		c = tcopy(s->n_right);
		nfree(*slot);
		*slot = c;
		tfree(s);
		DLIST_REMOVE(ip, qelem);
	}
}

static void
postincfold(struct p2env *p2e)
{
	struct interpass *ip, *ip1, *ip2, *u, *next;
	struct interpass *first[4], *second[4];
	int ts[4], n, i, ok, t;
	NODE *xs[4], *x;

	if (!MEM_DEFERRED || !xdmr || !xtemps)
		return;
	for (ip = DLIST_NEXT(&p2e->ipole, qelem); ip != &p2e->ipole; ip = next) {
		next = DLIST_NEXT(ip, qelem);
		/* a run of  t = x; x = x + k  pairs, then the statement using them */
		n = 0;
		ip1 = ip;
		while (n < 4 && (ip2 = DLIST_NEXT(ip1, qelem)) != &p2e->ipole &&
		    incpair(ip1, ip2, &ts[n], &xs[n])) {
			first[n] = ip1;
			second[n] = ip2;
			n++;
			ip1 = DLIST_NEXT(ip2, qelem);
			if (ip1 == &p2e->ipole)
				break;
		}
		if (n == 0)
			continue;
		u = ip1;
		next = DLIST_NEXT(second[n-1], qelem);
		if (u == &p2e->ipole || u->type != IP_NODE)
			continue;
		switch (u->ip_node->n_op) {
		case CBRANCH: case GOTO: case STASG: case STCALL: case USTCALL:
			continue;
		}
		if (callop(u->ip_node->n_op))
			continue;
		/* no call anywhere in the statement; each pointer used once as *t */
		cnt_t = 0;
		walkf(u->ip_node, callwalk, 0);
		if (cnt_t < 0)
			continue;
		ok = 1;
		for (i = 0; i < n && ok; i++) {
			int j;

			t = ts[i];
			x = xs[i];
			if (tempuses(p2e, t) != 2 || mentions(u->ip_node, x))
				ok = 0;
			for (j = 0; j < i && ok; j++)
				if (nodeeq(xs[j], x) || mentions(xs[j], x) || mentions(x, xs[j]))
					ok = 0;
		}
		if (!ok)
			continue;
		/* every use must be a  *t  */
		for (i = 0; i < n; i++) {
			NODE *copy = tcopy(u->ip_node);

			if (subst(copy, ts[i], xs[i]) != 1) {
				tfree(copy);
				ok = 0;
				break;
			}
			tfree(copy);
		}
		if (!ok)
			continue;
		for (i = 0; i < n; i++)
			subst(u->ip_node, ts[i], xs[i]);
		/* the copies  t = x  go; the increments move behind the use */
		for (i = 0; i < n; i++) {
			DLIST_REMOVE(first[i], qelem);
			tfree(first[i]->ip_node);
		}
		for (i = n - 1; i >= 0; i--) {
			DLIST_REMOVE(second[i], qelem);
			DLIST_INSERT_AFTER(u, second[i], qelem);
		}
		next = DLIST_NEXT(u, qelem);
	}
}

/*
 * Strict statement tree equality for tail merging.  Unlike treecmp()
 * in match.c (which deliberately matches loosely for FINDMOPS), a
 * false match here becomes wrong code, so n_type must agree on every
 * node and any op not explicitly handled compares unequal.  Ops that
 * carry state outside the common node fields (struct size/alignment,
 * bitfield descriptors packed in n_rval, xasm constraint strings)
 * never merge.
 */
static int
dltcmp(NODE *p1, NODE *p2)
{
	if (p1->n_op != p2->n_op || p1->n_type != p2->n_type)
		return 0;

	switch (p1->n_op) {
	case NAME:
	case ICON:
		if (getlval(p1) != getlval(p2) ||
		    strcmp(p1->n_name, p2->n_name) != 0)
			return 0;
		return 1;

	case OREG:
		if (getlval(p1) != getlval(p2) || p1->n_rval != p2->n_rval ||
		    strcmp(p1->n_name, p2->n_name) != 0)
			return 0;
		return 1;

	case REG:
	case TEMP:
		return p1->n_rval == p2->n_rval;

	case STASG:
	case STARG:
	case STCALL:
	case USTCALL:
	case STCLR:
	case FLD:
	case XASM:
	case XARG:
		return 0;
	}

	switch (optype(p1->n_op)) {
	case BITYPE:
		if (dltcmp(p1->n_right, p2->n_right) == 0)
			return 0;
		/* FALLTHROUGH */
	case UTYPE:
		return dltcmp(p1->n_left, p2->n_left);
	default:
		return 0;	/* unknown leaf, play safe */
	}
}

/*
 * Two dlnods are mergeable if both are plain statement trees
 * (IP_ASM also hides behind STMT op) that compare equal.
 */
static int
equop(struct dlnod *p1, struct dlnod *p2)
{
	if (p1 == 0 || p2 == 0 || p1->op != STMT || p2->op != STMT)
		return 0;
	if (p1->dlip->type != IP_NODE || p2->dlip->type != IP_NODE)
		return 0;
	return dltcmp(p1->dlip->ip_node, p2->dlip->ip_node);
}

/*
 * Insert a new label directly before the statement p, in both the
 * dlnod list and the interpass list.  The interpass is tmpalloc'd
 * and must survive this pass, which is why deljumps skips its
 * markset/markfree when tail merging is enabled.
 */
static struct dlnod *
insertl(struct dlnod *p)
{
	struct interpass *ip;
	struct dlnod *lp;

	ip = tmpalloc(sizeof(struct interpass));
	ip->type = IP_DEFLAB;
	ip->lineno = p->dlip->lineno;
	ip->ip_lbl = getlab2();

	lp = tmpalloc(sizeof(struct dlnod));
	lp->op = LABEL;
	lp->labno = ip->ip_lbl;
	lp->dlip = ip;
	lp->ref = 0;
	lp->refc = 0;

	lp->back = p->back;
	lp->forw = p;
	p->back->forw = lp;
	p->back = lp;

	DLIST_INSERT_BEFORE(p->dlip, ip, qelem);
	return lp;
}

/*
 * Cross-jumping: when the statement ahead of a jump is identical to
 * the statement ahead of the jump's target label, put a new label in
 * front of the target's copy and replace our copy with a jump there;
 * repeat until the tails differ.  Labels between the merged statement
 * and the jump (or the target) are unaffected: paths entering there
 * never executed the merged statement.
 */
static void
xjump(struct dlnod *p)
{
	struct dlnod *p1, *p2, *p3;

	if ((p2 = p->ref) == 0)
		return;
	p1 = p;
	for (;;) {
		while ((p1 = p1->back) && p1->op == LABEL)
			;
		while ((p2 = p2->back) && p2->op == LABEL)
			;
		if (p1 == p2 || equop(p1, p2) == 0)
			return;
		p3 = insertl(p2);
		tfree(p1->dlip->ip_node);
		p1->dlip->ip_node = mkunode(GOTO,
		    mklnode(ICON, p3->labno, 0, INT), 0, INT);
		p1->op = JBR;
		p1->labno = p3->labno;
		p1->ref = p3;
		p3->refc++;
		nchange++;
	}
}

/*
 * Merge the identical tails of two jumps to the same label: insert
 * a label ahead of the earlier jump's copy of the tail and let the
 * later jump enter the shared tail there instead, one statement at
 * a time.  Deletions happen only on the later side; a label found
 * there means another path joins in, which ends the merge.
 */
static void
backjmp(struct dlnod *p1, struct dlnod *p2)
{
	struct dlnod *p3;

	for (;;) {
		while (p1->back && p1->back->op == LABEL)
			p1 = p1->back;
		p1 = p1->back;
		p2 = p2->back;
		if (p1 == p2 || equop(p1, p2) == 0)
			return;
		p3 = insertl(p1);
		iprem(p2);		/* frees tree, unlinks interpass */
		p2->back->forw = p2->forw;
		p2->forw->back = p2->back;
		p2 = p2->forw;		/* the jump that lost its tail */
		decref(p2->ref);
		setlab(p2, p3->labno);
		p2->ref = p3;
		p3->refc++;
		nchange++;
	}
}

/*
 * Find pairs of jumps to the same (shared) label and merge their
 * identical tails.
 */
static void
comjump(struct dlnod *dl)
{
	struct dlnod *p1, *p2, *p3;

	for (p1 = dl->forw; p1 != 0; p1 = p1->forw)
		if (p1->op == JBR && (p2 = p1->ref) != 0 && p2->refc > 1)
			for (p3 = p1->forw; p3 != 0; p3 = p3->forw)
				if (p3->op == JBR && p3->ref == p2)
					backjmp(p1, p3);
}

static void
iterate(struct p2env *p2e, struct dlnod *dl)
{
	NODE *q;
	struct dlnod *p, *rp, *p1;
	int i;

	nchange = 0;
	for (p = dl->forw; p!=0; p = p->forw) {
		if ((p->op==JBR||p->op==CBR) && p->ref) {
			/* Resolves:
			 * jbr L7
			 * ...
			 * L7: jbr L8
			 */
			rp = nonlab(p->ref);
			if (rp->op==JBR && rp->labno && p->labno!=rp->labno) {
				setlab(p, rp->labno);
				decref(p->ref);
				rp->ref->refc++;
				p->ref = rp->ref;
				nchange++;
			}
		}
		if (p->op==CBR && (p1 = p->forw)->op==JBR) {
			/* Resolves:
			 * cbr L7
			 * jbr L8
			 * L7:
			 */
			rp = p->ref;
			do
				rp = rp->back;
			while (rp->op==LABEL);
			if (rp==p1) {
				decref(p->ref);
				p->ref = p1->ref;
				setlab(p, p1->labno);

				iprem(p1);

				p1->forw->back = p;
				p->forw = p1->forw;

				/*
				 * Invert the branch condition by negating
				 * the operator, keeping the operand order:
				 * the old c2-style LE/GT/ULE/UGT cases
				 * swapped the operands instead, which moved
				 * compare-against-constant operands to the
				 * left where no imm-compare insn matches.
				 */
				q = p->dlip->ip_node->n_left;
				i = q->n_op;
				/*
				 * The inverse relation, with the operands where
				 * they are: swapping them would put a constant on
				 * the left, which the backends do not expect.
				 */
				if ((i = invrel(i)) < 0)
					comperr("deljumps: unexpected op");
				q->n_op = i;
				nchange++;
			}
		}
		if (p->op == JBR) {
			/* Removes dead code */
			while (p->forw && p->forw->op!=LABEL &&
			    p->forw->op!=EROU) {
				nchange++;
				if (p->forw->ref)
					decref(p->forw->ref);

				iprem(p->forw);

				p->forw = p->forw->forw;
				p->forw->back = p;
			}
			rp = p->forw;
			while (rp && rp->op==LABEL) {
				if (p->ref == rp) {
					p->back->forw = p->forw;
					p->forw->back = p->back;
					iprem(p);
					p = p->back;
					decref(rp);
					nchange++;
					break;
				}
				rp = rp->forw;
			}
		}
		if (p->op == JBR) {
			if (tailmerge)
				xjump(p);
			p = codemove(p);
		}
	}
}

void
deljumps(struct p2env *p2e)
{
	struct interpass *ipole = &p2e->ipole;
	struct dlnod dln;
	MARK mark;

	rotleft = 0;
	if (xdmr) {
		struct interpass *ip;

		DLIST_FOREACH(ip, ipole, qelem)
			if (ip->type == IP_NODE)
				rotleft++;
		rotpool = rotleft ? tmpalloc(rotleft * sizeof(struct interpass)) : 0;
	}
	/*
	 * Tail merging creates label interpasses that must survive
	 * this pass, so the temp heap cannot be rolled back then.
	 * The dlnods are then freed with everything else when the
	 * function is done.
	 */
	if (tailmerge == 0)
		markset(&mark);

	memset(&dln, 0, sizeof(dln));
	listsetup(ipole, &dln);
	do {
		refcount(p2e, &dln);
		do {
			iterate(p2e, &dln);
		} while (nchange);
		if (xdmr) {
			int rot = 0;

			while (looprot(p2e, &dln)) {
				refcount(p2e, &dln);
				rot++;
			}
			nchange = rot;
			rot = dmrtails(&dln);
			if (rot) {
				refcount(p2e, &dln);
				nchange = 1;
			}
		}
		if (tailmerge)
			comjump(&dln);
	} while (nchange);

	if (tailmerge == 0)
		markfree(&mark);
}

void
optdump(struct interpass *ip)
{
	static char *nm[] = { "node", "prolog", "newblk", "epilog", "locctr",
		"deflab", "defnam", "asm" };
	printf("type %s\n", nm[ip->type-1]);
	switch (ip->type) {
	case IP_NODE:
#ifdef PCC_DEBUG
		fwalk(ip->ip_node, e2print, 0);
#endif
		break;
	case IP_DEFLAB:
		printf("label " LABFMT "\n", ip->ip_lbl);
		break;
	case IP_ASM:
		printf("%s", ip->ip_asm);
		break;
	}
}

/*
 * Build the basic blocks, algorithm 9.1, pp 529 in Compilers.
 *
 * Also fills the labelinfo struct with information about which bblocks
 * that contain which label.
 */

/*
 * A casel (VAX jump-table switch) is an XASM followed by a table of offsets:
 * a jump to any of the case labels that the flow graph cannot see otherwise.
 */
static int
iscasel(NODE *p)
{
	return p->n_op == XASM && p->n_name != NULL &&
	    strncmp(p->n_name, "casel", 5) == 0;
}

void
bblocks_build(struct p2env *p2e)
{
	struct interpass *ipole = &p2e->ipole;
	struct interpass *ip;
	struct basicblock *bb = NULL;
	int low, high;
	int count = 0;
	int i;

	BDEBUG(("bblocks_build (%p, %p)\n", &p2e->labinfo, &p2e->bbinfo));
	low = p2e->ipp->ip_lblnum;
	high = p2e->epp->ip_lblnum;

	/* 
	 * First statement is a leader.
	 * Any statement that is target of a jump is a leader.
	 * Any statement that immediately follows a jump is a leader.
	 */
	DLIST_INIT(&p2e->bblocks, bbelem);
	DLIST_FOREACH(ip, ipole, qelem) {
		if (bb == NULL || (ip->type == IP_EPILOG) ||
		    (ip->type == IP_DEFLAB) || (ip->type == IP_DEFNAM)) {
			bb = tmpalloc(sizeof(struct basicblock));
			bb->first = ip;
			SLIST_INIT(&bb->parents);
			SLIST_INIT(&bb->child);
			bb->dfnum = 0;
			bb->dfparent = 0;
			bb->semi = 0;
			bb->ancestor = 0;
			bb->idom = 0;
			bb->samedom = 0;
			bb->bucket = NULL;
			bb->df = NULL;
			bb->dfchildren = NULL;
			bb->Aorig = NULL;
			bb->Aphi = NULL;
			SLIST_INIT(&bb->phi);
			bb->bbnum = count;
			DLIST_INSERT_BEFORE(&p2e->bblocks, bb, bbelem);
			count++;
		}
		bb->last = ip;
		if ((ip->type == IP_NODE) && (ip->ip_node->n_op == GOTO || 
		    ip->ip_node->n_op == CBRANCH || iscasel(ip->ip_node)))
			bb = NULL;
		if (ip->type == IP_PROLOG)
			bb = NULL;
	}
	p2e->nbblocks = count;

	if (b2debug) {
		printf("Basic blocks in func: %d, low %d, high %d\n",
		    count, low, high);
		DLIST_FOREACH(bb, &p2e->bblocks, bbelem) {
			printf("bb(%d) %p: first %p last %p\n", bb->bbnum, bb,
			    bb->first, bb->last);
		}
	}

	p2e->labinfo.low = low;
	p2e->labinfo.size = high - low + 1;
	p2e->labinfo.arr = tmpalloc(p2e->labinfo.size * sizeof(struct basicblock *));
	for (i = 0; i < p2e->labinfo.size; i++) {
		p2e->labinfo.arr[i] = NULL;
	}
	
	p2e->bbinfo.size = count + 1;
	p2e->bbinfo.arr = tmpalloc(p2e->bbinfo.size * sizeof(struct basicblock *));
	for (i = 0; i < p2e->bbinfo.size; i++) {
		p2e->bbinfo.arr[i] = NULL;
	}

	/* Build the label table */
	DLIST_FOREACH(bb, &p2e->bblocks, bbelem) {
		if (bb->first->type == IP_DEFLAB)
			p2e->labinfo.arr[bb->first->ip_lbl - low] = bb;
	}

	if (b2debug) {
		DLIST_FOREACH(bb, &p2e->bblocks, bbelem) {
			printf("bblock %d\n", bb->bbnum);
			for (ip = bb->first; ip != bb->last;
			    ip = DLIST_NEXT(ip, qelem)) {
				printip2(ip);
			}
			printip2(ip);
		}

		printf("Label table:\n");
		for (i = 0; i < p2e->labinfo.size; i++)
			if (p2e->labinfo.arr[i])
				printf("Label %d bblock %p\n", i+low,
				    p2e->labinfo.arr[i]);
	}
}

/*
 * Build the control flow graph.
 */

void
cfg_build(struct p2env *p2e)
{
	/* Child and parent nodes */
	struct cfgnode *cnode; 
	struct cfgnode *pnode;
	struct basicblock *bb;
	NODE *p;

#ifdef notyet
	DLIST_FOREACH(bb, &p2e->bblocks, bbelem) {
		if (bb->first->type == IP_EPILOG)
			break;

		if (bb->last->type == IP_NODE) {
			p = bb->last->ip_node;
			switch (p->n_op) {
			case GOTO:
				if (p->n_left->n_op == ICON) {
					lbchk(p2e, p->n_left->n_lval);
					fillcnode(p2e, p->n_left->n_lval, bb);
				} else {
				}
				break;

			case CBRANCH:
				lbchk(p2e, p->n_right->n_lval);
				fillcnode(p2e, p->n_right->n_lval, bb);
				...

			default:
				...
			}
		} else {
			...
		}
	}
#endif

	DLIST_FOREACH(bb, &p2e->bblocks, bbelem) {
		if (bb->first->type == IP_EPILOG)
			break;
	
		cnode = tmpalloc(sizeof(struct cfgnode));
		pnode = tmpalloc(sizeof(struct cfgnode));
		pnode->bblock = bb;

		p = bb->last->ip_node;
		if (bb->last->type == IP_NODE && p->n_op == GOTO) {
			if (p->n_left->n_op == ICON) {
				if (getlval(p->n_left) - p2e->labinfo.low > p2e->labinfo.size)
					comperr("Label out of range: %d, base %d", 
					    getlval(p->n_left), p2e->labinfo.low);
				cnode->bblock = p2e->labinfo.arr[getlval(p->n_left) - p2e->labinfo.low];
				SLIST_INSERT_LAST(&cnode->bblock->parents, pnode, cfgelem);
				SLIST_INSERT_LAST(&bb->child, cnode, chld);
			} else {
				int *l;
#ifdef mach_z8001
				/*
				 * Computed goto.  A switch-dispatch GOTO(SWDISP)
				 * carries its OWN null-terminated target list in
				 * the GOTO's n_name - edge only to those, so the
				 * case bodies of one switch are not falsely made
				 * predecessors-of/reachable-from every other
				 * computed goto in the function (that over-
				 * connection makes every case body a multi-pred
				 * merge, and SSA removephi then floods the single
				 * dispatch point with phi copies -> spills).  A
				 * true "goto *p" (no SWDISP) still assumes any
				 * address-taken label in ip_labels.
				 */
				if (p->n_left->n_op == SWDISP)
					l = (int *)p->n_name;
				else
#endif
					l = p2e->epp->ip_labels;
				for (; *l; l++) {
					cnode->bblock = p2e->labinfo.arr[*l - p2e->labinfo.low];
					SLIST_INSERT_LAST(&cnode->bblock->parents, pnode, cfgelem);
					SLIST_INSERT_LAST(&bb->child, cnode, chld);
					cnode = tmpalloc(sizeof(struct cfgnode));
					pnode = tmpalloc(sizeof(struct cfgnode));
					pnode->bblock = bb;
				}
			}
			continue;
		}
		if ((bb->last->type == IP_NODE) && p->n_op == CBRANCH) {
			if (getlval(p->n_right) - p2e->labinfo.low > p2e->labinfo.size) 
				comperr("Label out of range: %d", getlval(p->n_left));

			cnode->bblock = p2e->labinfo.arr[getlval(p->n_right) - p2e->labinfo.low];
			SLIST_INSERT_LAST(&cnode->bblock->parents, pnode, cfgelem);
			SLIST_INSERT_LAST(&bb->child, cnode, chld);
			cnode = tmpalloc(sizeof(struct cfgnode));
			pnode = tmpalloc(sizeof(struct cfgnode));
			pnode->bblock = bb;
		}

		if (bb->last->type == IP_NODE && iscasel(p)) {
			int *l;

			/* the table of a casel can reach any label of the function */
			for (l = p2e->epp->ip_labels; *l; l++) {
				cnode->bblock = p2e->labinfo.arr[*l - p2e->labinfo.low];
				SLIST_INSERT_LAST(&cnode->bblock->parents, pnode, cfgelem);
				SLIST_INSERT_LAST(&bb->child, cnode, chld);
				cnode = tmpalloc(sizeof(struct cfgnode));
				pnode = tmpalloc(sizeof(struct cfgnode));
				pnode->bblock = bb;
			}
		}
		cnode->bblock = DLIST_NEXT(bb, bbelem);
		SLIST_INSERT_LAST(&cnode->bblock->parents, pnode, cfgelem);
		SLIST_INSERT_LAST(&bb->child, cnode, chld);
	}
}

void
cfg_dfs(struct basicblock *bb, unsigned int parent, struct bblockinfo *bbinfo)
{
	struct cfgnode *cn;

	if (bb->dfnum != 0)
		return;

	bb->dfnum = ++dfsnum;
	bb->dfparent = parent;
	bbinfo->arr[bb->dfnum] = bb;
	SLIST_FOREACH(cn, &bb->child, chld)
		cfg_dfs(cn->bblock, bb->dfnum, bbinfo);
	/* Don't bring in unreachable nodes in the future */
	bbinfo->size = dfsnum + 1;
}

static bittype *
setalloc(int nelem)
{
	bittype *b;
	int sz = (nelem+NUMBITS-1)/NUMBITS;

	b = tmpalloc(sz * sizeof(bittype));
	memset(b, 0, sz * sizeof(bittype));
	return b;
}

/*
 * Algorithm 19.9, pp 414 from Appel.
 */

void
dominators(struct p2env *p2e)
{
	struct cfgnode *cnode;
	struct basicblock *bb, *y, *v;
	struct basicblock *s, *sprime, *p;
	int h, i;

	DLIST_FOREACH(bb, &p2e->bblocks, bbelem) {
		bb->bucket = setalloc(p2e->bbinfo.size);
		bb->df = setalloc(p2e->bbinfo.size);
		bb->dfchildren = setalloc(p2e->bbinfo.size);
	}

	dfsnum = 0;
	cfg_dfs(DLIST_NEXT(&p2e->bblocks, bbelem), 0, &p2e->bbinfo);

	if (b2debug) {
		struct basicblock *bbb;
		struct cfgnode *ccnode, *cn;

		DLIST_FOREACH(bbb, &p2e->bblocks, bbelem) {
			printf("Basic block %d, parents: ", bbb->dfnum);
			SLIST_FOREACH(ccnode, &bbb->parents, cfgelem) {
				printf("%d, ", ccnode->bblock->dfnum);
			}
			printf("\nChildren: ");
			SLIST_FOREACH(cn, &bbb->child, chld)
				printf("%d, ", cn->bblock->dfnum);
			printf("\n");
		}
	}

	for(h = p2e->bbinfo.size - 1; h > 1; h--) {
		bb = p2e->bbinfo.arr[h];
		p = s = p2e->bbinfo.arr[bb->dfparent];
		SLIST_FOREACH(cnode, &bb->parents, cfgelem) {
			if (cnode->bblock->dfnum ==0)
				continue; /* Ignore unreachable code */

			if (cnode->bblock->dfnum <= bb->dfnum) 
				sprime = cnode->bblock;
			else 
				sprime = p2e->bbinfo.arr[ancestorwithlowestsemi
					      (cnode->bblock, &p2e->bbinfo)->semi];
			if (sprime->dfnum < s->dfnum)
				s = sprime;
		}
		bb->semi = s->dfnum;
		BITSET(s->bucket, bb->dfnum);
		bb->ancestor = p->dfnum;
		for (i = 1; i < p2e->bbinfo.size; i++) {
			if(TESTBIT(p->bucket, i)) {
				v = p2e->bbinfo.arr[i];
				y = ancestorwithlowestsemi(v, &p2e->bbinfo);
				if (y->semi == v->semi) 
					v->idom = p->dfnum;
				else
					v->samedom = y->dfnum;
			}
		}
		memset(p->bucket, 0, (p2e->bbinfo.size + 7)/8);
	}

	if (b2debug) {
		printf("Num\tSemi\tAncest\tidom\n");
		DLIST_FOREACH(bb, &p2e->bblocks, bbelem) {
			printf("%d\t%d\t%d\t%d\n", bb->dfnum, bb->semi,
			    bb->ancestor, bb->idom);
		}
	}

	for(h = 2; h < p2e->bbinfo.size; h++) {
		bb = p2e->bbinfo.arr[h];
		if (bb->samedom != 0) {
			bb->idom = p2e->bbinfo.arr[bb->samedom]->idom;
		}
	}
	DLIST_FOREACH(bb, &p2e->bblocks, bbelem) {
		if (bb->idom != 0 && bb->idom != bb->dfnum) {
			BDEBUG(("Setting child %d of %d\n",
			    bb->dfnum, p2e->bbinfo.arr[bb->idom]->dfnum));
			BITSET(p2e->bbinfo.arr[bb->idom]->dfchildren, bb->dfnum);
		}
	}
}


struct basicblock *
ancestorwithlowestsemi(struct basicblock *bblock, struct bblockinfo *bbinfo)
{
	struct basicblock *u = bblock;
	struct basicblock *v = bblock;

	while (v->ancestor != 0) {
		if (bbinfo->arr[v->semi]->dfnum < 
		    bbinfo->arr[u->semi]->dfnum) 
			u = v;
		v = bbinfo->arr[v->ancestor];
	}
	return u;
}

void
computeDF(struct p2env *p2e, struct basicblock *bblock)
{
	struct cfgnode *cn;
	int h, i;
	
	SLIST_FOREACH(cn, &bblock->child, chld) {
		if (cn->bblock->idom != bblock->dfnum)
			BITSET(bblock->df, cn->bblock->dfnum);
	}
	for (h = 1; h < p2e->bbinfo.size; h++) {
		if (!TESTBIT(bblock->dfchildren, h))
			continue;
		computeDF(p2e, p2e->bbinfo.arr[h]);
		for (i = 1; i < p2e->bbinfo.size; i++) {
			if (TESTBIT(p2e->bbinfo.arr[h]->df, i) && 
			    (p2e->bbinfo.arr[i] == bblock ||
			     (bblock->dfnum != p2e->bbinfo.arr[i]->idom))) 
			    BITSET(bblock->df, i);
		}
	}
}

void printDF(struct p2env *p2e)
{
	struct basicblock *bb;
	int i;

	printf("Dominance frontiers:\n");
    
	DLIST_FOREACH(bb, &p2e->bblocks, bbelem) {
		printf("bb %d : ", bb->dfnum);
	
		for (i=1; i < p2e->bbinfo.size;i++) {
			if (TESTBIT(bb->df,i)) {
				printf("%d ",i);
			}
		}
	    
		printf("\n");
	}
    
}



static struct basicblock *currbb;

/* Helper function for findTemps, Find assignment nodes. */
static void
searchasg(NODE *p, void *arg)
{
	struct pvarinfo *pv;
	int tempnr;
	struct varstack *stacke;
    
	if (p->n_op != ASSIGN)
		return;

	if (p->n_left->n_op != TEMP)
		return;

	tempnr=regno(p->n_left)-defsites.low;
    
	BITSET(currbb->Aorig, tempnr);
	
	pv = tmpcalloc(sizeof(struct pvarinfo));
	pv->next = defsites.arr[tempnr];
	pv->bb = currbb;
	pv->n_type = p->n_left->n_type;
	
	defsites.arr[tempnr] = pv;
	
	
	if (SLIST_FIRST(&defsites.stack[tempnr])==NULL) {
		stacke=tmpcalloc(sizeof (struct varstack));
		stacke->tmpregno=0;
		SLIST_INSERT_FIRST(&defsites.stack[tempnr],stacke,varstackelem);
	}
}

/* Walk the interpass looking for assignment nodes. */
void findTemps(struct interpass *ip)
{
	if (ip->type != IP_NODE)
		return;

	walkf(ip->ip_node, searchasg, 0);
}

/*
 * Algorithm 19.6 from Appel.
 */

void
placePhiFunctions(struct p2env *p2e)
{
	struct basicblock *bb;
	struct basicblock *y;
	struct interpass *ip;
	int maxtmp, i, j, k;
	struct pvarinfo *n;
	struct cfgnode *cnode;
	TWORD ntype;
	struct pvarinfo *pv;
	struct phiinfo *phi;
	int phifound;

	bb = DLIST_NEXT(&p2e->bblocks, bbelem);
	defsites.low = ((struct interpass_prolog *)bb->first)->ip_tmpnum;
	bb = DLIST_PREV(&p2e->bblocks, bbelem);
	maxtmp = ((struct interpass_prolog *)bb->first)->ip_tmpnum;
	defsites.size = maxtmp - defsites.low + 1;
	defsites.arr = tmpcalloc(defsites.size*sizeof(struct pvarinfo *));
	defsites.stack = tmpcalloc(defsites.size*sizeof(SLIST_HEAD(, varstack)));
	
	for (i=0;i<defsites.size;i++)
		SLIST_INIT(&defsites.stack[i]);	
	
	/* Find all defsites */
	DLIST_FOREACH(bb, &p2e->bblocks, bbelem) {
		currbb = bb;
		ip = bb->first;
		bb->Aorig = setalloc(defsites.size);
		bb->Aphi = setalloc(defsites.size);
		
		while (ip != bb->last) {
			findTemps(ip);
			ip = DLIST_NEXT(ip, qelem);
		}
		/* Make sure we get the last statement in the bblock */
		findTemps(ip);
	}

	/* For each variable */
	for (i = 0; i < defsites.size; i++) {
		/* While W not empty */
		while (defsites.arr[i] != NULL) {
			/* Remove some node n from W */
			n = defsites.arr[i];
			defsites.arr[i] = n->next;
			/* For each y in n->bb->df */
			for (j = 0; j < p2e->bbinfo.size; j++) {
				if (!TESTBIT(n->bb->df, j))
					continue;
				
				if (TESTBIT(p2e->bbinfo.arr[j]->Aphi, i))
					continue;

				y=p2e->bbinfo.arr[j];
				ntype = n->n_type;
				k = 0;
				/* Amount of predecessors for y */
				SLIST_FOREACH(cnode, &y->parents, cfgelem) 
					k++;
				/* Construct phi(...) 
				*/
			    
				phifound=0;
			    
				SLIST_FOREACH(phi, &y->phi, phielem) {
				    if (phi->tmpregno==i+defsites.low)
					phifound++;
				}
			    
				if (phifound==0) {
					if (b2debug)
					    printf("Phi in %d(%d) (%p) for %d\n",
					    y->dfnum,y->bbnum,y,i+defsites.low);

					/* If no live in, no phi node needed */
					if (!TESTBIT(y->in,
					    (i+defsites.low-p2e->ipp->ip_tmpnum+MAXREGS))) {
					if (b2debug)
					printf("tmp %d bb %d unused, no phi\n",
					    i+defsites.low, y->bbnum);
						/* No live in */
						BITSET(p2e->bbinfo.arr[j]->Aphi, i);
						continue;
					}

					phi = tmpcalloc(sizeof(struct phiinfo));
			    
					phi->tmpregno=i+defsites.low;
					phi->size=k;
					phi->n_type=ntype;
					phi->intmpregno=tmpcalloc(k*sizeof(int));
			    
					SLIST_INSERT_LAST(&y->phi,phi,phielem);
				} else {
				    if (b2debug)
					printf("Phi already in %d for %d\n",y->dfnum,i+defsites.low);
				}

				BITSET(p2e->bbinfo.arr[j]->Aphi, i);
				if (!TESTBIT(p2e->bbinfo.arr[j]->Aorig, i)) {
					pv = tmpalloc(sizeof(struct pvarinfo));
					pv->bb = y;
				        pv->n_type=ntype;
					pv->next = defsites.arr[i];
					defsites.arr[i] = pv;
				}
			}
		}
	}
}

/* Helper function for renamevar. */
static void
renamevarhelper(struct p2env *p2e,NODE *t,void *poplistarg)
{	
	SLIST_HEAD(, varstack) *poplist=poplistarg;
	int opty;
	int tempnr;
	int newtempnr;
	int x;
	struct varstack *stacke;
	
	if (t->n_op == ASSIGN && t->n_left->n_op == TEMP) {
		renamevarhelper(p2e,t->n_right,poplist);
				
		tempnr=regno(t->n_left)-defsites.low;
		
		newtempnr=p2e->epp->ip_tmpnum++;
		regno(t->n_left)=newtempnr;
		
		stacke=tmpcalloc(sizeof (struct varstack));
		stacke->tmpregno=newtempnr;
		SLIST_INSERT_FIRST(&defsites.stack[tempnr],stacke,varstackelem);
		
		stacke=tmpcalloc(sizeof (struct varstack));
		stacke->tmpregno=tempnr;
		SLIST_INSERT_FIRST(poplist,stacke,varstackelem);
	} else {
		if (t->n_op == TEMP) {
			tempnr=regno(t)-defsites.low;
		
			if (SLIST_FIRST(&defsites.stack[tempnr])!=NULL) {
				x=SLIST_FIRST(&defsites.stack[tempnr])->tmpregno;
				if (x == 0) {
					warner(Wuninitialized);
					x = defsites.low; /* can be anything */
				}
				regno(t)=x;
			}
		}
		
		opty = optype(t->n_op);
		
		if (opty != LTYPE)
			renamevarhelper(p2e, t->n_left,poplist);
		if (opty == BITYPE)
			renamevarhelper(p2e, t->n_right,poplist);
	}
}


void
renamevar(struct p2env *p2e,struct basicblock *bb)
{
    	struct interpass *ip;
	int h,j;
	SLIST_HEAD(, varstack) poplist;
	struct varstack *stacke;
	struct cfgnode *cfgn2, *cn;
	int tmpregno,newtmpregno;
	struct phiinfo *phi;

	SLIST_INIT(&poplist);

	SLIST_FOREACH(phi,&bb->phi,phielem) {
		tmpregno=phi->tmpregno-defsites.low;
		
		newtmpregno=p2e->epp->ip_tmpnum++;
		phi->newtmpregno=newtmpregno;

		stacke=tmpcalloc(sizeof (struct varstack));
		stacke->tmpregno=newtmpregno;
		SLIST_INSERT_FIRST(&defsites.stack[tmpregno],stacke,varstackelem);
		
		stacke=tmpcalloc(sizeof (struct varstack));
		stacke->tmpregno=tmpregno;
		SLIST_INSERT_FIRST(&poplist,stacke,varstackelem);		
	}


	ip=bb->first;

	while (1) {		
		if ( ip->type == IP_NODE) {
			renamevarhelper(p2e,ip->ip_node,&poplist);
		}

		if (ip==bb->last)
			break;

		ip = DLIST_NEXT(ip, qelem);
	}

	SLIST_FOREACH(cn, &bb->child, chld) {
		j=0;

		SLIST_FOREACH(cfgn2, &cn->bblock->parents, cfgelem) { 
			if (cfgn2->bblock->dfnum==bb->dfnum)
				break;
			
			j++;
		}

		SLIST_FOREACH(phi,&cn->bblock->phi,phielem) {
			phi->intmpregno[j]=SLIST_FIRST(&defsites.stack[phi->tmpregno-defsites.low])->tmpregno;
		}
	}

	for (h = 1; h < p2e->bbinfo.size; h++) {
		if (!TESTBIT(bb->dfchildren, h))
			continue;

		renamevar(p2e,p2e->bbinfo.arr[h]);
	}

	SLIST_FOREACH(stacke,&poplist,varstackelem) {
		tmpregno=stacke->tmpregno;
		
		defsites.stack[tmpregno].q_forw=defsites.stack[tmpregno].q_forw->varstackelem.q_forw;
	}
}

static struct cpinfo {
	int ipb, asz;
	CONSZ *conp;
	char **namep;
	int *valp;
} cpi;

static void
getconst(NODE *p)
{
	int ix;

	if (p->n_op == ASSIGN && p->n_left->n_op == TEMP && 
	    p->n_right->n_op == ICON) {
		ix = regno(p->n_left) - cpi.ipb;
//printf("Found %d val " CONFMT " name \n", regno(p->n_left), getlval(p->n_right));
		if (cpi.valp[ix])
			comperr("TEMP %d found", regno(p->n_left));
		cpi.valp[ix] = 1;
		cpi.conp[ix] = getlval(p->n_right);
		cpi.namep[ix] = p->n_right->n_name;
	}
	if (optype(p->n_op) == BITYPE)
		getconst(p->n_right);
	if (optype(p->n_op) != LTYPE)
		getconst(p->n_left);
}

static int replaced;

static void
replconst(NODE *p)
{  

	if (p->n_op == ASSIGN && p->n_left->n_op == TEMP) {
		replconst(p->n_right);
	} else if (p->n_op == TEMP) {
		if (cpi.valp[regno(p) - cpi.ipb]) {
//printf("replacing %d %p\n", regno(p), p);
			p->n_op = ICON;
			setlval(p, cpi.conp[regno(p) - cpi.ipb]);
			p->n_name = cpi.namep[regno(p) - cpi.ipb];
			replaced = 1;
			regno(p) = 0;
		}
	} else {
		if (optype(p->n_op) == BITYPE)
			replconst(p->n_right);
		if (optype(p->n_op) != LTYPE)
			replconst(p->n_left);
	}
}

/*
 * Simple version of constant propagation.
 * Should be replaced with conditional constant propagation someday.
 */
void
simple_cp(struct p2env *p2e)
{
	struct interpass *ip;

	cpi.ipb = p2e->ipp->ip_tmpnum;
	cpi.asz = p2e->epp->ip_tmpnum - cpi.ipb;
	cpi.conp = tmpcalloc(cpi.asz * sizeof(CONSZ));
	cpi.valp = tmpcalloc(cpi.asz * sizeof(int));
	cpi.namep = tmpcalloc(cpi.asz * sizeof(char *));

	/* First loop over and get all constants */
	DLIST_FOREACH(ip, &p2e->ipole, qelem) {
		if (ip->type == IP_NODE)
			getconst(ip->ip_node);
	}

	do {
		replaced = 0;
		/* Second loop replaces all uses of the constants */
		DLIST_FOREACH(ip, &p2e->ipole, qelem) {
			if (ip->type == IP_NODE)
				replconst(ip->ip_node);
		}
	} while (replaced);
}

enum pred_type {
    pred_unknown    = 0,
    pred_goto       = 1,
    pred_cond       = 2,
    pred_falltrough = 3,
} ;

void
removephi(struct p2env *p2e)
{
	struct basicblock *bb,*bbparent;
	struct cfgnode *cfgn;
	struct phiinfo *phi;
	int i;
	struct interpass *ip;
	struct interpass *pip;
	TWORD n_type;

	enum pred_type complex = pred_unknown ;

	int label=0;
	int newlabel;

	DLIST_FOREACH(bb, &p2e->bblocks, bbelem) {		
		SLIST_FOREACH(phi,&bb->phi,phielem) {
			/* Look at only one, notice break at end */
			i=0;
			
			SLIST_FOREACH(cfgn, &bb->parents, cfgelem) { 

				bbparent = cfgn->bblock;	/* cfg parent basic block */
				pip = bbparent->last;		/* last stmt in parent bb (goto, ...) */
				
				complex = pred_unknown ;
				BDEBUG(("removephi: %p in %d",pip,bb->dfnum));

				if (pip->type == IP_NODE && pip->ip_node->n_op == GOTO) {
					BDEBUG((" GOTO "));
					label = (int)getlval(pip->ip_node->n_left);
					complex = pred_goto ;
				} else if (pip->type == IP_NODE && pip->ip_node->n_op == CBRANCH) {
					BDEBUG((" CBRANCH "));
					label = (int)getlval(pip->ip_node->n_right);

					/* Check if parent got us here via branch */
					if (bb == p2e->labinfo.arr[label - p2e->ipp->ip_lblnum])
						complex = pred_cond ;
					else
						complex = pred_falltrough ;

				} else if (DLIST_PREV(bb, bbelem) == bbparent) {
					complex = pred_falltrough ;
				} else {
					    /* PANIC */
					comperr("Assumption blown in rem-phi") ;
				}
       
				BDEBUG((" Complex: %d ",complex)) ;

				switch (complex) {
				  case pred_goto:
					/* gotos can only go to this place. No bounce tab needed */
					SLIST_FOREACH(phi,&bb->phi,phielem) {
						if (phi->intmpregno[i]>0) {
							n_type=phi->n_type;
							ip = ipnode(mkbinode(ASSIGN,
							     mktemp(phi->newtmpregno, n_type),
							     mktemp(phi->intmpregno[i],n_type),
							     n_type));
							BDEBUG(("(%p, %d -> %d) ", ip, phi->intmpregno[i], phi->newtmpregno));
				
							DLIST_INSERT_BEFORE((bbparent->last), ip, qelem);
						}
					}
					break ;
				  case pred_cond:
					/* Here, we need a jump pad */
					newlabel=getlab2();
			
					ip = tmpalloc(sizeof(struct interpass));
					ip->type = IP_DEFLAB;
					/* Line number?? ip->lineno; */
					ip->ip_lbl = newlabel;
					DLIST_INSERT_BEFORE((bb->first), ip, qelem);

					SLIST_FOREACH(phi,&bb->phi,phielem) {
						if (phi->intmpregno[i]>0) {
							n_type=phi->n_type;
							ip = ipnode(mkbinode(ASSIGN,
							     mktemp(phi->newtmpregno, n_type),
							     mktemp(phi->intmpregno[i],n_type),
							     n_type));

							BDEBUG(("(%p, %d -> %d) ", ip, phi->intmpregno[i], phi->newtmpregno));
							DLIST_INSERT_BEFORE((bb->first), ip, qelem);
						}
					}
					/* add a jump to us */
					ip = ipnode(mkunode(GOTO, mklnode(ICON, label, 0, INT), 0, INT));
					DLIST_INSERT_BEFORE((bb->first), ip, qelem);
					setlval(pip->ip_node->n_right,newlabel);
					if (!logop(pip->ip_node->n_left->n_op))
						comperr("SSA not logop");
					pip->ip_node->n_left->n_label=newlabel;
					break ;
				  case pred_falltrough:
					if (bb->first->type == IP_DEFLAB) { 
						label = bb->first->ip_lbl; 
						BDEBUG(("falltrough label %d\n", label));
					} else {
						comperr("BBlock has no label?") ;
					}

					/* 
					 * add a jump to us. We _will_ be, or already have, added code in between.
					 * The code is created in the wrong order and switched at the insert, thus
					 * comming out correctly
					 */

					ip = ipnode(mkunode(GOTO, mklnode(ICON, label, 0, INT), 0, INT));
					DLIST_INSERT_AFTER((bbparent->last), ip, qelem);

					/* Add the code to the end, add a jump to us. */
					SLIST_FOREACH(phi,&bb->phi,phielem) {
						if (phi->intmpregno[i]>0) {
							n_type=phi->n_type;
							ip = ipnode(mkbinode(ASSIGN,
								mktemp(phi->newtmpregno, n_type),
								mktemp(phi->intmpregno[i],n_type),
								n_type));

							BDEBUG(("(%p, %d -> %d) ", ip, phi->intmpregno[i], phi->newtmpregno));
							DLIST_INSERT_AFTER((bbparent->last), ip, qelem);
						}
					}
					break ;
				default:
					comperr("assumption blown, complex is %d\n", complex) ;
				}
				BDEBUG(("\n"));
				i++;
			}
			break;
		}
	}
}

    
/*
 * Remove unreachable nodes in the CFG.
 */ 

void
remunreach(struct p2env *p2e)
{
	struct basicblock *bb, *nbb;
	struct interpass *next, *ctree;

	bb = DLIST_NEXT(&p2e->bblocks, bbelem);
	while (bb != &p2e->bblocks) {
		nbb = DLIST_NEXT(bb, bbelem);

		/* Code with dfnum 0 is unreachable */
		if (bb->dfnum != 0) {
			bb = nbb;
			continue;
		}

		/* Need the epilogue node for other parts of the
		   compiler, set its label to 0 and backend will
		   handle it. */ 
		if (bb->first->type == IP_EPILOG) {
			bb->first->ip_lbl = 0;
			bb = nbb;
			continue;
		}

		next = bb->first;
		do {
			ctree = next;
			next = DLIST_NEXT(ctree, qelem);
			
			if (ctree->type == IP_NODE)
				tfree(ctree->ip_node);
			DLIST_REMOVE(ctree, qelem);
		} while (ctree != bb->last);
			
		DLIST_REMOVE(bb, bbelem);
		bb = nbb;
	}
}

static void
printip2(struct interpass *ip)
{
	static char *foo[] = {
	   0, "NODE", "PROLOG", "STKOFF", "EPILOG", "DEFLAB", "DEFNAM", "ASM" };
	struct interpass_prolog *ipplg, *epplg;
	int *l;

	if (ip->type > MAXIP)
		printf("IP(%d) (%p): ", ip->type, ip);
	else
		printf("%s (%p): ", foo[ip->type], ip);
	switch (ip->type) {
	case IP_NODE: printf("\n");
#ifdef PCC_DEBUG
	fwalk(ip->ip_node, e2print, 0); break;
#endif
	case IP_PROLOG:
		ipplg = (struct interpass_prolog *)ip;
		printf("%s %s regs",
		    ipplg->ipp_name, ipplg->ipp_vis ? "(local)" : "");
		printf(" autos %d mintemp %d minlbl %d\n",
		    ipplg->ipp_autos, ipplg->ip_tmpnum, ipplg->ip_lblnum);
		break;
	case IP_EPILOG:
		epplg = (struct interpass_prolog *)ip;
		printf("%s %s regs",
		    epplg->ipp_name, epplg->ipp_vis ? "(local)" : "");
		printf(" autos %d mintemp %d minlbl %d\ncgoto labels: ",
		    epplg->ipp_autos, epplg->ip_tmpnum, epplg->ip_lblnum);
		for (l = epplg->ip_labels; *l; l++)
			printf("%d ", *l);
		printf("\n");
		break;
	case IP_DEFLAB: printf(LABFMT "\n", ip->ip_lbl); break;
	case IP_DEFNAM: printf("\n"); break;
	case IP_ASM: printf("%s\n", ip->ip_asm); break;
	default:
		break;
	}
}

void
printip(struct interpass *pole)
{
	struct interpass *ip;

	DLIST_FOREACH(ip, pole, qelem)
		printip2(ip);
}

#ifdef PCC_DEBUG
void flownodeprint(NODE *p,FILE *flowdiagramfile);

void
flownodeprint(NODE *p,FILE *flowdiagramfile)
{	
	struct attr *ap;
	int opty;
	char *o;

	fprintf(flowdiagramfile,"{");

	o = opst[p->n_op];
	
	while (*o != 0) {
		if (*o == '<' || *o == '>')
			fputc('\\', flowdiagramfile);
		
		fputc(*o, flowdiagramfile);
		o++;
	}
	
	switch( p->n_op ) {			
		case REG:
			fprintf(flowdiagramfile, " %s", rnames[p->n_rval]);
			break;

		case TEMP:
			fprintf(flowdiagramfile, " %d", regno(p));
			break;

		case XASM:
		case XARG:
			fprintf(flowdiagramfile, " '%s'", p->n_name);
			break;

		case ICON:
		case NAME:
		case OREG:
			fprintf(flowdiagramfile, " " );
			adrput(flowdiagramfile, p );
			break;

		case STCALL:
		case USTCALL:
		case STARG:
		case STASG:
			ap = attr_find(p->n_ap, ATTR_P2STRUCT);
			fprintf(flowdiagramfile, " size=%d", ap->iarg(0));
			fprintf(flowdiagramfile, " align=%d", ap->iarg(1));
			break;
	}
	
	opty = optype(p->n_op);
	
	if (opty != LTYPE) {
		fprintf(flowdiagramfile, "| {");
	
		flownodeprint(p->n_left, flowdiagramfile);
	
		if (opty == BITYPE) {
			fprintf(flowdiagramfile, "|");
			flownodeprint(p->n_right, flowdiagramfile);
		}
		fprintf(flowdiagramfile, "}");
	}
	
	fprintf(flowdiagramfile, "}");
}

void
printflowdiagram(struct p2env *p2e, char *type) {
	struct basicblock *bbb;
	struct cfgnode *cn;
	struct interpass *ip;
	struct interpass_prolog *plg;
	struct phiinfo *phi;
	char *name;
	char *filename;
	int filenamesize;
	char *ext = ".dot";
	FILE *flowdiagramfile;
	
	if (!g2debug)
		return;
	
	bbb = DLIST_NEXT(&p2e->bblocks, bbelem);
	ip = bbb->first;

	if (ip->type != IP_PROLOG)
		return;
	plg = (struct interpass_prolog *)ip;

	name = plg->ipp_name;
	
	filenamesize = (int)(strlen(name)+1+strlen(type)+strlen(ext)+1);
	filename = tmpalloc(filenamesize);
	snprintf(filename, filenamesize,"%s-%s%s",name,type,ext);
	
	flowdiagramfile = fopen(filename,"w");
	
	fprintf(flowdiagramfile, "digraph {\n");
	fprintf(flowdiagramfile, "rankdir=LR\n");
	
	DLIST_FOREACH(bbb, &p2e->bblocks, bbelem) {
		ip = bbb->first;
		
		fprintf(flowdiagramfile, "bb%p [shape=record ",bbb);
		
		if (ip->type == IP_PROLOG)
			fprintf(flowdiagramfile, "root ");

		fprintf(flowdiagramfile, "label=\"");
		
		SLIST_FOREACH(phi, &bbb->phi, phielem) {
			fprintf(flowdiagramfile, "Phi %d|", phi->tmpregno);
		}		
		
		
		while (1) {
			switch (ip->type) {
				case IP_NODE: 
					flownodeprint(ip->ip_node, flowdiagramfile);
					break;

				case IP_DEFLAB: 
					fprintf(flowdiagramfile, "Label: %d", ip->ip_lbl);
					break;

				case IP_PROLOG:
					plg = (struct interpass_prolog *)ip;
					fprintf(flowdiagramfile, "%s %s", plg->ipp_name, type);
					break;
			}

			fprintf(flowdiagramfile, "|");
			fprintf(flowdiagramfile, "|");

			if (ip == bbb->last)
				break;

			ip = DLIST_NEXT(ip, qelem);
		}
		fprintf(flowdiagramfile, "\"]\n");
		
		SLIST_FOREACH(cn, &bbb->child, chld) {
			char *color = "black";
			struct interpass *pip = bbb->last;

			if (pip->type == IP_NODE &&
			    pip->ip_node->n_op == CBRANCH) {
				int label = (int)getlval(pip->ip_node->n_right);

				if (cn->bblock ==
				  p2e->labinfo.arr[label - p2e->ipp->ip_lblnum])
					color = "red";
			}

			fprintf(flowdiagramfile, "bb%p -> bb%p [color=%s]\n",
			    bbb, cn->bblock, color);
		}
	}

	fprintf(flowdiagramfile, "}\n");
	fclose(flowdiagramfile);
}

#endif

/* walk all the programm */
void WalkAll(struct p2env* p2e, void (*f) (NODE*, void*), void* arg, int type)
{
	struct interpass *ipole = &p2e->ipole;
	struct interpass *ip ;
	if (0 != type) {
		DLIST_FOREACH(ip, ipole, qelem) {
			if (ip->type == IP_NODE && ip->ip_node->n_op == type)
				walkf(ip->ip_node, f, arg) ;
		}
	} else {
		DLIST_FOREACH(ip, ipole, qelem) {
			if (ip->type == IP_NODE)
				walkf(ip->ip_node, f, arg) ;
		}
	}
}
#if 0
static int is_goto_label(struct interpass*g, struct interpass* l)
{
	if (!g || !l)
		return 0 ;
	if (g->type != IP_NODE) return 0 ;
	if (l->type != IP_DEFLAB) return 0 ;
	if (g->ip_node->n_op != GOTO) return 0 ;
	if (g->ip_node->n_left->n_lval != l->ip_lbl) return 0 ;
	return 1 ;
}
#endif

/*
 * iterate over the basic blocks. 
 * In case a block has only one successor and that one has only one pred, and the link is a goto:
 * place the second one immediately behind the first one (in terms of nodes, means cut&resplice). 
 * This should take care of a lot of jumps.
 * one problem: Cannot move the first or last basic block (prolog/epilog). This is taken care of by
 * moving block #1 to #2, not #2 to #1
 * More complex (on the back cooker;) : first coalesc the inner loops (L1 cache), then go from inside out.
 */

static unsigned long count_blocks(struct p2env* p2e)
{
	struct basicblock* bb ;
	unsigned long count = 0 ;
	DLIST_FOREACH(bb, &p2e->bblocks, bbelem) {
		++count ;
	}
	return count ;
}

struct block_map {
	struct basicblock* block ;
	unsigned long index ;
	unsigned long thread ;
} ;

static unsigned long map_blocks(struct p2env* p2e, struct block_map* map, unsigned long count)
{
	struct basicblock* bb ;
	unsigned long indx = 0 ;
	int ignore = 2 ;
	unsigned long thread ;
	unsigned long changes ;

	DLIST_FOREACH(bb, &p2e->bblocks, bbelem) {
		map[indx].block = bb ;
		map[indx].index = indx ;

		/* ignore the first 2 labels, maybe up to 3 BBs */
		if (ignore) {
			if (bb->first->type == IP_DEFLAB) 
				--ignore;

			map[indx].thread = 1 ;	/* these are "fixed" */
		} else
			map[indx].thread = 0 ;

		indx++ ;
	}

	thread = 1 ;
	do {
		changes = 0 ;
		
		for (indx=0; indx < count; indx++) {
			/* find block without trace */
			if (map[indx].thread == 0) {
				/* do new thread */
				struct cfgnode *cn ;
				struct basicblock *block2 = 0;
				unsigned long i ;
				unsigned long added ;
				
				BDEBUG (("new thread %ld at block %ld\n", thread, indx)) ;

				bb = map[indx].block ;
				do {
					added = 0 ;

					for (i=0; i < count; i++) {
						if (map[i].block == bb && map[i].thread == 0) {
							map[i].thread = thread ;

							BDEBUG(("adding block %ld to trace %ld\n", i, thread)) ;

							changes ++ ;
							added++ ;

							/* 
							 * pick one from followers. For now (simple), pick the 
							 * one who is directly following us. If none of that exists,
							 * this code picks the last one.
							 */

							SLIST_FOREACH(cn, &bb->child, chld) {
								block2=cn->bblock ;
#if 1
								if (i+1 < count && map[i+1].block == block2)
									break ; /* pick that one */
#else
								if (block2) break ;
#endif
							}

							if (block2)
								bb = block2 ;
						}
					}
				} while (added) ;
				thread++ ;
			}
		}
	} while (changes);

	/* Last block is also a thread on it's own, and the highest one. */
/*
	thread++ ;
	map[count-1].thread = thread ;
*/
	if (b2debug) {
		printf("Threads\n");
		for (indx=0; indx < count; indx++) {
			printf("Block #%ld (lbl %d) Thread %ld\n", indx, map[indx].block->first->ip_lbl, map[indx].thread);
		}
	}
	return thread ;
}


void TraceSchedule(struct p2env* p2e)
{
	struct block_map* map ;
	unsigned long block_count = count_blocks(p2e);
	unsigned long i ;
	struct interpass *front, *back ;

	map = tmpalloc(block_count * sizeof(struct block_map));

	(void)map_blocks(p2e, map, block_count);

	back = map[0].block->last ;
	for (i=1; i < block_count; i++) {
		/* collect the trace */
		unsigned long j ;
		unsigned long thread = map[i].thread ;
		if (thread) {
			BDEBUG(("Thread %ld\n", thread)) ;
			for (j=i; j < block_count; j++) {
				if (map[j].thread == thread) {
					front = map[j].block->first ;

					BDEBUG(("Trace %ld, old BB %ld, next BB %ld\t",
						thread, i, j)) ;
					BDEBUG(("Label %d\n", front->ip_lbl)) ;
					DLIST_NEXT(back, qelem) = front ;
					DLIST_PREV(front, qelem) = back ;
					map[j].thread = 0 ;	/* done */
					back = map[j].block->last ;
					DLIST_NEXT(back, qelem) = 0 ;
				}
			}
		}
	}
	DLIST_NEXT(back, qelem) = &(p2e->ipole) ;
	DLIST_PREV(&p2e->ipole, qelem) = back ;
}

static void add_labels(struct p2env* p2e)
{
	struct interpass *ipole = &p2e->ipole ;
	struct interpass *ip ;

	DLIST_FOREACH(ip, ipole, qelem) {
		if (ip->type == IP_NODE && ip->ip_node->n_op == CBRANCH) {
			struct interpass *n = DLIST_NEXT(ip, qelem);
			if (n && n->type != IP_DEFLAB) {
				struct interpass* lab;
				int newlabel=getlab2() ;

				BDEBUG(("add_label L%d\n", newlabel));

				lab = tmpalloc(sizeof(struct interpass));
				lab->type = IP_DEFLAB;
				/* Line number?? ip->lineno; */
				lab->ip_lbl = newlabel;
				DLIST_INSERT_AFTER(ip, lab, qelem);
			}
		}
	}
}

/* node map */
#ifdef ENABLE_NEW
struct node_map {
	NODE* node ;		/* the node */
	unsigned int node_num ; /* node is equal to that one */
	unsigned int var_num ;	/* node computes this variable */
} ;

static unsigned long nodes_counter ;
static void node_map_count_walker(NODE* n, void* x)
{
	nodes_counter ++ ;
}

static void do_cse(struct p2env* p2e)
{
	nodes_counter = 0 ;
	WalkAll(p2e, node_map_count_walker, 0, 0) ;
	BDEBUG(("Found %ld nodes\n", nodes_counter)) ;
}
#endif

#define BITALLOC(ptr,all,sz) { \
	int sz__s = BIT2BYTE(sz); ptr = all(sz__s); memset(ptr, 0, sz__s); }
#define VALIDREG(p)	(p->n_op == REG && TESTBIT(validregs, regno(p)))
#define XCHECK(x) if (x < 0 || x >= xbits) printf("x out of range %d\n", x)
#define RUP(x) (((x)+NUMBITS-1)/NUMBITS)
#define SETCOPY(t,f,i,n) for (i = 0; i < RUP(n); i++) t[i] = f[i]
#define SETSET(t,f,i,n) for (i = 0; i < RUP(n); i++) t[i] |= f[i]
#define SETCLEAR(t,f,i,n) for (i = 0; i < RUP(n); i++) t[i] &= ~f[i]
#define SETCMP(v,t,f,i,n) for (i = 0; i < RUP(n); i++) \
	if (t[i] != f[i]) v = 1

static int xxx, xbits;
/*
 * Set/clear long term liveness for regs and temps.
 */
static void
unionize(NODE *p, struct basicblock *bb, int suboff)
{
	int o, ty;

	if ((o = p->n_op) == TEMP || VALIDREG(p)) {
		int b = regno(p);
		if (o == TEMP)
			b = b - suboff + MAXREGS;
XCHECK(b);
		BITSET(bb->gen, b);
	}
	if (asgop(o)) {
		if (p->n_left->n_op == TEMP || VALIDREG(p)) {
			int b = regno(p->n_left);
			if (p->n_left->n_op == TEMP)
				b = b - suboff + MAXREGS;
XCHECK(b);
			BITCLEAR(bb->gen, b);
			BITSET(bb->killed, b);
			unionize(p->n_right, bb, suboff);
			return;
		}
	}
	ty = optype(o);
	if (ty != LTYPE)
		unionize(p->n_left, bb, suboff);
	if (ty == BITYPE)
		unionize(p->n_right, bb, suboff);
}

/*
 * Found an extended assembler node, so growel out gen/killed nodes.
 */
static void
xasmionize(NODE *p, void *arg)
{
	struct basicblock *bb = arg;
	int cw, b;

	if (p->n_op == ICON && p->n_type == STRTY)
		return; /* dummy end marker */

	cw = xasmcode(p->n_name);
	if (XASMVAL(cw) == 'n' || XASMVAL(cw) == 'm')
		return; /* no flow analysis */
	p = p->n_left;
 
	if (XASMVAL(cw) == 'g' && p->n_op != TEMP && p->n_op != REG)
		return; /* no flow analysis */

	b = regno(p);
#if 0
	if (XASMVAL(cw) == 'r' && p->n_op == TEMP)
		addnotspill(b);
#endif
#define MKTOFF(r)	((r) - xxx)
	if (XASMISOUT(cw)) {
		if (p->n_op == TEMP) {
			BITCLEAR(bb->gen, MKTOFF(b));
			BITSET(bb->killed, MKTOFF(b));
		} else if (p->n_op == REG) {
			BITCLEAR(bb->gen, b);
			BITSET(bb->killed, b);	 
		} else
			uerror("bad xasm node type %d", p->n_op);
	}
	if (XASMISINP(cw)) {
		if (p->n_op == TEMP) {
			BITSET(bb->gen, MKTOFF(b));
		} else if (p->n_op == REG) {
			BITSET(bb->gen, b);
		} else if (optype(p->n_op) != LTYPE) {
			if (XASMVAL(cw) == 'r')
				uerror("couldn't find available register");
			else
				uerror("bad xasm node type2");
		}
	}
}

/*
 * Do variable liveness analysis.  Only analyze the long-lived
 * variables, and save the live-on-exit temporaries in a bit-field
 * at the end of each basic block. This bit-field is later used
 * when doing short-range liveness analysis in Build().
 */
void
liveanal(struct p2env *p2e)
{
	struct basicblock *bb;
	struct interpass *ip;
	bittype *saved;
	int mintemp, again;

	xbits = p2e->epp->ip_tmpnum - p2e->ipp->ip_tmpnum + MAXREGS;
	mintemp = p2e->ipp->ip_tmpnum;

	/* Just fetch space for the temporaries from heap */
	DLIST_FOREACH(bb, &p2e->bblocks, bbelem) {
		BITALLOC(bb->gen,tmpalloc,xbits);
		BITALLOC(bb->killed,tmpalloc,xbits);
		BITALLOC(bb->in,tmpalloc,xbits);
		BITALLOC(bb->out,tmpalloc,xbits);
	}
	BITALLOC(saved,tmpalloc,xbits);

	xxx = mintemp;
	/*
	 * generate the gen-killed sets for all basic blocks.
	 */
	DLIST_FOREACH(bb, &p2e->bblocks, bbelem) {
		for (ip = bb->last; ; ip = DLIST_PREV(ip, qelem)) {
			/* gen/killed is 'p', this node is 'n' */
			if (ip->type == IP_NODE) {
				if (ip->ip_node->n_op == XASM)
					flist(ip->ip_node->n_left,
					    xasmionize, bb);
				else
					unionize(ip->ip_node, bb, mintemp);
			}
			if (ip == bb->first)
				break;
		}
		memcpy(bb->in, bb->gen, BIT2BYTE(xbits));
#ifdef PCC_DEBUG
#define PRTRG(x) printf("%d ", x < MAXREGS ? x : x + p2e->ipp->ip_tmpnum-MAXREGS)
		if (b2debug > 1) {
			int i;

			printf("basic block %d\ngen: ", bb->bbnum);
			for (i = 0; i < xbits; i++)
				if (TESTBIT(bb->gen, i))
					PRTRG(i);
			printf("\nkilled: ");
			for (i = 0; i < xbits; i++)
				if (TESTBIT(bb->killed, i))
					PRTRG(i);
			printf("\n");
		}
#endif
	}
	/* do liveness analysis on basic block level */
	do {
		struct cfgnode *cn;
		int j;

		again = 0;
		/* XXX - loop should be in reversed execution-order */
		DLIST_FOREACH_REVERSE(bb, &p2e->bblocks, bbelem) {
			SETCOPY(saved, bb->out, j, xbits);
			SLIST_FOREACH(cn, &bb->child, chld) {
				SETSET(bb->out, cn->bblock->in, j, xbits);
			}
			SETCMP(again, saved, bb->out, j, xbits);
			SETCOPY(saved, bb->in, j, xbits);
			SETCOPY(bb->in, bb->out, j, xbits);
			SETCLEAR(bb->in, bb->killed, j, xbits);
			SETSET(bb->in, bb->gen, j, xbits);
			SETCMP(again, saved, bb->in, j, xbits);
		}
	} while (again);

#ifdef PCC_DEBUG
	DLIST_FOREACH(bb, &p2e->bblocks, bbelem) {
		if (b2debug) {
			int i;

			printf("all basic block %d\nin: ", bb->bbnum);
			for (i = 0; i < xbits; i++)
				if (TESTBIT(bb->in, i))
					PRTRG(i);
			printf("\nout: ");
			for (i = 0; i < xbits; i++)
				if (TESTBIT(bb->out, i))
					PRTRG(i);
			printf("\n");
		}
	}
#endif
}
