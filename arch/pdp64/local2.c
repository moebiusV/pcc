/*	$Id$	*/
/*
 *	pdp64: pass 2 machine-dependent code.
 *
 * The frame (all of it below the saved frame pointer):
 *	16+(fp)		the first stack argument (a variadic function: 64+(fp), after its register save area)
 *	8(fp)		the return address
 *	0(fp)		the caller's fp
 *	-8..-8n(fp)	the n callee-saved registers, pushed by pushm
 *	below		the automatics and temporaries, then the outgoing argument area at 0(sp)
 * A function with no automatics, no saved registers and no outgoing arguments has no frame at all, and
 * one with only outgoing arguments just moves sp: its stack parameters are then reached from sp.
 */
# include "pass2.h"
# include <ctype.h>
# include <string.h>
# include "pdp64-names.h"
# include "pdp64-limits.h"

extern int pdp64_varfunc;

#define	MFRAME	0	/* push fp ... */
#define	MSP	1	/* only sp is moved */
#define	MNONE	2	/* nothing */

static int isnowrap(NODE *);
static int mode, nsaved, savemask, autosz, outsz;
static int wrapint;		/* -mwrap: re-extend signed int results too */
static int nowrap;		/* -mnowrap: signed int arithmetic does not overflow (so needs no re-extension) */
static int stkpos;		/* start of the bounce space for struct results, from fp */
static int bouncesz;		/* its size: the largest struct a call here returns */
static int fpcmp;		/* the last compare was floating point */
static int fpinv;		/* ... and its condition was inverted by pass 1 */

void
deflab(int label)
{
	printf(LABFMT ":\n", label);
}

static int
memsz(TWORD t)
{
	if (t > BTMASK)
		return 8;
	switch (t) {
	case CHAR: case UCHAR: case BOOL:
		return 1;
	case SHORT: case USHORT:
		return 2;
	case INT: case UNSIGNED: case FLOAT:
		return 4;
	}
	return 8;
}

/* a 32-bit (or narrower) integer, kept sign-extended; its constants too */
static CONSZ
conval(NODE *p)
{
	CONSZ v = getlval(p);

	if (p->n_name[0] == '\0') {
		switch (p->n_type) {
		case INT: case UNSIGNED: case FLOAT:
			return (int)v;
		case SHORT:
			return (short)v;
		case USHORT:
			return (unsigned short)v;
		case CHAR:
			return (signed char)v;
		case UCHAR: case BOOL:
			return (unsigned char)v;
		}
	}
	return v;
}

/*
 * The base register and displacement a frame-pointer-relative operand is printed with.
 */
static char *
basereg(int base)
{
	if (base == FPREG && mode != MFRAME)
		return rnames[STKREG];
	return rnames[base];
}

static long
fpoff(int base, long off)
{
	if (base != FPREG)
		return off;
	if (mode == MFRAME)
		return off < 0 ? off - 8 * nsaved : off;
	/*
	 * No frame pointer: the frame pointer's place is the return address less 8, the stack pointer is the
	 * saved registers and the locals and the outgoing area lower.  A local (below the saved registers)
	 * is autosz+outsz+off above sp; a parameter is also 8 per saved register above that, less the 8 of the saved fp.
	 */
	return off < 0 ? autosz + outsz + off : 8 * nsaved + autosz + outsz - 8 + off;
}

/* does the statement take the address of the frame in a way that needs the register (frame_address, not &local)? */
static int
chkreal(NODE *p, NODE *parent)
{
	int o = p->n_op;

	if (o == REG && regno(p) == FPREG)
		return !(parent != NULL && (parent->n_op == PLUS || parent->n_op == MINUS) && parent->n_left == p &&
		    parent->n_right->n_op == ICON && parent->n_right->n_name[0] == '\0');
	if (optype(o) == UTYPE)
		return chkreal(p->n_left, p);
	if (optype(o) == BITYPE)
		return chkreal(p->n_left, p) || chkreal(p->n_right, p);
	return 0;
}

/*
 * Does p, a node of a statement, need the frame pointer as a register (not just to reach a parameter)?
 */
