/*
 * Small-C compiler -- unary operators, subscripts and calls.
 *
 * Small C, 8088/8086 version -- modified by R. Grehan, BYTE Magazine.
 * Retargeted to the smol machine for smolc, with structs and sizeof.
 */

#include <stdio.h>
#include <string.h>

#include "scc.h"

static int dosizeof(Lvalue *lv);
static int exprsize(Lvalue *lv, int k);
static int hier14(Lvalue *lv);
static int member(Lvalue *lv, int k, int arrow);
static void calltype(Lvalue *lv, int type, int tag, int isptr);
static int primary(Lvalue *lv);
static int statref(char *ptr, Lvalue *lv);
static void experr(void);
static void callfunction(char *ptr);

int
hier13(Lvalue *lv)
{
	char *ptr;
	int k;

	if (match("++")) { /* ++lval */
		if (hier13(lv) == 0) {
			needlval();
			return 0;
		}
		step(inc, lv);
		return 0;
	} else if (match("--")) { /* --lval */
		if (hier13(lv) == 0) {
			needlval();
			return 0;
		}
		step(dec, lv);
		return 0;
	} else if (match("~")) { /* ~ */
		if (hier13(lv))
			rvalue(lv);
		else if (isstructval(lv))
			error("struct used as a value");
		com();
		lv->val = ~lv->val;
		lv->stg = NULL;
		return 0;
	} else if (match("!")) { /* ! */
		if (hier13(lv))
			rvalue(lv);
		else if (isstructval(lv))
			error("struct used as a value");
		lneg();
		lv->val = !lv->val;
		lv->stg = NULL;
		return 0;
	} else if (match("-")) { /* unary - */
		if (hier13(lv))
			rvalue(lv);
		else if (isstructval(lv))
			error("struct used as a value");
		neg();
		lv->val = -lv->val;
		lv->stg = NULL;
		return 0;
	} else if (match("*")) { /* unary * */
		if (hier13(lv))
			rvalue(lv);
		if (lv->ptr) /* the type the pointer expression points to */
			lv->ind = lv->ptr;
		else if ((ptr = lv->sym))
			lv->ind = ptr[TYPE];
		else
			lv->ind = CINT;
		lv->ptr = 0;     /* not a pointer or array */
		lv->isconst = 0; /* not a constant */
		lv->val = 1;     /* omit rvalue() on a call */
		lv->stg = NULL;
		return 1;
	} else if (match("&")) { /* unary & */
		if (hier13(lv) == 0) {
			/* &f is f: a function's name is already its address */
			if (lv->sym && lv->sym[IDENT] == FUNCTION)
				return 0;
			error("illegal address");
			return 0;
		}
		if ((ptr = lv->sym) == NULL) {
			error("illegal address");
			return 0;
		}
		lv->ptr = ptr[TYPE];
		lv->tag = ptr[TAG];
		if (lv->ind)
			return 0;
		/* global and non-array */
		address(ptr);
		lv->ind = ptr[TYPE];
		return 0;
	} else if (amatch("sizeof", 6)) {
		return dosizeof(lv);
	} else {
		k = hier14(lv);
		if (match("++")) { /* lval++ */
			if (k == 0) {
				needlval();
				return 0;
			}
			step(inc, lv);
			dec(stepsize(lv));
			return 0;
		} else if (match("--")) { /* lval-- */
			if (k == 0) {
				needlval();
				return 0;
			}
			step(dec, lv);
			inc(stepsize(lv));
			return 0;
		}
		return k;
	}
}

/*
 * sizeof, as "sizeof (type)" or "sizeof expression".  The expression
 * is parsed only for its type: the code it generates is discarded.
 * The result is a constant.
 */
static int
dosizeof(Lvalue *lv)
{
	char *before, *start;
	int k, n, paren, size;

	paren = match("(");
	if (paren == 0 || (size = typesize()) < 0) {
		n = nfix; /* the code is discarded, and so are its fields */
		setstage(&before, &start);
		if (paren)
			k = hier1(lv);
		else
			k = hier13(lv);
		size = exprsize(lv, k);
		clearstage(before, NULL);
		nfix = n;
	}
	if (paren)
		needtoken(")");
	memset(lv, 0, sizeof(*lv));
	lv->isconst = 1;
	lv->val = size;
	loadconst(size); /* as constant() does */
	return 0;
}

