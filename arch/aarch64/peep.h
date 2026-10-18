/*
 * peep.h -- a text optimizer for the aarch64 assembly of a whole file (-xdmr).
 *
 * The compiler keeps the assembly of the file in memory (code.c) and hands it
 * here when the job is over.  Everything is static; the file is included.
 *
 * Each function is a run of labels and instructions.  The instructions the
 * compiler prints are parsed into a mnemonic and operands, each with the
 * registers it reads and writes; an instruction that is not understood reads
 * and writes everything and nothing moves across it.  Liveness is computed
 * over the labels and branches of the function.  The rewrites are:
 *
 *	copy propagation, dead code	mov x3,x2; ldr w0,[x3] -> ldr w0,[x2]
 *	post-index addressing		ldr w0,[x1]; add x1,x1,#4 -> ldr w0,[x1],#4
 *	compare with zero as a branch	cmp w0,#0; b.ne L -> cbnz w0,L
 *	sign bit and bit tests		cmp w0,#0; b.lt L -> tbnz w0,#31,L
 *					and w1,w0,#8; cmp w1,#0; b.eq L -> tbz w0,#3,L
 *	repeated compares		cmp w0,#0; b.ne A; cmp w0,#0; b.ge B
 *	redundant extensions		ldrb w3,[x0]; uxtb w3,w3
 *	index registers			lsl x1,x1,#2; add x2,x0,x1; ldr w3,[x2]
 *					 -> ldr w3,[x0,x1,lsl #2]
 *	a conditional value		b.lt L; movz w0,#1; b M; L: movz w0,#0; M:
 *					 -> cset w0,ge
 *
 * Loads are never removed or merged: a volatile object is read by a load the
 * compiler cannot tell from any other.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef unsigned long long ap_mask;

#define	AP_SP		31
#define	AP_D(n)		(32 + (n))
#define	AP_BIT(r)	((ap_mask)1 << (r))
#define	AP_ALL		(~(ap_mask)0)

/* instruction kinds */
#define	AK_OTHER	0	/* a directive, kept as it is */
#define	AK_LABEL	1
#define	AK_INS		2
#define	AK_DEAD		3

/* flags of an instruction */
#define	AF_MEMR		001
#define	AF_MEMW		002
#define	AF_CALL		004
#define	AF_BR		010	/* control does not continue to the next instruction */
#define	AF_CBR		020	/* a conditional branch */
#define	AF_RET		040
#define	AF_BARRIER	0100	/* unknown: reads and writes everything */
#define	AF_KEEP		0200	/* has an effect beyond its registers: never removed */

struct ai {
	int kind;
	char *txt;		/* AK_OTHER, AK_LABEL */
	char mn[16];
	char op[5][72];
	int nop;
	ap_mask use, def;
	int fuse, fdef;		/* reads / writes the condition codes */
	int fl;
	int tgt;		/* the label a branch goes to, -1 if not known */
};

struct ap {
	struct ai *v;
	int n, cap;
	int lo, hi;		/* the function being worked on: v[lo..hi) */
	ap_mask *lout;		/* what is live after each instruction */
	char *fout;		/* are the condition codes live after it */
	ap_mask *lin;
	char *fin;
	int dirty;
};

/* ---- registers and operands ---- */

/* x0-x30 and w0-w30: the number; sp 31; xzr/wzr -1; d/s: 32+n; else -2 */
static int
ap_reg(const char *s, int *w)
{
	char c = s[0];
	int n;

	if (strcmp(s, "sp") == 0) {
		*w = 'x';
		return AP_SP;
	}
	if (strcmp(s, "xzr") == 0 || strcmp(s, "wzr") == 0) {
		*w = s[0];
		return -1;
	}
	if ((c != 'x' && c != 'w' && c != 'd' && c != 's') || !isdigit((unsigned char)s[1]))
		return -2;
	n = atoi(s + 1);
	if (n > 31 || s[1 + (n > 9 ? 2 : 1)] != '\0')
		return -2;
	if ((c == 'x' || c == 'w') && n > 30)
		return -2;
	*w = c;
	return (c == 'x' || c == 'w') ? n : AP_D(n);
}

static void
ap_regname(char *buf, int r, int w)
{
	if (r == AP_SP)
		strcpy(buf, "sp");
	else if (r >= 32)
		sprintf(buf, "%c%d", w, r - 32);
	else
		sprintf(buf, "%c%d", w, r);
}

/* a memory operand */
struct ap_mem {
	int base;		/* register number of the base, -2 if not a [reg...] */
	int idx;		/* index register or -2 */
	int pre;		/* [x,#n]! */
	int imm;		/* the offset is an immediate (or none) */
	long off;
	int sym;		/* [x16,:lo12:sym] */
};

static int
ap_parse_mem(const char *s, struct ap_mem *m)
{
	char t[72], *p, *q;
	int w;

	memset(m, 0, sizeof(*m));
	m->base = m->idx = -2;
	if (s[0] != '[')
		return 0;
	strncpy(t, s + 1, sizeof(t) - 1);
	t[sizeof(t) - 1] = '\0';
	p = strchr(t, ']');
	if (p == NULL)
		return 0;
	if (p[1] == '!')
		m->pre = 1;
	*p = '\0';
	q = strchr(t, ',');
	if (q != NULL)
		*q++ = '\0';
	m->base = ap_reg(t, &w);
	if (m->base < 0 || w != 'x')
		return 0;
	if (q == NULL) {
		m->imm = 1;
		return 1;
	}
	while (*q == ' ')
		q++;
	if (*q == '#') {
		m->imm = 1;
		m->off = strtol(q + 1, NULL, 0);
	} else if (strncmp(q, ":lo12:", 6) == 0) {
		m->sym = 1;
	} else {
		char r[16];
		int i = 0;

		while (*q && *q != ',' && i < 15)
			r[i++] = *q++;
		r[i] = '\0';
		m->idx = ap_reg(r, &w);
		if (m->idx < 0)
			return 0;
	}
	return 1;
}

/* a plain [xb] or [xb,#0] */
static int
ap_plain_mem(const char *s, int *base)
{
	struct ap_mem m;

	if (!ap_parse_mem(s, &m) || !m.imm || m.off != 0 || m.pre || m.sym || m.idx >= 0)
		return 0;
	*base = m.base;
	return 1;
}

static int
ap_isimm(const char *s)
{
	return s[0] == '#';
}

static int
ap_isreg(const char *s)
{
	int w;

	return ap_reg(s, &w) != -2;
}

static int
ap_num(const char *s, long *v)
{
	char *e;

	if (s[0] != '#')
		return 0;
	*v = strtol(s + 1, &e, 0);
	return *e == '\0';
}

/* ---- parsing ---- */

static void
ap_split(struct ai *a, char *rest)
{
	int depth = 0, i;
	char *p = rest, *start = rest;

	a->nop = 0;
	if (*rest == '\0')
		return;
	for (;; p++) {
		if (*p == '[')
			depth++;
		else if (*p == ']')
			depth--;
		if ((*p == ',' && depth == 0) || *p == '\0') {
			int len = (int)(p - start);

			if (a->nop < 5 && len < 72) {
				memcpy(a->op[a->nop], start, len);
				a->op[a->nop][len] = '\0';
				a->nop++;
			} else
				a->fl |= AF_BARRIER;
			if (*p == '\0')
				break;
			start = p + 1;
			while (*start == ' ')
				start++;
		}
	}
	for (i = 0; i < a->nop; i++) {
		char *e = a->op[i] + strlen(a->op[i]);

		while (e > a->op[i] && e[-1] == ' ')
			*--e = '\0';
	}
}

