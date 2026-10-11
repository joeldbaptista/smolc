/*
 * Small-C compiler -- driver.
 *
 * Small C, 8088/8086 version -- modified by R. Grehan, BYTE Magazine.
 * Retargeted to the smol machine for smolc.
 */

#define _POSIX_C_SOURCE 200809L /* fileno() */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cc.h"
#include "notice.h"

static void *xcalloc(size_t n, size_t sz);
static int getarg(int n, char *s, int size, int argc, char **argv);
static void usage(void);
static void ask(void);
static FILE *mustopen(const char *fn, const char *mode);
static void setops(void);
static void parse(void);
static void outside(void);

/*
 * Allocate zeroed memory or give up.  The compiler is a one-shot
 * program, so nothing it allocates is ever released.
 */
static void *
xcalloc(size_t n, size_t sz)
{
	void *p;

	if (!(p = calloc(n, sz))) {
		fputs("out of memory\n", stderr);
		exit(ERRCODE);
	}
	return p;
}

int
main(int argc, char *argv[])
{
	argcs = argc;
	argvs = argv;
	fputs("Small-C Compiler, ", stderr);
	fputs(VERSION, stderr);
	fputs(CRIGHT1, stderr);
	fputs(CRIGHT2, stderr);

	swnext = xcalloc(SWTABSZ, sizeof(Word));
	swend = swnext + SWTABSZ - SWSIZ;
	stage = xcalloc(STAGESIZE, 1);
	stagelast = stage + STAGELIMIT;
	wq = xcalloc(WQTABSZ, sizeof(int));
	litq = xcalloc(LITABSZ, 1);
	macn = xcalloc(MACNSIZE, 1);
	macq = xcalloc(MACQSIZE, 1);
	pline = xcalloc(LINESIZE, 1);
	mline = xcalloc(LINESIZE, 1);

	stagenext = NULL; /* direct output mode */
	swactive = 0;     /* not in a switch */
	iflevel = 0;      /* #if... nesting level */
	skiplevel = 0;    /* #if... not encountered */
	macptr = 0;       /* clear the macro pool */
	csp = 0;          /* relative stack pointer */
	errflag = 0;      /* not skipping errors till ";" */
	eof = 0;          /* not at end of input yet */
	ncmp = 0;         /* not in a compound statement */
	files = 0;
	filearg = 0;
	swused = 0;
	quote[0] = '"'; /* fake a quote literal */
	quote[1] = 0;
	func1 = 1;  /* the next function is the first */
	ccode = 1;  /* enable preprocessing */
	wqptr = wq; /* clear the while queue */
	input = input2 = NULL;

	ask();        /* get user options */
	openfile();   /* and the initial input file */
	preprocess(); /* fetch the first line */

	symtab = xcalloc(SYMTBSZ, 1);
	membtab = xcalloc(NUMMEMB, SYMMAX);
	locptr = STARTLOC;
	glbptr = STARTGLB;

	header();  /* intro code */
	setops();  /* set values in the op arrays */
	parse();   /* process ALL input */
	outside(); /* verify we ended outside any function */
	trailer(); /* follow-up code */
	if (fclose(output) == EOF)
		nerrors++;
	return nerrors ? ERRCODE : 0;
}

/*
 * Process all input text.  At this level only static declarations,
 * defines, includes and function definitions are legal.
 */
static void
parse(void)
{
	while (eof == 0) {
		if (amatch("extern", 6)) {
			dodeclare(EXTERNAL);
		} else if (amatch("typedef", 7)) {
			dotypedef();
			ns();
		} else if (amatch("static", 6)) {
			/* internal linkage: its label is local to the file */
			filestatic = 1;
			if (dodeclare(STATIC) == 0)
				newfunc();
			filestatic = 0;
		} else if (dodeclare(STATIC)) {
			/* a global declaration or definition */
		} else if (match("#asm")) {
			doasm();
		} else if (match("#include")) {
			doinclude();
		} else if (match("#define")) {
			addmac();
		} else {
			newfunc();
		}
		blanks(); /* force eof if pending */
	}
}

/* Dump the literal pool. */
void
dumplits(int size)
{
	int j, k;

	k = 0;
	while (k < litptr) {
		defstorage(size);
		j = 10;
		while (j--) {
			outdec(getint(litq + k, size));
			k = k + size;
			if (j == 0 || k >= litptr) {
				nl();
				break;
			}
			outbyte(',');
		}
	}
}

/* Dump zeroes for default initial values. */
void
dumpzero(int size, int count)
{
	if (count <= 0)
		return;
	ot("\tresb ");
	outdec(size * count);
	nl();
}

