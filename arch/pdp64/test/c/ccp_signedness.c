/* expect: 0 */
int test(void)
{
	int x = 0;
	x = ~x;
	if (x != 0xffffffff) return 1;
	return 0;
}
