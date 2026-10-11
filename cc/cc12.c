/*
 * Small-C compiler -- declarations.
 *
 * Small C, 8088/8086 version -- modified by R. Grehan, BYTE Magazine.
 * Retargeted to the smol machine for smolc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cc.h"

static int declglb(Tspec *ts, int class);
static int defstatic(int type, int tag, int ident, int *k);
static void deflabel(const char *name);
static void skipparams(void);
static void keepstatic(const char *s);
static void dumpstatic(void);
static int findtag(const char *name);
static void addmemb(int tag, const char *name, int ident, int type,
                    int mtag, int offset, int dim, int fptr);
static int roundup(int n, int align);
static int dofuncdecl(int class, int type, int tag, int isptr);
static int doparams(void);
static void docparams(void);
static void doknrnames(void);
static void doknrtypes(void);
static void knrlayout(void);
static int knrsize(char *p);
static void fixargs(void);
static int istype(void);
static void regfunc(int type, int tag, int isptr);
static void startfunc(void);
static void dobody(void);
static int initials(int size, int ident, int dim, int *len);
static void init(int size, int ident, int *dim);
static void doargs(Tspec *ts);
static int initglobal(int type, int tag, int ident, int dim);
static void initlocal(char *sym, int ident, int type, int tag, int dim,
                      int strv);
static void initunsized(char *sym, int type, int tag);
static void patchnum(char *p, int v);
static void addnum(char *p, int v);
static void skipinit(void);
static void initval(int type, int tag, int ident, int dim, int off);
static void initarr(int type, int tag, int *dim, int off);
static void initagg(int tag, int off);
static void initscalar(int type, int ident, int off);
static void initstr(int v, int *dim, int off);
static void initptrstr(int v, int off);
static void initcopy(int tag, int off);
static int initfunc(int off);
static int structexpr(void);
static int atend(void);
static int initfollows(void);
static void locaddr(int off);
static void litaddr(int v);
static void ensure(int n);
static void putbuf(int off, int size, Word v);
static void emitbuf(int n);
static void dumpbytes(const char *p, int n);
static void dumpzeros(int n);

/* the data of the current function's static locals, for dumpstatic() */
static char *statdata;
static size_t statlen, statcap;

/* the chain and array length of each struct member, by index */
static int membnext[NUMMEMB];
static int membdim[NUMMEMB];

/* the initializer being parsed */
static int ilocal;  /* true for a local, which gets code, not data */
static char *isym;  /* the local */
static char *ibuf;  /* the global's image */
static int ibufsz;  /* bytes allocated at ibuf */
static int ilen;    /* bytes of the image in use */
static const char zeros[BPW];

/*
 * Strings that initialize a global's char pointers.  Each one is
 * dumped under its label after the object, from the literal pool.
 */
#define NUMSTR 64
static int strlab[NUMSTR]; /* label */
static int stroff[NUMSTR]; /* offset in the literal pool */
static int strat[NUMSTR];  /* offset of the pointer in the image */
static char *strsym[NUMSTR]; /* else the function whose address it is */
static int nstr;

/* Open an include file. */
void
doinclude(void)
{
	char path[2 * LINESIZE];
	char *cp;

	blanks(); /* skip over to the name */
	if (*lptr == '"' || *lptr == '<') {
		cp = ++lptr;
		while (*cp) {
			if (*cp == '"' || *cp == '>')
				*cp = '\0';
			++cp;
		}
	}
	if ((input2 = fopen(lptr, "r")) == NULL && incdir &&
	    strlen(incdir) + strlen(lptr) + 2 <= sizeof(path)) {
		/* not in the current directory, so try the -I one */
		strcpy(path, incdir);
		strcat(path, "/");
		strcat(path, lptr);
		input2 = fopen(path, "r");
	}
	if (input2 == NULL)
		error("open failure on include file");
	/*
	 * Clear the rest of the line so the next read comes from the
	 * new file, if it opened.
	 */
	killline();
}

/* Test for global declarations. */
int
dodeclare(int class)
{
	Tspec ts;

	if (typespec(&ts) == 0) {
		if (class != EXTERNAL)
			return 0;
		ts.type = CINT; /* "extern x;" declares an int */
		ts.tag = ts.dim = ts.fptr = 0;
		ts.ident = VARIABLE;
	}
	if (ts.type == CSTRUCT && ts.ident == VARIABLE && match(";"))
		return 1; /* the tag alone, defined or announced */
	/* a function definition brings its own body, and no semicolon */
	if (declglb(&ts, class) == 0)
		ns();
	return 1;
}

/*
 * Define the storage of the object named in ssname, of type, and tag,
 * with its initializer if one follows: a global, or a static local
 * when statlab is set.  ident is VARIABLE, POINTER or ARRAY, and *k
 * the declared length, set to the final one.  Returns the ident, which
 * an unsized array without an initializer turns into POINTER.
 */
static int
defstatic(int type, int tag, int ident, int *k)
{
	if (type == CSTRUCT && ident != POINTER) {
		*k = initglobal(type, tag, ident, *k);
		return ident;
	}
	if (type == CFUNC) {
		*k = initglobal(type, 0, ident, *k);
		return ident;
	}
	if (type == CSTRUCT)
		return initials(BPW, ident, *k, k);
	return initials(type >> 2, ident, *k, k);
}

/*
 * Declare global variables and functions of the type specifier ts.
 * Returns true if a function definition ended the declaration.
 */
