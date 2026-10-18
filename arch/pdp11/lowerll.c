/*
 * long long on the PDP-11.
 *
 * The machine has no 64-bit operations and no registers to hold one, so a long
 * long is an eight-byte object in memory (four words, the most significant
 * first, as a long is two words with the high one first) and every operation on
 * it is a call to a helper that takes addresses:
 *
 *	a + b	is	(_lladd(&t, &a, &b), &t)	t a temporary on the stack
 *
 * Pass 1 has typed and converted the tree as for any integer type; ecomp() then
 * calls lowerll(), which rewrites every node whose value is a long long into an
 * expression whose value is the *address* of the result, and every node that
 * consumes one (a comparison, a conversion to int, a call argument, ...) into a
 * call.  Pass 2 never sees a long long computation, only pointers, ints and
 * calls.  The helpers (arch/pdp11/ll.c, compiled once to llhelpers.h) are
 * appended to each file that uses them, as for unsigned divide.
 *
 * Passing: a long long argument is its four words, pushed so that the memory
 * image on the stack is the object.  Returning: the callee copies the value to
 * a static buffer, _llret, and returns its address in r0; the caller copies it
 * out at once.  Floating-point conversions are not supported.
 */
#ifdef LOWERLL
#include <stdlib.h>
#include <stdio.h>

int ll_used;			/* bit per helper, see llfns[] */

struct llfn {
	char *name;
	TWORD rt;
};

#define	LL_CPY	0
#define	LL_ADD	1
#define	LL_SUB	2
#define	LL_MUL	3
#define	LL_DVS	4
#define	LL_DVU	5
#define	LL_MDS	6
#define	LL_MDU	7
#define	LL_AND	8
#define	LL_OR	9
#define	LL_XOR	10
#define	LL_SHL	11
#define	LL_SHR	12
#define	LL_USH	13
#define	LL_NEG	14
#define	LL_COM	15
#define	LL_CMP	16
#define	LL_UCM	17
#define	LL_FLS	18
#define	LL_FLU	19
#define	LL_TOL	20
#define	LL_M32	21
#define	LL_UDM	22
#define	LL_ULR	23

static struct llfn llfns[] = {
	{ "_llcpy", INT }, { "_lladd", INT }, { "_llsub", INT }, { "_llmul", INT },
	{ "_lldvs", INT }, { "_lldvu", INT }, { "_llmds", INT }, { "_llmdu", INT },
	{ "_lland", INT }, { "_llor", INT }, { "_llxor", INT }, { "_llshl", INT },
	{ "_llshr", INT }, { "_llush", INT }, { "_llneg", INT }, { "_llcom", INT },
	{ "_llcmp", INT }, { "_llucm", INT }, { "_llfls", INT }, { "_llflu", INT },
	{ "_lltol", LONG }, { "_llm32", INT }, { "_lludm", INT }, { "_llulr", ULONG },
};

/* constants and the return buffer, written out by ejobcode() */
struct llconst { CONSZ v; };
struct llconst llpool[512];
int nllpool;
int ll_retbuf;			/* _llret is referenced */

static int
isll(TWORD t)
{
	return t == LONGLONG || t == ULONGLONG;
}

/* a function symbol that is not declared in the program */
static P1ND *
llsym(char *name, TWORD type)
{
	P1ND *f = block(NAME, NULL, NULL, INT, 0, 0);
	int oblvl = blevel;

	blevel = 0;
	f->n_sp = lookup(addname(name), SNORMAL);
	blevel = oblvl;
	if (f->n_sp->sclass == SNULL) {
		f->n_sp->sclass = EXTERN;
		f->n_sp->stype = type;
		if (ISFTN(type)) {
			f->n_sp->sdf = permalloc(sizeof(union dimfun));
			f->n_sp->sdf->dlst = 0;
		}
	}
	f->n_type = f->n_sp->stype;
	return clocal(f);
}

static P1ND *
llcall(int which, P1ND *a, P1ND *b, P1ND *c, P1ND *d)
{
	P1ND *args, *f;

	ll_used |= 1 << which;
	f = llsym(llfns[which].name, INCREF(llfns[which].rt) + (FTN-PTR));
	args = a;
	if (b)
		args = buildtree(CM, args, b);
	if (c)
		args = buildtree(CM, args, c);
	if (d)
		args = buildtree(CM, args, d);
	return buildtree(CALL, f, args);
}

