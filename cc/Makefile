# smolc compiler: BYTE Small-C 1.0 retargeted to the smol machine.

include ../config.mk

OBJ = cc1.o cc11.o cc12.o cc13.o cc14.o cc21.o cc22.o cc31.o cc32.o \
      cc33.o cc41.o cc42.o

all: scc

scc: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

$(OBJ): cc.h ccdef.h notice.h

clean:
	rm -f scc $(OBJ)

.PHONY: all clean
