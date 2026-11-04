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
 *
 * A load is never removed (a volatile object is read by a load the compiler cannot tell from any other).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define	PP_MAXOP	3
typedef unsigned long long ppm;
#define	PP_FLAGS	(1ull << 40)
#define	PP_ALL		((1ull << 41) - 1)

enum { PK_OTHER, PK_LABEL, PK_INS, PK_DEAD };
enum { M_BARRIER, M_MOV, M_ALU2, M_CMP, M_UN, M_CLR, M_TST, M_CVT, M_DIV3, M_BR, M_CBR, M_CALL, M_RET, M_PUSH, M_POP };

struct pl {
	int kind;
	char *txt;		/* the line, without the newline */
	char mn[16];
	int cls;		/* M_* */
	int nops;
	char op[PP_MAXOP][96];
	ppm rd, wr;	/* registers read and written (bit n is rn, PP_FLAGS the codes) */
	int store;		/* writes memory */
	int load;		/* reads memory (or a call, a barrier) */
	char target[64];	/* a branch's label */
	int lblidx;		/* the line a branch goes to, -1 if not in the region */
	int note;		/* a comment line to keep as it is */
	int callargs;		/* of a call: the argument registers it passes */
	int outsz;		/* of a ret: the size of the outgoing argument area of its function */
	ppm live;		/* live after this instruction */
	ppm df, dl;	/* registers whose whole 64 bits (df) or low 32 bits (dl, a superset) are looked at later: after the instruction */
};

static int pp_retpair = 1;
static int pp_args = NARGREGS;	/* of the next call: the argument registers it passes ("; args:N", written by zzzcode G) */
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
	{ "div", M_DIV3 }, { "divu", M_DIV3 }, { "mod", M_DIV3 }, { "modu", M_DIV3 }, { "rol", M_DIV3 }, { "rori", M_DIV3 }, { "rori32", M_DIV3 },
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
		return v <= 31 ? v : -1;
	}
	return -1;
}