static int
declglb(Tspec *ts, int class)
{
	Decl d;
	char *sym;
	int dup, isptr, j, k;

	for (;;) {
		if (endst())
			return 0;
		if (declarator(ts, &d) == 0)
			illname();
		/*
		 * Remember a clash but report it later: a prototype may
		 * legally precede the definition of the same function.
		 */
		sym = findglb(ssname);
		dup = (sym != NULL);
		if (filestatic && sym && sym[PRIV] == 0)
			error("static declaration follows non-static");
		isptr = (d.ident == POINTER);
		if (d.isfp == 0 && d.ident != ARRAY && match("(")) {
			/* a parameter list, maybe empty: a prototype or a definition */
			if (dofuncdecl(class, d.type, d.tag, isptr))
				return 1;
			sym = addsym(ssname, FUNCTION, d.type, 1, &glbptr, class,
			             d.tag);
			if (sym)
				sym[FPTR] = (char)isptr;
		} else {
			if (dup)
				multidef(ssname);
			j = d.ident;
			k = j == ARRAY ? d.dim : j == POINTER ? 0 : 1;
			if (class == EXTERNAL)
				external(ssname);
			else
				j = defstatic(d.type, d.tag, j, &k);
			sym = addsym(ssname, j, d.type, k, &glbptr, class, d.tag);
			if (sym) {
				sym[FPTR] = (char)d.fptr;
				if (j == ARRAY)
					putint(k, sym + DIM, DIMSIZE);
			}
		}
		if (sym && filestatic)
			sym[PRIV] = 1;
		if (match(",") == 0) /* more? */
			return 0;
	}
}

/*
 * Declare local variables of the type specifier ts.  A variable may
 * have an initializer, which runs where it is declared.
 */
void
declloc(Tspec *ts)
{
	Decl d;
	char *sym;
	Word v;
	int dim, j, k, strv, t, tag;

	if (swactive)
		error("not allowed in switch");
	if (noloc)
		error("not allowed with goto");
	if (declared < 0)
		error("must declare first in block");
	for (;;) {
		if (endst())
			return;
		if (declarator(ts, &d) == 0)
			illname();
		/* no multidef check: block locals are kept together */
		t = d.type;
		tag = d.tag;
		j = d.ident;
		dim = d.dim;
		strv = -1;
		if (j == ARRAY && dim == 0 && d.isfp == 0 && initfollows() == 0)
			j = POINTER; /* "char s[];" is a pointer, as it was */
		if (j == ARRAY && dim == 0) {
			match("=");
			if (t == CCHAR && qstr(&v)) {
				/* a string gives the length at once */
				strv = (int)v;
				dim = (int)strlen(litq + strv) + 1;
			} else {
				sym = addsym(ssname, j, t, csp, &locptr,
				             AUTOMATIC, tag);
				sym[FPTR] = (char)d.fptr;
				initunsized(sym, t, tag);
				if (match(",") == 0)
					return;
				continue;
			}
		}
		if (j == ARRAY)
			k = dim * objsize(t, tag);
		else if (j == POINTER)
			k = BPW;
		else if (t == CCHAR)
			k = SBPC;
		else
			k = objsize(t, tag);
		if (t == CSTRUCT)
			k = roundup(k, BPW);
		declared = declared + k;
		sym = addsym(ssname, j, t, csp - declared, &locptr, AUTOMATIC,
		             tag);
		sym[FPTR] = (char)d.fptr;
		if (j == ARRAY)
			putint(dim, sym + DIM, DIMSIZE);
		if (strv >= 0 || match("="))
			initlocal(sym, j, t, tag, dim, strv);
		if (match(",") == 0)
			return;
	}
}

/*
 * Parse a function pointer declarator after "(*": "name)(...)", or
 * "name[n])(...)" for an array of them.  The name may be left out, as
 * in a prototype or sizeof.  Sets ssname, and *dim to the array length,
 * or to -1 if it is not an array.  The parameter types are skipped,
 * since calls are not checked against them.  Returns true if a name
 * was given.
 */
int
fpdecl(int *dim)
{
	int named;

	if ((named = symname(ssname, YES)) == 0)
		*ssname = 0;
	*dim = -1;
	if (match("["))
		*dim = needsub();
	needtoken(")");
	needtoken("(");
	skipparams();
	return named;
}

/* Skip a parameter list, its "(" already read, through its ")". */
static void
skipparams(void)
{
	int depth;

	depth = 0;
	for (;;) {
		blanks();
		if (cch == 0) {
			error("missing )");
			return;
		}
		if (cch == '(') {
			++depth;
		} else if (cch == ')' && depth-- == 0) {
			gch();
			return;
		}
		gch();
	}
}

/*
 * Declare static locals, after "static": objects in static memory, as
 * globals are, that only their block can name.  Each one gets an
 * internal label, and an initializer must be constant, as a global's.
 */
void
declstatic(void)
{
	Decl d;
	Tspec ts;
	char *before, *lit, *start, *sym;
	int j, k, saved;

	if (typespec(&ts) == 0) {
		error("missing type");
		junk();
		return;
	}
	if (ts.type == CSTRUCT && ts.ident == VARIABLE && streq(lptr, ";"))
		return; /* the tag alone */
	/* an initializer reuses the literal pool, which holds the function's */
	saved = litptr;
	if ((lit = malloc((size_t)LITABSZ)) == NULL) {
		error("out of memory");
		exit(ERRCODE);
	}
	memcpy(lit, litq, (size_t)LITABSZ);
	/*
	 * The data is staged and kept for the end of the function: here
	 * it could follow the function's entry label, which would then
	 * name the data instead of the code.
	 */
	setstage(&before, &start);
	for (;;) {
		if (endst())
			break;
		if (declarator(&ts, &d) == 0)
			illname();
		j = d.ident;
		k = j == ARRAY ? d.dim : j == POINTER ? 0 : 1;
		statlab = getlabel();
		j = defstatic(d.type, d.tag, j, &k);
		sym = addsym(ssname, j, d.type, statlab, &locptr, LSTATIC,
		             d.tag);
		statlab = 0;
		sym[FPTR] = (char)d.fptr;
		if (j == ARRAY)
			putint(k, sym + DIM, DIMSIZE);
		if (match(",") == 0)
			break;
	}
	*stagenext = 0;
	keepstatic(start);
	clearstage(before, NULL);
	memcpy(litq, lit, (size_t)LITABSZ);
	litptr = saved;
	free(lit);
}

