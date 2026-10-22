/*
 * peep.h -- a text optimizer for the pdp64 assembly of a whole file (-xdmr).  Included by code.c.
 *
 * The compiler keeps the assembly of the file in memory and hands it here when the job is over.  A run of
 * instructions and local labels (a function) is parsed into mnemonic and operands, each instruction with the
 * registers it reads and writes and whether it reads or writes the condition codes (N Z V C are one bit here).
 * An instruction not understood reads and writes everything and nothing moves across it.  Liveness is computed
 * over the labels and branches of the function.  The rewrites:
 *
 *	copy propagation		movq r0,r1; movq (r1),r2  ->  movq (r0),r2
 *	dead instructions		movq r0,r1 with r1 and the codes not used again: removed
 *	a result moved on		movq (r0),r1; movq r1,r2  ->  movq (r0),r2   (r1 dead afterwards)
 *	a tst after a move or a load	movq (r0),r1; tst r1; beq L  ->  movq (r0),r1; beq L
 *
 * A load is never removed (a volatile object is read by a load the compiler cannot tell from any other).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define	PP_MAXOP	3
#define	PP_FLAGS	(1u << 16)
#define	PP_ALL		0x1ffffu

enum { PK_OTHER, PK_LABEL, PK_INS, PK_DEAD };
enum { M_BARRIER, M_MOV, M_ALU2, M_CMP, M_UN, M_CLR, M_TST, M_CVT, M_DIV3, M_BR, M_CBR, M_CALL, M_RET, M_PUSH, M_POP };

struct pl {
	int kind;
	char *txt;		/* the line, without the newline */
	char mn[16];
	int cls;		/* M_* */
	int nops;
	char op[PP_MAXOP][96];
	unsigned rd, wr;	/* registers read and written (bit n is rn, PP_FLAGS the codes) */
	int store;		/* writes memory */
	int load;		/* reads memory (or a call, a barrier) */
	char target[64];	/* a branch's label */
	int lblidx;		/* the line a branch goes to, -1 if not in the region */
	int note;		/* a comment line to keep as it is */
	int outsz;		/* of a ret: the size of the outgoing argument area of its function */
	unsigned live;		/* live after this instruction */
	unsigned df, dl;	/* registers whose whole 64 bits (df) or low 32 bits (dl, a superset) are looked at later: after the instruction */
};

static int pp_retpair = 1;
static int pp_out = 1;		/* of the function being read: its outgoing argument area (unknown: 1) */

static struct {
	char *mn;
	int cls;
} pp_cls[] = {
	{ "movq", M_MOV }, { "movl", M_MOV }, { "movw", M_MOV }, { "movb", M_MOV }, { "movzl", M_MOV }, { "movzw", M_MOV },
	{ "movzb", M_MOV }, { "lea", M_MOV },
	{ "add", M_ALU2 }, { "sub", M_ALU2 }, { "and", M_ALU2 }, { "bis", M_ALU2 }, { "bic", M_ALU2 }, { "xor", M_ALU2 },
	{ "mul", M_ALU2 }, { "mulh", M_ALU2 }, { "shl", M_ALU2 }, { "shr", M_ALU2 }, { "sar", M_ALU2 },
	{ "fadd", M_ALU2 }, { "fsub", M_ALU2 }, { "fmul", M_ALU2 }, { "fadd32", M_ALU2 }, { "fsub32", M_ALU2 },
	{ "fmul32", M_ALU2 }, { "fdiv", M_ALU2 }, { "fdiv32", M_ALU2 },
	{ "cmp", M_CMP }, { "bit", M_CMP }, { "fcmp", M_CMP }, { "fcmp32", M_CMP },
	{ "inc", M_UN }, { "dec", M_UN }, { "neg", M_UN }, { "com", M_UN }, { "asl", M_UN }, { "asr", M_UN },
	{ "clr", M_CLR }, { "tst", M_TST },
	{ "fcvtdq", M_CVT }, { "fcvtqd", M_CVT }, { "fcvtds", M_CVT }, { "fcvtsd", M_CVT }, { "fcvtsl", M_CVT },
	{ "fcvtls", M_CVT }, { "fcvtdqu", M_CVT }, { "fcvtqud", M_CVT }, { "fneg", M_CVT },
	{ "div", M_DIV3 }, { "divu", M_DIV3 }, { "mod", M_DIV3 }, { "modu", M_DIV3 }, { "rol", M_DIV3 }, { "rori", M_DIV3 },
	{ "br", M_BR },
	{ "beq", M_CBR }, { "bne", M_CBR }, { "bmi", M_CBR }, { "bpl", M_CBR }, { "bvc", M_CBR }, { "bvs", M_CBR },
	{ "bcc", M_CBR }, { "bcs", M_CBR }, { "bge", M_CBR }, { "blt", M_CBR }, { "bgt", M_CBR }, { "ble", M_CBR },
	{ "bhi", M_CBR }, { "blos", M_CBR },
	{ "cbne", M_CBR }, { "cbeq", M_CBR }, { "cbpl", M_CBR }, { "cbmi", M_CBR }, { "cbvc", M_CBR }, { "cbvs", M_CBR }, { "cbcc", M_CBR },
	{ "cbcs", M_CBR }, { "cbge", M_CBR }, { "cblt", M_CBR }, { "cbgt", M_CBR }, { "cble", M_CBR }, { "cbhi", M_CBR }, { "cblos", M_CBR },
	{ "call", M_CALL }, { "calli", M_CALL }, { "ret", M_RET }, { "push", M_PUSH }, { "pop", M_POP },
	{ NULL, 0 }
};

