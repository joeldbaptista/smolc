/*
 * Small-C compiler -- statement parser.
 *
 * Small C, 8088/8086 version -- modified by R. Grehan, BYTE Magazine.
 * Retargeted to the smol machine for smolc.
 */

#include <stdio.h>

#include "scc.h"

static void compound(void);
static void doif(void);
static void doexpr(void);
static void dowhile(void);
static void dodo(void);
static void dofor(void);
static void doswitch(void);
static void docase(void);
static void dodefault(void);
static void dogoto(void);
static int dolabel(void);
static int addlabel(void);
static void doreturn(void);
static void dobreak(void);
static void docont(void);

/*
 * Called whenever the syntax requires a statement.  Compiles that
 * statement and returns a number saying which one it was.
 */
int
statement(void)
{
	Tspec ts;

	if (cch == 0 && eof)
		return lastst;
	if (amatch("typedef", 7)) {
		dotypedef();
		ns();
	} else if (istypename() && typespec(&ts)) {
		if (ts.type != CSTRUCT || ts.ident != VARIABLE ||
		    match(";") == 0) { /* else the tag alone */
			declloc(&ts);
			ns();
		}
	} else if (amatch("static", 6)) {
		declstatic();
		ns();
	} else {
		if (declared >= 0) {
			if (ncmp > 1 && declared)
				nogo = declared; /* disable goto */
			csp = modstk(csp - declared, NO);
			declared = -1;
		}
		if (match("{")) {
			compound();
		} else if (amatch("if", 2)) {
			doif();
			lastst = STIF;
		} else if (amatch("while", 5)) {
			dowhile();
			lastst = STWHILE;
		} else if (amatch("do", 2)) {
			dodo();
			lastst = STDO;
		} else if (amatch("for", 3)) {
			dofor();
			lastst = STFOR;
		} else if (amatch("switch", 6)) {
			doswitch();
			lastst = STSWITCH;
		} else if (amatch("case", 4)) {
			docase();
			lastst = STCASE;
		} else if (amatch("default", 7)) {
			dodefault();
			lastst = STDEF;
		} else if (amatch("goto", 4)) {
			dogoto();
			lastst = STGOTO;
		} else if (dolabel()) {
			lastst = STLABEL;
		} else if (amatch("return", 6)) {
			doreturn();
			ns();
			lastst = STRETURN;
		} else if (amatch("break", 5)) {
			dobreak();
			ns();
			lastst = STBREAK;
		} else if (amatch("continue", 8)) {
			docont();
			ns();
			lastst = STCONT;
		} else if (match(";")) {
			errflag = 0;
		} else if (match("#asm")) {
			doasm();
			lastst = STASM;
		} else {
			doexpr();
			ns();
			lastst = STEXPR;
		}
	}
	return lastst;
}

/* Semicolon enforcer, called wherever the syntax requires one. */
void
ns(void)
{
	if (match(";") == 0)
		error("no semicolon");
	else
		errflag = 0;
}

static void
compound(void)
{
	char *savloc;
	int savcsp;

	savcsp = csp;
	savloc = locptr;
	declared = 0; /* local variables may now be declared */
	++ncmp;       /* open a new level */
	while (match("}") == 0) {
		if (eof) {
			error("no final }");
			break;
		}
		statement();
	}
	--ncmp; /* close the current level */
	if (lastst != STRETURN && lastst != STGOTO)
		modstk(savcsp, NO); /* delete local variable space */
	csp = savcsp;

	cptr = savloc; /* retain labels */
	while (cptr < locptr) {
		cptr2 = nextsym(cptr);
		if (cptr[IDENT] == LABEL) {
			while (cptr < cptr2)
				*savloc++ = *cptr++;
		} else {
			cptr = cptr2;
		}
	}
	locptr = savloc; /* delete the local symbols */
	declared = -1;   /* variables may not be declared */
}

static void
doif(void)
{
	int flab1, flab2;

	flab1 = getlabel(); /* label for the false branch */
	test(flab1, YES);   /* get the expression, branch if false */
	statement();        /* if true, do a statement */
	if (amatch("else", 4) == 0) {
		/* simple "if": print the false label and exit */
		postlabel(flab1);
		return;
	}
	flab2 = getlabel();
	if (lastst != STRETURN && lastst != STGOTO)
		jump(flab2);
	postlabel(flab1); /* print the false label */
	statement();      /* and do the "else" clause */
	postlabel(flab2); /* print the true label */
}

static void
doexpr(void)
{
	char *before, *start;
	int isconst;
	Word val;

	for (;;) {
		setstage(&before, &start);
		expression(&isconst, &val);
		clearstage(before, start);
		if (cch != ',')
			break;
		bump(1);
	}
}

static void
dowhile(void)
{
	int q[4]; /* local copy of the queue entry */

	addwhile(q);          /* add an entry for "break" */
	postlabel(q[WQLOOP]); /* loop label */
	test(q[WQEXIT], YES); /* see if true */
	statement();          /* if so, do a statement */
	jump(q[WQLOOP]);      /* loop back */
	postlabel(q[WQEXIT]); /* exit label */
	delwhile();           /* delete the queue entry */
}

