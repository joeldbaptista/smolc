/*
 * struct2.c -- arrays of structs, structs inside structs, nested
 * definitions and global initializers.
 */

#include stdio.h

struct pt {
	int x, y;
};

struct seg {
	char tag;            /* offset 0 */
	struct pt ends[2];   /* offset 8, 32 bytes */
	struct pt *mid;      /* offset 40 */
};                       /* size 48 */

/* a struct defined inside another one's member list */
struct item {
	char name[6];        /* offset 0 */
	struct money {
		int units;
		char cur[4];
	} price;             /* offset 8, 16 bytes */
	char *note;          /* offset 24 */
	int qty[3];          /* offset 32 */
};                       /* size 56 */

struct item shop[] = {
	{"apple", {3, "EUR"}, "red", {1, 2, 3}},
	{"pear", {5, "USD"}, 0, {4}},
	{"fig"},
};

struct seg line = {'L', {{1, 2}, {7, 9}}};
struct pt grid[4];
struct money cash = {-12, {'G', 'B', 'P'}};

/* Small-C has no ?: operator */
char *
orelse(char *s, char *t)
{
	if (s)
		return s;
	return t;
}

int
total(struct item *it, int n)
{
	int i, s;

	s = 0;
	for (i = 0; i < n; ++i)
		s += it[i].price.units * (it[i].qty[0] + it[i].qty[1] +
		                          it[i].qty[2]);
	return s;
}

int
main(void)
{
	struct seg local[2];
	struct item *ip;
	struct pt *pp;
	int i;

	printf("sizes: %d %d %d %d %d %d\n", sizeof(struct seg),
	       sizeof(struct item), sizeof(struct money), sizeof shop,
	       sizeof line.ends, sizeof shop[0].qty);
	for (i = 0; i < 3; ++i) {
		ip = &shop[i];
		printf("%s %d %s %s %d %d %d\n", ip->name, ip->price.units,
		       ip->price.cur, orelse(ip->note, "-"),
		       ip->qty[0], ip->qty[1], ip->qty[2]);
	}
	printf("total = %d\n", total(shop, 3));
	printf("cash = %d %s\n", cash.units, cash.cur);

	line.mid = &line.ends[1];
	printf("line %c: %d %d - %d %d, mid %d\n", line.tag, line.ends[0].x,
	       line.ends[0].y, line.ends[1].x, line.ends[1].y,
	       line.mid->y);

	for (i = 0; i < 4; ++i) {
		grid[i].x = i;
		(grid + i)->y = 10 * i;
	}
	pp = grid;
	pp += 3;
	printf("grid: %d %d %d, pp - grid = %d, grid->y = %d\n",
	       grid[2].y, pp->x, (*pp).y, pp - grid, grid->y);

	local[1].ends[0].x = 5;
	local[1].ends[1] .y = 6;
	local[0].tag = 'a';
	local[1].tag = local[0].tag + 1;
	pp = local[1].ends;
	printf("local: %c %d %d %d\n", local[1].tag, pp->x, pp[1].y,
	       &local[1] - &local[0]);
	return 0;
}