/* sequence: do a, then the value of b */
static P1ND *
llseq(P1ND *a, P1ND *b)
{
	return block(COMOP, a, b, b->n_type, b->n_df, b->pss);
}

/* a stack object for a result */
static P1ND *
llstk(TWORD t)
{
	struct symtab *sp;

	sp = getsymtab("0hej", SSTMT);
	sp->stype = t;
	sp->squal = 0;
	sp->sdf = NULL;
	sp->sss = NULL;
	sp->sclass = AUTO;
	sp->soffset = NOOFFSET;
	oalloc(sp, &autooff);
	return nametree(sp);
}

static P1ND *lladdr(P1ND *lv);

/* two separate addresses of a fresh stack object (the object node itself is used up) */
static void
llpair(TWORD t, P1ND **a, P1ND **b)
{
	P1ND *tmp = llstk(t);

	*a = lladdr(p1tcopy(tmp));
	*b = lladdr(p1tcopy(tmp));
	p1tfree(tmp);
}

static P1ND *
lladdr(P1ND *lv)
{
	return buildtree(ADDROF, lv, NULL);
}

/* the address of a 64-bit constant in the data the file ends with */
static P1ND *
llconst(CONSZ v, TWORD t)
{
	char name[16];
	int i;
	P1ND *n;
	struct symtab *sp;

	for (i = 0; i < nllpool; i++)
		if (llpool[i].v == v)
			break;
	if (i == nllpool) {
		if (nllpool >= (int)(sizeof llpool / sizeof llpool[0]))
			cerror("too many long long constants");
		llpool[nllpool++].v = v;
	}
	snprintf(name, sizeof name, "_Lc%d", i);
	n = block(NAME, NULL, NULL, INT, 0, 0);
	n->n_sp = sp = lookup(addname(name), SNORMAL);
	if (sp->sclass == SNULL) {
		sp->sclass = EXTERN;
		sp->stype = t;
		sp->sdf = NULL;
		sp->sss = NULL;
	}
	n->n_type = sp->stype;
	return lladdr(clocal(n));
}

static P1ND *
llbuf(void)
{
	P1ND *n;
	struct symtab *sp;

	ll_retbuf = 1;
	n = block(NAME, NULL, NULL, INT, 0, 0);
	n->n_sp = sp = lookup(addname("_llret"), SNORMAL);
	if (sp->sclass == SNULL) {
		sp->sclass = EXTERN;
		sp->stype = LONGLONG;
		sp->sdf = NULL;
		sp->sss = NULL;
	}
	n->n_type = sp->stype;
	return lladdr(clocal(n));
}

static P1ND *
llptrtemp(TWORD pt, int *nr)
{
	P1ND *t = tempnode(0, pt, NULL, NULL);

	*nr = regno(t);
	return t;
}

/* a value of an integer type, converted to long or unsigned long, for the fl* helpers */
static P1ND *
llfrom(P1ND *s, TWORD st)
{
	int uns;
	TWORD lt;
	P1ND *v;

	uns = st == UNSIGNED || st == USHORT || st == UCHAR || st == ULONG || ISPTR(st) || st == BOOL;
	lt = uns ? ULONG : LONG;
	if (st == FLOAT || st == DOUBLE || st == LDOUBLE) {
		uerror("conversion between long long and floating point is not supported");
		return bcon(0);
	}
	if (st == lt)
		v = s;
	else
		v = block(SCONV, s, NULL, lt, NULL, NULL);
	{
		P1ND *a, *b;

		llpair(LONGLONG, &a, &b);
		return llseq(llcall(uns ? LL_FLU : LL_FLS, a, v, NULL, NULL), b);
	}
}

static P1ND *lw(P1ND *p, int *llp);

/* a value that is long long, as an address; or 0 if the node is not one */
static P1ND *
lladdrof(P1ND *p)
{
	int isl;
	P1ND *x = lw(p, &isl);

	if (!isl)
		cerror("lladdrof");
	return x;
}

