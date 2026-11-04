/* expect: 0 -- the compiler's reuse of a value it has loaded or stored: not across a store, a call, a label or a volatile access */
int g1 = 5;
long g2 = 7;
volatile int vol;
static int calls;
int bump(void) { calls++; g1 += 100; return g1; }
int twice(int *p) { int a = *p; int b = *p; return a + b; }
int between(int *p, int *q) { int a = *p; *q = 9; int b = *p; return a * 1000 + b; }
int acall(void) { int a = g1; bump(); int b = g1; return b - a; }
int vtwice(void) { int a = vol; int b = vol; return a + b; }
long narrow(void) { char c; short s; int i; long l; c = -3; s = -300; i = -70000; l = -5000000000L;
	return (long)c + s + i + l + (unsigned char)c + (unsigned short)s; }
long fwd(long x) { long t[4]; t[1] = x; t[2] = x + 1; return t[1] + t[2]; }
int cloop(int n) { int s = 0, i; for (i = 0; i < n; i++) s += g1; return s; }
int test(void)
{
	int x = 4, y = 4;
	long r = 0;
	if (twice(&x) != 8) r |= 1;
	if (between(&x, &x) != 4 * 1000 + 9 && between(&x, &y) != 4009) r |= 2;
	g1 = 5;
	if (acall() != 100) r |= 4;
	vol = 6;
	if (vtwice() != 12) r |= 8;
	if (narrow() != (-3L + -300 + -70000 + -5000000000L + 253 + 65236)) r |= 16;
	if (fwd(10) != 21) r |= 32;
	g1 = 3;
	if (cloop(5) != 15) r |= 64;
	return (int)r;
}
