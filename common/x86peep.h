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
	xp_frame64(&L);
#endif
	xp_emit(&L, out);
	while (L.n > 0)
		free(L.v[--L.n]);
	free(L.v);
}
