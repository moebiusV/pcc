/*
 * The long long helpers of the PDP-11 backend (see lowerll.c), in C that uses
 * no long long.  genll.py compiles this and turns it into llhelpers.h; ccom
 * appends the ones a file needs, as routines local to the file.
 *
 * A long long is two longs, the high one first, as a long is two words.
 * Every helper takes the addresses of its operands; r may be a or b.
 */
struct ll { unsigned long hi, lo; };

/*
 * x >> n for an unsigned long: ashc shifts arithmetically, so shift the long and clear the
 * top n bits.  The compiler calls this for every unsigned long right shift.
 */
unsigned long
_llulr(x, n) unsigned long x; int n;
{
	long s;

	if (n <= 0)
		return x;
	if (n >= 32)
		return 0;
	s = (long)x >> 1;
	s &= 0x7fffffffL;
	return s >> (n - 1);
}

_llcpy(r, a) struct ll *r, *a;
{
	r->hi = a->hi;
	r->lo = a->lo;
}

_lladd(r, a, b) struct ll *r, *a, *b;
{
	unsigned long hi, lo;

	lo = a->lo + b->lo;
	hi = a->hi + b->hi;
	if (lo < a->lo)
		hi++;
	r->hi = hi;
	r->lo = lo;
}

_llsub(r, a, b) struct ll *r, *a, *b;
{
	unsigned long hi, lo;

	lo = a->lo - b->lo;
	hi = a->hi - b->hi;
	if (a->lo < b->lo)
		hi--;
	r->hi = hi;
	r->lo = lo;
}

_lland(r, a, b) struct ll *r, *a, *b;
{
	r->hi = a->hi & b->hi;
	r->lo = a->lo & b->lo;
}

_llor(r, a, b) struct ll *r, *a, *b;
{
	r->hi = a->hi | b->hi;
	r->lo = a->lo | b->lo;
}

_llxor(r, a, b) struct ll *r, *a, *b;
{
	r->hi = a->hi ^ b->hi;
	r->lo = a->lo ^ b->lo;
}

_llneg(r, a) struct ll *r, *a;
{
	unsigned long hi, lo;

	lo = -a->lo;
	hi = ~a->hi;
	if (lo == 0)
		hi++;
	r->hi = hi;
	r->lo = lo;
}

_llcom(r, a) struct ll *r, *a;
{
	r->hi = ~a->hi;
	r->lo = ~a->lo;
}

/* -1, 0, 1 as a is less than, equal to or greater than b */
_llcmp(a, b) struct ll *a, *b;
{
	if (a->hi != b->hi)
		return (long)a->hi < (long)b->hi ? -1 : 1;
	if (a->lo != b->lo)
		return a->lo < b->lo ? -1 : 1;
	return 0;
}

_llucm(a, b) struct ll *a, *b;
{
	if (a->hi != b->hi)
		return a->hi < b->hi ? -1 : 1;
	if (a->lo != b->lo)
		return a->lo < b->lo ? -1 : 1;
	return 0;
}

_llshl(r, a, n) struct ll *r, *a; int n;
{
	unsigned long hi, lo;

	hi = a->hi;
	lo = a->lo;
	if (n >= 64) {
		hi = 0;
		lo = 0;
	} else if (n >= 32) {
		hi = lo << (n - 32);
		lo = 0;
	} else if (n > 0) {
		hi = (hi << n) | (_llulr(lo, 32 - n));
		lo = lo << n;
	}
	r->hi = hi;
	r->lo = lo;
}

_llshr(r, a, n) struct ll *r, *a; int n;
{
	unsigned long hi, lo;
	long s;

	hi = a->hi;
	lo = a->lo;
	s = (long)hi >> 31;
	if (n >= 64) {
		hi = s;
		lo = s;
	} else if (n >= 32) {
		lo = (long)hi >> (n - 32);
		hi = s;
	} else if (n > 0) {
		lo = _llulr(lo, n) | (hi << (32 - n));
		hi = (long)hi >> n;
	}
	r->hi = hi;
	r->lo = lo;
}

