test:
	./run.sh
all: all.c
	gcc -o $@ $<
abo: abo.c
	gcc -o $@ $<
abz: abz.c
	gcc -o $@ $<
clean:
	rm -f *.out *.log *.err abo abz all
.PHONY: test clean
