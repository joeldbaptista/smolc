/*
 * Small-C compiler -- expression parser.
 *
 * Small C, 8088/8086 version -- modified by R. Grehan, BYTE Magazine.
 *
 * Each hier function parses one precedence level and returns non-zero
 * when the result is an lvalue.  See struct lvalue in scc.h.
 */

#include <stdio.h>
#include <string.h>

#include "scc.h"

static int skim(const char *opstr, Jumper testfunc, int dropval,
                int endval, Hier hier, Lvalue *lv);
static void dropout(int k, Jumper testfunc, int exit1, Lvalue *lv);
static int plnge(const char *opstr, int opoff, Hier hier, Lvalue *lv);
static int plnge1(Hier hier, Lvalue *lv);
static Word calc(Word left, Binop oper, Word right);
static int hier2(Lvalue *lv);
static void loadval(Lvalue *lv, int k);
static int hier3(Lvalue *lv);
static int hier4(Lvalue *lv);
static int hier5(Lvalue *lv);
static int hier6(Lvalue *lv);
static int hier7(Lvalue *lv);
static int hier8(Lvalue *lv);
static int hier9(Lvalue *lv);
static int hier10(Lvalue *lv);
static int hier11(Lvalue *lv);
static int hier12(Lvalue *lv);

/* Skim over the terms adjoining || and && operators. */
static int
skim(const char *opstr, Jumper testfunc, int dropval, int endval,
     Hier hier, Lvalue *lv)
{
	int droplab, endlab, hits, k;

	droplab = 0;
	hits = 0;
	for (;;) {
		k = plnge1(hier, lv);
		if (nextop(opstr)) {
			bump(opsize);
			if (hits == 0) {
				hits = 1;
				droplab = getlabel();
			}
			dropout(k, testfunc, droplab, lv);
		} else if (hits) {
			dropout(k, testfunc, droplab, lv);
			loadconst(endval);
			jump(endlab = getlabel());
			postlabel(droplab);
			loadconst(dropval);
			postlabel(endlab);
			lv->ind = 0;
			lv->ptr = 0;
			lv->isconst = 0;
			lv->val = 0;
			lv->stg = NULL;
			return 0;
		} else {
			return k;
		}
	}
}

/* Test for an early dropout from a || or && evaluation. */
static void
dropout(int k, Jumper testfunc, int exit1, Lvalue *lv)
{
	if (k)
		rvalue(lv);
	else if (lv->isconst)
		loadconst(lv->val);
	else if (isstructval(lv))
		error("struct used as a value");
	(*testfunc)(exit1); /* jumps on false */
}

/* Plunge to a lower level. */
static int
plnge(const char *opstr, int opoff, Hier hier, Lvalue *lv)
{
	Lvalue lv2 = {0};
	int k;

	k = plnge1(hier, lv);
	if (nextop(opstr) == 0)
		return k;
	if (k)
		rvalue(lv);
	for (;;) {
		if (nextop(opstr)) {
			bump(opsize);
			opindex = opindex + opoff;
			plnge2(op[opindex], op2[opindex], hier, lv, &lv2);
		} else {
			return 0;
		}
	}
}

/* Unary plunge to a lower level. */
static int
plnge1(Hier hier, Lvalue *lv)
{
	char *before, *start;
	int k;

	setstage(&before, &start);
	k = (*hier)(lv);
	if (lv->isconst)
		clearstage(before, NULL); /* load the constant later */
	return k;
}