static int
chkf(NODE *p, NODE *parent)
{
	int o = p->n_op;

	if (o == OREG && R2TEST(regno(p)))
		return R2UPK1(regno(p)) == FPREG || R2UPK2(regno(p)) == FPREG;
	if (o == OREG && regno(p) == FPREG)
		return getlval(p) < 0;
	if (o == REG && regno(p) == FPREG) {
		if (parent != NULL && parent->n_op == PLUS && parent->n_left == p &&
		    parent->n_right->n_op == ICON && parent->n_right->n_name[0] == '\0' &&
		    getlval(parent->n_right) > 0)
			return 0;
		return 1;
	}
	if (optype(o) == UTYPE)
		return chkf(p->n_left, p);
	if (optype(o) == BITYPE)
		return chkf(p->n_left, p) || chkf(p->n_right, p);
	return 0;
}

/* the size of the outgoing argument area needed by this tree */
static int
chkout(NODE *p)
{
	int o = p->n_op, n, m;

	if (o == OREG && regno(p) == STKREG && !R2TEST(regno(p))) {
		n = getlval(p) + memsz(p->n_type);
		if ((p->n_type == STRTY || p->n_type == UNIONTY) && p->n_ap != NULL &&
		    attr_find(p->n_ap, ATTR_P2STRUCT) != NULL)
			n = getlval(p) + attr_find(p->n_ap, ATTR_P2STRUCT)->iarg(0);
		return n;
	}
	switch (optype(o)) {
	case UTYPE:
		return chkout(p->n_left);
	case BITYPE:
		n = chkout(p->n_left);
		m = chkout(p->n_right);
		return n > m ? n : m;
	}
	return 0;
}

static void
offcalc(struct interpass_prolog *ipp)
{
	struct interpass *ip;
	int i, needfp = 0, needreal = 0;

	outsz = 0;
	DLIST_FOREACH(ip, &ipp->ipp_ip, qelem) {
		int n;

		if (ip->type == IP_EPILOG)
			break;
		if (ip->type != IP_NODE)
			continue;
		if (chkf(ip->ip_node, NULL))
			needfp = 1;
		if (chkreal(ip->ip_node, NULL))
			needreal = 1;
		n = chkout(ip->ip_node);
		if (n > outsz)
			outsz = n;
	}
	outsz = (outsz + 7) & ~7;
	autosz = (p2maxautooff + 7) & ~7;

	nsaved = savemask = 0;
	for (i = 0; i < MAXREGS; i++)
		if (TESTBIT(p2env.p_regs, i) && (i >= R8 && i <= R15)) {
			nsaved++;
			savemask |= 1 << (i == R15 ? 15 : i);
		}
	if (pdp64_varfunc || needreal || (!xdmr && (autosz || nsaved || needfp)))
		mode = MFRAME;
	else if (autosz || nsaved || outsz)
		mode = MSP;		/* with -xdmr no frame pointer: it costs six to ten bytes a function */
	else
		mode = MNONE;
}

/* the one callee-saved register of savemask */
static int
onesaved(void)
{
	int r = 0;

	while (!(savemask >> r & 1))
		r++;
	return r;
}

void
prologue(struct interpass_prolog *ipp)
{
	int i;

	offcalc(ipp);
	if (mode == MNONE)
		return;
	if (mode == MSP) {
		if (nsaved == 1)
			printf("\t" M_SOP_PUSH " r%d\n", onesaved());
		else if (nsaved)
			printf("\t" M_SOP_PUSHM " #%d\n", savemask);
		if (autosz + outsz)
			printf("\t" M_ALU_SUB " #%d,sp\n", autosz + outsz);
		return;
	}
	if (pdp64_varfunc) {
		/* the register arguments below the stack arguments, then the return address again */
		printf("\t" M_SOP_POP " r12\n");
		for (i = 5; i >= 0; i--)
			printf("\t" M_SOP_PUSH " r%d\n", i);
		printf("\t" M_SOP_PUSH " r12\n");
	}
	printf("\t" M_SOP_PUSH " fp\n");
	printf("\t" M_MOV_MOVQ " sp,fp\n");
	if (nsaved == 1)
		printf("\t" M_SOP_PUSH " r%d\n", onesaved());
	else if (nsaved)
		printf("\t" M_SOP_PUSHM " #%d\n", savemask);
	if (autosz + outsz)
		printf("\t" M_ALU_SUB " #%d,sp\n", autosz + outsz);
}

