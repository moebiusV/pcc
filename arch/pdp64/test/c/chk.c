/* expect: 0 -- __builtin_chk, __builtin_chkz and __builtin_chk2: the value passes through when the check holds; __chk_fail is called with the kind, the value and the limit when it does not */
static int nfail, lastkind;
static long lastvalue;
static unsigned long lastlimit;
void __chk_fail(int kind, const char *file, int line, long value, unsigned long limit)
{
	nfail++;
	lastkind = kind;
	lastvalue = value;
	lastlimit = limit;
}
static long a[100];
long get(unsigned long i) { return a[__builtin_chk(i, 100)]; }
long getn(long i, long n) { return __builtin_chk(i, n); }
char *nn(char *p) { return __builtin_chkz(p); }
long band(long i) { return __builtin_chk2(i, 10, 50); }
long bandv(long i, long lo, long hi) { return __builtin_chk2(i, lo, hi); }
int test(void)
{
	long r = 0;
	char c;

	a[7] = 77;
	if (get(7) != 77 || nfail) r |= 1;				/* in range: no failure, the value passes */
	get(99);
	if (nfail) r |= 2;						/* the last element is in range */
	get(100);
	if (nfail != 1 || lastkind != 1 || lastvalue != 100 || lastlimit != 100) r |= 4;
	nfail = 0;
	getn(-1, 5);							/* a negative index is a huge unsigned number */
	if (nfail != 1 || lastkind != 1 || lastvalue != -1) r |= 8;
	nfail = 0;
	if (getn(3, 5) != 3 || getn(0, 5) != 0 || nfail) r |= 16;
	getn(5, 5); getn(0, 0);
	if (nfail != 2) r |= 32;
	nfail = 0;
	if (nn(&c) != &c || nfail) r |= 64;
	nn(0);
	if (nfail != 1 || lastkind != 2 || lastvalue != 0) r |= 128;
	nfail = 0;
	if (band(10) != 10 || band(49) != 49 || band(30) != 30 || nfail) r |= 256;
	band(9); band(50); band(-1000);
	if (nfail != 3 || lastkind != 3) r |= 512;
	if (lastvalue != -1000 - 10 || lastlimit != 40) r |= 1024;	/* value i - lo, limit hi - lo */
	nfail = 0;
	if (bandv(5, 5, 6) != 5 || nfail) r |= 2048;
	bandv(6, 5, 6); bandv(4, 5, 6);
	if (nfail != 2) r |= 4096;
	nfail = 0;
	bandv(0, 3, 3);						/* an empty range fails for every value */
	if (nfail != 1) r |= 8192;
	return (int)r;
}
