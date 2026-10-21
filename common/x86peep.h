/*
 * x86peep.h -- a text pass over the assembly of a whole compilation unit,
 * shared by the i386 and amd64 backends (-xdmr).  The file is included, not
 * linked: everything is static.
 *
 * (Loop rotation, which used to be here, is done on the IR by looprot() in
 * mip/optim2.c, for every target.)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct xp_lines {
	char **v;
	int n;
};

/* a tab-indented conditional jump "jcc target": mnemonic and target */
static int
xp_condjump(const char *s, char *mn, char *tgt)
{
	if (s[0] != '\t' || s[1] != 'j' || strncmp(s + 1, "jmp", 3) == 0)
		return 0;
	return sscanf(s + 1, "%15s %63s", mn, tgt) == 2;
}

/*
 * "testl %r,%r" right after an instruction that left the codes of %r, and
 * before a branch that looks only at those codes, is redundant:
 *	decl %edx; testl %edx,%edx; jl X
 * The ops that clear OF and CF like test does (and, or, xor) serve every
 * branch; add, sub, inc and dec set OF and CF differently, so they serve the
 * branches on Z and S only (and the signed ones, whose overflow case is
 * undefined behaviour in C).
 */
static int
xp_dest(const char *s, const char *ops[], int n, const char **opnd, char *op)
{
	int i;

	if (s[0] != '\t')
		return 0;
	for (i = 0; i < n; i++) {
		size_t l = strlen(ops[i]);

		if (strncmp(s + 1, ops[i], l) == 0 && s[1 + l] == ' ') {
			strcpy(op, ops[i]);
			*opnd = s + 2 + l;
			return 1;
		}
	}
	return 0;
}

static void
xp_deltest(struct xp_lines *L)
{
	static const char *arith[] = { "decl", "incl", "addl", "subl", "decq", "incq", "addq", "subq" };
	static const char *logic[] = { "andl", "orl", "xorl", "andq", "orq", "xorq" };
	int i, o, w;
	char op[8], mn[16], tgt[64], reg[16], t[64];
	const char *opnd, *c;

	for (i = 0; i + 2 < L->n; i++) {
		int islogic;

		if (xp_dest(L->v[i], arith, 8, &opnd, op))
			islogic = 0;
		else if (xp_dest(L->v[i], logic, 6, &opnd, op))
			islogic = 1;
		else
			continue;
		/* the destination register is the last operand */
		c = strrchr(opnd, ',');
		c = c != NULL ? c + 1 : opnd;
		if (c[0] != '%' || strlen(c) > 8 || strchr(c, '(') != NULL)
			continue;
		strcpy(reg, c);
		w = op[3] == 'q' || op[3] == 'l' ? op[3] : 'l';
		snprintf(t, sizeof(t), "\ttest%c %s,%s", w, reg, reg);
		if (strcmp(L->v[i + 1], t) != 0)
			continue;
		if (!xp_condjump(L->v[i + 2], mn, tgt))
			continue;
		/*
		 * The signed relations (jl, jge, ...) look at OF: after an
		 * add/sub/inc/dec that wraps (an unsigned value tested as an
		 * int) they would answer differently from the test.
		 */
		if (!islogic && strcmp(mn, "je") && strcmp(mn, "jne") &&
		    strcmp(mn, "jz") && strcmp(mn, "jnz") && strcmp(mn, "js") &&
		    strcmp(mn, "jns"))
			continue;
		if (islogic && (strcmp(mn, "jp") == 0 || strcmp(mn, "jnp") == 0))
			continue;
		/* drop the test */
		free(L->v[i + 1]);
		for (o = i + 1; o + 1 < L->n; o++)
			L->v[o] = L->v[o + 1];
		L->n--;
	}
}

