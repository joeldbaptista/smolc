/*
 * pointers.c -- pointers, and how Small-C scales them.
 *
 * A char pointer steps one byte, an int pointer steps eight, because
 * an int is 64 bits.  Small-C emits the scaling itself, so "++ip"
 * becomes "add r0, 8" while "++cp" becomes "inc r0".
 *
 * Small-C has no pointer arrays and no structs, so a pointer here is
 * always to a char or to an int.
 */

#include stdio.h

int numbers[5] = {10, 20, 30, 40, 50};
char letters[] = "abcde";

int
main(void)
{
	int *ip;
	char *cp;
	int i;

	ip = numbers;
	cp = letters;
	for (i = 0; i < 5; ++i) {
		printf("numbers[%d] = %2d   letters[%d] = %c\n",
		       i, *ip, i, *cp);
		++ip;
		++cp;
	}

	/* a pointer difference counts objects, not bytes */
	printf("ip - numbers = %d\n", ip - numbers);
	printf("cp - letters = %d\n", cp - letters);
	return 0;
}
