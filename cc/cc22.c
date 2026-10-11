/*
 * Small-C compiler -- preprocessor and output.
 *
 * Small C, 8088/8086 version -- modified by R. Grehan, BYTE Magazine.
 */

#include <stdio.h>
#include <stdlib.h>

#include "cc.h"

static void ifline(void);
static void keepch(int c);
static void noiferr(void);
static int putmac(int c);
static unsigned hash(const char *sname);
static void errout(const char *msg, FILE *fp);
static void xout(void);

/* Read lines, acting on #ifdef/#ifndef/#else/#endif, until one is live. */
static void
ifline(void)
{
	for (;;) {
		readline();
		if (eof)
			return;
		if (match("#ifdef")) {
			++iflevel;
			if (skiplevel)
				continue;
			symname(msname, NO);
			if (search(msname, macn, NAMESIZE + 2, MACNEND,
			           MACNBR, 0) == 0)
				skiplevel = iflevel;
			continue;
		}
		if (match("#ifndef")) {
			++iflevel;
			if (skiplevel)
				continue;
			symname(msname, NO);
			if (search(msname, macn, NAMESIZE + 2, MACNEND,
			           MACNBR, 0))
				skiplevel = iflevel;
			continue;
		}
		if (match("#else")) {
			if (iflevel) {
				if (skiplevel == iflevel)
					skiplevel = 0;
				else if (skiplevel == 0)
					skiplevel = iflevel;
			} else {
				noiferr();
			}
			continue;
		}
		if (match("#endif")) {
			if (iflevel) {
				if (skiplevel == iflevel)
					skiplevel = 0;
				--iflevel;
			} else {
				noiferr();
			}
			continue;
		}
		if (skiplevel)
			continue;
		if (cch == 0)
			continue;
		break;
	}
}

static void
keepch(int c)
{
	if (pptr < LINEMAX)
		pline[++pptr] = (char)c;
}

/*
 * Fetch the next logical line, expanding macros, stripping comments and
 * collapsing white space.  Strings and character constants pass through
 * untouched.
 */
void
preprocess(void)
{
	int c, k;

	if (ccode) {
		line = mline;
		ifline();
		if (eof)
			return;
	} else {
		line = pline;
		readline();
		return;
	}
	pptr = -1;
	while (cch != NEWLINE && cch) {
		if (white()) {
			keepch(' ');
			while (white())
				gch();
		} else if (cch == '"') {
			keepch(cch);
			gch();
			while (cch != '"' ||
			       (*(lptr - 1) == '\\' && *(lptr - 2) != '\\')) {
				if (cch == 0) {
					error("no quote");
					break;
				}
				keepch(gch());
			}
			gch();
			keepch('"');
		} else if (cch == '\'') {
			keepch('\'');
			gch();
			while (cch != '\'' ||
			       (*(lptr - 1) == '\\' && *(lptr - 2) != '\\')) {
				if (cch == 0) {
					error("no apostrophe");
					break;
				}
				keepch(gch());
			}
			gch();
			keepch('\'');
		} else if (cch == '/' && nch == '*') {
			bump(2);
			while (!(cch == '*' && nch == '/')) {
				if (cch) {
					bump(1);
				} else {
					ifline();
					if (eof)
						break;
				}
			}
			bump(2);
		} else if (an(cch)) {
			k = 0;
			while (an(cch) && k < NAMEMAX) {
				msname[k++] = (char)cch;
				gch();
			}
			msname[k] = 0;
			if (search(msname, macn, NAMESIZE + 2, MACNEND,
			           MACNBR, 0)) {
				k = getint(cptr + NAMESIZE, 2);
				while ((c = macq[k++]))
					keepch(c);
				while (an(cch))
					gch();
			} else {
				k = 0;
				while ((c = msname[k++]))
					keepch(c);
			}
		} else {
			keepch(gch());
		}
	}
	if (pptr >= LINEMAX)
		error("line too long");
	keepch(0);
	line = pline;
	bump(0);
}

static void
noiferr(void)
{
	error("no matching #if...");
	errflag = 0;
}

void
addmac(void)
{
	int k;

	if (symname(msname, NO) == 0) {
		illname();
		killline();
		return;
	}
	k = 0;
	if (search(msname, macn, NAMESIZE + 2, MACNEND, MACNBR, 0) == 0) {
		if ((cptr2 = cptr)) {
			while ((*cptr2++ = msname[k++]))
				;
		} else {
			error("macro name table full");
			return;
		}
	}
	putint(macptr, cptr + NAMESIZE, 2);
	while (white())
		gch();
	while (putmac(gch()))
		;
	if (macptr >= MACMAX) {
		error("macro string queue full");
		exit(ERRCODE);
	}
}

static int
putmac(int c)
{
	macq[macptr] = (char)c;
	if (macptr < MACMAX)
		++macptr;
	return c;
}

/*
 * Search for a symbol match.  On return cptr points at the slot found,
 * at an empty slot, or is NULL when the table is full.
 */
