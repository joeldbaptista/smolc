/*
 * stdlib.c -- numeric conversion, memory allocation and arguments.
 *
 * An int is 64 bits.  Small-C has no unsigned type, so the unsigned
 * conversions shift with Ushr1(), which clears the sign bit that an
 * arithmetic shift would copy.
 */

#include stdio.h

char *Umemptr; /* first free heap byte; crt0 sets it */

/* Shift n right by one bit, as if n were unsigned. */
int
Ushr1(int n)
{
	int m;

	m = 1;
	m = ~(m << 63);
	return (n >> 1) & m;
}

int
abs(int nbr)
{
	if (nbr < 0)
		return -nbr;
	return nbr;
}

int
sign(int nbr)
{
	if (nbr > 0)
		return 1;
	if (nbr == 0)
		return 0;
	return -1;
}

/* Convert the decimal string s, with optional sign, to an integer. */
int
atoi(char *s)
{
	int n, sign;

	while (isspace(*s))
		++s;
	sign = 1;
	switch (*s) {
	case '-':
		sign = -1;
		/* FALLTHROUGH */
	case '+':
		++s;
	}
	n = 0;
	while (isdigit(*s))
		n = 10 * n + *s++ - '0';
	return sign * n;
}

/* Convert s to an unsigned integer in base b.  Non-standard. */
int
atoib(char *s, int b)
{
	int digit, n;

	n = 0;
	while (isspace(*s))
		++s;
	while ((digit = (127 & *s++)) >= '0') {
		if (digit >= 'a')
			digit -= 87;
		else if (digit >= 'A')
			digit -= 55;
		else
			digit -= '0';
		if (digit >= b)
			break;
		n = b * n + digit;
	}
	return n;
}

/*
 * Convert an unsigned decimal string to the integer nbr.
 * Returns the field size, else ERR on overflow.
 */
int
utoi(char *decstr, int *nbr)
{
	int d, t;

	d = 0;
	*nbr = 0;
	while (*decstr >= '0' && *decstr <= '9') {
		t = *nbr;
		t = (10 * t) + (*decstr++ - '0');
		if (t >= 0 && *nbr < 0)
			return ERR;
		d++;
		*nbr = t;
	}
	return d;
}

/*
 * Convert a signed decimal string to the integer nbr.
 * Returns the field length, else ERR.
 */
int
dtoi(char *decstr, int *nbr)
{
	int len, s;

	if (*decstr == '-') {
		s = 1;
		++decstr;
	} else {
		s = 0;
	}
	if ((len = utoi(decstr, nbr)) < 0)
		return ERR;
	if (*nbr < 0)
		return ERR;
	if (s) {
		*nbr = -*nbr;
		return ++len;
	}
	return len;
}

/*
 * Convert an octal string to the integer nbr.
 * Returns the field size, else ERR.
 */
int
otoi(char *octstr, int *nbr)
{
	int d, t;

	d = 0;
	*nbr = 0;
	while (*octstr >= '0' && *octstr <= '7') {
		t = *nbr;
		t = (t << 3) + (*octstr++ - '0');
		if (t >= 0 && *nbr < 0)
			return ERR;
		d++;
		*nbr = t;
	}
	return d;
}

/*
 * Convert a hex string of up to 16 digits to the integer nbr.
 * Returns the field size, else ERR.
 */
int
xtoi(char *hexstr, int *nbr)
{
	char *cp;
	int b, d;

	d = *nbr = 0;
	cp = hexstr;
	while (*cp == '0')
		++cp;
	for (;;) {
		if (*cp >= '0' && *cp <= '9')
			b = 48;
		else if (*cp >= 'A' && *cp <= 'F')
			b = 55;
		else if (*cp >= 'a' && *cp <= 'f')
			b = 87;
		else
			return cp - hexstr;
		if (d < 16)
			++d;
		else
			return ERR;
		*nbr = (*nbr << 4) + (*cp++ - b);
	}
}

/* Convert n to a signed decimal string in s. */
void
itoa(int n, char *s)
{
	char *ptr;
	int neg;

	ptr = s;
	/* digits come from a negative n, which also holds the most negative int */
	if (!(neg = n < 0))
		n = -n;
	do {
		*ptr++ = '0' - n % 10;
	} while ((n = n / 10) < 0);
	if (neg)
		*ptr++ = '-';
	*ptr = '\0';
	reverse(s);
}

