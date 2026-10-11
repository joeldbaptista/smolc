/* sas - assembler for the smol register machine */
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arg.h"
#include "smol.h"
#include "common.h"

#define MAXLINE   512
#define MAXLABEL   64
#define MAXSTR    256
#define MAXSYM  16384
#define MAXTOK     64
#define MAXOPD      3
#define MAXFILES  256
#define MAXDATA  (MEMSZ - DATABASE)

typedef enum {
	TOK_IDENT,
	TOK_NUM,
	TOK_STR,
	TOK_OP,
} TokKind;

typedef struct {
	TokKind kind;
	char    s[MAXSTR];     /* source text, local names mangled, or a string */
	int     len;           /* bytes in s, for TOK_STR */
	int64_t num;           /* value, for TOK_NUM */
} Tok;

typedef enum {
	SYM_CODE,
	SYM_DATA,
} SymKind;

typedef struct {
	char    name[MAXLABEL];
	int64_t addr;          /* instruction index, or data byte address */
	SymKind kind;
} Sym;

typedef struct {
	char *text;
	int   file;            /* index into inpaths */
	int   lineno;
} Line;

/* an operand: a register, an immediate, or a memory reference */
typedef enum {
	OPD_REG,
	OPD_IMM,
	OPD_MEM,
} OpdKind;

typedef struct {
	OpdKind    kind;
	int        reg;        /* OPD_REG, or the base register of OPD_MEM */
	int64_t    val;        /* OPD_IMM value, or OPD_MEM offset or address */
	int        isabs;      /* OPD_MEM: val is the address, with no base */
	int        isbyte;     /* OPD_MEM: written "byte [...]" */
	const Tok *at;         /* first token, for messages */
} Opd;

/* how a mnemonic's operands are read */
typedef enum {
	F_MOV,                 /* mov: the operands choose the opcode */
	F_MOVSX,               /* movsx r, byte [m] */
	F_ALU,                 /* op r, r|imm */
	F_REG,                 /* op r */
	F_CMP,                 /* cmp r, r|imm */
	F_JUMP,                /* op label, or op r */
	F_JCC,                 /* op label */
	F_NONE,                /* op */
	F_XCHG,                /* xchg r, r */
	F_SYS,                 /* sys n */
} Form;

typedef struct {
	char *name;
	Op    op;
	Form  form;
} Mnem;

static void lerr(const char *fmt, ...) __attribute__((noreturn));
static void readfile(int file);
static int signpos(const Tok *t, int n);
static int lex(int file, const char *s, Tok *t);
static int isop(const Tok *t, const char *op);
static int iskw(const Tok *t, const char *kw);
static int regnum(const Tok *t);
static const Mnem *findmnem(const char *name);
static int isreserved(const char *name);
static int64_t expectnum(const Tok *t);
static const char *expectname(const Tok *t);
static Sym *findsym(const char *name);
static void addsym(const char *name, int64_t addr, SymKind kind);
static int64_t symaddr(const Tok *t, SymKind expect);
static void bindpending(SymKind kind, int64_t addr);
static int value(const Tok *t, int i, int n, int64_t *val);
static int operand(const Tok *t, int i, int n, Opd *o);
static int operands(const Tok *t, int n, Opd *o);
static int isdata(const Tok *t);
static int datasize(const Tok *t, int n);
static void emitdata(const Tok *t, int n);
static void emit(Instr *in, Op op, int rd, int rs, int rt, int64_t imm, int isimm);
static int wantreg(const Opd *o);
static void parsemov(const Opd *o, int nopd, Instr *out);
static void parseinstr(const Tok *t, int n, Instr *out);
static void pass(int final);
static void writeout(const char *outpath);
static void usage(void);

static const char opers[] = ",[]+-:";

