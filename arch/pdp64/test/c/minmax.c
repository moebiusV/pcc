/* expect: 0 -- min, max, abs: signed and unsigned, equal operands, the most negative number */
long mx(long a, long b) { return a > b ? a : b; }
long mn(long a, long b) { return a < b ? a : b; }
unsigned long umx(unsigned long a, unsigned long b) { return a > b ? a : b; }
unsigned long umn(unsigned long a, unsigned long b) { return a < b ? a : b; }
long ab(long a) { return a < 0 ? -a : a; }
int iab(int a) { return a < 0 ? -a : a; }
long clamp(long x, long lo, long hi) { if (x < lo) x = lo; if (x > hi) x = hi; return x; }
long maxof(long *a, long n) { long m = a[0], i; for (i = 1; i < n; i++) if (a[i] > m) m = a[i]; return m; }
unsigned long umaxof(unsigned long *a, long n) { unsigned long m = a[0]; long i; for (i = 1; i < n; i++) if (a[i] > m) m = a[i]; return m; }
long minof(long *a, long n) { long m = a[0], i; for (i = 1; i < n; i++) if (a[i] < m) m = a[i]; return m; }
int test(void)
{
	long r = 0;
	static const long v[] = { -9223372036854775807L - 1, -100, -1, 0, 1, 7, 100, 9223372036854775807L };
	long i, j;

	for (i = 0; i < 8; i++) {
		if (ab(v[i]) != (v[i] < 0 ? -v[i] : v[i])) r |= 1;
		if (iab((int)v[i]) != ((int)v[i] < 0 ? -(int)v[i] : (int)v[i])) r |= 2;
		for (j = 0; j < 8; j++) {
			long a = v[i], b = v[j];
			unsigned long ua = a, ub = b;

			if (mx(a, b) != (a > b ? a : b) || mn(a, b) != (a < b ? a : b)) r |= 4;
			if (umx(ua, ub) != (ua > ub ? ua : ub) || umn(ua, ub) != (ua < ub ? ua : ub)) r |= 8;
			if (clamp(a, -50, b) != (a < -50 ? -50 : a > b ? b : a) && b >= -50) r |= 16;
		}
	}
	if (maxof((long *)v, 8) != 9223372036854775807L || minof((long *)v, 8) != -9223372036854775807L - 1) r |= 32;
	if (umaxof((unsigned long *)v, 8) != (unsigned long)-1L) r |= 64;
	return r;
}