int
search(const char *sname, char *buf, int len, char *end, int max, int off)
{
	cptr = cptr2 = buf + (int)(hash(sname) % (unsigned)(max - 1)) * len;
	while (*cptr != 0) {
		if (astreq(sname, cptr + off, NAMEMAX))
			return 1;
		if ((cptr = cptr + len) >= end)
			cptr = buf;
		if (cptr == cptr2) {
			cptr = NULL;
			return 0;
		}
	}
	return 0;
}

static unsigned
hash(const char *sname)
{
	unsigned i;
	int c;

	i = 0;
	while ((c = *sname++))
		i = (i << 1) + (unsigned)c;
	return i;
}

void
setstage(char **before, char **start)
{
	if ((*before = stagenext) == NULL)
		stagenext = stage;
	*start = stagenext;
}

void
clearstage(char *before, char *start)
{
	*stagenext = 0;
	if ((stagenext = before))
		return;
	if (start) {
#ifdef OPTIMIZE
		peephole(start);
#else
		sout(start, output);
#endif
	}
}

void
outdec(Word number)
{
	char buf[24];
	unsigned long long u;
	int i;

	if (number < 0) {
		outbyte('-');
		u = 0 - (unsigned long long)number;
	} else {
		u = (unsigned long long)number;
	}
	i = 0;
	do {
		buf[i++] = (char)('0' + u % 10);
		u /= 10;
	} while (u);
	while (i)
		outbyte(buf[--i]);
}

void
ol(const char *ptr)
{
	ot(ptr);
	nl();
}

/* Output a C string.  Unlike outstr(), it passes tabs through. */
void
ot(const char *ptr)
{
	while (*ptr)
		outbyte(*ptr++);
}

/*
 * Act like a real C compiler: prefix every label with an underscore.
 * That also keeps C names apart from smol registers and keywords.
 */
void
outlab(const char *ptr)
{
	ot("_");
	outstr(ptr);
}

void
outstr(const char *ptr)
{
	/* symbol table names are terminated by a length byte */
	while (*ptr >= ' ')
		outbyte(*ptr++);
}

int
outbyte(int c)
{
	if (stagenext) {
		if (stagenext == stagelast) {
			error("staging buffer overflow");
			return 0;
		}
		*stagenext++ = (char)c;
	} else {
		cout(c, output);
	}
	return c;
}

void
cout(int c, FILE *fp)
{
	if (fputc(c, fp) == EOF)
		xout();
}

void
sout(const char *s, FILE *fp)
{
	while (*s)
		cout(*s++, fp);
}

void
lout(const char *s, FILE *fp)
{
	sout(s, fp);
	cout(NEWLINE, fp);
}

static void
xout(void)
{
	fputs("output error", stderr);
	exit(ERRCODE);
}

void
nl(void)
{
	outbyte(NEWLINE);
}

void
col(void)
{
#ifdef COL
	outbyte(':');
#endif
}

void
error(const char *msg)
{
	int c;

	if (errflag)
		return;
	errflag = 1;
	nerrors++;
	lout(line, stderr);
	errout(msg, stderr);
	if (bell)
		fputc(7, stderr);
	if (pauseerr) {
		/* the original read stderr, which DOS allowed */
		while ((c = fgetc(stdin)) != NEWLINE && c != EOF)
			;
	}
	if (listfp)
		errout(msg, listfp);
}

static void
errout(const char *msg, FILE *fp)
{
	char *p;

	p = line + 2;
	while (p++ <= lptr)
		cout(' ', fp);
	lout("/\\", fp);
	sout("**** ", fp);
	lout(msg, fp);
}

int
streq(const char *s1, const char *s2)
{
	int k;

	k = 0;
	while (s2[k]) {
		if (s1[k] != s2[k])
			return 0;
		++k;
	}
	return k;
}

int
astreq(const char *s1, const char *s2, int len)
{
	int k;

	k = 0;
	while (k < len) {
		if (s1[k] != s2[k])
			break;
		/*
		 * Symbol table names end with the symbol length held as a
		 * binary byte, so anything below a space ends the name.
		 */
		if (s1[k] < ' ')
			break;
		if (s2[k] < ' ')
			break;
		++k;
	}
	if (an(s1[k]))
		return 0;
	if (an(s2[k]))
		return 0;
	return k;
}

int
match(const char *lit)
{
	int k;

	blanks();
	if ((k = streq(lptr, lit))) {
		bump(k);
		return 1;
	}
	return 0;
}

int
amatch(const char *lit, int len)
{
	int k;

	blanks();
	if ((k = astreq(lptr, lit, len))) {
		bump(k);
		while (an(cch))
			inbyte();
		return 1;
	}
	return 0;
}

int
nextop(const char *list)
{
	char tok[4];

	opindex = 0;
	blanks();
	for (;;) {
		opsize = 0;
		while (*list > ' ')
			tok[opsize++] = *list++;
		tok[opsize] = 0;
		if ((opsize = streq(lptr, tok))) {
			if (*(lptr + opsize) != '=' &&
			    *(lptr + opsize) != *(lptr + opsize - 1))
				return 1;
		}
		if (*list) {
			++list;
			++opindex;
		} else {
			return 0;
		}
	}
}

void
blanks(void)
{
	for (;;) {
		while (cch) {
			if (white())
				gch();
			else
				return;
		}
		if (line == mline)
			return;
		preprocess();
		if (eof)
			break;
	}
}
