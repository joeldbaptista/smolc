/*
 * string.c -- string and memory functions.
 */

#include stdio.h

char *
strcat(char *s, char *t)
{
	char *d;

	d = s;
	--s;
	while (*++s)
		;
	while ((*s++ = *t++))
		;
	return d;
}

char *
strncat(char *s, char *t, int n)
{
	char *d;

	d = s;
	--s;
	while (*++s)
		;
	while (n--) {
		if ((*s++ = *t++))
			continue;
		return d;
	}
	*s = 0;
	return d;
}

char *
strchr(char *str, char c)
{
	while (*str) {
		if (*str == c)
			return str;
		++str;
	}
	return NULL;
}

char *
strrchr(char *s, char c)
{
	char *ptr;

	ptr = NULL;
	while (*s) {
		if (*s == c)
			ptr = s;
		++s;
	}
	return ptr;
}

int
strcmp(char *s, char *t)
{
	while (*s == *t) {
		if (*s == 0)
			return 0;
		++s;
		++t;
	}
	return *s - *t;
}

int
strncmp(char *s, char *t, int n)
{
	while (n && *s == *t) {
		if (*s == 0)
			return 0;
		++s;
		++t;
		--n;
	}
	if (n)
		return *s - *t;
	return 0;
}

char *
strcpy(char *s, char *t)
{
	char *d;

	d = s;
	while ((*s++ = *t++))
		;
	return d;
}

char *
strncpy(char *dest, char *sour, int n)
{
	char *d;

	d = dest;
	while (n-- > 0) {
		if ((*d++ = *sour++))
			continue;
		while (n-- > 0)
			*d++ = 0;
	}
	*d = 0;
	return dest;
}

int
strlen(char *s)
{
	char *t;

	t = s - 1;
	while (*++t)
		;
	return t - s;
}

/* Reverse s in place. */
void
reverse(char *s)
{
	char *j;
	int c;

	j = s + strlen(s) - 1;
	while (s < j) {
		c = *s;
		*s++ = *j;
		*j-- = c;
	}
}

/* Remove the leading blanks of str. */
void
left(char *str)
{
	char *str2;

	str2 = str;
	while (*str2 == ' ')
		++str2;
	while ((*str++ = *str2++))
		;
}

/* Fill n bytes at dest with ch. */
void
pad(char *dest, int ch, int n)
{
	while (n-- > 0)
		*dest++ = ch;
}

/*
 * Collating sequence placing digits and letters in dictionary order,
 * with upper and lower case treated alike.
 */
char Ulex[128] = {
    /**** NUL - / ****/
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47,
    /**** 0-9 ****/
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74,
    /**** : ; < = > ? @ ****/
    48, 49, 50, 51, 52, 53, 54,
    /**** A-Z ****/
    75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87,
    88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100,
    /**** [ \ ] ^ _ ` ****/
    55, 56, 57, 58, 59, 60,
    /**** a-z ****/
    75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87,
    88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100,
    /**** { | } ~ ****/
    61, 62, 63, 64,
    /**** DEL ****/
    101};

/* Dictionary-order comparison: < 0, 0 or > 0 as s is <, = or > t. */
int
lexcmp(char *s, char *t)
{
	while (lexorder(*s, *t) == 0) {
		if (*s++)
			++t;
		else
			return 0;
	}
	return lexorder(*s, *t);
}

int
lexorder(char c1, char c2)
{
	return Ulex[c1 & 127] - Ulex[c2 & 127];
}
