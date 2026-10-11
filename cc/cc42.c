/*
 * Small-C compiler -- smol instruction emitters.
 *
 * r0 is the primary register and r1 the secondary.  A binary operator
 * leaves its result in the primary; a comparison leaves 0 or 1 there.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "cc.h"

static void zerotest(const char *jump, int label);
static void compare(const char *set);
static const char *setjump(char *p);
static char *putline(char *p, char *next);
static char *nextline(char *p);
static int linestarts(char *p, const char *s);
static int numline(char *p, const char *prefix, Word *n);
static int constline(char *p, Word *n);
static int addline(char *p, Word *n);
static void outnum(Word n);

/* Add the primary and secondary registers, result in the primary. */
void
ffadd(void)
{
	ol("\tadd r0, r1");
}

/* Subtract the primary from the secondary, result in the primary. */
void
ffsub(void)
{
	ol("\tsub r1, r0");
	ol("\tmov r0, r1");
}

/* Multiply the primary and secondary, result in the primary. */
void
ffmult(void)
{
	ol("\tmul r0, r1");
}

/*
 * Divide the secondary by the primary, leaving the quotient in the
 * primary and the remainder in the secondary.
 */
void
ffdiv(void)
{
	ol("\tmov r2, r1");
	ol("\tdiv r2, r0");
	ol("\trem r1, r0");
	ol("\tmov r0, r2");
}

/*
 * Remainder of secondary/primary, leaving the remainder in the primary
 * and the quotient in the secondary.
 */
void
ffmod(void)
{
	ffdiv();
	swap();
}

/* Inclusive "or" of the primary and secondary, result in the primary. */
void
ffor(void)
{
	ol("\tor r0, r1");
}

/* Exclusive "or" of the primary and secondary, result in the primary. */
void
ffxor(void)
{
	ol("\txor r0, r1");
}

/* "and" of the primary and secondary, result in the primary. */
void
ffand(void)
{
	ol("\tand r0, r1");
}

/* Logical negation of the primary register. */
void
lneg(void)
{
	ol("\tcmp r0, 0");
	ol("\tsete r0");
}

/*
 * Arithmetic shift right of the secondary register by the number of
 * bits in the primary, result in the primary.
 */
void
ffasr(void)
{
	ol("\tsar r1, r0");
	ol("\tmov r0, r1");
}

/*
 * Arithmetic shift left of the secondary register by the number of bits
 * in the primary, result in the primary.
 */
void
ffasl(void)
{
	ol("\tshl r1, r0");
	ol("\tmov r0, r1");
}

/* Two's complement of the primary register. */
void
neg(void)
{
	ol("\tneg r0");
}

/* One's complement of the primary register. */
void
com(void)
{
	ol("\tnot r0");
}

/*
 * Increment the primary register by one object of n bytes.  Zero
 * stands for a plain integer, which steps by one.
 */
void
inc(int n)
{
	if (n <= 1) {
		ol("\tinc r0");
	} else {
		ot("\tadd r0, ");
		outdec(n);
		nl();
	}
}

/* Decrement the primary register by one object of n bytes. */
void
dec(int n)
{
	if (n <= 1) {
		ol("\tdec r0");
	} else {
		ot("\tsub r0, ");
		outdec(n);
		nl();
	}
}

/*
 * The comparisons test the secondary against the primary.  Small-C
 * applies the unsigned forms only to pointers, and an address on smol
 * is never negative, so each unsigned form emits the signed test.  They
 * stay separate functions because test() tells them apart.
 */

/* Test for equal to. */
void
ffeq(void)
{
	compare("sete");
}

/* Test for not equal to. */
void
ffne(void)
{
	compare("setne");
}

/* Test for less than (signed). */
void
fflt(void)
{
	compare("setl");
}

/* Test for less than or equal to (signed). */
void
ffle(void)
{
	compare("setle");
}

/* Test for greater than (signed). */
void
ffgt(void)
{
	compare("setg");
}

/* Test for greater than or equal to (signed). */
void
ffge(void)
{
	compare("setge");
}

/* Test for less than (unsigned). */
void
ult(void)
{
	compare("setl");
}

/* Test for less than or equal to (unsigned). */
void
ule(void)
{
	compare("setle");
}

/* Test for greater than (unsigned). */
void
ugt(void)
{
	compare("setg");
}

/* Test for greater than or equal to (unsigned). */
void
uge(void)
{
	compare("setge");
}

/*
 * The zero tests below jump to label when the test is false, so each
 * one emits the opposite condition.
 */
static void
zerotest(const char *jump, int label)
{
	ol("\tcmp r0, 0");
	ot("\t");
	ot(jump);
	outbyte(' ');
	printlabel(label);
	nl();
}

/* Compare the secondary with the primary, and set the primary with set. */
static void
compare(const char *set)
{
	ol("\tcmp r1, r0");
	ot("\t");
	ot(set);
	ol(" r0");
}

/* Test for equal to zero. */
void
eq0(int label)
{
	zerotest("jne", label);
}

/* Test for not equal to zero. */
void
ne0(int label)
{
	zerotest("je", label);
}

/* Test for less than zero. */
void
lt0(int label)
{
	zerotest("jge", label);
}

/* Test for less than or equal to zero. */
void
le0(int label)
{
	zerotest("jg", label);
}

/* Test for greater than zero. */
void
gt0(int label)
{
	zerotest("jle", label);
}

/* Test for greater than or equal to zero. */
void
ge0(int label)
{
	zerotest("jl", label);
}

