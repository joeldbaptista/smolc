/*
 * Small-C compiler -- shared declarations.
 *
 * smolc: BYTE Small-C 1.0 retargeted to the smol register machine.
 * Based on Small-C by J. E. Hendrix, after Ron Cain.
 * MS-DOS conversion by R. E. Grehan, BYTE Magazine.
 */

#ifndef CC_H
#define CC_H

#include <stdio.h>

#include "ccdef.h"

#define YES 1
#define NO 0
#define NEWLINE '\n'

/*
 * A binary operator emits code combining the primary register (r0) and
 * the secondary register (r1).  A jumper emits a conditional branch to
 * an internal label.  A hier function parses one precedence level and
 * returns non-zero when it yielded an lvalue.  A step operator emits an
 * increment or decrement of n bytes.
 */
/*
 * A target int.  Constant expressions are evaluated in this type, so
 * they fold to the same value the smol machine would compute.
 */
typedef long long Word;

typedef struct lvalue Lvalue;
typedef struct stag Stag;
typedef struct tspec Tspec;
typedef struct decl Decl;
typedef void (*Binop)(void);
typedef void (*Jumper)(int lab);
typedef int (*Hier)(Lvalue *lv);
typedef void (*Stepop)(int n);

struct lvalue {
	char *sym;   /* symbol table entry, else NULL for a constant */
	int ind;     /* type of indirect object to fetch, else 0 */
	int ptr;     /* type of pointer or array, else 0 */
	int isconst; /* true if a constant expression */
	Word val;    /* value of that constant, plus auxiliary uses */
	int sreg;    /* true if the secondary register was altered */
	Binop oper;  /* highest/last binary operator applied */
	char *stg;   /* stage address of "oper 0" code, else NULL */
	int tag;     /* struct tag when ind or ptr is CSTRUCT, else 0 */
};

/*
 * A type specifier: a keyword, a struct or union, or a typedef name.
 * A typedef name may stand for a pointer or an array, which ident says.
 */
struct tspec {
	int type;  /* CCHAR, CINT, CSTRUCT or CFUNC */
	int tag;   /* the struct, also the one a function pointer returns */
	int ident; /* VARIABLE, or POINTER or ARRAY for such a typedef */
	int dim;   /* the array length, for an ARRAY typedef */
	int fptr;  /* for CFUNC: what the function returns, as at FPTR */
};

/* A declarator, combined with its type specifier. */
struct decl {
	int ident; /* VARIABLE, POINTER or ARRAY */
	int type;  /* CCHAR, CINT, CSTRUCT or CFUNC */
	int tag;   /* as in Tspec */
	int dim;   /* for ARRAY: the length, 0 if "[]" */
	int fptr;  /* for CFUNC: what the function returns, as at FPTR */
	int isfp;  /* true if written as "(*name)(...)" */
};

/*
 * A struct tag.  Its members form a chain through membtab, in the
 * order they were declared, so a struct defined inside another one's
 * member list does not break up the outer struct's members.
 */
struct stag {
	char name[NAMESIZE];
	int size;     /* bytes, a multiple of align */
	int align;    /* alignment of the strictest member */
	int first;    /* index of the first member, else -1 */
	int last;     /* index of the last member, else -1 */
	int complete; /* true once the member list was read */
	int kind;     /* STRUCT or UNION */
};

/* cc1.c -- global state */
extern char bell;     /* audible alarm on errors? */
extern char monitor;  /* monitor function headers? */
extern char optimize; /* optimize the staging buffer? */
extern char pauseerr; /* pause for the operator on errors? */

extern char *stage;                /* output staging buffer */
extern char *stagelast;            /* last address in stage */
extern char *stagenext;            /* next address in stage */
extern char *symtab;               /* symbol table */
extern char *litq;                 /* literal pool */
extern char *macn;                 /* macro name buffer */
extern char *macq;                 /* macro string buffer */
extern char *pline;                /* parsing buffer */
extern char *mline;                /* macro buffer */
extern char *line;                 /* points to pline or mline */
extern char *lptr;                 /* scan position within line */
extern char *glbptr;               /* next free global symbol entry */
extern char *locptr;               /* next free local symbol entry */
extern char *cptr, *cptr2, *cptr3; /* work pointers */
extern char quote[2];              /* literal string holding '"' */
extern char msname[NAMESIZE];      /* macro symbol name */
extern char ssname[NAMESIZE];      /* static symbol name */

