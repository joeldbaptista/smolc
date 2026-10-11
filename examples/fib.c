/*
 * fib.c -- Fibonacci, computed twice.
 *
 * Shows: recursion and for loops.  An int is 64 bits on smol, so
 * fib(92) is the last value that fits.
 */

#include stdio.h

int
fib(int n)
{
	if (n < 2)
		return n;
	return fib(n - 1) + fib(n - 2);
}

int
main(void)
{
	int i;
	int a, b, t;

	for (i = 0; i < 10; ++i)
		printf("fib(%d) = %d\n", i, fib(i));

	a = 0;
	b = 1;
	for (i = 0; i < 10; ++i) {
		t = a + b;
		a = b;
		b = t;
	}
	printf("iterative fib(10) = %d\n", a);
	return 0;
}
