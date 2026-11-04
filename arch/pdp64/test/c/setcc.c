/* expect: 0 -- a comparison used as a value (cmp, set) in every condition, returned, stored, summed; signed and unsigned; against zero and a constant */
long lt(long a, long b) { return a < b; }
long le(long a, long b) { return a <= b; }
long gt(long a, long b) { return a > b; }
long ge(long a, long b) { return a >= b; }
long eq(long a, long b) { return a == b; }
long ne(long a, long b) { return a != b; }
long ult(unsigned long a, unsigned long b) { return a < b; }
long ule(unsigned long a, unsigned long b) { return a <= b; }
long ugt(unsigned long a, unsigned long b) { return a > b; }
long uge(unsigned long a, unsigned long b) { return a >= b; }
long lt0(long a) { return a < 0; }
long eq0(long a) { return a == 0; }
long gt5(long a) { return a > 5; }
long ne7(long a) { return a != 7; }
int ilt(int a, int b) { return a < b; }
long sgn(long a) { return (a > 0) - (a < 0); }
long cnt(long *a, long n, long k) { long c = 0, i; for (i = 0; i < n; i++) c += a[i] > k; return c; }
long both(long a, long b) { long x = a < b; long y = a > b; return x * 10 + y; }
long nest(long a, long b, long c) { return (a < b) + (b < c) * 2 + (a == c) * 4; }
void st(long *p, long a) { *p = a < 3; }
int test(void)
{
	long r = 0;
	static const long v[] = { -5, -1, 0, 1, 3, 5, 6, 7, 100 };
	long i, j, w[9];

	for (i = 0; i < 9; i++) {
		if (lt0(v[i]) != (v[i] < 0) || eq0(v[i]) != (v[i] == 0) || gt5(v[i]) != (v[i] > 5) || ne7(v[i]) != (v[i] != 7)) r |= 1;
		if (sgn(v[i]) != (v[i] > 0 ? 1 : v[i] < 0 ? -1 : 0)) r |= 2;
		for (j = 0; j < 9; j++) {
			long a = v[i], b = v[j];
			unsigned long ua = a, ub = b;

			if (lt(a, b) != (a < b) || le(a, b) != (a <= b) || gt(a, b) != (a > b) || ge(a, b) != (a >= b) || eq(a, b) != (a == b) || ne(a, b) != (a != b)) r |= 4;
			if (ult(ua, ub) != (ua < ub) || ule(ua, ub) != (ua <= ub) || ugt(ua, ub) != (ua > ub) || uge(ua, ub) != (ua >= ub)) r |= 8;
			if (ilt((int)a, (int)b) != ((int)a < (int)b)) r |= 16;
			if (both(a, b) != (a < b) * 10 + (a > b)) r |= 32;
			if (nest(a, b, v[(i + j) % 9]) != (a < b) + (b < v[(i + j) % 9]) * 2 + (a == v[(i + j) % 9]) * 4) r |= 64;
		}
		w[i] = 99;
		st(&w[i], v[i]);
		if (w[i] != (v[i] < 3)) r |= 128;
	}
	if (cnt(w, 9, 0) != 4 || cnt((long *)v, 9, 2) != 5) r |= 256;
	return r;
}
