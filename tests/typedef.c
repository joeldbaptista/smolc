/*
 * typedef.c -- typedef names, and file-scope static.
 *
 * abs() below is static, so it does not clash with the library's
 * abs(), which the program is linked with.
 */

#include stdio.h

typedef int Num;
typedef char Name[8];
typedef struct node Node;
typedef struct node *Link;
typedef int (*Binop)(int, int);
typedef struct {
	int x, y;
} Point;
typedef Point *PointPtr;
typedef Binop Ops[3];
typedef union {
	int i;
	char c[8];
} Bits;

struct node {
	Num val;
	Link next;
};

static int counter;
static Name label = "static";
static int helper(int v);

/* a static function with the name of a library function */
static int
abs(int x)
{
	return x < 0 ? -x * 100 : x * 100;
}

static int
bump(void)
{
	return ++counter;
}

/* defined without "static", it keeps the linkage of its prototype */
int
helper(int v)
{
	return v + 1;
}

Num
add(Num a, Num b)
{
	return a + b;
}

Num
sub(Num a, Num b)
{
	return a - b;
}

Ops table = {add, sub, add};

Num
sum(Link l)
{
	Num s = 0;

	for (; l; l = l->next)
		s += l->val;
	return s;
}

Point
mid(Point a, Point b)
{
	Point m;

	m.x = (a.x + b.x) / 2;
	m.y = (a.y + b.y) / 2;
	return m;
}

Num
apply(Binop f, Num a, Num b)
{
	return f(a, b);
}

/* typedef names in K&R parameter declarations */
kr(p, f) PointPtr p; Binop f;
{
	return f(p->x, p->y);
}

int
main(void)
{
	Node a, b;
	Link l;
	Name n;
	Point p = {2, 4}, q = {6, 8}, r;
	PointPtr pp;
	Binop f = sub;
	Bits bits;

	a.val = 1;
	a.next = &b;
	b.val = 2;
	b.next = NULL;
	l = &a;
	strcpy(n, "abc");
	printf("sum = %d, n = %s\n", sum(l), n);
	printf("sizeof: %d %d %d %d %d %d %d %d\n", sizeof n, sizeof(Name),
	       sizeof(Node), sizeof(Point), sizeof(Link), sizeof(Ops),
	       sizeof(Binop), sizeof(Bits));
	r = mid(p, q);
	pp = &r;
	printf("mid = %d %d", r.x, r.y);
	pp->x = 10;
	printf(", now %d\n", r.x);
	printf("calls: %d %d %d %d\n", f(9, 4), apply(add, 2, 3),
	       table[1](10, 3), kr(&p, add));
	bits.i = 258;
	printf("union: %d %d\n", bits.c[0], bits.c[1]);
	printf("static: %d %d %d %d %s %d\n", abs(-3), abs(4), bump(), bump(),
	       label, helper(41));
	return 0;
}