void
eoftn(struct interpass_prolog *ipp)
{
	if (ipp->ipp_ip.ip_lbl == 0)
		return; /* no code needs to be generated */

	if (mode == MSP) {
		if (autosz + outsz)
			printf("\t" M_ALU_ADD " #%d,sp\n", autosz + outsz);
		if (nsaved == 1)
			printf("\t" M_SOP_POP " r%d\n", onesaved());
		else if (nsaved)
			printf("\t" M_SOP_POPM " #%d\n", savemask);
	} else if (mode == MFRAME) {
		if (nsaved) {
			if (autosz + outsz)
				printf("\t" M_LEA_LEA " %d(fp),sp\n", -8 * nsaved);
			if (nsaved == 1)
				printf("\t" M_SOP_POP " r%d\n", onesaved());
			else
				printf("\t" M_SOP_POPM " #%d\n", savemask);
		} else if (autosz + outsz)
			printf("\t" M_MOV_MOVQ " fp,sp\n");
		printf("\t" M_SOP_POP " fp\n");
		if (pdp64_varfunc) {
			printf("\t" M_SOP_POP " r12\n");
			printf("\t" M_ALU_ADD " #48,sp\n");
			printf("\t" M_SOP_PUSH " r12\n");
		}
	}
	/* for peep.h: ret reads r1 only in a function that returns a struct in r0:r1 */
	printf("\t; ret:%s,out=%d\n", ipp->ipp_type == STRTY || ipp->ipp_type == UNIONTY ? "pair" : "one", outsz);
	printf("\t" M_SYS_NONE_RET "\n");
}

/*
 * add/sub/...
 */
void
hopcode(int f, int o)
{
	char *str;

	switch (o) {
	case PLUS: str = M_ALU_ADD; break;
	case MINUS: str = M_ALU_SUB; break;
	case AND: str = M_ALU_AND; break;
	case OR: str = M_ALU_BIS; break;
	case ER: str = M_ALU_XOR; break;
	default:
		comperr("hopcode2: %d", o);
		str = 0; /* XXX gcc */
	}
	printf("%s", str);
}

/*
 * Return type size in bytes.
 */
int
tlen(NODE *p)
{
	return memsz(p->n_type);
}

int
fldexpand(NODE *p, int cookie, char **cp)
{
	return 0;
}

/*
 * Load the constant v into the register named rn.
 */
static void
loadconstn(char *rn, long long v)
{
	if (v == 0) {
		printf("\t" M_SOP_CLR " %s\n", rn);
		return;
	}
	if (v >= -(1LL << 31) && v < (1LL << 31)) {
		printf("\t" M_MOV_MOVQ " #%lld,%s\n", v, rn);
		return;
	}
	/* wider than the immediate: the assembler's literal pool, asked for with = */
	printf("\t" M_MOV_MOVQ " =%lld,%s\n", v, rn);
}

/* print a symbolic constant: name+offset */
static void
symput(FILE *fp, NODE *p)
{
	CONSZ val = getlval(p);

	fprintf(fp, "%s", p->n_name);
	if (val > 0)
		fprintf(fp, "+%lld", val);
	else if (val < 0)
		fprintf(fp, "%lld", val);
}

/*
 * Structure copy.  The destination is the left node (memory), the source address is in the
 * right node's register.  A1 and A2 are registers (A3 as well for a long copy: mcpy).
 */
/* reg = the address of the memory operand l: a move when it is just a register */
static void
leaof(NODE *l, char *reg)
{
	if (l->n_op == OREG && !R2TEST(regno(l)) && l->n_name[0] == '\0' && fpoff(regno(l), getlval(l)) == 0) {
		printf("\t" M_MOV_MOVQ " %s,%s\n", basereg(regno(l)), reg);
		return;
	}
	printf("\t" M_LEA_LEA " ");
	adrput(stdout, l);
	printf(",%s\n", reg);
}