static const Mnem mnems[] = {
	{"mov", OP_MOV, F_MOV}, {"movsx", OP_LOADB, F_MOVSX},
	{"add", OP_ADD, F_ALU}, {"sub", OP_SUB, F_ALU},
	{"mul", OP_MUL, F_ALU}, {"div", OP_DIV, F_ALU},
	{"rem", OP_REM, F_ALU}, {"and", OP_AND, F_ALU},
	{"or", OP_OR, F_ALU}, {"xor", OP_XOR, F_ALU},
	{"shl", OP_SHL, F_ALU}, {"sar", OP_SHR, F_ALU},
	{"inc", OP_INC, F_REG}, {"dec", OP_DEC, F_REG},
	{"neg", OP_NEG, F_REG}, {"not", OP_COM, F_REG},
	{"push", OP_PUSH, F_REG}, {"pop", OP_POP, F_REG},
	{"sete", OP_SEQ, F_REG}, {"setne", OP_SNE, F_REG},
	{"setl", OP_SLT, F_REG}, {"setle", OP_SLE, F_REG},
	{"setg", OP_SGT, F_REG}, {"setge", OP_SGE, F_REG},
	{"cmp", OP_CMP, F_CMP},
	{"jmp", OP_JMP, F_JUMP}, {"call", OP_CALL, F_JUMP},
	{"je", OP_JE, F_JCC}, {"jne", OP_JNE, F_JCC},
	{"jl", OP_JL, F_JCC}, {"jle", OP_JLE, F_JCC},
	{"jg", OP_JG, F_JCC}, {"jge", OP_JGE, F_JCC},
	{"ret", OP_RET, F_NONE}, {"hlt", OP_HALT, F_NONE},
	{"xchg", OP_XCHG, F_XCHG}, {"sys", OP_SYS, F_SYS},
};

static const char *keywords[] = {"byte", "db", "dq", "resb"};

static char *inpaths[MAXFILES];
static int ninpaths;

static Line *lines;
static int nlines, maxlines;
static const Line *cur;          /* line being assembled, for messages */

static Sym syms[MAXSYM];
static int nsym;

static Tok pending[MAXSYM];      /* labels waiting for their next item */
static int npending;

static Instr instrs[MAXINSTR];
static int ninstr;               /* also the code address in pass 1 */
static uint8_t data[MAXDATA];
static int ndata;                /* also the data offset in pass 1 */
static int entry;

/* dies with the current file and line as a prefix */
static void
lerr(const char *fmt, ...)
{
	va_list ap;

	if (cur)
		fprintf(stderr, "%s:%d: ", inpaths[cur->file], cur->lineno);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}

static void
readfile(int file)
{
	FILE *fp;
	char buf[MAXLINE];
	size_t len;
	int lineno;

	if (!(fp = fopen(inpaths[file], "r")))
		die("%s:", inpaths[file]);

	lineno = 0;
	while (fgets(buf, sizeof(buf), fp)) {
		lineno++;
		len = strlen(buf);
		if (len > 0 && buf[len-1] == '\n')
			buf[--len] = '\0';
		else if (!feof(fp))
			die("%s:%d: line too long (max %d)", inpaths[file],
			    lineno, MAXLINE - 2);
		if (nlines == maxlines) {
			maxlines = maxlines ? 2 * maxlines : 1024;
			if (!(lines = realloc(lines, maxlines * sizeof(*lines))))
				die("out of memory");
		}
		if (!(lines[nlines].text = malloc(len + 1)))
			die("out of memory");
		memcpy(lines[nlines].text, buf, len + 1);
		lines[nlines].file = file;
		lines[nlines].lineno = lineno;
		nlines++;
	}

	fclose(fp);
}

/*
 * True if a '-' after the n tokens t[] starts a negative number. It
 * does where an operand or value starts: first in a statement, after
 * the mnemonic, a ',' or a '['. After a register, a name, a number or
 * a ']' it subtracts, as in "[r7 - 8]" or "name - 8".
 */
