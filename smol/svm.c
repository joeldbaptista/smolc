/* smol - register machine virtual machine */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arg.h"
#include "smol.h"
#include "common.h"

#define MAXFD    32      /* open files, including stdin, stdout, stderr */
#define MAXPATH  4096    /* longest path a program may pass to the host */

typedef struct {
	int64_t regs[NREG];   /* indexed by Reg; regs[SP] is the stack pointer */
	uint8_t mem[MEMSZ];   /* data memory, addressed by byte */
	int     ndata;        /* number of static data bytes at DATABASE */
	int64_t stacklim;     /* lowest address the stack may reach */
	FILE   *fd[MAXFD];    /* host files, indexed by program fd */
	int     zf, sf, of;
	int     pc;
	int     status;       /* exit status */
} Machine;

static int loadprog(const char *inpath, Instr prog[], int max, int *entry, Machine *m);
static void setargs(Machine *m, int argc, char *argv[]);
static int64_t checkaddr(const Machine *m, int64_t a, int size);
static int64_t load(const Machine *m, int64_t a, int isbyte);
static void store(Machine *m, int64_t a, int64_t v, int isbyte);
static void push(Machine *m, int64_t v);
static int64_t pop(Machine *m);
static int64_t srcval(const Machine *m, const Instr *in);
static int64_t cmpval(const Machine *m, const Instr *in);
static const char *getstr(const Machine *m, int64_t a, char *buf);
static int validfd(const Machine *m, int64_t fd);
static int syscall(Machine *m, int64_t n);
static void execute(const Instr *prog, int nprog, int entry, Machine *m);
static void hexdump(FILE *fp, const uint8_t *p, int addr, int n);
static void dumpstate(const char *outpath, const Machine *m);
static void usage(void);

static Instr prog[MAXINSTR];
static Machine machine;

static int
loadprog(const char *inpath, Instr prog[], int max, int *entry, Machine *m)
{
	FILE *fp;
	Header hdr;
	EncInstr e;
	int i;

	if (!(fp = fopen(inpath, "rb")))
		die("%s:", inpath);

	if (fread(&hdr, sizeof(hdr), 1, fp) != 1)
		die("%s: truncated header", inpath);
	if (memcmp(hdr.magic, SMOL_MAGIC, 4) != 0)
		die("%s: bad magic (not a smol binary)", inpath);
	if (hdr.ninstr < 0 || hdr.ninstr > max)
		die("%s: invalid instruction count %d", inpath, hdr.ninstr);
	if (hdr.entry < 0 || hdr.entry >= hdr.ninstr)
		die("%s: invalid entry point %d", inpath, hdr.entry);
	if (hdr.ndata < 0 || hdr.ndata > MEMSZ / 2)
		die("%s: invalid data byte count %d", inpath, hdr.ndata);

	for (i = 0; i < hdr.ninstr; i++) {
		if (fread(&e, sizeof(e), 1, fp) != 1)
			die("%s: truncated instruction stream", inpath);
		if (e.op > OP_HALT)
			die("%s: invalid opcode %d at instruction %d", inpath, e.op, i);
		if (e.rd >= NREG || e.rs >= NREG || e.rt >= NREG)
			die("%s: invalid register at instruction %d", inpath, i);
		if (e.isimm > 1)
			die("%s: invalid immediate flag at instruction %d",
			    inpath, i);
		if ((e.op == OP_JMP || e.op == OP_CALL ||
		    (e.op >= OP_JE && e.op <= OP_JNE)) &&
		    (e.imm < 0 || e.imm >= hdr.ninstr))
			die("%s: invalid code address %lld at instruction %d",
			    inpath, (long long)e.imm, i);
		if (e.op == OP_SYS && (e.imm < 0 || e.imm >= NSYS))
			die("%s: invalid service %lld at instruction %d",
			    inpath, (long long)e.imm, i);

		decode(&e, &prog[i]);
	}

	if (hdr.ndata > 0 && fread(&m->mem[DATABASE], hdr.ndata, 1, fp) != 1)
		die("%s: truncated data section", inpath);
	m->ndata = hdr.ndata;
	m->stacklim = DATABASE + WORDUP(hdr.ndata);

	fclose(fp);
	*entry = hdr.entry;
	return hdr.ninstr;
}

/*
 * Copies the program arguments to the top of memory, followed below
 * by a null-terminated vector of their addresses. On entry r1 holds
 * argc, r2 the vector address, and the stack starts below the vector.
 */
