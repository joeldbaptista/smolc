# smolc compiler: BYTE Small-C 1.0 retargeted to the smol machine.

include ../config.mk

OBJ = globals.o main.o decl.o stmt.o types.o lex.o preproc.o expr.o \
      unary.o lvalue.o codegen.o emit.o

all: scc

scc: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

$(OBJ): scc.h defs.h notice.h

clean:
	rm -f scc $(OBJ)

.PHONY: all clean