static int
ap_in(const char *mn, const char *list)
{
	size_t l = strlen(mn);
	const char *p = list;

	while (*p) {
		const char *e = strchr(p, ' ');
		size_t n = e ? (size_t)(e - p) : strlen(p);

		if (n == l && strncmp(p, mn, n) == 0)
			return 1;
		if (!e)
			break;
		p = e + 1;
	}
	return 0;
}

static int
ap_isbranch(const char *mn)
{
	return strcmp(mn, "b") == 0 || strncmp(mn, "b.", 2) == 0 ||
	    ap_in(mn, "cbz cbnz tbz tbnz");
}

static void
ap_use(struct ai *a, int i)
{
	int w, r;

	if (i >= a->nop)
		return;
	r = ap_reg(a->op[i], &w);
	if (r >= 0)
		a->use |= AP_BIT(r);
}

static void
ap_def(struct ai *a, int i)
{
	int w, r;

	if (i >= a->nop)
		return;
	r = ap_reg(a->op[i], &w);
	if (r >= 0)
		a->def |= AP_BIT(r);
}

/* a memory reference: base and index are read, the base maybe written */
static void
ap_memuse(struct ai *a, int i)
{
	struct ap_mem m;

	if (i >= a->nop || !ap_parse_mem(a->op[i], &m)) {
		a->fl |= AF_BARRIER;
		return;
	}
	a->use |= AP_BIT(m.base);
	if (m.idx >= 0)
		a->use |= AP_BIT(m.idx);
	if (m.pre || (i + 1 < a->nop && ap_isimm(a->op[i + 1])))
		a->def |= AP_BIT(m.base);
}

static void
ap_dataflow(struct ai *a)
{
	const char *mn = a->mn;
	int i;

	a->use = a->def = 0;
	a->fuse = a->fdef = 0;
	a->fl &= AF_BARRIER;
	a->tgt = -1;
	if (a->fl & AF_BARRIER)
		return;

	if (ap_in(mn, "add sub and orr eor mul sdiv udiv lsl lsr asr adds subs ands bic "
	    "fadd fsub fmul fdiv")) {
		ap_def(a, 0);
		for (i = 1; i < a->nop; i++)
			ap_use(a, i);
		if (ap_in(mn, "adds subs ands"))
			a->fdef = 1;
	} else if (ap_in(mn, "mov neg mvn sxtb sxth sxtw uxtb uxth fmov fneg fcvt scvtf ucvtf "
	    "fcvtzs fcvtzu movz movn ubfx")) {
		ap_def(a, 0);
		ap_use(a, 1);
	} else if (strcmp(mn, "movk") == 0) {
		ap_def(a, 0);
		ap_use(a, 0);
	} else if (ap_in(mn, "msub madd")) {
		ap_def(a, 0);
		for (i = 1; i < a->nop; i++)
			ap_use(a, i);
	} else if (ap_in(mn, "cmp cmn tst fcmp")) {
		ap_use(a, 0);
		ap_use(a, 1);
		ap_use(a, 2);
		a->fdef = 1;
	} else if (strcmp(mn, "cset") == 0) {
		ap_def(a, 0);
		a->fuse = 1;
	} else if (strcmp(mn, "csel") == 0) {
		ap_def(a, 0);
		ap_use(a, 1);
		ap_use(a, 2);
		a->fuse = 1;
	} else if (strcmp(mn, "adrp") == 0) {
		ap_def(a, 0);
	} else if (ap_in(mn, "ldr ldrb ldrh ldrsb ldrsh ldrsw ldur")) {
		ap_def(a, 0);
		ap_memuse(a, 1);
		a->fl |= AF_MEMR;
	} else if (strcmp(mn, "ldp") == 0) {
		ap_def(a, 0);
		ap_def(a, 1);
		ap_memuse(a, 2);
		a->fl |= AF_MEMR;
	} else if (ap_in(mn, "str strb strh stur")) {
		ap_use(a, 0);
		ap_memuse(a, 1);
		a->fl |= AF_MEMW;
	} else if (strcmp(mn, "stp") == 0) {
		ap_use(a, 0);
		ap_use(a, 1);
		ap_memuse(a, 2);
		a->fl |= AF_MEMW;
	} else if (strcmp(mn, "b") == 0) {
		a->fl |= AF_BR;
	} else if (strncmp(mn, "b.", 2) == 0) {
		a->fl |= AF_CBR;
		a->fuse = 1;
	} else if (ap_in(mn, "cbz cbnz tbz tbnz")) {
		a->fl |= AF_CBR;
		ap_use(a, 0);
	} else if (strcmp(mn, "br") == 0) {
		a->fl |= AF_BR | AF_BARRIER;
		ap_use(a, 0);
	} else if (strcmp(mn, "bl") == 0 || strcmp(mn, "blr") == 0) {
		int r;

		a->fl |= AF_CALL | AF_MEMR | AF_MEMW | AF_KEEP;
		a->def = 0x7ffffULL | AP_BIT(30);	/* x0-x18, x30 */
		for (r = 0; r < 8; r++)
			a->def |= AP_BIT(AP_D(r));
		for (r = 16; r < 32; r++)
			a->def |= AP_BIT(AP_D(r));
		a->use = 0x1ffULL;			/* x0-x8 */
		for (r = 0; r < 8; r++)
			a->use |= AP_BIT(AP_D(r));
		a->fdef = 1;
		if (strcmp(mn, "blr") == 0)
			ap_use(a, 0);
	} else if (strcmp(mn, "ret") == 0) {
		int r;

		a->fl |= AF_RET | AF_BR | AF_KEEP;
		a->use = AP_BIT(0) | AP_BIT(AP_D(0)) | AP_BIT(29) | AP_BIT(30) | AP_BIT(AP_SP);
		for (r = 19; r <= 28; r++)
			a->use |= AP_BIT(r);
		for (r = 8; r <= 15; r++)
			a->use |= AP_BIT(AP_D(r));
	} else {
		a->fl |= AF_BARRIER;
	}
	/* the stack, the frame and the link register are not ours to drop */
	if (a->def & (AP_BIT(AP_SP) | AP_BIT(29) | AP_BIT(30)))
		a->fl |= AF_KEEP;
}

static void
ap_parse_line(struct ap *P, char *line)
{
	struct ai *a;
	size_t len = strlen(line);

	if (P->n == P->cap) {
		P->cap = P->cap ? P->cap * 2 : 1024;
		P->v = realloc(P->v, P->cap * sizeof(struct ai));
	}
	a = &P->v[P->n++];
	memset(a, 0, sizeof(*a));
	a->tgt = -1;
	if (line[0] == '\t' && islower((unsigned char)line[1])) {
		char *p = line + 1, *q;
		int i = 0;

		while (*p && *p != ' ' && *p != '\t' && i < 15)
			a->mn[i++] = *p++;
		a->mn[i] = '\0';
		if (*p != '\0' && *p != ' ' && *p != '\t') {	/* not one of ours */
			a->kind = AK_OTHER;
			a->txt = strdup(line);
			return;
		}
		while (*p == ' ' || *p == '\t')
			p++;
		/* a comment or a second statement: not ours, keep the line as it is */
		if (strstr(p, "//") != NULL || strchr(p, ';') != NULL || strchr(p, '"') != NULL) {
			a->kind = AK_OTHER;
			a->txt = strdup(line);
			return;
		}
		q = strdup(p);
		a->kind = AK_INS;
		ap_split(a, q);
		free(q);
		ap_dataflow(a);
	} else if (len > 1 && line[len - 1] == ':' && line[0] != '\t' && line[0] != ' ') {
		a->kind = AK_LABEL;
		a->txt = strdup(line);
	} else {
		a->kind = AK_OTHER;
		a->txt = strdup(line);
	}
}