/* Return the length of the object an expression designates. */
static int
exprsize(Lvalue *lv, int k)
{
	char *sym;

	sym = lv->sym;
	if (isstructval(lv))
		return objsize(CSTRUCT, lv->tag);
	if (k == 0 && lv->ptr && sym && sym[IDENT] == ARRAY) {
		if (sym[CLASS] == MEMBER)
			return memblen(sym) * objsize(sym[TYPE], sym[TAG]);
		if (getint(sym + DIM, DIMSIZE) == 0) {
			error("sizeof needs the array's length");
			return BPW;
		}
		return (int)getint(sym + DIM, DIMSIZE) *
		       objsize(sym[TYPE], sym[TAG]);
	}
	if (k == 0)
		return BPW; /* a value: an int or an address */
	if (lv->ind)
		return objsize(lv->ind, lv->tag);
	if (sym[IDENT] == POINTER)
		return BPW;
	return objsize(sym[TYPE], sym[TAG]); /* a global, reached by name */
}

static int
hier14(Lvalue *lv)
{
	Lvalue lv2 = {0};
	char *before, *ptr, *start;
	int k, n, tag, type;

	k = primary(lv);
	ptr = lv->sym;
	blanks();
	if (cch == '[' || cch == '(' || cch == '.' ||
	    (cch == '-' && nch == '>')) {
		lv->sreg = 1; /* the secondary register will be used */
		for (;;) {
			ptr = lv->sym; /* a member access replaces it */
			if (match("[")) { /* [subscript] */
				/*
				 * The base is an array, a pointer variable, or
				 * any pointer value, such as a call's result,
				 * which is already in the primary register.
				 */
				if (ptr && ptr[IDENT] == ARRAY) {
					type = ptr[TYPE];
					tag = ptr[TAG];
				} else if (ptr && ptr[IDENT] == POINTER && k) {
					rvalue(lv);
					type = ptr[TYPE];
					tag = ptr[TAG];
				} else if (lv->ptr) {
					if (k)
						rvalue(lv);
					type = lv->ptr;
					tag = lv->tag;
				} else {
					error("can't subscript");
					junk();
					needtoken("]");
					return 0;
				}
				n = objsize(type, tag);
				setstage(&before, &start);
				lv2.isconst = 0;
				/* lv2 is a deadend on both sides */
				plnge2(NULL, NULL, hier1, &lv2, &lv2);
				needtoken("]");
				if (lv2.isconst) {
					clearstage(before, NULL);
					if (lv2.val) {
						loadconst2(lv2.val * n);
						ffadd();
					}
				} else {
					scalereg(n);
					ffadd();
				}
				lv->ptr = 0;
				lv->ind = type;
				lv->tag = tag;
				k = 1;
			} else if (match("(")) { /* function(...) */
				if (ptr == NULL) {
					callfunction(NULL);
				} else if (ptr[IDENT] != FUNCTION) {
					if (k && !lv->val)
						rvalue(lv);
					callfunction(NULL);
				} else {
					callfunction(ptr);
				}
				lv->sym = NULL;
				lv->isconst = 0;
				lv->val = 0;
				lv->ind = lv->ptr = lv->tag = 0;
				/* type the result as declared */
				if (ptr && ptr[IDENT] == FUNCTION)
					calltype(lv, ptr[TYPE], ptr[TAG],
					         ptr[FPTR]);
				else if (ptr && ptr[TYPE] == CFUNC)
					calltype(lv, ptr[FPTR] & ~RETPTR,
					         ptr[TAG], ptr[FPTR] & RETPTR);
				k = 0;
			} else if (match("->")) {
				k = member(lv, k, YES);
			} else if (match(".")) {
				k = member(lv, k, NO);
			} else {
				return k;
			}
		}
	}
	if (ptr == NULL)
		return k;
	if (ptr[IDENT] == FUNCTION) {
		/* its value is its address; the symbol keeps its type */
		address(ptr);
		return 0;
	}
	return k;
}

/*
 * Type lv as the result of a call to a function that returns type,
 * and tag if a struct, or a pointer to them if isptr is true.
 */
static void
calltype(Lvalue *lv, int type, int tag, int isptr)
{
	lv->tag = tag;
	if (isptr)
		lv->ptr = type;
	else if (type == CSTRUCT)
		lv->ind = CSTRUCT; /* r0 holds its address */
}

/*
 * Apply ".name" or, when arrow is true, "->name" to lv, which k says
 * is an lvalue.  The member's address ends up in the primary register,
 * and lv describes the member.  Returns true if the member is an
 * lvalue, which every member but an array is.
 */
