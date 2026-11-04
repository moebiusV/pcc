/* expect: 0 -- signed int division and remainder by constants that are not powers of two (a multiply, a shift and the sign), checked against the values by a loop over many dividends */
int d3(int x) { return x / 3; }
int d5(int x) { return x / 5; }
int d7(int x) { return x / 7; }
int d10(int x) { return x / 10; }
int d100(int x) { return x / 100; }
int d1000(int x) { return x / 1000; }
int d12345(int x) { return x / 12345; }
int dbig(int x) { return x / 2147483647; }
int d641(int x) { return x / 641; }
int m3(int x) { return x % 3; }
int m10(int x) { return x % 10; }
int m60(int x) { return x % 60; }
int m65521(int x) { return x % 65521; }
int mbig(int x) { return x % 1000000007; }
int dm10(int x) { return x / 10 + x % 10 * 100; }
int e1(int a, int b) { return (a * b + 7) / 10 + (a - b) % 12; }
int e2(int a) { return (a + 1) / 3 - (a * 5) % 7; }
int ref(int x, int d, int op)
{
	/* the same, by repeated subtraction on the absolute value, no division */
	long ax = x < 0 ? -(long)x : x, q = 0, r = ax;
	long dd = d;
	long hi = dd;
	int sh = 0;

	while (hi * 2 <= ax) { hi *= 2; sh++; }
	for (; sh >= 0; sh--, hi /= 2)
		if (r >= hi) { r -= hi; q += 1L << sh; }
	if (x < 0) { q = -q; r = -r; }
	return op ? (int)r : (int)q;
}
int test(void)
{
	long r = 0;
	static const int v[] = { 0, 1, -1, 2, 3, -3, 5, 9, 10, 11, 99, 100, 101, -99, -100, -101, 999, 1000, 1001, 12344, 12345, 12346, 65520, 65521, 65522,
	    1000000006, 1000000007, 1000000008, 2147483646, 2147483647, -2147483647, -2147483647 - 1, -2147483647 + 1, 641, 640, 642, 1234567890, -1234567890 };
	int i, x;
	unsigned seed = 12345;

	for (i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
		x = v[i];
		if (d3(x) != ref(x, 3, 0) || d5(x) != ref(x, 5, 0) || d7(x) != ref(x, 7, 0) || d10(x) != ref(x, 10, 0) || d100(x) != ref(x, 100, 0)) r |= 1;
		if (d1000(x) != ref(x, 1000, 0) || d12345(x) != ref(x, 12345, 0) || dbig(x) != ref(x, 2147483647, 0) || d641(x) != ref(x, 641, 0)) r |= 2;
		if (m3(x) != ref(x, 3, 1) || m10(x) != ref(x, 10, 1) || m60(x) != ref(x, 60, 1) || m65521(x) != ref(x, 65521, 1) || mbig(x) != ref(x, 1000000007, 1)) r |= 4;
		if (dm10(x) != ref(x, 10, 0) + ref(x, 10, 1) * 100) r |= 8;
	}
	for (i = 0; i < 3000; i++) {
		seed = seed * 1103515245u + 12345u;
		x = (int)(seed >> 1) - (int)(seed & 0x40000000u ? 0 : 1073741824);
		if (i & 1) x = -x;
		if (d10(x) != ref(x, 10, 0) || m10(x) != ref(x, 10, 1) || d7(x) != ref(x, 7, 0) || m60(x) != ref(x, 60, 1) || d12345(x) != ref(x, 12345, 0)) { r |= 16; break; }
	}
	{
		int a, b;

		for (a = -50; a < 50; a += 7)
			for (b = -30; b < 40; b += 5) {
				if (e1(a, b) != ref(a * b + 7, 10, 0) + ref(a - b, 12, 1)) r |= 32;
			}
		for (a = -1000; a < 1000; a += 13)
			if (e2(a) != ref(a + 1, 3, 0) - ref(a * 5, 7, 1)) r |= 64;
	}
	return r;
}