static int
signpos(const Tok *t, int n)
{
	int first;

	if (n == 0)
		return 1;
	first = (n >= 2 && isop(&t[1], ":")) ? 2 : 0;
	if (n - 1 == first)
		return 1;    /* just after the mnemonic or directive */
	return isop(&t[n-1], ",") || isop(&t[n-1], "[");
}

/*
 * Splits s into tokens, up to a ';' comment. A name that starts with
 * '.' is local to its file, so it gets the file index appended, which
 * makes it distinct from the same name in any other file.
 */
static int
lex(int file, const char *s, Tok *t)
{
	const char *p;
	char *end;
	size_t len;
	int n, c;

	n = 0;
	p = s;
	for (;;) {
		while (*p == ' ' || *p == '\t' || *p == '\r')
			p++;
		if (*p == '\0' || *p == ';')
			break;
		if (n >= MAXTOK)
			lerr("too many tokens");

		if (*p == '"') {
			/* a string, for db */
			t[n].kind = TOK_STR;
			t[n].len = 0;
			for (p++; *p != '"'; p++) {
				if (*p == '\0')
					lerr("unterminated string");
				c = *p;
				if (c == '\\') {
					switch (*++p) {
					case 'n':  c = '\n'; break;
					case 't':  c = '\t'; break;
					case '0':  c = '\0'; break;
					case '\\': c = '\\'; break;
					case '"':  c = '"'; break;
					default:
						lerr("unknown escape '\\%c'", *p);
					}
				}
				if (t[n].len == MAXSTR)
					lerr("string too long");
				t[n].s[t[n].len++] = (char)c;
			}
			p++;
			n++;
			continue;
		}

		if (isdigit((unsigned char)*p) ||
		    (*p == '-' && isdigit((unsigned char)p[1]) && signpos(t, n))) {
			errno = 0;
			t[n].kind = TOK_NUM;
			if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
				t[n].num = strtoll(p, &end, 16);
			else
				t[n].num = strtoll(p, &end, 10);
			if (errno == ERANGE || isalnum((unsigned char)*end) ||
			    *end == '_' || *end == '.')
				lerr("invalid number");
			len = end - p;
		} else if (isalpha((unsigned char)*p) || *p == '_' || *p == '.') {
			t[n].kind = TOK_IDENT;
			len = 1;
			while (isalnum((unsigned char)p[len]) || p[len] == '_' ||
			    p[len] == '.')
				len++;
		} else {
			if (!strchr(opers, *p))
				lerr("unexpected character '%c'", *p);
			t[n].kind = TOK_OP;
			len = 1;
		}

		if (len + 8 >= MAXLABEL)
			lerr("token too long");
		memcpy(t[n].s, p, len);
		t[n].s[len] = '\0';
		if (t[n].kind == TOK_IDENT && *p == '.')
			sprintf(t[n].s + len, "@%d", file);
		p += len;
		n++;
	}

	return n;
}

static int
isop(const Tok *t, const char *op)
{
	return t->kind == TOK_OP && strcmp(t->s, op) == 0;
}

static int
iskw(const Tok *t, const char *kw)
{
	return t->kind == TOK_IDENT && strcmp(t->s, kw) == 0;
}

static int
regnum(const Tok *t)
{
	int i;

	if (t->kind != TOK_IDENT)
		return -1;

	for (i = 0; i < NREG; i++)
		if (strcmp(regnames[i], t->s) == 0)
			return i;

	return -1;
}

static const Mnem *
findmnem(const char *name)
{
	size_t i;

	for (i = 0; i < sizeof(mnems)/sizeof(mnems[0]); i++)
		if (strcmp(mnems[i].name, name) == 0)
			return &mnems[i];

	return NULL;
}

static int
isreserved(const char *name)
{
	size_t i;

	for (i = 0; i < NREG; i++)
		if (strcmp(regnames[i], name) == 0)
			return 1;
	for (i = 0; i < sizeof(keywords)/sizeof(keywords[0]); i++)
		if (strcmp(keywords[i], name) == 0)
			return 1;

	return findmnem(name) != NULL;
}

