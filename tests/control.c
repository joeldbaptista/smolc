/*
 * control.c -- statements: if, while, do, for, break, continue,
 * switch, goto and return, plus short-circuit evaluation.
 */

#include stdio.h

int calls;

int
touch(int v)
{
	++calls;
	return v;
}

int
kind(int c)
{
	switch (c) {
	case 1:
		return 10;
	case 2: /* FALLTHROUGH */
	case 3:
		c = c * 100;
	case 4:
		return c + 1;
	case -5:
		return -50;
	}
	return 0;
}

int
main(void)
{
	int i, j, s;

	s = 0;
	for (i = 0; i < 10; ++i) {
		if (i == 3)
			continue;
		if (i == 8)
			break;
		s += i;
	}
	printf("for: %d\n", s);

	i = 0;
	do {
		i += 3;
	} while (i < 10);
	printf("do: %d\n", i);

	i = 5;
	s = 1;
	while (i)
		s *= i--;
	printf("while: %d\n", s);

	for (i = 1; i <= 5; ++i)
		printf("%d ", kind(i));
	printf("%d %d\n", kind(-5), kind(99));

	s = 0;
	for (i = 0; i < 3; ++i) {
		for (j = 0; j < 3; ++j) {
			switch (j) {
			case 1:
				continue;
			default:
				s += 10 * i + j;
			}
		}
	}
	printf("nested: %d\n", s);

	i = 0;
again:
	if (++i < 4)
		goto again;
	printf("goto: %d\n", i);

	calls = 0;
	if (touch(0) && touch(1))
		printf("wrong\n");
	if (touch(1) || touch(1))
		printf("or: calls = %d\n", calls);
	s = (touch(2) && touch(3)) + (touch(0) || touch(0));
	printf("values: %d, calls = %d\n", s, calls);

	if (s > 5)
		printf("wrong\n");
	else if (s == 1)
		printf("else-if\n");
	else
		printf("wrong\n");
	return 3;
}