/* the register named by the whole of s, or -1 */
static int
pp_reg(const char *s, int n)
{
	int v;

	if (n == 2 && s[0] == 's' && s[1] == 'p')
		return 14;
	if (n == 2 && s[0] == 'f' && s[1] == 'p')
		return 13;
	if (n >= 2 && n <= 3 && s[0] == 'r' && isdigit((unsigned char)s[1])) {
		v = atoi(s + 1);
		if (n == 3 && !isdigit((unsigned char)s[2]))
			return -1;
		return v <= 15 ? v : -1;
	}
	return -1;
}

/* the registers named anywhere in an operand (not in quotes, whole identifiers only) */
static unsigned
pp_regs(const char *s)
{
	unsigned m = 0;
	const char *p = s, *q;
	int r, quote = 0;

	while (*p) {
		if (*p == '"') {
			quote = !quote;
			p++;
			continue;
		}
		if (quote || !(isalpha((unsigned char)*p) || *p == '_' || *p == '.' || *p == '$')) {
			p++;
			continue;
		}
		for (q = p; isalnum((unsigned char)*q) || *q == '_' || *q == '.' || *q == '$'; q++)
			;
		if (p == s || (p[-1] != '#' && !isalnum((unsigned char)p[-1]))) {
			r = pp_reg(p, q - p);
			if (r >= 0)
				m |= 1u << r;
		}
		p = q;
	}
	return m;
}

static int
pp_isreg(const char *s)
{
	return pp_reg(s, strlen(s)) >= 0;
}