static void
stasg(NODE *p, int res)
{
	NODE *l = p->n_left, *r = p->n_right;
	struct attr *ap = attr_find(p->n_ap, ATTR_P2STRUCT);
	int n = ap->iarg(0), al = ap->iarg(1), k = 0, w;
	char *mn, *src, srcbuf[16], dst[64];
	long off;
	int simple = l->n_op == OREG && !R2TEST(regno(l)) && l->n_name[0] == '\0';

	if (res) {
		/* the value is the address of the destination, in the register of the right operand */
		leaof(l, rnames[regno(r)]);
		return;
	}
	snprintf(srcbuf, sizeof srcbuf, "%s", rnames[regno(r)]);
	src = srcbuf;

	{
		/* how many load-store pairs would the words take?  Three or more cost more than mcpy's four instructions */
		int m = n, items = 0, ww;

		while (m > 0) {
			ww = (al >= 8 && m >= 8) ? 8 : (al >= 4 && m >= 4) ? 4 : (al >= 2 && m >= 2) ? 2 : 1;
			m -= ww;
			items++;
		}
		if (items > 2)
			n += 1000000;		/* mark: use mcpy */
	}
	if (n > 1000000 || n > 16) {
		if (n > 1000000)
			n -= 1000000;
		/* mcpy dest,src,count, all three consumed: copies in registers of our own */
		int dreg = regno(getlr(p, '1'));
		int sreg = regno(getlr(p, '2'));
		int creg = regno(getlr(p, '3'));

		leaof(l, rnames[dreg]);
		printf("\t" M_MOV_MOVQ " %s,%s\n", src, rnames[sreg]);
		loadconstn(rnames[creg], n);
		printf("\t" M_MBLK_MCPY " %s,%s,%s\n", rnames[dreg], rnames[sreg], rnames[creg]);
		return;
	}
	/* a few words: through the scratch register */
	if (simple) {
		off = fpoff(regno(l), getlval(l));
		snprintf(dst, sizeof dst, "%s", basereg(regno(l)));
	} else if (l->n_op == NAME) {
		off = 0;
		dst[0] = 0;
	} else {
		leaof(l, rnames[regno(getlr(p, '1'))]);
		off = 0;
		snprintf(dst, sizeof dst, "%s", rnames[regno(getlr(p, '1'))]);
	}
	for (; n > 0; n -= w, k += w) {
		if (al >= 8 && n >= 8) { w = 8; mn = M_MOV_MOVQ; }
		else if (al >= 4 && n >= 4) { w = 4; mn = M_MOV_MOVL; }
		else if (al >= 2 && n >= 2) { w = 2; mn = M_MOV_MOVW; }
		else { w = 1; mn = M_MOV_MOVB; }
		printf("\t%s %d(%s),r12\n", mn, k, src);
		if (l->n_op != NAME)
			printf("\t%s r12,%ld(%s)\n", mn, off + k, dst);
		else {
			printf("\t%s r12,", mn);
			symput(stdout, l);
			if (k)
				printf("+%d", k);
			printf("\n");
		}
	}
}