/* Binary plunge to a lower level. */
void
plnge2(Binop oper, Binop oper2, Hier hier, Lvalue *lv, Lvalue *lv2)
{
	char *before, *start;
	int n;

	if (oper && isstructval(lv))
		error("struct used as a value");
	setstage(&before, &start);
	lv->sreg = 1;      /* flag the secondary register as used */
	lv->stg = NULL;    /* flag as not "... oper 0" syntax */
	if (lv->isconst) { /* constant on the left, not yet loaded */
		if (plnge1(hier, lv2))
			rvalue(lv2);
		else if (isstructval(lv2))
			error("struct used as a value");
		if (lv->val == 0)
			lv->stg = stagenext;
		loadconst2(lv->val * scale(oper, lv2, lv));
	} else { /* non-constant on the left */
		push();
		if (plnge1(hier, lv2))
			rvalue(lv2);
		else if (isstructval(lv2))
			error("struct used as a value");
		if (lv2->isconst) { /* constant on the right */
			if (lv2->val == 0)
				lv->stg = start;
			/* other commutative operators could join this */
			if (oper == ffadd) {
				csp = csp + BPW;
				clearstage(before, NULL);
				/* load the secondary register */
				loadconst2(lv2->val * scale(oper, lv, lv2));
			} else {
				/* load the primary register */
				loadconst(lv2->val * scale(oper, lv, lv2));
				smartpop(lv2, start);
			}
		} else { /* non-constants on both sides */
			smartpop(lv2, start);
			scalereg(scale(oper, lv, lv2));
			if ((n = scale(oper, lv2, lv)) > 1) {
				swap();
				scalereg(n);
				if (oper == ffsub)
					swap();
			}
		}
	}
	if (oper) {
		if ((lv->isconst = lv->isconst && lv2->isconst)) {
			lv->val = calc(lv->val, oper, lv2->val);
			clearstage(before, NULL);
			lv->sreg = 0;
		} else if (lv->ptr == 0 && lv2->ptr == 0) {
			(*oper)();
			lv->oper = oper; /* identify the operator */
		} else {
			(*oper2)();
			lv->oper = oper2; /* identify the operator */
		}
		/* a pointer difference counts objects, not bytes */
		if (oper == ffsub && lv->ptr && lv2->ptr &&
		    (n = objsize(lv->ptr, lv->tag)) > 1) {
			swap();
			loadconst(n);
			ffdiv();
		}
		if (oper == ffsub || oper == ffadd)
			result(lv, lv2);
	}
}

/*
 * Fold a binary operator over two constants, with the results the smol
 * machine gives: arithmetic wraps, and shift counts are taken mod 64.
 */
static Word
calc(Word left, Binop oper, Word right)
{
	unsigned long long l, r;

	l = (unsigned long long)left;
	r = (unsigned long long)right;
	if (oper == ffor)
		return left | right;
	else if (oper == ffxor)
		return left ^ right;
	else if (oper == ffand)
		return left & right;
	else if (oper == ffeq)
		return left == right;
	else if (oper == ffne)
		return left != right;
	else if (oper == ffle)
		return left <= right;
	else if (oper == ffge)
		return left >= right;
	else if (oper == fflt)
		return left < right;
	else if (oper == ffgt)
		return left > right;
	else if (oper == ffasr)
		return left >> (right & 63);
	else if (oper == ffasl)
		return (Word)(l << (r & 63));
	else if (oper == ffadd)
		return (Word)(l + r);
	else if (oper == ffsub)
		return (Word)(l - r);
	else if (oper == ffmult)
		return (Word)(l * r);
	else if (oper == ffdiv)
		return right == -1 ? (Word)(0 - l) : right ? left / right : 0;
	else if (oper == ffmod)
		return right == -1 ? 0 : right ? left % right : 0;
	else
		return 0;
}

void
expression(int *isconst, Word *val)
{
	Lvalue lv = {0};

	if (hier1(&lv))
		rvalue(&lv);
	if (lv.isconst) {
		*isconst = 1;
		*val = lv.val;
	} else {
		*isconst = 0;
	}
}