/* the value of a long long expression, tested against zero */
static P1ND *
llnonzero(P1ND *a, TWORD t, int eq)
{
	P1ND *c;

	c = llcall(isll(t) && t == ULONGLONG ? LL_UCM : LL_CMP, a, llconst(0, t), NULL, NULL);
	return buildtree(eq ? EQ : NE, c, bcon(0));
}

/* a call that returns an address of a long long: copy the result at once */
static P1ND *
llretcopy(P1ND *call)
{
	P1ND *a, *b;

	llpair(LONGLONG, &a, &b);
	return llseq(llcall(LL_CPY, a, call, NULL, NULL), b);
}

/* the four words of the object at address a, as call arguments (the first lowest); *pre copies it first */
static void
llwords(P1ND *a, P1ND **pre, P1ND *w[4])
{
	P1ND *t, *x;
	int i;

	t = llstk(LONGLONG);
	*pre = llcall(LL_CPY, lladdr(p1tcopy(t)), a, NULL, NULL);
	for (i = 0; i < 4; i++) {
		x = block(PLUS, lladdr(p1tcopy(t)), bcon(2 * i), PTR+UNSIGNED, NULL, NULL);
		x = block(UMUL, x, NULL, UNSIGNED, NULL, NULL);
		w[i] = block(FUNARG, x, NULL, x->n_type, x->n_df, x->pss);
	}
	p1tfree(t);
}

/*
 * A call: a long long argument is four words, read from a copy made before the call;
 * a long long result is copied out of the return buffer at once.
 */
static P1ND *
llcallnode(P1ND *p, int isl)
{
	P1ND *args[256], *stack[256], *a, *x, *pre[32], *w[4];
	int na = 0, ns = 0, i, npre = 0, la;

	p->n_left = lw(p->n_left, &la);
	if (p->n_op == CALL) {
		for (a = p->n_right; a->n_op == CM; a = p1nfree(a))
			stack[ns++] = a->n_right;
		stack[ns++] = a;
		while (ns > 0) {
			a = stack[--ns];
			if (a->n_op == FUNARG && isll(a->n_left->n_type)) {
				if (npre >= 32 || na > 250)
					cerror("too many long long arguments");
				x = lw(a->n_left, &la);
				llwords(x, &pre[npre++], w);
				for (i = 0; i < 4; i++)
					args[na++] = w[i];
				p1nfree(a);
			} else if (a->n_op == FUNARG) {
				a->n_left = lw(a->n_left, &la);
				args[na++] = a;
			} else
				args[na++] = lw(a, &la);
		}
		x = args[0];
		for (i = 1; i < na; i++)
			x = block(CM, x, args[i], INT, NULL, NULL);
		p->n_right = x;
	}
	if (isl) {
		p->n_type = PTR+LONGLONG;
		p->n_df = NULL;
		p->pss = NULL;
	}
	x = p;
	for (i = npre - 1; i >= 0; i--)
		x = llseq(pre[i], x);
	if (isl)
		x = llretcopy(x);
	return x;
}

