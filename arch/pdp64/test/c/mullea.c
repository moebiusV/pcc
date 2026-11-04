/* expect: 0 -- multiplication by 3, 5, 9 with an added constant (lea with a scaled index), negative and large values */
long m3(long x) { return x * 3; }
long m5(long x) { return x * 5; }
long m9(long x) { return x * 9; }
long m3a(long x) { return x * 3 + 7; }
long m5a(long x) { return x * 5 - 100; }
long m9a(long x) { return 9 * x + 100000; }
unsigned im5(unsigned x) { return x * 5 + 1; }
long idx(long *a, long i) { return a[i * 5 + 2]; }
int test(void)
{
	long r = 0, i;
	static const long v[] = { 0, 1, -1, 2, -7, 1000, -1000, 3000000000L, -3000000000L, 4611686018427387903L };

	for (i = 0; i < 10; i++) {
		long x = v[i];

		if (m3(x) != x + x + x || m5(x) != x * 4 + x || m9(x) != x * 8 + x) r |= 1;
		if (m3a(x) != x * 3 + 7 || m5a(x) != x * 5 - 100 || m9a(x) != 9 * x + 100000) r |= 2;
		if (im5((unsigned)x) != (unsigned)x * 5u + 1u) r |= 4;
	}
	{
		long a[20];

		for (i = 0; i < 20; i++) a[i] = i * 100;
		if (idx(a, 3) != 1700 || idx(a, 0) != 200) r |= 8;
	}
	return r;
}
