/*
 * structloc.c -- a struct defined inside a function, and definitions
 * combined with variable declarations.
 */

#include stdio.h
struct g { char c; int n; } gv, *gp;
main()
{
	struct loc { int a; char b[3]; } x, *p;
	p = &x;
	p->a = 41;
	++x.a;
	x.b[2] = 'q';
	gp = &gv;
	gp->n = sizeof(struct loc) + sizeof gv;
	printf("%d %c %d\n", x.a, p->b[2], gv.n);
	return 0;
}
