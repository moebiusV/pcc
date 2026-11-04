/* expect: 0 -- bit fields: extract (extb, extzb) and insert (insb) in every position and width, signed and unsigned, 32-bit words and shifts */
struct bf { unsigned a:3, b:5, c:9; int d:4; unsigned e:7; int f:5; };
struct bq { unsigned long a:5, b:40, c:19; long d:8; };
unsigned fb(struct bf *p) { return p->b; }
unsigned fc(struct bf *p) { return p->c; }
int fd(struct bf *p) { return p->d; }
int ff(struct bf *p) { return p->f; }
unsigned fe(struct bf *p) { return p->e; }
void sb(struct bf *p, unsigned v) { p->b = v; }
void sc(struct bf *p, unsigned v) { p->c = v; }
void sd(struct bf *p, int v) { p->d = v; }
void sf(struct bf *p, int v) { p->f = v; }
void sa(struct bf *p, unsigned v) { p->a = v; }
unsigned long qb(struct bq *p) { return p->b; }
long qd(struct bq *p) { return p->d; }
void qsb(struct bq *p, unsigned long v) { p->b = v; }
void qsc(struct bq *p, unsigned long v) { p->c = v; }
void qsd(struct bq *p, long v) { p->d = v; }
unsigned ex(unsigned x) { return (x >> 3) & 0x1f; }
unsigned long ex2(unsigned long x) { return (x >> 11) & 0xfff; }
unsigned long ex3(unsigned long x) { return (x << 4) >> 9; }
long ex4(long x) { return (x << 4) >> 9; }
unsigned ex5(unsigned x) { return (x << 5) >> 28; }
int ex6(int x) { return (x << 5) >> 28; }
int test(void)
{
	long r = 0;
	struct bf s;
	struct bq q;
	unsigned *w = (unsigned *)&s;
	int i;

	*w = 0; w[1] = 0;
	sa(&s, 5); sb(&s, 19); sc(&s, 300); sd(&s, -3); sf(&s, -9); s.e = 100;
	if (s.a != 5 || fb(&s) != 19 || fc(&s) != 300 || fd(&s) != -3 || ff(&s) != -9 || fe(&s) != 100) r |= 1;
	sb(&s, 0xff);
	if (fb(&s) != 31 || s.a != 5 || fc(&s) != 300 || fd(&s) != -3) r |= 2;
	sd(&s, 7); if (fd(&s) != 7 || fc(&s) != 300) r |= 4;
	sd(&s, 8); if (fd(&s) != -8) r |= 8;
	sf(&s, 15); if (ff(&s) != 15) r |= 16;
	sf(&s, 16); if (ff(&s) != -16 || fe(&s) != 100) r |= 32;
	sc(&s, 511); if (fc(&s) != 511 || fb(&s) != 31 || fd(&s) != -8) r |= 64;
	sc(&s, 512); if (fc(&s) != 0 || fb(&s) != 31) r |= 128;
	q.a = 0; q.b = 0; q.c = 0; q.d = 0;
	qsb(&q, 0x123456789aUL); qsc(&q, 0x7ffff); qsd(&q, -100); q.a = 17;
	if (qb(&q) != 0x123456789aUL || qd(&q) != -100 || q.c != 0x7ffff || q.a != 17) r |= 256;
	qsb(&q, ~0UL); if (qb(&q) != 0xffffffffffUL || q.c != 0x7ffff || q.a != 17) r |= 512;
	for (i = 0; i < 4; i++) {
		unsigned x = 0xdeadbeefu + i * 0x1111;
		unsigned long y = 0xfedcba9876543210UL * (i + 1);
		long z = (long)y;
		int zi = (int)x;

		if (ex(x) != ((x >> 3) & 31)) r |= 1024;
		if (ex2(y) != ((y >> 11) & 0xfff)) r |= 2048;
		if (ex3(y) != ((y << 4) >> 9)) r |= 4096;
		if (ex4(z) != (z << 4) >> 9) r |= 8192;
		if (ex5(x) != (x << 5) >> 28) r |= 16384;
		if (ex6(zi) != (zi << 5) >> 28) r |= 32768;
	}
	return r;
}
