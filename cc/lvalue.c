/*
 * Small-C compiler -- lvalues, tests and literals.
 *
 * Small C, 8088/8086 version -- modified by R. Grehan, BYTE Magazine.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "scc.h"

static int number(Word *val);
static int pstr(Word *val);
static int litchar(void);

/*
 * Return the factor that scales v2 in "v1 oper v2": the length of the
 * object v1 points to, when v1 is a pointer or array, v2 is not, and
 * oper adds or subtracts.  Otherwise the factor is 1.
 */
int
scale(Binop oper, Lvalue *v1, Lvalue *v2)
{
	if (oper != ffadd && oper != ffsub)
		return 1;
	if (v1->ptr == 0 || v2->ptr)
		return 1;
	return objsize(v1->ptr, v1->tag);
}

/* Return true if lv is a whole struct, whose address is in r0. */
int
isstructval(Lvalue *lv)
{
	return lv->ind == CSTRUCT && lv->ptr == 0;
}

/*
 * Return the number of bytes ++ and -- step lv by: the length of the
 * object it points to, or 0 for a plain integer, which steps by one.
 */
int
stepsize(Lvalue *lv)
{
	if (lv->ptr == 0)
		return 0;
	return objsize(lv->ptr, lv->tag);
}

/* Determine the type of a binary operation. */
void
result(Lvalue *lv, Lvalue *lv2)
{
	if (lv->ptr != 0 && lv2->ptr != 0) {
		/* a pointer difference is an int, not an object */
		lv->ptr = 0;
		lv->ind = 0;
		lv->tag = 0;
	} else if (lv2->ptr) {
		lv->sym = lv2->sym;
		lv->ind = lv2->ind;
		lv->ptr = lv2->ptr;
		lv->tag = lv2->tag;
	}
}

void
step(Stepop oper, Lvalue *lv)
{
	if (lv->ind) {
		if (lv->sreg) {
			push();
			rvalue(lv);
			(*oper)(stepsize(lv));
			pop();
			store(lv);
			return;
		}
		move();
		lv->sreg = 1;
	}
	rvalue(lv);
	(*oper)(stepsize(lv));
	store(lv);
}

void
store(Lvalue *lv)
{
	if (lv->ind == CSTRUCT)
		movestruct(objsize(CSTRUCT, lv->tag)); /* from r0 to r1 */
	else if (lv->ind)
		putstk(lv);
	else
		putmem(lv);
}

void
rvalue(Lvalue *lv)
{
	if (lv->ind == CSTRUCT)
		error("struct used as a value");
	else if (lv->sym != NULL && lv->ind == 0)
		getmem(lv);
	else
		indirect(lv);
}

/* Compile a test, branching to label when the expression is false. */
void
test(int label, int parens)
{
	Lvalue lv = {0};
	Binop oper;
	char *before, *start;

	before = start = NULL;
	if (parens)
		needtoken("(");
	for (;;) {
		setstage(&before, &start);
		if (hier1(&lv))
			rvalue(&lv);
		else if (isstructval(&lv))
			error("struct used as a value");
		if (match(","))
			clearstage(before, start);
		else
			break;
	}
	if (parens)
		needtoken(")");
	if (lv.isconst) { /* constant expression */
		clearstage(before, NULL);
		if (lv.val)
			return;
		jump(label);
		return;
	}
	if (lv.stg) {           /* stage address of the "oper 0" code */
		oper = lv.oper; /* the operator applied */
		if (oper == ffeq || oper == ule)
			zerojump(eq0, label, &lv);
		else if (oper == ffne || oper == ugt)
			zerojump(ne0, label, &lv);
		else if (oper == ffgt)
			zerojump(gt0, label, &lv);
		else if (oper == ffge)
			zerojump(ge0, label, &lv);
		else if (oper == uge)
			clearstage(lv.stg, NULL);
		else if (oper == fflt)
			zerojump(lt0, label, &lv);
		else if (oper == ult)
			zerojump(ult0, label, &lv);
		else if (oper == ffle)
			zerojump(le0, label, &lv);
		else
			testjump(label);
	} else {
		testjump(label);
	}
	clearstage(before, start);
}

int
constexpr(Word *val)
{
	char *before, *start;
	int isconst;

	setstage(&before, &start);
	expression(&isconst, val);
	clearstage(before, NULL); /* scratch the generated code */
	if (isconst == 0)
		error("must be constant expression");
	return isconst;
}

/* Load a constant into the primary register. */
void
loadconst(Word val)
{
	immed();
	outdec(val);
	nl();
}

/* Load a constant into the secondary register. */
void
loadconst2(Word val)
{
	immed2();
	outdec(val);
	nl();
}

int
constant(Lvalue *lv)
{
	lv->isconst = 1; /* assume it will be a constant */
	if (number(&lv->val)) {
		immed();
	} else if (pstr(&lv->val)) {
		immed();
	} else if (qstr(&lv->val)) {
		lv->isconst = 0; /* no: it is a string address */
		immed();
		printlabel(litlab);
		ot(" + ");
	} else {
		return 0;
	}
	outdec(lv->val);
	nl();
	return 1;
}

static int
number(Word *val)
{
	unsigned long long k;
	int minus;

	k = minus = 0;
	for (;;) {
		if (match("+"))
			;
		else if (match("-"))
			minus = 1;
		else
			break;
	}
	if (isdigit((unsigned char)cch) == 0)
		return 0;
	while (isdigit((unsigned char)cch))
		k = k * 10 + (unsigned long long)(inbyte() - '0');
	if (minus)
		k = 0 - k;
	*val = (Word)k;
	return 1;
}

void
address(char *ptr)
{
	immed();
	symlabel(ptr);
	nl();
}

/* Parse a character constant, packing up to two characters. */
static int
pstr(Word *val)
{
	int k;

	k = 0;
	if (match("'") == 0)
		return 0;
	while (cch != '\'') {
		if (cch == 0)
			break;
		k = (k & 255) * 256 + (litchar() & 255);
	}
	gch();
	*val = k;
	return 1;
}

/* Parse a quoted string into the literal pool. */
int
qstr(Word *val)
{
	if (match(quote) == 0)
		return 0;
	*val = litptr;
	while (cch != '"') {
		if (cch == 0)
			break;
		stowlit(litchar(), 1);
	}
	gch();
	litq[litptr++] = 0;
	return 1;
}

void
stowlit(Word value, int size)
{
	if ((litptr + size) >= LITMAX) {
		error("literal queue overflow");
		exit(ERRCODE);
	}
	putint(value, litq + litptr, size);
	litptr = litptr + size;
}

/* Return the current literal character and advance lptr. */
static int
litchar(void)
{
	int i, oct;

	if (cch != '\\' || nch == 0)
		return gch();
	gch();
	if (cch == 'n') {
		gch();
		return NEWLINE;
	}
	if (cch == 't') {
		gch();
		return 9; /* HT */
	}
	if (cch == 'b') {
		gch();
		return 8; /* BS */
	}
	if (cch == 'f') {
		gch();
		return 12; /* FF */
	}
	i = 3;
	oct = 0;
	while (i-- > 0 && cch >= '0' && cch <= '7')
		oct = (oct << 3) + gch() - '0';
	if (i == 2)
		return gch();
	return oct;
}