/* ---- liveness ---- */

static int
ap_find_label(struct ap *P, const char *name)
{
	int i;
	size_t l = strlen(name);

	for (i = P->lo; i < P->hi; i++) {
		struct ai *a = &P->v[i];

		if (a->kind == AK_LABEL && strlen(a->txt) == l + 1 && strncmp(a->txt, name, l) == 0)
			return i;
	}
	return -1;
}

static void
ap_live(struct ap *P)
{
	int i, n = P->hi - P->lo, changed;
	struct ai *v = P->v + P->lo;

	for (i = 0; i < n; i++) {
		struct ai *a = &v[i];

		a->tgt = -1;
		if (a->kind == AK_INS && ap_isbranch(a->mn) && a->nop > 0)
			a->tgt = ap_find_label(P, a->op[a->nop - 1]);
	}
	for (i = 0; i <= n; i++) {
		P->lin[i] = P->lout[i] = 0;
		P->fin[i] = P->fout[i] = 0;
	}
	do {
		changed = 0;
		for (i = n - 1; i >= 0; i--) {
			struct ai *a = &v[i];
			ap_mask out = 0, in;
			int fo = 0, fi;

			if (a->kind == AK_INS && (a->fl & AF_BARRIER)) {
				out = AP_ALL;
				fo = 1;
			} else if (a->kind == AK_INS && (a->fl & AF_RET)) {
				out = 0;
			} else {
				if (!(a->kind == AK_INS && (a->fl & AF_BR)) && i + 1 < n) {
					out |= P->lin[i + 1];
					fo |= P->fin[i + 1];
				}
				if (a->kind == AK_INS && ap_isbranch(a->mn)) {
					if (a->tgt >= 0) {
						out |= P->lin[a->tgt - P->lo];
						fo |= P->fin[a->tgt - P->lo];
					} else {
						out = AP_ALL;
						fo = 1;
					}
				}
			}
			if (a->kind == AK_INS) {
				in = (out & ~a->def) | a->use;
				if (a->fl & AF_BARRIER)
					in = AP_ALL;
				fi = a->fdef ? a->fuse : (fo | a->fuse);
			} else {
				in = out;
				fi = fo;
			}
			if (in != P->lin[i] || out != P->lout[i] || fi != P->fin[i] || fo != P->fout[i])
				changed = 1;
			P->lin[i] = in;
			P->lout[i] = out;
			P->fin[i] = fi;
			P->fout[i] = fo;
		}
	} while (changed);
	P->dirty = 0;
}

#define	AP_LOUT(P, i)	((P)->lout[(i) - (P)->lo])
#define	AP_FOUT(P, i)	((P)->fout[(i) - (P)->lo])

static void
ap_ensure(struct ap *P)
{
	if (P->dirty)
		ap_live(P);
}

/* the next/previous label or instruction that is not dead */
static int
ap_next(struct ap *P, int i)
{
	for (i++; i < P->hi; i++)
		if (P->v[i].kind == AK_INS || P->v[i].kind == AK_LABEL)
			return i;
	return -1;
}

static int
ap_prev(struct ap *P, int i)
{
	for (i--; i >= P->lo; i--)
		if (P->v[i].kind == AK_INS || P->v[i].kind == AK_LABEL)
			return i;
	return -1;
}

static void
ap_kill(struct ap *P, int i)
{
	P->v[i].kind = AK_DEAD;
	P->dirty = 1;
}

/* Replace an instruction.  NULL ends the operands. */
static void
ap_setops(struct ap *P, struct ai *a, const char *mn, const char *o0, const char *o1,
    const char *o2, const char *o3)
{
	const char *o[4];
	char t[4][72];
	int i;

	o[0] = o0; o[1] = o1; o[2] = o2; o[3] = o3;
	for (i = 0; i < 4; i++)
		if (o[i] != NULL) {
			strncpy(t[i], o[i], 71);
			t[i][71] = '\0';
		}
	strcpy(a->mn, mn);
	a->nop = 0;
	for (i = 0; i < 4; i++)
		if (o[i] != NULL)
			strcpy(a->op[a->nop++], t[i]);
	a->fl = 0;
	ap_dataflow(a);
	P->dirty = 1;
}

/* does a read or write register r? */
static int
ap_touches(struct ai *a, int r)
{
	if (a->fl & AF_BARRIER)
		return 1;
	return ((a->use | a->def) & AP_BIT(r)) != 0;
}

static int
ap_stops(struct ai *a)
{
	return (a->fl & (AF_BR | AF_CBR | AF_CALL | AF_RET | AF_BARRIER)) != 0;
}

static int
ap_is_ldst(struct ai *a)
{
	return ap_in(a->mn, "ldr ldrb ldrh ldrsb ldrsh ldrsw str strb strh");
}

/* ---- copy propagation ---- */

struct ap_copy {
	int src;		/* -1: no copy */
	int wonly;		/* a 32-bit move: only the 32-bit name is the source's */
};

/* which operands are plain register reads, as a bit set */
static unsigned
ap_readops(struct ai *a)
{
	const char *mn = a->mn;
	unsigned m = 0;
	int i;

	if (ap_in(mn, "add sub and orr eor mul sdiv udiv lsl lsr asr adds subs ands bic "
	    "mov neg mvn sxtb sxth sxtw uxtb uxth msub madd fadd fsub fmul fdiv fmov fneg "
	    "fcvt scvtf ucvtf fcvtzs fcvtzu csel ubfx")) {
		for (i = 1; i < a->nop; i++)
			m |= 1u << i;
		if (strcmp(mn, "csel") == 0)
			m &= ~(1u << 3);
	} else if (ap_in(mn, "cmp cmn tst fcmp")) {
		m = 3;
		if (a->nop > 2)
			m |= 4;
	} else if (ap_in(mn, "str strb strh stur")) {
		m = 1;
	} else if (strcmp(mn, "stp") == 0) {
		m = 3;
	} else if (ap_in(mn, "cbz cbnz tbz tbnz")) {
		m = 1;
	}
	return m;
}