#if defined(mach_amd64)
/*
 * (-xdmr) amd64: a function with a frame pointer, and no push or %rsp in its body, does without one.
 *
 *	pushq %rbp; movq %rsp,%rbp; subq $S,%rsp; movq %rbx,-8(%rbp) ...    ... movq -8(%rbp),%rbx; leave; ret
 * becomes
 *	pushq %rbx ...; subq $A,%rsp  ...    ... addq $A,%rsp; popq %rbx ...; ret
 * and every N(%rbp) in the body is N'(%rsp).  The saved registers are pushed (one byte, two with a REX
 * prefix), not stored into the frame (four or five bytes, and the same to load).  A is a multiple of 16
 * so that an object keeps its alignment; a dummy push makes the number of pushes odd, so that %rsp is
 * 16-byte aligned at a call (the return address and the pushes are an odd number of words).
 */
static int
xp_isregsave(const char *s, char *reg, int *off, int restore)
{
	/* "\tmovq %rbx,-8(%rbp)"  or, restore, "\tmovq -8(%rbp),%rbx" */
	char a[32], b[32];
	int n;

	if (strncmp(s, "\tmovq ", 6) != 0)
		return 0;
	n = 0;
	if (!restore) {
		if (sscanf(s + 6, "%%%15[a-z0-9],%d(%%rbp)%n", a, off, &n) != 2 || s[6 + n] != '\0')
			return 0;
	} else {
		if (sscanf(s + 6, "%d(%%rbp),%%%15[a-z0-9]%n", off, a, &n) != 2 || s[6 + n] != '\0')
			return 0;
	}
	(void)b;
	if (strcmp(a, "rbx") && strcmp(a, "r12") && strcmp(a, "r13") && strcmp(a, "r14") && strcmp(a, "r15"))
		return 0;
	strcpy(reg, a);
	return 1;
}

/* the line with every N(%rbp) rewritten as M(%rsp); 0 if %rbp or %rsp is used in another way */
static int
xp_fixrbp(const char *line, char *out, int A, int kp)
{
	const char *p = line;
	char *o = out;

	while (*p) {
		if (strncmp(p, "%rsp", 4) == 0 || (strncmp(p, "%rbp", 4) == 0 && !(p > line && p[-1] == '(')))
			return 0;
		if (strncmp(p, "%rbp", 4) == 0) {
			/* backwards over  N( : the displacement already copied to out */
			const char *b = p - 1;		/* the "(" */
			int nd = 0, neg = 0, v;
			char dig[16];

			if (b == line)
				return 0;
			b--;
			while (b >= line && b[0] >= '0' && b[0] <= '9')
				b--, nd++;
			if (b >= line && b[0] == '-')
				neg = 1, b--;
			if (nd == 0 || nd > 9 || (b >= line && (isalnum((unsigned char)b[0]) || b[0] == '_' || b[0] == '.')))
				return 0;
			memcpy(dig, p - 1 - nd, nd);
			dig[nd] = '\0';
			v = atoi(dig);
			if (v == 0)
				return 0;
			v = neg ? A - v : A + 8 * kp - 8 + v;
			o -= nd + neg + 1;	/* the "N(" or "-N(" already copied */
			if (v == 0)
				o += sprintf(o, "(%%rsp");
			else
				o += sprintf(o, "%d(%%rsp", v);
			p += 4;
			continue;
		}
		*o++ = *p++;
	}
	*o = '\0';
	return 1;
}

