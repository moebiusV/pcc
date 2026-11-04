/* expect: 0 -- values the allocator keeps in the frame are read once before a loop that does not store to them and has registers to spare */
long id(long x) { return x; }
long many(long a, long b, long c, long d, long e, long f, long g, long h, long n)
{
	long s = 0, i, t;
	long a1 = id(a), b1 = id(b), c1 = id(c), d1 = id(d), e1 = id(e), f1 = id(f), g1 = id(g), h1 = id(h);

	/* a call-free loop with many invariant values */
	for (i = 0; i < n; i++) {
		t = a1 * i + b1 - c1;
		t += d1 * (i & 3) + e1 - f1;
		t ^= g1 + h1;
		s += t;
	}
	return s + a1 + b1 + c1 + d1 + e1 + f1 + g1 + h1;
}
long stores(long a, long b, long c, long d, long e, long n)
{
	long a1 = id(a), b1 = id(b), c1 = id(c), d1 = id(d), e1 = id(e), i, s = 0;

	for (i = 0; i < n; i++) {		/* the loop changes some of them: those are not hoisted */
		s += a1 + b1 + c1 + d1 + e1;
		a1 += i;
		if (i & 1)
			d1 -= 1;
	}
	return s + a1 + d1;
}
long nested(long *p, long n, long m, long k1, long k2, long k3, long k4)
{
	long i, j, s = 0, q1 = id(k1), q2 = id(k2), q3 = id(k3), q4 = id(k4);

	for (i = 0; i < n; i++)
		for (j = 0; j < m; j++)
			s += p[j] * q1 + q2 - q3 + q4 + i;
	return s + q1 + q2 + q3 + q4;
}
int test(void)
{
	long r = 0, i, s;
	long p[6] = { 1, 2, 3, 4, 5, 6 };

	s = 0;
	for (i = 0; i < 7; i++) {
		long t = (3 * i + 2 - 3) + (7 * (i & 3) + 5 - 6);

		t ^= 7 + 8;
		s += t;
	}
	if (many(3, 2, 3, 7, 5, 6, 7, 8, 7) != s + 3 + 2 + 3 + 7 + 5 + 6 + 7 + 8) r |= 1;
	{
		long a = 1, b = 2, c = 3, d = 4, e = 5, ss = 0, ii;

		for (ii = 0; ii < 6; ii++) {
			ss += a + b + c + d + e;
			a += ii;
			if (ii & 1)
				d -= 1;
		}
		if (stores(1, 2, 3, 4, 5, 6) != ss + a + d) r |= 2;
	}
	{
		long sum = 0, ii, jj;

		for (ii = 0; ii < 3; ii++)
			for (jj = 0; jj < 4; jj++)
				sum += p[jj] * 2 + 3 - 4 + 5 + ii;
		if (nested(p, 3, 4, 2, 3, 4, 5) != sum + 2 + 3 + 4 + 5) r |= 4;
	}
	return r;
}
