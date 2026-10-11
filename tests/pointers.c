/*
 * pointers.c -- addresses, scaling, function pointers and chars.
 */

#include stdio.h

char *greet = "hey";
int table[4] = {5, 6, 7, 8};
char bytes[4];

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

/* call the function whose address is f */
int
apply(int f, int a, int b)
{
	return f(a, b);
}

void
setint(int *p, int v)
{
	*p = v;
}

int
main(void)
{
	int local[3];
	int *ip, *jp;
	char *cp;
	char c;
	int n, f;

	setint(&n, 77);
	printf("n = %d\n", n);
	setint(&local[2], 9);
	setint(local, 4);
	local[1] = local[0] + local[2];
	printf("local = %d %d %d\n", local[0], local[1], local[2]);

	ip = table;
	jp = &table[3];
	printf("jp - ip = %d, *jp = %d, ip[2] = %d\n", jp - ip, *jp, ip[2]);
	printf("bytes = %d\n", (jp - ip) * 8 == (cp = jp) - (cp = ip));
	printf("ip < jp = %d, ip > jp = %d\n", ip < jp, ip > jp);
	++ip;
	*ip += 100;
	printf("table[1] = %d, ip++ = %d, *ip = %d\n", table[1], *ip++, *ip);

	c = 200;
	printf("c = %d\n", c);
	bytes[0] = 65;
	bytes[1] = bytes[0] + 1;
	cp = bytes;
	*(cp + 2) = 'C';
	printf("bytes = %s, greet = %s, greet[1] = %c\n", bytes, greet,
	       greet[1]);

	f = add;
	printf("apply add = %d\n", apply(f, 2, 3));
	printf("apply sub = %d\n", apply(sub, 2, 3));
	printf("direct = %d\n", f(10, 20));
	return 0;
}