static int64_t
expectnum(const Tok *t)
{
	if (t->kind != TOK_NUM)
		lerr("expected number, got '%s'", t->s);

	return t->num;
}

static const char *
expectname(const Tok *t)
{
	if (t->kind != TOK_IDENT || isreserved(t->s))
		lerr("expected label, got '%s'", t->s);

	return t->s;
}

static Sym *
findsym(const char *name)
{
	int i;

	for (i = 0; i < nsym; i++)
		if (strcmp(syms[i].name, name) == 0)
			return &syms[i];

	return NULL;
}

static void
addsym(const char *name, int64_t addr, SymKind kind)
{
	if (isreserved(name))
		lerr("'%s' is a reserved word", name);
	if (findsym(name))
		lerr("duplicate label '%s'", name);
	if (nsym >= MAXSYM)
		lerr("too many labels (max %d)", MAXSYM);

	strcpy(syms[nsym].name, name);
	syms[nsym].addr = addr;
	syms[nsym].kind = kind;
	nsym++;
}

/* address of label t, which must be of kind expect, or of any kind if expect < 0 */
static int64_t
symaddr(const Tok *t, SymKind expect)
{
	const char *name;
	Sym *s;

	name = expectname(t);
	if (!(s = findsym(name)))
		lerr("undefined label '%s'", name);
	if ((int)expect >= 0 && s->kind != expect)
		lerr("label '%s' is a %s label, expected %s", name,
		    s->kind == SYM_CODE ? "code" : "static data",
		    expect == SYM_CODE ? "code" : "static data");

	return s->addr;
}

/*
 * A label line does not say whether it names code or data. It names
 * whichever item comes next, so pending labels are bound when that
 * item is seen.
 */
static void
bindpending(SymKind kind, int64_t addr)
{
	int i;

	for (i = 0; i < npending; i++)
		addsym(pending[i].s, addr, kind);
	npending = 0;
}

/*
 * Parses a value at t[i]: a number, or a label's address, maybe with
 * "+ k" or "- k" after it. Returns the token count.
 */
static int
value(const Tok *t, int i, int n, int64_t *val)
{
	if (i >= n)
		lerr("expected value");
	if (t[i].kind == TOK_NUM) {
		*val = t[i].num;
		return 1;
	}
	*val = symaddr(&t[i], (SymKind)-1);
	if (i + 1 < n && (isop(&t[i+1], "+") || isop(&t[i+1], "-"))) {
		if (i + 2 >= n)
			lerr("expected number after '%s'", t[i+1].s);
		if (isop(&t[i+1], "+"))
			*val += expectnum(&t[i+2]);
		else
			*val -= expectnum(&t[i+2]);
		return 3;
	}

	return 1;
}

/*
 * Parses one operand at t[i]: "rN", a value, or a memory reference
 * "[rN]", "[rN + k]", "[rN - k]", "[name]" or "[name + k]", maybe after
 * "byte". Returns the token count.
 */
static int
operand(const Tok *t, int i, int n, Opd *o)
{
	int k;

	memset(o, 0, sizeof(*o));
	if (i >= n)
		lerr("missing operand");
	o->at = &t[i];
	k = i;
	if (iskw(&t[k], "byte")) {
		o->isbyte = 1;
		if (++k >= n || !isop(&t[k], "["))
			lerr("expected '[' after 'byte'");
	}
	if (!isop(&t[k], "[")) {
		if ((o->reg = regnum(&t[k])) >= 0) {
			o->kind = OPD_REG;
			return 1;
		}
		o->kind = OPD_IMM;
		return value(t, k, n, &o->val);
	}

	o->kind = OPD_MEM;
	if (++k >= n)
		lerr("expected memory operand");
	if ((o->reg = regnum(&t[k])) >= 0) {
		k++;
		if (k < n && (isop(&t[k], "+") || isop(&t[k], "-"))) {
			if (k + 1 >= n)
				lerr("expected number after '%s'", t[k].s);
			o->val = expectnum(&t[k+1]);
			if (isop(&t[k], "-"))
				o->val = -o->val;
			k += 2;
		}
	} else {
		o->reg = 0;
		o->isabs = 1;
		k += value(t, k, n, &o->val);
	}
	if (k >= n || !isop(&t[k], "]"))
		lerr("expected ']'");

	return k + 1 - i;
}