static void
xp_frame64(struct xp_lines *L)
{
	int i, j, k, n, S, off, nsave, ok, A, kp, e;
	char regs[8][8], t[1200], reg[16];
	int offs[8];
	struct xp_lines N;
	int cap = L->n + 64;

	N.v = malloc(cap * sizeof(char *));
	N.n = 0;
#define	PUSH(str) do { if (N.n == cap) N.v = realloc(N.v, (cap *= 2) * sizeof(char *)); N.v[N.n++] = strdup(str); } while (0)
	for (i = 0; i < L->n; i++) {
		if (strcmp(L->v[i], "\tpushq %rbp") != 0 || i + 1 >= L->n || strcmp(L->v[i+1], "\tmovq %rsp,%rbp") != 0) {
			PUSH(L->v[i]);
			continue;
		}
		j = i + 2;
		S = 0;
		if (j < L->n && sscanf(L->v[j], "\tsubq $%d,%%rsp", &S) == 1)
			j++;
		nsave = 0;
		while (j < L->n && nsave < 8 && xp_isregsave(L->v[j], reg, &off, 0)) {
			strcpy(regs[nsave], reg);
			offs[nsave++] = off < 0 ? -off : off;	/* from %rbp downward */
			j++;
		}
		/* the function ends at its .size */
		for (e = j; e < L->n && strncmp(L->v[e], "\t.size", 6) != 0; e++)
			if (strcmp(L->v[e], "\tpushq %rbp") == 0)
				break;
		if (e >= L->n || strncmp(L->v[e], "\t.size", 6) != 0)
			goto keep;
		/* the body: no %rsp, no push or pop, only N(%rbp) for %rbp, each leave behind its restores and before a ret */
		ok = 1;
		for (k = j; k < e && ok; k++) {
			const char *s = L->v[k];

			if (strcmp(s, "\tleave") == 0) {
				int r;

				if (k + 1 >= e || strcmp(L->v[k+1], "\tret") != 0) {
					ok = 0;
					break;
				}
				for (r = 0; r < nsave; r++) {
					const char *q = L->v[k - 1 - r];
					char rg[16];
					int of2;

					/* restores come in the order of the saves, just before the leave */
					if (k - 1 - r < j || !xp_isregsave(q, rg, &of2, 1) || (of2 < 0 ? -of2 : of2) != offs[nsave - 1 - r] ||
					    strcmp(rg, regs[nsave - 1 - r]) != 0)
						ok = 0;
				}
				continue;
			}
			if (strstr(s, "pushq") || strstr(s, "popq") || strstr(s, "ret $") || strstr(s, "enter") ||
			    (s[0] == '\t' && s[1] == '.'))
				ok = 0;
			else if (!xp_fixrbp(s, t, 0, 0))
				ok = 0;
		}
		if (!ok)
			goto keep;
		/* the layout */
		kp = nsave;
		{
			int auto_sz = nsave ? offs[0] - 8 : S;
			int lo = nsave ? offs[0] : 0, q2;

			for (q2 = 0; q2 < nsave; q2++)
				if (offs[q2] < lo)
					lo = offs[q2];
			if (nsave)
				auto_sz = lo - 8;
			A = (auto_sz + 15) & ~15;
		}
		if ((kp & 1) == 0)
			kp++;		/* the dummy push: an odd number of words besides the return address */
		for (k = 0; k < nsave; k++) {
			snprintf(t, sizeof t, "\tpushq %%%s", regs[k]);
			PUSH(t);
		}
		if (kp != nsave)
			PUSH("\tpushq %rax");
		if (A) {
			snprintf(t, sizeof t, "\tsubq $%d,%%rsp", A);
			PUSH(t);
		}
		for (k = j; k < e; k++) {
			const char *s = L->v[k];

			if (strcmp(s, "\tleave") == 0) {
				int r;

				/* the restores just before it are already in N: take them back, then pop */
				N.n -= 0;
				for (r = 0; r < nsave; r++)
					free(N.v[--N.n]);
				if (A) {
					snprintf(t, sizeof t, "\taddq $%d,%%rsp", A);
					PUSH(t);
				}
				if (kp != nsave)
					PUSH("\tpopq %rcx");
				for (r = nsave - 1; r >= 0; r--) {
					snprintf(t, sizeof t, "\tpopq %%%s", regs[r]);
					PUSH(t);
				}
				continue;			/* the ret follows */
			}
			xp_fixrbp(s, t, A, kp);
			PUSH(t);
		}
		i = e - 1;
		continue;
	keep:
		PUSH(L->v[i]);
	}
	for (i = 0; i < L->n; i++)
		free(L->v[i]);
	free(L->v);
	*L = N;
	(void)n;
}
#endif

