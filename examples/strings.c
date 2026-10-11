/*
 * strings.c -- the Small-C string library.
 *
 * Shows: char pointers, pointer arithmetic, and strlen(), strcpy(),
 * strcat(), strchr() and strcmp() from clib.
 *
 * Local arrays are allowed, but locals cannot be initialised where
 * they are declared, so buf is filled by strcpy() instead.
 */

#include stdio.h

char *needle;

int
main(void)
{
	char buf[40];
	char *p;

	needle = "world";
	strcpy(buf, "hello, ");
	strcat(buf, needle);
	printf("buf      = \"%s\"\n", buf);
	printf("strlen   = %d\n", strlen(buf));

	p = strchr(buf, ',');
	if (p)
		printf("comma at = %d\n", p - buf);
	printf("strcmp   = %d\n", strcmp(buf, "hello, world"));

	/* walk it backwards a character at a time */
	p = buf + strlen(buf);
	while (p > buf)
		putchar(*--p);
	putchar('\n');
	return 0;
}
