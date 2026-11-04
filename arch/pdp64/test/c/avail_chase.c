/* expect: 0 -- loads through the register they overwrite (p = *p) are not taken for the load before them (value availability after a register copy) */
struct n { struct n *next; long v; unsigned char tag; };
long chase(long *p) { long *a = (long *)*p; long *b = (long *)*a; long *c = (long *)*b; return *c; }
long walk(struct n *p, int k) { while (k--) p = p->next; return p->v; }
long two(struct n *ci, int idx)
{
	struct n *f = ci->next;		/* the value held in a register, loaded again from the register's old address */
	if (f->tag != 102) return -1;
	{ struct n *cl = ci->next->next; return cl->v + (idx > (int)cl->tag); }
}
int test(void)
{
	long r = 0, x = 42, y, z, w;
	long *py = &x, *pz, *pw;
	struct n n3 = { 0, 7, 3 }, n2 = { &n3, 6, 102 }, n1 = { &n2, 5, 1 }, n0 = { &n1, 4, 0 };

	pz = (long *)&py; pw = (long *)&pz;
	y = (long)pw; z = (long)&y;
	(void)z; (void)w;
	if (chase((long *)&pw) != 42) r |= 1;
	if (walk(&n0, 3) != 7 || walk(&n0, 1) != 5) r |= 2;
	if (two(&n1, 0) != 7 || two(&n1, 9) != 8 || two(&n0, 0) != -1) r |= 4;
	return r;
}