/* Keep the data text s for dumpstatic(). */
static void
keepstatic(const char *s)
{
	size_t n;

	n = strlen(s);
	if (statlen + n + 1 > statcap) {
		statcap = 2 * (statlen + n + 1);
		if ((statdata = realloc(statdata, statcap)) == NULL) {
			error("out of memory");
			exit(ERRCODE);
		}
	}
	memcpy(statdata + statlen, s, n + 1);
	statlen += n;
}

/* Emit the static locals' data kept while compiling the function. */
static void
dumpstatic(void)
{
	if (statlen) {
		sout(statdata, output);
		statlen = 0;
	}
}

/* Return true if "=" follows, but not "==".  Nothing is consumed. */
static int
initfollows(void)
{
	blanks();
	return cch == '=' && nch != '=';
}

/*
 * Initialize global objects.  dim is the declared length, 0 if none;
 * *len is set to the length, which an unsized array takes from its
 * initializer.
 */
static int
initials(int size, int ident, int dim, int *len)
{
	int savedim;

	litptr = 0;
	if (dim == 0)
		dim = -1;
	savedim = dim;

	deflabel(ssname);

	if (match("=")) {
		if (match("{")) {
			while (dim) {
				init(size, ident, &dim);
				if (match(",") == 0)
					break;
			}
			needtoken("}");
		} else {
			init(size, ident, &dim);
		}
	}
	if (dim == -1 && dim == savedim) {
		size = BPW;
		stowlit(0, size);
		ident = POINTER;
	}
	dumplits(size);
	dumpzero(size, dim);
	/* an unsized array counted down from -1 as values were stored */
	*len = savedim > 0 ? savedim : -1 - dim;
	return ident;
}

/* Evaluate one initializer. */
static void
init(int size, int ident, int *dim)
{
	Word value;

	if (qstr(&value)) {
		if (ident == VARIABLE || size != 1)
			error("must assign to char pointer or array");
		*dim = *dim - (litptr - value);
		if (ident == POINTER)
			point();
	} else if (constexpr(&value)) {
		if (ident == POINTER)
			error("cannot assign to pointer");
		stowlit(value, size);
		*dim = *dim - 1;
	}
}

/* Get the required array size. */
int
needsub(void)
{
	Word val;

	if (match("]"))
		return 0; /* null size */
	if (constexpr(&val) == 0)
		val = 1;
	if (val < 0) {
		error("negative size illegal");
		val = -val;
	}
	needtoken("]"); /* force a single dimension */
	return (int)val;
}

/*
 * Begin a function.  Called from parse(), this tries to make a function
 * out of the following text.
 */
void
newfunc(void)
{
	char fname[NAMESIZE];

	locptr = STARTLOC; /* clear local variables */
	if (monitor)
		lout(line, stderr);
	if (symname(ssname, YES) == 0) {
		error("illegal function or declaration");
		killline(); /* invalidate the line */
		return;
	}
	fntag = 0; /* no type was written, so it returns an int */
	startfunc();
	regfunc(CINT, 0, 0);
	/* parsing the parameters overwrites ssname, so save the name */
	strcpy(fname, ssname);
	if (match("(") == 0)
		error("no open paren");
	locptr = STARTLOC;
	if (doparams() == 0)
		doknrtypes();
	strcpy(ssname, fname);
	dobody();
}

/*
 * Parse what follows the open paren of a function declarator that was
 * reached through declglb(), meaning a return type was written.
 *
 * Compiles the body and returns 1 when this turns out to be a
 * definition.  Returns 0 for a prototype, leaving the caller to
 * register it.
 */
static int
dofuncdecl(int class, int type, int tag, int isptr)
{
	char fname[NAMESIZE];
	int knr;

	strcpy(fname, ssname);
	locptr = STARTLOC;
	knr = (doparams() == 0);
	if (knr)
		doknrtypes();
	strcpy(ssname, fname);
	blanks();
	if (knr == 0 && streq(lptr, "{") == 0)
		return 0; /* no body follows, so it is a prototype */
	if (class == EXTERNAL)
		error("cannot define an extern function");
	fntag = 0;
	if (type == CSTRUCT && isptr == 0) {
		fntag = tag; /* returns a struct by value */
		objsize(CSTRUCT, tag); /* which must be complete */
	}
	startfunc();
	regfunc(type, tag, isptr);
	dobody();
	return 1;
}

/* Reset the per-function state, and label the program entry once. */
static void
startfunc(void)
{
	nogo = 0;            /* enable goto statements */
	noloc = 0;           /* enable block-local declarations */
	lastst = 0;          /* no statement yet */
	litptr = 0;          /* clear the literal pool */
	litlab = getlabel(); /* label the next literal pool */
	if (fntag)
		fnretlab = getlabel(); /* label the struct return buffer */
	if (func1) {
		postlabel(beglab);
		func1 = 0;
	}
}

/*
 * Record ssname in the global table as a function defined here, which
 * returns type, a pointer to type if isptr is true.
 */
