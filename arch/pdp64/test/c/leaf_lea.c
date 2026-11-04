/* expect: 0 -- address arithmetic with constants (lea), array and struct addressing, pointer differences */
struct s { char c; int i; long l; char pad[40]; long m; };
long off1(long *p) { return *(p + 5) + *(p + 100) + *(p - 3); }
long off2(struct s *p) { return p->l + p->m + p->i + p->c; }
char *addr(char *p) { return p + 4000; }
long diff(long *a, long *b) { return b - a; }
int test(void)
{
	long a[200], r = 0, i;
	struct s v;
	for (i = 0; i < 200; i++) a[i] = i * 3;
	if (off1(a + 10) != 15 * 3 + 110 * 3 + 7 * 3) r |= 1;
	v.c = -2; v.i = 40; v.l = 500; v.m = 6000;
	if (off2(&v) != 500 + 6000 + 40 - 2) r |= 2;
	if (addr((char *)0) != (char *)4000) r |= 4;
	if (diff(a + 3, a + 50) != 47) r |= 8;
	return (int)r;
}