extern Binop op[16];  /* signed binary operators */
extern Binop op2[16]; /* unsigned counterparts */

extern Stag tags[NUMTAGS]; /* struct tags, from index 1 */
extern int ntags;          /* last tag index in use */
extern char *membtab;      /* struct members, SYMMAX bytes each */
extern int nmembs;         /* members in use */

#define NUMFIX 200
extern char *fixsym;       /* while set: record address loads of others */
extern char *fixpos[NUMFIX]; /* the number fields getloc() wrote */
extern int nfix;

extern int statlab;  /* label of the static local being defined, else 0 */
extern int filestatic; /* true while a file-scope static is declared */
extern int fntag;    /* tag of the struct the function returns, else 0 */
extern int fnretlab; /* label of the function's struct return buffer */

extern char *incdir; /* include directory from -I, else NULL */
extern int nerrors;  /* errors reported so far */

extern int argcs;    /* saved argc */
extern char **argvs; /* saved argv */

extern Word *swnext; /* next free switch table entry */
extern Word *swend;  /* last usable switch table entry */
extern int *wq;     /* while queue */
extern int *wqptr;  /* next free while queue entry */

extern int argstk, argtop; /* function argument stack offsets */
extern int beglab;         /* label of the first function */
extern int cch, nch;       /* current and next input characters */
extern int ccode;          /* non-zero while parsing C, zero in #asm */
extern int csp;            /* compiler relative stack pointer */
extern int declared;       /* local bytes declared, -1 when done */
extern int eof;            /* set once input is exhausted */
extern int errflag;        /* non-zero after the first error */
extern int filearg;        /* current file argument index */
extern int files;          /* non-zero if files were named */
extern int func1;          /* true until the first function is seen */
extern int iflevel;        /* #if... nesting level */
extern int lastst;         /* last statement type compiled */
extern int litlab;         /* label assigned to the literal pool */
extern int litptr;         /* next free literal pool byte */
extern int macptr;         /* next free macro pool byte */
extern int ncmp;           /* open compound statements */
extern int nogo;           /* > 0 disables goto statements */
extern int noloc;          /* > 0 disables block locals */
extern int nxtlab;         /* next available label number */
extern int opindex;        /* index of the matched operator */
extern int opsize;         /* length of that operator in bytes */
extern int pptr;           /* index into the parsing buffer */
extern int skiplevel;      /* level at which #if... skipping began */
extern int swactive;       /* true inside a switch */
extern int swdefault;      /* default label number, else 0 */
extern int swused;         /* true if any switch was compiled */

extern FILE *input;  /* current source file */
extern FILE *input2; /* current include file */
extern FILE *output; /* assembly output */
extern FILE *listfp; /* listing device, else NULL */

/* cc11.c -- driver */
void dumplits(int size);
void dumpzero(int size, int count);
void openfile(void);

/* cc12.c -- declarations */
void doinclude(void);
int dodeclare(int class);
void declloc(Tspec *ts);
int structkw(void);
int dostruct(int kind);
int isstructval(Lvalue *lv);
char *findmemb(int tag, const char *name);
int memblen(const char *m);
int objsize(int type, int tag);
int typesize(void);
void declstatic(void);
int fpdecl(int *dim);
int needsub(void);

/* cc14.c -- type specifiers, declarators and typedefs */
int typespec(Tspec *ts);
int istypename(void);
int declarator(Tspec *ts, Decl *d);
void dotypedef(void);
void newfunc(void);

/* cc13.c -- statements */
int statement(void);
void ns(void);
void doasm(void);

/* cc21.c -- lexer and symbol table */
void junk(void);
int endst(void);
void illname(void);
void multidef(const char *sname);
void needtoken(const char *str);
void needlval(void);
char *findglb(const char *sname);
char *findloc(const char *sname);
char *addsym(const char *sname, int id, int typ, int value,
             char **lgptrptr, int class, int tag);