static void
pp_parse(struct pl *l)
{
	char *t = l->txt, *p, *e;
	int i, depth;
	unsigned r;

	l->kind = PK_OTHER;
	l->note = 0;
	l->cls = M_BARRIER;
	l->lblidx = -1;
	l->rd = l->wr = 0;
	l->store = l->load = 0;
	if (t[0] != '\t') {
		if (t[0] && t[strlen(t) - 1] == ':')
			l->kind = PK_LABEL;
		return;
	}
	if (t[1] == ';') {
		/* "; ret:one" or "; ret:pair", just before a ret: what the ret reads (written by eoftn) */
		l->kind = PK_DEAD;
		l->note = 1;
		if (strncmp(t, "\t; ret:", 7) == 0)
			pp_retpair = strncmp(t + 7, "pair", 4) == 0;
		if (strncmp(t, "\t; ret:", 7) == 0 && strstr(t, ",out=") != NULL)
			pp_out = atoi(strstr(t, ",out=") + 5);
		return;
	}
	if (t[1] == '.' || t[1] == 0)
		return;
	l->kind = PK_INS;
	p = t + 1;
	for (i = 0; *p && *p != ' ' && i < 15; p++)
		l->mn[i++] = *p;
	l->mn[i] = 0;
	while (*p == ' ')
		p++;
	l->nops = 0;
	depth = 0;
	e = l->op[0];
	for (; *p; p++) {
		if (*p == '(')
			depth++;
		else if (*p == ')')
			depth--;
		if (*p == ',' && depth == 0) {
			*e = 0;
			if (++l->nops >= PP_MAXOP) {
				l->cls = M_BARRIER;
				break;
			}
			e = l->op[l->nops];
			continue;
		}
		if (e < l->op[l->nops] + 94)
			*e++ = *p;
	}
	if (l->nops < PP_MAXOP) {
		*e = 0;
		if (l->op[l->nops][0])
			l->nops++;
	}
	for (i = 0; pp_cls[i].mn; i++)
		if (strcmp(pp_cls[i].mn, l->mn) == 0)
			l->cls = pp_cls[i].cls;
	/* auto-increment forms and the like: not understood */
	for (i = 0; i < l->nops; i++)
		if (strstr(l->op[i], ")+") != NULL || strstr(l->op[i], "-(") != NULL || strstr(l->op[i], ")-") != NULL)
			l->cls = M_BARRIER;
	if (l->cls == M_BARRIER)
		return;
	/* operand roles */
	switch (l->cls) {
	case M_MOV: case M_CVT:
		if (l->nops != 2) { l->cls = M_BARRIER; return; }
		if (l->mn[0] == 'l') {		/* lea: the address, not the contents */
			l->rd = pp_regs(l->op[0]);
		} else if (pp_isreg(l->op[0])) {
			l->rd = pp_regs(l->op[0]);
		} else if (l->op[0][0] == '#') {
			l->rd = 0;
		} else {
			l->rd = pp_regs(l->op[0]);
			l->load = 1;
		}
		if (pp_isreg(l->op[1]))
			l->wr = pp_regs(l->op[1]);
		else {
			l->rd |= pp_regs(l->op[1]);
			l->store = 1;
		}
		if (l->mn[0] != 'l')		/* lea leaves the codes alone */
			l->wr |= PP_FLAGS;
		break;
	case M_ALU2:
		if (l->nops != 2 || !pp_isreg(l->op[1])) { l->cls = M_BARRIER; return; }
		l->rd = pp_regs(l->op[0]) | pp_regs(l->op[1]);
		l->wr = pp_regs(l->op[1]) | PP_FLAGS;
		l->load = !pp_isreg(l->op[0]) && l->op[0][0] != '#';
		break;
	case M_CMP:
		if (l->nops != 2) { l->cls = M_BARRIER; return; }
		l->rd = pp_regs(l->op[0]) | pp_regs(l->op[1]);
		l->wr = PP_FLAGS;
		l->load = !pp_isreg(l->op[0]) && l->op[0][0] != '#' || !pp_isreg(l->op[1]) && l->op[1][0] != '#';
		break;
	case M_TST:
		if (l->nops != 1) { l->cls = M_BARRIER; return; }
		l->rd = pp_regs(l->op[0]);
		l->wr = PP_FLAGS;
		l->load = !pp_isreg(l->op[0]);
		break;
	case M_UN: case M_CLR:
		if (l->nops != 1) { l->cls = M_BARRIER; return; }
		r = pp_regs(l->op[0]);
		if (pp_isreg(l->op[0])) {
			l->rd = l->cls == M_CLR ? 0 : r;
			l->wr = r | PP_FLAGS;
		} else {
			l->rd = r;
			l->load = l->store = 1;
			l->wr = PP_FLAGS;
		}
		break;
	case M_DIV3:
		if (l->nops != 3) { l->cls = M_BARRIER; return; }
		l->rd = pp_regs(l->op[0]) | pp_regs(l->op[1]);
		l->wr = pp_regs(l->op[2]) | PP_FLAGS;
		break;
	case M_BR: case M_CBR:
		if (l->cls == M_CBR && l->mn[0] == 'c') {
			/* cbCC rd,#imm,label: a compare and a branch in one */
			if (l->nops != 3 || l->op[2][0] != '.' || pp_regs(l->op[2])) { l->cls = M_BARRIER; return; }
			strncpy(l->target, l->op[2], sizeof l->target - 1);
			l->target[sizeof l->target - 1] = 0;
			l->rd = pp_regs(l->op[0]) | PP_FLAGS;
			l->wr = PP_FLAGS;
			break;
		}
		if (l->nops != 1 || l->op[0][0] != '.' || pp_regs(l->op[0])) {
			if (l->cls == M_CBR) { l->cls = M_BARRIER; return; }
			l->rd = pp_regs(l->op[0]);	/* an indirect jump leaves the function */
			l->cls = M_RET;
			l->rd |= PP_ALL;
			return;
		}
		strncpy(l->target, l->op[0], sizeof l->target - 1);
		l->target[sizeof l->target - 1] = 0;
		l->rd = l->cls == M_CBR ? PP_FLAGS : 0;
		break;
	case M_CALL:
		l->rd = 0x3f | (1u << 14);		/* the arguments, and the stack */
		for (i = 0; i < l->nops; i++)
			l->rd |= pp_regs(l->op[i]);
		l->wr = 0x10ffu | PP_FLAGS;	/* r0-r7, r12 */
		l->load = l->store = 1;
		break;
	case M_RET:
		l->rd = 0x1 | (pp_retpair ? 0x2 : 0) | (0xfu << 8) | (1u << 15) | (3u << 13);
		pp_retpair = 1;
		l->outsz = pp_out;
		pp_out = 1;
		break;
	case M_PUSH:
		l->rd = pp_regs(l->op[0]) | (1u << 14);
		l->wr = 1u << 14;
		l->store = 1;
		break;
	case M_POP:
		l->rd = 1u << 14;
		l->wr = pp_regs(l->op[0]) | (1u << 14);
		l->load = 1;
		break;
	}
}