/*
 * Small rewrites of single instructions and of a branch:
 *	xorq %rax,%rax		-> xorl %eax,%eax	(same effect, no REX prefix)
 *	movzbq m,%rax		-> movzbl m,%eax	(writing the low half clears the high half)
 *	jmp L ... L: ret	-> ret			(a jump of two bytes to a return of one)
 */
static int
xp_reg64to32(const char *r, char *out)
{
	static const char *const n64[] = { "%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi", NULL };
	static const char *const n32[] = { "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi", NULL };
	int i;

	for (i = 0; n64[i]; i++)
		if (strcmp(r, n64[i]) == 0) {
			strcpy(out, n32[i]);
			return 1;
		}
	return 0;
}

static void
xp_small(struct xp_lines *L)
{
	int i, j;
	char r[16], r32[16], t[256], *c;

	for (i = 0; i < L->n; i++) {
		char *s = L->v[i];

		if (strncmp(s, "\txorq ", 6) == 0) {
			/* xorq %r,%r with the same register */
			if (sscanf(s + 6, "%15[^,],%15s", r, t) == 2 && strcmp(r, t) == 0 && xp_reg64to32(r, r32)) {
				snprintf(t, sizeof t, "\txorl %s,%s", r32, r32);
				free(L->v[i]);
				L->v[i] = strdup(t);
			}
		} else if (strncmp(s, "\tmovzbq ", 8) == 0 || strncmp(s, "\tmovzwq ", 8) == 0) {
			/* movzbq src,%reg */
			c = strrchr(s, ',');
			if (c != NULL && xp_reg64to32(c + 1, r32)) {
				snprintf(t, sizeof t, "\tmovz%cl %.*s,%s", s[5], (int)(c - (s + 8)), s + 8, r32);
				free(L->v[i]);
				L->v[i] = strdup(t);
			}
		}
	}
	/* jmp to a label that is followed by ret (labels directly after each other count as one) */
	for (i = 0; i < L->n; i++) {
		char lab[64];

		if (sscanf(L->v[i], "\tjmp %63s", lab) != 1 || lab[0] != '.')
			continue;
		snprintf(t, sizeof t, "%s:", lab);
		for (j = 0; j < L->n; j++)
			if (strcmp(L->v[j], t) == 0)
				break;
		if (j >= L->n)
			continue;
		for (j++; j < L->n && L->v[j][0] == '.' && strchr(L->v[j], ':') != NULL && L->v[j][strlen(L->v[j]) - 1] == ':'; j++)
			;
		if (j < L->n && strcmp(L->v[j], "\tret") == 0) {
			free(L->v[i]);
			L->v[i] = strdup("\tret");
		}
	}
}

/*
 * A register copy that undoes one just made, and a register cleared twice:
 *	movq %rdi,%r12 ... movq %r12,%rdi	(neither written in between)
 *	xorl %eax,%eax ... xorl %eax,%eax
 * The registers are compared with their parts as one (%eax is %rax).
 */