static void
regfunc(int type, int tag, int isptr)
{
	char *ptr;

	if ((ptr = findglb(ssname)) && filestatic && ptr[PRIV] == 0)
		error("static declaration follows non-static");
	if (ptr) { /* already known? */
		if (ptr[IDENT] != FUNCTION)
			multidef(ssname);
		else if (ptr[OFFSET] == FUNCTION)
			multidef(ssname);
		else {
			/* earlier assumed to be a function, or prototyped */
			ptr[OFFSET] = FUNCTION;
			ptr[CLASS] = STATIC;
			ptr[TYPE] = (char)type;
			ptr[TAG] = (char)tag;
			ptr[FPTR] = (char)isptr;
		}
	} else if ((ptr = addsym(ssname, FUNCTION, type, FUNCTION, &glbptr,
	                         STATIC, tag))) {
		ptr[FPTR] = (char)isptr;
	}
	if (ptr && filestatic)
		ptr[PRIV] = 1;
}

/* Compile a function body.  The parameters are already in scope. */
static void
dobody(void)
{
	entry();
	csp = 0; /* preset the stack pointer */
	statement();
	if (lastst != STRETURN && lastst != STGOTO)
		ffret();
	if (litptr) {
		printlabel(litlab);
		col();
		dumplits(1); /* dump the literals */
	}
	dumpstatic();
	if (fntag) {
		/* return copies the struct here, and the caller copies it out */
		postlabel(fnretlab);
		dumpzero(1, objsize(CSTRUCT, fntag));
	}
}

/* Non-consuming test for a type keyword at the scan position. */
static int
istype(void)
{
	return istypename();
}

/*
 * Parse a parameter list, the open paren already consumed and the
 * closing one consumed here.
 *
 * Returns 1 for a C99 list, whose types are now recorded, or 0 for a
 * K&R name list whose types still have to be read.
 */
static int
doparams(void)
{
	blanks();
	if (streq(lptr, ")") || istype()) {
		docparams();
		return 1;
	}
	doknrnames();
	return 0;
}

/*
 * Parse a C99 parameter list, in which each parameter carries its own
 * type.  A parameter may be unnamed, as in a prototype.
 */
static void
docparams(void)
{
	Decl d;
	Tspec ts;
	char *sym;
	int j, psize;

	argstk = 0;
	if (match(")")) { /* "()" takes no parameters */
		argtop = 0;
		return;
	}
	for (;;) {
		if (amatch("void", 4)) {
			if (match(")")) /* "(void)" takes none either */
				break;
			ts.type = CINT;
			ts.tag = ts.dim = ts.fptr = 0;
			ts.ident = VARIABLE;
		} else if (typespec(&ts) == 0) {
			error("missing parameter type");
			junk();
			break;
		}
		declarator(&ts, &d); /* the name may be left out */
		j = d.ident;
		if (j == ARRAY) /* an array parameter is a pointer */
			j = POINTER;
		psize = BPW;
		if (d.type == CSTRUCT && j == VARIABLE) /* passed by value */
			psize = roundup(objsize(CSTRUCT, d.tag), BPW);
		/*
		 * The caller pushes psize bytes, and the parameter's address
		 * is at their low end, BPW bytes lower than its start.
		 */
		if (*ssname && findloc(ssname))
			multidef(ssname);
		else if (*ssname && (sym = addsym(ssname, j, d.type,
		                                  argstk + psize - BPW, &locptr,
		                                  AUTOMATIC, d.tag)))
			sym[FPTR] = (char)d.fptr;
		argstk = argstk + psize;
		if (match(",") == 0) {
			needtoken(")");
			break;
		}
	}
	fixargs();
}

/*
 * Parse a K&R parameter list, which names the parameters but gives no
 * types.  Those follow the closing paren and are read by doknrtypes().
 */
static void
doknrnames(void)
{
	argstk = 0;               /* init the argument count */
	while (match(")") == 0) { /* then count the arguments */
		/* any legal name bumps the argument count */
		if (symname(ssname, YES)) {
			if (findloc(ssname))
				multidef(ssname);
			else {
				addsym(ssname, 0, 0, argstk, &locptr,
				       AUTOMATIC, 0);
				argstk = argstk + BPW;
			}
		} else {
			error("illegal argument name");
			junk();
		}
		blanks();
		/* if not a closing paren, it should be a comma */
		if (streq(lptr, ")") == 0) {
			if (match(",") == 0)
				error("no comma");
		}
		if (endst())
			break;
	}
	argtop = argstk;
}

/* Read the type declarations that follow a K&R parameter list. */
static void
doknrtypes(void)
{
	Tspec ts;

	while (argstk) {
		/* now let the user declare those argument types */
		if (typespec(&ts) == 0) {
			error("wrong number of arguments");
			break;
		}
		doargs(&ts);
		ns();
	}
	knrlayout();
}

/*
 * Give the K&R parameters their offsets, once their types are known:
 * a struct passed by value takes its size, rounded up to whole words,
 * and anything else one word.  doknrnames() added them left to right.
 */
static void
knrlayout(void)
{
	char *p;
	int pos, size, total;

	total = 0;
	for (p = STARTLOC; p < locptr; p = nextsym(p))
		total += knrsize(p);
	pos = 0;
	for (p = STARTLOC; p < locptr; p = nextsym(p)) {
		size = knrsize(p);
		/* the address is at the low end, BPW below the start */
		putint(total - (pos + size - BPW), p + OFFSET, OFFSIZE);
		pos += size;
	}
	argtop = total;
}

/* Return the bytes the caller pushes for the parameter p. */
static int
knrsize(char *p)
{
	if (p[IDENT] == VARIABLE && p[TYPE] == CSTRUCT)
		return roundup(objsize(CSTRUCT, p[TAG]), BPW);
	return BPW;
}

