/* expect: 0 -- a dense switch as a jump table: holes, a default, no default, negative cases, every integer type, the cases at both ends, a loop around the switch */
int dense(int x)
{
	switch (x) {
	case 0: return 10;
	case 1: return 11;
	case 2: return 12;
	case 3: return 13;
	case 5: return 15;
	case 6: return 16;
	case 7: return 17;
	case 8: return 18;
	case 9: return 19;
	default: return -1;
	}
}
int nodefault(int x)
{
	int r = 100;

	switch (x) {
	case -3: r = 1; break;
	case -2: r = 2; break;
	case -1: r = 3; break;
	case 0: r = 4; break;
	case 1: r = 5; break;
	case 2: r = 6; break;
	case 4: r = 8; break;
	case 5: r = 9; break;
	case 6: r = 10; break;
	}
	return r;
}
long lsw(long x)
{
	switch (x) {
	case 100: return 1;
	case 101: return 2;
	case 102: return 3;
	case 103: return 4;
	case 105: return 6;
	case 106: return 7;
	case 107: return 8;
	case 108: return 9;
	}
	return 0;
}
int csw(unsigned char c)
{
	switch (c) {
	case 'a': return 1;
	case 'b': return 2;
	case 'c': return 3;
	case 'd': return 4;
	case 'e': return 5;
	case 'f': return 6;
	case 'g': return 7;
	case 'h': return 8;
	case 'i': return 9;
	}
	return 0;
}
unsigned usw(unsigned x)
{
	switch (x) {
	case 4000000000u: return 1;
	case 4000000001u: return 2;
	case 4000000002u: return 3;
	case 4000000003u: return 4;
	case 4000000004u: return 5;
	case 4000000005u: return 6;
	case 4000000006u: return 7;
	case 4000000007u: return 8;
	}
	return 99;
}
int fall(int x)
{
	int r = 0;

	switch (x) {
	case 0: r += 1;
	case 1: r += 2;
	case 2: r += 4;
	case 3: r += 8; break;
	case 4: r += 16;
	case 5: r += 32;
	case 6: r += 64;
	case 7: r += 128; break;
	default: r = -5;
	}
	return r;
}
int vm(const int *code, int n)
{
	int acc = 0, pc;

	for (pc = 0; pc < n; pc++) {
		switch (code[pc]) {
		case 0: acc += 1; break;
		case 1: acc += 2; break;
		case 2: acc *= 3; break;
		case 3: acc -= 4; break;
		case 4: acc ^= 5; break;
		case 5: acc |= 8; break;
		case 6: acc &= 0xff; break;
		case 7: acc <<= 1; break;
		case 8: acc >>= 1; break;
		default: acc += 1000; break;
		}
	}
	return acc;
}
int test(void)
{
	long r = 0;
	int i;
	static const int code[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 2, 7, 3, 8 };
	static const int dr[] = { 10, 11, 12, 13, -1, 15, 16, 17, 18, 19, -1, -1 };
	static const int nr[] = { 1, 2, 3, 4, 5, 6, 100, 8, 9, 10, 100 };

	for (i = 0; i < 12; i++)
		if (dense(i) != dr[i]) r |= 1;
	if (dense(-1) != -1 || dense(-2147483647 - 1) != -1 || dense(2147483647) != -1) r |= 2;
	for (i = -3; i <= 7; i++)
		if (nodefault(i) != nr[i + 3]) r |= 4;
	if (nodefault(-4) != 100 || nodefault(1000000) != 100) r |= 8;
	for (i = 99; i < 111; i++)
		if (lsw(i) != (i < 100 || i == 104 || i > 108 ? 0 : i - 99)) r |= 16;
	if (lsw(-100) != 0 || lsw(9223372036854775807L) != 0) r |= 32;
	for (i = 96; i < 106; i++)
		if (csw(i) != (i >= 97 && i <= 105 ? i - 96 : 0)) r |= 64;
	if (csw(0) != 0 || csw(255) != 0 || csw(97 + 256) != 1 + 0) r |= 128;
	for (i = 0; i < 10; i++)
		if (usw(4000000000u + i - 1) != (i >= 1 && i <= 8 ? (unsigned)i : 99u)) r |= 256;
	if (usw(0) != 99 || usw(3999999999u) != 99 || usw(4000000008u) != 99) r |= 512;
	if (fall(0) != 15 || fall(1) != 14 || fall(3) != 8 || fall(4) != 240 || fall(7) != 128 || fall(8) != -5 || fall(-1) != -5) r |= 1024;
	{
		int acc = 0, pc;

		for (pc = 0; pc < 15; pc++) {
			switch (code[pc]) {
			case 0: acc += 1; break;
			case 1: acc += 2; break;
			case 2: acc *= 3; break;
			case 3: acc -= 4; break;
			case 4: acc ^= 5; break;
			case 5: acc |= 8; break;
			case 6: acc &= 0xff; break;
			case 7: acc <<= 1; break;
			case 8: acc >>= 1; break;
			default: acc += 1000; break;
			}
		}
		if (vm(code, 15) != acc) r |= 2048;
	}
	return r;
}
