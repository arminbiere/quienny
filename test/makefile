test:
	./run.sh
all: all.c
	gcc -o $@ $<
noall: noall.c
	gcc -o $@ $<
clean:
	rm -f *.out *.log *.err all noall
.PHONY: test clean