/*
 * Rewrite the parameter offsets once the whole list is known.  Each one
 * was recorded at its position from the left, but the generated code
 * addresses it by its distance from the top of the frame.
 */
static void
fixargs(void)
{
	char *p;

	argtop = argstk;
	p = STARTLOC;
	while (p < locptr) {
		putint(argtop - getint(p + OFFSET, OFFSIZE), p + OFFSET,
		       OFFSIZE);
		p = nextsym(p);
	}
}

/*
 * Declare argument types.  Called from newfunc(), this adds an entry to
 * the local symbol table for each named argument.
 */
static void
doargs(Tspec *ts)
{
	Decl d;
	char *argptr;
	int j, legalname;

	for (;;) {
		if (argstk == 0)
			return; /* no arguments */
		if ((legalname = declarator(ts, &d)) == 0)
			illname();
		j = d.ident;
		if (j == ARRAY) /* an array parameter is a pointer */
			j = POINTER;
		if (legalname) {
			if ((argptr = findloc(ssname))) {
				/* the offsets wait for knrlayout() */
				argptr[IDENT] = (char)j;
				argptr[TYPE] = (char)d.type;
				argptr[TAG] = (char)d.tag;
				argptr[FPTR] = (char)d.fptr;
			} else {
				error("not an argument");
			}
		}
		argstk = argstk - BPW; /* count down */
		if (endst())
			return;
		if (match(",") == 0)
			error("no comma");
	}
}

/* Match "struct" or "union".  Returns STRUCT, UNION or 0. */
int
structkw(void)
{
	if (amatch("struct", 6))
		return STRUCT;
	if (amatch("union", 5))
		return UNION;
	return 0;
}

/*
 * Parse a struct or union type after its keyword, which kind tells: a
 * tag, and a member list if one follows.  Returns the tag index, else
 * 0 after an error.  A tag may be used before its members are known,
 * which a member "struct node *next" inside struct node needs.  The
 * members of a union all start at offset 0.
 */
int
dostruct(int kind)
{
	char name[NAMESIZE];
	Stag *st;
	Decl d;
	Tspec mts;
	int al, dim, j, size, t;

	if (symname(name, YES) == 0) {
		blanks();
		if (cch != '{') {
			error("missing struct tag");
			return 0;
		}
		*name = 0; /* anonymous, as in "typedef struct { ... } T;" */
	}
	if (*name == 0 || (t = findtag(name)) == 0) {
		if (ntags == NUMTAGS - 1) {
			error("too many struct tags");
			return 0;
		}
		t = ++ntags;
		strcpy(tags[t].name, name);
		tags[t].align = 1;
		tags[t].first = tags[t].last = -1;
		tags[t].kind = kind;
	} else if (tags[t].kind != kind) {
		error("struct and union tag clash");
	}
	if (match("{") == 0)
		return t;
	st = &tags[t];
	if (st->complete)
		multidef(name);
	st->size = 0;
	st->align = 1;
	st->first = st->last = -1;
	while (match("}") == 0) {
		if (eof) {
			error("no closing brace");
			break;
		}
		if (typespec(&mts) == 0) {
			error("missing member type");
			junk();
			match(";");
			continue;
		}
		for (;;) {
			if (declarator(&mts, &d) == 0) {
				illname();
				break;
			}
			j = d.ident;
			dim = 1;
			if (j == ARRAY && (dim = d.dim) == 0)
				error("member array needs a size");
			if (j == POINTER || d.type == CFUNC) {
				size = BPW * dim;
				al = BPW;
			} else if (d.type == CSTRUCT) {
				size = objsize(CSTRUCT, d.tag) * dim;
				al = tags[d.tag].align;
			} else {
				size = objsize(d.type, 0) * dim;
				al = objsize(d.type, 0);
			}
			if (findmemb(t, ssname))
				multidef(ssname);
			if (kind == UNION) {
				addmemb(t, ssname, j, d.type, d.tag, 0, dim, d.fptr);
				if (size > st->size)
					st->size = size;
			} else {
				st->size = roundup(st->size, al);
				addmemb(t, ssname, j, d.type, d.tag, st->size, dim,
				        d.fptr);
				st->size = st->size + size;
			}
			if (al > st->align)
				st->align = al;
			if (match(",") == 0)
				break;
		}
		ns();
	}
	st->size = roundup(st->size, st->align);
	st->complete = 1;
	return t;
}

/* Return the index of the tag name, else 0. */
static int
findtag(const char *name)
{
	int t;

	for (t = 1; t <= ntags; ++t)
		if (strcmp(tags[t].name, name) == 0)
			return t;
	return 0;
}

/* Append a member to the chain of struct tag. */
static void
addmemb(int tag, const char *name, int ident, int type, int mtag,
        int offset, int dim, int fptr)
{
	char *m;

	if (nmembs == NUMMEMB) {
		error("too many struct members");
		exit(ERRCODE);
	}
	m = membtab + nmembs * SYMMAX;
	m[IDENT] = (char)ident;
	m[TYPE] = (char)type;
	m[CLASS] = MEMBER;
	m[TAG] = (char)mtag;
	m[FPTR] = (char)fptr;
	putint(offset, m + OFFSET, OFFSIZE);
	strcpy(m + NAME, name);
	membdim[nmembs] = dim;
	membnext[nmembs] = -1;
	if (tags[tag].last < 0)
		tags[tag].first = nmembs;
	else
		membnext[tags[tag].last] = nmembs;
	tags[tag].last = nmembs;
	++nmembs;
}

/* Return the entry of member name in struct tag, else NULL. */
char *
findmemb(int tag, const char *name)
{
	char *m;
	int i;

	if (tag <= 0 || tag > ntags)
		return NULL;
	for (i = tags[tag].first; i >= 0; i = membnext[i]) {
		m = membtab + i * SYMMAX;
		if (astreq(name, m + NAME, NAMEMAX))
			return m;
	}
	return NULL;
}

