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

static int mode, nsaved, savemask, autosz, outsz;
static int nowrap;		/* -mnowrap: signed int arithmetic does not overflow (so needs no re-extension) */
static int stkpos;		/* start of the bounce space for struct results, from fp */
static int bouncesz;		/* its size: the largest struct a call here returns */

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
	return off - 8 + (mode == MSP ? outsz : 0);
}

/*
 * Does p, a node of a statement, need the frame pointer as a register (not just to reach a parameter)?
 */
static int
chkf(NODE *p, NODE *parent)
{
	int o = p->n_op;

	if (o == OREG && regno(p) == FPREG && !R2TEST(regno(p)))
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
	int i, needfp = 0;

	outsz = 0;
	DLIST_FOREACH(ip, &ipp->ipp_ip, qelem) {
		int n;

		if (ip->type == IP_EPILOG)
			break;
		if (ip->type != IP_NODE)
			continue;
		if (chkf(ip->ip_node, NULL))
			needfp = 1;
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
	if (autosz || nsaved || needfp || pdp64_varfunc)
		mode = MFRAME;
	else if (outsz)
		mode = MSP;
	else
		mode = MNONE;
}

void
prologue(struct interpass_prolog *ipp)
{
	int i;

	offcalc(ipp);
	if (mode == MNONE)
		return;
	if (mode == MSP) {
		printf("\t" M_ALU_SUB " #%d,sp\n", outsz);
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
	if (nsaved)
		printf("\t" M_SOP_PUSHM " #%d\n", savemask);
	if (autosz + outsz)
		printf("\t" M_ALU_SUB " #%d,sp\n", autosz + outsz);
}

void
eoftn(struct interpass_prolog *ipp)
{
	if (ipp->ipp_ip.ip_lbl == 0)
		return; /* no code needs to be generated */

	if (mode == MSP)
		printf("\t" M_ALU_ADD " #%d,sp\n", outsz);
	else if (mode == MFRAME) {
		if (nsaved) {
			if (autosz + outsz)
				printf("\t" M_LEA_LEA " %d(fp),sp\n", -8 * nsaved);
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
	long long lo, hi;

	if (v == 0) {
		printf("\t" M_SOP_CLR " %s\n", rn);
		return;
	}
	if (v >= -(1LL << 47) && v < (1LL << 47)) {
		printf("\t" M_MOV_MOVQ " #%lld,%s\n", v, rn);
		return;
	}
	/* v = (hi << 32) + lo, lo a signed 32-bit number */
	lo = (int)v;
	hi = (v - lo) >> 32;
	printf("\t" M_MOV_MOVQ " #%lld,%s\n", hi, rn);
	printf("\t" M_ALU_SHL " #32,%s\n", rn);
	if (lo)
		printf("\t" M_ALU_ADD " #%lld,%s\n", lo, rn);
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
static void
stasg(NODE *p, int res)
{
	NODE *l = p->n_left, *r = p->n_right;
	struct attr *ap = attr_find(p->n_ap, ATTR_P2STRUCT);
	int n = ap->iarg(0), al = ap->iarg(1), k = 0, w;
	char *mn, *src, srcbuf[16], dst[64];
	long off;

	if (l->n_op == OREG) {
		off = fpoff(regno(l), getlval(l));
		snprintf(dst, sizeof dst, "%s", basereg(regno(l)));
	} else {
		off = 0;
		dst[0] = 0;
	}
	snprintf(srcbuf, sizeof srcbuf, "%s", rnames[regno(r)]);
	src = srcbuf;

	if (res)
		goto result;
	if (n > 16 && al >= 1) {
		/* mcpy dest,src,count, all three consumed: copies in registers of our own */
		int dreg = regno(getlr(p, '1'));
		int sreg = regno(getlr(p, '2'));
		int creg = regno(getlr(p, '3'));

		if (l->n_op == OREG)
			printf("\t" M_LEA_LEA " %ld(%s),%s\n", off, dst, rnames[dreg]);
		else {
			printf("\t" M_LEA_LEA " ");
			symput(stdout, l);
			printf(",%s\n", rnames[dreg]);
		}
		printf("\t" M_MOV_MOVQ " %s,%s\n", src, rnames[sreg]);
		loadconstn(rnames[creg], n);
		printf("\t" M_MBLK_MCPY " %s,%s,%s\n", rnames[dreg], rnames[sreg], rnames[creg]);
		goto result;
	}
	/* a few words: through the scratch register */
	for (; n > 0; n -= w, k += w) {
		if (al >= 8 && n >= 8) { w = 8; mn = M_MOV_MOVQ; }
		else if (al >= 4 && n >= 4) { w = 4; mn = M_MOV_MOVL; }
		else if (al >= 2 && n >= 2) { w = 2; mn = M_MOV_MOVW; }
		else { w = 1; mn = M_MOV_MOVB; }
		printf("\t%s %d(%s),r12\n", mn, k, src);
		if (l->n_op == OREG)
			printf("\t%s r12,%ld(%s)\n", mn, off + k, dst);
		else {
			printf("\t%s r12,", mn);
			symput(stdout, l);
			if (k)
				printf("+%d", k);
			printf("\n");
		}
	}
result:
	if (res) {
		int rr = regno(r);

		if (l->n_op == OREG)
			printf("\t" M_LEA_LEA " %ld(%s),%s\n", off, dst, rnames[rr]);
		else {
			printf("\t" M_LEA_LEA " ");
			symput(stdout, l);
			printf(",%s\n", rnames[rr]);
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

	case 'X':	/* re-extend the 32-bit result in A1 */
		if ((p->n_type == INT || p->n_type == UNSIGNED) && !nowrap)
			printf("\t" M_MOV_MOVL " %s,%s\n", rnames[regno(getlr(p, '1'))], rnames[regno(getlr(p, '1'))]);
		break;

	case 'Y':	/* re-extend the 32-bit result in AL */
		if ((p->n_type == INT || p->n_type == UNSIGNED) && !nowrap)
			printf("\t" M_MOV_MOVL " %s,%s\n", rnames[regno(getlr(p, 'L'))], rnames[regno(getlr(p, 'L'))]);
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
			printf("\t" M_MOV_MOVQ " r%d,%d(fp)\n", i, fpoff(FPREG, -stkpos + i * 8));
		printf("\t" M_LEA_LEA " %d(fp),r0\n", fpoff(FPREG, -stkpos));
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
		if (R2TEST(regno(p)) || p->n_name[0] != '\0') {
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

/*   printf conditional and unconditional branches */
void
cbgen(int o, int lab)
{
	if (o < EQ || o > UGT)
		comperr("bad conditional branch: %s", opst[o]);
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

void
myreader(struct interpass *ipole)
{
	struct interpass *ip;

	bouncesz = 0;
	DLIST_FOREACH(ip, ipole, qelem) {
		if (ip->type != IP_NODE)
			continue;
		walkf(ip->ip_node, fixcalls, 0);
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

void
mycanon(NODE *p)
{
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
		return v >= -(1LL << 47) && v < (1LL << 47) ? SRDIR : SRNOPE;
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
	if (strcmp(str, "nowrap") == 0)
		nowrap = 1;
}

/*
 * Do something target-dependent for xasm arguments.
 */
int
myxasm(struct interpass *ip, NODE *p)
{
	return 0;
}
