#include <cstdio>
#include <cstdlib>
#include <vector>

static FILE *input, *output;
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
  bool equal(const monom &m) const {
    if (mask != m.mask)
      return false;
    for (unsigned i = 0, bit = 1; i != variables; i++, bit <<= 1)
      if ((bit & mask) && (values & bit) != (m.values & bit))
        return false;
    return true;
  }
  bool match (const monom & m, unsigned & where) {
    if (mask != m.mask)
      return false;
    bool matched = false;
    for (unsigned i = 0, bit = 1; i != variables; i++, bit <<= 1) {
      if (!(bit & mask))
        continue;
      if ((values & bit) == (m.values & bit))
        continue;
      if (matched)
        return false;
      matched = true;
      where = bit;
    }
    return matched;
  }
};

typedef vector<monom> polynom;

static bool contains(const polynom &p, const monom &needle) {
  for (auto m : p)
    if (m.equal (needle))
      return true;
  return false;
}

static void insert(polynom &p, const monom &m) {
  if (!contains(p, m))
    p.push_back (m);
}

static void parse(polynom &p) {
  monom m;
  if (m.first()) {
    p.push_back(m);
    while (m.read())
      insert(p, m);
  }
}

static void print(const polynom &p) {
  for (auto m : p)
    m.print();
}

int main(int argc, char **argv) {
  if (argc > 3)
    die("too many arguments");
  if (argc == 1)
    input = stdin, output = stdout;
  else if (!(input = fopen(argv[1], "r")))
    die("can not read input file given as first argument");
  else if (argc == 2)
    output = stdout;
  else if (!(output = fopen(argv[2], "w")))
    die("can not write output file given as second argument");
  polynom p;
  parse(p);
  vector<monom> primes;
  while (!p.empty()) {
    polynom q;
    for (size_t i = 0; i != p.size(); i++) {
      auto &m1 = p[i];
      bool prime = true;
      for (size_t j = 0; j != p.size(); j++) {
        if (i == j)
          continue;
        auto &m2 = p[j];
        unsigned bit;
        if (!m1.match (m2, bit))
          continue;
        monom m;
        m.mask = m1.mask & ~bit;
        m.values = m1.values;
        prime = false;
        insert (q, m);
      }
      if (prime)
        insert (primes, m1);
    }
    p = q;
  }
  print(primes);
  if (argc > 1)
    fclose(input);
  if (argc > 2)
    fclose(output);
  return 0;
}