static void
setargs(Machine *m, int argc, char *argv[])
{
	int64_t top, vec[256];
	size_t len;
	int i;

	if (argc > 255)
		die("too many program arguments (max 255)");

	top = MEMSZ;
	for (i = argc - 1; i >= 0; i--) {
		len = strlen(argv[i]) + 1;
		if (len > MEMSZ / 16)
			die("program argument too long");
		top -= len;
		memcpy(&m->mem[top], argv[i], len);
		vec[i] = top;
	}
	vec[argc] = 0;
	top &= ~(int64_t)(WORDSZ - 1);
	top -= (argc + 1) * WORDSZ;
	memcpy(&m->mem[top], vec, (argc + 1) * WORDSZ);

	m->regs[R1] = argc;
	m->regs[R2] = top;
	m->regs[SP] = top;
}

static int64_t
checkaddr(const Machine *m, int64_t a, int size)
{
	if (a < DATABASE || a > MEMSZ - size)
		die("pc=%d: invalid address %lld", m->pc, (long long)a);

	return a;
}

static int64_t
load(const Machine *m, int64_t a, int isbyte)
{
	int64_t v;

	if (isbyte)
		return (int8_t)m->mem[checkaddr(m, a, 1)];

	memcpy(&v, &m->mem[checkaddr(m, a, WORDSZ)], WORDSZ);
	return v;
}

static void
store(Machine *m, int64_t a, int64_t v, int isbyte)
{
	if (isbyte)
		m->mem[checkaddr(m, a, 1)] = (uint8_t)v;
	else
		memcpy(&m->mem[checkaddr(m, a, WORDSZ)], &v, WORDSZ);
}

static void
push(Machine *m, int64_t v)
{
	if (m->regs[SP] - WORDSZ < m->stacklim)
		die("pc=%d: stack overflow", m->pc);
	m->regs[SP] -= WORDSZ;
	store(m, m->regs[SP], v, 0);
}

static int64_t
pop(Machine *m)
{
	int64_t v;

	if (m->regs[SP] > MEMSZ - WORDSZ)
		die("pc=%d: stack underflow", m->pc);

	v = load(m, m->regs[SP], 0);
	m->regs[SP] += WORDSZ;
	return v;
}

static int64_t
srcval(const Machine *m, const Instr *in)
{
	return in->isimm ? in->imm : m->regs[in->rs];
}

/* second operand of CMP */
static int64_t
cmpval(const Machine *m, const Instr *in)
{
	return in->isimm ? in->imm : m->regs[in->rt];
}

/* copies the string at a into buf, which holds MAXPATH bytes */
static const char *
getstr(const Machine *m, int64_t a, char *buf)
{
	int i;

	for (i = 0; i < MAXPATH; i++)
		if ((buf[i] = (char)load(m, a + i, 1)) == '\0')
			return buf;

	die("pc=%d: string at %lld too long", m->pc, (long long)a);
}

static int
validfd(const Machine *m, int64_t fd)
{
	return fd >= 0 && fd < MAXFD && m->fd[fd];
}

/* runs host service n; returns 1 if the program exits */
static int
syscall(Machine *m, int64_t n)
{
	char path[MAXPATH], path2[MAXPATH], mode[4];
	int64_t *r;
	int c, i;

	r = m->regs;
	switch (n) {
	case SYS_EXIT:
		m->status = (int)(r[R1] & 255);
		return 1;
	case SYS_GETC:
		if (!validfd(m, r[R1]) || (c = fgetc(m->fd[r[R1]])) == EOF)
			r[R0] = -1;
		else
			r[R0] = c;
		break;
	case SYS_PUTC:
		if (!validfd(m, r[R1]) ||
		    fputc((int)(r[R2] & 255), m->fd[r[R1]]) == EOF)
			r[R0] = -1;
		else
			r[R0] = r[R2] & 255;
		break;
	case SYS_OPEN:
		getstr(m, r[R1], path);
		getstr(m, r[R2], path2);
		if (strcmp(path2, "r") && strcmp(path2, "w") &&
		    strcmp(path2, "a") && strcmp(path2, "u")) {
			r[R0] = -1;
			break;
		}
		strcpy(mode, strcmp(path2, "u") ? path2 : "r+");
		for (i = 3; i < MAXFD && m->fd[i]; i++)
			;
		if (i == MAXFD || !(m->fd[i] = fopen(path, mode)))
			r[R0] = -1;
		else
			r[R0] = i;
		break;
	case SYS_CLOSE:
		if (!validfd(m, r[R1])) {
			r[R0] = -1;
		} else if (r[R1] < 3) {
			r[R0] = fflush(m->fd[r[R1]]) ? -1 : 0;
		} else {
			r[R0] = fclose(m->fd[r[R1]]) ? -1 : 0;
			m->fd[r[R1]] = NULL;
		}
		break;
	case SYS_UNLINK:
		r[R0] = remove(getstr(m, r[R1], path)) ? -1 : 0;
		break;
	case SYS_RENAME:
		getstr(m, r[R1], path);
		getstr(m, r[R2], path2);
		r[R0] = rename(path, path2) ? -1 : 0;
		break;
	default:
		die("pc=%d: invalid service %lld", m->pc, (long long)n);
	}

	return 0;
}