char *nextsym(char *entry);
Word getint(char *addr, int len);
void putint(Word i, char *addr, int len);
int symname(char *sname, int ucase);
int getlabel(void);
void postlabel(int label);
void printlabel(int label);
int alpha(int c);
int an(int c);
void addwhile(int *ptr);
void delwhile(void);
int *readwhile(int *ptr);
int white(void);
int gch(void);
void bump(int n);
void killline(void);
int inbyte(void);
void readline(void);

/* cc22.c -- preprocessor and output */
void preprocess(void);
void addmac(void);
int search(const char *sname, char *buf, int len, char *end, int max,
           int off);
void setstage(char **before, char **start);
void clearstage(char *before, char *start);
void outdec(Word number);
void ol(const char *ptr);
void ot(const char *ptr);
void outlab(const char *ptr);
void outstr(const char *ptr);
int outbyte(int c);
void cout(int c, FILE *fp);
void sout(const char *s, FILE *fp);
void lout(const char *s, FILE *fp);
void nl(void);
void col(void);
void error(const char *msg);
int streq(const char *s1, const char *s2);
int astreq(const char *s1, const char *s2, int len);
int match(const char *lit);
int amatch(const char *lit, int len);
int nextop(const char *list);
void blanks(void);

/* cc31.c -- expression parser */
void plnge2(Binop oper, Binop oper2, Hier hier, Lvalue *lv, Lvalue *lv2);
void expression(int *isconst, Word *val);
int hier1(Lvalue *lv);

/* cc32.c -- unary operators, subscripts and calls */
int hier13(Lvalue *lv);

/* cc33.c -- lvalues, tests and literals */
int scale(Binop oper, Lvalue *v1, Lvalue *v2);
int stepsize(Lvalue *lv);
void result(Lvalue *lv, Lvalue *lv2);
void step(Stepop oper, Lvalue *lv);
void store(Lvalue *lv);
void rvalue(Lvalue *lv);
void test(int label, int parens);
int constexpr(Word *val);
int constant(Lvalue *lv);
void loadconst(Word val);
void loadconst2(Word val);
void address(char *ptr);
int qstr(Word *val);
void stowlit(Word value, int size);

/* cc41.c -- code generator */
void header(void);
void trailer(void);
void loadargc(int val);
void entry(void);
void external(const char *name);
void indirect(Lvalue *lv);
void getmem(Lvalue *lv);
void getloc(char *sym);
void symlabel(char *sym);
void putmem(Lvalue *lv);
void putstk(Lvalue *lv);
void move(void);
void swap(void);
void immed(void);
void immed2(void);
void push(void);
void smartpop(Lvalue *lv, char *start);
void pop(void);
void sw(int tablab);
void ffcall(const char *sname);
void ffcallsym(char *sym);
void ffret(void);
void callstk(void);
void jump(int label);
void testjump(int label);
void zerojump(Jumper oper, int label, Lvalue *lv);
void defstorage(int size);
void point(void);
int modstk(int newsp, int save);
void scalereg(int n);
void movestruct(int size);
void zeromem(int size);
int pushstruct(int size);
void loadstk(int off);

/* cc42.c -- smol instruction emitters */
void ffadd(void);
void ffsub(void);
void ffmult(void);
void ffdiv(void);
void ffmod(void);
void ffor(void);
void ffxor(void);
void ffand(void);
void lneg(void);
void ffasr(void);
void ffasl(void);
void neg(void);
void com(void);
void inc(int n);
void dec(int n);
void ffeq(void);
void ffne(void);
void fflt(void);
void ffle(void);
void ffgt(void);
void ffge(void);
void ult(void);
void ule(void);
void ugt(void);
void uge(void);
void eq0(int label);
void ne0(int label);
void lt0(int label);
void le0(int label);
void gt0(int label);
void ge0(int label);
void ult0(int label);
void peephole(char *ptr);

#endif /* CC_H */
