#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>

//------------------------------------------------------------------------//

using namespace std;

//------------------------------------------------------------------------//

// Global input and output variables.

static const char *path;
static size_t lineno = 1;
static FILE *input, *output;

static int read_char() {
  int res = getc(input);
  if (res == '\n')
    lineno++;
  return res;
}

//------------------------------------------------------------------------//

// Represent the range of variables '[0, ..., n-1]'.

// The purpose of this struct is to enable the use of the idiom
//
//   for (auto i : variables)
//
// It not only simplifies the code but more important avoids that the
// compiler needs to rely on alias analysis to make sure that 'variables' is
// not written inside the loop and has to be read during every iteration.

// In more recent C++ versions 'itoa' could be used instead.

struct range {
  size_t size = 0;
  void operator++(int) { size++; }
  struct iterator {
    size_t current;
    void operator++() { current++; }
    size_t operator*() const { return current; }
    bool operator!=(const iterator &other) const {
      return current != other.current;
    }
    iterator(size_t i) : current(i) {}
  };
  iterator begin() { return iterator(0); }
  iterator end() { return iterator(size); }
};

static range variables;

//------------------------------------------------------------------------//

static void die(const char *msg) {
  fprintf(stderr, "quienny: error: %s\n", msg);
  exit(1);
}

static void parse_error(const char *msg) {
  fprintf(stderr, "quienny: parse error: at line %zu in '%s': %s\n", lineno,
          path, msg);
  exit(1);
}

//------------------------------------------------------------------------//

// A monome consists of a bit-vector of 'values' masked by 'mask'.  Only
// value bits which have a corresponding mask bit set are valid.  The others
// are invalid, thus "don't cares" ('-').

struct monom {
  vector<bool> mask;
  vector<bool> values;
  void print();
  bool first(); // Parse first monome.
  bool read();  // Parse following monomes.
  bool operator==(const monom &) const;
  bool operator<(const monom &) const;
  bool match(const monom &, size_t &where) const;
};

void monom::print() {
  for (auto i : variables)
    fputc(mask[i] ? '0' + values[i] : '-', output);
  fputc('\n', output);
}

// Parsing the first monome in the 'input' file also sets the range of
// variables.  The function returns 'false' if end-of-file is found instead.

bool monom::first() {
  int ch = read_char();
  if (ch == EOF)
    return false;
  while (ch != '\n') {
    bool value = false;
    if (ch == '1')
      value = true;
    else if (ch != '0')
      parse_error("expected '0' or '1' or new-line");
    values.push_back(value);
    mask.push_back(true);
    ch = read_char();
    variables++;
  }
  return true;
}

// Parsing the remaining monome in the 'input' file after parsing the first
// one. These monomes need to have the size of the 'first' parsed monome.
// The function returns 'false' if the end-of-file is reached.

bool monom::read() {
  int ch = read_char();
  if (ch == EOF)
    return false;
  for (auto i : variables) {
    bool value = false;
    if (ch == '1')
      value = true;
    else if (ch != '0')
      parse_error("expected '0' or '1'");
    values[i] = value;
    mask[i] = true;
    ch = read_char();
  }
  if (ch != '\n')
    parse_error("expected new-line");
  return true;
}

bool monom::operator==(const monom &other) const {
  if (mask != other.mask)
    return false;
  for (auto i : variables)
    if (mask[i] && values[i] != other.values[i])
      return false;
  return true;
}

bool monom::operator<(const monom &other) const {
  for (auto i : variables) {
    const bool this_mask = mask[i];
    const bool this_value = values[i];
    const bool other_mask = other.mask[i];
    const bool other_value = other.values[i];
    if (this_mask < other_mask)
      return other_value;
    if (this_mask > other_mask)
      return !this_value;
    if (this_value < other_value)
      return true;
    if (this_value > other_value)
      return false;
  }
  return false;
}

// Check whether the 'other' monom differs in exactly one valid bit. If this
// is the case the result parameter 'where' is set to that bit position.

bool monom::match(const monom &other, size_t &where) const {
  if (mask != other.mask)
    return false;
  bool matched = false;
  for (auto i : variables) {
    if (!mask[i])
      continue;
    if (values[i] == other.values[i])
      continue;
    if (matched)
      return false;
    matched = true;
    where = i;
  }
  return matched;
}

//------------------------------------------------------------------------//

struct polynom {
  vector<monom> monoms;
  bool contains(const monom &) const;
  void insert(const monom &m);
  void parse();
  void sort();
  void print() const;

  bool empty () const { return monoms.empty (); }
  size_t size () const { return monoms.size (); }
  void clear () { monoms.clear (); }
  const monom & operator [] (size_t i) const { return monoms[i]; }
};

bool polynom::contains(const monom &needle) const {
  for (auto m : monoms)
    if (m == needle)
      return true;
  return false;
}

void polynom::insert(const monom &m) {
  if (!contains(m))
    monoms.push_back(m);
}

void polynom::parse() {
  monom m;
  if (m.first()) {
    insert(m);
    while (m.read())
      insert(m);
  }
}

void polynom::sort() { ::sort(monoms.begin(), monoms.end()); }

void polynom::print() const {
  for (auto m : monoms)
    m.print();
}

//------------------------------------------------------------------------//

int main(int argc, char **argv) {
  if (argc > 3)
    die("more than two arguments");
  if (argc == 1)
    input = stdin, output = stdout, path = "<stdin>";
  else if (!(input = fopen(path = argv[1], "r")))
    die("can not read input file given as first argument");
  else if (argc == 2)
    output = stdout;
  else if (!(output = fopen(argv[2], "w")))
    die("can not write output file given as second argument");
  polynom p;
  p.parse();
  polynom q, primes;
  vector<bool> prime;
  while (!p.empty()) {
    const size_t size = p.size();
    prime.clear();
    for (size_t i = 0; i != size; i++)
      prime.push_back(true);
    q.clear();
    for (size_t i = 0; i + 1 != size; i++) {
      const auto &mi = p[i];
      for (size_t j = i + 1; j != size; j++) {
        const auto &mj = p[j];
        size_t k;
        if (!mi.match(mj, k))
          continue;
        prime[i] = prime[j] = false;
        monom m = mi;
        m.mask[k] = false;
        q.insert(m);
      }
    }
    for (size_t i = 0; i != size; i++)
      if (prime[i])
        primes.insert(p[i]);
    p = q;
  }
  primes.sort();
  primes.print();
  if (argc > 1)
    fclose(input);
  if (argc > 2)
    fclose(output);
  return 0;
}
