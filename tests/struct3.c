/*
 * struct3.c -- whole structs: assignment, arguments and return values
 * by value, unions, and functions that return struct pointers.
 */

#include stdio.h

struct pt {
	int x, y;
};

struct rgb {
	char r, g, b;    /* 3 bytes: a by-value argument pushes 8 */
};

struct box {
	struct pt lo, hi;
	struct rgb ink;
};

union word {
	int i;
	char c[8];
};

struct tagged {
	char kind;
	union word w;    /* offset 8 */
};

union word init = {258};
struct pt origin;
struct box gbox;

struct pt mkpt(int x, int y);
struct pt *corner(struct box *b, int hi);

struct pt
mkpt(int x, int y)
{
	struct pt p;

	p.x = x;
	p.y = y;
	return p;
}

struct pt
add(struct pt a, struct pt b)
{
	a.x += b.x;  /* a is a copy, so the caller's struct is unchanged */
	a.y = a.y + b.y;
	return a;
}

/* recursion through a by-value return */
struct pt
power(struct pt p, int n)
{
	if (n == 0)
		return mkpt(1, 1);
	return mkpt(p.x * power(p, n - 1).x, p.y * power(p, n - 1).y);
}

int
dot(struct pt a, int k, struct pt b)
{
	return k * (a.x * b.x + a.y * b.y);
}

int
lum(struct rgb c, int scale)
{
	return (c.r + c.g + c.b) * scale;
}

struct pt *
corner(struct box *b, int hi)
{
	if (hi)
		return &b->hi;
	return &b->lo;
}

int f() { return 5; }   /* empty parentheses with a body */

int
main(void)
{
	struct pt a, b, c, *pp;
	struct box bx, *bp;
	struct rgb col;
	union word w;
	struct tagged t;
	int fp;

	a = mkpt(1, 2);
	b = mkpt(10, 20);
	c = add(a, b);
	printf("add: %d %d, a still %d %d\n", c.x, c.y, a.x, a.y);
	printf("nested: %d\n", add(add(a, b), mkpt(100, 200)).y);
	printf("power: %d %d\n", power(mkpt(2, 3), 4).x,
	       power(mkpt(2, 3), 4).y);
	printf("dot: %d\n", dot(a, 3, b));

	bx.lo = a;
	bx.hi = b;
	bx.ink.r = 10;
	bx.ink.g = 20;
	bx.ink.b = 30;
	gbox = bx;
	bp = &gbox;
	bp->lo.x = 99;
	printf("copy: %d %d %d, bx.lo.x still %d\n", gbox.lo.x, gbox.hi.y,
	       gbox.ink.b, bx.lo.x);
	col = gbox.ink;
	printf("lum: %d\n", lum(col, 2));
	printf("corner: %d %d\n", corner(&bx, 1)->y, corner(&bx, 0)->x);
	*corner(&bx, 0) = mkpt(-5, -6);
	pp = corner(&bx, 0);
	printf("through pointer: %d %d\n", pp->x, bx.lo.y);

	a = b = c = mkpt(7, 8);
	origin = a;
	a = a;
	printf("chain: %d %d %d %d\n", a.x, b.y, c.x, origin.y);

	/* an indirect call returns an int: Small-C has no function types */
	fp = dot;
	printf("indirect: %d\n", fp(mkpt(1, 1), 2, mkpt(2, 3)));

	w.i = 258;
	printf("union: %d %d %d %d\n", sizeof w, w.c[0], w.c[1], init.c[1]);
	w.c[0] = 3;
	printf("union write: %d\n", w.i);
	t.kind = 'u';
	t.w = w;
	printf("tagged: %c %d %d\n", t.kind, t.w.i, sizeof(struct tagged));
	printf("f = %d, sizes %d %d\n", f(), sizeof(struct rgb),
	       sizeof(struct box));
	return 0;
}