/* is this a plain-register operand position that holds only a source? */
static int
pp_replace(char *op, int from, int to)
{
	/* replace the register `from` by `to` as a whole token in op; 1 if anything changed */
	char out[128], *o = out;
	const char *p = op, *q;
	int r, quote = 0, ch = 0;

	while (*p) {
		if (*p == '"')
			quote = !quote;
		if (!quote && (isalpha((unsigned char)*p) || *p == '_' || *p == '.' || *p == '$') &&
		    (p == op || (p[-1] != '#' && !isalnum((unsigned char)p[-1]) && p[-1] != '_' && p[-1] != '.' && p[-1] != '$'))) {
			for (q = p; isalnum((unsigned char)*q) || *q == '_' || *q == '.' || *q == '$'; q++)
				;
			r = pp_reg(p, q - p);
			if (r == from) {
				o += sprintf(o, "r%d", to);
				p = q;
				ch = 1;
				continue;
			}
			while (p < q)
				*o++ = *p++;
			continue;
		}
		*o++ = *p++;
	}
	*o = 0;
	if (ch)
		strcpy(op, out);
	return ch;
}

static void
pp_unparse(struct pl *l)
{
	char buf[400];
	int i;

	strcpy(buf, "\t");
	strcat(buf, l->mn);
	for (i = 0; i < l->nops; i++) {
		strcat(buf, i ? "," : " ");
		strcat(buf, l->op[i]);
	}
	l->txt = strdup(buf);
	pp_parse(l);
}

/* may the register be renamed (not sp, fp, the scratch r12) */
#define	PP_REN(r)	((r) <= 11 || (r) == 15)

/* live-in of the instruction or label at line j (j < b) */
static unsigned
pp_in(struct pl *L, int j)
{
	struct pl *l = &L[j];

	if (l->kind == PK_LABEL)
		return l->live;
	if (l->cls == M_BARRIER)
		return PP_ALL;
	return (l->live & ~l->wr) | l->rd;
}

/* liveness over one region [a, b) of lines: .live of an instruction is what is live after it, of a label what is live at it */
static void
pp_live(struct pl *L, int a, int b)
{
	int i, j, k, ch;
	unsigned out;

	for (i = a; i < b; i++)
		if (L[i].kind == PK_INS && (L[i].cls == M_BR || L[i].cls == M_CBR)) {
			L[i].lblidx = -1;
			for (j = a; j < b; j++)
				if (L[j].kind == PK_LABEL && strncmp(L[j].txt, L[i].target, strlen(L[i].target)) == 0 &&
				    L[j].txt[strlen(L[i].target)] == ':') {
					L[i].lblidx = j;
					break;
				}
		}
	for (i = a; i < b; i++)
		L[i].live = 0;
	do {
		ch = 0;
		for (i = b - 1; i >= a; i--) {
			if (L[i].kind == PK_DEAD)
				continue;
			for (k = i + 1; k < b && L[k].kind == PK_DEAD; k++)
				;
			if (L[i].kind == PK_LABEL) {
				out = k < b ? pp_in(L, k) : PP_ALL;
			} else if (L[i].kind == PK_INS) {
				if (L[i].cls == M_BARRIER)
					out = PP_ALL;
				else if (L[i].cls == M_RET)
					out = 0;
				else {
					out = 0;
					if (L[i].cls != M_BR)
						out = k < b ? pp_in(L, k) : PP_ALL;
					if (L[i].cls == M_BR || L[i].cls == M_CBR)
						out |= L[i].lblidx < 0 ? PP_ALL : L[L[i].lblidx].live;
				}
			} else
				continue;
			if (L[i].live != out) {
				L[i].live = out;
				ch = 1;
			}
		}
	} while (ch);
}


/*
 * Demand analysis.  An integer value of 32 bits is kept sign-extended in a register, and the arithmetic of the compiler
 * extends the result of an unsigned operation again ("movl r,r").  The extension matters only where the upper bits
 * are looked at: a compare, a right shift, a division, an address, a call or a return, a store of 64 bits; adding,
 * subtracting, multiplying, shifting left, and, or and xor, a move, a narrow store and an extension itself look at the
 * low 32 bits only and pass what they are asked for on to their operands.  A "movl r,r" or "movzl r,r" after which
 * the register is looked at in its low 32 bits only is dead.
 */
#define	PP_REGS	0xffffu
#define	PP_B(r)	(1u << (r))

static int
pp_lvl(unsigned df, unsigned dl, int r)
{
	return (df >> r & 1) ? 2 : (dl >> r & 1) ? 1 : 0;
}

/* give register r (a bit) the demand level lv */
#define	PP_DEM(f, g, r, lv)	do { if ((lv) == 2) { (f) |= PP_B(r); (g) |= PP_B(r); } else if ((lv) == 1) (g) |= PP_B(r); } while (0)