/* Return the number of elements of member m, which is 1 unless an array. */
int
memblen(const char *m)
{
	return membdim[(m - membtab) / SYMMAX];
}

/*
 * Initializers.  One parser reads them for globals and for locals.
 * For a global it fills a byte image of the object, which constants
 * must fill; for a local it generates code that stores each value, so
 * any expression will do.  As in C, a member or element without a
 * value is zero, and the braces of an inner struct or array may be
 * left out, its values then coming from the enclosing list.
 */

/*
 * Define the global object named in ssname, of type, and tag, with the
 * initializer that may follow: a struct, a function pointer, or an
 * array of either when ident is ARRAY.  dim is the array length, 0 if
 * not given.  Returns the length, which an unsized array takes from
 * its initializer.
 */
static int
initglobal(int type, int tag, int ident, int dim)
{
	char name[NAMESIZE];
	int i, size;

	strcpy(name, ssname); /* parsing the initializer may reuse ssname */
	litptr = 0;
	nstr = 0;
	ilocal = 0;
	size = objsize(type, tag);
	deflabel(name);
	if (match("=") == 0) {
		if (ident == ARRAY && dim == 0)
			error("array needs a length");
		dumpzeros(ident == ARRAY ? dim * size : size);
		strcpy(ssname, name);
		return ident == ARRAY ? dim : 1;
	}
	ilen = 0;
	if (ident == ARRAY) {
		initarr(type, tag, &dim, 0);
	} else if (type == CSTRUCT) {
		blanks();
		if (cch != '{')
			error("missing {");
		initagg(tag, 0);
		dim = 1;
	} else {
		initscalar(type, VARIABLE, 0);
		dim = 1;
	}
	emitbuf(dim * size);
	for (i = 0; i < nstr; ++i) {
		if (strsym[i])
			continue; /* a function's address, not a string */
		postlabel(strlab[i]);
		dumpbytes(litq + stroff[i], (int)strlen(litq + stroff[i]) + 1);
	}
	litptr = 0;
	strcpy(ssname, name);
	return dim;
}

/* Label the static object being defined: a global, or a static local. */
static void
deflabel(const char *name)
{
	if (statlab) {
		printlabel(statlab);
	} else if (filestatic) {
		ot("._"); /* local to the file in the assembler */
		outstr(name);
	} else {
		outlab(name);
	}
	col();
	nl();
}

/*
 * Initialize the local sym, which was just declared, from the value
 * that follows "=".  strv >= 0 is a string the caller already read, at
 * that offset in the literal pool.
 */
static void
initlocal(char *sym, int ident, int type, int tag, int dim, int strv)
{
	char *before, *start;
	int size;

	/* give sym its place on the stack before code refers to it */
	if (ncmp > 1 && declared)
		nogo = declared;
	csp = modstk(csp - declared, NO);
	declared = 0;
	ilocal = 1;
	isym = sym;
	setstage(&before, &start);
	if (ident == ARRAY || (ident == VARIABLE && type == CSTRUCT)) {
		/* an aggregate: values left out are zero */
		size = ident == ARRAY ? dim * objsize(type, tag)
		                      : objsize(CSTRUCT, tag);
		locaddr(0);
		move();
		zeromem(size);
	}
	if (strv >= 0) {
		initstr(strv, &dim, 0);
	} else if (ident == VARIABLE && type == CSTRUCT) {
		blanks();
		if (cch != '{' && structexpr() == 0)
			error("missing {");
		initagg(tag, 0);
	} else {
		initval(type, tag, ident, dim, 0);
	}
	clearstage(before, start);
	ilocal = 0;
}

/*
 * Initialize the local array sym, declared with "[]", from the list in
 * braces that follows "=", and give it the length of that list.
 *
 * The array is allocated by itself, below everything declared before
 * it.  Its address relative to the stack pointer then does not depend
 * on its length, so the stores can be generated before the length is
 * known.  The allocation and the clearing, which come before them, get
 * their sizes written into the staged code afterwards.
 */
static void
initunsized(char *sym, int type, int tag)
{
	char *alloc, *before, *start, *zsize;
	int csp0, dim, i, k;

	if (ncmp > 1 && declared)
		nogo = declared;
	csp = modstk(csp - declared, NO);
	declared = 0;
	csp0 = csp;
	putint(csp0, sym + OFFSET, OFFSIZE); /* until the length is known */
	ilocal = 1;
	isym = sym;
	setstage(&before, &start);
	fixsym = sym;
	nfix = 0;
	ot("\tsub r7, ");
	alloc = stagenext;
	ot("          "); /* room for the size */
	nl();
	locaddr(0);
	move();
	ot("\tmov r2, ");
	zsize = stagenext;
	ot("          ");
	nl();
	ffcall("CCZERO");
	dim = 0;
	initarr(type, tag, &dim, 0);
	if (dim == 0)
		error("empty initializer");
	k = dim * objsize(type, tag);
	if (type == CSTRUCT)
		k = roundup(k, BPW);
	patchnum(alloc, k);
	patchnum(zsize, dim * objsize(type, tag));
	/* every other local is k bytes further from the stack pointer */
	for (i = 0; i < nfix; ++i)
		addnum(fixpos[i], k);
	fixsym = NULL;
	csp = csp0 - k;
	putint(csp, sym + OFFSET, OFFSIZE);
	putint(dim, sym + DIM, DIMSIZE);
	clearstage(before, start);
	ilocal = 0;
}

