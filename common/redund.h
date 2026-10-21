/*
 * Assembly text, after the code generator and before c2 (-xdce): an instruction that computes into a
 * register what the instruction before the last compare-and-branch already computed into it is dropped:
 *
 *	bicl3 $255,r6,r0 ; cmpl r0,$A ; jeql L ; bicl3 $255,r6,r0 ; cmpl r0,$B
 *
 * (a chain of  (x & ~0xff) == A || (x & ~0xff) == B ...).  The register is still intact on the path that
 * did not branch, because the only things between are compares and conditional branches, and the next
 * instruction is a compare, so the condition codes the dropped one would have set are not wanted.
 * VAX syntax: the destination is the last operand and is not also read.
 */
#ifndef REDUND_H
#define REDUND_H

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* mnemonic of the line at p (after a tab), in m; returns the operand text or NULL */
static const char *
rd_op(const char *p, char *m, size_t msz)
{
	size_t n = 0;

	while (*p == '\t' || *p == ' ')
		p++;
	while (*p && *p != '\t' && *p != ' ' && *p != '\n' && n + 1 < msz)
		m[n++] = *p++;
	m[n] = '\0';
	while (*p == '\t' || *p == ' ')
		p++;
	return p;
}

static int
rd_isalu(const char *m)
{
	static const char *const ops[] = { "bicl3", "bisl3", "xorl3", "addl3", "subl3", "mull3", "ashl",
	    "movzbl", "movzwl", "cvtbl", "cvtwl", "mcoml", "mnegl", "extzv", "movl", "movab", "moval", NULL };
	int i;

	for (i = 0; ops[i]; i++)
		if (strcmp(m, ops[i]) == 0)
			return 1;
	return 0;
}

static int
rd_iscmp(const char *m)
{
	return strcmp(m, "cmpl") == 0 || strcmp(m, "cmpb") == 0 || strcmp(m, "cmpw") == 0 ||
	    strcmp(m, "tstl") == 0 || strcmp(m, "tstb") == 0 || strcmp(m, "tstw") == 0 ||
	    strcmp(m, "bitl") == 0 || strcmp(m, "bitb") == 0 || strcmp(m, "bitw") == 0;
}

/* the register that the operand list ends in, "r6": returns its length, 0 if the last operand is not one */
static int
rd_dest(const char *ops, const char **start)
{
	const char *e = ops + strcspn(ops, "\n"), *s = e;

	while (s > ops && s[-1] != ',')
		s--;
	if (s[0] == 'r' && isdigit((unsigned char)s[1]) && (e - s == 2 || (e - s == 3 && isdigit((unsigned char)s[2])))) {
		*start = s;
		return (int)(e - s);
	}
	return 0;
}

/* does the register name occur as an operand in the text (as r1 in "r10" is not r1) */
static int
rd_mentions(const char *t, size_t tn, const char *r, int rn)
{
	size_t i;

	for (i = 0; i + rn <= tn; i++)
		if (memcmp(t + i, r, rn) == 0 && (i == 0 || !isalnum((unsigned char)t[i - 1])) &&
		    (i + rn == tn || !isalnum((unsigned char)t[i + rn])))
			return 1;
	return 0;
}

/* drop the repeats in buf (a malloced string, edited in place) */
static void
dropredundant(char *buf)
{
	char *line, *next, *w = buf, *keep = NULL;
	size_t keepn = 0;
	char m[16], m2[16];
	int state = 0;		/* 0: nothing; 1: the instruction is kept; 2: a compare since; 3: a branch since */
	const char *ops;

	for (line = buf; *line; line = next) {
		size_t n;
		char *nl = strchr(line, '\n');

		next = nl ? nl + 1 : line + strlen(line);
		n = (size_t)(next - line);
		if (line[0] != '\t') {			/* a label or a directive */
			state = 0;
			memmove(w, line, n);
			w += n;
			continue;
		}
		ops = rd_op(line, m, sizeof m);
		if (state >= 1 && rd_isalu(m) && keep != NULL && n == keepn + 0 && memcmp(line, keep, n) == 0 &&
		    state == 3) {
			/* the same instruction again, and the next one is a compare: its condition codes are not wanted */
			const char *nops = rd_op(next, m2, sizeof m2);

			(void)nops;
			if (next[0] == '\t' && rd_iscmp(m2))
				continue;			/* dropped */
		}
		memmove(w, line, n);
		if (rd_isalu(m)) {
			const char *d;
			int dn = rd_dest(ops, &d);

			if (dn && !rd_mentions(ops, (size_t)(d - ops), d, dn) && strstr(ops, "+") == NULL &&
			    strstr(ops, "-(") == NULL) {
				keep = w;
				keepn = n;
				state = 1;
			} else
				state = 0;
		} else if (state >= 1 && rd_iscmp(m))
			state = state == 1 ? 2 : state;
		else if (state == 2 && m[0] == 'j' && strcmp(m, "jbr") != 0 && strcmp(m, "jmp") != 0 &&
		    strcmp(m, "jsb") != 0)
			state = 3;
		else if (state == 3 && rd_iscmp(m))
			state = 3;
		else if (state == 3 && m[0] == 'j' && strcmp(m, "jbr") != 0 && strcmp(m, "jmp") != 0 &&
		    strcmp(m, "jsb") != 0)
			state = 3;
		else
			state = 0;
		w += n;
	}
	*w = '\0';
}

#endif