static void
pp_xfer(struct pl *l, unsigned dfo, unsigned dlo, unsigned *dfi, unsigned *dli)
{
	unsigned f, g, m;
	int d, sr, lv;
	char *mn = l->mn;

	if (l->kind == PK_LABEL) {
		*dfi = l->df;
		*dli = l->dl;
		return;
	}
	if (l->cls == M_BARRIER) {
		*dfi = *dli = PP_REGS;
		return;
	}
	f = dfo & ~l->wr & PP_REGS;
	g = dlo & ~l->wr & PP_REGS;
	/* the default: everything read is looked at in full */
	m = l->rd & PP_REGS;
	switch (l->cls) {
	case M_MOV:
		if (l->nops != 2)
			break;
		if (pp_isreg(l->op[1])) {
			d = pp_reg(l->op[1], strlen(l->op[1]));
			lv = pp_lvl(dfo, dlo, d);
			if (pp_isreg(l->op[0]) && mn[0] == 'm') {
				sr = pp_reg(l->op[0], strlen(l->op[0]));
				m &= ~PP_B(sr);
				if (strcmp(mn, "movq") == 0)
					PP_DEM(f, g, sr, lv);
				else if (lv)
					g |= PP_B(sr);
			} else if (mn[0] == 'l' && !pp_isreg(l->op[0])) {	/* lea: low bits of the address from low bits of the registers */
				unsigned a = pp_regs(l->op[0]);

				m &= ~a;
				if (lv == 2) { f |= a; g |= a; } else if (lv == 1) g |= a;
			}
		} else if (pp_isreg(l->op[0]) && mn[0] == 'm') {		/* a store */
			sr = pp_reg(l->op[0], strlen(l->op[0]));
			m &= ~PP_B(sr);
			if (strcmp(mn, "movq") == 0) {
				f |= PP_B(sr);
				g |= PP_B(sr);
			} else
				g |= PP_B(sr);
			m |= pp_regs(l->op[1]);
		}
		break;
	case M_ALU2:
		if (l->nops != 2 || !pp_isreg(l->op[1]))
			break;
		if (strcmp(mn, "add") == 0 || strcmp(mn, "sub") == 0 || strcmp(mn, "and") == 0 || strcmp(mn, "bis") == 0 ||
		    strcmp(mn, "bic") == 0 || strcmp(mn, "xor") == 0 || strcmp(mn, "mul") == 0 || strcmp(mn, "shl") == 0) {
			d = pp_reg(l->op[1], strlen(l->op[1]));
			lv = pp_lvl(dfo, dlo, d);
			m &= ~PP_B(d);
			PP_DEM(f, g, d, lv);
			if (pp_isreg(l->op[0])) {
				sr = pp_reg(l->op[0], strlen(l->op[0]));
				if (strcmp(mn, "shl") != 0) {
					m &= ~PP_B(sr);
					PP_DEM(f, g, sr, lv);
				}
			}
		}
		break;
	case M_UN:
		if (l->nops == 1 && pp_isreg(l->op[0]) &&
		    (strcmp(mn, "neg") == 0 || strcmp(mn, "com") == 0 || strcmp(mn, "inc") == 0 || strcmp(mn, "dec") == 0 ||
		    strcmp(mn, "asl") == 0)) {
			d = pp_reg(l->op[0], strlen(l->op[0]));
			lv = pp_lvl(dfo, dlo, d);
			m &= ~PP_B(d);
			PP_DEM(f, g, d, lv);
		}
		break;
	}
	*dfi = f | m;
	*dli = g | m;
}

static void
pp_demand(struct pl *L, int a, int b)
{
	int i, k, ch;
	unsigned fo, go, fi, gi;

	for (i = a; i < b; i++)
		L[i].df = L[i].dl = 0;
	do {
		ch = 0;
		for (i = b - 1; i >= a; i--) {
			if (L[i].kind == PK_DEAD || L[i].kind == PK_OTHER)
				continue;
			for (k = i + 1; k < b && L[k].kind == PK_DEAD; k++)
				;
			if (L[i].kind == PK_LABEL) {
				if (k < b)
					pp_xfer(&L[k], L[k].df, L[k].dl, &fi, &gi);
				else
					fi = gi = PP_REGS;
				if (L[i].df != fi || L[i].dl != gi) {
					L[i].df = fi;
					L[i].dl = gi;
					ch = 1;
				}
				continue;
			}
			/* an instruction: what is demanded after it */
			fo = go = 0;
			if (L[i].cls == M_BARRIER)
				fo = go = PP_REGS;
			else if (L[i].cls != M_RET) {
				if (L[i].cls != M_BR) {
					if (k < b)
						pp_xfer(&L[k], L[k].df, L[k].dl, &fo, &go);
					else
						fo = go = PP_REGS;
				}
				if (L[i].cls == M_BR || L[i].cls == M_CBR) {
					if (L[i].lblidx < 0)
						fo = go = PP_REGS;
					else {
						fo |= L[L[i].lblidx].df;
						go |= L[L[i].lblidx].dl;
					}
				}
			}
			if (L[i].df != fo || L[i].dl != go) {
				L[i].df = fo;
				L[i].dl = go;
				ch = 1;
			}
		}
	} while (ch);
}