static void
execute(const Instr *prog, int nprog, int entry, Machine *m)
{
	Instr in;
	int64_t a, b, r;
	uint64_t s;

	m->pc = entry;
	for (;;) {
		if (m->pc < 0 || m->pc >= nprog)
			die("pc out of range: %d", m->pc);

		in = prog[m->pc];

		if (in.op == OP_HALT)
			break;

		switch (in.op) {
		case OP_PUT:
			m->regs[in.rd] = in.imm;
			break;
		case OP_MOV:
			m->regs[in.rd] = m->regs[in.rs];
			break;
		case OP_LOAD:
		case OP_LOADB:
			a = in.isimm ? in.imm : m->regs[in.rs] + in.imm;
			m->regs[in.rd] = load(m, a, in.op == OP_LOADB);
			break;
		case OP_STORE:
		case OP_STOREB:
			a = in.isimm ? in.imm : m->regs[in.rs] + in.imm;
			store(m, a, m->regs[in.rd], in.op == OP_STOREB);
			break;
		case OP_PUSH:
			push(m, m->regs[in.rd]);
			break;
		case OP_POP:
			m->regs[in.rd] = pop(m);
			break;
		case OP_INC:
			m->regs[in.rd] = (int64_t)((uint64_t)m->regs[in.rd] + 1);
			break;
		case OP_DEC:
			m->regs[in.rd] = (int64_t)((uint64_t)m->regs[in.rd] - 1);
			break;
		case OP_NEG:
			m->regs[in.rd] = (int64_t)(0 - (uint64_t)m->regs[in.rs]);
			break;
		case OP_COM:
			m->regs[in.rd] = ~m->regs[in.rs];
			break;
		case OP_ADD:
			m->regs[in.rd] = (int64_t)((uint64_t)m->regs[in.rd] +
			    (uint64_t)srcval(m, &in));
			break;
		case OP_SUB:
			m->regs[in.rd] = (int64_t)((uint64_t)m->regs[in.rd] -
			    (uint64_t)srcval(m, &in));
			break;
		case OP_MUL:
			m->regs[in.rd] = (int64_t)((uint64_t)m->regs[in.rd] *
			    (uint64_t)srcval(m, &in));
			break;
		case OP_DIV:
		case OP_REM:
			a = srcval(m, &in);
			if (a == 0)
				die("pc=%d: division by zero", m->pc);
			if (a == -1 && m->regs[in.rd] == INT64_MIN)
				die("pc=%d: division overflow", m->pc);
			if (in.op == OP_DIV)
				m->regs[in.rd] /= a;
			else
				m->regs[in.rd] %= a;
			break;
		case OP_AND:
			m->regs[in.rd] &= srcval(m, &in);
			break;
		case OP_OR:
			m->regs[in.rd] |= srcval(m, &in);
			break;
		case OP_XOR:
			m->regs[in.rd] ^= srcval(m, &in);
			break;
		case OP_SHL:
			s = (uint64_t)srcval(m, &in) & 63;
			m->regs[in.rd] = (int64_t)((uint64_t)m->regs[in.rd] << s);
			break;
		case OP_SHR:
			s = (uint64_t)srcval(m, &in) & 63;
			a = m->regs[in.rd];
			m->regs[in.rd] = a < 0 ? ~(~a >> s) : a >> s;
			break;
		case OP_SEQ:
			m->regs[in.rd] = m->zf;
			break;
		case OP_SNE:
			m->regs[in.rd] = !m->zf;
			break;
		case OP_SLT:
			m->regs[in.rd] = m->sf != m->of;
			break;
		case OP_SLE:
			m->regs[in.rd] = m->zf || m->sf != m->of;
			break;
		case OP_SGT:
			m->regs[in.rd] = !m->zf && m->sf == m->of;
			break;
		case OP_SGE:
			m->regs[in.rd] = m->sf == m->of;
			break;
		case OP_CMP:
			a = cmpval(m, &in);
			b = m->regs[in.rs];
			r = (int64_t)((uint64_t)b - (uint64_t)a);
			m->zf = (r == 0);
			m->sf = (r < 0);
			m->of = (((b ^ a) & (b ^ r)) < 0);
			break;
		case OP_JMP:
			m->pc = (int)in.imm;
			continue;
		case OP_JMPR:
			m->pc = (int)m->regs[in.rs];
			continue;
		case OP_CALL:
			push(m, m->pc + 1);
			m->pc = (int)in.imm;
			continue;
		case OP_CALLR:
			push(m, m->pc + 1);
			m->pc = (int)m->regs[in.rs];
			continue;
		case OP_RET:
			m->pc = (int)pop(m);
			continue;
		case OP_JE:
			if (m->zf)
				goto jump;
			break;
		case OP_JL:
			if (m->sf != m->of)
				goto jump;
			break;
		case OP_JG:
			if (!m->zf && m->sf == m->of)
				goto jump;
			break;
		case OP_JGE:
			if (m->sf == m->of)
				goto jump;
			break;
		case OP_JLE:
			if (m->zf || m->sf != m->of)
				goto jump;
			break;
		case OP_JNE:
			if (!m->zf)
				goto jump;
			break;
		case OP_XCHG:
			a = m->regs[in.rd];
			m->regs[in.rd] = m->regs[in.rs];
			m->regs[in.rs] = a;
			break;
		case OP_SYS:
			if (syscall(m, in.imm))
				return;
			break;
		default:
			die("pc=%d: invalid opcode %d", m->pc, in.op);
		}
		m->pc++;
		continue;
jump:
		m->pc = (int)in.imm;
	}
}

