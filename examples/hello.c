/*
 * hello.c -- the classic, in Small-C.
 *
 * Shows: an initialised global string, a while loop, putchar() and
 * printf().  Note that argv is an int array, because Small-C has no
 * pointer arrays.
 */

#include stdio.h

char greeting[] = "Hello, smol!";

int
main(int argc, int argv[])
{
	int i;

	i = 0;
	while (greeting[i])
		putchar(greeting[i++]);
	putchar('\n');
	printf("argc = %d\n", argc);
	return 0;
}