/* the registers named anywhere in an operand (not in quotes, whole identifiers only) */
static ppm
pp_regs(const char *s)
{
	ppm m = 0;
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
				m |= 1ull << r;
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
	ppm r;

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
		if (strncmp(t, "\t; vol", 6) == 0)
			l->note = 0;
		if (strncmp(t, "\t; args:", 8) == 0) {
			pp_args = atoi(t + 8);
			l->note = 0;		/* a note for this pass only */
		}
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
	/* pushm and popm: a mask of registers, bit n is rn */
	if (pdp64_target != PDP64_R20 && (strcmp(l->mn, "pushm") == 0 || strcmp(l->mn, "popm") == 0) && l->nops == 1 && l->op[0][0] == '#')
		l->cls = l->mn[1] == 'u' ? M_PUSH : M_POP;
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
		/* a move, a load, a store, lea: no codes written (errata 39; before it a move wrote N and Z). The liveness model
		 * neither lets one kill the codes of an earlier compare nor use any: right under both rules. */
		break;
	case M_ALU2:
		if (l->nops != 2 || !pp_isreg(l->op[1])) { l->cls = M_BARRIER; return; }
		l->rd = pp_regs(l->op[0]) | pp_regs(l->op[1]);
		l->wr = pp_regs(l->op[1]);
		/* only what writes the codes in the old and in the new rules kills them: a shift by a register count wrote nothing for a count of
		 * 0 (it writes the same for all counts now); multiplies, floating-point and rotates are not relied on either way */
		if (strcmp(l->mn, "add") == 0 || strcmp(l->mn, "sub") == 0 || strcmp(l->mn, "and") == 0 || strcmp(l->mn, "bis") == 0 ||
		    strcmp(l->mn, "bic") == 0 || strcmp(l->mn, "xor") == 0 ||
		    ((strcmp(l->mn, "shl") == 0 || strcmp(l->mn, "shr") == 0 || strcmp(l->mn, "sar") == 0) && l->op[0][0] == '#' &&
		    strcmp(l->op[0], "#0") != 0))
			l->wr |= PP_FLAGS;
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
			l->wr = r | (l->cls == M_UN ? PP_FLAGS : 0);	/* clr: not relied on */
		} else {
			l->rd = r;
			l->load = l->store = 1;
			l->wr = l->cls == M_UN ? PP_FLAGS : 0;
		}
		break;
	case M_DIV3:
		if (l->nops != 3) { l->cls = M_BARRIER; return; }
		l->rd = pp_regs(l->op[0]) | pp_regs(l->op[1]);
		l->wr = pp_regs(l->op[2]);		/* div, mod, rol, rori32...: no codes relied on */
		break;
	case M_BR: case M_CBR:
		if (l->cls == M_CBR && l->mn[0] == 'c') {
			/* cbCC rd,#imm,label: a compare and a branch in one */
			if (l->nops != 3 || l->op[2][0] != '.' || pp_regs(l->op[2])) { l->cls = M_BARRIER; return; }
			strncpy(l->target, l->op[2], sizeof l->target - 1);
			l->target[sizeof l->target - 1] = 0;
			l->rd = pp_regs(l->op[0]) | pp_regs(l->op[1]);	/* it compares for itself ... */
			l->wr = 0;					/* ... and writes no codes (errata 39; before it, as cmp did) */
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
		l->callargs = pp_args;
		l->rd = ((1ull << pp_args) - 1) | (1ull << 14);		/* the arguments, and the stack */
		pp_args = NARGREGS;
		for (i = 0; i < l->nops; i++)
			l->rd |= pp_regs(l->op[i]);
		l->wr = 0x10ffull | 0xff0000ull | PP_FLAGS;	/* r0-r7, r12 */
		l->load = l->store = 1;
		break;
	case M_RET:
		l->rd = 0x1 | (pp_retpair ? 0x2 : 0) | (0xfull << 8) | (1ull << 15) | (3ull << 13) | 0xff000000ull;
		pp_retpair = 1;
		l->outsz = pp_out;
		pp_out = 1;
		break;
	case M_PUSH:
		l->rd = pp_regs(l->op[0]) | (1ull << 14);
		if (l->mn[3] == 'h' && l->mn[4] == 'm')
			l->rd = (strtoull(l->op[0] + 1, NULL, 0) & 0xffffffffull) | (1ull << 14);
		l->wr = 1ull << 14;
		l->store = 1;
		break;
	case M_POP:
		l->rd = 1ull << 14;
		l->wr = pp_regs(l->op[0]) | (1ull << 14);
		if (l->mn[2] == 'p' && l->mn[3] == 'm')
			l->wr = (strtoull(l->op[0] + 1, NULL, 0) & 0xffffffffull) | (1ull << 14);
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
	pp_args = l->callargs ? l->callargs : l->cls == M_CALL ? 0 : NARGREGS;
	pp_parse(l);
}

/* may the register be renamed (not sp, fp, the scratch r12) */
#define	PP_REN(r)	((r) <= 11 || (r) == 15 || (r) >= 16)

/* live-in of the instruction or label at line j (j < b) */
static ppm
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
	ppm out;

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
#define	PP_REGS	0xffffffffull
#define	PP_B(r)	(1ull << (r))

static int
pp_lvl(ppm df, ppm dl, int r)
{
	return (df >> r & 1) ? 2 : (dl >> r & 1) ? 1 : 0;
}

/* give register r (a bit) the demand level lv */
#define	PP_DEM(f, g, r, lv)	do { if ((lv) == 2) { (f) |= PP_B(r); (g) |= PP_B(r); } else if ((lv) == 1) (g) |= PP_B(r); } while (0)