static void
dodo(void)
{
	int q[4], top;

	addwhile(q);
	postlabel(top = getlabel());
	statement();
	needtoken("while");
	postlabel(q[WQLOOP]);
	test(q[WQEXIT], YES);
	jump(top);
	postlabel(q[WQEXIT]);
	delwhile();
	ns();
}

static void
dofor(void)
{
	int q[4], lab1, lab2;

	addwhile(q);
	lab1 = getlabel();
	lab2 = getlabel();
	needtoken("(");
	if (match(";") == 0) {
		doexpr(); /* expression 1 */
		ns();
	}
	postlabel(lab1);
	if (match(";") == 0) {
		test(q[WQEXIT], NO); /* expression 2 */
		ns();
	}
	jump(lab2);
	postlabel(q[WQLOOP]);
	if (match(")") == 0) {
		doexpr(); /* expression 3 */
		needtoken(")");
	}
	jump(lab1);
	postlabel(lab2);
	statement();
	jump(q[WQLOOP]);
	postlabel(q[WQEXIT]);
	delwhile();
}

static void
doswitch(void)
{
	int q[4], endlab, swact, swdef, tablab;
	Word *swnex, *swptr;

	swact = swactive;
	swdef = swdefault;
	swnex = swptr = swnext;
	addwhile(q);
	*(wqptr + WQLOOP - WQSIZ) = 0;
	needtoken("(");
	doexpr(); /* evaluate the switch expression */
	needtoken(")");
	swdefault = 0;
	swactive = 1;
	jump(endlab = getlabel());
	statement(); /* cases, etc. */
	jump(q[WQEXIT]);
	postlabel(endlab);
	sw(tablab = getlabel()); /* match the cases */
	if (swdefault)
		jump(swdefault);
	/* the table is static data, so it does not interrupt the code */
	postlabel(tablab);
	while (swptr < swnext) {
		defstorage(CINT >> 2);
		printlabel((int)*swptr++); /* case label */
		outbyte(',');
		outdec(*swptr++); /* case value */
		nl();
	}
	defstorage(CINT >> 2);
	outdec(0);
	nl();
	postlabel(q[WQEXIT]);
	delwhile();
	swnext = swnex;
	swdefault = swdef;
	swactive = swact;
}

static void
docase(void)
{
	if (swactive == 0)
		error("not in switch");
	if (swnext > swend) {
		error("too many cases");
		return;
	}
	postlabel(*swnext++ = getlabel());
	constexpr(swnext++);
	needtoken(":");
}

static void
dodefault(void)
{
	if (swactive) {
		if (swdefault)
			error("multiple defaults");
	} else {
		error("not in switch");
	}
	needtoken(":");
	postlabel(swdefault = getlabel());
}

static void
dogoto(void)
{
	if (nogo > 0)
		error("not allowed with block-locals");
	else
		noloc = 1;
	if (symname(ssname, YES))
		jump(addlabel());
	else
		error("bad label");
	ns();
}

static int
dolabel(void)
{
	char *savelptr;

	blanks();
	savelptr = lptr;
	if (symname(ssname, YES)) {
		if (gch() == ':') {
			postlabel(addlabel());
			return 1;
		}
		bump((int)(savelptr - lptr));
	}
	return 0;
}

static int
addlabel(void)
{
	if ((cptr = findloc(ssname))) {
		if (cptr[IDENT] != LABEL)
			error("not a label");
	} else {
		cptr = addsym(ssname, LABEL, LABEL, getlabel(), &locptr,
		              LABEL, 0);
	}
	return getint(cptr + OFFSET, OFFSIZE);
}

static void
doreturn(void)
{
	Lvalue lv = {0};
	char *before, *start;

	if (endst() == 0 && fntag) {
		/* copy the struct to the return buffer, and return that */
		setstage(&before, &start);
		hier1(&lv);
		if (isstructval(&lv) == 0 || lv.tag != fntag)
			error("struct types differ");
		ot("\tmov r1, ");
		printlabel(fnretlab);
		nl();
		movestruct(objsize(CSTRUCT, fntag));
		ot("\tmov r0, ");
		printlabel(fnretlab);
		nl();
		clearstage(before, start);
		modstk(0, YES);
	} else if (endst() == 0) {
		doexpr();
		modstk(0, YES);
	} else {
		modstk(0, NO);
	}
	ffret();
}

static void
dobreak(void)
{
	int *ptr;

	if ((ptr = readwhile(wqptr)) == 0)
		return;
	modstk(ptr[WQSP], NO);
	jump(ptr[WQEXIT]);
}

static void
docont(void)
{
	int *ptr;

	ptr = wqptr;
	for (;;) {
		if ((ptr = readwhile(ptr)) == 0)
			return;
		if (ptr[WQLOOP])
			break;
	}
	modstk(ptr[WQSP], NO);
	jump(ptr[WQLOOP]);
}

void
doasm(void)
{
	ccode = 0; /* mark the mode as "asm" */
	for (;;) {
		readline();
		if (match("#endasm"))
			break;
		if (eof)
			break;
		sout(line, output);
	}
	killline();
	ccode = 1;
}