/* parses the comma-separated operands after the mnemonic t[0] */
static int
operands(const Tok *t, int n, Opd *o)
{
	int i, nopd;

	nopd = 0;
	i = 1;
	while (i < n) {
		if (nopd == MAXOPD)
			lerr("too many operands");
		i += operand(t, i, n, &o[nopd++]);
		if (i < n) {
			if (!isop(&t[i], ","))
				lerr("expected ',' between operands");
			if (++i == n)
				lerr("expected operand after ','");
		}
	}

	return nopd;
}

/* true if the statement t is a data directive */
static int
isdata(const Tok *t)
{
	return iskw(t, "db") || iskw(t, "dq") || iskw(t, "resb");
}

/* bytes a data directive occupies; it also checks the directive's shape */
static int
datasize(const Tok *t, int n)
{
	int i, size;

	if (iskw(&t[0], "resb")) {
		if (n != 2 || expectnum(&t[1]) < 0)
			lerr("expected 'resb <count>'");
		return (int)t[1].num;
	}

	if (n == 1)
		lerr("'%s' needs at least one value", t[0].s);
	size = 0;
	i = 1;
	while (i < n) {
		if (iskw(&t[0], "db") && t[i].kind == TOK_STR) {
			size += t[i].len;
		} else if (iskw(&t[0], "db")) {
			expectnum(&t[i]);
			size++;
		} else {
			if (t[i].kind != TOK_NUM && t[i].kind != TOK_IDENT)
				lerr("expected value, got '%s'", t[i].s);
			if (i + 1 < n && (isop(&t[i+1], "+") ||
			    isop(&t[i+1], "-")))
				i += 2;
			size += WORDSZ;
		}
		i++;
		if (i < n) {
			if (!isop(&t[i], ","))
				lerr("expected ',' between values");
			if (++i == n)
				lerr("expected value after ','");
		}
	}

	return size;
}

static void
emitdata(const Tok *t, int n)
{
	int64_t v;
	int i;

	if (iskw(&t[0], "resb")) {
		memset(&data[ndata], 0, t[1].num);
		ndata += (int)t[1].num;
		return;
	}

	for (i = 1; i < n; i++) {
		if (t[i].kind == TOK_STR) {
			memcpy(&data[ndata], t[i].s, t[i].len);
			ndata += t[i].len;
		} else if (iskw(&t[0], "db")) {
			if (t[i].num < -128 || t[i].num > 255)
				lerr("byte value %lld out of range",
				    (long long)t[i].num);
			data[ndata++] = (uint8_t)t[i].num;
		} else {
			i += value(t, i, n, &v) - 1;
			memcpy(&data[ndata], &v, WORDSZ);
			ndata += WORDSZ;
		}
		if (i + 1 < n)
			i++;    /* skip the ',' */
	}
}

static void
emit(Instr *in, Op op, int rd, int rs, int rt, int64_t imm, int isimm)
{
	in->op = op;
	in->rd = rd;
	in->rs = rs;
	in->rt = rt;
	in->imm = imm;
	in->isimm = isimm;
}

static int
wantreg(const Opd *o)
{
	if (o->kind != OPD_REG)
		lerr("expected register, got '%s'", o->at->s);

	return o->reg;
}

/*
 *   mov rA, rB              mov rA, number or label
 *   mov rA, [m]             mov [m], rA
 *   mov byte [m], rA        (a byte load is movsx)
 */
