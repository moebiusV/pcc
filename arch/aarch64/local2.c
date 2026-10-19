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

# include "pass2.h"
# include <ctype.h>
# include <string.h>

static int stkpos;		/* start of the bounce space for struct results, from x29 */
static int bouncesz;		/* its size: the largest struct a call here returns */
static int regoff[MAXREGS];	/* where the callee-saved registers are kept */
static int needframe;
static int locsz;		/* distance from x29 to the caller's sp */
static int outsz;		/* outgoing argument area at the bottom */
static int framesz;
static int fpcmp;		/* the last compare was floating point */
static int fpinv;		/* ... and its condition was inverted by pass 1 */

void
deflab(int label)
{
	printf(LABFMT ":\n", label);
}

/*
 * The registers by their machine numbers.  x16 and x17 are never allocated:
 * they are the scratch registers of the code printed here.
 */
static int
mreg(int r)
{
	if (r < 16)
		return r;
	if (r < FPREG)
		return r - 16 + 19;
	if (r == FPREG)
		return 29;
	return r - D0;
}

static int
wide(TWORD t)
{
	if (t > BTMASK)
		return 1;
	switch (t) {
	case CHAR: case UCHAR: case SHORT: case USHORT:
	case INT: case UNSIGNED: case BOOL: case FLOAT:
		return 0;
	}
	return 1;
}

static char *
regname(int r, TWORD t)
{
	static char buf[4][16];
	static int n;
	char *s = buf[n++ & 3];

	if (r >= D0)
		snprintf(s, 16, "%c%d", t == FLOAT ? 's' : 'd', r - D0);
	else if (r == STKREG)
		strcpy(s, "sp");
	else
		snprintf(s, 16, "%c%d", wide(t) ? 'x' : 'w', mreg(r));
	return s;
}