void
zzzcode(NODE *p, int c)
{
	NODE *l, *r;
	struct attr *ap;
	CONSZ v;
	int i;

	switch (c) {
	case 'K':	/* constant (left) into A1 */
	case 'k':	/* constant (right) into the left register */
		if (c == 'K') {
			l = getlr(p, 'L');
			r = getlr(p, '1');
		} else {
			l = getlr(p, 'R');
			r = getlr(p, 'L');
		}
		if (l->n_name[0] != '\0') {
			printf("\t" M_LEA_LEA " ");
			symput(stdout, l);
			printf(",%s\n", rnames[regno(r)]);
		} else
			loadconstn(rnames[regno(r)], conval(l));
		break;

	case 'A':	/* A1 = AL + constant (the right operand) */
	case 'a':	/* A1 = AL - constant */
		l = getlr(p, 'L');
		r = getlr(p, 'R');
		v = conval(r);
		if (c == 'a')
			v = -v;
		if (l->n_op == REG && regno(l) == FPREG)
			v = fpoff(FPREG, v);
		if (v == 0)
			printf("\t" M_MOV_MOVQ " %s,%s\n", l->n_op == REG && regno(l) == FPREG ? basereg(FPREG) : rnames[regno(l)],
			    rnames[regno(getlr(p, '1'))]);
		else
			printf("\t" M_LEA_LEA " %lld(%s),%s\n", v, l->n_op == REG && regno(l) == FPREG ? basereg(FPREG) : rnames[regno(l)],
			    rnames[regno(getlr(p, '1'))]);
		break;

	case 'N':	/* A1 = AL + symbol, AL the other operand */
		r = getlr(p, 'R');
		printf("\t" M_LEA_LEA " ");
		symput(stdout, r);
		printf(",%s\n", rnames[regno(getlr(p, '1'))]);
		printf("\t" M_ALU_ADD " %s,%s\n", rnames[regno(getlr(p, 'L'))], rnames[regno(getlr(p, '1'))]);
		break;

	case 'X':	/* re-extend the result in A1 to the width of its type */
	case 'Y':	/* ... in AL */
	case 'U':	/* ... in AL, for ~: only types narrower than int need it */
		{
			char *mn = NULL, *rn = rnames[regno(getlr(p, c == 'X' ? '1' : 'L'))];

			switch (p->n_type) {
			case INT:
				/* signed overflow is undefined: the 64-bit result is the right one whenever it is defined (-mwrap: wrap anyway); but a left shift is how pass 1 reads a bit-field, into the sign bit */
				if (c != 'U' && (wrapint || p->n_op == LS) && !nowrap && !isnowrap(p))
					mn = M_MOV_MOVL;
				break;
			case UNSIGNED:
				if (c != 'U' && !nowrap && !isnowrap(p))
					mn = M_MOV_MOVL;
				break;
			case SHORT: mn = M_MOV_MOVW; break;
			case USHORT: mn = M_MOVZ_MOVZW; break;
			case CHAR: mn = M_MOV_MOVB; break;
			case UCHAR: case BOOL: mn = M_MOVZ_MOVZB; break;
			}
			if (mn)
				printf("\t%s %s,%s\n", mn, rn, rn);
		}
		break;

	case 'C':	/* the target of a call or jump: a symbol, or an absolute address */
		l = getlr(p, 'L');
		if (l->n_name[0] != '\0')
			symput(stdout, l);
		else
			printf("@" CONFMT, conval(l));
		break;

	case 'F':	/* a floating-point compare (see cbgen) */
		fpinv = attr_find(p->n_ap, ATTR_FP_SWAPPED) != NULL;
		fpcmp = 1;
		i = fpinv ? p->n_op == GT : (p->n_op == LE || p->n_op == LT);	/* reversed */
		l = getlr(p, 'L');
		r = getlr(p, 'R');
		if (i) {
			NODE *x = l; l = r; r = x;
		}
		printf("\t%s %s,%s\n", p->n_left->n_type == FLOAT ? M_FESC_FCMP32 : M_ALU_FCMP, rnames[regno(l)], rnames[regno(r)]);
		break;

	case 'M':	/* and with 0xff, 0xffff or 0xffffffff is a zero-extending move: A1 = AL & mask */
		{
			CONSZ w = getlval(getlr(p, 'R'));

			printf("\t%s %s,%s\n", w == 0xff ? M_MOVZ_MOVZB : w == 0xffff ? M_MOVZ_MOVZW : M_MOVZ_MOVZL,
			    rnames[regno(getlr(p, 'L'))], rnames[regno(getlr(p, '1'))]);
		}
		break;

	case 'S':	/* struct assignment */
		stasg(p, 0);
		break;

	case 'T':	/* ... whose value is the address of the destination, in the right register */
		stasg(p, 1);
		break;

	case 'R':	/* a struct result of up to 16 bytes comes back in r0:r1: store it in the bounce space,
			 * and r0 is made its address */
		ap = attr_find(p->n_ap, ATTR_PDP64_STRRET);
		if (ap == NULL || ap->iarg(0) == SC_MEM)
			break;
		for (i = 0; i < ap->iarg(1); i++)
			printf("\t" M_MOV_MOVQ " r%d,%ld(%s)\n", i, fpoff(FPREG, -stkpos + i * 8), basereg(FPREG));
		printf("\t" M_LEA_LEA " %ld(%s),r0\n", fpoff(FPREG, -stkpos), basereg(FPREG));
		break;

	default:
		comperr("zzzcode %c", c);
	}
}