int
hier1(Lvalue *lv)
{
	Lvalue lv2 = {0}, lv3 = {0};
	Binop opr;
	int k;

	k = plnge1(hier2, lv);
	if (lv->isconst)
		loadconst(lv->val);
	if (match("|="))
		opr = ffor;
	else if (match("^="))
		opr = ffxor;
	else if (match("&="))
		opr = ffand;
	else if (match("+="))
		opr = ffadd;
	else if (match("-="))
		opr = ffsub;
	else if (match("*="))
		opr = ffmult;
	else if (match("/="))
		opr = ffdiv;
	else if (match("%="))
		opr = ffmod;
	else if (match(">>="))
		opr = ffasr;
	else if (match("<<="))
		opr = ffasl;
	else if (match("="))
		opr = NULL;
	else
		return k;
	if (k == 0) {
		needlval();
		return 0;
	}
	lv3.sym = lv->sym;
	lv3.ind = lv->ind;
	if (isstructval(lv)) {
		/* copy the whole struct; the value is the source's address */
		if (opr) {
			error("struct used as a value");
			return 0;
		}
		push(); /* the destination's address */
		hier1(&lv2);
		if (isstructval(&lv2) == 0 || lv2.tag != lv->tag)
			error("struct types differ");
		pop();
		lv3.tag = lv->tag;
		store(&lv3);
		return 0;
	}
	if (lv->ind) {
		if (opr) {
			push();
			rvalue(lv);
		}
		plnge2(opr, opr, hier1, lv, &lv2);
		if (opr)
			pop();
	} else {
		if (opr) {
			rvalue(lv);
			plnge2(opr, opr, hier1, lv, &lv2);
		} else {
			if (hier1(&lv2))
				rvalue(&lv2);
			else if (isstructval(&lv2))
				error("struct used as a value");
			lv->sreg = lv2.sreg;
		}
	}
	store(&lv3);
	return 0;
}

/*
 * The conditional operator, "c ? a : b".  The value of a or b ends up
 * in the primary register, and the result takes the type of whichever
 * of them is a pointer or a struct.
 */
static int
hier2(Lvalue *lv)
{
	Lvalue lv2 = {0}, lv3 = {0};
	int endlab, flab, k;

	k = plnge1(hier3, lv);
	if (match("?") == 0)
		return k;
	loadval(lv, k);
	testjump(flab = getlabel()); /* jumps when c is false */
	k = hier1(&lv2);
	lv2.isconst = 0; /* hier1() loads a constant itself */
	loadval(&lv2, k);
	jump(endlab = getlabel());
	postlabel(flab);
	needtoken(":");
	loadval(&lv3, hier2(&lv3));
	postlabel(endlab);
	if (lv3.ptr || isstructval(&lv3))
		lv2 = lv3;
	memset(lv, 0, sizeof(*lv));
	lv->sreg = 1; /* the branches may have used the secondary */
	lv->ptr = lv2.ptr;
	lv->tag = lv2.tag;
	if (isstructval(&lv2))
		lv->ind = CSTRUCT;
	return 0;
}

/*
 * Bring the value of lv into the primary register: k says lv is an
 * lvalue.  A constant may still be unloaded, and a struct's value is
 * its address, which is there already.
 */
static void
loadval(Lvalue *lv, int k)
{
	if (isstructval(lv))
		return;
	if (k)
		rvalue(lv);
	else if (lv->isconst)
		loadconst(lv->val);
}

static int
hier3(Lvalue *lv)
{
	return skim("||", eq0, 1, 0, hier4, lv);
}

static int
hier4(Lvalue *lv)
{
	return skim("&&", ne0, 0, 1, hier5, lv);
}

static int
hier5(Lvalue *lv)
{
	return plnge("|", 0, hier6, lv);
}

static int
hier6(Lvalue *lv)
{
	return plnge("^", 1, hier7, lv);
}

static int
hier7(Lvalue *lv)
{
	return plnge("&", 2, hier8, lv);
}

static int
hier8(Lvalue *lv)
{
	return plnge("== !=", 3, hier9, lv);
}

static int
hier9(Lvalue *lv)
{
	return plnge("<= >= < >", 5, hier10, lv);
}

static int
hier10(Lvalue *lv)
{
	return plnge(">> <<", 9, hier11, lv);
}

static int
hier11(Lvalue *lv)
{
	return plnge("+ -", 11, hier12, lv);
}

static int
hier12(Lvalue *lv)
{
	return plnge("* / %", 13, hier13, lv);
}