static P1ND *
lw(P1ND *p, int *llp)
{
	TWORD t = p->n_type;
	int isl = isll(t), la = 0, ra = 0;
	P1ND *l = NULL, *r = NULL, *x;
	TWORD lt = 0, rt = 0;
	int op = p->n_op, which;

	*llp = isl;
	switch (op) {
	case ICON:
		if (isl && p->n_sp == NULL) {
			x = llconst(glval(p), t);
			p1nfree(p);
			return x;
		}
		return p;
	case NAME:
		if (isl)
			return lladdr(p);
		return p;
	}
	if (coptype(op) == LTYPE)
		return p;

	if (op == CALL || op == UCALL)
		return llcallnode(p, isl);

	/* children first */
	if (coptype(op) == UTYPE) {
		lt = p->n_left->n_type;
		l = p->n_left = lw(p->n_left, &la);
	} else {
		lt = p->n_left->n_type;
		l = p->n_left = lw(p->n_left, &la);
		rt = p->n_right->n_type;
		r = p->n_right = lw(p->n_right, &ra);
	}

	switch (op) {
	case UMUL:
		if (isl) {		/* *(address of long long) as an lvalue is the address */
			p1nfree(p);
			return l;
		}
		return p;
	case ADDROF:
		if (la) {		/* &long_long_lvalue */
			p1nfree(p);
			return l;
		}
		return p;
	case SCONV:
		if (isl && la) {	/* long long to long long: the same bits */
			p1nfree(p);
			return l;
		}
		if (isl) {
			x = llfrom(l, lt);
			p1nfree(p);
			return x;
		}
		if (la) {
			if (t == FLOAT || t == DOUBLE || t == LDOUBLE) {
				uerror("conversion between long long and floating point is not supported");
				return bcon(0);
			}
			x = llcall(LL_TOL, l, NULL, NULL, NULL);
			if (t != LONG && t != ULONG) {
				x = block(SCONV, x, NULL, t, p->n_df, p->pss);
			} else
				x->n_type = t;
			p1nfree(p);
			return x;
		}
		return p;
	case UMINUS: case COMPL:
		if (!isl)
			return p;
		{
			P1ND *a, *b;

			llpair(t, &a, &b);
			x = llseq(llcall(op == UMINUS ? LL_NEG : LL_COM, a, l, NULL, NULL), b);
		}
		p1nfree(p);
		return x;
	case NOT:
		if (la) {
			x = llnonzero(l, lt, 1);
			p1nfree(p);
			return x;
		}
		return p;
	case PLUS: case MINUS: case MUL: case DIV: case MOD:
	case AND: case OR: case ER:
		if (!isl)
			return p;
		switch (op) {
		case PLUS: which = LL_ADD; break;
		case MINUS: which = LL_SUB; break;
		case MUL: which = LL_MUL; break;
		case DIV: which = t == ULONGLONG ? LL_DVU : LL_DVS; break;
		case MOD: which = t == ULONGLONG ? LL_MDU : LL_MDS; break;
		case AND: which = LL_AND; break;
		case OR: which = LL_OR; break;
		default: which = LL_XOR; break;
		}
		{
			P1ND *a, *b;

			llpair(t, &a, &b);
			x = llseq(llcall(which, a, l, r, NULL), b);
		}
		p1nfree(p);
		return x;
	case LS: case RS:
		if (!isl)
			return p;
		which = op == LS ? LL_SHL : t == ULONGLONG ? LL_USH : LL_SHR;
		{
			P1ND *a, *b;

			llpair(t, &a, &b);
			x = llseq(llcall(which, a, l, r, NULL), b);
		}
		p1nfree(p);
		return x;
	case EQ: case NE: case LT: case LE: case GT: case GE:
	case ULT: case ULE: case UGT: case UGE:
		if (!la && !ra)
			return p;
		x = llcall((op >= ULE || lt == ULONGLONG || rt == ULONGLONG) ? LL_UCM : LL_CMP, l, r, NULL, NULL);
		/* the comparison helpers return -1, 0 or 1: a signed test of that against zero */
		x = buildtree(op == ULT ? LT : op == ULE ? LE : op == UGT ? GT : op == UGE ? GE : op, x, bcon(0));
		p1nfree(p);
		return x;
	case ANDAND: case OROR:
		if (la)
			p->n_left = llnonzero(l, lt, 0);
		if (ra)
			p->n_right = llnonzero(r, rt, 0);
		return p;
	case CBRANCH:
		if (la)
			p->n_left = llnonzero(l, lt, 0);
		return p;
	case ASSIGN:
		if (!isl)
			return p;
		{
			int n;
			P1ND *tp = llptrtemp(PTR+LONGLONG, &n);

			x = llseq(llseq(buildtree(ASSIGN, tp, l),
			    llcall(LL_CPY, tempnode(n, PTR+LONGLONG, NULL, NULL), r, NULL, NULL)),
			    tempnode(n, PTR+LONGLONG, NULL, NULL));
			p1nfree(p);
			return x;
		}
	case PLUSEQ: case MINUSEQ: case MULEQ: case DIVEQ: case MODEQ:
	case ANDEQ: case OREQ: case EREQ: case LSEQ: case RSEQ:
		if (!isl)
			return p;
		switch (op) {
		case PLUSEQ: which = LL_ADD; break;
		case MINUSEQ: which = LL_SUB; break;
		case MULEQ: which = LL_MUL; break;
		case DIVEQ: which = t == ULONGLONG ? LL_DVU : LL_DVS; break;
		case MODEQ: which = t == ULONGLONG ? LL_MDU : LL_MDS; break;
		case ANDEQ: which = LL_AND; break;
		case OREQ: which = LL_OR; break;
		case EREQ: which = LL_XOR; break;
		case LSEQ: which = LL_SHL; break;
		default: which = t == ULONGLONG ? LL_USH : LL_SHR; break;
		}
		{
			int n;
			P1ND *tp = llptrtemp(PTR+LONGLONG, &n);

			x = llseq(llseq(buildtree(ASSIGN, tp, l),
			    llcall(which, tempnode(n, PTR+LONGLONG, NULL, NULL),
			    tempnode(n, PTR+LONGLONG, NULL, NULL), r, NULL)),
			    tempnode(n, PTR+LONGLONG, NULL, NULL));
			p1nfree(p);
			return x;
		}
	case INCR: case DECR:
		if (!isl)
			return p;
		{
			int n;
			P1ND *tp = llptrtemp(PTR+LONGLONG, &n), *oa, *ob;

			if (!ra)
				r = llfrom(r, rt);
			llpair(t, &oa, &ob);

			/* the old value is the result; r is the amount, a long long constant */
			x = llseq(llseq(llseq(buildtree(ASSIGN, tp, l),
			    llcall(LL_CPY, oa, tempnode(n, PTR+LONGLONG, NULL, NULL), NULL, NULL)),
			    llcall(op == INCR ? LL_ADD : LL_SUB, tempnode(n, PTR+LONGLONG, NULL, NULL),
			    tempnode(n, PTR+LONGLONG, NULL, NULL), r, NULL)),
			    ob);
			p1nfree(p);
			return x;
		}
	case COMOP:
		if (isl) {
			p->n_type = r->n_type;
			p->n_df = r->n_df;
			p->pss = r->pss;
		}
		return p;
	case QUEST:
		if (la)			/* the condition is a long long: test it against zero */
			p->n_left = llnonzero(l, lt, 0);
		if (isl) {
			p->n_type = PTR+LONGLONG;
			p->n_df = NULL;
			p->pss = NULL;
		}
		return p;
	case COLON:
		if (isl) {
			p->n_type = PTR+LONGLONG;
			p->n_df = NULL;
			p->pss = NULL;
		}
		return p;
	case FUNARG:
		return p;
	}
	if (isl)
		cerror("long long operation %d is not supported", op);
	return p;
}

