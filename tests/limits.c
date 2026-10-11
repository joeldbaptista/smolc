/*
 * limits.c -- the features that lift earlier limits: local
 * initializers, brace elision, sizeof of local arrays, K&R struct
 * parameters by value, ?:, typed call results, and long names.
 */

#include stdio.h

struct pt {
	int x, y;
};

struct line {
	struct pt a, b;
	char label[4];
};

union num {
	int i;
	char c[8];
};

/* brace elision in a global: the inner braces are left out */
struct line lines[] = {1, 2, 3, 4, "ab", 5, 6, {7, 8}, "cd"};
int a_rather_long_name_one = 1;
int a_rather_long_name_two = 2;

struct pt mk(int x, int y);
int *second(int *v);
char *pick(int i);

struct pt
mk(int x, int y)
{
	struct pt p = {x, y};

	return p;
}

int *
second(int *v)
{
	return v + 1;
}

char *
pick(int i)
{
	return i ? "one" : "zero";
}

/* a K&R definition that takes a struct by value */
span(l, k) struct line l; int k;
{
	l.b.x = 0; /* a copy: the caller's line is unchanged */
	return k * (l.b.y - l.a.y);
}

int
main(void)
{
	int i = 3, j = i * 2;
	char c = 'q', *s = "str";
	int v[4] = {10, 20};
	char word[] = "hello";
	char pad[8] = "hi";
	struct pt p = {1}, q = mk(5, 6), r = q;
	struct line ln = {{1, 2}, 3, 4, "xy"};
	union num u = {258};
	int big[100];
	int w[] = {i, i + 1, mk(9, 9).x, 4};
	struct pt pts[] = {{1, 2}, 3, 4};
	char after = 'z';

	printf("scalars: %d %d %c %s\n", i, j, c, s);
	printf("arrays: %d %d %d %d, %s %d, %s %d %d\n", v[0], v[1], v[2],
	       v[3], word, sizeof word, pad, pad[5], sizeof pad);
	printf("structs: %d %d, %d %d, %d %d\n", p.x, p.y, q.x, q.y, r.x,
	       r.y);
	printf("line: %d %d %d %d %s\n", ln.a.x, ln.a.y, ln.b.x, ln.b.y,
	       ln.label);
	printf("union: %d %d\n", u.c[0], u.c[1]);
	printf("global: %d %d %s, %d %d %s\n", lines[0].b.x, lines[0].b.y,
	       lines[0].label, lines[1].a.y, lines[1].b.x, lines[1].label);
	printf("sizeof: %d %d %d\n", sizeof big, sizeof v / sizeof v[0],
	       sizeof lines);
	printf("span: %d, b.x still %d\n", span(ln, 10), ln.b.x);

	for (i = 0; i < 3; ++i)
		printf("%s ", i == 0 ? "a" : i == 1 ? "b" : "c");
	j = i > 2 ? i * 100 : -1;
	r = j ? mk(7, 8) : q;
	printf("?: %d %d %d %d\n", j, r.x, r.y, (i < 0 ? 0 : 1) + 1);
	printf("results: %d %d %c %c %d\n", second(v)[0], *second(v),
	       pick(1)[1], *pick(0), (v + 1)[0]);
	printf("unsized: %d %d %d %d %d, %d %d %d %c\n", w[0], w[1], w[2],
	       w[3], sizeof w, pts[1].x, pts[1].y, sizeof pts, after);
	printf("long names: %d %d\n", a_rather_long_name_one,
	       a_rather_long_name_two);
	return 0;
}