/* Verify the compile ended outside any function. */
static void
outside(void)
{
	if (ncmp)
		error("no closing bracket");
}

/*
 * Get command line argument n into s, which holds size bytes.
 * Returns the number of characters moved, else EOF.
 */
static int
getarg(int n, char *s, int size, int argc, char **argv)
{
	char *str;
	int i;

	if (n < 0 || n >= argc) {
		*s = '\0';
		return EOF;
	}
	str = argv[n];
	i = 0;
	while (i < size - 1) {
		if ((s[i] = str[i]) == '\0')
			break;
		++i;
	}
	s[i] = '\0';
	return i;
}

static void
usage(void)
{
	sout("usage: scc [file]... [-Idir] [-m] [-a] [-p] [-l1|-l2]", stderr);
#ifdef OPTIMIZE
	sout(" [-o]", stderr);
#endif
	cout(NEWLINE, stderr);
	exit(ERRCODE);
}

/* Get run options. */
static void
ask(void)
{
	int i;

	i = 0;
	nxtlab = 0;
	listfp = NULL;
	output = stdout;
	optimize = NO;
	bell = NO;
	monitor = NO;
	pauseerr = NO;
	line = mline;
	while (getarg(++i, line, LINESIZE, argcs, argvs) != EOF) {
		if (line[0] != '-')
			continue;
		if (line[1] == 'I' && line[2]) {
			/* line is reused, so keep the argument itself */
			incdir = argvs[i] + 2;
			continue;
		}
		if (toupper(line[1]) == 'L' && isdigit(line[2]) &&
		    line[3] <= ' ') {
			/*
			 * The original selected a DOS file descriptor by
			 * number.  Only stdout and stderr have a Unix
			 * equivalent.
			 */
			if (line[2] == '1')
				listfp = stdout;
			else if (line[2] == '2')
				listfp = stderr;
			else
				usage();
			continue;
		}
		if (line[2] <= ' ') {
			if (toupper(line[1]) == 'A') {
				bell = YES;
				continue;
			}
			if (toupper(line[1]) == 'M') {
				monitor = YES;
				continue;
			}
#ifdef OPTIMIZE
			if (toupper(line[1]) == 'O') {
				optimize = YES;
				continue;
			}
#endif
			if (toupper(line[1]) == 'P') {
				pauseerr = YES;
				continue;
			}
		}
		usage();
	}
}

/*
 * Open the next input file.  When stdout is a terminal, the output
 * goes to a file named after the first input, with ".sas" in place of
 * its extension.  Otherwise it goes to stdout.
 */
void
openfile(void)
{
	char outfn[LINESIZE + 4];
	char *dot;

	input = NULL;
	while (getarg(++filearg, pline, LINESIZE, argcs, argvs) != EOF) {
		if (pline[0] == '-')
			continue;
		dot = strrchr(pline, '.');
		if (dot && strchr(dot, '/'))
			dot = NULL; /* the dot belongs to a directory */
		if (!dot) {
			dot = pline + strlen(pline);
			if (dot - pline + 2 < LINESIZE)
				strcpy(dot, ".c");
		}
		strcpy(outfn, pline);
		strcpy(outfn + (dot - pline), ".sas");
		input = mustopen(pline, "r");
		if (!files && isatty(fileno(stdout)))
			output = mustopen(outfn, "w");
		files = YES;
		killline();
		return;
	}
	if (files++)
		eof = YES;
	else
		input = stdin;
	killline();
}

/* Open a file with error checking. */
static FILE *
mustopen(const char *fn, const char *mode)
{
	FILE *fp;

	if ((fp = fopen(fn, mode)))
		return fp;
	sout("open error on ", stderr);
	lout(fn, stderr);
	exit(ERRCODE);
	/* NOTREACHED */
}

/* Fill the binary operator tables. */
static void
setops(void)
{
	op2[0] = op[0] = ffor;  /* heir5 */
	op2[1] = op[1] = ffxor; /* heir6 */
	op2[2] = op[2] = ffand; /* heir7 */
	op2[3] = op[3] = ffeq;  /* heir8 */
	op2[4] = op[4] = ffne;
	op2[5] = ule;
	op[5] = ffle; /* heir9 */
	op2[6] = uge;
	op[6] = ffge;
	op2[7] = ult;
	op[7] = fflt;
	op2[8] = ugt;
	op[8] = ffgt;
	op2[9] = op[9] = ffasr; /* heir10 */
	op2[10] = op[10] = ffasl;
	op2[11] = op[11] = ffadd; /* heir11 */
	op2[12] = op[12] = ffsub;
	op2[13] = op[13] = ffmult; /* heir12 */
	op2[14] = op[14] = ffdiv;
	op2[15] = op[15] = ffmod;
}