static int
ap_copyprop(struct ap *P)
{
	struct ap_copy cp[32];
	int i, r, changed = 0;

	for (r = 0; r < 32; r++)
		cp[r].src = -1;
	for (i = P->lo; i < P->hi; i++) {
		struct ai *a = &P->v[i];
		unsigned m;
		int j, touched = 0;

		if (a->kind == AK_LABEL || (a->kind == AK_INS && (a->fl & AF_BARRIER))) {
			for (r = 0; r < 32; r++)
				cp[r].src = -1;
			continue;
		}
		if (a->kind != AK_INS)
			continue;
		m = ap_readops(a);
		for (j = 0; j < a->nop; j++) {
			if (m & (1u << j)) {
				int w, rr = ap_reg(a->op[j], &w);

				if (rr >= 0 && rr < 31 && cp[rr].src >= 0 && (w == 'x' || w == 'w') &&
				    !(cp[rr].wonly && w == 'x')) {
					ap_regname(a->op[j], cp[rr].src, w);
					touched = 1;
				}
			} else if (a->op[j][0] == '[') {
				struct ap_mem mm;

				if (ap_parse_mem(a->op[j], &mm) && mm.base < 31 && cp[mm.base].src >= 0 &&
				    !cp[mm.base].wonly && !mm.pre && !(j + 1 < a->nop && ap_isimm(a->op[j + 1]))) {
					char nb[8], t[72];
					const char *rest = strchr(a->op[j], ',');
					const char *bang = strchr(a->op[j], ']');

					ap_regname(nb, cp[mm.base].src, 'x');
					if (rest != NULL && rest < bang)
						snprintf(t, sizeof(t), "[%s%s", nb, rest);
					else
						snprintf(t, sizeof(t), "[%s%s", nb, bang);
					strcpy(a->op[j], t);
					touched = 1;
				}
			}
		}
		if (touched) {
			ap_dataflow(a);
			changed = 1;
		}
		for (r = 0; r < 31; r++) {
			if (a->def & AP_BIT(r)) {
				int k;

				cp[r].src = -1;
				for (k = 0; k < 31; k++)
					if (cp[k].src == r)
						cp[k].src = -1;
			}
		}
		if (strcmp(a->mn, "mov") == 0 && a->nop == 2) {
			int w1, w2, d = ap_reg(a->op[0], &w1), s = ap_reg(a->op[1], &w2);

			if (d >= 0 && d < 31 && s >= 0 && s < 31 && d != s && w1 == w2 &&
			    (w1 == 'x' || w1 == 'w')) {
				cp[d].src = s;
				cp[d].wonly = (w1 == 'w');
			}
		}
	}
	if (changed)
		P->dirty = 1;
	return changed;
}

/* ---- dead code ---- */

static int
ap_dce(struct ap *P)
{
	int i, changed = 0;

	ap_ensure(P);
	for (i = P->hi - 1; i >= P->lo; i--) {
		struct ai *a = &P->v[i];

		if (a->kind != AK_INS ||
		    (a->fl & (AF_BARRIER | AF_KEEP | AF_MEMR | AF_MEMW | AF_CBR | AF_BR | AF_CALL | AF_RET)))
			continue;
		if (a->def == 0 && a->fdef == 0)
			continue;
		if ((a->def & AP_LOUT(P, i)) != 0)
			continue;
		if (a->fdef && AP_FOUT(P, i))
			continue;
		ap_kill(P, i);
		changed = 1;
		ap_ensure(P);
	}
	return changed;
}

/* ---- branches on a comparison with zero ---- */

static int
ap_single_bit(unsigned long long m)
{
	return m != 0 && (m & (m - 1)) == 0;
}

static int
ap_log2(unsigned long long m)
{
	int n = 0;

	while (m > 1) {
		m >>= 1;
		n++;
	}
	return n;
}

static int
ap_cmpbr(struct ap *P)
{
	int i, changed = 0;
	long v;

	ap_ensure(P);
	for (i = P->lo; i < P->hi; i++) {
		struct ai *a = &P->v[i], *b, *c;
		int j, k, w;
		char tgt[72], nb[16];

		if (a->kind != AK_INS)
			continue;
		/* cmp wN,#0; b.cc L */
		if (strcmp(a->mn, "cmp") == 0 && a->nop == 2 && ap_num(a->op[1], &v) && v == 0 &&
		    ap_reg(a->op[0], &w) >= 0) {
			j = ap_next(P, i);
			if (j < 0 || P->v[j].kind != AK_INS)
				continue;
			b = &P->v[j];
			if (strncmp(b->mn, "b.", 2) != 0 || b->tgt < 0 || AP_FOUT(P, j))
				continue;
			if (strcmp(b->mn, "b.eq") == 0 || strcmp(b->mn, "b.ne") == 0) {
				strcpy(tgt, b->op[0]);
				ap_setops(P, b, strcmp(b->mn, "b.eq") == 0 ? "cbz" : "cbnz", a->op[0], tgt, NULL, NULL);
				ap_kill(P, i);
				changed = 1;
				ap_ensure(P);
			} else if ((strcmp(b->mn, "b.lt") == 0 || strcmp(b->mn, "b.ge") == 0) &&
			    P->hi - P->lo < 4000) {
				strcpy(tgt, b->op[0]);
				sprintf(nb, "#%d", w == 'x' ? 63 : 31);
				ap_setops(P, b, strcmp(b->mn, "b.lt") == 0 ? "tbnz" : "tbz", a->op[0], nb, tgt, NULL);
				ap_kill(P, i);
				changed = 1;
				ap_ensure(P);
			}
			continue;
		}
		/* and wD,wS,#mask; cmp wD,#0; b.eq/ne L, with D dead afterwards */
		if (strcmp(a->mn, "and") == 0 && a->nop == 3 && ap_isreg(a->op[1]) && a->op[2][0] == '#') {
			unsigned long long mask = strtoull(a->op[2] + 1, NULL, 0);
			int d = ap_reg(a->op[0], &w);

			if (d < 0)
				continue;
			j = ap_next(P, i);
			if (j < 0 || P->v[j].kind != AK_INS)
				continue;
			c = &P->v[j];
			if (strcmp(c->mn, "cmp") != 0 || c->nop != 2 || strcmp(c->op[0], a->op[0]) != 0 ||
			    !ap_num(c->op[1], &v) || v != 0)
				continue;
			k = ap_next(P, j);
			if (k < 0 || P->v[k].kind != AK_INS)
				continue;
			b = &P->v[k];
			if ((strcmp(b->mn, "b.eq") != 0 && strcmp(b->mn, "b.ne") != 0) || b->tgt < 0 ||
			    AP_FOUT(P, k) || (AP_LOUT(P, k) & AP_BIT(d)))
				continue;
			strcpy(tgt, b->op[0]);
			if (ap_single_bit(mask) && P->hi - P->lo < 4000 && ap_log2(mask) < (a->op[1][0] == 'w' ? 32 : 64)) {
				sprintf(nb, "#%d", ap_log2(mask));
				ap_setops(P, b, strcmp(b->mn, "b.eq") == 0 ? "tbz" : "tbnz", a->op[1], nb, tgt, NULL);
				ap_kill(P, i);
				ap_kill(P, j);
			} else {
				char mm[72], rr[8];

				strcpy(mm, a->op[2]);
				strcpy(rr, a->op[1]);
				ap_setops(P, c, "tst", rr, mm, NULL, NULL);
				ap_kill(P, i);
			}
			changed = 1;
			ap_ensure(P);
		}
	}
	return changed;
}

/* ---- repeated compares, extensions, adrp: a forward scan ---- */

#define	EX_Z8	001	/* the register holds a value below 256 */
#define	EX_S8	002	/* sign-extended from a byte within 32 bits */
#define	EX_Z16	004
#define	EX_S16	010
#define	EX_UP0	020	/* the upper 32 bits of the x register are zero */

