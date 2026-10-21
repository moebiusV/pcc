/*	$Id$	*/
/*
 * Copyright(C) Caldera International Inc. 2001-2002. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code and documentation must retain the above
 * copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditionsand the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 * 	This product includes software developed or owned by Caldera
 *	International, Inc.
 * Neither the name of Caldera International, Inc. nor the names of other
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OFLIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

# include "pass1.h"

#define	NODE P1ND
#define	nfree p1nfree
#define	tfree p1tfree
#undef n_type
#define n_type ptype
#undef n_qual
#define n_qual pqual
#undef n_df
#define n_df pdf


# define SWAP(p,q) {sp=p; p=q; q=sp;}
# define RCON(p) (p->n_right->n_op==ICON)
# define RO(p) p->n_right->n_op
# define RV(p) glval(p->n_right)
# define LCON(p) (p->n_left->n_op==ICON)
# define LO(p) p->n_left->n_op
# define LV(p) glval(p->n_left)

/* remove left node */
static NODE *
zapleft(NODE *p)
{
	NODE *q;

	q = p->n_left;
	nfree(p->n_right);
	nfree(p);
	return q;
}

/*
 * Remove a shift by zero.  The shift may have been given a type other than
 * its operand's (clocal() retypes (int)(u << n) to an int shift of the
 * unsigned operand), so keep the conversion when the types differ.
 */
static NODE *
zapshift(NODE *p)
{
	NODE *q = p->n_left;
	TWORD t = p->n_type;
	union dimfun *df = p->n_df;
	struct ssdesc *ss = p->pss;

	nfree(p->n_right);
	nfree(p);
	if (q->n_type != t)
		q = block(SCONV, q, NIL, t, df, ss);
	return q;
}

/*
 * fortran function arguments
 */
static NODE *
fortarg(NODE *p)
{
	if( p->n_op == CM ){
		p->n_left = fortarg( p->n_left );
		p->n_right = fortarg( p->n_right );
		return(p);
	}

	while( ISPTR(p->n_type) ){
		p = buildtree( UMUL, p, NIL );
	}
	return( optim(p) );
}

	/* mapping relationals when the sides are reversed */
// short revrel[] ={ EQ, NE, GE, GT, LE, LT, UGE, UGT, ULE, ULT };

/*
 * The same value, as far as can be seen without running anything: the leaves are equal and the operators
 * are free of side effects.
 */
static int
sametree(P1ND *a, P1ND *b)
{
	if (a->n_op != b->n_op || a->n_type != b->n_type)
		return 0;
	switch (a->n_op) {
	case NAME:
		return a->n_sp == b->n_sp && glval(a) == glval(b) && !ISVOL(a->n_qual);
	case ICON:
		return a->n_sp == b->n_sp && glval(a) == glval(b);
	case TEMP:
	case REG:
		return regno(a) == regno(b);
	case UMUL: case SCONV: case PCONV: case COMPL: case UMINUS:
		return !ISVOL(a->n_qual) && sametree(a->n_left, b->n_left);
	case PLUS: case MINUS: case MUL: case AND: case OR: case ER: case LS: case RS:
		return sametree(a->n_left, b->n_left) && sametree(a->n_right, b->n_right);
	}
	return 0;
}

/* x with an AND of all ones of its width (mbedtls writes  (x & 0xFFFFFFFF) >> n ) taken off */
static P1ND *
unmask(P1ND *x, int w)
{
	P1ND *c;

	if (x->n_op == AND && (c = x->n_right)->n_op == ICON && c->n_sp == NULL &&
	    (w >= 64 ? glval(c) == -1 : (glval(c) & (((CONSZ)1 << w) - 1)) == (((CONSZ)1 << w) - 1)))
		return x->n_left;
	return x;
}

/*
 * One term of a byte swap of a 32-bit value x: the byte `i' of x moved to the byte `j', as (x & m) << k,
 * (x & m) >> k, (x << k) & m or (x >> k) & m, with k a whole number of bytes and m one byte.  Returns the
 * operand and the two byte numbers.
 */
