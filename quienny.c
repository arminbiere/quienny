#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned variables;

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

struct sets {
  struct set *first, *last;
};

#define ENQUEUE(ANCHOR, ELEMENT)                                               \
  do {                                                                         \
    (ELEMENT)->next = 0;                                                       \
    if ((ANCHOR)->last)                                                        \
      (ANCHOR)->last->next = (ELEMENT);                                        \
    else                                                                       \
      (ANCHOR)->first = (ELEMENT);                                             \
    (ELEMENT)->prev = (ANCHOR)->last;                                          \
    (ANCHOR)->last = ELEMENT;                                                  \
  } while (0)

#define DEQUEUE(ANCHOR, ELEMENT)                                               \
  do {                                                                         \
    if ((ELEMENT)->prev)                                                       \
      (ELEMENT)->prev->next = (ELEMENT)->next;                                 \
    else                                                                       \
      (ANCHOR)->first = (ELEMENT)->next;                                       \
    if ((ELEMENT)->next)                                                       \
      (ELEMENT)->next->prev = (ELEMENT)->prev;                                 \
    else                                                                       \
      (ANCHOR)->last = (ELEMENT)->prev;                                        \
  } while (0)

#define NEW(PTR)                                                               \
  do {                                                                         \
    (PTR) = malloc(sizeof *(PTR));                                             \
  } while (0)

#define DELETE(PTR)                                                            \
  do {                                                                         \
    free(*PTR);                                                                \
  } while (0)

#define CLEAR(PTR)                                                             \
  do {                                                                         \
    memset((PTR), 0, sizeof *(PTR));                                           \
  } while (0)

static void print_monom (struct monom * monom) {
}

static void print_polynom (struct polynom * polynom) {
}

int main(int argc, char **argv) {
  struct polynom p = {0, 0};
  struct monom m = {.mask = 7, .values = 4};
  variables = 3;
  ENQUEUE(&p, &m);
  print_polynom (p);
  DEQUEUE(&p, &m);
  return 0;
}