static int
xp_regid(const char *r)
{
	static const char *const base[] = { "ax", "bx", "cx", "dx", "si", "di", "bp", "sp" };
	int i, n;

	if (r[0] != '%')
		return -1;
	r++;
	for (i = 0; i < 8; i++) {
		const char *b = base[i];

		if (r[0] == 'r' && strncmp(r + 1, b, 2) == 0 && r[3] == '\0')
			return i;
		if (r[0] == 'e' && strncmp(r + 1, b, 2) == 0 && r[3] == '\0')
			return i;
		if (strncmp(r, b, 2) == 0 && r[2] == '\0')
			return i;
	}
	/* %al %ah %bl ... */
	if ((r[0] == 'a' || r[0] == 'b' || r[0] == 'c' || r[0] == 'd') && (r[1] == 'l' || r[1] == 'h') && r[2] == '\0')
		return r[0] - 'a' == 0 ? 0 : r[0] - 'a' == 1 ? 1 : r[0] - 'a' == 2 ? 2 : 3;
	if ((strncmp(r, "sil", 3) == 0 || strncmp(r, "dil", 3) == 0 || strncmp(r, "bpl", 3) == 0 || strncmp(r, "spl", 3) == 0) &&
	    r[3] == '\0')
		return r[0] == 's' && r[1] == 'i' ? 4 : r[0] == 'd' ? 5 : r[0] == 'b' ? 6 : 7;
	if (r[0] == 'r' && r[1] >= '0' && r[1] <= '9') {
		n = atoi(r + 1);
		if (n >= 8 && n <= 15) {
			const char *q = r + 1;

			while (*q >= '0' && *q <= '9')
				q++;
			if (*q == '\0' || ((*q == 'd' || *q == 'w' || *q == 'b') && q[1] == '\0'))
				return n;
		}
	}
	return -1;
}

/* does this instruction line possibly write register id?  1 also when it cannot be told */
static int
xp_writes(const char *s, int id)
{
	static const char *const known[] = { "mov", "lea", "add", "sub", "and", "or", "xor", "imul", "shl", "sal",
	    "shr", "sar", "rol", "ror", "neg", "not", "inc", "dec", "sbb", "adc", "cmov", "set", "cmp", "test",
	    "bt", NULL };
	char mn[24];
	const char *c, *e;
	int i, ok = 0, nop = 0;

	if (s[0] != '\t')
		return 1;
	for (i = 0, c = s + 1; *c && *c != ' ' && i < 23; c++)
		mn[i++] = *c;
	mn[i] = '\0';
	for (i = 0; known[i]; i++)
		if (strncmp(mn, known[i], strlen(known[i])) == 0)
			ok = 1;
	if (strncmp(mn, "movs", 4) == 0 && strlen(mn) <= 5 && mn[3] == 's')
		return 1;			/* movsb, movsq... string moves; movsd/movss are SSE below */
	if (strncmp(mn, "movs", 4) == 0 && (mn[4] == 'd' || mn[4] == 's') && mn[5] == '\0')
		ok = 0;				/* SSE move: no general register is written unless it is the target */
	if (strncmp(mn, "rep", 3) == 0 || strncmp(mn, "call", 4) == 0 || mn[0] == 'j' || strncmp(mn, "ret", 3) == 0)
		return 1;
	if (!ok)
		return 1;
	if (*c == '\0')
		return 1;			/* an instruction without operands */
	e = c + strlen(c);
	{
		const char *q;

		for (q = c; *q; q++)
			if (*q == ',')
				nop++;
	}
	if (strncmp(mn, "cmp", 3) == 0 || strncmp(mn, "test", 4) == 0 || strncmp(mn, "bt", 2) == 0)
		return 0;
	if (nop == 0 && (strncmp(mn, "imul", 4) == 0 || strncmp(mn, "mul", 3) == 0))
		return 1;
	/* the destination: the last operand */
	for (c = e; c > s && c[-1] != ','; c--)
		;
	if (c[0] == '%') {
		int r = xp_regid(c);

		if (r < 0)
			return 0;		/* an xmm register, say */
		return r == id;
	}
	return 0;				/* memory */
}

/* is the label named on this line (".L12:") used by a jump or anything else? */
static int
xp_labelused(struct xp_lines *L, const char *labc)
{
	char lab[64];
	size_t n = strlen(labc) - 1;		/* without the colon */
	int i;
	const char *q;

	if (n >= sizeof lab)
		return 1;
	memcpy(lab, labc, n);
	lab[n] = '\0';
	for (i = 0; i < L->n; i++) {
		if (L->v[i][0] != '\t')
			continue;
		for (q = L->v[i]; (q = strstr(q, lab)) != NULL; q++)
			if (!(q[n] >= '0' && q[n] <= '9'))
				return 1;
	}
	return 0;
}

