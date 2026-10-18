/*
 * x86peep.h -- text passes over the assembly of a whole compilation unit,
 * shared by the i386 and amd64 backends (-xdmr).  The file is included, not
 * linked: everything is static.
 *
 * Loop rotation, as c2 of dmr-cc and of 32V does it:
 *
 *	L:	test...			jmp	L
 *		jcc	X		B:	body
 *		body...		=>	L:	test...
 *		jmp	L			jncc	B
 *	X:				X:
 *
 * One branch per iteration instead of two, and the same number of
 * instructions.  A "continue" that jumps to L still finds the test there.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct xp_lines {
	char **v;
	int n;
};

static int
xp_islabel(const char *s)
{
	return s[0] == '.' && s[1] == 'L' && strchr(s, ':') != NULL &&
	    s[strlen(s) - 1] == ':';
}

/* the inverse of conditional jump j, or NULL if it has none we trust */
static const char *
xp_invert(const char *j)
{
	static const char *tab[][2] = {
		{ "je", "jne" }, { "jz", "jnz" }, { "jl", "jge" },
		{ "jle", "jg" }, { "jb", "jae" }, { "jbe", "ja" },
		{ "js", "jns" }, { "jc", "jnc" },
	};
	unsigned i;

	for (i = 0; i < sizeof(tab) / sizeof(tab[0]); i++) {
		if (strcmp(j, tab[i][0]) == 0)
			return tab[i][1];
		if (strcmp(j, tab[i][1]) == 0)
			return tab[i][0];
	}
	return NULL;
}

/* a tab-indented conditional jump "jcc target": mnemonic and target */
static int
xp_condjump(const char *s, char *mn, char *tgt)
{
	if (s[0] != '\t' || s[1] != 'j' || strncmp(s + 1, "jmp", 3) == 0)
		return 0;
	return sscanf(s + 1, "%15s %63s", mn, tgt) == 2;
}

/* an instruction we may move: no label, jump, call, return, directive */
static int
xp_movable(const char *s)
{
	if (s[0] != '\t' || s[1] == '.' || s[1] == 'j' || s[1] == '\0')
		return 0;
	if (strncmp(s + 1, "call", 4) == 0 || strncmp(s + 1, "ret", 3) == 0 ||
	    strncmp(s + 1, "leave", 5) == 0 || strncmp(s + 1, "f", 1) == 0 ||
	    strncmp(s + 1, "sahf", 4) == 0 || strncmp(s + 1, "ucom", 4) == 0 ||
	    strncmp(s + 1, "comi", 4) == 0 || strncmp(s + 1, "ucomi", 5) == 0)
		return 0;
	return 1;
}

/*
 * Is there a loop head at line i?  Returns the line of the conditional jump
 * in *jp and of the jmp back in *kp.
 */
static int
xp_loop(struct xp_lines *L, int i, int *jp, int *kp)
{
	char mn[16], tgt[64], mn2[16], tgt2[64], lab[64];
	int j, k, t;

	if (!xp_islabel(L->v[i]) || strlen(L->v[i]) >= sizeof(lab))
		return 0;
	strcpy(lab, L->v[i]);
	lab[strlen(lab) - 1] = '\0';
	/* the test: one to four instructions and a conditional jump */
	for (j = i + 1, t = 0; j < L->n && t < 5; j++, t++) {
		if (xp_condjump(L->v[j], mn, tgt))
			break;
		if (!xp_movable(L->v[j]))
			return 0;
	}
	if (j >= L->n || t == 0 || t >= 5 || xp_invert(mn) == NULL)
		return 0;
	/* the body ends in "jmp lab" followed by the label the test exits to */
	for (k = j + 1; k < L->n - 1 && k - j < 4000; k++) {
		if (L->v[k][0] == '\t' && strncmp(L->v[k] + 1, "jmp", 3) == 0 &&
		    sscanf(L->v[k] + 1, "%15s %63s", mn2, tgt2) == 2 &&
		    strcmp(tgt2, lab) == 0) {
			char want[70];

			snprintf(want, sizeof(want), "%s:", tgt);
			if (strcmp(L->v[k + 1], want) == 0) {
				*jp = j;
				*kp = k;
				return 1;
			}
		}
	}
	return 0;
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

static int xp_serial;

static void
xp_emit(struct xp_lines *L, int lo, int hi, FILE *out)
{
	int i, j, k;
	char mn[16], tgt[64];

	for (i = lo; i < hi; i++) {
		if (xp_loop(L, i, &j, &k) && k < hi) {
			int b = xp_serial++, t;

			xp_condjump(L->v[j], mn, tgt);
			fprintf(out, "\tjmp %.*s\n", (int)strlen(L->v[i]) - 1,
			    L->v[i]);
			fprintf(out, ".Lxp%d:\n", b);
			xp_emit(L, j + 1, k, out);	/* the body */
			fprintf(out, "%s\n", L->v[i]);	/* the test */
			for (t = i + 1; t < j; t++)
				fprintf(out, "%s\n", L->v[t]);
			fprintf(out, "\t%s .Lxp%d\n", xp_invert(mn), b);
			i = k;	/* the exit label follows */
			continue;
		}
		fprintf(out, "%s\n", L->v[i]);
	}
}

/* Read the assembly in buf, write it with the loops rotated. */
static void
x86_looprot(char *buf, FILE *out)
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
	xp_emit(&L, 0, L.n, out);
	while (L.n > 0)
		free(L.v[--L.n]);
	free(L.v);
}
