/* expect: 0 flags: -xchecks -- every subscript of an array of known size is checked: a[i], b[i][j], i[a]; not &a[n], a constant in range, a pointer, a one-element array */
static int nfail;
static long lastvalue;
static unsigned long lastlimit;
void __chk_fail(int kind, const char *file, int line, long value, unsigned long limit)
{
	nfail++;
	lastvalue = value;
	lastlimit = limit;
}
static long a[10];
static long b[3][4];
static char tail[1];
static long sum(void) { long s = 0, i; for (i = 0; i < 10; i++) s += a[i]; return s; }
static long rd(long i) { return a[i]; }
static long rd2(long i, long j) { return b[i][j]; }
static long rdu(unsigned char i) { return i[a]; }
static long *endp(void) { return &a[10]; }
static long wr(long i, long v) { a[i] = v; return a[0]; }
static long viap(long *p, long i) { return p[i]; }
static char flex(long i) { return tail[i]; }
int test(void)
{
	long r = 0, i;

	for (i = 0; i < 10; i++) a[i] = i + 1;
	for (i = 0; i < 12; i++) b[i / 4][i % 4] = 100 + i;
	if (sum() != 55 || nfail) r |= 1;				/* a loop in range: no failure, the same sum */
	if (rd(9) != 10 || rd(0) != 1 || nfail) r |= 2;
	rd(10);
	if (nfail != 1 || lastvalue != 10 || lastlimit != 10) r |= 4;	/* one past the end fails */
	nfail = 0;
	rd(-1);
	if (nfail != 1 || lastvalue != -1) r |= 8;
	nfail = 0;
	if (rd2(2, 3) != 111 || rd2(0, 0) != 100 || nfail) r |= 16;
	rd2(3, 0);
	if (nfail != 1 || lastlimit != 3) r |= 32;			/* the first subscript against 3 */
	nfail = 0;
	rd2(1, 4);
	if (nfail != 1 || lastlimit != 4) r |= 64;			/* the second against 4 */
	nfail = 0;
	if (rdu(9) != 10 || nfail) r |= 128;				/* i[a] and an unsigned char index */
	rdu(200);
	if (nfail != 1) r |= 256;
	nfail = 0;
	if (endp() != &a[10] || nfail) r |= 512;			/* &a[n] is an address, not a read: no check */
	wr(10, 5);
	if (nfail != 1) r |= 1024;					/* a store is checked too */
	nfail = 0;
	if (viap(a, 3) != 4 || nfail) r |= 2048;			/* a pointer's subscript is not checked (the array's size is lost) */
	flex(7);
	if (nfail) r |= 4096;						/* a one-element trailing array is not checked */
	return (int)r;
}