static char *
xreg(int r)
{
	return regname(r, LONG);
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

/*
 * Offsets from the frame pointer: stack-passed parameters were given
 * PARAMMARK + their place above the caller's stack pointer.
 */
static long
fpoff(int base, long off)
{
	if (base == FPREG && off >= PARAMMARK && off < PARAMMARK + 0x1000000)
		return off - PARAMMARK + locsz;
	return off;
}

/* Can an access of size sz use the offset directly? */
static int
offok(long off, int sz)
{
	if (off >= -256 && off <= 255)
		return 1;
	return off > 0 && (off % sz) == 0 && off / sz <= 4095;
}

static int
isbitmask(unsigned long long v, int is64)
{
	int size, i, tr;
	unsigned long long mask, e;

	if (!is64) {
		v &= 0xffffffffULL;
		v |= v << 32;
	}
	if (v == 0 || v == ~0ULL)
		return 0;
	for (size = 64; size > 2; size /= 2) {
		mask = (1ULL << (size/2)) - 1;
		if ((v & mask) != ((v >> (size/2)) & mask))
			break;
	}
	mask = size == 64 ? ~0ULL : (1ULL << size) - 1;
	e = v & mask;
	tr = 0;
	for (i = 0; i < size; i++)
		if (((e >> i) & 1) != ((e >> ((i+1) % size)) & 1))
			tr++;
	return tr == 2;
}

/*
 * Print the instructions that put the constant v in register rd (named
 * in the width of the value: is64).
 */
static void
loadconstn(char *rn, long long v, int is64)
{
	unsigned long long u = v, nu;
	int bits = is64 ? 64 : 32;
	int i, z = 0, n = 0, first = 1;
	unsigned c;

	if (!is64)
		u &= 0xffffffffULL;
	nu = ~u;
	if (!is64)
		nu &= 0xffffffffULL;
	for (i = 0; i < bits; i += 16) {
		if (((u >> i) & 0xffff) != 0)
			z++;
		if (((u >> i) & 0xffff) != 0xffff)
			n++;
	}
	if (z <= 1) {
		for (i = 0; i < bits; i += 16)
			if (((u >> i) & 0xffff) != 0)
				break;
		if (i >= bits)
			i = 0;
		printf("\tmovz %s,#%llu", rn, (u >> i) & 0xffff);
		if (i)
			printf(",lsl #%d", i);
		printf("\n");
		return;
	}
	if (n <= 1) {
		for (i = 0; i < bits; i += 16)
			if (((u >> i) & 0xffff) != 0xffff)
				break;
		if (i >= bits)
			i = 0;
		printf("\tmovn %s,#%llu", rn, (nu >> i) & 0xffff);
		if (i)
			printf(",lsl #%d", i);
		printf("\n");
		return;
	}
	if (isbitmask(u, is64)) {
		printf("\torr %s,%s,#0x%llx\n", rn, is64 ? "xzr" : "wzr", u);
		return;
	}
	if (z <= n) {
		for (i = 0; i < bits; i += 16) {
			c = (u >> i) & 0xffff;
			if (c == 0)
				continue;
			printf("\t%s %s,#%u", first ? "movz" : "movk", rn, c);
			if (i)
				printf(",lsl #%d", i);
			printf("\n");
			first = 0;
		}
	} else {
		for (i = 0; i < bits; i += 16) {
			c = (u >> i) & 0xffff;
			if (c == 0xffff)
				continue;
			if (first)
				printf("\tmovn %s,#%u", rn, (unsigned)((nu >> i) & 0xffff));
			else
				printf("\tmovk %s,#%u", rn, c);
			if (i)
				printf(",lsl #%d", i);
			printf("\n");
			first = 0;
		}
	}
}

static void
loadconst(int rd, long long v, int is64)
{
	loadconstn(regname(rd, is64 ? LONG : INT), v, is64);
}

/*
 * rd = rn + v (v may be negative), through x17 if v does not fit.
 */
static void
addimm(char *rd, char *rn, long long v)
{
	char *op = "add";

	if (v < 0) {
		op = "sub";
		v = -v;
	}
	if (v == 0) {
		if (strcmp(rd, rn))
			printf("\tmov %s,%s\n", rd, rn);
	} else if (v <= 4095) {
		printf("\t%s %s,%s,#%lld\n", op, rd, rn, v);
	} else if ((v & 0xfff) == 0 && v <= 0xfff000) {
		printf("\t%s %s,%s,#%lld,lsl #12\n", op, rd, rn, v >> 12);
	} else {
		loadconstn("x17", v, 1);
		printf("\t%s %s,%s,x17\n", op, rd, rn);
	}
}

/*
 * A load or store of register rt at base+off.
 */
static void
memins(char *mn, char *rt, char *base, long off, int sz)
{
	if (offok(off, sz)) {
		printf("\t%s %s,[%s,#%ld]\n", mn, rt, base, off);
		return;
	}
	loadconstn("x16", off, 1);
	printf("\t%s %s,[%s,x16]\n", mn, rt, base);
}

/*
 * Calculate stack size and offsets.
 */
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

static int
iscall(NODE *p)
{
	int o = p->n_op;

	if (callop(o))
		return 1;
	if ((o == REG || o == OREG) && regno(p) == FPREG)
		return 1;	/* the frame pointer is in use */
	if (optype(o) == UTYPE)
		return iscall(p->n_left);
	if (optype(o) == BITYPE)
		return iscall(p->n_left) || iscall(p->n_right);
	return 0;
}

static void
offcalc(struct interpass_prolog *ipp)
{
	struct interpass *ip;
	int i, addto, n, hascall = 0;

	outsz = 0;
	DLIST_FOREACH(ip, &ipp->ipp_ip, qelem) {
		if (ip->type == IP_EPILOG)
			break;
		if (ip->type != IP_NODE)
			continue;
		if (iscall(ip->ip_node))
			hascall = 1;
		n = chkout(ip->ip_node);
		if (n > outsz)
			outsz = n;
	}
	outsz = (outsz + 15) & ~15;

	addto = (p2maxautooff + 7) & ~7;
	for (i = 0; i < MAXREGS; i++)
		if (TESTBIT(p2env.p_regs, i)) {
			regoff[i] = addto;
			addto += 8;
			hascall = 1;	/* a frame, whatever it saved */
		}
	locsz = (addto + 15) & ~15;
	framesz = locsz + outsz;
	needframe = hascall || p2maxautooff > AUTOINIT/SZCHAR || outsz;
}

/*
 * Save (or restore) the permanent registers that are used, in pairs where two
 * of them are next to each other.
 */
static void
saveregs(int save)
{
	int i, j;

	for (i = 0; i < MAXREGS; i++) {
		if (!TESTBIT(p2env.p_regs, i))
			continue;
		for (j = i + 1; j < MAXREGS && !TESTBIT(p2env.p_regs, j); j++)
			;
		if (j < MAXREGS && (j >= D0) == (i >= D0) && regoff[j] == regoff[i] + 8 &&
		    regoff[i] <= 504 && (regoff[i] & 7) == 0) {
			printf("\t%s %s,%s,[x29,#%d]\n", save ? "stp" : "ldp",
			    i >= D0 ? regname(i, DOUBLE) : xreg(i),
			    j >= D0 ? regname(j, DOUBLE) : xreg(j), regoff[i]);
			i = j;
		} else
			memins(save ? "str" : "ldr",
			    i >= D0 ? regname(i, DOUBLE) : xreg(i), "x29", regoff[i], 8);
	}
}

void
prologue(struct interpass_prolog *ipp)
{
	offcalc(ipp);
	if (!needframe)
		return;
	if (outsz == 0 && framesz <= 504) {
		printf("\tstp x29,x30,[sp,#-%d]!\n", framesz);
		printf("\tmov x29,sp\n");
	} else {
		addimm("sp", "sp", -(long long)framesz);
		if (outsz <= 504)
			printf("\tstp x29,x30,[sp,#%d]\n", outsz);
		else {
			addimm("x16", "sp", outsz);
			printf("\tstp x29,x30,[x16]\n");
		}
		addimm("x29", "sp", outsz);
	}
	saveregs(1);
}

void
eoftn(struct interpass_prolog *ipp)
{
	if (ipp->ipp_ip.ip_lbl == 0)
		return; /* no code needs to be generated */

	if (needframe) {
		saveregs(0);
		if (outsz == 0 && framesz <= 504)
			printf("\tldp x29,x30,[sp],#%d\n", framesz);
		else {
			if (outsz <= 504)
				printf("\tldp x29,x30,[sp,#%d]\n", outsz);
			else {
				addimm("x16", "sp", outsz);
				printf("\tldp x29,x30,[x16]\n");
			}
			addimm("sp", "sp", framesz);
		}
	}
	printf("\tret\n");
	printf("\t.size %s,.-%s\n", ipp->ipp_name, ipp->ipp_name);
}

/*
 * add/sub/...
 */
void
hopcode(int f, int o)
{
	char *str;

	switch (o) {
	case PLUS:
		str = "add";
		break;
	case MINUS:
		str = "sub";
		break;
	case AND:
		str = "and";
		break;
	case OR:
		str = "orr";
		break;
	case ER:
		str = "eor";
		break;
	default:
		comperr("hopcode2: %d", o);
		str = 0; /* XXX gcc */
	}
	printf("%s%c", str, f);
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
 * The place a memory operand needs to have been prepared: a large
 * offset or a symbol goes through x16.
 */
static int
needx16(NODE *q)
{
	if (q->n_op == NAME)
		return 1;
	if (q->n_op != OREG)
		return 0;
	if (q->n_name[0] != '\0')
		return 1;
	return !offok(fpoff(regno(q), getlval(q)), memsz(q->n_type));
}

static void
prepmem(NODE *q)
{
	if (!needx16(q))
		return;
	if (q->n_op == NAME && q->n_name[0] != '\0') {
		if (getlval(q))
			printf("\tadrp x16,%s+%lld\n", q->n_name, getlval(q));
		else
			printf("\tadrp x16,%s\n", q->n_name);
	} else if (q->n_op == NAME)
		loadconstn("x16", getlval(q), 1);
	else
		loadconstn("x16", fpoff(regno(q), getlval(q)), 1);
}

/*
 * Structure copy: dest is the left node, the source address is in the
 * right node's register.  tmp is a register to count with.
 */
static void
stasg(NODE *p, int res)
{
	NODE *l = p->n_left, *r = p->n_right;
	struct attr *ap = attr_find(p->n_ap, ATTR_P2STRUCT);
	int n = ap->iarg(0), k = 0, lab, big;
	char *src, srcbuf[16];

	/* destination address into x16 */
	if (l->n_op == OREG)
		addimm("x16", xreg(regno(l)), fpoff(regno(l), getlval(l)));
	else if (l->n_name[0]) {
		printf("\tadrp x16,%s", l->n_name);
		if (getlval(l))
			printf("+%lld", getlval(l));
		printf("\n\tadd x16,x16,:lo12:%s", l->n_name);
		if (getlval(l))
			printf("+%lld", getlval(l));
		printf("\n");
	} else
		loadconstn("x16", getlval(l), 1);

	strcpy(srcbuf, xreg(regno(r)));
	src = srcbuf;
	big = n > 128;
	if (big) {
		/* A1: result (if any), then the source cursor and the count */
		int sreg = regno(getlr(p, res ? '2' : '1'));
		int creg = regno(getlr(p, res ? '3' : '2'));

		strcpy(srcbuf, xreg(sreg));
		printf("\tmov %s,%s\n", src, xreg(regno(r)));
		loadconst(creg, n / 8, 1);
		lab = getlab2();
		printf(LABFMT ":\n", lab);
		printf("\tldr x17,[%s],#8\n", src);
		printf("\tstr x17,[x16],#8\n");
		printf("\tsubs %s,%s,#1\n", xreg(creg), xreg(creg));
		printf("\tb.ne " LABFMT "\n", lab);
		n %= 8;
		k = 0;
	}
	for (; n >= 8; n -= 8, k += 8) {
		printf("\tldr x17,[%s,#%d]\n", src, k);
		printf("\tstr x17,[x16,#%d]\n", k);
	}
	if (n >= 4) {
		printf("\tldr w17,[%s,#%d]\n", src, k);
		printf("\tstr w17,[x16,#%d]\n", k);
		n -= 4, k += 4;
	}
	if (n >= 2) {
		printf("\tldrh w17,[%s,#%d]\n", src, k);
		printf("\tstrh w17,[x16,#%d]\n", k);
		n -= 2, k += 2;
	}
	if (n >= 1) {
		printf("\tldrb w17,[%s,#%d]\n", src, k);
		printf("\tstrb w17,[x16,#%d]\n", k);
	}
	if (res) {
		/* the result is the address of the destination */
		if (l->n_op == OREG)
			addimm(xreg(regno(getlr(p, '1'))), xreg(regno(l)),
			    fpoff(regno(l), getlval(l)));
		else {
			printf("\tadrp %s,%s\n", xreg(regno(getlr(p, '1'))), l->n_name);
			printf("\tadd %s,%s,:lo12:%s\n", xreg(regno(getlr(p, '1'))),
			    xreg(regno(getlr(p, '1'))), l->n_name);
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
			char *rn = regname(regno(r), LONG);

			printf("\tadrp %s,%s", rn, l->n_name);
			if (getlval(l))
				printf("+%lld", getlval(l));
			printf("\n\tadd %s,%s,:lo12:%s", rn, rn, l->n_name);
			if (getlval(l))
				printf("+%lld", getlval(l));
			printf("\n");
		} else
			loadconst(regno(r), getlval(l), wide(r->n_type));
		break;

	case 'P':	/* prepare memory operands */
		if (optype(p->n_op) == LTYPE)
			prepmem(p);
		else {
			prepmem(p->n_left);
			if (optype(p->n_op) == BITYPE)
				prepmem(p->n_right);
		}
		break;

	case 'A':	/* A1 = AL + constant (the right operand) */
	case 'a':	/* A1 = AL - constant */
		l = getlr(p, 'L');
		r = getlr(p, 'R');
		v = getlval(r);
		if (c == 'a')
			v = -v;
		if (l->n_op == REG && regno(l) == FPREG)
			v = fpoff(FPREG, v);
		if (r->n_name[0] != '\0') {
			char *rn = regname(regno(getlr(p, '1')), LONG);

			printf("\tadrp x17,%s\n\tadd %s,x17,:lo12:%s\n",
			    r->n_name, rn, r->n_name);
			printf("\tadd %s,%s,%s\n", rn, rn,
			    regname(regno(l), LONG));
			break;
		}
		addimm(regname(regno(getlr(p, '1')), LONG),
		    regname(regno(l), LONG), v);
		break;

	case 'G':	/* address of a global: A1 = &AL */
		l = getlr(p, 'L');
		printf("\tadrp %s,%s", regname(regno(getlr(p, '1')), LONG), l->n_name);
		if (getlval(l))
			printf("+%lld", getlval(l));
		printf("\n\tadd %s,%s,:lo12:%s", regname(regno(getlr(p, '1')), LONG),
		    regname(regno(getlr(p, '1')), LONG), l->n_name);
		if (getlval(l))
			printf("+%lld", getlval(l));
		printf("\n");
		break;

	case 'N':	/* the right constant, negated */
		printf("%lld", -getlval(getlr(p, 'R')));
		break;

	case 'L':	/* the right constant as a logical immediate */
		r = getlr(p, 'R');
		v = getlval(r);
		if (!wide(p->n_type))
			v &= 0xffffffffLL;
		printf("0x%llx", (unsigned long long)v);
		break;

	case 'V':	/* integer compare follows */
		fpcmp = 0;
		break;

	case 'F':	/* floating point compare follows */
		fpcmp = 1;
		fpinv = attr_find(p->n_ap, ATTR_FP_SWAPPED) != NULL;
		break;

	case 'S':	/* struct assignment, no value */
		stasg(p, 0);
		break;

	case 'B':	/* struct result through memory: x8 gets the bounce space */
		ap = attr_find(p->n_ap, ATTR_AARCH64_STRRET);
		if (ap == NULL || ap->iarg(0) == SC_MEM)
			addimm("x8", "x29", stkpos);
		break;

	case 'R':	/* x0 is made the address of the struct result: in the bounce space already (written through
		 * x8, which the standard does not ask the callee to return), or stored there from the registers */
		ap = attr_find(p->n_ap, ATTR_AARCH64_STRRET);
		if (ap == NULL || ap->iarg(0) == SC_MEM) {
			addimm("x0", "x29", stkpos);
			break;
		}
		addimm("x16", "x29", stkpos);
		for (i = 0; i < ap->iarg(2); i++) {
			if (ap->iarg(0) == SC_REG)
				printf("\tstr x%d,[x16,#%d]\n", i, i * 8);
			else if (ap->iarg(1) == FLOAT)
				printf("\tstr s%d,[x16,#%d]\n", i, i * 4);
			else
				printf("\tstr d%d,[x16,#%d]\n", i, i * 8);
		}
		printf("\tmov x0,x16\n");
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
	long val = getlval(p);

	switch (p->n_op) {
	case ICON:
		if (p->n_name[0] != '\0') {
			fprintf(fp, "%s", p->n_name);
			if (val)
				fprintf(fp, "+%ld", val);
		} else
			fprintf(fp, "%ld", val);
		return;

	default:
		comperr("illegal conput, p %p", p);
	}
}

/*
 * insput: the register of an operand in its 32-bit name (w0, s0).
 */
void
insput(NODE *p)
{
	if (p->n_op != REG)
		comperr("insput");
	printf("%s", regname(regno(p), p->n_type == FLOAT || p->n_type == DOUBLE ?
	    FLOAT : INT));
}

/*
 * upput: the register of an operand in its 64-bit name (x0, d0).
 */
void
upput(NODE *p, int size)
{
	if (p->n_op != REG)
		comperr("upput bad op %d size %d", p->n_op, size);
	printf("%s", regname(regno(p), p->n_type == FLOAT || p->n_type == DOUBLE ?
	    DOUBLE : LONG));
}

void
adrput(FILE *io, NODE *p)
{
	long off;
	int sz;

	switch (p->n_op) {

	case NAME:
		sz = memsz(p->n_type);
		if (p->n_name[0] != '\0') {
			fprintf(io, "[x16,:lo12:%s", p->n_name);
			if (getlval(p))
				fprintf(io, "+%lld", getlval(p));
			fprintf(io, "]");
		} else
			fprintf(io, "[x16]");
		return;

	case OREG:
		sz = memsz(p->n_type);
		if (R2TEST(regno(p)) || p->n_name[0] != '\0') {
			if (x2debug || c2debug || e2debug || o2debug || f2debug || s2debug || r2debug) {
				fprintf(io, "<OREG form r=%d name=%s>", regno(p), p->n_name);
				return;
			}
			comperr("adrput: OREG form r=%d name=%s lval=%lld", regno(p), p->n_name, getlval(p));
		}
		off = fpoff(regno(p), getlval(p));
		if (offok(off, sz))
			fprintf(io, "[%s,#%ld]", xreg(regno(p)), off);
		else
			fprintf(io, "[%s,x16]", xreg(regno(p)));
		return;

	case ICON:
		/* addressable value of the constant */
		fputc('#', io);
		conput(io, p);
		return;

	case REG:
		fprintf(io, "%s", regname(p->n_rval, p->n_type));
		return;

	default:
		comperr("illegal address, op %d, node %p", p->n_op, p);
		return;

	}
}

static char *
ccbranches[] = {
	"eq",		/* jumpe */
	"ne",		/* jumpn */
	"le",		/* jumple */
	"lt",		/* jumpl */
	"ge",		/* jumpge */
	"gt",		/* jumpg */
	"ls",		/* jumple (jlequ) */
	"lo",		/* jumpl (jlssu) */
	"hs",		/* jumpge (jgequ) */
	"hi",		/* jumpg (jgtru) */
};

/*
 * After fcmp an unordered result (a NaN) makes <, <=, > and >= false, and
 * == false and != true.  Pass 1 turns "if (a < b) s" into a jump over s on
 * the opposite condition (marked ATTR_FP_SWAPPED), which then has to be
 * true for unordered.  The order is EQ, NE, LE, LT, GE, GT.
 */
static char *
fpbranches[] = {
	"eq", "ne", "ls", "mi", "ge", "gt",
};
static char *
fpibranches[] = {
	"eq", "ne", "le", "lt", "pl", "hi",
};

/*   printf conditional and unconditional branches */
void
cbgen(int o, int lab)
{
	if (o < EQ || o > UGT)
		comperr("bad conditional branch: %s", opst[o]);
	if (fpcmp && o <= GT)
		printf("\tb.%s " LABFMT "\n",
		    (fpinv ? fpibranches : fpbranches)[o-EQ], lab);
	else
		printf("\tb.%s " LABFMT "\n", ccbranches[o-EQ], lab);
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
		ps = (ps + 15) & ~15;
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
		/* reserved after the locals: [stkpos, stkpos+bouncesz) */
		stkpos = (p2autooff + 7) & ~7;
		p2autooff = stkpos + bouncesz;
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

	if (t == FLOAT || t == DOUBLE || t == LDOUBLE)
		printf("\tfmov %s,%s\n", regname(d, t), regname(s, t));
	else
		printf("\tmov %s,%s\n", regname(d, t), regname(s, t));
}

/*
 * Can we allocate another register of class c, given that r[] are used?
 */
int
COLORMAP(int c, int *r)
{

	switch (c) {
	case CLASSA:
		return r[CLASSA] < 26;
	case CLASSB:
		return r[CLASSB] < 31;
	}
	return 0; /* XXX gcc */
}

char *rnames[MAXREGS] = {
	"x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
	"x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
	"x19", "x20", "x21", "x22", "x23", "x24", "x25", "x26",
	"x27", "x28", "x29", "sp",
	"d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
	"d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15",
	"d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23",
	"d24", "d25", "d26", "d27", "d28", "d29", "d30",
};

/*
 * Return a class suitable for a specific type.
 */
int
gclass(TWORD t)
{
	if (t == FLOAT || t == DOUBLE || t == LDOUBLE)
		return CLASSB;
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

	switch (shape) {
	case SFUNCALL:
		if (o == STCALL || o == USTCALL)
			return SRREG;
		break;
	case SIMM12:
		if (o != ICON || p->n_name[0])
			break;
		v = getlval(p);
		if (v >= 0 && v <= 4095)
			return SRDIR;
		break;
	case SNIMM12:
		if (o != ICON || p->n_name[0])
			break;
		v = getlval(p);
		if (v >= -4095 && v < 0)
			return SRDIR;
		break;
	case SLOGIMM:
		if (o != ICON || p->n_name[0])
			break;
		v = getlval(p);
		if (isbitmask((unsigned long long)v, 0))
			return SRDIR;
		break;
	case SLOGIMM64:
		if (o != ICON || p->n_name[0])
			break;
		v = getlval(p);
		if (isbitmask((unsigned long long)v, 1))
			return SRDIR;
		break;
	case SSH32:
		if (o != ICON || p->n_name[0])
			break;
		v = getlval(p);
		if (v >= 0 && v < 32)
			return SRDIR;
		break;
	case SSH64:
		if (o != ICON || p->n_name[0])
			break;
		v = getlval(p);
		if (v >= 0 && v < 64)
			return SRDIR;
		break;
	default:
		cerror("special: %x\n", shape);
	}
	return SRNOPE;
}

/*
 * Target-dependent command-line options.
 */
void
mflags(char *str)
{
}

/*
 * Do something target-dependent for xasm arguments.
 */
int
myxasm(struct interpass *ip, NODE *p)
{
	return 0;
}