static int
pp_region(struct pl *L, int a, int b)
{
	int i, j, k, changed = 0, pass;

	for (pass = 0; pass < 6; pass++) {
		int ch = 0;

		pp_live(L, a, b);
		for (i = a; i < b; i++) {
			struct pl *l = &L[i];

			if (l->kind != PK_INS)
				continue;
			/* a pure instruction whose results nobody reads */
			if ((l->cls == M_MOV || l->cls == M_CLR || l->cls == M_ALU2 || l->cls == M_UN || l->cls == M_CVT) &&
			    !l->load && !l->store && (l->wr & ~PP_FLAGS) != 0 && (l->wr & (3u << 13)) == 0 &&
			    (l->wr & l->live) == 0) {
				l->kind = PK_DEAD;
				ch = 1;
				continue;
			}
			/* lea d(rX),rX with the codes dead afterwards: inc, dec or add #d (two bytes for -8..7) */
			if (l->cls == M_MOV && strcmp(l->mn, "lea") == 0 && l->nops == 2 && pp_isreg(l->op[1]) && !(l->live & PP_FLAGS)) {
				char *x = l->op[1], *e;
				long d = strtol(l->op[0], &e, 10);

				if (e != l->op[0] && *e == '(' && strncmp(e + 1, x, strlen(x)) == 0 && strcmp(e + 1 + strlen(x), ")") == 0 &&
				    d >= -8 && d <= 7 && d != 0) {
					char xr[24];

					strcpy(xr, x);
					if (d == 1)
						strcpy(l->mn, "inc");
					else if (d == -1)
						strcpy(l->mn, "dec");
					else {
						strcpy(l->mn, "add");
						sprintf(l->op[0], "#%ld", d);
						strcpy(l->op[1], xr);
						l->nops = 2;
						pp_unparse(l);
						ch = 1;
						continue;
					}
					strcpy(l->op[0], xr);
					l->nops = 1;
					pp_unparse(l);
					ch = 1;
					continue;
				}
			}
			/* movq rA,rB: use rA where rB is read, while neither changes */
			if (l->cls == M_MOV && strcmp(l->mn, "movq") == 0 && pp_isreg(l->op[0]) && pp_isreg(l->op[1])) {
				int ra = pp_reg(l->op[0], strlen(l->op[0])), rb = pp_reg(l->op[1], strlen(l->op[1]));

				if (ra == rb || !PP_REN(ra) || !PP_REN(rb))
					continue;
				for (j = i + 1; j < b; j++) {
					struct pl *m = &L[j];

					if (m->kind == PK_DEAD)
						continue;
					if (m->kind != PK_INS)
						break;
					if (m->cls == M_BARRIER || m->cls == M_CALL || m->cls == M_RET || m->cls == M_BR || m->cls == M_CBR ||
					    m->cls == M_PUSH || m->cls == M_POP)
						break;
					if (m->rd & (1u << rb)) {
						/* which operands may take rA: sources, and any register inside an address */
						int nch = 0;

						for (k = 0; k < m->nops; k++) {
							int isdst = (k == m->nops - 1) &&
							    (m->cls == M_ALU2 || m->cls == M_MOV || m->cls == M_CVT || m->cls == M_UN ||
							    m->cls == M_CLR || m->cls == M_DIV3 ? m->cls != M_DIV3 || k == 2 : 0) &&
							    pp_isreg(m->op[k]);

							if (m->cls == M_CMP || m->cls == M_TST)
								isdst = 0;
							if (m->cls == M_ALU2 && k == 1)
								isdst = 1;
							if (isdst)
								continue;
							nch += pp_replace(m->op[k], rb, ra);
						}
						if (nch) {
							pp_unparse(m);
							ch = 1;
						}
					}
					if (m->wr & ((1u << ra) | (1u << rb)))
						break;
				}
				continue;
			}
		}
		/* a result moved on:  op ...,rA; movq rA,rB with rA dead afterwards  ->  op ...,rB  (rA is only written by op) */
		pp_live(L, a, b);
		for (i = a; i < b; i++) {
			struct pl *l = &L[i], *m;

			if (l->kind != PK_INS || l->cls != M_MOV || l->nops != 2 || !pp_isreg(l->op[1]))
				continue;
			if (l->mn[0] == 'l')
				continue;		/* lea may not set the codes as a move does */
			for (j = i + 1; j < b && L[j].kind == PK_DEAD; j++)
				;
			if (j >= b || L[j].kind != PK_INS)
				continue;
			m = &L[j];
			if (m->cls == M_MOV && strcmp(m->mn, "movq") == 0 && m->nops == 2 && pp_isreg(m->op[0]) && pp_isreg(m->op[1]) &&
			    strcmp(m->op[0], l->op[1]) == 0 && strcmp(m->op[1], m->op[0]) != 0) {
				int ra = pp_reg(m->op[0], strlen(m->op[0])), rb = pp_reg(m->op[1], strlen(m->op[1]));

				if (!PP_REN(ra) || !PP_REN(rb))
					continue;
				if (m->live & (1u << ra))
					continue;		/* rA is read again */
				if (l->rd & (1u << rb))
					continue;		/* the first instruction reads rB (it would read the new value) */
				if (l->store)
					continue;
				strcpy(l->op[1], m->op[1]);
				pp_unparse(l);
				m->kind = PK_DEAD;
				ch = 1;
			}
		}
		/* call f; <epilogue>; ret  ->  <epilogue>; jmp f   (the arguments are in registers, the epilogue leaves them alone) */
		for (i = a; i < b; i++) {
			struct pl *l = &L[i];

			if (l->kind != PK_INS || l->cls != M_CALL || strcmp(l->mn, "call") != 0 || l->nops != 1 || strchr(l->op[0], '(') != NULL ||
			    pp_isreg(l->op[0]) || l->op[0][0] == '@')
				continue;
			for (j = i + 1; j < b; j++) {
				struct pl *m = &L[j];

				if (m->kind == PK_DEAD)
					continue;
				if (m->kind != PK_INS)
					break;
				if (m->cls == M_RET && strcmp(m->mn, "ret") == 0)
					break;
				if (strcmp(m->mn, "popm") == 0 || strcmp(m->mn, "pop") == 0 || (strcmp(m->mn, "add") == 0 && m->nops == 2 && strcmp(m->op[1], "sp") == 0) ||
				    (strcmp(m->mn, "movq") == 0 && strcmp(m->op[0], "fp") == 0 && strcmp(m->op[1], "sp") == 0) ||
				    (strcmp(m->mn, "lea") == 0 && m->nops == 2 && strcmp(m->op[1], "sp") == 0 && strstr(m->op[0], "(fp)") != NULL))
					continue;
				break;
			}
			if (j < b && L[j].kind == PK_INS && L[j].cls == M_RET && strcmp(L[j].mn, "ret") == 0 && L[j].outsz == 0) {
				/* r12 is not a callee's concern, but a pop of it (the variadic epilogue) is not allowed */
				int k, ok = 1;

				for (k = i + 1; k < j; k++)
					if (L[k].kind == PK_INS && strstr(L[k].txt, "r12") != NULL)
						ok = 0;
				/* the frame goes away: nothing may hold an address in it (a local passed on, a copy of sp or fp) */
				for (k = a; k < b; k++)
					if (L[k].kind == PK_INS && L[k].cls != M_BARRIER &&
					    ((strcmp(L[k].mn, "lea") == 0 && (strstr(L[k].op[0], "(fp)") != NULL || strstr(L[k].op[0], "(sp)") != NULL) &&
					    strcmp(L[k].op[1], "sp") != 0) ||
					    (strcmp(L[k].mn, "movq") == 0 && L[k].nops == 2 && (strcmp(L[k].op[0], "sp") == 0 || strcmp(L[k].op[0], "fp") == 0) &&
					    strcmp(L[k].op[1], "fp") != 0)))
						ok = 0;
				if (!ok)
					continue;
				strcpy(L[j].mn, "jmp");
				strcpy(L[j].op[0], l->op[0]);
				L[j].nops = 1;
				pp_unparse(&L[j]);
				l->kind = PK_DEAD;
				ch = 1;
			}
		}
		/* movq rA,rB; movq rB,rA: the second changes nothing */
		for (i = a; i < b; i++) {
			struct pl *l = &L[i], *m;

			if (l->kind != PK_INS || l->cls != M_MOV || strcmp(l->mn, "movq") != 0 || !pp_isreg(l->op[0]) || !pp_isreg(l->op[1]))
				continue;
			for (j = i + 1; j < b && L[j].kind == PK_DEAD; j++)
				;
			if (j >= b || L[j].kind != PK_INS)
				continue;
			m = &L[j];
			if (m->cls == M_MOV && strcmp(m->mn, "movq") == 0 && strcmp(m->op[0], l->op[1]) == 0 && strcmp(m->op[1], l->op[0]) == 0 &&
			    !(m->live & PP_FLAGS)) {
				m->kind = PK_DEAD;
				ch = 1;
			}
		}
		/* an extension that nobody looks at */
		pp_demand(L, a, b);
		if (getenv("PDP64_PEEPDEBUG"))
			for (i = a; i < b; i++)
				fprintf(stderr, "%-28s df=%04x dl=%04x live=%05x cls=%d\n", L[i].txt, L[i].df, L[i].dl, L[i].live, L[i].cls);
		for (i = a; i < b; i++) {
			struct pl *l = &L[i];

			if (l->kind == PK_INS && l->cls == M_MOV && l->nops == 2 && (strcmp(l->mn, "movl") == 0 || strcmp(l->mn, "movzl") == 0) &&
			    pp_isreg(l->op[0]) && strcmp(l->op[0], l->op[1]) == 0 && !(l->live & PP_FLAGS)) {
				int r = pp_reg(l->op[0], strlen(l->op[0]));

				if (pp_lvl(l->df, l->dl, r) < 2) {
					l->kind = PK_DEAD;
					ch = 1;
				}
			}
		}
		/* a tst of the register a move or load has just written, in front of a branch on N, Z and V */
		for (i = a; i < b; i++) {
			struct pl *l = &L[i], *p = NULL, *n = NULL;

			if (l->kind != PK_INS || l->cls != M_TST || !pp_isreg(l->op[0]))
				continue;
			for (j = i - 1; j >= a && L[j].kind == PK_DEAD; j--)
				;
			if (j >= a && L[j].kind == PK_INS)
				p = &L[j];
			for (j = i + 1; j < b && L[j].kind == PK_DEAD; j++)
				;
			if (j < b && L[j].kind == PK_INS)
				n = &L[j];
			if (p && n && p->cls == M_MOV && p->mn[0] == 'm' && !p->store && pp_isreg(p->op[1]) &&
			    strcmp(p->op[1], l->op[0]) == 0 && n->cls == M_CBR &&
			    (strcmp(n->mn, "beq") == 0 || strcmp(n->mn, "bne") == 0 || strcmp(n->mn, "bmi") == 0 ||
			    strcmp(n->mn, "bpl") == 0 || strcmp(n->mn, "blt") == 0 || strcmp(n->mn, "bge") == 0 ||
			    strcmp(n->mn, "ble") == 0 || strcmp(n->mn, "bgt") == 0)) {
				l->kind = PK_DEAD;
				ch = 1;
			}
		}
		/* cmp rX,#imm ; bCC L  ->  cbCC rX,#imm,L  (imm -16..15, a label of this section); tst rX ; bCC L  ->  cbCC rX,#0,L */
		for (i = a; i < b; i++) {
			struct pl *n = &L[i], *p = NULL;

			if (n->kind != PK_INS || n->cls != M_CBR || n->mn[0] != 'b' || n->nops != 1 || strncmp(n->op[0], ".L", 2) != 0)
				continue;
			for (j = i - 1; j >= a && L[j].kind == PK_DEAD; j--)
				;
			if (j >= a && L[j].kind == PK_INS)
				p = &L[j];
			if (p == NULL)
				continue;
			if (p->cls == M_CMP && strcmp(p->mn, "cmp") == 0 && p->nops == 2 && pp_isreg(p->op[0]) && p->op[1][0] == '#') {
				long v = strtol(p->op[1] + 1, NULL, 10);

				if (v < -16 || v > 15)
					continue;
				strcpy(n->op[2], n->op[0]);
				strcpy(n->op[0], p->op[0]);
				strcpy(n->op[1], p->op[1]);
			} else if (p->cls == M_TST && p->nops == 1 && pp_isreg(p->op[0])) {
				strcpy(n->op[2], n->op[0]);
				strcpy(n->op[0], p->op[0]);
				strcpy(n->op[1], "#0");
			} else
				continue;
			memmove(n->mn + 1, n->mn, strlen(n->mn) + 1);
			n->mn[0] = 'c';
			n->nops = 3;
			p->kind = PK_DEAD;
			pp_unparse(n);
			ch = 1;
		}
		if (!ch)
			break;
		changed = 1;
	}
	return changed;
}

static void
pdp64_peep(char *buf, FILE *out)
{
	struct pl *L;
	char *p;
	int n = 0, i, a;

	for (p = buf; *p; p++)
		if (*p == '\n')
			n++;
	L = calloc(n + 2, sizeof(*L));
	i = 0;
	for (p = buf; *p; ) {
		char *nl = strchr(p, '\n');

		if (nl == NULL)
			nl = p + strlen(p);
		L[i].txt = malloc(nl - p + 1);
		memcpy(L[i].txt, p, nl - p);
		L[i].txt[nl - p] = 0;
		pp_parse(&L[i]);
		i++;
		p = *nl ? nl + 1 : nl;
	}
	n = i;
	/* regions: runs of instructions and labels */
	for (a = 0; a < n; ) {
		int b;

		if (L[a].kind == PK_OTHER) {
			a++;
			continue;
		}
		for (b = a; b < n && L[b].kind != PK_OTHER; b++)
			;
		pp_region(L, a, b);
		a = b;
	}
	for (i = 0; i < n; i++)
		if (L[i].kind != PK_DEAD || L[i].note)
			fprintf(out, "%s\n", L[i].txt);
}