static void
xp_copies(struct xp_lines *L)
{
	int i, j, a, b, n;
	char ra[16], rb[16], t[64];

	for (i = 0; i < L->n; i++) {
		const char *s = L->v[i];
		int zero = 0;

		if (sscanf(s, "\tmovq %15[^,],%15s", ra, rb) == 2 && (a = xp_regid(ra)) >= 0 && (b = xp_regid(rb)) >= 0 && a != b) {
			snprintf(t, sizeof t, "\tmovq %s,%s", rb, ra);
		} else if (sscanf(s, "\txorl %15[^,],%15s", ra, rb) == 2 && strcmp(ra, rb) == 0 && (a = xp_regid(ra)) >= 0) {
			zero = 1;
			b = a;
			snprintf(t, sizeof t, "%s", s);
		} else
			continue;
		for (j = i + 1, n = 0; j < L->n && n < 16; j++, n++) {
			if (L->v[j][0] == '\0')
				continue;
			if (L->v[j][0] == '.' && L->v[j][strlen(L->v[j]) - 1] == ':' && !xp_labelused(L, L->v[j]))
				continue;		/* a label nothing jumps to */
			if (strcmp(L->v[j], t) == 0) {
				if (zero) {
					/* the flags: the next line may not read them */
					const char *nx = j + 1 < L->n ? L->v[j+1] : "";

					if (nx[0] == '\t' && (nx[1] == 'j' || strncmp(nx + 1, "set", 3) == 0 ||
					    strncmp(nx + 1, "cmov", 4) == 0 || strncmp(nx + 1, "adc", 3) == 0 ||
					    strncmp(nx + 1, "sbb", 3) == 0))
						break;
				}
				L->v[j][0] = '\0';	/* drop it: an empty line, taken out below */
				continue;
			}
			if (xp_writes(L->v[j], a) || (!zero && xp_writes(L->v[j], b)))
				break;
		}
	}
}

/*
 * Arguments passed on the stack:
 *	subq $16,%rsp; movl %r14d,(%rsp)		->	pushq %rax; pushq %r14
 *	subq $16,%rsp; movq %rax,(%rsp); movl $3,8(%rsp)	->	pushq $3; pushq %rax
 * a push is one or two bytes, the store three to seven and the subtraction four.  The slots are the stores
 * that follow the subtraction at once (their sources registers or constants); an unused slot at the top
 * (the padding to 16 bytes) is a push of anything.
 */
static int
xp_to64(const char *r, char *out)
{
	/* %eax -> %rax, %r14d -> %r14, %rax stays */
	size_t n = strlen(r);

	if (r[0] != '%' || n < 3 || n > 6)
		return 0;
	if (r[1] == 'e' && n == 4) {
		out[0] = '%';
		out[1] = 'r';
		strcpy(out + 2, r + 2);
		return 1;
	}
	if (r[1] == 'r' && r[2] >= '0' && r[2] <= '9') {
		strcpy(out, r);
		if (out[n - 1] == 'd' || out[n - 1] == 'w' || out[n - 1] == 'b')
			out[n - 1] = '\0';
		return 1;
	}
	if (r[1] == 'r' && n == 4) {
		strcpy(out, r);
		return 1;
	}
	return 0;
}

