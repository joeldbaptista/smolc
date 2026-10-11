/*
 * ctype.c -- character classification and case conversion.
 */

#include stdio.h

int
isalpha(int c)
{
	return (c <= 'z' && c >= 'a') || (c <= 'Z' && c >= 'A');
}

int
isdigit(int c)
{
	return c <= '9' && c >= '0';
}

int
isalnum(int c)
{
	return isalpha(c) || isdigit(c);
}

int
isascii(int c)
{
	return c >= 0 && c <= 127;
}

int
iscntrl(int c)
{
	return (c >= 0 && c <= 31) || c == 127;
}

int
isgraph(int c)
{
	return c >= 33 && c <= 126;
}

int
islower(int c)
{
	return c <= 'z' && c >= 'a';
}

int
isupper(int c)
{
	return c <= 'Z' && c >= 'A';
}

int
isprint(int c)
{
	return c >= 32 && c <= 126;
}

int
ispunct(int c)
{
	return isgraph(c) && !isalnum(c);
}

int
isspace(int c)
{
	/* the first test gives a quick exit in most cases */
	return c <= ' ' && (c == ' ' || (c <= 13 && c >= 9));
}

int
isxdigit(int c)
{
	return (c <= 'f' && c >= 'a') || (c <= 'F' && c >= 'A') ||
	       isdigit(c);
}

int
toascii(int c)
{
	return c & 127;
}

int
tolower(int c)
{
	if (isupper(c))
		return c + 32;
	return c;
}

int
toupper(int c)
{
	if (islower(c))
		return c - 32;
	return c;
}
