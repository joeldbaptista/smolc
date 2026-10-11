include ../config.mk

SRC = common.c
OBJ = $(SRC:.c=.o)

all: smol sas

.c.o:
	$(CC) -c $(CFLAGS) $<

smol: svm.o $(OBJ)
	$(CC) -o $@ svm.o $(OBJ) $(LDFLAGS)

sas: sas.o $(OBJ)
	$(CC) -o $@ sas.o $(OBJ) $(LDFLAGS)

svm.o: svm.c smol.h common.h arg.h
sas.o: sas.c smol.h common.h arg.h
common.o: common.c common.h smol.h

clean:
	rm -f smol sas *.o

.PHONY: all clean