static void
xp_stackargs(struct xp_lines *L)
{
	int i, j, k, N, m, off, ok, expect, idx[8];
	char src[8][32], t[64], reg[32];

	for (i = 0; i + 1 < L->n; i++) {
		if (sscanf(L->v[i], "\tsubq $%d,%%rsp", &N) != 1 || N < 8 || N > 64 || (N & 7))
			continue;
		m = 0;
		ok = 0;
		expect = -1;			/* the slot the next store has to be: highest first */
		for (j = i + 1; j < L->n; j++) {
			const char *s = L->v[j];
			char a[32];

			if (s[0] != '\t')
				break;			/* a label */
			if (strncmp(s, "\tcall", 5) == 0 && strstr(s, "%rsp") == NULL) {
				if (m > 0 && expect < 0)
					ok = 1;		/* all the slots are stored: the rewrite is complete */
				break;
			}
			if (strstr(s, "%rsp") == NULL && s[1] != 'j' && strncmp(s, "\tret", 4) != 0 &&
			    strncmp(s, "\tleave", 6) != 0 && strstr(s, "pushq") == NULL && strstr(s, "popq") == NULL)
				continue;
			if (strncmp(s, "\tmovl ", 6) == 0 || strncmp(s, "\tmovq ", 6) == 0) {
				int n1 = 0;

				off = 0;
				if (sscanf(s + 6, "%31[^,],%d(%%rsp)%n", a, &off, &n1) >= 2 && n1 && s[6 + n1] == '\0')
					;
				else if (n1 = 0, sscanf(s + 6, "%31[^,],(%%rsp)%n", a, &n1) >= 1 && n1 && s[6 + n1] == '\0')
					off = 0;
				else
					break;
				if (off < 0 || (off & 7) || off / 8 >= 8)
					break;
				if (expect < 0 && m == 0) {
					/* the first store: the highest slot, which must be the last of the N / 8 words or the one before */
					if (off / 8 != N / 8 - 1 && off / 8 != N / 8 - 2)
						break;
					expect = off / 8;
				}
				if (off / 8 != expect)
					break;
				if (a[0] == '$')
					strcpy(src[m], a);
				else if (a[0] == '%' && xp_to64(a, reg))
					strcpy(src[m], reg);
				else
					break;
				idx[m++] = j;
				expect--;
				continue;
			}
			break;				/* any other use of %rsp, a jump */
		}
		if (!ok || expect != -1 || (N / 8 != m && N / 8 != m + 1))
			continue;
		/* the stores become pushes, the subtraction the padding word (or nothing) */
		if (N / 8 == m + 1) {
			free(L->v[i]);
			L->v[i] = strdup("\tpushq %rax");
		} else {
			free(L->v[i]);
			L->v[i] = strdup("");
		}
		for (k = 0; k < m; k++) {
			snprintf(t, sizeof t, "\tpushq %s", src[k]);
			free(L->v[idx[k]]);
			L->v[idx[k]] = strdup(t);
		}
	}
}

static void
xp_emit(struct xp_lines *L, FILE *out)
{
	int i;

	for (i = 0; i < L->n; i++)
		fprintf(out, "%s\n", L->v[i]);
}

/* Read the assembly in buf, write it with the redundant tests removed. */
static void
x86_peep(char *buf, FILE *out)
{
	struct xp_lines L;
	char *p, *e;
	int cap = 1024;

	L.v = malloc(cap * sizeof(char *));
	L.n = 0;
	for (p = buf; *p; p = e + 1) {
		e = strchr(p, '\n');
		if (e == NULL)
			e = p + strlen(p);
		if (L.n == cap)
			L.v = realloc(L.v, (cap *= 2) * sizeof(char *));
		L.v[L.n] = malloc(e - p + 1);
		memcpy(L.v[L.n], p, e - p);
		L.v[L.n][e - p] = '\0';
		L.n++;
		if (*e == '\0')
			break;
	}
	xp_deltest(&L);
#if defined(mach_amd64)
	xp_small(&L);
	xp_copies(&L);
	xp_stackargs(&L);
	xp_frame64(&L);
#endif
	xp_emit(&L, out);
	while (L.n > 0)
		free(L.v[--L.n]);
	free(L.v);
}