static int
ap_scan(struct ap *P)
{
	unsigned ex[32];
	char adrp[32][72];
	char cmpkey[160];
	int cmpreg[2] = { -1, -1 };
	int cmpvalid = 0, i, r, changed = 0;

	for (r = 0; r < 32; r++) {
		ex[r] = 0;
		adrp[r][0] = '\0';
	}
	ap_ensure(P);
	for (i = P->lo; i < P->hi; i++) {
		struct ai *a = &P->v[i];
		const char *mn = a->mn;
		unsigned nex = 0;
		int d, w;

		if (a->kind == AK_LABEL || (a->kind == AK_INS && (a->fl & AF_BARRIER))) {
			for (r = 0; r < 32; r++) {
				ex[r] = 0;
				adrp[r][0] = '\0';
			}
			cmpvalid = 0;
			continue;
		}
		if (a->kind != AK_INS)
			continue;

		/* a repeated compare */
		if (ap_in(mn, "cmp cmn tst fcmp") && a->nop == 2) {
			char key[160];

			snprintf(key, sizeof(key), "%s %s,%s", mn, a->op[0], a->op[1]);
			if (cmpvalid && strcmp(key, cmpkey) == 0) {
				ap_kill(P, i);
				changed = 1;
				continue;
			}
		}
		/* a repeated adrp */
		if (strcmp(mn, "adrp") == 0 && a->nop == 2) {
			d = ap_reg(a->op[0], &w);
			if (d >= 0 && d < 31 && strcmp(adrp[d], a->op[1]) == 0) {
				ap_kill(P, i);
				changed = 1;
				continue;
			}
		}
		/* an extension of what is already extended */
		d = a->nop > 0 ? ap_reg(a->op[0], &w) : -2;
		if (a->nop == 2 && d >= 0 && d < 31 && w == 'w') {
			int s, w2;

			s = ap_reg(a->op[1], &w2);
			if (s >= 0 && s < 31 && w2 == 'w' && (ex[s] & EX_UP0)) {
				int redundant = 0;

				if (strcmp(mn, "uxtb") == 0 && (ex[s] & EX_Z8))
					redundant = 1;
				else if (strcmp(mn, "sxtb") == 0 && (ex[s] & EX_S8))
					redundant = 1;
				else if (strcmp(mn, "uxth") == 0 && (ex[s] & (EX_Z8 | EX_Z16)))
					redundant = 1;
				else if (strcmp(mn, "sxth") == 0 && (ex[s] & (EX_S8 | EX_S16)))
					redundant = 1;
				if (redundant) {
					if (d == s) {
						ap_kill(P, i);
						changed = 1;
						continue;
					}
					ap_setops(P, a, "mov", a->op[0], a->op[1], NULL, NULL);
					changed = 1;
					mn = a->mn;
				}
			}
		}
		/* sxtb wD,wS; cmp wD,#0; b.eq/ne with wS a byte already: test wS */
		if (strcmp(mn, "sxtb") == 0 && a->nop == 2) {
			int s, w2, j, k;
			long v;

			s = ap_reg(a->op[1], &w2);
			j = ap_next(P, i);
			k = j >= 0 ? ap_next(P, j) : -1;
			d = ap_reg(a->op[0], &w);
			if (s >= 0 && s < 31 && d >= 0 && w == 'w' && w2 == 'w' && (ex[s] & EX_Z8) &&
			    (ex[s] & EX_UP0) && k >= 0 && P->v[j].kind == AK_INS && P->v[k].kind == AK_INS &&
			    strcmp(P->v[j].mn, "cmp") == 0 && strcmp(P->v[j].op[0], a->op[0]) == 0 &&
			    ap_num(P->v[j].op[1], &v) && v == 0 &&
			    (strcmp(P->v[k].mn, "b.eq") == 0 || strcmp(P->v[k].mn, "b.ne") == 0) &&
			    P->v[k].tgt >= 0 && !AP_FOUT(P, k) && !(AP_LOUT(P, k) & AP_BIT(d))) {
				char tgt[72];

				strcpy(tgt, P->v[k].op[0]);
				ap_setops(P, &P->v[k], strcmp(P->v[k].mn, "b.eq") == 0 ? "cbz" : "cbnz",
				    a->op[1], tgt, NULL, NULL);
				ap_kill(P, i);
				ap_kill(P, j);
				changed = 1;
				continue;
			}
		}

		/* what is known after this instruction */
		if (a->nop > 0 && a->op[0][0] == 'w') {
			int s, w2;

			if (strcmp(mn, "ldrb") == 0)
				nex = EX_Z8 | EX_Z16 | EX_UP0;
			else if (strcmp(mn, "ldrsb") == 0)
				nex = EX_S8 | EX_S16 | EX_UP0;
			else if (strcmp(mn, "ldrh") == 0)
				nex = EX_Z16 | EX_UP0;
			else if (strcmp(mn, "ldrsh") == 0)
				nex = EX_S16 | EX_UP0;
			else if (strcmp(mn, "uxtb") == 0)
				nex = EX_Z8 | EX_Z16 | EX_UP0;
			else if (strcmp(mn, "sxtb") == 0)
				nex = EX_S8 | EX_S16 | EX_UP0;
			else if (strcmp(mn, "uxth") == 0)
				nex = EX_Z16 | EX_UP0;
			else if (strcmp(mn, "sxth") == 0)
				nex = EX_S16 | EX_UP0;
			else if (strcmp(mn, "mov") == 0 && a->nop == 2 &&
			    (s = ap_reg(a->op[1], &w2)) >= 0 && s < 31 && w2 == 'w')
				nex = ex[s] | EX_UP0;
			else if (a->def != 0)
				nex = EX_UP0;
		}
		for (r = 0; r < 31; r++)
			if (a->def & AP_BIT(r)) {
				ex[r] = 0;
				adrp[r][0] = '\0';
			}
		if (nex != 0) {
			d = ap_reg(a->op[0], &w);
			if (d >= 0 && d < 31)
				ex[d] = nex;
		}
		if (strcmp(mn, "adrp") == 0 && a->nop == 2) {
			d = ap_reg(a->op[0], &w);
			if (d >= 0 && d < 31)
				strcpy(adrp[d], a->op[1]);
		}
		/* the condition codes */
		if (ap_in(mn, "cmp cmn tst fcmp") && a->nop == 2) {
			snprintf(cmpkey, sizeof(cmpkey), "%s %s,%s", mn, a->op[0], a->op[1]);
			cmpvalid = 1;
			cmpreg[0] = ap_reg(a->op[0], &w);
			cmpreg[1] = ap_isreg(a->op[1]) ? ap_reg(a->op[1], &w) : -1;
		} else if (a->fdef) {
			cmpvalid = 0;
		} else if (cmpvalid) {
			if ((cmpreg[0] >= 0 && (a->def & AP_BIT(cmpreg[0]))) ||
			    (cmpreg[1] >= 0 && (a->def & AP_BIT(cmpreg[1]))))
				cmpvalid = 0;
		}
	}
	if (changed)
		P->dirty = 1;
	return changed;
}

/* ---- post-index addressing ---- */

