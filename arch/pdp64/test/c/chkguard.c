/* expect: 0 flags: -xchecks -- a guard (an if, a while, an early exit) proves a subscript; a guard that does not hold at the use proves nothing and the check traps */
static int nfail;
void __chk_fail(int kind, const char *file, int line, long value, unsigned long limit)
{
	nfail++;
}
static long a[10];
static long g1(unsigned n) { if (n >= 10) return -1; return a[n]; }
static long g2(int n) { if (n >= 10) return -1; return a[n]; }
static long g3(int n) { if (n >= 0 && n < 10) return a[n]; return -1; }
static long g4(int n) { if (n < 0 || n >= 10) return -1; return a[n]; }
static long g5(int n) { if ((unsigned)n >= 10) return -1; return a[n]; }
static long g6(int n) { if (n >= 0 && n < 10) { n++; return a[n]; } return -1; }
static long g7(int n, int c) { if (c) { if (n >= 10) return -1; } return a[n]; }
static long g9(unsigned n, int s) { switch (s) { case 1: if (n >= 10) return -1; case 2: return a[n]; } return -1; }
static long x1(unsigned n) { return n < 10 && a[n]; }
static long x2(unsigned n) { return n >= 10 || a[n]; }
static long x3(unsigned n) { return n < 10 ? a[n] : -1; }
static long x4(unsigned n) { return n < 10 ? -1 : a[n]; }
static long x5(unsigned n) { return a[n] && n < 10; }
static long y1(unsigned n, int c) { if (n < 10) { c++; } else { return -1; } return a[n]; }
static long y2(unsigned n) { if (n >= 10) { return -1; } else { n = 50; } return a[n]; }
static long y3(unsigned n) { while (n >= 10) n -= 7; return a[n]; }
static long y4(unsigned n) { do { n -= 1; } while (n >= 10); return a[n]; }
static long y5(unsigned n, int c) { while (n >= 10) { if (c) break; n -= 7; } return a[n]; }
static long y6(unsigned i, unsigned m) { if (m <= 10 && i < m) return a[i]; return -1; }
static long y7(unsigned m) { long s = 0; unsigned i; if (m > 10) return 0; for (i = 0; i < m; i++) s += a[i]; return s; }
static long y8(unsigned m) { long s = 0; unsigned i; if (m > 11) return 0; for (i = 0; i < m; i++) s += a[i]; return s; }
int test(void)
{
	long r = 0;
	int i;

	for (i = 0; i < 10; i++) a[i] = i + 1;
	if (g1(3) != 4 || g1(10) != -1 || g3(9) != 10 || g3(10) != -1 || g4(-1) != -1 || g4(0) != 1 || g5(-3) != -1 || g5(9) != 10)
		r |= 1;
	if (nfail) r |= 2;			/* the guarded ones never fail */
	g2(-1);
	if (nfail != 1) r |= 4;			/* a guard of the upper bound only: a negative subscript traps */
	g6(9);
	if (nfail != 2) r |= 8;			/* n changed after the guard */
	g7(10, 0);
	if (nfail != 3) r |= 16;		/* the guard was in another branch */
	g9(12, 2);
	if (nfail != 4) r |= 32;		/* case 2 is entered past the guard */
	if (x1(3) != 1 || x1(30) != 0 || x2(30) != 1 || x2(3) != 1 || x3(2) != 3 || x3(20) != -1 || nfail != 4) r |= 64;	/* guards inside an expression: silent */
	x4(20);
	if (nfail != 5) r |= 128;		/* the wrong arm traps */
	x5(20);
	if (nfail != 6) r |= 256;		/* the subscript before the test */
	if (y1(3, 0) != 4 || y3(35) != 8 || y4(15) != 10 || y4(2) != 2 || y6(3, 8) != 4 || y6(3, 2) != -1 || y7(10) != 55 || nfail != 6)
		r |= 512;		/* the provable ones are silent and right */
	y2(3);
	if (nfail != 7) r |= 1024;		/* n was written in the else: the check stays */
	y5(15, 1);
	if (nfail != 8) r |= 2048;		/* a loop left by a break proves nothing */
	y8(11);
	if (nfail != 9) r |= 4096;		/* m <= 11 against an array of 10 */
	return r;
}
