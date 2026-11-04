/* expect: 0 -- tail calls: direct, through a register, through memory, with a frame and callee-saved registers, with a local's address (no tail call), recursive */
long add3(long a, long b, long c) { return a * 100 + b * 10 + c; }
long (*fp)(long, long, long) = add3;
struct ops { long (*op)(struct ops *, long); long k; };
static long viaops(struct ops *o, long x) { return x + o->k; }
long direct(long a, long b) { return add3(a, b, 7); }
long indirect(long (*f)(long, long, long), long a) { return f(a, a + 1, a + 2); }
long global(long a) { return fp(a, 1, 2); }
long method(struct ops *o, long x) { return o->op(o, x); }
long saved(long a, long b)
{
	long t = add3(a, b, 1);		/* a and b live across the call: callee-saved registers, popped before the tail call */
	return add3(t % 7, a, b);
}
long viareg(long a, long (*f)(long, long, long))
{
	long t = add3(a, a, a);
	return f(t % 5, a, 3);		/* f lives across a call: a callee-saved register that the epilogue restores */
}
long sink(long *p) { return p[0] + p[1]; }
long local(long a) { long v[2]; v[0] = a; v[1] = 2 * a; return sink(v); }	/* no tail call: the frame is the argument */
long sum(long n, long acc) { if (n == 0) return acc; return sum(n - 1, acc + n); }
long cond(long a) { if (a > 0) return add3(a, a, a); return add3(0, 0, a); }
long many(long a, long b, long c, long d, long e, long f, long g) { return a + b + c + d + e + f + g; }
long stackarg(long a) { return many(a, 1, 2, 3, 4, 5, 6); }	/* an argument on the stack: not a tail call */
int test(void)
{
	long r = 0;
	struct ops o;

	o.op = viaops; o.k = 100;
	if (direct(1, 2) != 127) r |= 1;
	if (indirect(add3, 1) != 123 + 0) r |= 2;
	if (global(3) != 312) r |= 4;
	if (method(&o, 5) != 105) r |= 8;
	if (saved(2, 3) != add3((2 * 100 + 3 * 10 + 1) % 7, 2, 3)) r |= 16;
	if (viareg(4, add3) != add3(444 % 5, 4, 3)) r |= 32;
	if (local(5) != 15) r |= 64;
	if (sum(100, 0) != 5050) r |= 128;
	if (cond(2) != 222 || cond(-3) != -3) r |= 256;
	if (stackarg(10) != 31) r |= 512;
	return r;
}
