/* expect: 0 flags: -xchecks -- the checks that can be proved are dropped (a loop variable with a constant bound, a mask, an unsigned char); the others stay and still trap */
static int nfail;
void __chk_fail(int kind, const char *file, int line, long value, unsigned long limit)
{
	nfail++;
}
static long a[10];
static long t[256];
static long loop1(void) { long s = 0; int i; for (i = 0; i < 10; i++) s += a[i]; return s; }
static long loop2(void) { long s = 0; for (int i = 2; i <= 9; i += 2) s += a[i]; return s; }
static long over(void) { long s = 0; int i; for (i = 0; i < 11; i++) s += a[i]; return s; }
static long moves(void) { long s = 0; int i; for (i = 0; i < 5; i++) { i += 3; s += a[i + 5]; } return s; }
static long mask(unsigned x) { return a[x % 10] + t[x & 255]; }
static long uch(unsigned char c) { return t[c]; }
static long var1(void) { long s = 0; int n = 5, i; for (i = 0; i < n; i++) s += a[i]; return s; }
static long var2(void) { long s = 0; int n = 11, i; for (i = 0; i < n; i++) s += a[i]; return s; }
static long var3(int c) { long s = 0; int n = 5, i; if (c) n = 12; for (i = 0; i < n; i++) s += a[i]; return s; }
static long var4(unsigned char n) { long s = 0; int i; for (i = 0; i < n; i++) s += t[i]; return s; }
static long gone(void) { long s = 0; int i; for (i = 0; i < 10; i++) { s += a[i]; goto in; } return s; in: return a[i + 3]; }
int test(void)
{
	long r = 0;
	int i;

	for (i = 0; i < 10; i++) a[i] = i + 1;
	if (loop1() != 55 || nfail) r |= 1;
	if (loop2() != 24 || nfail != 0) r |= 2;
	if (mask(123) != a[3] + t[123] || uch(200) != t[200] || nfail) r |= 4;
	over();
	if (nfail != 1) r |= 8;				/* the eleventh element: one failure */
	moves();
	if (nfail != 2) r |= 16;			/* i changed inside the loop: the check stays */
	var1(); var4(255);
	if (nfail != 2) r |= 32;			/* a variable bound of a single constant write, a narrow type: provable */
	var2();
	if (nfail != 3) r |= 64;			/* the same with a bound too large: traps */
	var3(1);
	if (nfail != 5) r |= 128;			/* two writes of n: not provable, traps twice (n = 12) */
	return r;
}