/* Test for less than zero (unsigned), which is never true. */
void
ult0(int label)
{
	jump(label);
}

/*
 * The peephole optimizer, on when -o is given.  It rewrites these
 * sequences (complete list):
 *
 *	mov r0, N; add r0, r7; add r0, M  ->  mov r0, N+M; add r0, r7
 *	mov r0, N; add r0, r7; mov r0, [r0]  ->  mov r0, [r7 + N]
 *	mov r0, N; add r0, r7; movsx r0, byte [r0]
 *	                                  ->  movsx r0, byte [r7 + N]
 *	setCC r0; cmp r0, 0; je L         ->  jNCC L, the opposite jump
 *	setCC r0; cmp r0, 0; jne L        ->  jCC L
 *
 * The first three are a local variable access, the last two a
 * comparison tested by if, while, for, && or ||, none of which uses
 * the 0 or 1 afterwards.  It runs when staged code is written out,
 * after unpush() has adjusted the offsets, so it may change the text
 * unpush() relies on.
 */
void
peephole(char *ptr)
{
	char *line, *next, *after;
	const char *jump;
	Word n, m;

	if (optimize == NO) {
		while (*ptr)
			ptr = putline(ptr, nextline(ptr));
		return;
	}
	while (*ptr) {
		line = ptr;
		next = nextline(line);
		if (constline(line, &n) && linestarts(next, "\tadd r0, r7\n")) {
			after = nextline(next);
			while (addline(after, &m)) { /* fold member offsets */
				n += m;
				after = nextline(after);
			}
			if (linestarts(after, "\tmov r0, [r0]\n")) {
				sout("\tmov r0, [r7 + ", output);
				ptr = nextline(after);
			} else if (linestarts(after, "\tmovsx r0, byte [r0]\n")) {
				sout("\tmovsx r0, byte [r7 + ", output);
				ptr = nextline(after);
			} else {
				sout("\tmov r0, ", output);
				outnum(n);
				sout("\n\tadd r0, r7\n", output);
				ptr = after;
				continue;
			}
			outnum(n);
			sout("]\n", output);
			continue;
		}
		if ((jump = setjump(line)) &&
		    linestarts(next, "\tcmp r0, 0\n")) {
			after = nextline(next);
			if (linestarts(after, "\tje ")) {
				jump = jump + strlen(jump) + 1; /* the opposite */
				after += 4;
			} else if (linestarts(after, "\tjne ")) {
				after += 5;
			} else {
				jump = NULL;
			}
			if (jump) {
				cout('\t', output);
				sout(jump, output);
				cout(' ', output);
				ptr = after;
				next = nextline(after);
			}
		}
		ptr = putline(ptr, next);
	}
}

/*
 * Write the text from p up to next, the start of the following line,
 * without trailing blanks: a pushed line keeps some while staged, so
 * that unpush() can overwrite it with a longer one.  Returns next.
 */
static char *
putline(char *p, char *next)
{
	char *end;

	end = next;
	if (end > p && end[-1] == '\n')
		--end;
	while (end > p && end[-1] == ' ')
		--end;
	while (p < end)
		cout(*p++, output);
	if (next > end && next[-1] == '\n')
		cout('\n', output);
	return next;
}

/*
 * If the line at p is "setCC r0", return the jump taken when CC holds,
 * followed in memory by the opposite one.  Else return NULL.
 */
static const char *
setjump(char *p)
{
	static const char *jumps[] = {
		"\tsete r0\n", "je\0jne",
		"\tsetne r0\n", "jne\0je",
		"\tsetl r0\n", "jl\0jge",
		"\tsetle r0\n", "jle\0jg",
		"\tsetg r0\n", "jg\0jle",
		"\tsetge r0\n", "jge\0jl",
	};
	size_t i;

	for (i = 0; i < sizeof(jumps) / sizeof(jumps[0]); i += 2)
		if (linestarts(p, jumps[i]))
			return jumps[i + 1];
	return NULL;
}

/* Return the start of the line after the one at p. */
static char *
nextline(char *p)
{
	while (*p && *p != '\n')
		++p;
	return *p ? p + 1 : p;
}

/* Return true if the line at p starts with s, which ends with a newline. */
static int
linestarts(char *p, const char *s)
{
	while (*s)
		if (*p++ != *s++)
			return 0;
	return 1;
}

/* Return true if the line at p is "<prefix><number>\n", setting *n. */
static int
numline(char *p, const char *prefix, Word *n)
{
	Word v;
	int neg;

	while (*prefix)
		if (*p++ != *prefix++)
			return 0;
	while (*p == ' ')
		++p;
	neg = (*p == '-');
	if (neg)
		++p;
	if (isdigit((unsigned char)*p) == 0)
		return 0;
	v = 0;
	while (isdigit((unsigned char)*p))
		v = v * 10 + (*p++ - '0');
	if (*p != '\n')
		return 0;
	*n = neg ? -v : v;
	return 1;
}

/* Return true if the line at p loads a constant into r0. */
static int
constline(char *p, Word *n)
{
	return numline(p, "\tmov r0, ", n);
}

/* Return true if the line at p adds a constant to r0. */
static int
addline(char *p, Word *n)
{
	return numline(p, "\tadd r0, ", n);
}

/* Write n to the output in decimal. */
static void
outnum(Word n)
{
	char buf[24];
	int i;

	if (n < 0) {
		cout('-', output);
		n = -n;
	}
	i = 0;
	do {
		buf[i++] = (char)('0' + n % 10);
		n /= 10;
	} while (n);
	while (i)
		cout(buf[--i], output);
}
