/*
 * sort.c -- bubble sort over an initialised global array.
 *
 * Shows: global array initialisers, nested loops, and subscripting.
 * Small-C scales int subscripts for you, which you can see in the
 * emitted "shl r0, 3" before each index is added to the base.
 */

#include stdio.h

#define COUNT 8

int data[COUNT] = {42, 7, 19, 3, 25, 11, 1, 30};

void
show(char *label)
{
	int i;

	printf("%s", label);
	for (i = 0; i < COUNT; ++i)
		printf(" %d", data[i]);
	putchar('\n');
}

int
main(void)
{
	int i, j;
	int t;

	show("before:");
	for (i = 0; i < COUNT - 1; ++i) {
		for (j = 0; j < COUNT - 1 - i; ++j) {
			if (data[j] > data[j + 1]) {
				t = data[j];
				data[j] = data[j + 1];
				data[j + 1] = t;
			}
		}
	}
	show("after: ");
	return 0;
}
