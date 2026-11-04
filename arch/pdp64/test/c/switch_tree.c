/* expect: 0 -- switch dispatch by halves: dense, sparse, negative, unsigned, 64-bit, with and without default, falling through */
int dense(int x) { switch (x) { case 0: return 10; case 1: return 11; case 2: return 12; case 3: return 13; case 4: return 14; case 5: return 15;
	case 6: return 16; case 7: return 17; case 8: return 18; case 9: return 19; case 10: return 20; default: return -1; } }
int sparse(int x) { switch (x) { case -100000: return 1; case -7: return 2; case 0: return 3; case 9: return 4; case 50: return 5;
	case 1000: return 6; case 70000: return 7; case 2000000000: return 8; } return 0; }
int fall(int x) { int r = 0; switch (x) { case 1: r += 1; case 2: r += 2; case 3: r += 4; break; case 4: r += 8; case 5: r += 16; break;
	case 6: case 7: case 8: r += 32; break; case 9: r = 99; } return r; }
long u32(unsigned x) { switch (x) { case 0u: return 1; case 1u: return 2; case 2u: return 3; case 3u: return 4; case 4000000000u: return 5;
	case 4000000001u: return 6; case 7u: return 7; case 8u: return 8; } return 0; }
long l64(long x) { switch (x) { case -9000000000000L: return 1; case -1: return 2; case 0: return 3; case 5: return 4; case 6: return 5;
	case 7: return 6; case 8: return 7; case 9000000000000L: return 8; case 9000000000001L: return 9; } return 0; }
int ch(char c) { switch (c) { case 'a': return 1; case 'e': return 2; case 'i': return 3; case 'o': return 4; case 'u': return 5; case 'y': return 6; case ' ': return 7; } return 0; }
int test(void)
{
	int r = 0, i;
	for (i = -2; i <= 12; i++) if (dense(i) != (i >= 0 && i <= 10 ? 10 + i : -1)) r |= 1;
	if (sparse(-100000) != 1 || sparse(-7) != 2 || sparse(0) != 3 || sparse(9) != 4 || sparse(50) != 5 || sparse(1000) != 6 ||
	    sparse(70000) != 7 || sparse(2000000000) != 8 || sparse(1) != 0 || sparse(-8) != 0 || sparse(2000000001) != 0) r |= 2;
	if (fall(1) != 7 || fall(2) != 6 || fall(3) != 4 || fall(4) != 24 || fall(5) != 16 || fall(6) != 32 || fall(8) != 32 || fall(9) != 99 || fall(0) != 0 || fall(10) != 0) r |= 4;
	if (u32(0) != 1 || u32(3) != 4 || u32(4000000000u) != 5 || u32(4000000001u) != 6 || u32(7) != 7 || u32(8) != 8 || u32(5) != 0 || u32(0xffffffffu) != 0) r |= 8;
	if (l64(-9000000000000L) != 1 || l64(-1) != 2 || l64(0) != 3 || l64(7) != 6 || l64(9000000000001L) != 9 || l64(9000000000000L) != 8 || l64(4) != 0 || l64(-2) != 0) r |= 16;
	if (ch('a') != 1 || ch('y') != 6 || ch(' ') != 7 || ch('z') != 0 || ch('i') != 3 || ch('u') != 5 || ch(0) != 0) r |= 32;
	return r;
}
