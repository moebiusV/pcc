/* expect: 0 -- conditional constant propagation: constants through joins, loops, dead branches, unsigned and narrow types, a division by zero kept, a loop that runs once */
long zero;
long a1(long a) { long x = 3, y; if (x > 2) y = a + 1; else y = a - 1; return y * x; }
long a2(long n) { long s = 0, k = 4; long i; for (i = 0; i < n; i++) s += k * 2; return s; }
long a3(long a) { long m; if (a) m = 1; else m = 1; return m + 10 * m; }
long a4(long a) { long d = 0, z = 10; while (z > 0) { z--; d += 2; } return d + a; }
long a5(long a) { long v = 0; if (a > 5) v = 7; else v = 9; return v; }	/* two values: not a constant */
unsigned long a6(void) { unsigned long u = 0; u = u - 1; return u >> 60; }
int a7(void) { signed char c = 100; c = c + 100; return c; }	/* wraps to -56 */
int a8(void) { unsigned char c = 250; c = c + 10; return c; }	/* 4 */
long a9(long a) { long k = 0; if (k) return a / zero; return 5; }	/* the division is dead */
long a10(long a) { long i = 0; long s = 0; do { s += a; i++; } while (i < 1); return s; }
long a11(long a) { long x = 2; long y = x << 3; long z = y % 5; return a + z * (x - 2) + y; }
long a12(long a) { long x = 1; while (x) { if (a > 3) { x = 0; } else { a++; } } return a; }
long a13(void) { long p = 0, q = 0; int i; for (i = 0; i < 3; i++) { p = q; q = i; } return p * 10 + q; }
long a14(long a) { long x = -7; return x / 2 + x % 3 + (x >> 1) + ((unsigned long)x >> 62); }
long a15(char *s) { char *p = 0; if (p == s) return 1; return 2; }
int test(void)
{
	long r = 0;

	if (a1(5) != 18) r |= 1;
	if (a2(3) != 24 || a2(0) != 0) r |= 2;
	if (a3(0) != 11 || a3(1) != 11) r |= 4;
	if (a4(5) != 25) r |= 8;
	if (a5(9) != 7 || a5(1) != 9) r |= 16;
	if (a6() != 15) r |= 32;
	if (a7() != -56) r |= 64;
	if (a8() != 4) r |= 128;
	if (a9(1) != 5) r |= 256;
	if (a10(6) != 6) r |= 512;
	if (a11(2) != 2 + 16) r |= 1024;
	if (a12(0) != 4) r |= 2048;
	if (a13() != 12) r |= 4096;
	if (a14(0) != -3 + -1 + -4 + 3) r |= 8192;
	if (a15(0) != 1 || a15("x") != 2) r |= 16384;
	return r;
}