void
adrcon(CONSZ val)
{
	printf("#" CONFMT, val);
}

void
conput(FILE *fp, NODE *p)
{
	switch (p->n_op) {
	case ICON:
		if (p->n_name[0] != '\0')
			symput(fp, p);
		else
			fprintf(fp, CONFMT, conval(p));
		return;

	default:
		comperr("illegal conput, p %p", p);
	}
}

void
insput(NODE *p)
{
	comperr("insput");
}

void
upput(NODE *p, int size)
{
	comperr("upput bad op %d size %d", p->n_op, size);
}

void
adrput(FILE *io, NODE *p)
{
	long off;

	switch (p->n_op) {

	case NAME:
		if (p->n_name[0] != '\0')
			symput(io, p);
		else
			fprintf(io, "@%lld", getlval(p));
		return;

	case OREG:
		if (R2TEST(regno(p)) && p->n_name[0] == '\0') {
			int r1 = R2UPK1(regno(p)), r2 = R2UPK2(regno(p)), sh = R2UPK3(regno(p));

			off = fpoff(r1, getlval(p));
			if (off)
				fprintf(io, "%ld", off);
			fprintf(io, "(%s)(%s", basereg(r1), rnames[r2]);
			if (sh)
				fprintf(io, "*%d", 1 << sh);
			fprintf(io, ")");
			return;
		}
		if (p->n_name[0] != '\0') {
			if (x2debug || c2debug || e2debug || o2debug || f2debug || s2debug || r2debug) {
				fprintf(io, "<OREG form r=%d name=%s>", regno(p), p->n_name);
				return;
			}
			comperr("adrput: OREG form r=%d name=%s lval=%lld", regno(p), p->n_name, getlval(p));
		}
		off = fpoff(regno(p), getlval(p));
		if (off)
			fprintf(io, "%ld", off);
		fprintf(io, "(%s)", basereg(regno(p)));
		return;

	case ICON:
		/* addressable value of the constant */
		fputc('#', io);
		conput(io, p);
		return;

	case REG:
		fprintf(io, "%s", rnames[p->n_rval]);
		return;

	default:
		comperr("illegal address, op %d, node %p", p->n_op, p);
		return;

	}
}

static char *
ccbranches[] = {
	M_BRANCH_BEQ,	/* jumpe */
	M_BRANCH_BNE,	/* jumpn */
	M_BRANCH_BLE,	/* jumple */
	M_BRANCH_BLT,	/* jumpl */
	M_BRANCH_BGE,	/* jumpge */
	M_BRANCH_BGT,	/* jumpg */
	M_BRANCH_BLOS,	/* jumple (jlequ) */
	M_BRANCH_BCS,	/* jumpl (jlssu) */
	M_BRANCH_BCC,	/* jumpge (jgequ) */
	M_BRANCH_BHI,	/* jumpg (jgtru) */
};

/*
 * After a floating-point compare N is a<b, Z is a==b, V is unordered, C is clear.  An unordered pair
 * makes <, <=, > and >= false (and != true), so < and <= are done by comparing the other way round and
 * asking > and >=, which are false for it.  Pass 1 turns "if (a < b) s" into a jump over s on the
 * opposite condition (marked ATTR_FP_SWAPPED), which must then be true for an unordered pair: that
 * is a<b -> bpl (not N), a<=b -> reversed blt, a>b -> ble, a>=b -> blt.  The order is EQ, NE, LE, LT, GE, GT.
 */
static char *
fpbranches[] = {
	M_BRANCH_BEQ, M_BRANCH_BNE, M_BRANCH_BGE, M_BRANCH_BGT, M_BRANCH_BGE, M_BRANCH_BGT,
};
static char *
fpibranches[] = {
	M_BRANCH_BEQ, M_BRANCH_BNE, M_BRANCH_BLE, M_BRANCH_BLT, M_BRANCH_BPL, M_BRANCH_BLT,
};

