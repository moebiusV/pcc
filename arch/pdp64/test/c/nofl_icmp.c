/* expect: 0 -- integer comparisons with the constants of the cbCC forms (16-bit immediates, 8-bit immediates of setCC, wider ones through r12), as values, branches and registers, signed and unsigned, 64 bits down to a byte: a checksum made by the host compiler */
static long V[14] = { 0L, 1L, -1L, 127L, 128L, -129L, 40000L, 65535L, 0x7fffffffL, 0x80000000L, 0xffffffffL, 0x123456789L, 0x7fffffffffffffffL, 0x8000000000000000L };
static unsigned long h;
#define ADD(x) (h = h * 31 + (x))
static int l_lt_v0(long a) { return a < (long)0L; }
static int l_lt_b0(long a) { if (a < (long)0L) return 7; return 3; }
static int l_lt_v1(long a) { return a < (long)127L; }
static int l_lt_b1(long a) { if (a < (long)127L) return 7; return 3; }
static int l_lt_v2(long a) { return a < (long)-129L; }
static int l_lt_b2(long a) { if (a < (long)-129L) return 7; return 3; }
static int l_lt_v3(long a) { return a < (long)40000L; }
static int l_lt_b3(long a) { if (a < (long)40000L) return 7; return 3; }
static int l_lt_v4(long a) { return a < (long)0x80000000L; }
static int l_lt_b4(long a) { if (a < (long)0x80000000L) return 7; return 3; }
static int l_lt_v5(long a) { return a < (long)0x123456789L; }
static int l_lt_b5(long a) { if (a < (long)0x123456789L) return 7; return 3; }
static int l_lt_rv(long a, long b) { return a < b; }
static int l_lt_rb(long a, long b) { if (a < b) return 7; return 3; }
static int l_lt_rt(long a, long b) { return (a < b) ? 10 : 20; }
static int l_lt_m(long *p, long b) { return *p < b; }
static int l_le_v0(long a) { return a <= (long)0L; }
static int l_le_b0(long a) { if (a <= (long)0L) return 7; return 3; }
static int l_le_v1(long a) { return a <= (long)127L; }
static int l_le_b1(long a) { if (a <= (long)127L) return 7; return 3; }
static int l_le_v2(long a) { return a <= (long)-129L; }
static int l_le_b2(long a) { if (a <= (long)-129L) return 7; return 3; }
static int l_le_v3(long a) { return a <= (long)40000L; }
static int l_le_b3(long a) { if (a <= (long)40000L) return 7; return 3; }
static int l_le_v4(long a) { return a <= (long)0x80000000L; }
static int l_le_b4(long a) { if (a <= (long)0x80000000L) return 7; return 3; }
static int l_le_v5(long a) { return a <= (long)0x123456789L; }
static int l_le_b5(long a) { if (a <= (long)0x123456789L) return 7; return 3; }
static int l_le_rv(long a, long b) { return a <= b; }
static int l_le_rb(long a, long b) { if (a <= b) return 7; return 3; }
static int l_le_rt(long a, long b) { return (a <= b) ? 10 : 20; }
static int l_le_m(long *p, long b) { return *p <= b; }
static int l_gt_v0(long a) { return a > (long)0L; }
static int l_gt_b0(long a) { if (a > (long)0L) return 7; return 3; }
static int l_gt_v1(long a) { return a > (long)127L; }
static int l_gt_b1(long a) { if (a > (long)127L) return 7; return 3; }
static int l_gt_v2(long a) { return a > (long)-129L; }
static int l_gt_b2(long a) { if (a > (long)-129L) return 7; return 3; }
static int l_gt_v3(long a) { return a > (long)40000L; }
static int l_gt_b3(long a) { if (a > (long)40000L) return 7; return 3; }
static int l_gt_v4(long a) { return a > (long)0x80000000L; }
static int l_gt_b4(long a) { if (a > (long)0x80000000L) return 7; return 3; }
static int l_gt_v5(long a) { return a > (long)0x123456789L; }
static int l_gt_b5(long a) { if (a > (long)0x123456789L) return 7; return 3; }
static int l_gt_rv(long a, long b) { return a > b; }
static int l_gt_rb(long a, long b) { if (a > b) return 7; return 3; }
static int l_gt_rt(long a, long b) { return (a > b) ? 10 : 20; }
static int l_gt_m(long *p, long b) { return *p > b; }
static int l_ge_v0(long a) { return a >= (long)0L; }
static int l_ge_b0(long a) { if (a >= (long)0L) return 7; return 3; }
static int l_ge_v1(long a) { return a >= (long)127L; }
static int l_ge_b1(long a) { if (a >= (long)127L) return 7; return 3; }
static int l_ge_v2(long a) { return a >= (long)-129L; }
static int l_ge_b2(long a) { if (a >= (long)-129L) return 7; return 3; }
static int l_ge_v3(long a) { return a >= (long)40000L; }
static int l_ge_b3(long a) { if (a >= (long)40000L) return 7; return 3; }
static int l_ge_v4(long a) { return a >= (long)0x80000000L; }
static int l_ge_b4(long a) { if (a >= (long)0x80000000L) return 7; return 3; }
static int l_ge_v5(long a) { return a >= (long)0x123456789L; }
static int l_ge_b5(long a) { if (a >= (long)0x123456789L) return 7; return 3; }
static int l_ge_rv(long a, long b) { return a >= b; }
static int l_ge_rb(long a, long b) { if (a >= b) return 7; return 3; }
static int l_ge_rt(long a, long b) { return (a >= b) ? 10 : 20; }
static int l_ge_m(long *p, long b) { return *p >= b; }
static int l_eq_v0(long a) { return a == (long)0L; }
static int l_eq_b0(long a) { if (a == (long)0L) return 7; return 3; }
static int l_eq_v1(long a) { return a == (long)127L; }
static int l_eq_b1(long a) { if (a == (long)127L) return 7; return 3; }
static int l_eq_v2(long a) { return a == (long)-129L; }
static int l_eq_b2(long a) { if (a == (long)-129L) return 7; return 3; }
static int l_eq_v3(long a) { return a == (long)40000L; }
static int l_eq_b3(long a) { if (a == (long)40000L) return 7; return 3; }
static int l_eq_v4(long a) { return a == (long)0x80000000L; }
static int l_eq_b4(long a) { if (a == (long)0x80000000L) return 7; return 3; }
static int l_eq_v5(long a) { return a == (long)0x123456789L; }
static int l_eq_b5(long a) { if (a == (long)0x123456789L) return 7; return 3; }
static int l_eq_rv(long a, long b) { return a == b; }
static int l_eq_rb(long a, long b) { if (a == b) return 7; return 3; }
static int l_eq_rt(long a, long b) { return (a == b) ? 10 : 20; }
static int l_eq_m(long *p, long b) { return *p == b; }
static int l_ne_v0(long a) { return a != (long)0L; }
static int l_ne_b0(long a) { if (a != (long)0L) return 7; return 3; }
static int l_ne_v1(long a) { return a != (long)127L; }
static int l_ne_b1(long a) { if (a != (long)127L) return 7; return 3; }
static int l_ne_v2(long a) { return a != (long)-129L; }
static int l_ne_b2(long a) { if (a != (long)-129L) return 7; return 3; }
static int l_ne_v3(long a) { return a != (long)40000L; }
static int l_ne_b3(long a) { if (a != (long)40000L) return 7; return 3; }
static int l_ne_v4(long a) { return a != (long)0x80000000L; }
static int l_ne_b4(long a) { if (a != (long)0x80000000L) return 7; return 3; }
static int l_ne_v5(long a) { return a != (long)0x123456789L; }
static int l_ne_b5(long a) { if (a != (long)0x123456789L) return 7; return 3; }
static int l_ne_rv(long a, long b) { return a != b; }
static int l_ne_rb(long a, long b) { if (a != b) return 7; return 3; }
static int l_ne_rt(long a, long b) { return (a != b) ? 10 : 20; }
static int l_ne_m(long *p, long b) { return *p != b; }
static int ul_lt_v0(unsigned long a) { return a < (unsigned long)0L; }
static int ul_lt_b0(unsigned long a) { if (a < (unsigned long)0L) return 7; return 3; }
static int ul_lt_v1(unsigned long a) { return a < (unsigned long)127L; }
static int ul_lt_b1(unsigned long a) { if (a < (unsigned long)127L) return 7; return 3; }
static int ul_lt_v2(unsigned long a) { return a < (unsigned long)-129L; }
static int ul_lt_b2(unsigned long a) { if (a < (unsigned long)-129L) return 7; return 3; }
static int ul_lt_v3(unsigned long a) { return a < (unsigned long)40000L; }
static int ul_lt_b3(unsigned long a) { if (a < (unsigned long)40000L) return 7; return 3; }
static int ul_lt_v4(unsigned long a) { return a < (unsigned long)0x80000000L; }
static int ul_lt_b4(unsigned long a) { if (a < (unsigned long)0x80000000L) return 7; return 3; }
static int ul_lt_v5(unsigned long a) { return a < (unsigned long)0x123456789L; }
static int ul_lt_b5(unsigned long a) { if (a < (unsigned long)0x123456789L) return 7; return 3; }
static int ul_lt_rv(unsigned long a, unsigned long b) { return a < b; }
static int ul_lt_rb(unsigned long a, unsigned long b) { if (a < b) return 7; return 3; }
static int ul_lt_rt(unsigned long a, unsigned long b) { return (a < b) ? 10 : 20; }
static int ul_lt_m(unsigned long *p, unsigned long b) { return *p < b; }
static int ul_le_v0(unsigned long a) { return a <= (unsigned long)0L; }
static int ul_le_b0(unsigned long a) { if (a <= (unsigned long)0L) return 7; return 3; }
static int ul_le_v1(unsigned long a) { return a <= (unsigned long)127L; }
static int ul_le_b1(unsigned long a) { if (a <= (unsigned long)127L) return 7; return 3; }
static int ul_le_v2(unsigned long a) { return a <= (unsigned long)-129L; }
static int ul_le_b2(unsigned long a) { if (a <= (unsigned long)-129L) return 7; return 3; }
static int ul_le_v3(unsigned long a) { return a <= (unsigned long)40000L; }
static int ul_le_b3(unsigned long a) { if (a <= (unsigned long)40000L) return 7; return 3; }
static int ul_le_v4(unsigned long a) { return a <= (unsigned long)0x80000000L; }
static int ul_le_b4(unsigned long a) { if (a <= (unsigned long)0x80000000L) return 7; return 3; }
static int ul_le_v5(unsigned long a) { return a <= (unsigned long)0x123456789L; }
static int ul_le_b5(unsigned long a) { if (a <= (unsigned long)0x123456789L) return 7; return 3; }
static int ul_le_rv(unsigned long a, unsigned long b) { return a <= b; }
static int ul_le_rb(unsigned long a, unsigned long b) { if (a <= b) return 7; return 3; }
static int ul_le_rt(unsigned long a, unsigned long b) { return (a <= b) ? 10 : 20; }
static int ul_le_m(unsigned long *p, unsigned long b) { return *p <= b; }
static int ul_gt_v0(unsigned long a) { return a > (unsigned long)0L; }
static int ul_gt_b0(unsigned long a) { if (a > (unsigned long)0L) return 7; return 3; }
static int ul_gt_v1(unsigned long a) { return a > (unsigned long)127L; }
static int ul_gt_b1(unsigned long a) { if (a > (unsigned long)127L) return 7; return 3; }
static int ul_gt_v2(unsigned long a) { return a > (unsigned long)-129L; }
static int ul_gt_b2(unsigned long a) { if (a > (unsigned long)-129L) return 7; return 3; }
static int ul_gt_v3(unsigned long a) { return a > (unsigned long)40000L; }
static int ul_gt_b3(unsigned long a) { if (a > (unsigned long)40000L) return 7; return 3; }
static int ul_gt_v4(unsigned long a) { return a > (unsigned long)0x80000000L; }
static int ul_gt_b4(unsigned long a) { if (a > (unsigned long)0x80000000L) return 7; return 3; }
static int ul_gt_v5(unsigned long a) { return a > (unsigned long)0x123456789L; }
static int ul_gt_b5(unsigned long a) { if (a > (unsigned long)0x123456789L) return 7; return 3; }
static int ul_gt_rv(unsigned long a, unsigned long b) { return a > b; }
static int ul_gt_rb(unsigned long a, unsigned long b) { if (a > b) return 7; return 3; }
static int ul_gt_rt(unsigned long a, unsigned long b) { return (a > b) ? 10 : 20; }
static int ul_gt_m(unsigned long *p, unsigned long b) { return *p > b; }
static int ul_ge_v0(unsigned long a) { return a >= (unsigned long)0L; }
static int ul_ge_b0(unsigned long a) { if (a >= (unsigned long)0L) return 7; return 3; }
static int ul_ge_v1(unsigned long a) { return a >= (unsigned long)127L; }
static int ul_ge_b1(unsigned long a) { if (a >= (unsigned long)127L) return 7; return 3; }
static int ul_ge_v2(unsigned long a) { return a >= (unsigned long)-129L; }
static int ul_ge_b2(unsigned long a) { if (a >= (unsigned long)-129L) return 7; return 3; }
static int ul_ge_v3(unsigned long a) { return a >= (unsigned long)40000L; }
static int ul_ge_b3(unsigned long a) { if (a >= (unsigned long)40000L) return 7; return 3; }
static int ul_ge_v4(unsigned long a) { return a >= (unsigned long)0x80000000L; }
static int ul_ge_b4(unsigned long a) { if (a >= (unsigned long)0x80000000L) return 7; return 3; }
static int ul_ge_v5(unsigned long a) { return a >= (unsigned long)0x123456789L; }
static int ul_ge_b5(unsigned long a) { if (a >= (unsigned long)0x123456789L) return 7; return 3; }
static int ul_ge_rv(unsigned long a, unsigned long b) { return a >= b; }
static int ul_ge_rb(unsigned long a, unsigned long b) { if (a >= b) return 7; return 3; }
static int ul_ge_rt(unsigned long a, unsigned long b) { return (a >= b) ? 10 : 20; }
static int ul_ge_m(unsigned long *p, unsigned long b) { return *p >= b; }
static int ul_eq_v0(unsigned long a) { return a == (unsigned long)0L; }
static int ul_eq_b0(unsigned long a) { if (a == (unsigned long)0L) return 7; return 3; }
static int ul_eq_v1(unsigned long a) { return a == (unsigned long)127L; }
static int ul_eq_b1(unsigned long a) { if (a == (unsigned long)127L) return 7; return 3; }
static int ul_eq_v2(unsigned long a) { return a == (unsigned long)-129L; }
static int ul_eq_b2(unsigned long a) { if (a == (unsigned long)-129L) return 7; return 3; }
static int ul_eq_v3(unsigned long a) { return a == (unsigned long)40000L; }
static int ul_eq_b3(unsigned long a) { if (a == (unsigned long)40000L) return 7; return 3; }
static int ul_eq_v4(unsigned long a) { return a == (unsigned long)0x80000000L; }
static int ul_eq_b4(unsigned long a) { if (a == (unsigned long)0x80000000L) return 7; return 3; }
static int ul_eq_v5(unsigned long a) { return a == (unsigned long)0x123456789L; }
static int ul_eq_b5(unsigned long a) { if (a == (unsigned long)0x123456789L) return 7; return 3; }
static int ul_eq_rv(unsigned long a, unsigned long b) { return a == b; }
static int ul_eq_rb(unsigned long a, unsigned long b) { if (a == b) return 7; return 3; }
static int ul_eq_rt(unsigned long a, unsigned long b) { return (a == b) ? 10 : 20; }
static int ul_eq_m(unsigned long *p, unsigned long b) { return *p == b; }
static int ul_ne_v0(unsigned long a) { return a != (unsigned long)0L; }
static int ul_ne_b0(unsigned long a) { if (a != (unsigned long)0L) return 7; return 3; }
static int ul_ne_v1(unsigned long a) { return a != (unsigned long)127L; }
static int ul_ne_b1(unsigned long a) { if (a != (unsigned long)127L) return 7; return 3; }
static int ul_ne_v2(unsigned long a) { return a != (unsigned long)-129L; }
static int ul_ne_b2(unsigned long a) { if (a != (unsigned long)-129L) return 7; return 3; }
static int ul_ne_v3(unsigned long a) { return a != (unsigned long)40000L; }
static int ul_ne_b3(unsigned long a) { if (a != (unsigned long)40000L) return 7; return 3; }
static int ul_ne_v4(unsigned long a) { return a != (unsigned long)0x80000000L; }
static int ul_ne_b4(unsigned long a) { if (a != (unsigned long)0x80000000L) return 7; return 3; }
static int ul_ne_v5(unsigned long a) { return a != (unsigned long)0x123456789L; }
static int ul_ne_b5(unsigned long a) { if (a != (unsigned long)0x123456789L) return 7; return 3; }
static int ul_ne_rv(unsigned long a, unsigned long b) { return a != b; }
static int ul_ne_rb(unsigned long a, unsigned long b) { if (a != b) return 7; return 3; }
static int ul_ne_rt(unsigned long a, unsigned long b) { return (a != b) ? 10 : 20; }
static int ul_ne_m(unsigned long *p, unsigned long b) { return *p != b; }
static int i_lt_v0(int a) { return a < (int)0L; }
static int i_lt_b0(int a) { if (a < (int)0L) return 7; return 3; }
static int i_lt_v1(int a) { return a < (int)127L; }
static int i_lt_b1(int a) { if (a < (int)127L) return 7; return 3; }
static int i_lt_v2(int a) { return a < (int)-129L; }
static int i_lt_b2(int a) { if (a < (int)-129L) return 7; return 3; }
static int i_lt_v3(int a) { return a < (int)40000L; }
static int i_lt_b3(int a) { if (a < (int)40000L) return 7; return 3; }
static int i_lt_v4(int a) { return a < (int)0x80000000L; }
static int i_lt_b4(int a) { if (a < (int)0x80000000L) return 7; return 3; }
static int i_lt_v5(int a) { return a < (int)0x123456789L; }
static int i_lt_b5(int a) { if (a < (int)0x123456789L) return 7; return 3; }
static int i_lt_rv(int a, int b) { return a < b; }
static int i_lt_rb(int a, int b) { if (a < b) return 7; return 3; }
static int i_lt_rt(int a, int b) { return (a < b) ? 10 : 20; }
static int i_lt_m(int *p, int b) { return *p < b; }
static int i_le_v0(int a) { return a <= (int)0L; }
static int i_le_b0(int a) { if (a <= (int)0L) return 7; return 3; }
static int i_le_v1(int a) { return a <= (int)127L; }
static int i_le_b1(int a) { if (a <= (int)127L) return 7; return 3; }
static int i_le_v2(int a) { return a <= (int)-129L; }
static int i_le_b2(int a) { if (a <= (int)-129L) return 7; return 3; }
static int i_le_v3(int a) { return a <= (int)40000L; }
static int i_le_b3(int a) { if (a <= (int)40000L) return 7; return 3; }
static int i_le_v4(int a) { return a <= (int)0x80000000L; }
static int i_le_b4(int a) { if (a <= (int)0x80000000L) return 7; return 3; }
static int i_le_v5(int a) { return a <= (int)0x123456789L; }
static int i_le_b5(int a) { if (a <= (int)0x123456789L) return 7; return 3; }
static int i_le_rv(int a, int b) { return a <= b; }
static int i_le_rb(int a, int b) { if (a <= b) return 7; return 3; }
static int i_le_rt(int a, int b) { return (a <= b) ? 10 : 20; }
static int i_le_m(int *p, int b) { return *p <= b; }
static int i_gt_v0(int a) { return a > (int)0L; }
static int i_gt_b0(int a) { if (a > (int)0L) return 7; return 3; }
static int i_gt_v1(int a) { return a > (int)127L; }
static int i_gt_b1(int a) { if (a > (int)127L) return 7; return 3; }
static int i_gt_v2(int a) { return a > (int)-129L; }
static int i_gt_b2(int a) { if (a > (int)-129L) return 7; return 3; }
static int i_gt_v3(int a) { return a > (int)40000L; }
static int i_gt_b3(int a) { if (a > (int)40000L) return 7; return 3; }
static int i_gt_v4(int a) { return a > (int)0x80000000L; }
static int i_gt_b4(int a) { if (a > (int)0x80000000L) return 7; return 3; }
static int i_gt_v5(int a) { return a > (int)0x123456789L; }
static int i_gt_b5(int a) { if (a > (int)0x123456789L) return 7; return 3; }
static int i_gt_rv(int a, int b) { return a > b; }
static int i_gt_rb(int a, int b) { if (a > b) return 7; return 3; }
static int i_gt_rt(int a, int b) { return (a > b) ? 10 : 20; }
static int i_gt_m(int *p, int b) { return *p > b; }
static int i_ge_v0(int a) { return a >= (int)0L; }
static int i_ge_b0(int a) { if (a >= (int)0L) return 7; return 3; }
static int i_ge_v1(int a) { return a >= (int)127L; }
static int i_ge_b1(int a) { if (a >= (int)127L) return 7; return 3; }
static int i_ge_v2(int a) { return a >= (int)-129L; }
static int i_ge_b2(int a) { if (a >= (int)-129L) return 7; return 3; }
static int i_ge_v3(int a) { return a >= (int)40000L; }
static int i_ge_b3(int a) { if (a >= (int)40000L) return 7; return 3; }
static int i_ge_v4(int a) { return a >= (int)0x80000000L; }
static int i_ge_b4(int a) { if (a >= (int)0x80000000L) return 7; return 3; }
static int i_ge_v5(int a) { return a >= (int)0x123456789L; }
static int i_ge_b5(int a) { if (a >= (int)0x123456789L) return 7; return 3; }
static int i_ge_rv(int a, int b) { return a >= b; }
static int i_ge_rb(int a, int b) { if (a >= b) return 7; return 3; }
static int i_ge_rt(int a, int b) { return (a >= b) ? 10 : 20; }
static int i_ge_m(int *p, int b) { return *p >= b; }
static int i_eq_v0(int a) { return a == (int)0L; }
static int i_eq_b0(int a) { if (a == (int)0L) return 7; return 3; }
static int i_eq_v1(int a) { return a == (int)127L; }
static int i_eq_b1(int a) { if (a == (int)127L) return 7; return 3; }
static int i_eq_v2(int a) { return a == (int)-129L; }
static int i_eq_b2(int a) { if (a == (int)-129L) return 7; return 3; }
static int i_eq_v3(int a) { return a == (int)40000L; }
static int i_eq_b3(int a) { if (a == (int)40000L) return 7; return 3; }
static int i_eq_v4(int a) { return a == (int)0x80000000L; }
static int i_eq_b4(int a) { if (a == (int)0x80000000L) return 7; return 3; }
static int i_eq_v5(int a) { return a == (int)0x123456789L; }
static int i_eq_b5(int a) { if (a == (int)0x123456789L) return 7; return 3; }
static int i_eq_rv(int a, int b) { return a == b; }
static int i_eq_rb(int a, int b) { if (a == b) return 7; return 3; }
static int i_eq_rt(int a, int b) { return (a == b) ? 10 : 20; }
static int i_eq_m(int *p, int b) { return *p == b; }
static int i_ne_v0(int a) { return a != (int)0L; }
static int i_ne_b0(int a) { if (a != (int)0L) return 7; return 3; }
static int i_ne_v1(int a) { return a != (int)127L; }
static int i_ne_b1(int a) { if (a != (int)127L) return 7; return 3; }
static int i_ne_v2(int a) { return a != (int)-129L; }
static int i_ne_b2(int a) { if (a != (int)-129L) return 7; return 3; }
static int i_ne_v3(int a) { return a != (int)40000L; }
static int i_ne_b3(int a) { if (a != (int)40000L) return 7; return 3; }
static int i_ne_v4(int a) { return a != (int)0x80000000L; }
static int i_ne_b4(int a) { if (a != (int)0x80000000L) return 7; return 3; }
static int i_ne_v5(int a) { return a != (int)0x123456789L; }
static int i_ne_b5(int a) { if (a != (int)0x123456789L) return 7; return 3; }
static int i_ne_rv(int a, int b) { return a != b; }
static int i_ne_rb(int a, int b) { if (a != b) return 7; return 3; }
static int i_ne_rt(int a, int b) { return (a != b) ? 10 : 20; }
static int i_ne_m(int *p, int b) { return *p != b; }
static int uc_lt_v0(unsigned char a) { return a < (unsigned char)0L; }
static int uc_lt_b0(unsigned char a) { if (a < (unsigned char)0L) return 7; return 3; }
static int uc_lt_v1(unsigned char a) { return a < (unsigned char)127L; }
static int uc_lt_b1(unsigned char a) { if (a < (unsigned char)127L) return 7; return 3; }
static int uc_lt_v2(unsigned char a) { return a < (unsigned char)-129L; }
static int uc_lt_b2(unsigned char a) { if (a < (unsigned char)-129L) return 7; return 3; }
static int uc_lt_v3(unsigned char a) { return a < (unsigned char)40000L; }
static int uc_lt_b3(unsigned char a) { if (a < (unsigned char)40000L) return 7; return 3; }
static int uc_lt_v4(unsigned char a) { return a < (unsigned char)0x80000000L; }
static int uc_lt_b4(unsigned char a) { if (a < (unsigned char)0x80000000L) return 7; return 3; }
static int uc_lt_v5(unsigned char a) { return a < (unsigned char)0x123456789L; }
static int uc_lt_b5(unsigned char a) { if (a < (unsigned char)0x123456789L) return 7; return 3; }
static int uc_lt_rv(unsigned char a, unsigned char b) { return a < b; }
static int uc_lt_rb(unsigned char a, unsigned char b) { if (a < b) return 7; return 3; }
static int uc_lt_rt(unsigned char a, unsigned char b) { return (a < b) ? 10 : 20; }
static int uc_lt_m(unsigned char *p, unsigned char b) { return *p < b; }
static int uc_le_v0(unsigned char a) { return a <= (unsigned char)0L; }
static int uc_le_b0(unsigned char a) { if (a <= (unsigned char)0L) return 7; return 3; }
static int uc_le_v1(unsigned char a) { return a <= (unsigned char)127L; }
static int uc_le_b1(unsigned char a) { if (a <= (unsigned char)127L) return 7; return 3; }
static int uc_le_v2(unsigned char a) { return a <= (unsigned char)-129L; }
static int uc_le_b2(unsigned char a) { if (a <= (unsigned char)-129L) return 7; return 3; }
static int uc_le_v3(unsigned char a) { return a <= (unsigned char)40000L; }
static int uc_le_b3(unsigned char a) { if (a <= (unsigned char)40000L) return 7; return 3; }
static int uc_le_v4(unsigned char a) { return a <= (unsigned char)0x80000000L; }
static int uc_le_b4(unsigned char a) { if (a <= (unsigned char)0x80000000L) return 7; return 3; }
static int uc_le_v5(unsigned char a) { return a <= (unsigned char)0x123456789L; }
static int uc_le_b5(unsigned char a) { if (a <= (unsigned char)0x123456789L) return 7; return 3; }
static int uc_le_rv(unsigned char a, unsigned char b) { return a <= b; }
static int uc_le_rb(unsigned char a, unsigned char b) { if (a <= b) return 7; return 3; }
static int uc_le_rt(unsigned char a, unsigned char b) { return (a <= b) ? 10 : 20; }
static int uc_le_m(unsigned char *p, unsigned char b) { return *p <= b; }
static int uc_gt_v0(unsigned char a) { return a > (unsigned char)0L; }
static int uc_gt_b0(unsigned char a) { if (a > (unsigned char)0L) return 7; return 3; }
static int uc_gt_v1(unsigned char a) { return a > (unsigned char)127L; }
static int uc_gt_b1(unsigned char a) { if (a > (unsigned char)127L) return 7; return 3; }
static int uc_gt_v2(unsigned char a) { return a > (unsigned char)-129L; }
static int uc_gt_b2(unsigned char a) { if (a > (unsigned char)-129L) return 7; return 3; }
static int uc_gt_v3(unsigned char a) { return a > (unsigned char)40000L; }
static int uc_gt_b3(unsigned char a) { if (a > (unsigned char)40000L) return 7; return 3; }
static int uc_gt_v4(unsigned char a) { return a > (unsigned char)0x80000000L; }
static int uc_gt_b4(unsigned char a) { if (a > (unsigned char)0x80000000L) return 7; return 3; }
static int uc_gt_v5(unsigned char a) { return a > (unsigned char)0x123456789L; }
static int uc_gt_b5(unsigned char a) { if (a > (unsigned char)0x123456789L) return 7; return 3; }
static int uc_gt_rv(unsigned char a, unsigned char b) { return a > b; }
static int uc_gt_rb(unsigned char a, unsigned char b) { if (a > b) return 7; return 3; }
static int uc_gt_rt(unsigned char a, unsigned char b) { return (a > b) ? 10 : 20; }
static int uc_gt_m(unsigned char *p, unsigned char b) { return *p > b; }
static int uc_ge_v0(unsigned char a) { return a >= (unsigned char)0L; }
static int uc_ge_b0(unsigned char a) { if (a >= (unsigned char)0L) return 7; return 3; }
static int uc_ge_v1(unsigned char a) { return a >= (unsigned char)127L; }
static int uc_ge_b1(unsigned char a) { if (a >= (unsigned char)127L) return 7; return 3; }
static int uc_ge_v2(unsigned char a) { return a >= (unsigned char)-129L; }
static int uc_ge_b2(unsigned char a) { if (a >= (unsigned char)-129L) return 7; return 3; }
static int uc_ge_v3(unsigned char a) { return a >= (unsigned char)40000L; }
static int uc_ge_b3(unsigned char a) { if (a >= (unsigned char)40000L) return 7; return 3; }
static int uc_ge_v4(unsigned char a) { return a >= (unsigned char)0x80000000L; }
static int uc_ge_b4(unsigned char a) { if (a >= (unsigned char)0x80000000L) return 7; return 3; }
static int uc_ge_v5(unsigned char a) { return a >= (unsigned char)0x123456789L; }
static int uc_ge_b5(unsigned char a) { if (a >= (unsigned char)0x123456789L) return 7; return 3; }
static int uc_ge_rv(unsigned char a, unsigned char b) { return a >= b; }
static int uc_ge_rb(unsigned char a, unsigned char b) { if (a >= b) return 7; return 3; }
static int uc_ge_rt(unsigned char a, unsigned char b) { return (a >= b) ? 10 : 20; }
static int uc_ge_m(unsigned char *p, unsigned char b) { return *p >= b; }
static int uc_eq_v0(unsigned char a) { return a == (unsigned char)0L; }
static int uc_eq_b0(unsigned char a) { if (a == (unsigned char)0L) return 7; return 3; }
static int uc_eq_v1(unsigned char a) { return a == (unsigned char)127L; }
static int uc_eq_b1(unsigned char a) { if (a == (unsigned char)127L) return 7; return 3; }
static int uc_eq_v2(unsigned char a) { return a == (unsigned char)-129L; }
static int uc_eq_b2(unsigned char a) { if (a == (unsigned char)-129L) return 7; return 3; }
static int uc_eq_v3(unsigned char a) { return a == (unsigned char)40000L; }
static int uc_eq_b3(unsigned char a) { if (a == (unsigned char)40000L) return 7; return 3; }
static int uc_eq_v4(unsigned char a) { return a == (unsigned char)0x80000000L; }
static int uc_eq_b4(unsigned char a) { if (a == (unsigned char)0x80000000L) return 7; return 3; }
static int uc_eq_v5(unsigned char a) { return a == (unsigned char)0x123456789L; }
static int uc_eq_b5(unsigned char a) { if (a == (unsigned char)0x123456789L) return 7; return 3; }
static int uc_eq_rv(unsigned char a, unsigned char b) { return a == b; }
static int uc_eq_rb(unsigned char a, unsigned char b) { if (a == b) return 7; return 3; }
static int uc_eq_rt(unsigned char a, unsigned char b) { return (a == b) ? 10 : 20; }
static int uc_eq_m(unsigned char *p, unsigned char b) { return *p == b; }
static int uc_ne_v0(unsigned char a) { return a != (unsigned char)0L; }
static int uc_ne_b0(unsigned char a) { if (a != (unsigned char)0L) return 7; return 3; }
static int uc_ne_v1(unsigned char a) { return a != (unsigned char)127L; }
static int uc_ne_b1(unsigned char a) { if (a != (unsigned char)127L) return 7; return 3; }
static int uc_ne_v2(unsigned char a) { return a != (unsigned char)-129L; }
static int uc_ne_b2(unsigned char a) { if (a != (unsigned char)-129L) return 7; return 3; }
static int uc_ne_v3(unsigned char a) { return a != (unsigned char)40000L; }
static int uc_ne_b3(unsigned char a) { if (a != (unsigned char)40000L) return 7; return 3; }
static int uc_ne_v4(unsigned char a) { return a != (unsigned char)0x80000000L; }
static int uc_ne_b4(unsigned char a) { if (a != (unsigned char)0x80000000L) return 7; return 3; }
static int uc_ne_v5(unsigned char a) { return a != (unsigned char)0x123456789L; }
static int uc_ne_b5(unsigned char a) { if (a != (unsigned char)0x123456789L) return 7; return 3; }
static int uc_ne_rv(unsigned char a, unsigned char b) { return a != b; }
static int uc_ne_rb(unsigned char a, unsigned char b) { if (a != b) return 7; return 3; }
static int uc_ne_rt(unsigned char a, unsigned char b) { return (a != b) ? 10 : 20; }
static int uc_ne_m(unsigned char *p, unsigned char b) { return *p != b; }
int test(void)
{
	int i, j;
	long x = 0;

	for (i = 0; i < 14; i++) ADD(l_lt_v0((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_lt_b0((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_lt_v1((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_lt_b1((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_lt_v2((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_lt_b2((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_lt_v3((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_lt_b3((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_lt_v4((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_lt_b4((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_lt_v5((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_lt_b5((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_le_v0((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_le_b0((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_le_v1((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_le_b1((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_le_v2((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_le_b2((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_le_v3((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_le_b3((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_le_v4((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_le_b4((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_le_v5((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_le_b5((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_gt_v0((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_gt_b0((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_gt_v1((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_gt_b1((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_gt_v2((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_gt_b2((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_gt_v3((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_gt_b3((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_gt_v4((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_gt_b4((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_gt_v5((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_gt_b5((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ge_v0((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ge_b0((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ge_v1((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ge_b1((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ge_v2((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ge_b2((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ge_v3((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ge_b3((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ge_v4((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ge_b4((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ge_v5((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ge_b5((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_eq_v0((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_eq_b0((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_eq_v1((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_eq_b1((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_eq_v2((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_eq_b2((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_eq_v3((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_eq_b3((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_eq_v4((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_eq_b4((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_eq_v5((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_eq_b5((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ne_v0((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ne_b0((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ne_v1((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ne_b1((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ne_v2((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ne_b2((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ne_v3((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ne_b3((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ne_v4((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ne_b4((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ne_v5((long)V[i]));
	for (i = 0; i < 14; i++) ADD(l_ne_b5((long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_lt_v0((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_lt_b0((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_lt_v1((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_lt_b1((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_lt_v2((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_lt_b2((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_lt_v3((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_lt_b3((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_lt_v4((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_lt_b4((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_lt_v5((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_lt_b5((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_le_v0((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_le_b0((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_le_v1((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_le_b1((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_le_v2((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_le_b2((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_le_v3((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_le_b3((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_le_v4((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_le_b4((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_le_v5((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_le_b5((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_gt_v0((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_gt_b0((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_gt_v1((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_gt_b1((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_gt_v2((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_gt_b2((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_gt_v3((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_gt_b3((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_gt_v4((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_gt_b4((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_gt_v5((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_gt_b5((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ge_v0((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ge_b0((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ge_v1((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ge_b1((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ge_v2((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ge_b2((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ge_v3((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ge_b3((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ge_v4((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ge_b4((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ge_v5((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ge_b5((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_eq_v0((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_eq_b0((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_eq_v1((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_eq_b1((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_eq_v2((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_eq_b2((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_eq_v3((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_eq_b3((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_eq_v4((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_eq_b4((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_eq_v5((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_eq_b5((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ne_v0((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ne_b0((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ne_v1((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ne_b1((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ne_v2((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ne_b2((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ne_v3((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ne_b3((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ne_v4((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ne_b4((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ne_v5((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(ul_ne_b5((unsigned long)V[i]));
	for (i = 0; i < 14; i++) ADD(i_lt_v0((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_lt_b0((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_lt_v1((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_lt_b1((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_lt_v2((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_lt_b2((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_lt_v3((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_lt_b3((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_lt_v4((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_lt_b4((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_lt_v5((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_lt_b5((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_le_v0((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_le_b0((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_le_v1((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_le_b1((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_le_v2((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_le_b2((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_le_v3((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_le_b3((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_le_v4((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_le_b4((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_le_v5((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_le_b5((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_gt_v0((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_gt_b0((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_gt_v1((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_gt_b1((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_gt_v2((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_gt_b2((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_gt_v3((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_gt_b3((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_gt_v4((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_gt_b4((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_gt_v5((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_gt_b5((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ge_v0((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ge_b0((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ge_v1((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ge_b1((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ge_v2((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ge_b2((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ge_v3((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ge_b3((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ge_v4((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ge_b4((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ge_v5((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ge_b5((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_eq_v0((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_eq_b0((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_eq_v1((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_eq_b1((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_eq_v2((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_eq_b2((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_eq_v3((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_eq_b3((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_eq_v4((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_eq_b4((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_eq_v5((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_eq_b5((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ne_v0((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ne_b0((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ne_v1((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ne_b1((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ne_v2((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ne_b2((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ne_v3((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ne_b3((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ne_v4((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ne_b4((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ne_v5((int)V[i]));
	for (i = 0; i < 14; i++) ADD(i_ne_b5((int)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_lt_v0((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_lt_b0((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_lt_v1((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_lt_b1((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_lt_v2((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_lt_b2((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_lt_v3((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_lt_b3((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_lt_v4((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_lt_b4((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_lt_v5((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_lt_b5((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_le_v0((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_le_b0((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_le_v1((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_le_b1((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_le_v2((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_le_b2((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_le_v3((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_le_b3((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_le_v4((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_le_b4((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_le_v5((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_le_b5((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_gt_v0((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_gt_b0((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_gt_v1((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_gt_b1((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_gt_v2((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_gt_b2((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_gt_v3((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_gt_b3((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_gt_v4((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_gt_b4((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_gt_v5((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_gt_b5((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ge_v0((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ge_b0((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ge_v1((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ge_b1((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ge_v2((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ge_b2((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ge_v3((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ge_b3((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ge_v4((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ge_b4((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ge_v5((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ge_b5((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_eq_v0((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_eq_b0((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_eq_v1((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_eq_b1((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_eq_v2((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_eq_b2((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_eq_v3((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_eq_b3((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_eq_v4((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_eq_b4((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_eq_v5((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_eq_b5((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ne_v0((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ne_b0((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ne_v1((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ne_b1((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ne_v2((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ne_b2((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ne_v3((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ne_b3((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ne_v4((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ne_b4((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ne_v5((unsigned char)V[i]));
	for (i = 0; i < 14; i++) ADD(uc_ne_b5((unsigned char)V[i]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(l_lt_rv((long)V[i], (long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(l_lt_rb((long)V[i], (long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(l_lt_rt((long)V[i], (long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { long y = (long)V[i]; ADD(l_lt_m(&y, (long)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(l_le_rv((long)V[i], (long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(l_le_rb((long)V[i], (long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(l_le_rt((long)V[i], (long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { long y = (long)V[i]; ADD(l_le_m(&y, (long)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(l_gt_rv((long)V[i], (long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(l_gt_rb((long)V[i], (long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(l_gt_rt((long)V[i], (long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { long y = (long)V[i]; ADD(l_gt_m(&y, (long)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(l_ge_rv((long)V[i], (long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(l_ge_rb((long)V[i], (long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(l_ge_rt((long)V[i], (long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { long y = (long)V[i]; ADD(l_ge_m(&y, (long)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(l_eq_rv((long)V[i], (long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(l_eq_rb((long)V[i], (long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(l_eq_rt((long)V[i], (long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { long y = (long)V[i]; ADD(l_eq_m(&y, (long)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(l_ne_rv((long)V[i], (long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(l_ne_rb((long)V[i], (long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(l_ne_rt((long)V[i], (long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { long y = (long)V[i]; ADD(l_ne_m(&y, (long)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(ul_lt_rv((unsigned long)V[i], (unsigned long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(ul_lt_rb((unsigned long)V[i], (unsigned long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(ul_lt_rt((unsigned long)V[i], (unsigned long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { unsigned long y = (unsigned long)V[i]; ADD(ul_lt_m(&y, (unsigned long)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(ul_le_rv((unsigned long)V[i], (unsigned long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(ul_le_rb((unsigned long)V[i], (unsigned long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(ul_le_rt((unsigned long)V[i], (unsigned long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { unsigned long y = (unsigned long)V[i]; ADD(ul_le_m(&y, (unsigned long)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(ul_gt_rv((unsigned long)V[i], (unsigned long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(ul_gt_rb((unsigned long)V[i], (unsigned long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(ul_gt_rt((unsigned long)V[i], (unsigned long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { unsigned long y = (unsigned long)V[i]; ADD(ul_gt_m(&y, (unsigned long)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(ul_ge_rv((unsigned long)V[i], (unsigned long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(ul_ge_rb((unsigned long)V[i], (unsigned long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(ul_ge_rt((unsigned long)V[i], (unsigned long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { unsigned long y = (unsigned long)V[i]; ADD(ul_ge_m(&y, (unsigned long)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(ul_eq_rv((unsigned long)V[i], (unsigned long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(ul_eq_rb((unsigned long)V[i], (unsigned long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(ul_eq_rt((unsigned long)V[i], (unsigned long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { unsigned long y = (unsigned long)V[i]; ADD(ul_eq_m(&y, (unsigned long)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(ul_ne_rv((unsigned long)V[i], (unsigned long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(ul_ne_rb((unsigned long)V[i], (unsigned long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(ul_ne_rt((unsigned long)V[i], (unsigned long)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { unsigned long y = (unsigned long)V[i]; ADD(ul_ne_m(&y, (unsigned long)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(i_lt_rv((int)V[i], (int)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(i_lt_rb((int)V[i], (int)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(i_lt_rt((int)V[i], (int)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { int y = (int)V[i]; ADD(i_lt_m(&y, (int)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(i_le_rv((int)V[i], (int)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(i_le_rb((int)V[i], (int)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(i_le_rt((int)V[i], (int)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { int y = (int)V[i]; ADD(i_le_m(&y, (int)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(i_gt_rv((int)V[i], (int)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(i_gt_rb((int)V[i], (int)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(i_gt_rt((int)V[i], (int)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { int y = (int)V[i]; ADD(i_gt_m(&y, (int)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(i_ge_rv((int)V[i], (int)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(i_ge_rb((int)V[i], (int)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(i_ge_rt((int)V[i], (int)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { int y = (int)V[i]; ADD(i_ge_m(&y, (int)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(i_eq_rv((int)V[i], (int)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(i_eq_rb((int)V[i], (int)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(i_eq_rt((int)V[i], (int)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { int y = (int)V[i]; ADD(i_eq_m(&y, (int)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(i_ne_rv((int)V[i], (int)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(i_ne_rb((int)V[i], (int)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(i_ne_rt((int)V[i], (int)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { int y = (int)V[i]; ADD(i_ne_m(&y, (int)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(uc_lt_rv((unsigned char)V[i], (unsigned char)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(uc_lt_rb((unsigned char)V[i], (unsigned char)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(uc_lt_rt((unsigned char)V[i], (unsigned char)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { unsigned char y = (unsigned char)V[i]; ADD(uc_lt_m(&y, (unsigned char)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(uc_le_rv((unsigned char)V[i], (unsigned char)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(uc_le_rb((unsigned char)V[i], (unsigned char)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(uc_le_rt((unsigned char)V[i], (unsigned char)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { unsigned char y = (unsigned char)V[i]; ADD(uc_le_m(&y, (unsigned char)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(uc_gt_rv((unsigned char)V[i], (unsigned char)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(uc_gt_rb((unsigned char)V[i], (unsigned char)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(uc_gt_rt((unsigned char)V[i], (unsigned char)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { unsigned char y = (unsigned char)V[i]; ADD(uc_gt_m(&y, (unsigned char)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(uc_ge_rv((unsigned char)V[i], (unsigned char)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(uc_ge_rb((unsigned char)V[i], (unsigned char)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(uc_ge_rt((unsigned char)V[i], (unsigned char)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { unsigned char y = (unsigned char)V[i]; ADD(uc_ge_m(&y, (unsigned char)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(uc_eq_rv((unsigned char)V[i], (unsigned char)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(uc_eq_rb((unsigned char)V[i], (unsigned char)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(uc_eq_rt((unsigned char)V[i], (unsigned char)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { unsigned char y = (unsigned char)V[i]; ADD(uc_eq_m(&y, (unsigned char)V[j])); }
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(uc_ne_rv((unsigned char)V[i], (unsigned char)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(uc_ne_rb((unsigned char)V[i], (unsigned char)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) ADD(uc_ne_rt((unsigned char)V[i], (unsigned char)V[j]));
	for (i = 0; i < 14; i++) for (j = 0; j < 14; j++) { unsigned char y = (unsigned char)V[i]; ADD(uc_ne_m(&y, (unsigned char)V[j])); }
	return h != 2591729886469671168UL;
}