static int
member(Lvalue *lv, int k, int arrow)
{
	char name[NAMESIZE];
	char *m;
	int off;

	if (arrow) {
		if (lv->ptr != CSTRUCT) {
			error("not a struct pointer");
			return 0;
		}
		if (k)
			rvalue(lv); /* the pointer's value is the address */
	} else if (isstructval(lv) == 0) { /* an lvalue, or a returned struct */
		error("not a struct");
		return 0;
	}
	if (symname(name, YES) == 0) {
		error("missing member name");
		return 0;
	}
	if ((m = findmemb(lv->tag, name)) == NULL) {
		error("not a member");
		return 0;
	}
	if ((off = getint(m + OFFSET, OFFSIZE)))
		inc(off);
	lv->sym = m;
	lv->tag = m[TAG];
	lv->isconst = 0;
	lv->val = 0;
	lv->stg = NULL;
	if (m[IDENT] == ARRAY) {
		lv->ind = lv->ptr = m[TYPE];
		return 0;
	}
	if (m[IDENT] == POINTER) {
		lv->ind = CINT;
		lv->ptr = m[TYPE];
		return 1;
	}
	lv->ind = m[TYPE];
	lv->ptr = 0;
	return 1;
}

static int
primary(Lvalue *lv)
{
	char sname[NAMESIZE];
	char *ptr;
	int k;

	if (match("(")) { /* (expression,...) */
		do
			k = hier1(lv);
		while (match(","));
		needtoken(")");
		return k;
	}
	memset(lv, 0, sizeof(*lv)); /* clear the lvalue */
	if (symname(sname, YES)) {
		if ((ptr = findloc(sname))) {
			if (ptr[IDENT] == LABEL) {
				experr();
				return 0;
			}
			if (ptr[CLASS] == LSTATIC)
				return statref(ptr, lv);
			getloc(ptr);
			lv->sym = ptr;
			lv->tag = ptr[TAG];
			lv->ind = ptr[TYPE];
			if (ptr[IDENT] == POINTER) {
				lv->ind = CINT;
				lv->ptr = ptr[TYPE];
			}
			if (ptr[IDENT] == ARRAY) {
				lv->ptr = ptr[TYPE];
				return 0;
			}
			return 1;
		}
		if ((ptr = findglb(sname)) && ptr[IDENT] != FUNCTION)
			return statref(ptr, lv);
		ptr = addsym(sname, FUNCTION, CINT, 0, &glbptr, AUTOEXT, 0);
		lv->sym = ptr;
		lv->ind = 0;
		return 0;
	}
	if (constant(lv) == 0)
		experr();
	return 0;
}

/*
 * Set up lv for the object ptr in static memory, a global or a static
 * local.  It is reached by its label.  Returns true for an lvalue.
 */
static int
statref(char *ptr, Lvalue *lv)
{
	lv->sym = ptr;
	lv->tag = ptr[TAG];
	lv->ind = 0;
	if (ptr[IDENT] == VARIABLE && ptr[TYPE] == CSTRUCT) {
		/* reached by address, like a local */
		address(ptr);
		lv->ind = CSTRUCT;
		return 1;
	}
	if (ptr[IDENT] != ARRAY) {
		if (ptr[IDENT] == POINTER)
			lv->ptr = ptr[TYPE];
		return 1;
	}
	address(ptr);
	lv->ind = lv->ptr = ptr[TYPE];
	return 0;
}

static void
experr(void)
{
	error("invalid expression");
	loadconst(0);
	junk();
}

/*
 * Compile a call.  ptr is the symbol table entry, or NULL for a call
 * through the address in the primary register.  That address is pushed
 * first, below the arguments, and fetched back for the call.  A struct
 * argument is pushed as a copy of the whole struct.
 */
static void
callfunction(char *ptr)
{
	Lvalue lv;
	int k, nargs;

	nargs = 0;
	if (ptr == NULL)
		push();
	blanks(); /* the open paren was already seen */
	while (streq(lptr, ")") == 0) {
		if (endst())
			break;
		memset(&lv, 0, sizeof(lv));
		k = hier1(&lv);
		if (isstructval(&lv)) {
			nargs += pushstruct(objsize(CSTRUCT, lv.tag));
		} else {
			if (k)
				rvalue(&lv);
			push();
			nargs = nargs + BPW;
		}
		if (match(",") == 0)
			break;
	}
	needtoken(")");
	if (ptr == NULL || streq(ptr + NAME, "CCARGC") == 0)
		loadargc(nargs >> LBPW);
	if (ptr) {
		ffcallsym(ptr);
	} else {
		loadstk(nargs);
		callstk();
		nargs = nargs + BPW;
	}
	csp = modstk(csp + nargs, YES);
}
