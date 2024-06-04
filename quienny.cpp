#include <cstdio>
#include <cstdlib>
#include <vector>

static FILE *input, * output;
static unsigned variables;

using namespace std;

static void die(const char *msg) {
  fprintf(stderr, "quienny: error: %s\n", msg);
  exit(1);
}

struct monom {
  unsigned mask;
  unsigned values;
  void print() {
    for (unsigned i = 0, bit = 1; i != variables; i++, bit <<= 1)
      fputc(mask & bit ? '0' + !!(values & bit) : '-', output);
    fputc('\n', output);
  }
  bool first() {
    int ch = getc(input);
    if (ch == EOF)
      return false;
    unsigned bits = 0, bit = 1;
    for (; ch != '\n'; bit <<= 1, ch = getc(input))
      if (variables++ == 32)
        die("minterm too large");
      else if (ch == '1')
        bits |= bit;
      else if (ch != '0')
        die("expected '0' or '1' or new-line");
    mask = bit - 1;
    values = bits;
    return true;
  }
  bool read() {
    int ch = getc(input);
    if (ch == EOF)
      return false;
    unsigned bits = 0, bit = 1;
    for (unsigned i = 0; i != variables; i++, bit <<= 1, ch = getc(input))
      if (ch == '1')
        bits |= bit;
      else if (ch != '0')
        die("expected '0' or '1'");
    if (ch != '\n')
      die("expected new-line");
    mask = bit - 1;
    values = bits;
    return true;
  }
};

struct block {
  unsigned count;
  unsigned size;
  size_t offset;
};

struct set {
  unsigned mask;
  vector<block> blocks;
};

struct sets {
  struct set *first, *last;
};

int main(int argc, char **argv) {
  if (argc > 3)
    die ("too many arguments");
  if (argc == 1)
    input = stdin, output = stdout;
  else if (!(input = fopen (argv[1], "r")))
    die ("can not read input file given as first argument");
  else if (argc == 2)
    output = stdout;
  else if (!(output = fopen (argv[2], "w")))
    die ("can not write output file given as second argument");
  monom m;
  vector<monom> p;
  if (m.first()) {
    p.push_back(m);
    while (m.read())
      p.push_back(m);
    for (auto m : p)
      m.print();
  }
  if (argc > 1)
    fclose (input);
  if (argc > 2)
    fclose (output);
  return 0;
}