/* return e (a long long expression): copy it to _llret and give its address */
P1ND *
llreturn(P1ND *q)
{
	int isl;
	P1ND *a = lw(q, &isl);

	return llseq(llcall(LL_CPY, llbuf(), a, NULL, NULL), llbuf());
}

static void
lldump(P1ND *p, int d)
{
	int i;

	for (i = 0; i < d; i++)
		fprintf(stderr, "  ");
	fprintf(stderr, "op %d type %d\n", p->n_op, (int)p->n_type);
	if (coptype(p->n_op) != LTYPE)
		lldump(p->n_left, d+1);
	if (coptype(p->n_op) == BITYPE)
		lldump(p->n_right, d+1);
}

/* the data the lowered code refers to: constants, and the buffer a long long is returned in */
void
putllconsts(void)
{
	int i, j;

	if (nllpool) {
		/* in the text: c2 drops a directive that follows a jmp, and the constants never change */
		for (i = 0; i < nllpool; i++) {
			printf("__Lc%d:\n", i);
			for (j = 0; j < 4; j++)
				printf("%lo\n", (long)((llpool[i].v >> (48 - 16 * j)) & 0xffff));
		}
	}
	if (ll_retbuf)
		printf(".comm __llret,10\n");
}

/* x >> n for an unsigned long, a call (the PDP-11 shift instructions are arithmetic) */
P1ND *
llulshr(P1ND *x, P1ND *n)
{
	return llcall(LL_ULR, x, n, NULL, NULL);
}

/* the entry point: ecomp() calls this with each expression tree */
P1ND *
lowerll(P1ND *p)
{
	int isl;

	p = lw(p, &isl);
	if (getenv("LLDBG")) { fprintf(stderr, "--lowered\n"); lldump(p, 0); }
	return p;
}

#endif /* LOWERLL */
