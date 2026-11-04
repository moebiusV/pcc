/* expect: 0 -- post-increment and pre-decrement operands: *p++, *--p, strides of every size, a copy kept, a pointer used after */
struct s3 { char a, b, c; };
struct s12 { int a, b, c; };
void cpy(char *d, char *s, long n) { while (n--) *d++ = *s++; }
long len(char *s) { char *p = s; while (*p) p++; return p - s; }
void cpyz(char *d, char *s) { while ((*d++ = *s++)) ; }
long sumi(int *a, long n) { long s = 0; while (n--) s += *a++; return s; }
long sums(short *a, long n) { long s = 0; long i; for (i = 0; i < n; i++) s += *a++; return s; }
long suml(long *a, long n) { long s = 0; long i; for (i = 0; i < n; i++) { s += *a; a++; } return s; }
long sumr(int *a, long n) { long s = 0; a += n; while (n--) s += *--a; return s; }
long sumst(struct s12 *a, long n) { long s = 0; long i; for (i = 0; i < n; i++) { s += a->b; a++; } return s; }
long sums3(struct s3 *a, long n) { long s = 0; long i; for (i = 0; i < n; i++) { s += a->a; a++; } return s; }
char *after(char *p, char *q) { char c = *p++; *q++ = c; return p; }
long both(int *p) { long x = *p++; long y = *p++; return x * 10 + y + (long)(*p); }
void rev(char *d, char *s, long n) { s += n; while (n--) *d++ = *--s; }
unsigned char *ubytes(unsigned char *p, long *out) { long x = *p++; x = x * 256 + *p++; *out = x; return p; }
int test(void)
{
	long r = 0;
	char a[16], b[16];
	int ia[6] = { 1, 2, 3, 4, 5, 6 };
	short sa[4] = { 10, 20, 30, 40 };
	long la[3] = { 100, 200, 300 };
	struct s12 st[3] = { { 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 9 } };
	struct s3 t3[3] = { { 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 9 } };
	unsigned char ub[3] = { 0x12, 0xfe, 0x7 };
	long v;
	int i;

	for (i = 0; i < 16; i++) a[i] = i + 1;
	cpy(b, a, 16);
	for (i = 0; i < 16; i++) if (b[i] != i + 1) r |= 1;
	if (len("hello") != 5) r |= 2;
	cpyz(b, "abc");
	if (b[0] != 'a' || b[2] != 'c' || b[3] != 0) r |= 4;
	if (sumi(ia, 6) != 21) r |= 8;
	if (sums(sa, 4) != 100) r |= 16;
	if (suml(la, 3) != 600) r |= 32;
	if (sumr(ia, 6) != 21) r |= 64;
	if (sumst(st, 3) != 15) r |= 128;
	if (sums3(t3, 3) != 12) r |= 256;
	if (after(a, b) != a + 1 || b[0] != 1) r |= 512;
	if (both(ia) != 12 + 3) r |= 1024;
	rev(b, a, 5);
	if (b[0] != 5 || b[4] != 1) r |= 2048;
	if (ubytes(ub, &v) != ub + 2 || v != 0x12fe) r |= 4096;
	return r;
}