static P1ND *
bsterm(P1ND *t, int *i, int *j)
{
	P1ND *s, *m;
	CONSZ mv, k;
	int b, sh;

	if ((t->n_op == LS || t->n_op == RS) && t->n_right->n_op == ICON && t->n_right->n_sp == NULL &&
	    t->n_left->n_op == AND && (m = t->n_left->n_right)->n_op == ICON && m->n_sp == NULL) {
		k = glval(t->n_right);
		mv = glval(m) & 0xffffffff;
		s = t->n_left->n_left;
		if (k % 8 != 0 || k <= 0 || k >= 32)
			return NULL;
		for (b = 0; b < 4; b++)
			if (mv == ((CONSZ)0xff << (8 * b)))
				break;
		if (b == 4)
			return NULL;
		sh = (int)(k / 8);
		*i = b;
		*j = t->n_op == LS ? b + sh : b - sh;
	} else if (t->n_op == AND && (m = t->n_right)->n_op == ICON && m->n_sp == NULL &&
	    (t->n_left->n_op == LS || t->n_left->n_op == RS) && t->n_left->n_right->n_op == ICON &&
	    t->n_left->n_right->n_sp == NULL) {
		k = glval(t->n_left->n_right);
		mv = glval(m) & 0xffffffff;
		s = t->n_left->n_left;
		if (k % 8 != 0 || k <= 0 || k >= 32)
			return NULL;
		for (b = 0; b < 4; b++)
			if (mv == ((CONSZ)0xff << (8 * b)))
				break;
		if (b == 4)
			return NULL;
		sh = (int)(k / 8);
		*j = b;
		*i = t->n_left->n_op == LS ? b - sh : b + sh;
	} else
		return NULL;
	if (*i < 0 || *i > 3 || *j < 0 || *j > 3)
		return NULL;
	return s;
}

/* the leaves of an or of at most four terms */
static int
bsleaves(P1ND *p, P1ND **lv, int n)
{
	if (p->n_op == OR) {
		n = bsleaves(p->n_left, lv, n);
		return n < 0 ? n : bsleaves(p->n_right, lv, n);
	}
	if (n >= 4)
		return -1;
	lv[n] = p;
	return n + 1;
}

/*
 * The four bytes of an unsigned 32-bit x written the other way round, from shifts and masks (-xdce):
 * (x & 0xff) << 24 | (x & 0xff00) << 8 | (x & 0xff0000) >> 8 | (x & 0xff000000) >> 24, mbedtls's way of
 * saying it, and the builtin's.  Targets with an instruction for it have it; the others get the shifts back.
 */
static P1ND *
bswap(P1ND *p)
{
	P1ND *lv[4], *x[4], *t;
	int i[4], j[4], k, seen = 0;

	if ((p->n_type != UNSIGNED && p->n_type != ULONG) || tsize(p->n_type, p->n_df, p->pss) != 32)
		return NULL;
	if (bsleaves(p, lv, 0) != 4)
		return NULL;
	for (k = 0; k < 4; k++) {
		if (lv[k]->n_type != p->n_type || (x[k] = bsterm(lv[k], &i[k], &j[k])) == NULL || j[k] != 3 - i[k])
			return NULL;
		if (seen & (1 << i[k]))
			return NULL;
		seen |= 1 << i[k];
		if (k > 0 && !sametree(x[0], x[k]))
			return NULL;
	}
	t = p1alloc();
	*t = *p;
	t->n_op = BSWAP;
	t->n_left = p1tcopy(x[0]);
	t->n_right = NULL;
	p1tfree(p);
	return t;
}

/*
 * (x << n) | (x >> (w - n)) for an unsigned x of w bits and a constant n is a rotate left by n (-xdce).  Many
 * targets have an instruction for it; the others get the two shifts back in pass 2.
 */
static P1ND *
rotate(P1ND *p)
{
	P1ND *l = p->n_left, *r = p->n_right, *a, *b, *x, *y, *t;
	TWORD ty = p->n_type;
	int w;
	CONSZ n, m;

	if (ty != UNSIGNED && ty != ULONG && ty != ULONGLONG)
		return NULL;
	w = (int)tsize(ty, p->n_df, p->pss);
	if (l->n_op == RS) {
		t = l; l = r; r = t;
	}
	a = l;		/* the left shift */
	b = r;		/* the right shift */
	if (a->n_op != LS || b->n_op != RS || a->n_right->n_op != ICON || b->n_right->n_op != ICON ||
	    a->n_right->n_sp != NULL || b->n_right->n_sp != NULL || a->n_type != ty || b->n_type != ty)
		return NULL;
	n = glval(a->n_right);
	m = glval(b->n_right);
	if (n <= 0 || m <= 0 || n + m != w)
		return NULL;
	x = a->n_left;
	y = unmask(b->n_left, w);
	if (!sametree(x, y))
		return NULL;
	/* the value is x (copied out of the left shift), the nodes of the other side are freed */
	a->n_left = NULL;
	t = p1alloc();
	*t = *p;
	t->n_op = ROT;
	t->n_left = x;
	t->n_right = a->n_right;
	a->n_right = NULL;
	p1tfree(b);
	p1nfree(a);
	p1nfree(p);
	return t;
}


