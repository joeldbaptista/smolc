/*
 * Small-C compiler -- type specifiers, declarators and typedefs.
 *
 * Every declaration reads a type specifier with typespec() and then
 * its declarators with declarator(), which together give each name its
 * kind (variable, pointer or array) and type.  A typedef name is a type
 * specifier that may stand for a pointer, an array or a function
 * pointer, so declarator() combines it with what the declaration adds.
 *
 * The original Small-C pointers have one level, and its arrays one
 * dimension.  A typedef cannot take a declaration past those.
 */

#include <stdio.h>
#include <string.h>

#include "cc.h"

typedef struct tdef Tdef;
struct tdef {
	char name[NAMESIZE];
	Tspec ts;
};

static Tdef *findtdef(const char *name);
static int peektdef(Tspec *ts);
static void plain(Tspec *ts, int type, int tag);

/*
 * Typedef names, visible from their declaration to the end of the
 * file, wherever they are declared.
 */
static Tdef tdefs[NUMTDEFS];
static int ntdefs;

/* Return the typedef name, else NULL. */
static Tdef *
findtdef(const char *name)
{
	int i;

	for (i = 0; i < ntdefs; ++i)
		if (strcmp(tdefs[i].name, name) == 0)
			return &tdefs[i];
	return NULL;
}

/*
 * If a typedef name comes next, consume it and copy its type to ts, if
 * ts is not NULL.  Returns true if there was one.  Anything else is
 * left unread.
 */
static int
peektdef(Tspec *ts)
{
	char name[NAMESIZE];
	char *save;
	Tdef *td;

	blanks();
	save = lptr;
	if (symname(name, YES) == 0)
		return 0;
	if ((td = findtdef(name)) == NULL) {
		bump((int)(save - lptr)); /* nonzero: a name was read */
		return 0;
	}
	if (ts)
		*ts = td->ts;
	else
		bump((int)(save - lptr));
	return 1;
}

/* Set ts to the plain type, and tag if a struct. */
static void
plain(Tspec *ts, int type, int tag)
{
	ts->type = type;
	ts->tag = tag;
	ts->ident = VARIABLE;
	ts->dim = 0;
	ts->fptr = 0;
}

/*
 * Parse a type specifier: char, int, void, struct, union or a typedef
 * name.  Returns false, consuming nothing, if none comes next.
 */
int
typespec(Tspec *ts)
{
	int kw;

	if (amatch("char", 4))
		plain(ts, CCHAR, 0);
	else if (amatch("int", 3))
		plain(ts, CINT, 0);
	else if (amatch("void", 4))
		plain(ts, CINT, 0); /* void has no size; treat it as a word */
	else if ((kw = structkw()))
		plain(ts, CSTRUCT, dostruct(kw));
	else
		return peektdef(ts);
	return 1;
}

/* Return true if a type specifier comes next.  Nothing is consumed. */
int
istypename(void)
{
	blanks();
	return astreq(lptr, "char", 4) || astreq(lptr, "int", 3) ||
	       astreq(lptr, "void", 4) || astreq(lptr, "struct", 6) ||
	       astreq(lptr, "union", 5) || peektdef(NULL);
}

/*
 * Parse a declarator for the type specifier ts: "name", "*name",
 * "name[n]", "(*name)(...)" or "(*name[n])(...)".  The name may be
 * left out, as in a prototype or sizeof.  A function's parameter list
 * after the name is left for the caller.  Sets ssname, and d.  Returns
 * true if a name was given.
 */
int
declarator(Tspec *ts, Decl *d)
{
	int dim, named, ptrs;

	ptrs = match("*");
	d->type = ts->type;
	d->tag = ts->tag;
	d->fptr = ts->fptr;
	d->dim = 0;
	d->isfp = 0;
	if (match("(*")) {
		/* a function pointer, returning what ts and the star say */
		named = fpdecl(&dim);
		if (ts->type == CFUNC || ts->ident == ARRAY ||
		    (ts->ident == POINTER && ptrs))
			error("unsupported return type");
		d->fptr = ts->type |
		          (ptrs || ts->ident == POINTER ? RETPTR : 0);
		d->type = CFUNC;
		d->ident = dim >= 0 ? ARRAY : VARIABLE;
		d->dim = dim >= 0 ? dim : 0;
		d->isfp = 1;
		return named;
	}
	if ((named = symname(ssname, YES)) == 0)
		*ssname = 0;
	d->ident = ptrs ? POINTER : VARIABLE;
	if (match("[")) {
		if (ptrs)
			error("no pointer arrays");
		d->dim = needsub();
		d->ident = ARRAY;
	}
	/* what a typedef name stands for */
	if (ts->ident == POINTER) {
		if (d->ident != VARIABLE)
			error("no pointers to pointers or arrays of pointers");
		d->ident = POINTER;
	} else if (ts->ident == ARRAY) {
		if (d->ident != VARIABLE)
			error("no pointers to arrays or arrays of arrays");
		d->ident = ARRAY;
		d->dim = ts->dim;
	}
	return named;
}

/*
 * Declare typedef names, after "typedef": each declarator's name now
 * stands for the type it would give a variable.
 */
void
dotypedef(void)
{
	Decl d;
	Tdef *td;
	Tspec ts;

	if (typespec(&ts) == 0) {
		error("missing type");
		junk();
		return;
	}
	for (;;) {
		if (endst())
			return;
		if (declarator(&ts, &d) == 0) {
			illname();
			return;
		}
		if (d.ident == ARRAY && d.dim == 0)
			error("typedef array needs a length");
		if ((td = findtdef(ssname)) == NULL) {
			if (ntdefs == NUMTDEFS) {
				error("too many typedefs");
				return;
			}
			td = &tdefs[ntdefs++];
			strcpy(td->name, ssname);
		} else {
			multidef(ssname);
		}
		td->ts.type = d.type;
		td->ts.tag = d.tag;
		td->ts.ident = d.ident;
		td->ts.dim = d.dim;
		td->ts.fptr = d.fptr;
		if (match(",") == 0)
			return;
	}
}
