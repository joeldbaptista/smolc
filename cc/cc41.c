/*
 * Small-C compiler -- smol code generator.
 *
 * r0 is the primary register, which the 8088 version kept in BX, and
 * r1 the secondary, which it kept in DX.  r2 is scratch, r3 carries
 * the argument count into a call, and r7 is the stack pointer.
 */

#include <ctype.h>
#include <stdio.h>

#include "cc.h"

/*
 * unpush() overwrites a staged push with a swap, so the two lines must
 * have the same length.  It also finds stack-relative address loads by
 * their second line, which nothing else may emit.
 */
#define PUSHLINE "\tpush r0    "
#define SWAPLINE "\txchg r0, r1"
#define SPLINE "\tadd r0, r7"
#define FIXWIDTH 10

static void unpush(char *dest);

/* Print all assembler info before any code is generated. */
void
header(void)
{
	ol("; smolc output");
	beglab = getlabel();
}

/*
 * Print any assembler stuff needed at the end.  The assembler links
 * every file of a program in one run, so there is nothing to declare.
 */
void
trailer(void)
{
}

/* Load the argument count before a function call. */
void
loadargc(int val)
{
	if (search("NOCCARGC", macn, NAMESIZE + 2, MACNEND, MACNBR, 0) == 0) {
		ot("\tmov r3, ");
		outdec(val);
		nl();
	}
}

/* Declare an entry point: the label of the function named in ssname. */
void
entry(void)
{
	char *sym;

	if ((sym = findglb(ssname)))
		symlabel(sym);
	else
		outlab(ssname);
	col();
	nl();
}

/* Declare an external reference, which the assembler resolves itself. */
void
external(const char *name)
{
	(void)name;
}

/* Fetch an object indirect into the primary register. */
void
indirect(Lvalue *lv)
{
	if (lv->ind == CCHAR)
		ol("\tmovsx r0, byte [r0]");
	else
		ol("\tmov r0, [r0]");
}

/*
 * Write the label of a symbol in static memory: a global's name, or the
 * internal label of a static local.
 */
void
symlabel(char *sym)
{
	if (sym[CLASS] == LSTATIC) {
		printlabel((int)getint(sym + OFFSET, OFFSIZE));
	} else if (sym[PRIV]) {
		ot("._"); /* local to the file in the assembler */
		outstr(sym + NAME);
	} else {
		outlab(sym + NAME);
	}
}

/* Fetch a static memory cell into the primary register. */
void
getmem(Lvalue *lv)
{
	char *sym;

	sym = lv->sym;
	if (sym[IDENT] != POINTER && sym[TYPE] == CCHAR)
		ot("\tmovsx r0, byte [");
	else
		ot("\tmov r0, [");
	symlabel(sym);
	ol("]");
}

/*
 * Fetch the address of the given symbol into the primary register.
 * While fixsym is set, the stack offsets are not final yet: the number
 * goes in a field of FIXWIDTH characters, padded in front, so that it
 * can be changed in place, and its place is recorded unless sym is
 * fixsym itself.  See initunsized().
 */
void
getloc(char *sym)
{
	char buf[24];
	int n;

	n = (int)(getint(sym + OFFSET, OFFSIZE) - csp);
	if (fixsym == NULL || stagenext == NULL) {
		loadconst(n);
	} else {
		immed();
		if (sym != fixsym && nfix == NUMFIX)
			error("too many locals in the initializer");
		else if (sym != fixsym)
			fixpos[nfix++] = stagenext;
		sprintf(buf, "%*d", FIXWIDTH, n);
		ot(buf);
		nl();
	}
	ol(SPLINE);
}

/* Store the primary register into a static cell. */
void
putmem(Lvalue *lv)
{
	char *sym;

	sym = lv->sym;
	if (sym[IDENT] != POINTER && sym[TYPE] == CCHAR)
		ot("\tmov byte [");
	else
		ot("\tmov [");
	symlabel(sym);
	ol("], r0");
}

/* Store the primary register through the address in the secondary. */
void
putstk(Lvalue *lv)
{
	if (lv->ind == CCHAR)
		ol("\tmov byte [r1], r0");
	else
		ol("\tmov [r1], r0");
}

/* Move the primary register to the secondary. */
void
move(void)
{
	ol("\tmov r1, r0");
}

/* Swap the primary and secondary registers. */
void
swap(void)
{
	ol(SWAPLINE);
}

/* Partial instruction loading an immediate into the primary register. */
void
immed(void)
{
	ot("\tmov r0, ");
}

/* Partial instruction loading an immediate into the secondary. */
void
immed2(void)
{
	ot("\tmov r1, ");
}

/* Push the primary register onto the stack. */
void
push(void)
{
	ol(PUSHLINE);
	csp = csp - BPW;
}

