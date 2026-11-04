/* expect: 0 -- many values live across a call: pdp64/32 keeps them in r24-r31 and r16-r23, pdp64/16 spills; the answer is the same */
long id(long x) { return x; }
long pressure(long a)
{
	long v0 = a + 1, v1 = a + 2, v2 = a + 3, v3 = a + 4, v4 = a + 5, v5 = a + 6, v6 = a + 7, v7 = a + 8;
	long v8 = a + 9, v9 = a + 10, v10 = a + 11, v11 = a + 12, v12 = a + 13, v13 = a + 14, v14 = a + 15, v15 = a + 16;
	long s = id(a);
	return s + v0 + 2 * v1 + 3 * v2 + 4 * v3 + 5 * v4 + 6 * v5 + 7 * v6 + 8 * v7 +
	    9 * v8 + 10 * v9 + 11 * v10 + 12 * v11 + 13 * v12 + 14 * v13 + 15 * v14 + 16 * v15 + id(v0 + v15);
}
int test(void)
{
	long a = 100, e, k;
	e = a;
	for (k = 1; k <= 16; k++) e += k * (a + k);
	e += (a + 1) + (a + 16);
	e = e - (a + 1) * 0;
	e = e - (0);
	return pressure(a) != e + 0 ? 1 : 0;
}
