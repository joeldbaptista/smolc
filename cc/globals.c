/*
 * Small-C compiler -- global state.
 *
 * Every global the compiler shares between translation units is defined
 * here and declared in scc.h.
 */

#include <stdio.h>

#include "scc.h"

char bell;
char monitor;
char optimize;
char pauseerr;

char *stage;
char *stagelast;
char *stagenext;
char *symtab;
char *litq;
char *macn;
char *macq;
char *pline;
char *mline;
char *line;
char *lptr;
char *glbptr;
char *locptr;
char *cptr, *cptr2, *cptr3;
char quote[2];
char msname[NAMESIZE];
char ssname[NAMESIZE];

Binop op[16];
Binop op2[16];

Stag tags[NUMTAGS];
int ntags;
char *membtab;
int nmembs;

char *fixsym;
char *fixpos[NUMFIX];
int nfix;

int statlab;
int filestatic;
int fntag;
int fnretlab;

char *incdir;
int nerrors;

int argcs;
char **argvs;

Word *swnext;
Word *swend;
int *wq;
int *wqptr;

int argstk, argtop;
int beglab;
int cch, nch;
int ccode;
int csp;
int declared;
int eof;
int errflag;
int filearg;
int files;
int func1;
int iflevel;
int lastst;
int litlab;
int litptr;
int macptr;
int ncmp;
int nogo;
int noloc;
int nxtlab;
int opindex;
int opsize;
int pptr;
int skiplevel;
int swactive;
int swdefault;
int swused;

FILE *input;
FILE *input2;
FILE *output;
FILE *listfp;
