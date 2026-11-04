/* expect: 0 -- calls whose arguments come from other registers (movcall), moves that pair up (mov2), and epilogues (popret, popmret) */
long id3(long a, long b, long c) { return a * 100 + b * 10 + c; }
long swap2(long a, long b) { return id3(b, a, a - b); }
long keep(long a, long b, long c) { long x = id3(a, b, c); long y = id3(c, a, b); return x - y; }
long many(long a, long b, long c, long d, long e, long f, long g, long h)
{ return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f + 7 * g + 8 * h; }
long chain(long a, long b, long c) { return many(c, b, a, b, c, a, b, c); }
long leafone(long a) { return a + 1; }
long deep(long n) { if (n == 0) return 0; return n + deep(n - 1); }
long tail(long a, long b) { return id3(b, b, a); }
long regs(long a, long b, long c, long d, long e)
{ long p = id3(a, b, c), q = id3(d, e, a), r = id3(b, d, e); return p + q + r + a + b + c + d + e; }
int test(void)
{
	long r = 0;
	if (swap2(1, 2) != 2 * 100 + 1 * 10 - 1) r |= 1;
	if (keep(1, 2, 3) != 123 - 312) r |= 2;
	if (chain(1, 2, 3) != 3 + 4 + 3 + 8 + 15 + 6 + 14 + 24) r |= 4;
	if (leafone(41) != 42) r |= 8;
	if (deep(10) != 55) r |= 16;
	if (tail(5, 6) != 666 - 0 + 5 - 6 && tail(5, 6) != 665) r |= 32;
	if (regs(1, 2, 3, 4, 5) != (123 + 451 + 245) + 15) r |= 64;
	return (int)r;
}