/*
 * Write the n bytes at p, which start at address addr, as hexdump -C
 * does: per line, the address, 16 bytes in hex, and the same bytes as
 * characters, with '.' for a byte that does not print. A "*" line
 * stands for lines equal to the one before it. The last line holds
 * the address just past the bytes.
 */
static void
hexdump(FILE *fp, const uint8_t *p, int addr, int n)
{
	int i, len, off, starred;

	starred = 0;
	for (off = 0; off < n; off += 16) {
		len = n - off < 16 ? n - off : 16;
		if (off > 0 && len == 16 && memcmp(p + off, p + off - 16, 16) == 0) {
			if (!starred)
				fprintf(fp, "*\n");
			starred = 1;
			continue;
		}
		starred = 0;
		fprintf(fp, "%08x ", addr + off);
		for (i = 0; i < 16; i++) {
			if (i == 8)
				fputc(' ', fp);
			if (i < len)
				fprintf(fp, " %02x", p[off + i]);
			else
				fprintf(fp, "   ");
		}
		fprintf(fp, "  |");
		for (i = 0; i < len; i++)
			fputc(p[off + i] >= ' ' && p[off + i] < 127 ? p[off + i] : '.',
			    fp);
		fprintf(fp, "|\n");
	}
	fprintf(fp, "%08x\n", addr + n);
}

static void
dumpstate(const char *outpath, const Machine *m)
{
	FILE *fp;
	int64_t v;
	int i;

	if (!(fp = fopen(outpath, "w")))
		die("%s:", outpath);

	for (i = 0; i < NREG; i++)
		fprintf(fp, "%s = %lld\n", regnames[i], (long long)m->regs[i]);
	fprintf(fp, "zf = %d\n", m->zf);
	fprintf(fp, "sf = %d\n", m->sf);
	fprintf(fp, "of = %d\n", m->of);

	for (i = 0; i + WORDSZ <= m->ndata; i += WORDSZ) {
		memcpy(&v, &m->mem[DATABASE + i], WORDSZ);
		fprintf(fp, "mem[%d] = %lld\n", DATABASE + i, (long long)v);
	}

	fprintf(fp, "data:\n");
	hexdump(fp, &m->mem[DATABASE], DATABASE, m->ndata);

	fclose(fp);
}

static void
usage(void)
{
	die("usage: %s [-o state] [-i] program [arg ...]", argv0);
}

int
main(int argc, char *argv[])
{
	char *inpath, *outpath;
	Machine *m;
	int nprog, entry;

	inpath = NULL;
	outpath = NULL;

	ARGBEGIN {
	case 'i':
		inpath = EARGF(usage());
		break;
	case 'o':
		outpath = EARGF(usage());
		break;
	default:
		usage();
	} ARGEND

	if (!inpath) {
		if (argc < 1)
			usage();
		inpath = argv[0];
		argc--;
		argv++;
	}

	m = &machine;
	m->fd[0] = stdin;
	m->fd[1] = stdout;
	m->fd[2] = stderr;
	nprog = loadprog(inpath, prog, MAXINSTR, &entry, m);
	/* argv[-1] is the program path, whichever way it was given */
	argv[-1] = inpath;
	setargs(m, argc + 1, argv - 1);
	execute(prog, nprog, entry, m);
	fflush(stdout);
	if (outpath)
		dumpstate(outpath, m);

	return m->status;
}