/* Unpush or pop, as required. */
void
smartpop(Lvalue *lv, char *start)
{
	if (lv->sreg)
		pop(); /* the secondary register was used */
	else
		unpush(start);
}

/*
 * Replace a push already in the staging buffer with a swap, then fix up
 * the stack-relative references that follow it.  Each one is a
 * constant load followed by SPLINE, and the constant is decreased by
 * BPW in place, one decimal digit at a time.
 */
static void
unpush(char *dest)
{
	const char *sour;
	char *p;
	int i;

	sour = SWAPLINE;
	while (*sour)
		*dest++ = *sour++;
	p = stagenext;
	while (--p > dest) { /* adjust the stack references */
		if (streq(p, SPLINE)) {
			--p;
			i = BPW;
			while (isdigit((unsigned char)*(--p))) {
				if ((*p = (char)(*p - i)) < '0') {
					*p = (char)(*p + 10);
					i = 1;
				} else {
					i = 0;
				}
			}
		}
	}
	csp = csp + BPW;
}

/* Pop the stack into the secondary register. */
void
pop(void)
{
	ol("\tpop r1");
	csp = csp + BPW;
}

/*
 * Copy a struct of size bytes from the address in the primary register
 * to the address in the secondary.  The primary keeps its value.
 */
void
movestruct(int size)
{
	ot("\tmov r2, ");
	outdec(size);
	nl();
	ffcall("CCMOVE");
}

/* Clear size bytes from the address in the secondary register. */
void
zeromem(int size)
{
	ot("\tmov r2, ");
	outdec(size);
	nl();
	ffcall("CCZERO");
}

/*
 * Push a copy of the struct of size bytes at the address in the
 * primary register, rounded up to whole words.  Returns the bytes
 * pushed.
 */
int
pushstruct(int size)
{
	int n;

	n = (size + BPW - 1) / BPW * BPW;
	ot("\tsub r7, ");
	outdec(n);
	nl();
	csp = csp - n;
	ol("\tmov r1, r7");
	movestruct(size);
	return n;
}

/* Load the word off bytes above the stack pointer into the primary. */
void
loadstk(int off)
{
	ot("\tmov r0, [r7 + ");
	outdec(off);
	ol("]");
}

/*
 * Process a switch statement: tablab labels the table of case label
 * and value pairs, which doswitch() emits after this call.  CCSWITCH
 * jumps to the matching case, or returns when none matches.
 */
void
sw(int tablab)
{
	ot("\tmov r1, ");
	printlabel(tablab);
	nl();
	ffcall("CCSWITCH");
	swused = 1;
}

/* Call the named subroutine. */
void
ffcall(const char *sname)
{
	ot("\tcall ");
	outlab(sname);
	nl();
}

/* Call the function whose symbol is sym. */
void
ffcallsym(char *sym)
{
	ot("\tcall ");
	symlabel(sym);
	nl();
}

/* Return from a subroutine. */
void
ffret(void)
{
	ol("\tret");
}

/* Call the address held in the primary register. */
void
callstk(void)
{
	ol("\tcall r0");
}

/* Jump to an internal label number. */
void
jump(int label)
{
	ot("\tjmp ");
	printlabel(label);
	nl();
}

/* Test the primary register and jump if false. */
void
testjump(int label)
{
	ol("\tcmp r0, 0");
	ot("\tje ");
	printlabel(label);
	nl();
}

/* Test the primary register against zero and jump if false. */
void
zerojump(Jumper oper, int label, Lvalue *lv)
{
	clearstage(lv->stg, NULL); /* purge the conventional code */
	(*oper)(label);
}

/* Define storage according to size. */
void
defstorage(int size)
{
	if (size == 1)
		ot("\tdb ");
	else
		ot("\tdq ");
}

/* Point to the following object, which gets a label of its own. */
void
point(void)
{
	int label;

	label = getlabel();
	ot("\tdq ");
	printlabel(label);
	nl();
	postlabel(label);
}

/* Modify the stack pointer to the value given. */
int
modstk(int newsp, int save)
{
	int k;

	(void)save; /* the primary register is never disturbed */
	k = newsp - csp;
	if (k == 0)
		return newsp;
	if (k > 0) {
		ot("\tadd r7, ");
		outdec(k);
	} else {
		ot("\tsub r7, ");
		outdec(-k);
	}
	nl();
	return newsp;
}

/*
 * Scale the primary register by n, from a count of objects of n bytes
 * to a count of bytes.  A power of two scales by a shift.
 */
void
scalereg(int n)
{
	int k;

	if (n <= 1)
		return;
	for (k = 0; (1 << k) < n; ++k)
		;
	if ((1 << k) == n) {
		ot("\tshl r0, ");
		outdec(k);
	} else {
		ot("\tmul r0, ");
		outdec(n);
	}
	nl();
}
