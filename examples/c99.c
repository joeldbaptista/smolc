/*
 * c99.c -- C99 function definitions and prototypes.
 *
 * The original Small-C accepted only K&R definitions, where the
 * parameter types follow the parameter list.  This compiler also
 * accepts the C99 form, where each parameter carries its own type:
 *
 *     int add(int a, int b)
 *     {
 *             return a + b;
 *     }
 *
 * Both forms may appear in the same file, so existing Small-C source
 * keeps working unchanged.
 *
 * What is supported: typed parameters, pointer and array parameters,
 * "void" as a return type, "(void)" for an empty parameter list, and
 * prototypes with or without parameter names.
 *
 * What is not: "unsigned", "long", "float", and anything else outside
 * the Small-C subset.  Parameter types are recorded but calls
 * are not checked against them.
 */

#include stdio.h

/* prototypes, named and unnamed */
int add(int a, int b);
int slen(char *);
void banner(void);
int total(int v[], int n);

int table[4] = {10, 20, 30, 40};

int
add(int a, int b)
{
	return a + b;
}

/* a pointer parameter */
int
slen(char *s)
{
	int n;

	n = 0;
	while (*s++)
		++n;
	return n;
}

/* void return, and an empty parameter list */
void
banner(void)
{
	printf("C99 definitions\n");
}

/* an array parameter, which decays to a pointer */
int
total(int v[], int n)
{
	int i, sum;

	sum = 0;
	for (i = 0; i < n; ++i)
		sum = sum + v[i];
	return sum;
}

/* a K&R definition, still accepted, in the same file */
twice(x) int x;
{
	return x + x;
}

int
main(void)
{
	banner();
	printf("add   = %d\n", add(3, 4));
	printf("slen  = %d\n", slen("hello"));
	printf("total = %d\n", total(table, 4));
	printf("twice = %d\n", twice(21));
	return 0;
}
