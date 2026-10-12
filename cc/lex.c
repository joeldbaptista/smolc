/*
 * Small-C compiler -- lexer and symbol table.
 *
 * Small C, 8088/8086 version -- modified by R. Grehan, BYTE Magazine.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "scc.h"

void
junk(void)
{
	if (an(inbyte())) {
		while (an(cch))
			gch();
	} else {
		while (an(cch) == 0) {
			if (cch == 0)
				break;
			gch();
		}
	}
	blanks();
}

int
endst(void)
{
	blanks();
	return streq(lptr, ";") || cch == 0;
}

void
illname(void)
{
	error("illegal symbol");
	junk();
}

void
multidef(const char *sname)
{
	(void)sname;
	error("already defined");
}

void
needtoken(const char *str)
{
	if (match(str) == 0)
		error("missing token");
}

void
needlval(void)
{
	error("must be lvalue");
}

char *
findglb(const char *sname)
{
	if (search(sname, STARTGLB, SYMMAX, ENDGLB, NUMGLBS, NAME))
		return cptr;
	return NULL;
}

char *
findloc(const char *sname)
{
	cptr = locptr - 1; /* search backward for block locals */
	while (cptr > STARTLOC) {
		cptr = cptr - *cptr;
		if (astreq(sname, cptr, NAMEMAX))
			return cptr - NAME;
		cptr = cptr - NAME - 1;
	}
	return NULL;
}

char *
addsym(const char *sname, int id, int typ, int value, char **lgptrptr,
       int class, int tag)
{
	if (lgptrptr == &glbptr) {
		if ((cptr2 = findglb(sname)))
			return cptr2;
		if (cptr == NULL) {
			error("global symbol table overflow");
			return NULL;
		}
	} else {
		if (locptr > (ENDLOC - SYMMAX)) {
			error("local symbol table overflow");
			exit(ERRCODE);
		}
		cptr = *lgptrptr;
	}
	cptr[IDENT] = (char)id;
	cptr[TYPE] = (char)typ;
	cptr[CLASS] = (char)class;
	cptr[TAG] = (char)tag;
	putint(value, cptr + OFFSET, OFFSIZE);
	cptr3 = cptr2 = cptr + NAME;
	while (an(*sname))
		*cptr2++ = *sname++;
	if (lgptrptr == &locptr) {
		*cptr2 = (char)(cptr2 - cptr3); /* set the length */
		*lgptrptr = ++cptr2;
	}
	return cptr;
}

char *
nextsym(char *entry)
{
	entry = entry + NAME;
	while (*entry++ >= ' ') /* find the length byte */
		;
	return entry;
}

/*
 * Get an integer of length len from addr, in the byte order that
 * putint() wrote.
 */
Word
getint(char *addr, int len)
{
	unsigned long long u;

	/* sign extend the top byte */
	u = (unsigned long long)(Word)(signed char)*(addr + --len);
	while (len--)
		u = (u << 8) | (unsigned long long)(*(addr + len) & 255);
	return (Word)u;
}

/* Put integer i of length len into addr, low byte first. */
void
putint(Word i, char *addr, int len)
{
	while (len--) {
		*addr++ = (char)i;
		i = i >> 8;
	}
}

/* Test whether the next input string is a legal symbol name. */
int
symname(char *sname, int ucase)
{
	int k;

#ifndef UPPER
	(void)ucase;
#endif
	blanks();
	if (alpha(cch) == 0)
		return (*sname = 0);
	k = 0;
	while (an(cch)) {
#ifdef UPPER
		if (ucase)
			sname[k] = toupper(gch());
		else
			sname[k] = gch();
#else
		sname[k] = (char)gch();
#endif
		if (k < NAMEMAX)
			++k;
	}
	sname[k] = 0;
	return 1;
}

/* Return the next available internal label number. */
int
getlabel(void)
{
	return ++nxtlab;
}

/* Post a label in the program. */
void
postlabel(int label)
{
	printlabel(label);
	col();
	nl();
}

/* Print the given number as a label. */
void
printlabel(int label)
{
	outstr(".L"); /* local to the file in the assembler */
	outdec(label);
}

/* Test whether c is alphabetic. */
int
alpha(int c)
{
	return isalpha((unsigned char)c) || c == '_';
}

/* Test whether c is alphanumeric. */
int
an(int c)
{
	return alpha(c) || isdigit((unsigned char)c);
}

void
addwhile(int *ptr)
{
	int k;

	ptr[WQSP] = csp;          /* and the stack pointer */
	ptr[WQLOOP] = getlabel(); /* and the looping label */
	ptr[WQEXIT] = getlabel(); /* and the exit label */
	if (wqptr == WQMAX) {
		error("too many active loops");
		exit(ERRCODE);
	}
	k = 0;
	while (k < WQSIZ)
		*wqptr++ = ptr[k++];
}

void
delwhile(void)
{
	if (wqptr > wq)
		wqptr = wqptr - WQSIZ;
}

int *
readwhile(int *ptr)
{
	if (ptr <= wq) {
		error("out of context");
		return NULL;
	}
	return ptr - WQSIZ;
}

int
white(void)
{
	return *lptr <= ' ' && *lptr != '\0';
}

int
gch(void)
{
	int c;

	if ((c = cch))
		bump(1);
	return c;
}

void
bump(int n)
{
	if (n)
		lptr = lptr + n;
	else
		lptr = line;
	if ((cch = nch = *lptr))
		nch = *(lptr + 1);
}

/* Discard the rest of the current line. */
void
killline(void)
{
	*line = 0;
	bump(0);
}

int
inbyte(void)
{
	while (cch == 0) {
		if (eof)
			return 0;
		preprocess();
	}
	return gch();
}

/* Read the next line of source into the line buffer. */
void
readline(void)
{
	FILE *unit;
	int c;

	if (input == NULL)
		openfile();
	if (eof)
		return;
	if ((unit = input2) == NULL)
		unit = input;
	if (fgets(line, LINEMAX, unit) == NULL) {
		fclose(unit);
		if (input2 != NULL)
			input2 = NULL;
		else
			input = NULL;
		*line = '\0';
	} else {
		if (strchr(line, NEWLINE) == NULL && feof(unit) == 0) {
			/* the rest would read as a line of its own */
			error("line too long");
			while ((c = fgetc(unit)) != EOF && c != NEWLINE)
				;
		}
		if (listfp) {
			if (listfp == output)
				cout(';', output);
			sout(line, listfp);
		}
	}
	bump(0);
}
