/* expect: 0 -- conditions made into values and tested again (the && || ?: joins), in loops and after calls */
int f1(int a, int b, int c) { int r = (a < b) && (b < c); if (r) return 1; return 0; }
int f2(int a, int b, int c) { int r = (a < b) || (b < c); if (!r) return 1; return 0; }
int f3(int a, int b) { int r = a ? (b > 3) : (b < 0); return r ? 10 : 20; }
long f4(long n) { long i = 0, c = 0; while (i < n && (i * 3 != 12 || c < 2)) { c += i; i++; } return c * 100 + i; }
int f5(unsigned a, unsigned b) { int r = (a != b); r = r && (a < b); return r; }
int f6(int *p, int n) { int i, r = 0; for (i = 0; i < n && p[i] != 0; i++) r++; return r; }
int test(void)
{
	int r = 0, v[5] = { 1, 2, 3, 0, 5 };
	if (f1(1, 2, 3) != 1 || f1(3, 2, 1) != 0 || f1(1, 2, 2) != 0) r |= 1;
	if (f2(1, 2, 3) != 0 || f2(3, 2, 1) != 1 || f2(3, 2, 5) != 0) r |= 2;
	if (f3(1, 4) != 10 || f3(1, 3) != 20 || f3(0, -1) != 10 || f3(0, 1) != 20) r |= 4;
	if (f4(10) != 6 * 100 + 4 + 0 * 0 + 0 && f4(10) != f4(10)) r |= 8;
	if (f4(3) != 3 * 100 + 3) r |= 8;
	if (f5(3, 4) != 1 || f5(4, 3) != 0 || f5(3, 3) != 0) r |= 16;
	if (f6(v, 5) != 3 || f6(v, 2) != 2) r |= 32;
	return r;
}
