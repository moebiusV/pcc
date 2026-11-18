/* expect: 0 flags: -xchecks -- the checks as instructions (ctcc, cteq, with .chk lines for the table of check sites): every check here passes, so none traps; a bound that does not fit the 12-bit immediate goes in a register */
static long a[10];
static long big[5000];
static long rd(unsigned long i) { return a[i]; }
static long rdb(long i) { return big[i]; }
static long sum(void) { long s = 0; int i; for (i = 0; i < 10; i++) s += a[i]; return s; }
static long gd(unsigned n) { if (n >= 10) return -1; return a[n]; }
static char *nn(char *p) { return __builtin_chkz(p); }
static long ck(long i) { return __builtin_chk(i, 100) + 1; }
int test(void)
{
	long r = 0;
	int i;
	static char c;

	for (i = 0; i < 10; i++) a[i] = i + 1;
	big[4999] = 7;
	if (rd(3) != 4 || rd(9) != 10) r |= 1;
	if (rdb(4999) != 7) r |= 2;
	if (sum() != 55) r |= 4;
	if (gd(5) != 6 || gd(50) != -1) r |= 8;
	if (nn(&c) != &c) r |= 16;
	if (ck(99) != 100) r |= 32;
	return r;
}
