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

// A monomiale consists of a bit-vector of 'values' masked by 'mask'.  Only
// value bits which have a corresponding mask bit set are valid.  The others
// are invalid, thus "don't cares" ('-').

struct monomial {
  size_t ones = 0;
  vector<bool> mask;
  vector<bool> values;
  void print();
  bool first(); // Parse first monomiale.
  bool read();  // Parse following monomiales.
  bool operator==(const monomial &) const;
  bool operator<(const monomial &) const;
  bool match(const monomial &, size_t &where) const;
};

void monomial::print() {
  for (auto i : variables)
    fputc(mask[i] ? '0' + values[i] : '-', output);
  fputc('\n', output);
}

// Parsing the first monomiale in the 'input' file also sets the range of
// variables.  The function returns 'false' if end-of-file is found instead.

bool monomial::first() {
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
    ones += value;
    ch = read_char();
    variables++;
  }
  return true;
}

// Parsing the remaining monomiale in the 'input' file after parsing the first
// one. These monomiales need to have the size of the 'first' parsed monomiale.
// The function returns 'false' if the end-of-file is reached.

bool monomial::read() {
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
    ones += value;
    ch = read_char();
  }
  if (ch != '\n')
    parse_error("expected new-line");
  return true;
}

bool monomial::operator==(const monomial &other) const {
  if (mask != other.mask)
    return false;
  for (auto i : variables)
    if (mask[i] && values[i] != other.values[i])
      return false;
  return true;
}

// The less-than operator '<' is used to sort and normalize the polynomial.
// Sorting is first done with respect to the number of (valid) '1' bits, and
// then lexicographically with respect to '0' < '-' < '1'.

bool monomial::operator<(const monomial &other) const {
  for (auto i : variables) {
    if (ones < other.ones)
      return true;
    if (ones > other.ones)
      return false;
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

// Check whether the 'other' monomial differs in exactly one valid bit. If this
// is the case the result parameter 'where' is set to that bit position.

bool monomial::match(const monomial &other, size_t &where) const {
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

// A polynomial is in essence a vector of monomialials.

struct polynom {

  vector<monomial> monomials;

  bool contains(const monomial &) const;
  void add(const monomial &m);
  void parse();
  void sort();
  void normalize();
  void print() const;

  bool empty() const { return monomials.empty(); }
  size_t size() const { return monomials.size(); }
  void clear() { monomials.clear(); }
  const monomial &operator[](size_t i) const { return monomials[i]; }
};

bool polynom::contains(const monomial &needle) const {
  for (auto m : monomials)
    if (m == needle)
      return true;
  return false;
}

void polynom::add(const monomial &m) {
  if (!contains(m))
    monomials.push_back(m);
}

void polynom::parse() {
  monomial m;
  if (m.first()) {
    add(m);
    while (m.read())
      add(m);
  }
}

void polynom::sort() { ::sort(monomials.begin(), monomials.end()); }

void polynom::print() const {
  for (auto m : monomials)
    m.print();
}

//------------------------------------------------------------------------//

// Generate the polynomial of primes (destroys 'p') with Quine-McCluskey.

void generate(polynom &p, polynom &primes) {
  vector<bool> prime;
  polynom tmp;
  while (!p.empty()) {
    const size_t size = p.size();
    prime.clear();
    for (size_t i = 0; i != size; i++)
      prime.push_back(true);
    tmp.clear();
    for (size_t i = 0; i + 1 != size; i++) {
      const auto &mi = p[i];
      for (size_t j = i + 1; j != size; j++) {
        const auto &mj = p[j];
        size_t k;
        if (!mi.match(mj, k))
          continue;
        prime[i] = prime[j] = false;
        monomial m = mi;
        m.mask[k] = false;
        if (m.values[k])
          m.ones--;
        tmp.add(m);
      }
    }
    for (size_t i = 0; i != size; i++)
      if (prime[i])
        primes.add(p[i]);
    p = tmp;
  }
}

//------------------------------------------------------------------------//

static void init(int argc, char **argv) {
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
}

static void reset(int argc) {
  if (argc > 1)
    fclose(input);
  if (argc > 2)
    fclose(output);
}

/*------------------------------------------------------------------------*/

int main(int argc, char **argv) {
  init(argc, argv);
  polynom p;
  p.parse();
  polynom primes;
  generate(p, primes);
  primes.sort();
  primes.print();
  reset(argc);
  return 0;
}
