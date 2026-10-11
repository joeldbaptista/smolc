/* common - definitions shared by smol and sas */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "smol.h"

char *argv0;    /* the program's name, for arg.h and messages */

const char *regnames[NREG] = {
	"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
	"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
};

void
die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}

	exit(1);
}

/* converts in to its on-disk form e */
void
encode(const Instr *in, EncInstr *e)
{
	memset(e, 0, sizeof(*e));
	e->op = (uint8_t)in->op;
	e->rd = (uint8_t)in->rd;
	e->rs = (uint8_t)in->rs;
	e->rt = (uint8_t)in->rt;
	e->isimm = (uint8_t)in->isimm;
	e->imm = in->imm;
}

/* converts the on-disk e to in; it does not check e */
void
decode(const EncInstr *e, Instr *in)
{
	in->op = (Op)e->op;
	in->rd = e->rd;
	in->rs = e->rs;
	in->rt = e->rt;
	in->isimm = e->isimm;
	in->imm = e->imm;
}
