# smolc: the Small-C compiler, the smol assembler and VM, and a runtime.

include config.mk

SCC   = cc/scc
SAS   = smol/sas
SMOL  = smol/smol

# crt0.sas must come first: it holds main, the program entry.
LIB = lib/crt0.sas lib/sys.sas lib/stdio.sas lib/string.sas \
      lib/ctype.sas lib/stdlib.sas

EXAMPLES = $(patsubst %.c,%.smol,$(wildcard examples/*.c))
TESTS    = $(patsubst %.c,%.smol,$(wildcard tests/*.c))

all: $(SCC) $(SAS) $(SMOL) $(LIB) $(EXAMPLES)

$(SCC): cc/*.c cc/*.h config.mk
	$(MAKE) -C cc

$(SAS) $(SMOL): smol/*.c smol/*.h config.mk
	$(MAKE) -C smol

# scc writes its banner and any errors to stderr; show them on failure
%.sas: %.c $(SCC) lib/stdio.h
	$(SCC) -o -Ilib $< > $@ 2> $@.err || \
		{ cat $@.err; rm -f $@ $@.err; exit 1; }
	@rm -f $@.err

%.smol: %.sas $(LIB) $(SAS)
	$(SAS) -o $@ lib/crt0.sas $< $(filter-out lib/crt0.sas,$(LIB))

run: all
	@for b in $(EXAMPLES); do \
		echo "== $$b"; (cd examples && ../$(SMOL) $$(basename $$b)); \
	done

test: all $(TESTS)
	@./tests/run.sh

clean:
	$(MAKE) -C cc clean
	$(MAKE) -C smol clean
	rm -f $(filter-out lib/crt0.sas,$(LIB)) examples/*.sas examples/*.smol \
		examples/TEST.TXT tests/*.sas tests/*.smol tests/asm/*.smol

.PHONY: all run test clean
.PRECIOUS: %.sas
