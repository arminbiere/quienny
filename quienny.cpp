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
//     ...
//
// It not only simplifies code but more important avoids that the compiler
// needs to rely on alias analysis to make sure that 'n' is not written
// inside the loop and has to be read during every iteration as in
//
//   for (size_t i = 0; i != variables; i++)
//     ...
//
// unless you explicitly write
//
//   for (size_t i = 0, end = variables; i != end; i++)
//     ...
//
// which clearly is harder to read.

struct range {
  size_t size = 0;
  void operator++(int) { size++; }
  operator size_t() const { return size; }
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

// We have two types of implementations of bitvectors used to store value
// bits and the mask of valid bits.  The first is of fixed size and the type
// given as argument to '-DFIXED=<type>' during compilation will determine
// how many variables are available (for instance with '-DFIXED=unsigned' we
// get '32 = 8 * 4' variables).  The second implementation uses a generic
// implementation with 'vector<bool>', which is kind of compact, but uses
// much more space per monomial than a plain word-based implementation and
// is accordingly also much slower.

#ifdef FIXED

#include <cstdint> // Allows to use 'uint32_t' for FIXED.

typedef FIXED word;

struct bitvector {
  word bits = 0;
  bool get(const size_t i) const { return bits & ((word)1 << i); }
  void set(const size_t i, bool value) {
    word mask = (word)1 << i;
    word bit = (word)value << i;
    bits = (bits & ~mask) | bit;
  }
  void add(const size_t i, bool value) { set(i, value); }
  bool operator!=(const bitvector &other) const { return bits != other.bits; }
  bool operator==(const bitvector &other) const { return bits == other.bits; }
  bool operator<(const bitvector &other) const { return bits > other.bits; }
  bool operator>(const bitvector &other) const { return bits < other.bits; }
  // LSB comes first in the input and output. Thus we need to reverse order.
};

const size_t max_variables = 8 * sizeof(word);

#else

struct bitvector {
  vector<bool> bits;
  bool get(const size_t i) const { return bits[i]; }
  void set(const size_t i, bool value) { bits[i] = value; };
  void add(const size_t, bool value) { bits.push_back(value); }
  bool operator!=(const bitvector &other) const { return bits != other.bits; }
  bool operator==(const bitvector &other) const { return bits == other.bits; }
  bool operator<(const bitvector &other) const { return bits < other.bits; }
  bool operator>(const bitvector &other) const { return bits > other.bits; }
};

const size_t max_variables = ~(size_t)0;

#endif

//------------------------------------------------------------------------//

// A monomial consists of a bit-vector of 'values' masked by 'mask'.  Only
// value bits which have a corresponding mask bit set are valid.  The others
// are invalid, thus "don't cares" ('-').

struct monomial {
  size_t ones = 0;
  bitvector mask;
  bitvector values;
  void debug() const;
  void print(FILE *) const;
  bool parse_first();
  bool parse_remaining();
  bool operator==(const monomial &) const;
  bool operator!=(const monomial &other) const { return !(*this == other); }
  bool operator<(const monomial &) const;
  bool match(const monomial &, size_t &where) const;
};

void monomial::print(FILE *file) const {
  for (auto i : variables)
    fputc(mask.get(i) ? '0' + values.get(i) : '-', file);
  fputc('\n', file);
}

void monomial::debug() const {
  fprintf(stderr, "%zu:", ones);
  for (auto i : variables)
    fputc(mask.get(i) + '0', stderr);
  fputc(':', stderr);
  for (auto i : variables)
    fputc(values.get(i) + '0', stderr);
}

// Parsing the first monomial in the 'input' file also sets the range of
// variables.  The function returns 'false' if end-of-file is found instead.

bool monomial::parse_first() {
  int ch = read_char();
  if (ch == EOF)
    return false;
  while (ch != '\n') {
    bool value = false;
    if (ch == '1')
      value = true;
    else if (ch != '0')
      parse_error("expected '0' or '1' or new-line");
    if (variables == max_variables)
      parse_error("monomial too large");
    values.add(variables, value);
    mask.add(variables, true);
    ones += value;
    ch = read_char();
    variables++;
  }
  return true;
}

// Parsing the remaining monomial in the 'input' file after parsing the first
// one. These monomials need to have the size of the 'first' parsed monomial.
// The function returns 'false' if the end-of-file is reached.

bool monomial::parse_remaining() {
  int ch = read_char();
  if (ch == EOF)
    return false;
  ones = 0;
  for (auto i : variables) {
    bool value = false;
    if (ch == '1')
      value = true;
    else if (ch != '0')
      parse_error("expected '0' or '1'");
    values.set(i, value);
    mask.set(i, true);
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
    if (mask.get(i) && values.get(i) != other.values.get(i))
      return false;
  return true;
}

// The less-than operator '<' is used to sort and normalize the polynomialial.
// Sorting is first done with respect to the number of (valid) '1' bits
// 'ones', then with respect to the mask , and finally the value bits.

bool monomial::operator<(const monomial &other) const {
  if (ones < other.ones)
    return true;
  if (ones > other.ones)
    return false;
  if (mask < other.mask)
    return true;
  if (mask > other.mask)
    return false;
  if (values < other.values)
    return true;
  if (values > other.values)
    return false;
  return false;
}

// Check whether the 'other' monomial differs in exactly one valid bit. If this
// is the case the result parameter 'where' is set to that bit position.  We
// specialize this for the fixed maximum variable sizes and therefore have
// two versions of the code (one with '#ifddef FIXED ... #else' and one for
// the general case withing '#else ... #endif').

bool monomial::match(const monomial &other, size_t &where) const {

#if 0
  debug(), fputc(' ', stderr), other.debug(), fputc('\n', stderr);
#endif

#ifdef NOPTIMIZE
  assert(ones <= other.ones);
  if (ones + 1 != other.ones)
    return false;

  if (mask != other.mask)
    return false;
#else
  assert(ones + 1 == other.ones);
  assert(mask == other.mask);
#endif

#ifdef FIXED

  // The fixed bit-vector version can use bit-twiddling hacks.

  const word difference = values.bits ^ other.values.bits;
  if (difference & (difference - 1)) // Not power-of two?
    return false;

  assert(difference); // Normalization makes them all different.

  where = 1;
  while ((word)1 << where != difference)
    where++;

  return true;

#else

  // The generic bit-vector version has to use indexed access.

  bool matched = false;

  for (auto i : variables) {
    const bool this_value = values.get(i);
    const bool other_value = other.values.get(i);
    if (this_value == other_value)
      continue;
    if (this_value > other_value)
      return false;
    if (matched)
      return false;
    matched = true;
    where = i;
  }

  assert(matched); // Normalization makes thema ll different.
  return matched;

#endif
}

//------------------------------------------------------------------------//

// A polynomialial is in essence a vector of monomials.

struct polynomial {

  vector<monomial> monomials;

  void parse();
  void normalize();
  void debug() const;
  void print(FILE *) const;

  bool empty() const { return monomials.empty(); }
  size_t size() const { return monomials.size(); }
  void clear() { monomials.clear(); }
  void add(const monomial &m) { monomials.push_back(m); }
  const monomial &operator[](size_t i) const { return monomials[i]; }
};

void polynomial::parse() {
  monomial m;
  if (m.parse_first()) {
    add(m);
    while (m.parse_remaining())
      add(m);
  }
}

// Normalize the polynomialial by sorting and removing duplicates.

void polynomial::normalize() {
  sort(monomials.begin(), monomials.end());
  const auto begin = monomials.begin();
  const auto end = monomials.end();
  if (begin == end)
    return;
  auto j = begin + 1;
  for (auto i = j; i != end; i++)
    if (*i != j[-1])
      *j++ = *i;
  monomials.resize(j - begin);
}

void polynomial::debug() const {
  for (auto m : monomials)
    m.debug(), fputc('\n', stderr);
}

void polynomial::print(FILE *file) const {
  for (auto m : monomials)
    m.print(file);
}

//------------------------------------------------------------------------//

static inline bool consensus(const monomial &mi, const monomial &mj,
                          polynomial &next, polynomial &primes) {
  size_t k;
  if (!mi.match(mj, k))
    return false;
  assert(!mi.values.get(k));
  monomial m = mi;
  m.mask.set(k, false);
  next.add(m);
  return true;
}

// Generate a normalized 'primes' polynomialial of 'p' (destroys 'p') based on
// the Quine-McCluskey algorithm in an non-optimzed and optimized variant.

void generate(polynomial &p, polynomial &primes) {

  vector<bool> prime;
  polynomial next;

  while (!p.empty()) {

    const size_t size = p.size();
    prime.clear();
    for (size_t i = 0; i != size; i++)
      prime.push_back(true);

    next.clear();

#ifdef NOPTIMIZE

    // This is the simple unoptimized version, which compares all pairs
    // (if enabled with './configure -n' or './configure --no-optimize').

    for (size_t i = 0; i + 1 != size; i++)
      for (size_t j = i + 1; j != size; j++)
        if (consensus(p[i], p[j], next, primes))
          prime[i] = prime[j] = false;

#else

    // This is the optimized version (enabled by default).  It uses sorting
    // by normalization to avoid a quadratic number of 'match' comparisons,
    // similarly to one pass in merge-sort.

    // A 'block' is an interval of monomials with the same number 'ones' of
    // valid true bits'.  A 'slice' is an interval of monomials with the
    // same number 'ones' of true bits (thus a sub-interval of a block) and
    // also the same valid bits in 'mask'.

    // Only slices with the same 'mask' have to be compared in consecutive
    // blocks.  Therefore we go over all pairs of subsequent blocks and
    // compare corresponding slices within them.

    // As blocks are ordered by 'ones' and within blocks slices are ordered
    // by 'mask' and we only need to compare monomials with the same number
    // of ones and the same mask, the overall complexity of one outer loop
    // round becomes linear in the size of that outer polynomialial 'p'.

    size_t begin_first_block = 0;
    size_t end_first_block = begin_first_block + 1;
    while (end_first_block != size &&
           p[begin_first_block].ones == p[end_first_block].ones)
      end_first_block++;

    while (end_first_block != size) {

      const size_t begin_second_block = end_first_block;
      size_t end_second_block = begin_second_block + 1;

      while (end_second_block != size &&
             p[begin_second_block].ones == p[end_second_block].ones)
        end_second_block++;

      if (p[begin_first_block].ones + 1 == p[begin_second_block].ones) {

        size_t begin_first_slice = begin_first_block;
        size_t begin_second_slice = begin_second_block;

        while (begin_first_slice != end_first_block) {

          size_t end_first_slice = begin_first_slice + 1;
          while (end_first_slice != end_first_block &&
                 p[begin_first_slice].mask == p[end_first_slice].mask)
            end_first_slice++;

          while (begin_second_slice != end_second_block &&
                 p[begin_first_slice].mask != p[begin_second_slice].mask)
            begin_second_slice++;

          if (begin_second_slice != end_second_block) {

            size_t end_second_slice = begin_second_slice + 1;
            while (end_second_slice != end_second_block &&
                   p[begin_first_slice].mask == p[end_second_slice].mask)
              end_second_slice++;

            // This is the same code as in the unoptimized version except
            // that we can restrict the comparisons to smaller intervals.

            for (size_t i = begin_first_slice; i != end_first_slice; i++)
              for (size_t j = begin_second_slice; j != end_second_slice; j++)
                if (consensus(p[i], p[j], next, primes))
                  prime[i] = prime[j] = false;
          }

          begin_first_slice = end_first_slice;
        }
      }

      begin_first_block = begin_second_block;
      end_first_block = end_second_block;
    }

#endif

    for (size_t i = 0; i != size; i++)
      if (prime[i])
        primes.add(p[i]);

    next.normalize(); // Sort and remove duplicates.
    p = next;
  }
}

//------------------------------------------------------------------------//

// Parse command line options and set/reset input and output files.

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
  polynomial minterms;
  minterms.parse();
  minterms.normalize();
  polynomial primes, tmp = minterms;
  generate(tmp, primes);
  primes.normalize();
  primes.print(output);
  reset(argc);
  return 0;
}
