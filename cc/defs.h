/*
 * Small-C compiler -- tunable parameters.
 *
 * Values under "target machine" describe the smol object code the
 * compiler emits, not the host the compiler runs on.  They must not be
 * changed to match the host.
 */

#ifndef DEFS_H
#define DEFS_H

/* compile options */
#define OPTIMIZE    /* compile the output optimizer */
#define COL         /* terminate labels with a colon */
/* #define UPPER */ /* force symbols to upper case */
#define LINK        /* will be used with a linking loader */

/* target machine */
#define BPW 8     /* bytes per word */
#define LBPW 3    /* log2(BPW) */
#define SBPC 1    /* stack bytes per character */
#define ERRCODE 7 /* exit status after a fatal error */

/* symbol table entry layout, as byte offsets */
#define IDENT 0
#define TYPE 1
#define CLASS 2
#define TAG 3  /* struct tag index for CSTRUCT, else 0 */
#define FPTR 4 /* for a function: true if it returns a pointer */
#define PRIV 5 /* true for a file-scope static, whose label is "._name" */
#define DIM 6  /* for an array: its length, else 0 */
#define DIMSIZE 2
#define OFFSET 8
#define NAME 10
#define OFFSIZE (NAME - OFFSET)
#define SYMAVG 18
#define SYMMAX (NAME + NAMESIZE)

/* symbol table size */
#define NUMLOCS 60
#define STARTLOC symtab
#define ENDLOC (symtab + (NUMLOCS * SYMAVG))
#define NUMGLBS 200
#define STARTGLB ENDLOC
#define ENDGLB (ENDLOC + ((NUMGLBS - 1) * SYMMAX))
#define SYMTBSZ (NUMLOCS * SYMAVG + NUMGLBS * SYMMAX)

/*
 * struct tags and their members.  A member entry has the layout of a
 * symbol table entry, with the member's byte offset at OFFSET, so an
 * lvalue can point at it like it points at a variable.
 */
#define NUMTAGS 100  /* tag indexes run from 1; 0 means none */
#define NUMTDEFS 100 /* typedef names */
#define NUMMEMB 400

/*
 * symbol names: 31 significant characters, as C99 guarantees.  A local
 * entry ends with a length byte, which must stay below ' ', so 31 is
 * also the most this layout allows.
 */
#define NAMESIZE 32
#define NAMEMAX 31

/* "switch" table, measured in ints */
#define SWMAX 60 /* cases per switch */
#define SWSIZ 2  /* ints per case: label, value */
#define SWTABSZ (SWMAX * SWSIZ)

/* "while" queue, measured in ints */
#define WQSIZ 3
#define WQTABSZ 30
#define WQMAX (wq + WQTABSZ - WQSIZ)

/* entry offsets in the while queue */
#define WQSP 0
#define WQLOOP 1
#define WQEXIT 2

/* literal pool */
#define LITABSZ 4000
#define LITMAX (LITABSZ - 1)

/* input line */
#define LINEMAX 511
#define LINESIZE 512

/* output staging buffer */
#define STAGESIZE 65536 /* holds a whole local initializer */
#define STAGELIMIT (STAGESIZE - 1)

/* macro (define) pool */
#define MACNBR 130
#define MACNSIZE (MACNBR * (NAMESIZE + 2))
#define MACNEND (macn + MACNSIZE)
#define MACQSIZE (MACNBR * 7)
#define MACMAX (MACQSIZE - 1)

/* identifier classes, stored at IDENT */
enum {
	LABEL,
	VARIABLE,
	ARRAY,
	POINTER,
	FUNCTION
};

/*
 * Types, stored at TYPE.  The low two bits make the type unique within
 * a length; the high bits give the length of the object in bytes.
 */
#define CCHAR (1 << 2)
#define CINT (BPW << 2)

/*
 * A struct or union has no fixed length, so its low bits alone mark it
 * and the TAG byte of the symbol names it.  Use objsize(), never
 * CSTRUCT >> 2, for its length.
 */
#define CSTRUCT 3

/*
 * A function pointer: one word holding a code address.  In its symbol,
 * FPTR holds the type the function returns, with RETPTR added if it
 * returns a pointer to that type, and TAG the struct it returns.
 */
#define CFUNC ((BPW << 2) | 1)
#define RETPTR 0x40

/* the keywords structkw() matches */
enum {
	STRUCT = 1,
	UNION
};

/* storage classes, stored at CLASS.  LABEL above doubles as class 0. */
enum {
	STATIC = 1,
	AUTOMATIC,
	EXTERNAL,
	AUTOEXT,
	MEMBER, /* a struct member, in the member table */
	LSTATIC /* a static local: OFFSET holds its data label */
};

/* statement types */
enum {
	STIF = 1,
	STWHILE,
	STRETURN,
	STBREAK,
	STCONT,
	STASM,
	STEXPR,
	STDO,
	STFOR,
	STSWITCH,
	STCASE,
	STDEF,
	STGOTO,
	STLABEL
};

#endif /* DEFS_H */
