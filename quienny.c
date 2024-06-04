#include <stdio.h>

struct monom {
  unsigned mask;
  unsigned values;
  struct monom *prev, *next;
};

struct polynom {
  struct monom *first, *last;
};

struct block {
  unsigned count;
  struct polynom polynom;
  struct block *prev, *next;
};

struct set {
  unsigned mask;
  struct block *first, *last;
};

static void enqueue_monom (struct polynom * p, struct monom * m) {
}

int main (int argc, char ** argv) {
  struct polynom p = { 0, 0 };
  struct monom m = {.mask = 7, .values = 4 };
  enqueue_monom (&p, &m);
  return 0;
}
