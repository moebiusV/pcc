/* expect: 0 -- switches: a jump table, a search by halves, wide and negative cases, unsigned, a byte; the comparisons are compare-and-branch; a checksum made by the host compiler */
static long V[] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,-1,-5,-6,100000,100001,0xffffffffL,0x100000000L,0x100000001L,-0x100000000L,0x7fffffffffffffffL,(-0x7fffffffffffffffL-1),200,-200,255,256,-129,127,128,32768,65536 };
static unsigned long h;
static int sw_dense(long x) { switch (x) { case 0L: return 100; case 1L: return 101; case 2L: return 102; case 3L: return 103; case 4L: return 104; case 5L: return 105; case 6L: return 106; case 7L: return 107; case 8L: return 108; case 9L: return 109; case 10L: return 110; case 11L: return 111; default: return -1; } }
static int swn_dense(long x) { switch (x) { case 0L: return 100; case 1L: return 101; case 2L: return 102; case 3L: return 103; case 4L: return 104; case 5L: return 105; case 6L: return 106; case 7L: return 107; case 8L: return 108; case 9L: return 109; case 10L: return 110; case 11L: return 111; } return -2; }
static int sw_densenz(long x) { switch (x) { case 3L: return 100; case 4L: return 101; case 5L: return 102; case 6L: return 103; case 7L: return 104; case 8L: return 105; case 9L: return 106; case 10L: return 107; case 11L: return 108; case 12L: return 109; case 13L: return 110; case 14L: return 111; default: return -1; } }
static int swn_densenz(long x) { switch (x) { case 3L: return 100; case 4L: return 101; case 5L: return 102; case 6L: return 103; case 7L: return 104; case 8L: return 105; case 9L: return 106; case 10L: return 107; case 11L: return 108; case 12L: return 109; case 13L: return 110; case 14L: return 111; } return -2; }
static int sw_sparse(long x) { switch (x) { case 1L: return 100; case 5L: return 101; case 100L: return 102; case 1000L: return 103; case 100000L: return 104; case (-5L): return 105; case (-100000L): return 106; default: return -1; } }
static int swn_sparse(long x) { switch (x) { case 1L: return 100; case 5L: return 101; case 100L: return 102; case 1000L: return 103; case 100000L: return 104; case (-5L): return 105; case (-100000L): return 106; } return -2; }
static int sw_wide(long x) { switch (x) { case 4294967296L: return 100; case 4294967297L: return 101; case (-4294967296L): return 102; case 9223372036854775807L: return 103; case (-0x7fffffffffffffffL-1): return 104; case 4294967295L: return 105; case 2147483648L: return 106; case 5L: return 107; default: return -1; } }
static int swn_wide(long x) { switch (x) { case 4294967296L: return 100; case 4294967297L: return 101; case (-4294967296L): return 102; case 9223372036854775807L: return 103; case (-0x7fffffffffffffffL-1): return 104; case 4294967295L: return 105; case 2147483648L: return 106; case 5L: return 107; } return -2; }
static int sw_unsig(unsigned long x) { switch (x) { case 1UL: return 100; case 3UL: return 101; case 4294967295UL: return 102; case 4294967296UL: return 103; case 18446744073709551615UL: return 104; case 7UL: return 105; case 9223372036854775808UL: return 106; case 100000UL: return 107; default: return -1; } }
static int swn_unsig(unsigned long x) { switch (x) { case 1UL: return 100; case 3UL: return 101; case 4294967295UL: return 102; case 4294967296UL: return 103; case 18446744073709551615UL: return 104; case 7UL: return 105; case 9223372036854775808UL: return 106; case 100000UL: return 107; } return -2; }
static int sw_uint(unsigned int x) { switch (x) { case 1UL: return 100; case 3UL: return 101; case 4294967295UL: return 102; case 2147483648UL: return 103; case 7UL: return 104; case 100000UL: return 105; case 2147483647UL: return 106; default: return -1; } }
static int swn_uint(unsigned int x) { switch (x) { case 1UL: return 100; case 3UL: return 101; case 4294967295UL: return 102; case 2147483648UL: return 103; case 7UL: return 104; case 100000UL: return 105; case 2147483647UL: return 106; } return -2; }
static int sw_chr(signed char x) { switch (x) { case (-5L): return 100; case (-1L): return 101; case 0L: return 102; case 1L: return 103; case 5L: return 104; case 127L: return 105; case (-128L): return 106; case 100L: return 107; case 50L: return 108; case 60L: return 109; default: return -1; } }
static int swn_chr(signed char x) { switch (x) { case (-5L): return 100; case (-1L): return 101; case 0L: return 102; case 1L: return 103; case 5L: return 104; case 127L: return 105; case (-128L): return 106; case 100L: return 107; case 50L: return 108; case 60L: return 109; } return -2; }
static int sw_neg(int x) { switch (x) { case (-1L): return 100; case (-2L): return 101; case (-3L): return 102; case (-4L): return 103; case (-5L): return 104; case (-6L): return 105; case (-7L): return 106; case (-8L): return 107; case (-9L): return 108; case 0L: return 109; case 1L: return 110; default: return -1; } }
static int swn_neg(int x) { switch (x) { case (-1L): return 100; case (-2L): return 101; case (-3L): return 102; case (-4L): return 103; case (-5L): return 104; case (-6L): return 105; case (-7L): return 106; case (-8L): return 107; case (-9L): return 108; case 0L: return 109; case 1L: return 110; } return -2; }
int test(void)
{
	int i;

	for (i = 0; i < 33; i++) h = h * 31 + (unsigned long)(long)sw_dense((long)V[i]);
	for (i = 0; i < 33; i++) h = h * 31 + (unsigned long)(long)swn_dense((long)V[i]);
	for (i = 0; i < 33; i++) h = h * 31 + (unsigned long)(long)sw_densenz((long)V[i]);
	for (i = 0; i < 33; i++) h = h * 31 + (unsigned long)(long)swn_densenz((long)V[i]);
	for (i = 0; i < 33; i++) h = h * 31 + (unsigned long)(long)sw_sparse((long)V[i]);
	for (i = 0; i < 33; i++) h = h * 31 + (unsigned long)(long)swn_sparse((long)V[i]);
	for (i = 0; i < 33; i++) h = h * 31 + (unsigned long)(long)sw_wide((long)V[i]);
	for (i = 0; i < 33; i++) h = h * 31 + (unsigned long)(long)swn_wide((long)V[i]);
	for (i = 0; i < 33; i++) h = h * 31 + (unsigned long)(long)sw_unsig((unsigned long)V[i]);
	for (i = 0; i < 33; i++) h = h * 31 + (unsigned long)(long)swn_unsig((unsigned long)V[i]);
	for (i = 0; i < 33; i++) h = h * 31 + (unsigned long)(long)sw_uint((unsigned int)V[i]);
	for (i = 0; i < 33; i++) h = h * 31 + (unsigned long)(long)swn_uint((unsigned int)V[i]);
	for (i = 0; i < 33; i++) h = h * 31 + (unsigned long)(long)sw_chr((signed char)V[i]);
	for (i = 0; i < 33; i++) h = h * 31 + (unsigned long)(long)swn_chr((signed char)V[i]);
	for (i = 0; i < 33; i++) h = h * 31 + (unsigned long)(long)sw_neg((int)V[i]);
	for (i = 0; i < 33; i++) h = h * 31 + (unsigned long)(long)swn_neg((int)V[i]);
	return h != 2091068040740660146UL;
}
