/* smol - shared instruction set definitions for the assembler and VM */
#ifndef SMOL_H
#define SMOL_H

#include <stdint.h>

#define NREG       16          /* r0..r15 */
#define MAXINSTR   262144      /* program-size cap, in instructions */
#define WORDSZ     8           /* bytes per word (64-bit) */
#define MEMSZ      (1 << 24)   /* bytes of data memory: 16 MiB */
#define DATABASE   8           /* address of the first static data byte */
#define SMOL_MAGIC "SMOL"      /* 4-byte on-disk file magic, no NUL */
#define ENDSYM     "__end"     /* data label the assembler puts after all data */

/* n rounded up to whole words */
#define WORDUP(n)  (((n) + WORDSZ - 1) & ~(WORDSZ - 1))

/*
 * Memory is one byte-addressed array of MEMSZ bytes. Static data starts
 * at DATABASE, so address 0 is never a valid object and works as a null
 * pointer. The heap follows the static data, and the stack grows down
 * from the top. Code lives in a separate instruction array, so a code
 * address is an instruction index.
 */

typedef enum {
	R0,    /* return value */
	R1,
	R2,
	R3,
	R4,
	R5,    /* first argument */
	R6,    /* base pointer */
	R7,    /* stack pointer */
	R8,
	R9,
	R10,
	R11,
	R12,
	R13,
	R14,
	R15,
} Reg;

#define SP R7    /* register push/pop/call/return use as stack pointer */

/*
 * Operand use, per opcode, with the assembler's mnemonic. "src" is imm
 * when isimm is 1, else rt for CMP, else rs.
 *
 *   PUT    rd = imm                   mov rd, 5    or  mov rd, label
 *   MOV    rd = rs                    mov rd, rs
 *   LOAD   rd = word at addr          mov rd, [rs + 8]
 *   STORE  word at addr = rd          mov [rs + 8], rd
 *   LOADB  rd = byte at addr, sign extended    movsx rd, byte [rs + 8]
 *   STOREB byte at addr = low byte of rd       mov byte [rs + 8], rd
 *          addr is rs + imm, or imm alone when isimm is 1, as in [label]
 *   PUSH   push rd                    POP    rd = pop
 *   INC    ++rd                       DEC    --rd
 *   NEG    rd = -rs                   COM    rd = ~rs     (neg, not)
 *   ADD .. SHR   rd op= src   (add sub mul div rem and or xor shl sar)
 *                SHR is an arithmetic shift
 *   CMP    flags from rs - src        cmp rs, src
 *   SEQ .. SGE   rd = 0 or 1, from the flags  (sete setne setl ...)
 *   JMP    goto imm                   JMPR   goto rs      (jmp)
 *   CALL   call imm                   CALLR  call rs      (call)
 *   RET    return                     JE .. JNE  conditional goto imm
 *   XCHG   swap rd and rs             SYS    host service imm
 *   HALT   stop                       (hlt)
 */
typedef enum {
	OP_PUT,
	OP_MOV,
	OP_LOAD,
	OP_STORE,
	OP_LOADB,
	OP_STOREB,
	OP_PUSH,
	OP_POP,
	OP_INC,
	OP_DEC,
	OP_NEG,
	OP_COM,
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_REM,
	OP_AND,
	OP_OR,
	OP_XOR,
	OP_SHL,
	OP_SHR,
	OP_SEQ,
	OP_SNE,
	OP_SLT,
	OP_SLE,
	OP_SGT,
	OP_SGE,
	OP_CMP,
	OP_JMP,
	OP_JMPR,
	OP_CALL,
	OP_CALLR,
	OP_RET,
	OP_JE,
	OP_JL,
	OP_JG,
	OP_JGE,
	OP_JLE,
	OP_JNE,
	OP_XCHG,
	OP_SYS,
	OP_HALT,    /* must stay last: the VM rejects any opcode above it */
} Op;

/*
 * Host services for OP_SYS. Arguments are in r1, r2; the result is in
 * r0. This list is complete.
 */
typedef enum {
	SYS_EXIT,     /* exit with status r1 */
	SYS_GETC,     /* r0 = next byte from fd r1, or -1 */
	SYS_PUTC,     /* write byte r2 to fd r1; r0 = r2, or -1 */
	SYS_OPEN,     /* open path r1 with mode string r2; r0 = fd, or -1 */
	SYS_CLOSE,    /* close fd r1; r0 = 0, or -1 */
	SYS_UNLINK,   /* remove path r1; r0 = 0, or -1 */
	SYS_RENAME,   /* rename path r1 to path r2; r0 = 0, or -1 */
	NSYS,
} Sys;

typedef struct instr Instr;
struct instr {
	Op      op;
	int     rd;     /* destination, or the register stored or pushed */
	int     rs;     /* first source register, or the address base */
	int     rt;     /* second source register */
	int64_t imm;    /* immediate, offset, or resolved code address */
	int     isimm;  /* 1 if the source operand is imm */
};

/*
 * On-disk format: one Header, then Header.ninstr EncInstr records, then
 * Header.ndata bytes of static data, loaded at DATABASE. Every field is
 * naturally aligned, so there is no implicit padding and each struct
 * can be read/written with a single fread/fwrite, as long as the
 * assembler and VM are built with the same compiler/ABI.
 */
typedef struct {
	char    magic[4];  /* SMOL_MAGIC */
	int32_t ninstr;    /* number of EncInstr records following */
	int32_t entry;     /* instruction index where execution begins */
	int32_t ndata;     /* number of static data bytes following */
} Header;

typedef struct {
	uint8_t op;        /* Op enum value */
	uint8_t rd;        /* register operands, 0 if unused */
	uint8_t rs;
	uint8_t rt;
	uint8_t isimm;     /* 1 if the source operand is imm */
	uint8_t pad[3];    /* reserved, always zero */
	int64_t imm;       /* immediate, offset, or resolved code address */
} EncInstr;

/* in common.c */
extern const char *regnames[NREG];  /* "r0" .. "r15" */
void encode(const Instr *in, EncInstr *e);
void decode(const EncInstr *e, Instr *in);

#endif /* SMOL_H */