/*
 * local optimizations, most of which are probably
 * machine independent
 */
NODE *
optim(NODE *p)
{
	int o, ty;
	NODE *sp, *q;
	OFFSZ sz;
	int i;

	if (odebug) return(p);
	if (p == NULL)
		return p;	/* an operand lost to an earlier error */

	ty = coptype(p->n_op);
	if( ty == LTYPE ) return(p);

	if( ty == BITYPE ) p->n_right = optim(p->n_right);
	p->n_left = optim(p->n_left);

	/* collect constants */
again:	o = p->n_op;
	switch(o){

	case SCONV:
		if (concast(p->n_left, p->n_type)) {
			q = p->n_left;
			nfree(p);
			p = q;
			break;
		}
		/* FALLTHROUGH */
	case PCONV:
		if (p->n_type != VOID)
			p = clocal(p);
		break;

	case FORTCALL:
		p->n_right = fortarg( p->n_right );
		break;

	case ADDROF:
		if (LO(p) == TEMP)
			break;
		if( LO(p) != NAME ) cerror( "& error" );

		if( !andable(p->n_left) && !statinit)
			break;

		LO(p) = ICON;

		setuleft:
		/* paint over the type of the left hand side with the type of the top */
		p->n_left->n_type = p->n_type;
		p->n_left->n_df = p->n_df;
		p->n_left->n_ap = p->n_ap;
		q = p->n_left;
		nfree(p);
		p = q;
		break;

	case NOT:
	case UMINUS:
	case COMPL:
		if (LCON(p) && conval(p->n_left, o, p->n_left))
			p = nfree(p);
		break;

	case UMUL:
		/* Do not discard ADDROF TEMP's */
		if (LO(p) == ADDROF && LO(p->n_left) != TEMP) {
			q = p->n_left->n_left;
			nfree(p->n_left);
			nfree(p);
			p = q;
			break;
		}
		if( LO(p) != ICON ) break;
		LO(p) = NAME;
		goto setuleft;

	case RS:
		if (LCON(p) && RCON(p) && conval(p->n_left, o, p->n_right))
			goto zapright;

		sz = tsize(p->n_type, p->n_df, p->pss);

		if (LO(p) == RS && RCON(p->n_left) && RCON(p) &&
		    (RV(p) + RV(p->n_left)) < sz) {
			/* two right-shift  by constants */
			RV(p) += RV(p->n_left);
			p->n_left = zapleft(p->n_left);
		}
#if 0
		  else if (LO(p) == LS && RCON(p->n_left) && RCON(p)) {
			RV(p) -= RV(p->n_left);
			if (RV(p) < 0)
				o = p->n_op = LS, RV(p) = -RV(p);
			p->n_left = zapleft(p->n_left);
		}
#endif
		if (RO(p) == ICON) {
			if (RV(p) < 0) {
				RV(p) = -RV(p);
				p->n_op = LS;
				goto again;
			}
#ifdef notyet /* must check for side effects, --a >> 32; */
			if (RV(p) >= tsize(p->n_type, p->n_df, p->n_sue) &&
			    ISUNSIGNED(p->n_type)) { /* ignore signed shifts */
				/* too many shifts */
				tfree(p->n_left);
				nfree(p->n_right);
				p->n_op = ICON; p->n_lval = 0; p->n_sp = NULL;
			} else
#endif
			/* avoid larger shifts than type size */
			if (RV(p) >= sz)
				werror("shift larger than type");
			if (RV(p) == 0)
				p = zapshift(p);
		}
		break;

	case LS:
		if (LCON(p) && RCON(p) && conval(p->n_left, o, p->n_right))
			goto zapright;

		sz = tsize(p->n_type, p->n_df, p->pss);

		if (LO(p) == LS && RCON(p->n_left) && RCON(p) &&
		    (RV(p) + RV(p->n_left)) < sz) {
			/* two left-shift  by constants (not past the type: that is 0, not a count mod the width) */
			RV(p) += RV(p->n_left);
			p->n_left = zapleft(p->n_left);
		}
#if 0
		  else if (LO(p) == RS && RCON(p->n_left) && RCON(p)) {
			RV(p) -= RV(p->n_left);
			p->n_left = zapleft(p->n_left);
		}
#endif
		if (RO(p) == ICON) {
			if (RV(p) < 0) {
				RV(p) = -RV(p);
				p->n_op = RS;
				goto again;
			}
#ifdef notyet /* must check for side effects */
			if (RV(p) >= tsize(p->n_type, p->n_df, p->n_sue)) {
				/* too many shifts */
				tfree(p->n_left);
				nfree(p->n_right);
				p->n_op = ICON; p->n_lval = 0; p->n_sp = NULL;
			} else
#endif
			/* avoid larger shifts than type size */
			if (RV(p) >= sz)
				werror("shift larger than type");
			if (RV(p) == 0)
				p = zapshift(p);
		}
		break;

	case QUEST:
		if (!LCON(p))
			break;
		if (LV(p) == 0) {
			q = p->n_right->n_right;
		} else {
			q = p->n_right->n_left;
			p->n_right->n_left = p->n_right->n_right;
		}
		p->n_right->n_op = UMUL; /* for tfree() */
		p1walkf(p, putjops, 0);
		tfree(p);
		p = q;
		break;

	case MINUS:
		if (LCON(p) && RCON(p) && p->n_left->n_sp == p->n_right->n_sp) {
			/* link-time constants, but both are the same */
			/* solve it now by forgetting the symbols */
			p->n_left->n_sp = p->n_right->n_sp = NULL;
		}
		if( !nncon(p->n_right) ) break;
		RV(p) = -RV(p);
		o = p->n_op = PLUS;
		/* FALLTHROUGH */
	case MUL:
		/*
		 * Check for u=(x-y)+z; where all vars are pointers to
		 * the same struct. This has two advantages:
		 * 1: avoid a mul+div
		 * 2: even if not allowed, people may get surprised if this
		 *    calculation do not give correct result if using
		 *    unaligned structs.
		 */
		if (o == MUL && p->n_type == INTPTR && RCON(p) &&
		    LO(p) == DIV && RCON(p->n_left) &&
		    RV(p) == RV(p->n_left) &&
		    LO(p->n_left) == MINUS) {
			q = p->n_left->n_left;
			if (q->n_left->n_type == PTR+STRTY &&
			    q->n_right->n_type == PTR+STRTY &&
			    suemeq(q->n_left->n_td->ss, q->n_right->n_td->ss)) {
				p = zapleft(p);
				p = zapleft(p);
				break;
			}
		}
		/* FALLTHROUGH */
	case PLUS:
	case AND:
	case OR:
	case ER:
		if (o == OR && xdce && (q = rotate(p)) != NULL) {
			p = q;
			goto again;
		}
		if (o == OR && xdce && (q = bswap(p)) != NULL) {
			p = q;
			goto again;
		}
		/* commutative ops; for now, just collect constants */
		/* someday, do it right */
		if( nncon(p->n_left) || ( LCON(p) && !RCON(p) ) )
			SWAP( p->n_left, p->n_right );
		/* make ops tower to the left, not the right */
		if( RO(p) == o ){
			SWAP(p->n_left, p->n_right);
#ifdef notdef
		/* Yetch, this breaks type correctness in trees */
		/* Code was probably written before types */
		/* All we can do here is swap and pray */
			NODE *t1, *t2, *t3;
			t1 = p->n_left;
			sp = p->n_right;
			t2 = sp->n_left;
			t3 = sp->n_right;
			/* now, put together again */
			p->n_left = sp;
			sp->n_left = t1;
			sp->n_right = t2;
			sp->n_type = p->n_type;
			p->n_right = t3;
#endif
			}
		if(o == PLUS && LO(p) == MINUS && RCON(p) && RCON(p->n_left) &&
		   conval(p->n_right, MINUS, p->n_left->n_right)){
			zapleft:

			q = p->n_left->n_left;
			nfree(p->n_left->n_right);
			nfree(p->n_left);
			p->n_left = q;
		}
		if( RCON(p) && LO(p)==o && RCON(p->n_left) &&
		    conval( p->n_right, o, p->n_left->n_right ) ){
			goto zapleft;
			}
		else if( LCON(p) && RCON(p) && conval( p->n_left, o, p->n_right ) ){
			zapright:
			nfree(p->n_right);
			q = makety(p->n_left, p->n_td);
			nfree(p);
			p = clocal(q);
			break;
			}

		/* change muls to shifts */

		if( o == MUL && nncon(p->n_right) && (i=ispow2(RV(p)))>=0){
			if( i == 0 ) { /* multiplication by 1 */
				goto zapright;
				}
			o = p->n_op = LS;
			p->n_right->n_type = INT;
			p->n_right->n_df = NULL;
			RV(p) = i;
			}

		/* change +'s of negative consts back to - */
		if( o==PLUS && nncon(p->n_right) && RV(p)<0 ){
			RV(p) = -RV(p);
			o = p->n_op = MINUS;
			}

		/* remove ops with RHS 0 */
		if ((o == PLUS || o == MINUS || o == OR || o == ER) &&
		    nncon(p->n_right) && RV(p) == 0 && !OPTIM_KEEPZERO(p)) {
			goto zapright;
		}
		break;

	case DIV:
		if( nncon( p->n_right ) && glval(p->n_right) == 1 )
			goto zapright;
		if (LCON(p) && RCON(p) && conval(p->n_left, DIV, p->n_right))
			goto zapright;
		if (RCON(p) && ISUNSIGNED(p->n_type) && (i=ispow2(RV(p))) > 0) {
			p->n_op = RS;
			RV(p) = i;
			q = p->n_right;
			if(tsize(q->n_type, q->n_df, q->pss) > SZINT)
				p->n_right = makety(q, tdint);

			break;
		}
		break;

	case MOD:
		if (RCON(p) && ISUNSIGNED(p->n_type) && ispow2(RV(p)) > 0) {
			p->n_op = AND;
			RV(p) = RV(p) -1;
			break;
		}
		break;

	case EQ:
	case NE:
	case LT:
//	case LE:
//	case GT:
	case GE:
	case ULT:
//	case ULE:
//	case UGT:
	case UGE:
		if (LCON(p) && RCON(p) &&
		    !ISPTR(p->n_left->n_type) && !ISPTR(p->n_right->n_type)) {
			/* Do constant evaluation */
			q = p->n_left;
			if (conval(q, o, p->n_right)) {
				nfree(p->n_right);
				nfree(p);
				p = q;
				break;
			}
		}

#if 0
		if( !LCON(p) ) break;

		/* exchange operands */

		sp = p->n_left;
		p->n_left = p->n_right;
		p->n_right = sp;
		p->n_op = revrel[p->n_op - EQ ];
#endif
		break;

	case CBRANCH:
		if (LCON(p)) {
			if (LV(p) == 0) {
				tfree(p);
				p = bcon(0);
			} else {
				tfree(p->n_left);
				p->n_left = p->n_right;
				p->n_op = GOTO;
			}
		}
		break;
				

#ifdef notyet
	case ASSIGN:
		/* Simple test to avoid two branches */
		if (RO(p) != NE)
			break;
		q = p->n_right;
		if (RCON(q) && RV(q) == 0 && LO(q) == AND &&
		    RCON(q->n_left) && (i = ispow2(RV(q->n_left))) &&
		    q->n_left->n_type == INT) {
			q->n_op = RS;
			RV(q) = i;
		}
		break;
#endif

	case ANDAND:
		if (!nncon(p->n_left))
			break;
		if (LV(p) == 0) { /* right not evaluated */
			p1walkf(p, putjops, 0);
			p1tfree(p);
			p = bcon(0);
#ifdef notyet /* result may be logical value */
		} else {
			q = p->n_right;
			nfree(nfree(p));
			p = cast(q, INT, 0);
#endif
		}
		break;
	case OROR:
		if (!nncon(p->n_left))
			break;
		if (LV(p) != 0) { /* right not evaluated */
			p1walkf(p, putjops, 0);
			p1tfree(p);
			p = bcon(1);
#ifdef notyet /* result may be logical value */
		} else {
			q = p->n_right;
			nfree(nfree(p));
			p = cast(q, INT, 0);
#endif
		}
		break;
	}

	return(p);
	}

int
ispow2(CONSZ c)
{
	int i;
	if( c <= 0 || (c&(c-1)) ) return(-1);
	for( i=0; c>1; ++i) c >>= 1;
	return(i);
}

int
nncon(NODE *p)
{
	/* is p a constant without a name */
	return( p->n_op == ICON && p->n_sp == NULL );
}