static int
ap_postindex(struct ap *P)
{
	int i, changed = 0;

	ap_ensure(P);
	/* ldr Rt,[xb]; ...; add xb,xb,#k  and  add xb,xb,#k; ...; ldr Rt,[xb,#-k] */
	for (i = P->lo; i < P->hi; i++) {
		struct ai *a = &P->v[i];
		long k;
		int b, w, j, base;

		if (a->kind != AK_INS || a->nop != 3 || a->op[0][0] != 'x')
			continue;
		if (!((strcmp(a->mn, "add") == 0 || strcmp(a->mn, "sub") == 0) && strcmp(a->op[0], a->op[1]) == 0 &&
		    ap_num(a->op[2], &k)))
			continue;
		b = ap_reg(a->op[0], &w);
		if (b < 0 || b >= 29)
			continue;
		if (a->mn[0] == 's')
			k = -k;
		if (k < -256 || k > 255 || k == 0)
			continue;

		/* the access before the add */
		for (j = ap_prev(P, i); j >= P->lo; j = ap_prev(P, j)) {
			struct ai *m = &P->v[j];
			int rt, wt;

			if (m->kind != AK_INS)
				break;
			if (ap_is_ldst(m) && m->nop == 2 && ap_plain_mem(m->op[1], &base) && base == b) {
				rt = ap_reg(m->op[0], &wt);
				if (rt == b)
					break;
				sprintf(m->op[1], "[%s]", a->op[0]);
				sprintf(m->op[2], "#%ld", k);
				m->nop = 3;
				ap_dataflow(m);
				ap_kill(P, i);
				changed = 1;
				ap_ensure(P);
				break;
			}
			if (ap_touches(m, b) || ap_stops(m))
				break;
		}
		if (P->v[i].kind == AK_DEAD)
			continue;

		/* the access after the add, at the old address */
		for (j = ap_next(P, i); j >= 0 && j < P->hi; j = ap_next(P, j)) {
			struct ai *m = &P->v[j];
			struct ap_mem mm;
			int rt, wt;

			if (m->kind != AK_INS)
				break;
			if (ap_is_ldst(m) && m->nop == 2 && ap_parse_mem(m->op[1], &mm) && mm.base == b &&
			    mm.imm && mm.off == -k && !mm.pre && !mm.sym && mm.idx < 0) {
				rt = ap_reg(m->op[0], &wt);
				if (rt == b)
					break;
				sprintf(m->op[1], "[%s]", a->op[0]);
				sprintf(m->op[2], "#%ld", k);
				m->nop = 3;
				ap_dataflow(m);
				ap_kill(P, i);
				changed = 1;
				ap_ensure(P);
				break;
			}
			if (ap_touches(m, b) || ap_stops(m))
				break;
		}
	}
	/* mov xc,xb; add xb,xb,#k; ...; ldr Rt,[xc]: the copy is the old pointer */
	for (i = P->lo; i < P->hi; i++) {
		struct ai *c = &P->v[i], *a;
		int cr, wc, br, wb, j, ni, base;
		long k;

		if (c->kind != AK_INS || strcmp(c->mn, "mov") != 0 || c->nop != 2 || c->op[0][0] != 'x')
			continue;
		cr = ap_reg(c->op[0], &wc);
		br = ap_reg(c->op[1], &wb);
		if (cr < 0 || cr >= 29 || br < 0 || br >= 29 || wb != 'x' || cr == br)
			continue;
		ni = ap_next(P, i);
		if (ni < 0)
			continue;
		a = &P->v[ni];
		if (a->kind != AK_INS || a->nop != 3 || (strcmp(a->mn, "add") != 0 && strcmp(a->mn, "sub") != 0) ||
		    strcmp(a->op[0], c->op[1]) != 0 || strcmp(a->op[1], c->op[1]) != 0 || !ap_num(a->op[2], &k))
			continue;
		if (a->mn[0] == 's')
			k = -k;
		if (k < -256 || k > 255 || k == 0)
			continue;
		for (j = ap_next(P, ni); j >= 0 && j < P->hi; j = ap_next(P, j)) {
			struct ai *m = &P->v[j];
			int rt, wt;

			if (m->kind != AK_INS)
				break;
			if (ap_is_ldst(m) && m->nop == 2 && ap_plain_mem(m->op[1], &base) && base == cr) {
				rt = ap_reg(m->op[0], &wt);
				if (rt == br || (rt != cr && (AP_LOUT(P, j) & AP_BIT(cr))))
					break;
				sprintf(m->op[1], "[%s]", c->op[1]);
				sprintf(m->op[2], "#%ld", k);
				m->nop = 3;
				ap_dataflow(m);
				ap_kill(P, i);
				ap_kill(P, ni);
				changed = 1;
				ap_ensure(P);
				break;
			}
			if (ap_touches(m, br) || ap_touches(m, cr) || ap_stops(m))
				break;
		}
	}
	return changed;
}

/* ---- index registers ---- */

/* log2 of the size of a load or store, -1 if unknown */
static int
ap_ldst_shift(struct ai *m)
{
	int w = m->op[0][0];

	if (ap_in(m->mn, "ldrb strb ldrsb"))
		return 0;
	if (ap_in(m->mn, "ldrh strh ldrsh"))
		return 1;
	if (strcmp(m->mn, "ldrsw") == 0)
		return 2;
	if (strcmp(m->mn, "ldr") == 0 || strcmp(m->mn, "str") == 0) {
		if (w == 'w' || w == 's')
			return 2;
		if (w == 'x' || w == 'd')
			return 3;
	}
	return -1;
}

/* the shift in an "lsl #n" or "sxtw #n" operand, -1 if there is none */
static int
ap_shiftof(const char *e, const char *kw)
{
	size_t l = strlen(kw);

	if (strncmp(e, kw, l) != 0)
		return -1;
	if (e[l] == '\0')
		return 0;
	if (strncmp(e + l, " #", 2) == 0)
		return atoi(e + l + 2);
	return -1;
}

/*
 * The first instruction after i that reads or writes register t, if none of
 * the registers in keep is written before it and no label, branch or call
 * comes first; -1 otherwise.
 */
static int
ap_user(struct ap *P, int i, int t, ap_mask keep)
{
	int k;

	for (k = ap_next(P, i); k >= 0 && k < P->hi; k = ap_next(P, k)) {
		struct ai *m = &P->v[k];

		if (m->kind != AK_INS || ap_stops(m))
			return -1;
		if (ap_touches(m, t))
			return k;
		if (m->def & keep)
			return -1;
	}
	return -1;
}

