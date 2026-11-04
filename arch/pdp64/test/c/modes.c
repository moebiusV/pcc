/* expect: 0 -- read-modify-write on memory (inc, dec, neg, com, addm, subm, bicm, bism), bic for x & ~y, sob for dec-and-branch loops */
long g; int gi; short gs; char gc;
struct r { long a, b; int c; long v[4]; } rr;
void inc1(long *p) { (*p)++; }
void dec1(long *p) { --*p; }
void neg1(long *p) { *p = -*p; }
void com1(long *p) { *p = ~*p; }
void add2(long *p, long v) { *p += v; }
void sub2(long *p, long v) { *p -= v; }
void or2(long *p, long v) { *p |= v; }
void clr2(long *p, long v) { *p &= ~v; }
void idx(long *p, long i, long v) { p[i] += v; p[i + 1]++; }
void incg(void) { g++; gi++; gs++; gc++; }
void flds(struct r *p, long v) { p->a += v; p->b -= v; p->c += 3; p->v[2] |= v; p->v[1]--; }
long andn(long x, long y) { return x & ~y; }
long andn2(long x, long y) { long t = ~y; return (x & t) + (y & ~x); }
unsigned andn3(unsigned x, unsigned y) { return x & ~y; }
long loop1(long n) { long s = 0; do { s += n; } while (--n); return s; }
long loop2(long n, long *a) { long s = 0; do { s += a[n - 1]; } while (--n != 0); return s; }
long loop3(long n) { long c = 0; while (n) { c++; n--; } return c; }
long loop4(long n, long k) { long c = 0; do { c += k; k++; n--; } while (n > 0); return c; }
long loop5(int n) { long c = 0; do { c += 2; } while (--n); return c; }
long w1(char *d, char *s, long n) { long c = 0; while (n--) { *d++ = *s++; c++; } return c; }
long w2(long n, long *a) { long c = 0; while (n--) { if (*a++ == 3) continue; c++; } return c; }
long w3(long n) { long c = 0; while (n--) { if (c == 4) break; c++; } return c; }
long w4(long n) { long c = 0; while (n--) c += n; return c; }		/* n used in the body */
long w5(long n) { long c = 0; while (n--) c++; return c + n; }		/* n used afterwards: -1 */
long w6(unsigned long n) { long c = 0; while (n--) { c += 2; if (c > 20) break; } return c; }
int test(void)
{
	long r = 0, x, a[6] = { 1, 2, 3, 4, 5, 6 };
	struct r s = { 10, 20, 30, { 1, 2, 3, 4 } };

	x = 5; inc1(&x); if (x != 6) r |= 1;
	dec1(&x); dec1(&x); if (x != 4) r |= 2;
	neg1(&x); if (x != -4) r |= 4;
	com1(&x); if (x != 3) r |= 8;
	add2(&x, 100); if (x != 103) r |= 16;
	sub2(&x, 3); if (x != 100) r |= 32;
	or2(&x, 3); if (x != 103) r |= 64;
	clr2(&x, 0x60); if (x != 7) r |= 128;
	idx(a, 2, 10); if (a[2] != 13 || a[3] != 5) r |= 256;
	g = 1; gi = -1; gs = 32767; gc = 127;
	incg(); if (g != 2 || gi != 0 || gs != -32768 || gc != -128) r |= 512;
	flds(&s, 5); if (s.a != 15 || s.b != 15 || s.c != 33 || s.v[2] != 7 || s.v[1] != 1) r |= 1024;
	if (andn(0xff, 0x0f) != 0xf0 || andn2(12, 10) != (4 + 2) || andn3(0xff00u, 0x0ff0u) != 0xf000u) r |= 2048;
	if (loop1(5) != 15) r |= 4096;
	if (loop2(6, a) != 1 + 2 + 13 + 5 + 5 + 6) r |= 8192;
	if (loop3(7) != 7) r |= 16384;
	if (loop4(3, 10) != 33) r |= 32768;
	if (loop5(4) != 8) r |= 65536;
	{
		char b1[8] = "abcdefg", b2[8];
		long q[5] = { 1, 3, 3, 4, 5 };

		if (w1(b2, b1, 7) != 7 || b2[6] != 'g' || b2[0] != 'a') r |= 131072;
		if (w1(b2, b1, 0) != 0) r |= 131072;
		if (w2(5, q) != 3 || w2(0, q) != 0) r |= 262144;
		if (w3(10) != 4 || w3(3) != 3 || w3(0) != 0) r |= 524288;
		if (w4(5) != 4 + 3 + 2 + 1 + 0) r |= 1048576;
		if (w5(3) != 3 - 1 || w5(0) != -1) r |= 2097152;
		if (w6(100) != 22 || w6(2) != 4 || w6(0) != 0) r |= 4194304;
	}
	return r;
}