static void
pp_xfer(struct pl *l, ppm dfo, ppm dlo, ppm *dfi, ppm *dli)
{
	ppm f, g, m;
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
				ppm a = pp_regs(l->op[0]);

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
	case M_DIV3:
		/* rori32 looks at the low 32 bits of its source only */
		if (strcmp(mn, "rori32") == 0 && l->nops == 3 && pp_isreg(l->op[0])) {
			sr = pp_reg(l->op[0], strlen(l->op[0]));
			m &= ~PP_B(sr);
			g |= PP_B(sr);
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
	ppm fo, go, fi, gi;

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


/*
 * Values known in a basic block (-m no-avail turns it off).  Each register has a key naming the value it holds: a constant,
 * the address "lea S", the load "mn M", or the key of the register it was copied from; the same key in the register that
 * is to receive it again makes the instruction redundant, and in another register, a register move (a quarter the size of a
 * lea of a symbol, or of a load).  A write to a register ends the keys that name it or were made from it; a store, a call or
 * anything not understood ends the loads; a label ends everything (it may be reached some other way).  A function with a
 * volatile access ("; vol", written by eoftn) keeps all its loads.
 */
#define	PP_NREG	32

struct av {
	char *key[PP_NREG];
	ppm dep[PP_NREG];		/* the registers the key's value was computed from */
	int fresh;
};

static void
av_clear(struct av *v)
{
	int i;

	for (i = 0; i < PP_NREG; i++)
		v->key[i] = NULL, v->dep[i] = 0;
}

static void
av_kill(struct av *v, ppm w)
{
	int i;

	for (i = 0; i < PP_NREG; i++)
		if (v->key[i] != NULL && ((w >> i & 1) || (v->dep[i] & w)))
			v->key[i] = NULL;
}

static void
av_killmem(struct av *v)
{
	int i;

	for (i = 0; i < PP_NREG; i++)
		if (v->key[i] != NULL && v->key[i][0] == 'm')
			v->key[i] = NULL;
}

static int
pp_avail(struct pl *L, int a, int b, int vol)
{
	struct av v;
	int i, ch = 0, rd;
	char key[220];
	ppm dep;

	av_clear(&v);
	v.fresh = 0;
	for (i = a; i < b; i++) {
		struct pl *l = &L[i];
		int dst = -1;

		if (l->kind == PK_LABEL) {
			av_clear(&v);
			continue;
		}
		if (l->kind != PK_INS)
			continue;
		if (l->cls == M_BARRIER || l->cls == M_RET) {
			av_clear(&v);
			continue;
		}
		key[0] = 0;
		dep = 0;
		if (l->cls == M_CLR && l->nops == 1 && pp_isreg(l->op[0])) {
			dst = pp_reg(l->op[0], strlen(l->op[0]));
			strcpy(key, "c0");
		} else if (l->cls == M_MOV && l->nops == 2 && pp_isreg(l->op[1])) {
			char *src = l->op[0];
			int islea = strcmp(l->mn, "lea") == 0, isq = strcmp(l->mn, "movq") == 0;

			dst = pp_reg(l->op[1], strlen(l->op[1]));
			if (pp_isreg(src)) {
				rd = pp_reg(src, strlen(src));
				if (isq && rd != dst && rd >= 0 && rd < PP_NREG && PP_REN(rd) && PP_REN(dst)) {
					/* movq rA,rB: the same value in both */
					if (v.key[rd] == NULL) {
						char *k = malloc(24);

						sprintf(k, "v%d", v.fresh++);
						v.key[rd] = k;
						v.dep[rd] = 1ull << rd;
					}
					if (v.key[dst] != NULL && strcmp(v.key[dst], v.key[rd]) == 0) {
						l->kind = PK_DEAD;
						ch = 1;
						continue;
					}
					av_kill(&v, 1ull << dst);
					v.key[dst] = v.key[rd];
					v.dep[dst] = v.dep[rd] | (1ull << dst);
					continue;
				}
			} else if (src[0] == '#') {
				if (isq)
					sprintf(key, "c%s", src + 1);
			} else if (islea) {
				if (strchr(src, '(') == NULL)
					sprintf(key, "l%s", src);		/* the address of a symbol */
			} else if (l->load && !vol && src[0] != '=' && src[0] != '@' && strstr(src, ")+") == NULL && strstr(src, "-(") == NULL &&
			    strstr(src, ")-") == NULL) {
				sprintf(key, "m%s %s", l->mn, src);		/* a load: the same text at the same address, nothing stored since */
				dep = pp_regs(src);
			}
		}
		if (dst >= 0 && dst < PP_NREG && PP_REN(dst) && key[0]) {
			int j, hit = -1;

			if (v.key[dst] != NULL && strcmp(v.key[dst], key) == 0) {
				l->kind = PK_DEAD;		/* the register holds this value already */
				ch = 1;
				continue;
			}
			for (j = 0; j < PP_NREG && hit < 0; j++)
				if (j != dst && PP_REN(j) && v.key[j] != NULL && strcmp(v.key[j], key) == 0 && (key[0] == 'l' || key[0] == 'm'))
					hit = j;
			if (hit >= 0) {
				/* in another register: a register move (loads extend to 64 bits, so the whole register is the value) */
				char *k = v.key[hit];
				ppm d = v.dep[hit];

				sprintf(l->op[0], "r%d", hit);
				strcpy(l->mn, "movq");
				l->nops = 2;
				pp_unparse(l);
				ch = 1;
				av_kill(&v, 1ull << dst);
				v.key[dst] = k;
				v.dep[dst] = d | (1ull << dst);
				continue;
			}
			av_kill(&v, 1ull << dst);
			if (key[0] == 'm' && (dep >> dst & 1))
				v.key[dst] = NULL;		/* the load overwrites a register of its own address */
			else {
				v.key[dst] = strdup(key);
				v.dep[dst] = dep | (1ull << dst);
			}
			continue;
		}
		/* anything else: what it writes ends the values built from it */
		if (l->store || l->cls == M_CALL || l->cls == M_PUSH || l->cls == M_POP)
			av_killmem(&v);
		av_kill(&v, l->wr & ~PP_FLAGS);
		if (l->cls == M_CALL)
			av_clear(&v);	/* what a call keeps (the callee-saved registers) is not worth the care */
	}
	return ch;
}

/*
 * A condition made into a value and then tested ("a = b < c; if (a)", the join of && and ||):
 *
 *	br .Lj with movq #1,rT (or clr rT) just before it  ...  clr rT .Lj: cbne rT,#0,.Lt
 *
 * The outcome of the test is known on each way in.  A way in that takes the branch goes to .Lt itself; the way in that falls
 * through (the clr) is left out together with the test if the test is not taken, or becomes the branch.  Done only when rT is dead
 * after the test, every reference to .Lj is such a jump, and the label is mentioned nowhere else in the file (a jump table).
 */
static struct pl *pp_all;
static int pp_nall;

static int
pp_const(struct pl *l, int rT, long *v)
{
	if (l->kind != PK_INS)
		return 0;
	if (l->cls == M_CLR && l->nops == 1 && pp_reg(l->op[0], strlen(l->op[0])) == rT) {
		*v = 0;
		return 1;
	}
	if (l->cls == M_MOV && strcmp(l->mn, "movq") == 0 && l->nops == 2 && l->op[0][0] == '#' && pp_reg(l->op[1], strlen(l->op[1])) == rT) {
		*v = strtol(l->op[0] + 1, NULL, 10);
		return 1;
	}
	return 0;
}

/* how often is the label named in the file besides its definition, by lines other than the unconditional branches of this region? */
static int
pp_labelrefs(const char *lab, int a, int b, int *refs, int max, int *others)
{
	int i, n = 0, ll = strlen(lab);

	*others = 0;
	for (i = 0; i < pp_nall; i++) {
		char *p = pp_all[i].txt;

		if (pp_all[i].kind == PK_LABEL)
			continue;
		for (; (p = strstr(p, lab)) != NULL; p += ll)
			if (!isdigit((unsigned char)p[ll]) && (p == pp_all[i].txt || !isalnum((unsigned char)p[-1])))
				break;
		if (p == NULL)
			continue;
		if (i >= a && i < b && pp_all[i].kind == PK_INS && pp_all[i].cls == M_BR && pp_all[i].nops == 1 && strcmp(pp_all[i].op[0], lab) == 0 && n < max)
			refs[n++] = i;
		else
			(*others)++;
	}
	return n;
}

static int
pp_thread(struct pl *L, int a, int b)
{
	int j, changed = 0;

	pp_live(L, a, b);
	for (j = a; j < b; j++) {
		struct pl *c = &L[j];
		int rT, i, i0, k, nl, ok = 1, nrefs = 0, taken_ft = 0, ft = 0, others, refs[16], labs[8], r;
		long v;
		char lab[8][64];

		if (c->kind != PK_INS || c->cls != M_CBR || c->nops != 3 || (strcmp(c->mn, "cbne") != 0 && strcmp(c->mn, "cbeq") != 0) ||
		    strcmp(c->op[1], "#0") != 0 || !pp_isreg(c->op[0]))
			continue;
		rT = pp_reg(c->op[0], strlen(c->op[0]));
		if (!PP_REN(rT) || (c->live >> rT & 1))
			continue;
		/* the labels just above the test */
		nl = 0;
		for (i = j - 1; i >= a && (L[i].kind == PK_LABEL || L[i].kind == PK_DEAD); i--)
			if (L[i].kind == PK_LABEL && nl < 8) {
				strcpy(lab[nl], L[i].txt);
				lab[nl][strlen(lab[nl]) - 1] = 0;
				labs[nl++] = i;
			}
		if (nl == 0 || i < a)
			continue;
		i0 = i;
		/* the way in by falling through */
		if (pp_const(&L[i0], rT, &v)) {
			ft = 1;
			taken_ft = (strcmp(c->mn, "cbne") == 0) == (v != 0);
		} else if (L[i0].kind == PK_INS && (L[i0].cls == M_BR || L[i0].cls == M_RET))
			ft = 0;
		else
			continue;
		/* the ways in by a jump: each unconditional, each with a constant before it, each taking the branch */
		for (k = 0; k < nl && ok; k++) {
			int cnt = pp_labelrefs(lab[k], a, b, refs + nrefs, 16 - nrefs, &others);

			if (others) {
				ok = 0;
				break;
			}
			nrefs += cnt;
		}
		if (!ok || nrefs == 0)
			continue;
		for (k = 0; k < nrefs && ok; k++) {
			for (r = refs[k] - 1; r >= a && L[r].kind == PK_DEAD; r--)
				;
			if (r < a || !pp_const(&L[r], rT, &v) || (strcmp(c->mn, "cbne") == 0) != (v != 0))
				ok = 0;
		}
		if (!ok)
			continue;
		for (k = 0; k < nrefs; k++) {
			for (r = refs[k] - 1; L[r].kind == PK_DEAD; r--)
				;
			L[r].kind = PK_DEAD;
			strcpy(L[refs[k]].op[0], c->op[2]);
			pp_unparse(&L[refs[k]]);
		}
		if (ft) {
			if (taken_ft) {
				strcpy(L[i0].mn, "br");
				strcpy(L[i0].op[0], c->op[2]);
				L[i0].nops = 1;
				pp_unparse(&L[i0]);
			} else
				L[i0].kind = PK_DEAD;
		}
		c->kind = PK_DEAD;
		changed = 1;
		pp_live(L, a, b);
	}
	return changed;
}

static int
pp_region(struct pl *L, int a, int b)
{
	int i, j, k, changed = 0, pass, wideok, ninsn = 0, vol = 0;

	for (i = a; i < b; i++) {
		if (L[i].kind == PK_INS)
			ninsn++;
		if (L[i].kind == PK_DEAD && strncmp(L[i].txt, "\t; vol", 6) == 0)
			vol = 1;
	}
	/* the wide cbCC reaches 64 KB: any label of a function of at most 9,000 instructions (54,000 bytes at six bytes each) is in reach */
	wideok = ninsn <= 9000;
	if (!wideok && getenv("PDP64_PEEPDEBUG"))
		fprintf(stderr, "peep: a function of %d instructions: no wide cbCC\n", ninsn);

	for (pass = 0; pass < 6; pass++) {
		int ch = 0;

		if (PDP64_OPT(PO_AVAIL) && pp_avail(L, a, b, vol))
			ch = 1;
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
					if (m->rd & (1ull << rb)) {
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
					if (m->wr & ((1ull << ra) | (1ull << rb)))
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
				if (m->live & (1ull << ra))
					continue;		/* rA is read again */
				if (l->rd & (1ull << rb))
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
		/* br L where L is directly a ret (nothing to undo in a frameless function): ret itself, two bytes whatever the distance */
		for (i = a; i < b; i++) {
			struct pl *l = &L[i];

			if (l->kind != PK_INS || l->cls != M_BR || l->lblidx < 0)
				continue;
			for (j = l->lblidx + 1; j < b && (L[j].kind == PK_LABEL || L[j].kind == PK_DEAD); j++)
				;
			if (j < b && L[j].kind == PK_INS && L[j].cls == M_RET && strcmp(L[j].mn, "ret") == 0) {
				strcpy(l->mn, "ret");
				l->nops = 0;
				pp_unparse(l);
				l->lblidx = -1;
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
		pp_live(L, a, b);
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
			/*
			 * cbCC writes no codes (errata 39).  The compare it replaces may leave codes for a later branch, set or carry: not when they are
			 * live after the branch, on either way out (liveness is conservative at labels: it is the union over every way in).
			 */
			if (n->live & PP_FLAGS)
				continue;
			if (p->cls == M_CMP && strcmp(p->mn, "cmp") == 0 && p->nops == 2 && pp_isreg(p->op[0]) && p->op[1][0] == '#') {
				long v = strtol(p->op[1] + 1, NULL, 10);

				/*
				 * -16..15 fits the four-byte form (with the 27-bit long form as the fallback, any distance).  Up to 16 bits
				 * the wide form takes six bytes and reaches 32,768 parcels: used only in a function that cannot be
				 * longer than that (no instruction is longer than six bytes) and whose label is in it.
				 */
				if (v < -16 || v > 15) {
					if (v < -32768 || v > 32767 || n->lblidx < 0 || !wideok)
						continue;
				}
				strcpy(n->op[2], n->op[0]);
				strcpy(n->op[0], p->op[0]);
				strcpy(n->op[1], p->op[1]);
			} else if (p->cls == M_CMP && strcmp(p->mn, "cmp") == 0 && p->nops == 2 && pp_isreg(p->op[0]) && pp_isreg(p->op[1])) {
				/* cmp ra,rb ; bCC L -> cbCC ra,rb,L: the label is in this section; cbCC writes the codes as cmp does */
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
		if (PDP64_OPT(PO_THREAD) && pp_thread(L, a, b))
			ch = 1;
		if (!ch)
			break;
		changed = 1;
	}
	return changed;
}


/*
 * Fused instructions of the new targets (the author's decision: the compiler fuses, the assembler never does):
 *	movq ra,rb ; call f		movcall ra,rb,f
 *	movq ra,rb ; movq rc,rd		mov2 ra,rb,rc,rd	(all four in the 3-bit hot table r0 r1 r8 r2 sp r3 r9 r10)
 *	pop rd ; ret			popret rd
 *	popm #mask ; ret		popmret #mask		(a mask of r8-r15 only: the field is mask/256 in 8 bits)
 * Each is the two instructions in order, so nothing is checked but that they are adjacent in one basic block (a label between them,
 * or a branch target, is a line of its own).  A run of register moves followed by a call is matched best-first by dynamic
 * programming: each fused pair saves two bytes whatever the kind, so the most pairs win.
 */
static int
pp_hot3(const char *s)
{
	return strcmp(s, "r0") == 0 || strcmp(s, "r1") == 0 || strcmp(s, "r8") == 0 || strcmp(s, "r2") == 0 || strcmp(s, "sp") == 0 ||
	    strcmp(s, "r3") == 0 || strcmp(s, "r9") == 0 || strcmp(s, "r10") == 0;
}

static int
pp_isrrmov(struct pl *l)
{
	return l->kind == PK_INS && l->cls == M_MOV && strcmp(l->mn, "movq") == 0 && l->nops == 2 && pp_isreg(l->op[0]) && pp_isreg(l->op[1]);
}

static int
pp_isdcall(struct pl *l)
{
	char *s = l->op[0];

	return l->kind == PK_INS && l->cls == M_CALL && strcmp(l->mn, "call") == 0 && l->nops == 1 &&
	    (isalpha((unsigned char)s[0]) || s[0] == '_' || s[0] == '.') && strchr(s, '(') == NULL && !pp_isreg(s);
}

/* can a popm of this mask be a popmret? */
static int
pp_maskok(const char *s)
{
	char *e;
	unsigned long m;

	if (s[0] != '#')
		return 0;
	m = strtoul(s + 1, &e, 0);
	return *e == 0 && m != 0 && (m & 0xff) == 0 && m <= 0xff00;
}

static int
pp_nextlive(struct pl *L, int i, int b)
{
	for (i++; i < b && L[i].kind == PK_DEAD; i++)
		;
	return i;
}

static void
pp_fuse(struct pl *L, int a, int b)
{
	int i, j, k, n, run[64], dp[66], take[66];

	for (i = a; i < b; i++) {
		struct pl *l = &L[i];

		if (l->kind != PK_INS)
			continue;
		j = pp_nextlive(L, i, b);
		if (j >= b || L[j].kind != PK_INS)
			continue;
		if (L[j].cls == M_RET && strcmp(L[j].mn, "ret") == 0) {
			if (PDP64_OPT(PO_POPRET) && l->cls == M_POP && strcmp(l->mn, "pop") == 0 && l->nops == 1 && pp_isreg(l->op[0])) {
				strcpy(l->mn, "popret");
				pp_unparse(l);
				l->kind = PK_OTHER;		/* not understood by the rest: a barrier */
				L[j].kind = PK_DEAD;
			} else if (PDP64_OPT(PO_POPMRET) && strcmp(l->mn, "popm") == 0 && l->nops == 1 && pp_maskok(l->op[0])) {
				strcpy(l->mn, "popmret");
				pp_unparse(l);
				L[j].kind = PK_DEAD;
			}
		}
	}
	/* runs of adjacent register moves, then maybe a direct call */
	for (i = a; i < b; i++) {
		if (!(pp_isrrmov(&L[i]) || pp_isdcall(&L[i])))
			continue;
		n = 0;
		for (j = i; j < b && n < 64; j = pp_nextlive(L, j, b)) {
			if (j >= b || !(pp_isrrmov(&L[j]) || (pp_isdcall(&L[j]) && PDP64_OPT(PO_MOVCALL))))
				break;
			run[n++] = j;
			if (L[j].cls == M_CALL)
				break;
		}
		if (n == 0)
			continue;
		/* edge (k-1,k): a mov2 of two moves, or a movcall of a move and the call */
		dp[0] = 0;
		dp[1] = 0;
		take[0] = take[1] = 0;
		for (k = 2; k <= n; k++) {
			struct pl *x = &L[run[k - 2]], *y = &L[run[k - 1]];
			int ok;

			if (y->cls == M_CALL)
				ok = PDP64_OPT(PO_MOVCALL) && pp_isrrmov(x);
			else
				ok = PDP64_OPT(PO_MOV2) && pp_isrrmov(x) && pp_isrrmov(y) && pp_hot3(x->op[0]) && pp_hot3(x->op[1]) &&
				    pp_hot3(y->op[0]) && pp_hot3(y->op[1]);
			dp[k] = dp[k - 1];
			take[k] = 0;
			if (ok && dp[k - 2] + 1 > dp[k]) {
				dp[k] = dp[k - 2] + 1;
				take[k] = 1;
			}
		}
		for (k = n; k >= 2; ) {
			if (take[k]) {
				struct pl *x = &L[run[k - 2]], *y = &L[run[k - 1]];

				char buf[400];

				if (y->cls == M_CALL)
					sprintf(buf, "\tmovcall %s,%s,%s", x->op[0], x->op[1], y->op[0]);
				else
					sprintf(buf, "\tmov2 %s,%s,%s,%s", x->op[0], x->op[1], y->op[0], y->op[1]);
				y->txt = strdup(buf);
				y->kind = PK_OTHER;
				y->cls = M_BARRIER;
				x->kind = PK_DEAD;
				k -= 2;
			} else
				k--;
		}
		i = run[n - 1];
	}
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
	pp_all = L;
	pp_nall = n;
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
		if (pdp64_target != PDP64_R20)
			pp_fuse(L, a, b);
		a = b;
	}
	for (i = 0; i < n; i++)
		if (L[i].kind != PK_DEAD || L[i].note)
			fprintf(out, "%s\n", L[i].txt);
}