static int
ap_index(struct ap *P)
{
	int i, changed = 0;

	ap_ensure(P);
	for (i = P->lo; i < P->hi; i++) {
		struct ai *a = &P->v[i], *b;
		int j, t, bd, ww;

		if (a->kind != AK_INS || a->nop < 2)
			continue;
		bd = ap_reg(a->op[0], &t);
		if (bd < 0 || bd >= 29)
			continue;

		/*
		 * sxtw xT,wS (or mov wT,wS, a zero extension) followed by the use of xT as
		 * the last operand of add, sub or cmp: the extension goes into the operand.
		 */
		if (((strcmp(a->mn, "sxtw") == 0 && a->nop == 2 && t == 'x') ||
		    (strcmp(a->mn, "mov") == 0 && a->nop == 2 && t == 'w')) && a->op[1][0] == 'w') {
			int s = ap_reg(a->op[1], &ww), ci, n;
			const char *kind = a->mn[0] == 's' ? "sxtw" : "uxtw";
			char tx[8];

			if (s < 0 || s >= 31)
				continue;
			sprintf(tx, "x%d", bd);
			j = ap_user(P, i, bd, AP_BIT(s));
			if (j < 0)
				continue;
			b = &P->v[j];
			/* ldr Rt,[xB,xT{,lsl #n}] with xT the extension: [xB,wS,sxtw{ #n}] */
			if (ap_is_ldst(b) && b->nop == 2) {
				struct ap_mem mm;
				int sh = ap_ldst_shift(b), rt, wt, nn = 0;
				const char *e;

				if (sh >= 0 && ap_parse_mem(b->op[1], &mm) && mm.idx == bd && mm.base != bd && !mm.pre &&
				    !mm.imm) {
					rt = ap_reg(b->op[0], &wt);
					e = strstr(b->op[1], "lsl #");
					if (e != NULL)
						nn = atoi(e + 5);
					if ((rt == bd || !(AP_LOUT(P, j) & AP_BIT(bd))) && (nn == 0 || nn == sh) &&
					    (e != NULL || strchr(b->op[1], ',') == strrchr(b->op[1], ','))) {
						char bs[8], nm[96];

						sprintf(bs, "x%d", mm.base);
						if (nn)
							snprintf(nm, sizeof(nm), "[%s,%s,%s #%d]", bs, a->op[1], kind, nn);
						else
							snprintf(nm, sizeof(nm), "[%s,%s,%s]", bs, a->op[1], kind);
						strcpy(b->op[1], nm);
						ap_dataflow(b);
						ap_kill(P, i);
						changed = 1;
						ap_ensure(P);
					}
				}
				continue;
			}
			ci = strcmp(b->mn, "cmp") == 0 || strcmp(b->mn, "cmn") == 0;
			if (!(((strcmp(b->mn, "add") == 0 || strcmp(b->mn, "sub") == 0) && (b->nop == 3 || b->nop == 4) &&
			    b->op[0][0] == 'x' && b->op[1][0] == 'x' && strcmp(b->op[2], tx) == 0 &&
			    strcmp(b->op[1], tx) != 0) || (ci && b->nop == 2 && b->op[0][0] == 'x' &&
			    strcmp(b->op[1], tx) == 0 && strcmp(b->op[0], tx) != 0)))
				continue;
			if (ci) {
				char r0[72], e[24];

				if (AP_LOUT(P, j) & AP_BIT(bd) && 0)
					continue;
				if ((AP_LOUT(P, j) & AP_BIT(bd)) != 0)
					continue;
				strcpy(r0, b->op[0]);
				sprintf(e, "%s", kind);
				{
					char mn[8], w2[72];

					strcpy(mn, b->mn);
					strcpy(w2, a->op[1]);
					ap_setops(P, b, mn, r0, w2, e, NULL);
				}
			} else {
				int d = ap_reg(b->op[0], &ww);

				n = b->nop == 3 ? 0 : ap_shiftof(b->op[3], "lsl");
				if (n < 0 || n > 4 || !(d == bd || !(AP_LOUT(P, j) & AP_BIT(bd))))
					continue;
				{
					char x[72], bb[72], dd[72], ext[24], mn[8];

					strcpy(bb, b->op[1]); strcpy(dd, b->op[0]); strcpy(x, a->op[1]); strcpy(mn, b->mn);
					if (n)
						sprintf(ext, "%s #%d", kind, n);
					else
						strcpy(ext, kind);
					ap_setops(P, b, mn, dd, bb, x, ext);
				}
			}
			ap_kill(P, i);
			changed = 1;
			ap_ensure(P);
			continue;
		}
		if (t != 'x')
			continue;
		/* lsl xT,xS,#n; add xD,xB,xT -> add xD,xB,xS,lsl #n */
		if (strcmp(a->mn, "lsl") == 0 && a->nop == 3 && a->op[1][0] == 'x' && a->op[2][0] == '#') {
			int s = ap_reg(a->op[1], &ww);

			if (s < 0 || s >= 31)
				continue;
			j = ap_user(P, i, bd, AP_BIT(s));
			if (j < 0)
				continue;
			b = &P->v[j];
			if (strcmp(b->mn, "add") == 0 && b->nop == 3 && b->op[0][0] == 'x' && b->op[1][0] == 'x' &&
			    strcmp(b->op[2], a->op[0]) == 0 && strcmp(b->op[1], a->op[0]) != 0) {
				int d = ap_reg(b->op[0], &t);

				if (d == bd || !(AP_LOUT(P, j) & AP_BIT(bd))) {
					char x[72], bb[72], dd[72], sh[24];

					strcpy(bb, b->op[1]); strcpy(dd, b->op[0]); strcpy(x, a->op[1]);
					sprintf(sh, "lsl %s", a->op[2]);
					ap_setops(P, b, "add", dd, bb, x, sh);
					ap_kill(P, i);
					changed = 1;
					ap_ensure(P);
				}
			}
			continue;
		}
		/* add xT,xB,xI{,ext}; ldr/str Rt,[xT] -> ldr/str Rt,[xB,xI{,ext}] */
		if (strcmp(a->mn, "add") == 0 && (a->nop == 3 || a->nop == 4) && a->op[1][0] == 'x' &&
		    ap_isreg(a->op[2])) {
			int sh, base, n, rt, wt, bb_ = ap_reg(a->op[1], &ww), ii = ap_reg(a->op[2], &ww);
			char mem[96];
			ap_mask keep;

			if (bb_ < 0 || ii < 0)
				continue;
			keep = AP_BIT(bb_) | AP_BIT(ii);
			j = ap_user(P, i, bd, keep);
			if (j < 0)
				continue;
			b = &P->v[j];
			if (!ap_is_ldst(b) || b->nop != 2)
				continue;
			sh = ap_ldst_shift(b);
			if (sh < 0 || !ap_plain_mem(b->op[1], &base) || base != bd)
				continue;
			rt = ap_reg(b->op[0], &wt);
			if (rt == bd || (AP_LOUT(P, j) & AP_BIT(bd)))
				continue;
			if (a->nop == 3) {
				if (a->op[2][0] != 'x')
					continue;
				snprintf(mem, sizeof(mem), "[%s,%s]", a->op[1], a->op[2]);
			} else if (a->op[2][0] == 'x' && (n = ap_shiftof(a->op[3], "lsl")) >= 0) {
				if (n == 0)
					snprintf(mem, sizeof(mem), "[%s,%s]", a->op[1], a->op[2]);
				else if (n == sh)
					snprintf(mem, sizeof(mem), "[%s,%s,lsl #%d]", a->op[1], a->op[2], n);
				else
					continue;
			} else if (a->op[2][0] == 'w' && (n = ap_shiftof(a->op[3], "sxtw")) >= 0) {
				if (n == 0)
					snprintf(mem, sizeof(mem), "[%s,%s,sxtw]", a->op[1], a->op[2]);
				else if (n == sh)
					snprintf(mem, sizeof(mem), "[%s,%s,sxtw #%d]", a->op[1], a->op[2], n);
				else
					continue;
			} else
				continue;
			strcpy(b->op[1], mem);
			ap_dataflow(b);
			ap_kill(P, i);
			changed = 1;
			ap_ensure(P);
		}
	}
	return changed;
}

/* ---- a conditional value ---- */

static const char *
ap_invert(const char *cc)
{
	static const char *t[][2] = {
		{ "eq", "ne" }, { "hs", "lo" }, { "mi", "pl" }, { "vs", "vc" },
		{ "hi", "ls" }, { "ge", "lt" }, { "gt", "le" },
	};
	int i;

	for (i = 0; i < (int)(sizeof(t) / sizeof(t[0])); i++) {
		if (strcmp(cc, t[i][0]) == 0)
			return t[i][1];
		if (strcmp(cc, t[i][1]) == 0)
			return t[i][0];
	}
	return NULL;
}