/* Convert unsigned n to a string in s, in the even base b.  Non-standard. */
void
itoab(int n, char *s, int b)
{
	char *ptr;
	int lowbit;

	ptr = s;
	b >>= 1;
	do {
		lowbit = n & 1;
		n = Ushr1(n);
		*ptr = ((n % b) << 1) + lowbit;
		if (*ptr < 10)
			*ptr += '0';
		else
			*ptr += 55;
		++ptr;
	} while (n /= b);
	*ptr = 0;
	reverse(s);
}

/*
 * The four functions below convert nbr to a string of width sz,
 * right adjusted and blank filled.  If sz > 0 the result is null
 * terminated; if sz = 0 the end of the string is found; if sz < 0
 * the last byte holds data.  Each returns str.
 */

/* Return the number of bytes itod() and the others below fill. */
int
Uwidth(char *str, int sz)
{
	if (sz > 0) {
		str[--sz] = NULL;
		return sz;
	}
	if (sz < 0)
		return -sz;
	while (str[sz] != NULL)
		++sz;
	return sz;
}

/* signed decimal */
char *
itod(int nbr, char *str, int sz)
{
	char sgn;

	sz = Uwidth(str, sz);
	if (nbr < 0)
		sgn = '-';
	else {
		sgn = ' ';
		nbr = -nbr; /* digits come from a negative nbr */
	}
	while (sz) {
		str[--sz] = '0' - nbr % 10;
		if ((nbr = nbr / 10) == 0)
			break;
	}
	if (sz)
		str[--sz] = sgn;
	while (sz > 0)
		str[--sz] = ' ';
	return str;
}

/* unsigned decimal */
char *
itou(int nbr, char *str, int sz)
{
	int lowbit;

	sz = Uwidth(str, sz);
	while (sz) {
		lowbit = nbr & 1;
		nbr = Ushr1(nbr); /* divide by 2 */
		str[--sz] = ((nbr % 5) << 1) + lowbit + '0';
		if ((nbr = nbr / 5) == 0)
			break;
	}
	while (sz)
		str[--sz] = ' ';
	return str;
}

/* octal */
char *
itoo(int nbr, char *str, int sz)
{
	sz = Uwidth(str, sz);
	while (sz) {
		str[--sz] = (nbr & 7) + '0';
		if ((nbr = Ushr1(nbr) >> 2) == 0)
			break;
	}
	while (sz)
		str[--sz] = ' ';
	return str;
}

/* hexadecimal */
char *
itox(int nbr, char *str, int sz)
{
	int digit;

	sz = Uwidth(str, sz);
	while (sz) {
		digit = nbr & 15;
		if (digit < 10)
			str[--sz] = digit + '0';
		else
			str[--sz] = digit + 55;
		if ((nbr = Ushr1(nbr) >> 3) == 0)
			break;
	}
	while (sz)
		str[--sz] = ' ';
	return str;
}

/*
 * Return the number of bytes between the heap and the stack.  If they
 * have met, the program exits with status 'M' when fatal is non-zero,
 * and zero is returned otherwise.
 */
int
avail(int fatal)
{
	char x;

	if (&x < Umemptr) {
		if (fatal)
			exit('M');
		return 0;
	}
	return &x - Umemptr;
}

/*
 * Allocate n bytes, cleared to zero if clear is true.  The heap is a
 * stack: memory must be freed in the reverse order of allocation.
 * Returns the block, else NULL.
 */
char *
Ualloc(int n, int clear)
{
	char *oldptr;

	if (n < 0 || n >= avail(YES))
		return NULL;
	if (clear)
		pad(Umemptr, NULL, n);
	oldptr = Umemptr;
	Umemptr += n;
	return oldptr;
}

char *
malloc(int size)
{
	return Ualloc(size, NO);
}

char *
calloc(int n, int size)
{
	return Ualloc(n * size, YES);
}

/* Free ptr, and every block allocated after it.  Returns ptr. */
char *
free(char *ptr)
{
	return Umemptr = ptr;
}

/*
 * Copy argument n of main's argc and argv into s, which holds size
 * bytes.  Returns the number of characters moved, else EOF.
 */
int
getarg(int n, char *s, int size, int argc, int *argv)
{
	char *str;
	int i;

	if (n < 0 || n >= argc) {
		*s = NULL;
		return EOF;
	}
	i = 0;
	str = argv[n];
	while (i < size - 1) {
		if ((s[i] = str[i]) == NULL)
			break;
		++i;
	}
	s[i] = NULL;
	return i;
}
