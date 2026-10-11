/*
 * struct.c -- structs reached by name and through pointers, sizeof,
 * and pointer arithmetic over structs.
 */

#include stdio.h

struct pad {
	char c;   /* offset 0 */
	int i;    /* offset 8: an int is aligned to 8 */
	char d;   /* offset 16 */
};            /* size 24: rounded up to the int alignment */

struct name {
	char text[5];
	char last;
};

struct pt {
	int x, y;
};

struct node {
	int val;
	struct node *next;
};

struct pt origin;
struct pt *gp;

int
area(struct pt *a, struct pt *b)
{
	return (b->x - a->x) * (b->y - a->y);
}

/* a K&R definition with a struct pointer parameter */
shift(p, dx) struct pt *p; int dx;
{
	p->x += dx;
	p->y = p->y + dx;
}

int
main(void)
{
	struct pt a, *p, *q;
	struct name n;
	struct node first, second, *np;
	struct pad pd;
	char *base, *cp;
	int i;

	printf("sizeof: %d %d %d %d %d %d %d\n", sizeof(struct pad),
	       sizeof(struct name), sizeof(struct pt), sizeof(struct node),
	       sizeof(struct node *), sizeof(char), sizeof(int));
	printf("sizeof expr: %d %d %d %d\n", sizeof a, sizeof(a.x),
	       sizeof(n.text[0]), sizeof origin);

	pd.c = 'A';
	pd.i = 1000;
	pd.d = -3;
	/* char pointers measure the member offsets in bytes */
	base = &pd;
	cp = &pd.i;
	printf("pad: %c %d %d at %d", pd.c, pd.i, pd.d, &pd.c - base);
	printf(" %d", cp - base);
	cp = &pd.d;
	printf(" %d\n", cp - base);

	a.x = 3;
	a.y = 4;
	origin.x = 1;
	origin.y = 1;
	printf("area = %d\n", area(&origin, &a));
	shift(&a, 10);
	printf("shifted = %d %d\n", a.x, a.y);
	gp = &origin;
	gp->y = 7;
	++gp->x;
	gp->x++;
	printf("origin = %d %d\n", origin.x, origin.y);

	strcpy(n.text, "abcd");
	n.last = n.text[1];
	n.text[0] = 'z';
	printf("name = %s %c %d\n", n.text, n.last, strlen(n.text));

	first.val = 1;
	first.next = &second;
	second.val = 2;
	second.next = NULL;
	for (np = &first; np; np = np->next)
		printf("node %d\n", np->val);
	printf("chain = %d %d\n", first.next->val, first.next->next == NULL);

	p = calloc(4, sizeof(struct pt));
	for (i = 0; i < 4; ++i) {
		p[i].x = i;
		(p + i)->y = i * i;
	}
	q = p;
	++q;
	q++;
	q = q + 1;
	printf("array: %d %d %d %d, q - p = %d, q->y = %d, (*q).x = %d\n",
	       p[0].y, p[1].y, p[2].y, p[3].y, q - p, q->y, (*q).x);
	q -= 2;
	printf("back: %d, q > p = %d\n", q->x, q > p);
	return 0;
}
