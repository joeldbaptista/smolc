/*
 * funcptr.c -- function pointer types and static locals.
 */

#include stdio.h

struct pt {
	int x, y;
};

struct op {
	char *name;
	int (*fn)(int, int);
};

int add(int a, int b);
int sub(int a, int b);
int mul(int a, int b);
struct pt mkpt(int x, int y);
char *word(int n);

/* a global function pointer and a dispatch table, both initialized */
int (*gfp)(int, int) = add;
int (*table[])(int, int) = {add, sub, &mul};
struct op ops[] = {{"add", add}, {"sub", sub}, {"mul", mul}};

int
add(int a, int b)
{
	return a + b;
}

int
sub(int a, int b)
{
	return a - b;
}

int
mul(int a, int b)
{
	return a * b;
}

struct pt
mkpt(int x, int y)
{
	struct pt p = {x, y};

	return p;
}

char *
word(int n)
{
	return n ? "yes" : "no";
}

/* C99 and K&R parameters of function pointer type */
int
apply(int (*f)(int, int), int a, int b)
{
	return f(a, b);
}

kapply(f, x) int (*f)(); int x;
{
	return (*f)(x, x);
}

/* a static local keeps its value between calls */
int
counter(void)
{
	static int n;
	static int calls = 100;

	++calls;
	return ++n * 1000 + calls;
}

/* a static local is shared by every level of a recursion */
int
depth(int k)
{
	static int deepest = 0;

	if (k > deepest)
		deepest = k;
	if (k < 5)
		depth(k + 1);
	return deepest;
}

/* static locals with the same name in two functions are distinct */
int
first(void)
{
	static int n = 1;

	return n++;
}

int
second(void)
{
	static int n = 50;
	static char msg[] = "hi";
	static char *ptr = "there";
	static struct pt corner = {3, 4};
	static int (*op)(int, int) = sub;

	return n++ + msg[1] + ptr[0] + corner.y + op(10, 4);
}

int
main(void)
{
	int (*fp)(int, int) = mul;
	int (*local[])(int, int) = {sub, add};
	struct pt (*make)(int, int) = mkpt;
	char *(*say)(int) = word;
	struct op *p;
	int i;

	printf("global: %d, table: %d %d %d\n", gfp(2, 3), table[0](7, 2),
	       table[1](7, 2), table[2](7, 2));
	for (i = 0; i < 3; ++i)
		printf("%s=%d ", ops[i].name, ops[i].fn(6, 3));
	p = &ops[1];
	printf("p->fn=%d\n", p->fn(9, 4));
	printf("local: %d %d %d, (*fp): %d\n", fp(4, 5), local[0](4, 5),
	       local[1](4, 5), (*fp)(2, 2));
	fp = &sub;
	printf("reassigned: %d, same: %d %d\n", fp(9, 1), fp == sub,
	       fp == add);
	printf("struct: %d %d, char *: %s %c\n", make(1, 2).y,
	       (*make)(5, 6).x, say(1), say(0)[1]);
	printf("params: %d %d %d\n", apply(add, 20, 22), apply(&mul, 6, 7),
	       kapply(mul, 9));
	printf("(*add): %d, sizeof: %d %d %d\n", (*add)(1, 2),
	       sizeof(int (*)(int)), sizeof table, sizeof ops);
	printf("counter: %d %d %d\n", counter(), counter(), counter());
	printf("depth: %d %d\n", depth(0), depth(3));
	printf("first: %d %d, second: %d %d, first: %d\n", first(), first(),
	       second(), second(), first());
	return 0;
}