static int
ap_label_refs(struct ap *P, int lab)
{
	int i, n = 0;

	for (i = P->lo; i < P->hi; i++)
		if (P->v[i].kind == AK_INS && ap_isbranch(P->v[i].mn) && P->v[i].tgt == lab)
			n++;
	return n;
}

static int
ap_cset(struct ap *P)
{
	int i, changed = 0;

	ap_ensure(P);
	for (i = P->lo; i < P->hi; i++) {
		/* b.cc La; movz wD,#v1; b Lb; La: movz wD,#v2; Lb: with {v1,v2} = {0,1} */
		struct ai *br = &P->v[i], *m1, *jb, *m2;
		int i1, i2, i3, i4, i5, v1, v2;
		long v;
		const char *inv;
		char reg[16], cc[8];

		if (br->kind != AK_INS || br->tgt < 0 ||
		    !(strncmp(br->mn, "b.", 2) == 0 || ap_in(br->mn, "cbz cbnz tbz tbnz")))
			continue;
		i1 = ap_next(P, i);
		i2 = i1 >= 0 ? ap_next(P, i1) : -1;
		i3 = i2 >= 0 ? ap_next(P, i2) : -1;
		i4 = i3 >= 0 ? ap_next(P, i3) : -1;
		i5 = i4 >= 0 ? ap_next(P, i4) : -1;
		if (i5 < 0)
			continue;
		m1 = &P->v[i1]; jb = &P->v[i2]; m2 = &P->v[i4];
		if (m1->kind != AK_INS || jb->kind != AK_INS || P->v[i3].kind != AK_LABEL ||
		    m2->kind != AK_INS || P->v[i5].kind != AK_LABEL)
			continue;
		if (strcmp(m1->mn, "movz") != 0 || strcmp(m2->mn, "movz") != 0 || strcmp(jb->mn, "b") != 0 ||
		    m1->nop != 2 || m2->nop != 2 || strcmp(m1->op[0], m2->op[0]) != 0 || m1->op[0][0] != 'w')
			continue;
		if (jb->tgt != i5 || br->tgt != i3)
			continue;
		if (!ap_num(m1->op[1], &v))
			continue;
		v1 = (int)v;
		if (!ap_num(m2->op[1], &v))
			continue;
		v2 = (int)v;
		if (!((v1 == 1 && v2 == 0) || (v1 == 0 && v2 == 1)))
			continue;
		if (ap_label_refs(P, i3) != 1 || ap_label_refs(P, i5) != 1)
			continue;
		/* taken gives v2, not taken gives v1 */
		strcpy(reg, m1->op[0]);
		if (strncmp(br->mn, "b.", 2) == 0) {
			strcpy(cc, br->mn + 2);
			inv = v1 == 1 ? ap_invert(cc) : cc;
			if (inv == NULL)
				continue;
			ap_setops(P, m1, "cset", reg, inv, NULL, NULL);
		} else {
			/* the test of a register or a bit: the value is whether it is set */
			int zero = strcmp(br->mn, "cbz") == 0 || strcmp(br->mn, "tbz") == 0;	/* taken when zero / clear */
			int one_if_set = (zero ? v1 == 1 : v2 == 1);	/* 1 when the register or bit is nonzero */
			char src[72], bit[16];

			strcpy(src, br->op[0]);
			if (br->mn[0] == 't') {
				if (AP_FOUT(P, i5) && !one_if_set)
					continue;
				strcpy(bit, br->op[1] + 1);
				if (one_if_set) {
					char b2[24];

					sprintf(b2, "#%s", bit);
					if (src[0] == 'x')	/* ubfx needs both of one width */
						reg[0] = 'x';
					ap_setops(P, m1, "ubfx", reg, src, b2, "#1");
					ap_kill(P, i2);
				} else {
					char mk[40], t2[72];

					/* tst wN,#(1<<bit); cset wD,eq */
					sprintf(mk, "#0x%llx", 1ULL << atoi(bit));
					strcpy(t2, src);
					ap_setops(P, m1, "tst", t2, mk, NULL, NULL);
					P->v[i2].kind = AK_INS;
					ap_setops(P, jb, "cset", reg, "eq", NULL, NULL);
				}
			} else {
				if (AP_FOUT(P, i5))
					continue;
				{
					char t2[72];

					strcpy(t2, src);
					ap_setops(P, m1, "cmp", t2, "#0", NULL, NULL);
					P->v[i2].kind = AK_INS;
					ap_setops(P, jb, "cset", reg, one_if_set ? "ne" : "eq", NULL, NULL);
				}
			}
			ap_kill(P, i);
			ap_kill(P, i3);
			ap_kill(P, i4);
			ap_kill(P, i5);
			changed = 1;
			ap_ensure(P);
			continue;
		}
		ap_kill(P, i);
		ap_kill(P, i2);
		ap_kill(P, i3);
		ap_kill(P, i4);
		ap_kill(P, i5);
		changed = 1;
		ap_ensure(P);
	}
	return changed;
}

/* ---- driver ---- */

static void
ap_function(struct ap *P)
{
	int round, n = P->hi - P->lo;

	P->lin = malloc((n + 1) * sizeof(ap_mask));
	P->lout = malloc((n + 1) * sizeof(ap_mask));
	P->fin = malloc(n + 1);
	P->fout = malloc(n + 1);
	P->dirty = 1;
	for (round = 0; round < 8; round++) {
		int c = 0;

		ap_ensure(P);
		c |= ap_copyprop(P);
		c |= ap_dce(P);
		c |= ap_scan(P);
		c |= ap_cmpbr(P);
		c |= ap_postindex(P);
		c |= ap_index(P);
		c |= ap_cset(P);
		c |= ap_dce(P);
		if (!c)
			break;
		P->dirty = 1;
	}
	free(P->lin); free(P->lout); free(P->fin); free(P->fout);
}

static void
ap_emit(struct ap *P, FILE *out)
{
	int i, j;

	for (i = 0; i < P->n; i++) {
		struct ai *a = &P->v[i];

		switch (a->kind) {
		case AK_DEAD:
			break;
		case AK_OTHER:
		case AK_LABEL:
			fprintf(out, "%s\n", a->txt);
			break;
		case AK_INS:
			fprintf(out, "\t%s", a->mn);
			for (j = 0; j < a->nop; j++)
				fprintf(out, "%s%s", j == 0 ? " " : ",", a->op[j]);
			fprintf(out, "\n");
			break;
		}
	}
}

/* Read the assembly in buf, write it optimized. */
static void
a64_peep(char *buf, FILE *out)
{
	struct ap P;
	char *p, *e;
	int i;

	memset(&P, 0, sizeof(P));
	for (p = buf; *p; p = e + 1) {
		char sv;

		e = strchr(p, '\n');
		if (e == NULL)
			e = p + strlen(p);
		sv = *e;
		*e = '\0';
		ap_parse_line(&P, p);
		*e = sv;
		if (*e == '\0')
			break;
	}
	/* a function: a run of instructions and labels */
	i = 0;
	while (i < P.n) {
		int j;

		if (P.v[i].kind == AK_OTHER) {
			i++;
			continue;
		}
		for (j = i; j < P.n && P.v[j].kind != AK_OTHER; j++)
			;
		P.lo = i;
		P.hi = j;
		ap_function(&P);
		i = j;
	}
	ap_emit(&P, out);
	for (i = 0; i < P.n; i++)
		free(P.v[i].txt);
	free(P.v);
}
