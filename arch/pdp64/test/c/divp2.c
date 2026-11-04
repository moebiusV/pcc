/* expect: 0 -- signed division and remainder by a power of two (shifts that round toward zero), every width, negative and extreme values */
int d2(int x) { return x / 2; }
int d8(int x) { return x / 8; }
int m8(int x) { return x % 8; }
int m2(int x) { return x % 2; }
long ld4(long x) { return x / 4; }
long lm16(long x) { return x % 16; }
long ld1k(long x) { return x / 1024; }
long lm1k(long x) { return x % 1024; }
long ld1g(long x) { return x / 1073741824L; }
short sd8(short x) { return x / 8; }
signed char cm4(signed char x) { return x % 4; }
long long lld32(long long x) { return x / 32; }
int dnoreg(int *p) { return *p / 4; }
int fsum(int n) { int s = 0, i; for (i = -n; i <= n; i++) s += i / 4 + i % 4 * 10; return s; }
long pdi(int *a, int *b) { return a - b; }
long pdl(long *a, long *b) { return a - b; }
long pdc(char *a, char *b) { return a - b; }
long pds(short *a, short *b) { return b - a; }
int e1(int a, int b) { return (a * b + 7) / 8 + (a - b) % 4; }
long e2(long a) { return (a * 3 - 5) / 16; }
int test(void)
{
	long r = 0;
	static const long vals[] = { 0, 1, -1, 2, -2, 3, -3, 7, -7, 8, -8, 9, -9, 15, -15, 16, -16, 17, -17, 1023, -1023, 1024, -1024, 1025, -1025,
	    2147483647L, -2147483647L - 1, 4611686018427387903L, -4611686018427387904L, 9223372036854775807L, -9223372036854775807L - 1 };
	int i;

	for (i = 0; i < (int)(sizeof vals / sizeof vals[0]); i++) {
		long v = vals[i];
		int w = (int)v;

		if (ld4(v) != v / 4 || lm16(v) != v % 16 || ld1k(v) != v / 1024 || lm1k(v) != v % 1024 || ld1g(v) != v / 1073741824L) r |= 1;
		if (d2(w) != w / 2 || d8(w) != w / 8 || m8(w) != w % 8 || m2(w) != w % 2) r |= 2;
		if (sd8((short)v) != (short)v / 8 || cm4((signed char)v) != (signed char)v % 4) r |= 4;
		if (lld32(v) != v / 32) r |= 8;
		{
			int pv = w;

			if (dnoreg(&pv) != w / 4) r |= 16;
		}
	}
	if (fsum(10) != -10 / 4 * 0 + (fsum(10))) r |= 32;
	{
		int s = 0, k;

		for (k = -10; k <= 10; k++) s += k / 4 + k % 4 * 10;
		if (fsum(10) != s || s != -(-10 / 4 + 0) * 0 + s) r |= 64;
	}
	{
		int ia[10];
		long la[10];
		char ca[10];
		short sa[10];

		if (pdi(&ia[9], &ia[2]) != 7 || pdi(&ia[1], &ia[8]) != -7) r |= 128;
		if (pdl(&la[9], &la[2]) != 7 || pdl(&la[1], &la[8]) != -7) r |= 256;
		if (pdc(&ca[9], &ca[2]) != 7 || pdc(&ca[1], &ca[8]) != -7) r |= 512;
		if (pds(&sa[9], &sa[2]) != -7 || pds(&sa[1], &sa[8]) != 7) r |= 1024;
	}
	{
		int a, b;

		for (a = -50; a < 50; a += 7)
			for (b = -30; b < 40; b += 5)
				if (e1(a, b) != (a * b + 7) / 8 + (a - b) % 4) r |= 2048;
		for (a = -1000; a < 1000; a += 13)
			if (e2(a) != (a * 3 - 5) / 16) r |= 4096;
	}
	return r;
}