_llush(r, a, n) struct ll *r, *a; int n;
{
	unsigned long hi, lo;

	hi = a->hi;
	lo = a->lo;
	if (n >= 64) {
		hi = 0;
		lo = 0;
	} else if (n >= 32) {
		lo = _llulr(hi, n - 32);
		hi = 0;
	} else if (n > 0) {
		lo = _llulr(lo, n) | (hi << (32 - n));
		hi = _llulr(hi, n);
	}
	r->hi = hi;
	r->lo = lo;
}

/* the 64-bit product of two longs */
_llm32(x, y, hp, lp) unsigned long x, y, *hp, *lp;
{
	unsigned long xl, xh, yl, yh, ll, lh, hl, hh, mid, lo, hi;

	xl = x & 0xffffL;
	xh = _llulr(x, 16);
	yl = y & 0xffffL;
	yh = _llulr(y, 16);
	ll = xl * yl;
	lh = xl * yh;
	hl = xh * yl;
	hh = xh * yh;
	mid = lh + hl;
	hi = hh;
	if (mid < lh)
		hi += 0x10000L;
	lo = ll + (mid << 16);
	if (lo < ll)
		hi++;
	hi += _llulr(mid, 16);
	*hp = hi;
	*lp = lo;
}

_llmul(r, a, b) struct ll *r, *a, *b;
{
	unsigned long hi, lo;

	_llm32(a->lo, b->lo, &hi, &lo);
	hi += a->hi * b->lo + a->lo * b->hi;
	r->hi = hi;
	r->lo = lo;
}

/* unsigned divide: quotient to q, remainder to rm */
_lludm(n, d, q, rm) struct ll *n, *d, *q, *rm;
{
	unsigned long nh, nl, dh, dl, qh, ql, rh, rl;
	int i, c;

	nh = n->hi;
	nl = n->lo;
	dh = d->hi;
	dl = d->lo;
	qh = ql = rh = rl = 0;
	for (i = 63; i >= 0; i--) {
		c = (rh & 0x80000000L) != 0;
		rh = (rh << 1) | _llulr(rl, 31);
		rl <<= 1;
		if (i >= 32)
			rl |= _llulr(nh, i - 32) & 1;
		else
			rl |= _llulr(nl, i) & 1;
		if (c || rh > dh || (rh == dh && rl >= dl)) {
			if (rl < dl)
				rh--;
			rl -= dl;
			rh -= dh;
			if (i >= 32)
				qh |= 1L << (i - 32);
			else
				ql |= 1L << i;
		}
	}
	q->hi = qh;
	q->lo = ql;
	rm->hi = rh;
	rm->lo = rl;
}

_lldvu(r, a, b) struct ll *r, *a, *b;
{
	struct ll q, rm;

	_lludm(a, b, &q, &rm);
	_llcpy(r, &q);
}

_llmdu(r, a, b) struct ll *r, *a, *b;
{
	struct ll q, rm;

	_lludm(a, b, &q, &rm);
	_llcpy(r, &rm);
}

_lldvs(r, a, b) struct ll *r, *a, *b;
{
	struct ll n, d, q, rm;
	int neg;

	neg = 0;
	_llcpy(&n, a);
	_llcpy(&d, b);
	if ((long)n.hi < 0) {
		_llneg(&n, &n);
		neg = 1;
	}
	if ((long)d.hi < 0) {
		_llneg(&d, &d);
		neg ^= 1;
	}
	_lludm(&n, &d, &q, &rm);
	if (neg)
		_llneg(&q, &q);
	_llcpy(r, &q);
}

_llmds(r, a, b) struct ll *r, *a, *b;
{
	struct ll n, d, q, rm;
	int neg;

	neg = 0;
	_llcpy(&n, a);
	_llcpy(&d, b);
	if ((long)n.hi < 0) {
		_llneg(&n, &n);
		neg = 1;
	}
	if ((long)d.hi < 0)
		_llneg(&d, &d);
	_lludm(&n, &d, &q, &rm);
	if (neg)
		_llneg(&rm, &rm);
	_llcpy(r, &rm);
}

_llfls(r, v) struct ll *r; long v;
{
	r->lo = v;
	r->hi = v < 0 ? 0xffffffffL : 0L;
}

_llflu(r, v) struct ll *r; unsigned long v;
{
	r->lo = v;
	r->hi = 0;
}

long
_lltol(a) struct ll *a;
{
	return a->lo;
}