/*   printf conditional and unconditional branches */
void
cbgen(int o, int lab)
{
	if (o < EQ || o > UGT)
		comperr("bad conditional branch: %s", opst[o]);
	if (fpcmp && o <= GT) {
		printf("\t%s " LABFMT "\n", (fpinv ? fpibranches : fpbranches)[o-EQ], lab);
		fpcmp = 0;
	} else
		printf("\t%s " LABFMT "\n", ccbranches[o-EQ], lab);
}

static void
fixcalls(NODE *p, void *arg)
{
	int ps;

	/* Prepare for struct return by allocating bounce space on stack */
	switch (p->n_op) {
	case STCALL:
	case USTCALL:
		ps = attr_find(p->n_ap, ATTR_P2STRUCT)->iarg(0);
		ps = (ps + 7) & ~7;
		if (ps > bouncesz)
			bouncesz = ps;
		break;
	}
}

static void sym2name(NODE *p, void *arg);

static int
isnarrowint(TWORD t)
{
	return t == CHAR || t == UCHAR || t == SHORT || t == USHORT || t == INT || t == UNSIGNED || t == BOOL;
}

/*
 * A store of a narrow integer writes the low bytes only, so a conversion to the type stored is not needed
 * when the value is not used:  x = (uchar)expr  is a movb of expr.  p is a statement, or the left of a
 * comma operator.
 */
#define	MAXNOW	256
static NODE *nowrapnodes[MAXNOW];	/* arithmetic whose result goes straight to a store of at most 32 bits */
static int nnow;

static int
isnowrap(NODE *p)
{
	int i;

	for (i = 0; i < nnow; i++)
		if (nowrapnodes[i] == p)
			return 1;
	return 0;
}

static void
stripsconv(NODE *p)
{
	NODE *l, *r;

	if (p->n_op != ASSIGN)
		return;
	l = p->n_left;
	r = p->n_right;
	if ((l->n_op == OREG || l->n_op == NAME || l->n_op == UMUL) && isnarrowint(l->n_type) && r->n_op == SCONV &&
	    (isnarrowint(r->n_left->n_type) || r->n_left->n_type == LONG || r->n_left->n_type == ULONG || r->n_left->n_type == LONGLONG || r->n_left->n_type == ULONGLONG) &&
	    tlen(r->n_left) >= tlen(l)) {
		p->n_right = r->n_left;
		nfree(r);
	}
	r = p->n_right;
	if ((l->n_op == OREG || l->n_op == NAME || l->n_op == UMUL) && isnarrowint(l->n_type) &&
	    (r->n_type == INT || r->n_type == UNSIGNED) && nnow < MAXNOW &&
	    (r->n_op == PLUS || r->n_op == MINUS || r->n_op == MUL || r->n_op == LS || r->n_op == UMINUS))
		nowrapnodes[nnow++] = r;
}

void
myreader(struct interpass *ipole)
{
	struct interpass *ip;

	bouncesz = 0;
	nnow = 0;
	DLIST_FOREACH(ip, ipole, qelem) {
		if (ip->type != IP_NODE)
			continue;
		walkf(ip->ip_node, fixcalls, 0);
		walkf(ip->ip_node, sym2name, 0);
	}
	DLIST_FOREACH(ip, ipole, qelem) {
		if (ip->type == IP_NODE)
			stripsconv(ip->ip_node);
	}
	if (bouncesz) {
		/* reserved after the locals: [-stkpos, -stkpos+bouncesz) from fp */
		p2autooff = (p2autooff + 7) & ~7;
		p2autooff += bouncesz;
		stkpos = p2autooff;
		if (p2autooff > p2maxautooff)
			p2maxautooff = p2autooff;
	}
	if (x2debug)
		printip(ipole);
}

/* no bit-fields in registers, no INTEMP shapes */
int
flshape(NODE *p)
{
	return SRNOPE;
}

int
shtemp(NODE *p)
{
	return 0;
}

