/* expect: 0 -- scaled index addressing with an unsigned long (or long long) index and a constant displacement: x[u+1], x[u-1], 2-D arrays, stores, byte, short, int, long and struct elements */
struct s { int a; long b; };
unsigned f1(unsigned *x, unsigned long u) { return x[u + 1]; }
unsigned f2(unsigned *x, unsigned long u) { return x[u - 1]; }
unsigned long f3(unsigned long *x, unsigned long u) { return x[u + 3]; }
unsigned short f4(unsigned short *x, unsigned long long u) { return x[u + 2]; }
unsigned char f5(unsigned char *x, unsigned long u) { return x[u + 5]; }
long f6(struct s *x, unsigned long u) { return x[u + 1].b + x[u].a; }
void st(unsigned *x, unsigned long u, unsigned v) { x[u + 2] = v; x[u] = v + 1; }
unsigned f7(unsigned (*m)[4], unsigned long i, unsigned long j) { return m[i + 1][j + 1]; }
int test(void)
{
	long r = 0;
	unsigned a[16];
	unsigned long l[16];
	unsigned short h[16];
	unsigned char c[16];
	struct s ss[6];
	unsigned long i;

	for (i = 0; i < 16; i++) { a[i] = i * 3 + 1; l[i] = i * 5 + 2; h[i] = i * 7 + 3; c[i] = i * 11 + 4; }
	for (i = 0; i < 6; i++) { ss[i].a = i; ss[i].b = 100 + i; }
	if (f1(a, 4) != a[5] || f2(a, 4) != a[3]) r |= 1;
	if (f3(l, 5) != l[8]) r |= 2;
	if (f4(h, 6) != h[8]) r |= 4;
	if (f5(c, 3) != c[8]) r |= 8;
	if (f6(ss, 2) != 103 + 2) r |= 16;
	st(a, 7, 40);
	if (a[9] != 40 || a[7] != 41) r |= 32;
	if (f7((unsigned (*)[4])a, 1, 1) != a[2 * 4 + 2]) r |= 64;
	return r;
}
