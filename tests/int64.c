/*
 * int64.c -- 64-bit int arithmetic, conversions and operators.
 */

#include stdio.h

int big;

int
fib(int n)
{
	int a, b, t;

	a = 0;
	b = 1;
	while (n--) {
		t = a + b;
		a = b;
		b = t;
	}
	return a;
}

int
main(void)
{
	int m, x;

	printf("fib(90) = %d\n", fib(90));
	m = 1;
	m = m << 63;
	printf("min = %d\n", m);
	printf("max = %d\n", ~m);
	printf("1<<40 = %d\n", 1 << 20 << 20);
	printf("-1 x = %x\n", -1);
	printf("-1 u = %u\n", -1);
	printf("-1 o = %o\n", -1);
	printf("5 b = %b\n", 5);
	printf("255 x = %x, 8 o = %o\n", 255, 8);
	printf("-7/2 = %d, -7%%2 = %d\n", -7 / 2, -7 % 2);
	x = -7;
	printf("x/2 = %d, x%%2 = %d, x*x = %d\n", x / 2, x % 2, x * x);
	printf("-16>>2 = %d, 3<<4 = %d\n", x - 9 >> 2, 3 << 4);
	printf("12&10 = %d, 12|10 = %d, 12^10 = %d, ~0 = %d\n",
	       12 & 10, 12 | 10, 12 ^ 10, ~0);
	printf("cmp: %d %d %d %d %d %d\n", x < 0, x <= -7, x > -7, x >= 0,
	       x == -7, x != -7);
	printf("not: %d %d, neg: %d\n", !x, !0, -x);
	big = 1000000;
	big = big * big * big;
	printf("10^18 = %d\n", big);
	printf("literal = %d\n", 123456789012345 * 1000 + 678);
	big += big / 1000;
	big -= 1;
	printf("big = %d\n", big);
	x = 6;
	x *= 7;
	x <<= 1;
	x >>= 2;
	x |= 256;
	x &= 257;
	x ^= 3;
	x %= 100;
	printf("compound = %d\n", x);
	printf("%5d|%-5d|%05d|%.2s\n", 42, 42, 42, "abc");
	return 0;
}