/* Add v to the right-aligned number in the ten characters at p. */
static void
addnum(char *p, int v)
{
	char buf[16];

	sprintf(buf, "%10d", atoi(p) + v);
	memcpy(p, buf, 10);
}

/* Write v in decimal over the ten blanks at p. */
static void
patchnum(char *p, int v)
{
	char buf[16];

	sprintf(buf, "%d", v);
	memcpy(p, buf, strlen(buf));
}

/* Skip to the end of the current brace list, after an error. */
static void
skipinit(void)
{
	int depth;

	depth = 0;
	for (;;) {
		blanks();
		if (cch == 0 || cch == ';' || (cch == '}' && depth == 0))
			return;
		if (cch == '{')
			++depth;
		else if (cch == '}')
			--depth;
		gch();
	}
}

/* Initialize one object of type, and tag, at byte offset off. */
static void
initval(int type, int tag, int ident, int dim, int off)
{
	if (ident == ARRAY)
		initarr(type, tag, &dim, off);
	else if (ident == VARIABLE && type == CSTRUCT)
		initagg(tag, off);
	else
		initscalar(type, ident, off);
}

/*
 * Initialize an array of *dim objects at off: "{ value, ... }", the
 * values alone when the braces were left out, or a string for a char
 * array.  An unsized array, *dim = 0, takes its length from the list.
 */
static void
initarr(int type, int tag, int *dim, int off)
{
	Word v;
	int braced, n, size;

	if (type == CCHAR && qstr(&v)) {
		initstr((int)v, dim, off);
		return;
	}
	size = objsize(type, tag);
	braced = match("{");
	if (braced == 0 && *dim == 0) {
		error("missing {");
		return;
	}
	n = 0;
	for (;;) {
		if (atend())
			break;
		if (*dim && n == *dim) {
			error("too many initializers");
			skipinit();
			break;
		}
		initval(type, tag, VARIABLE, 1, off + n * size);
		++n;
		if (braced == 0 && n == *dim)
			break; /* full: the rest belongs to the enclosing list */
		if (match(",") == 0)
			break;
	}
	if (braced)
		needtoken("}");
	if (*dim == 0)
		*dim = n;
}

/*
 * Initialize the struct or union tag at off: "{ value, ... }", or its
 * values alone when the braces were left out.  For a local, a struct
 * expression, such as a variable or a call, is copied instead.
 */
static void
initagg(int tag, int off)
{
	char *m;
	int braced, i;

	if (tags[tag].complete == 0) {
		objsize(CSTRUCT, tag); /* reports the error */
		return;
	}
	blanks();
	if (ilocal && cch != '{' && structexpr()) {
		initcopy(tag, off);
		return;
	}
	braced = match("{");
	for (i = tags[tag].first; i >= 0;) {
		if (atend())
			break;
		m = membtab + i * SYMMAX;
		initval(m[TYPE], m[TAG], m[IDENT], memblen(m),
		        off + (int)getint(m + OFFSET, OFFSIZE));
		/* a union takes one value, for its first member */
		i = tags[tag].kind == UNION ? -1 : membnext[i];
		if (i < 0 || match(",") == 0)
			break;
	}
	if (braced) {
		if (match(",") && atend() == 0) {
			error("too many initializers");
			skipinit();
		}
		needtoken("}");
	}
}

/* Initialize a char, int or pointer at off, from a value in optional braces. */
static void
initscalar(int type, int ident, int off)
{
	Lvalue lv = {0};
	Word v;
	int braced, k, size;

	size = ident == POINTER ? BPW : objsize(type, 0);
	braced = match("{");
	if (ident == POINTER && type == CCHAR && qstr(&v)) {
		initptrstr((int)v, off);
	} else if (ilocal == 0 && type == CFUNC && initfunc(off)) {
		/* the function's address is recorded for the image */
	} else if (ilocal) {
		locaddr(off);
		push();
		if ((k = hier1(&lv)))
			rvalue(&lv);
		else if (isstructval(&lv))
			error("struct used as a value");
		pop();
		lv.ind = size == 1 ? CCHAR : CINT;
		putstk(&lv);
	} else {
		constexpr(&v);
		putbuf(off, size, v);
	}
	if (braced) {
		match(",");
		needtoken("}");
	}
}

/* Store the string at pool offset v in the char array of *dim at off. */
static void
initstr(int v, int *dim, int off)
{
	int n;

	n = (int)strlen(litq + v) + 1;
	if (*dim == 0)
		*dim = n;
	if (n - 1 > *dim)
		error("string too long");
	if (n > *dim)
		n = *dim; /* "abc" fills char[3] without the null */
	if (ilocal) {
		locaddr(off);
		move();
		litaddr(v);
		movestruct(n);
	} else {
		ensure(off + n);
		memcpy(ibuf + off, litq + v, (size_t)n);
		litptr = v; /* the bytes are in the image: drop them */
	}
}

/* Point the char pointer at off to the string at pool offset v. */
static void
initptrstr(int v, int off)
{
	Lvalue lv = {0};

	if (ilocal) {
		locaddr(off);
		push();
		litaddr(v);
		pop();
		lv.ind = CINT;
		putstk(&lv);
	} else if (nstr == NUMSTR) {
		error("too many strings");
	} else {
		/* the string follows the object, under a label */
		ensure(off + BPW);
		strlab[nstr] = getlabel();
		stroff[nstr] = v;
		strsym[nstr] = NULL;
		strat[nstr++] = off;
	}
}

/*
 * In a global's image, put the address of the function named next, as
 * in "add" or "&add", at off.  Returns false, consuming nothing, if no
 * name follows.
 */
