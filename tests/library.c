/*
 * library.c -- the runtime: input, conversion, strings and memory.
 * It reads library.in on stdin.
 */

#include stdio.h

int
main(void)
{
	char buf[40], word[20], *p, *q;
	int a, b, n, c;

	n = scanf("%d %x %s", &a, &b, word);
	printf("scanf %d: %d %d %s\n", n, a, b, word);
	while ((c = getchar()) != '\n')
		;
	fgets(buf, 40, stdin);
	printf("fgets: %s", buf);
	c = getchar();
	ungetc(c, stdin);
	printf("ungetc: %c %c\n", c, getchar());
	n = 0;
	while ((c = getchar()) != EOF)
		++n;
	printf("rest: %d, eof: %d\n", n, feof(stdin) != 0);

	printf("atoi: %d %d\n", atoi("  -123x"), atoib("ff", 16));
	xtoi("7fffffffffffffff", &a);
	printf("xtoi: %d\n", a);
	itod(-42, buf, 8);
	itox(255, word, 6);
	printf("itod: [%s] itox: [%s]\n", buf, word);
	itou(-1, buf, 22);
	printf("itou: [%s]\n", buf);

	strcpy(buf, "alpha");
	strncat(buf, "betagamma", 4);
	printf("str: %s %d %d %d\n", buf, strlen(buf),
	       strncmp(buf, "alphx", 4), strcmp("a", "b") < 0);
	printf("chr: %s %s\n", strchr(buf, 'b'), strrchr(buf, 'a'));
	printf("lexcmp: %d %d\n", lexcmp("Apple", "apple"),
	       lexcmp("a1", "ab") < 0);
	printf("ctype: %d%d%d%d%d %c%c\n", isalpha('x'), isdigit('x'),
	       isspace('\t'), ispunct('!'), isupper('a'), toupper('q'),
	       tolower('Q'));

	/* avail() depends on the stack depth, so call it at one depth */
	a = avail(NO);
	p = malloc(100);
	q = calloc(10, 8);
	b = avail(NO);
	printf("heap: %d %d %d\n", q - p, a - b, *(q + 79));
	free(p);
	b = avail(NO);
	printf("freed: %d\n", a == b);
	return 0;
}