/* *(symbol+offset) is a memory operand of its own, reached pc-relative */
static void
sym2name(NODE *p, void *arg)
{
	NODE *q;

	if (p->n_op != UMUL)
		return;
	if ((q = p->n_left)->n_op == PCONV && q->n_left->n_op == ICON) {
		p->n_left = q->n_left;
		nfree(q);
	}
	if ((q = p->n_left)->n_op != ICON)
		return;
	if (q->n_name[0] == '\0' && (getlval(q) < -(1LL << 31) || getlval(q) >= (1LL << 31)))
		return;
	{
		CONSZ v = getlval(q);
		char *n = q->n_name;

		p->n_op = NAME;
		p->n_left = NULL;
		p->n_name = n;
		setlval(p, v);
		nfree(q);
	}
}

void
mycanon(NODE *p)
{
	walkf(p, sym2name, 0);
}

void
myoptim(struct interpass *ip)
{
}

void
rmove(int s, int d, TWORD t)
{
	printf("\t" M_MOV_MOVQ " %s,%s\n", rnames[s], rnames[d]);
}

/*
 * Can we allocate another register of class c, given that r[] are used?
 */
int
COLORMAP(int c, int *r)
{
	return r[CLASSA] < 13;
}

char *rnames[MAXREGS] = {
	"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
	"r8", "r9", "r10", "r11", "r15", "fp", "sp", "r12",
};

/*
 * Return a class suitable for a specific type.
 */
int
gclass(TWORD t)
{
	return CLASSA;
}

/*
 * Calculate argument sizes.  The argument area is in the frame (offcalc).
 */
void
lastcall(NODE *p)
{
	p->n_qual = 0;
}

/*
 * Special shapes.
 */
int
special(NODE *p, int shape)
{
	int o = p->n_op;
	CONSZ v;

	if (shape == SFPSP)
		return o == REG && (regno(p) == FPREG || regno(p) == STKREG) ? SRDIR : SRNOPE;
	if (shape == SMASK) {
		CONSZ w = o == ICON && !p->n_name[0] ? getlval(p) : 0;

		/* a 32-bit unsigned constant is kept sign-extended, so 0xffffffff of an int is -1 (a no-op, folded earlier) */
		return (w == 0xff || w == 0xffff || w == 0xffffffffLL) ? SRDIR : SRNOPE;
	}
	if (shape == SSYM)
		return o == ICON && p->n_name[0] ? SRDIR : SRNOPE;
	if (o != ICON || p->n_name[0])
		return SRNOPE;
	v = conval(p);
	switch (shape) {
	case SZERO:
		return v == 0 ? SRDIR : SRNOPE;
	case SONE:
		return v == 1 ? SRDIR : SRNOPE;
	case SMONE:
		return v == -1 ? SRDIR : SRNOPE;
	case SCCON:
		return v >= -256 && v < 256 ? SRDIR : SRNOPE;
	case SSCON:
		return v >= -32768 && v < 32768 ? SRDIR : SRNOPE;
	case SIMM:
		return v >= -(1LL << 31) && v < (1LL << 31) ? SRDIR : SRNOPE;
	case SWIDE:
		return v < -(1LL << 31) || v >= (1LL << 31) ? SRDIR : SRNOPE;
	case SDISP:
		return v >= -(1LL << 27) && v < (1LL << 27) ? SRDIR : SRNOPE;
	case SSMALL:
		return v >= -2048 && v < 2048 ? SRDIR : SRNOPE;
	case SSH:
		return v >= 0 && v < 64 ? SRDIR : SRNOPE;
	case SSHP:
		return v >= 1 && v < 64 ? SRDIR : SRNOPE;
	case SPCON:
		return v >= 0 ? SRDIR : SRNOPE;
	}
	cerror("special: %x\n", shape);
	return SRNOPE;
}

/*
 * Target-dependent command-line options.
 */
void
mflags(char *str)
{
	extern int rodata_in_text;

	if (strcmp(str, "nowrap") == 0)
		nowrap = 1;
	else if (strcmp(str, "wrap") == 0)
		wrapint = 1;
	else if (strcmp(str, "rodata") == 0)
		rodata_in_text = 0;		/* strings and constants in .data, as a linker with W^X wants */
}

/*
 * Do something target-dependent for xasm arguments.
 */
int
myxasm(struct interpass *ip, NODE *p)
{
	return 0;
}