static void
parsemov(const Opd *o, int nopd, Instr *out)
{
	if (nopd != 2)
		lerr("'mov' takes two operands");

	if (o[0].kind == OPD_MEM) {
		emit(out, o[0].isbyte ? OP_STOREB : OP_STORE, wantreg(&o[1]),
		    o[0].reg, 0, o[0].val, o[0].isabs);
		return;
	}
	if (o[1].kind == OPD_MEM && o[1].isbyte)
		lerr("a byte load is 'movsx <register>, byte [...]'");
	switch (o[1].kind) {
	case OPD_REG:
		emit(out, OP_MOV, wantreg(&o[0]), o[1].reg, 0, 0, 0);
		break;
	case OPD_IMM:
		emit(out, OP_PUT, wantreg(&o[0]), 0, 0, o[1].val, 0);
		break;
	case OPD_MEM:
		emit(out, OP_LOAD, wantreg(&o[0]), o[1].reg, 0, o[1].val,
		    o[1].isabs);
		break;
	}
}

/* assembles the instruction t into out */
static void
parseinstr(const Tok *t, int n, Instr *out)
{
	Opd o[MAXOPD];
	const Mnem *m;
	int nopd, r;

	if (t[0].kind != TOK_IDENT || !(m = findmnem(t[0].s)))
		lerr("unknown instruction '%s'", t[0].s);
	nopd = operands(t, n, o);

	switch (m->form) {
	case F_MOV:
		parsemov(o, nopd, out);
		return;
	case F_MOVSX:
		if (nopd != 2 || o[1].kind != OPD_MEM || !o[1].isbyte)
			lerr("expected 'movsx <register>, byte [...]'");
		emit(out, OP_LOADB, wantreg(&o[0]), o[1].reg, 0, o[1].val,
		    o[1].isabs);
		return;
	case F_ALU:
	case F_CMP:
		if (nopd != 2)
			lerr("'%s' takes two operands", m->name);
		if (o[1].kind == OPD_MEM)
			lerr("'%s' takes a register or a number", m->name);
		if (o[1].kind == OPD_IMM && o[1].val == 0 &&
		    (m->op == OP_DIV || m->op == OP_REM))
			lerr("division by zero");
		r = o[1].kind == OPD_REG ? o[1].reg : 0;
		if (m->form == F_CMP)
			emit(out, m->op, 0, wantreg(&o[0]), r, o[1].val,
			    o[1].kind == OPD_IMM);
		else
			emit(out, m->op, wantreg(&o[0]), r, 0, o[1].val,
			    o[1].kind == OPD_IMM);
		return;
	case F_REG:
		if (nopd != 1)
			lerr("'%s' takes one register", m->name);
		r = wantreg(&o[0]);
		emit(out, m->op, r, r, 0, 0, 0);
		return;
	case F_JUMP:
	case F_JCC:
		if (nopd != 1)
			lerr("'%s' takes one label", m->name);
		if (o[0].kind == OPD_REG && m->form == F_JUMP) {
			emit(out, m->op == OP_JMP ? OP_JMPR : OP_CALLR, 0,
			    o[0].reg, 0, 0, 0);
			return;
		}
		if (o[0].kind != OPD_IMM || o[0].at->kind != TOK_IDENT)
			lerr("'%s' takes a label", m->name);
		emit(out, m->op, 0, 0, 0, symaddr(o[0].at, SYM_CODE), 0);
		return;
	case F_NONE:
		if (nopd != 0)
			lerr("'%s' takes no operand", m->name);
		emit(out, m->op, 0, 0, 0, 0, 0);
		return;
	case F_XCHG:
		if (nopd != 2)
			lerr("'xchg' takes two registers");
		emit(out, OP_XCHG, wantreg(&o[0]), wantreg(&o[1]), 0, 0, 0);
		return;
	case F_SYS:
		if (nopd != 1 || o[0].kind != OPD_IMM || o[0].val < 0 ||
		    o[0].val >= NSYS)
			lerr("'sys' takes a service number from 0 to %d",
			    NSYS - 1);
		emit(out, OP_SYS, 0, 0, 0, o[0].val, 0);
		return;
	}
}

