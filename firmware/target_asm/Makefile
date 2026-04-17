# Build file for target assembly fragments

TARGETS=test_inc.th test_str.th test_null.th

all:	$(TARGETS)

%.th:	%.s
	64tass -b -o $@.bin -L $@.dump $<
	xxd -i -n $@ $@.bin | sed -e '/unsigned int/d' -e 's/unsigned char/const unsigned char/g' -e 's/ =/ PROGMEM =/g' -e 's/_th/_bin/g'> $@ || rm $@

.PHONY: clean
clean:
	-rm $(TARGETS)
	-rm *.dump
	-rm *.bin
