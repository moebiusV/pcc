/* expect: 0 -- floating-point comparisons with NaN, infinity and -0.0, double and float, in every form (if, !, else, while, ?:, && and ||, value, switch): fcbCC, and no flags */
union du { double d; long l; };
union fu { float f; int i; };
static double D(long l) { union du u; u.l = l; return u.d; }
static float F(int i) { union fu u; u.i = i; return u.f; }

/* the classes of a pair: 0 a<b  1 a==b  2 a>b  3 unordered; the truth of each operator for each class */
static const char tlt[4] = { 1, 0, 0, 0 }, tle[4] = { 1, 1, 0, 0 }, tgt[4] = { 0, 0, 1, 0 }, tge[4] = { 0, 1, 1, 0 }, teq[4] = { 0, 1, 0, 0 }, tne[4] = { 1, 0, 1, 1 };

#define FORMS(T, S, OP, N) \
static int S##if_##N(T a, T b) { if (a OP b) return 1; return 0; } \
static int S##val_##N(T a, T b) { return a OP b; } \
static int S##not_##N(T a, T b) { return !(a OP b); } \
static int S##tern_##N(T a, T b) { return (a OP b) ? 10 : 20; } \
static int S##nif_##N(T a, T b) { if (!(a OP b)) return 1; else return 0; } \
static int S##while_##N(T a, T b) { int n = 0; while (a OP b && n < 3) n++; return n; } \
static int S##do_##N(T a, T b) { int n = 0; do n++; while (!(a OP b) && n < 3); return n; } \
static int S##and_##N(T a, T b) { return (a OP b) && (b OP a); } \
static int S##or_##N(T a, T b) { return (a OP b) || (b OP a); } \
static int S##andn_##N(T a, T b) { return (a OP b) && !(b OP a); } \
static int S##orn_##N(T a, T b) { return !(a OP b) || (b OP a); } \
static int S##sw_##N(T a, T b) { switch (a OP b) { case 0: return 5; case 1: return 7; } return 9; } \
static int S##el_##N(T a, T b) { if (a OP b) return 1; else if (b OP a) return 2; return 3; } \
static int S##z_##N(T a, T b) { T z = a - a; if (z OP b) return 1; return 0; }

#define ALLOPS(T, S) FORMS(T, S, <, lt) FORMS(T, S, <=, le) FORMS(T, S, >, gt) FORMS(T, S, >=, ge) FORMS(T, S, ==, eq) FORMS(T, S, !=, ne)
ALLOPS(double, d)
ALLOPS(float, f)

static int fails;
static const char rcls[4] = { 2, 1, 0, 3 };	/* the class of the pair the other way round */

/* one operator, one pair of class c: every form against the truth x of the pair, and y of the pair the other way round */
#define CHK(T, S, N, t) \
static void chk_##S##_##N(T a, T b, int c) { \
	int x = t[c], y = t[(int)rcls[c]]; \
	if (S##if_##N(a, b) != x) fails++; \
	if (S##val_##N(a, b) != x) fails++; \
	if (S##not_##N(a, b) != !x) fails++; \
	if (S##tern_##N(a, b) != (x ? 10 : 20)) fails++; \
	if (S##nif_##N(a, b) != !x) fails++; \
	if (S##while_##N(a, b) != (x ? 3 : 0)) fails++; \
	if (S##do_##N(a, b) != (x ? 1 : 3)) fails++; \
	if (S##and_##N(a, b) != (x && y)) fails++; \
	if (S##or_##N(a, b) != (x || y)) fails++; \
	if (S##andn_##N(a, b) != (x && !y)) fails++; \
	if (S##orn_##N(a, b) != (!x || y)) fails++; \
	if (S##sw_##N(a, b) != (x ? 7 : 5)) fails++; \
	if (S##el_##N(a, b) != (x ? 1 : y ? 2 : 3)) fails++; \
}
#define CHKALL(T, S) CHK(T, S, lt, tlt) CHK(T, S, le, tle) CHK(T, S, gt, tgt) CHK(T, S, ge, tge) CHK(T, S, eq, teq) CHK(T, S, ne, tne) \
static void pair_##S(T a, T b, int c) { \
	chk_##S##_lt(a, b, c); chk_##S##_le(a, b, c); chk_##S##_gt(a, b, c); chk_##S##_ge(a, b, c); chk_##S##_eq(a, b, c); chk_##S##_ne(a, b, c); \
}
CHKALL(double, d)
CHKALL(float, f)
#define PAIR(S, a, b, c) pair_##S(a, b, c)

int test(void)
{
	double dnan = D(0x7ff8000000000000L), dnnan = D(0xfff8000000000000L), dinf = D(0x7ff0000000000000L), dninf = D(0xfff0000000000000L);
	double dzero = D(0L), dnzero = D(0x8000000000000000L), dtiny = D(1L);
	float fnan = F(0x7fc00000), fnnan = F(0xffc00000), finf = F(0x7f800000), fninf = F(0xff800000);
	float fzero = F(0), fnzero = F(0x80000000), ftiny = F(1);
	int c;

	c = 0; PAIR(d, 1.0, 2.0, 0); PAIR(d, dninf, dinf, 0); PAIR(d, dninf, -1.0, 0); PAIR(d, dnzero, dtiny, 0); PAIR(d, -1.0, dzero, 0);
	c = 1; PAIR(d, 2.0, 2.0, 1); PAIR(d, dnzero, dzero, 1); PAIR(d, dzero, dnzero, 1); PAIR(d, dinf, dinf, 1); PAIR(d, dninf, dninf, 1);
	c = 2; PAIR(d, 3.0, 2.0, 2); PAIR(d, dinf, dninf, 2); PAIR(d, dinf, 1e300, 2); PAIR(d, dtiny, dnzero, 2); PAIR(d, dzero, -1.0, 2);
	c = 3; PAIR(d, dnan, 2.0, 3); PAIR(d, 2.0, dnan, 3); PAIR(d, dnan, dnan, 3); PAIR(d, dnnan, dinf, 3); PAIR(d, dninf, dnnan, 3); PAIR(d, dnan, dnzero, 3); PAIR(d, dzero, dnan, 3);
	c = 0; PAIR(f, 1.0f, 2.0f, 0); PAIR(f, fninf, finf, 0); PAIR(f, fninf, -1.0f, 0); PAIR(f, fnzero, ftiny, 0); PAIR(f, -1.0f, fzero, 0);
	c = 1; PAIR(f, 2.0f, 2.0f, 1); PAIR(f, fnzero, fzero, 1); PAIR(f, fzero, fnzero, 1); PAIR(f, finf, finf, 1); PAIR(f, fninf, fninf, 1);
	c = 2; PAIR(f, 3.0f, 2.0f, 2); PAIR(f, finf, fninf, 2); PAIR(f, finf, 1e30f, 2); PAIR(f, ftiny, fnzero, 2); PAIR(f, fzero, -1.0f, 2);
	c = 3; PAIR(f, fnan, 2.0f, 3); PAIR(f, 2.0f, fnan, 3); PAIR(f, fnan, fnan, 3); PAIR(f, fnnan, finf, 3); PAIR(f, fninf, fnnan, 3); PAIR(f, fnan, fnzero, 3); PAIR(f, fzero, fnan, 3);
	return fails;
}