static int
initfunc(int off)
{
	char name[NAMESIZE];
	char *sym;
	int amp;

	amp = match("&");
	if (symname(name, YES) == 0) {
		if (amp)
			error("not a function");
		return amp;
	}
	if ((sym = findglb(name)) == NULL)
		sym = addsym(name, FUNCTION, CINT, 0, &glbptr, AUTOEXT, 0);
	if (sym == NULL || sym[IDENT] != FUNCTION) {
		error("not a function");
		return 1;
	}
	if (nstr == NUMSTR) {
		error("too many addresses");
		return 1;
	}
	ensure(off + BPW);
	strsym[nstr] = sym;
	strat[nstr++] = off;
	return 1;
}

/* Copy a struct expression of struct tag into the local at off. */
static void
initcopy(int tag, int off)
{
	Lvalue lv = {0};

	locaddr(off);
	push();
	hier1(&lv);
	if (isstructval(&lv) == 0 || lv.tag != tag)
		error("struct types differ");
	pop();
	movestruct(objsize(CSTRUCT, tag));
}

/*
 * Return true if a struct expression seems to follow: a name, maybe
 * after "*", whose symbol is a struct, an array of them, a pointer to
 * one or a function returning one.  Nothing is consumed.
 */
static int
structexpr(void)
{
	char name[NAMESIZE];
	char *save, *sym;
	int r;

	blanks();
	save = lptr;
	match("*");
	r = 0;
	if (symname(name, YES)) {
		if ((sym = findloc(name)) == NULL)
			sym = findglb(name);
		r = sym && sym[TYPE] == CSTRUCT && sym[IDENT] != LABEL;
	}
	if (lptr != save)
		bump((int)(save - lptr)); /* bump(0) would rewind the line */
	return r;
}

/* Return true at the end of an initializer list. */
static int
atend(void)
{
	blanks();
	return cch == '}' || cch == ';' || cch == 0;
}

/* Load the address of byte off of the local being initialized. */
static void
locaddr(int off)
{
	getloc(isym);
	if (off)
		inc(off);
}

/* Load the address of the string at pool offset v. */
static void
litaddr(int v)
{
	immed();
	printlabel(litlab);
	ot(" + ");
	outdec(v);
	nl();
}

/* Make the global image at least n bytes long, zero filled. */
static void
ensure(int n)
{
	int size;

	if (n <= ilen)
		return;
	if (n > ibufsz) {
		size = ibufsz ? ibufsz : 256;
		while (size < n)
			size = 2 * size;
		if ((ibuf = realloc(ibuf, (size_t)size)) == NULL) {
			error("out of memory");
			exit(ERRCODE);
		}
		ibufsz = size;
	}
	memset(ibuf + ilen, 0, (size_t)(n - ilen));
	ilen = n;
}

/* Store the size-byte value v at off in the global image, low byte first. */
static void
putbuf(int off, int size, Word v)
{
	ensure(off + size);
	putint(v, ibuf + off, size);
}

/*
 * Emit the first n bytes of the global image: a word at each string
 * pointer, "zero" for long runs of zeros, and bytes otherwise.
 */
static void
emitbuf(int n)
{
	int i, j, k, r;

	ensure(n);
	i = 0;
	while (i < n) {
		for (r = 0; r < nstr; ++r)
			if (strat[r] == i)
				break;
		if (r < nstr) {
			ot("\tdq ");
			if (strsym[r])
				symlabel(strsym[r]);
			else
				printlabel(strlab[r]);
			nl();
			i += BPW;
			continue;
		}
		/* the run up to the next string pointer */
		for (j = i + 1; j < n; ++j) {
			for (r = 0; r < nstr; ++r)
				if (strat[r] == j)
					break;
			if (r < nstr)
				break;
		}
		while (i < j) {
			for (k = i; k < j && ibuf[k] == 0; ++k)
				;
			if (k - i >= BPW || k == j) {
				dumpzeros(k - i);
				i = k;
				continue;
			}
			for (k = i; k < j && k - i < 16; ++k)
				if (ibuf[k] == 0 && k + BPW <= j &&
				    memcmp(ibuf + k, zeros, BPW) == 0)
					break; /* a long zero run starts here */
			dumpbytes(ibuf + i, k - i);
			i = k;
		}
	}
}

/* Emit n bytes from p as data. */
static void
dumpbytes(const char *p, int n)
{
	int i;

	while (n > 0) {
		defstorage(1);
		for (i = 0; i < 16 && n > 0; ++i, --n) {
			if (i)
				outbyte(',');
			outdec(*p++);
		}
		nl();
	}
}

/* Emit n zero bytes, if n is positive. */
static void
dumpzeros(int n)
{
	if (n > 0)
		dumpzero(1, n);
}

/* Return the length in bytes of an object of type, and tag if a struct. */
int
objsize(int type, int tag)
{
	if (type == CCHAR)
		return 1;
	if (type != CSTRUCT)
		return BPW;
	if (tag <= 0 || tag > ntags || tags[tag].complete == 0) {
		error("incomplete struct");
		return BPW;
	}
	return tags[tag].size;
}

/*
 * Parse a type name, as in "sizeof (struct node *)", and return its
 * length in bytes.  Returns -1, consuming nothing, if no type follows.
 */
int
typesize(void)
{
	Decl d;
	Tspec ts;
	char name[NAMESIZE];

	if (typespec(&ts) == 0)
		return -1;
	strcpy(name, ssname); /* an abstract declarator has no name */
	declarator(&ts, &d);
	strcpy(ssname, name);
	if (d.ident == ARRAY)
		return d.dim * objsize(d.type, d.tag);
	if (d.ident == POINTER)
		return BPW;
	return objsize(d.type, d.tag);
}

/* Round n up to a multiple of align. */
static int
roundup(int n, int align)
{
	return (n + align - 1) / align * align;
}