/*
 * Pass 1 (final = 0) records label addresses and sizes: every
 * instruction line is one instruction. Pass 2 (final = 1) assembles
 * instructions and data, resolving references.
 */
static void
pass(int final)
{
	Tok t[MAXTOK];
	int i, n, file, sz;
	Tok *p;

	ninstr = 0;
	ndata = 0;
	npending = 0;
	file = -1;
	for (i = 0; i < nlines; i++) {
		cur = &lines[i];
		if (cur->file != file) {
			/* a label at the end of a file names the code that follows */
			if (!final)
				bindpending(SYM_CODE, ninstr);
			file = cur->file;
		}
		if (!(n = lex(cur->file, cur->text, t)))
			continue;
		p = t;

		if (n >= 2 && isop(&t[1], ":")) {
			if (!final) {
				if (npending == MAXSYM)
					lerr("too many labels");
				expectname(&t[0]);
				pending[npending++] = t[0];
			}
			if (n == 2)
				continue;
			p = t + 2;
			n -= 2;
		}

		if (isdata(p)) {
			sz = datasize(p, n);
			if (sz > MAXDATA - ndata)
				lerr("too much static data");
			if (final)
				emitdata(p, n);
			else {
				bindpending(SYM_DATA, DATABASE + ndata);
				ndata += sz;
			}
			continue;
		}

		if (final) {
			parseinstr(p, n, &instrs[ninstr++]);
		} else {
			bindpending(SYM_CODE, ninstr);
			if (++ninstr > MAXINSTR)
				lerr("too many instructions (max %d)", MAXINSTR);
		}
	}
	cur = NULL;
	if (!final)
		bindpending(SYM_CODE, ninstr);
}

static void
writeout(const char *outpath)
{
	FILE *fp;
	Header hdr;
	EncInstr e;
	int i;

	if (!(fp = fopen(outpath, "wb")))
		die("%s:", outpath);

	memcpy(hdr.magic, SMOL_MAGIC, 4);
	hdr.ninstr = ninstr;
	hdr.entry = entry;
	hdr.ndata = ndata;
	if (fwrite(&hdr, sizeof(hdr), 1, fp) != 1)
		die("%s: write failed", outpath);

	for (i = 0; i < ninstr; i++) {
		encode(&instrs[i], &e);
		if (fwrite(&e, sizeof(e), 1, fp) != 1)
			die("%s: write failed", outpath);
	}

	if (ndata > 0 && fwrite(data, ndata, 1, fp) != 1)
		die("%s: write failed", outpath);

	if (fclose(fp) == EOF)
		die("%s:", outpath);
}

static void
usage(void)
{
	die("usage: %s -o out [-i] source.sas ...", argv0);
}

int
main(int argc, char *argv[])
{
	char *outpath;
	Sym *s;
	int i;

	outpath = NULL;

	ARGBEGIN {
	case 'i':
		if (ninpaths == MAXFILES)
			die("too many input files (max %d)", MAXFILES);
		inpaths[ninpaths++] = EARGF(usage());
		break;
	case 'o':
		outpath = EARGF(usage());
		break;
	default:
		usage();
	} ARGEND

	for (; argc > 0; argc--, argv++) {
		if (ninpaths == MAXFILES)
			die("too many input files (max %d)", MAXFILES);
		inpaths[ninpaths++] = argv[0];
	}
	if (!outpath || ninpaths == 0)
		usage();

	for (i = 0; i < ninpaths; i++)
		readfile(i);

	pass(0);
	addsym(ENDSYM, DATABASE + WORDUP(ndata), SYM_DATA);
	if (!(s = findsym("main")))
		die("missing required label 'main' (entry point)");
	if (s->kind != SYM_CODE)
		die("'main' must be a code label");
	entry = (int)s->addr;
	if (ninstr == 0)
		die("no instructions");

	pass(1);
	writeout(outpath);

	return 0;
}
