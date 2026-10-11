/*
 * sqrt2.c -- the square root of 2.0 by the Newton-Raphson method.
 *
 * smolc has no floating point, so a real number x is held as the int
 * x * SCALE: a fixed-point number with 9 decimal places.  An int has
 * 64 bits, so the largest product below, 2.0 * SCALE * SCALE, which is
 * 2 * 10^18, still fits.
 *
 * Newton-Raphson finds a root of f(x) = x*x - a by repeating
 *
 *     x' = x - f(x) / f'(x) = (x + a / x) / 2
 *
 * Each step about doubles the number of correct digits.  From the
 * second step on, the guesses fall toward sqrt(a), so the method stops
 * when a step no longer makes the guess smaller.
 */

#include stdio.h

#define SCALE 1000000000 /* 10^9: 9 decimal places */
#define MAXSTEP 20

/* Print the non-negative fixed-point number x as a decimal. */
void
show(int x)
{
	printf("%d.%09d", x / SCALE, x % SCALE);
}

/* Return a * b for fixed-point numbers a and b. */
int
fxmul(int a, int b)
{
	return a * b / SCALE;
}

/* Return a / b for fixed-point numbers a and b. */
int
fxdiv(int a, int b)
{
	return a * SCALE / b;
}

/* Return the square root of the fixed-point number a, printing each step. */
int
newton(int a)
{
	int n, x, y;

	x = SCALE; /* the first guess: 1.0 */
	printf("step  0: ");
	show(x);
	printf("\n");
	for (n = 1; n <= MAXSTEP; ++n) {
		y = (x + fxdiv(a, x)) / 2;
		if (n > 1 && y >= x)
			break; /* the guess no longer falls: x is the root */
		x = y;
		printf("step %2d: ", n);
		show(x);
		printf("\n");
	}
	return x;
}

int
main(void)
{
	int a, r;

	a = 2 * SCALE; /* 2.0 */
	r = newton(a);
	printf("sqrt(2.0) = ");
	show(r);
	printf("\ncheck: ");
	show(r);
	printf(" * ");
	show(r);
	printf(" = ");
	show(fxmul(r, r));
	printf("\n");
	return 0;
}
