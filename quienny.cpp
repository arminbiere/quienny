/*------------------------------------------------------------------------*/
/* Copyright (c) 2024, Armin Biere, University of Freiburg, Germany       */
/*------------------------------------------------------------------------*/

static const char *usage = " usage : quienny[-h | -v][<input>[<output>]]\n";

/*------------------------------------------------------------------------*/

#include <algorithm>
#include <cassert>
#include <cctype>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

//------------------------------------------------------------------------//

using namespace std;

//------------------------------------------------------------------------//

// Global input and output variables.

static FILE *input_file;
static const char *input_path;
static bool close_input;

static FILE *output_file;
static const char *output_path;
static bool close_output;

/*------------------------------------------------------------------------*/

static size_t lineno = 1;

static int read_char() {
  int res = getc(input_file);
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

static void die(const char *, ...) __attribute__((format(printf, 1, 2)));
static void verbose(const char *, ...) __attribute__((format(printf, 1, 2)));
static void parse_error(const char *, ...)
    __attribute__((format(printf, 1, 2)));

static void die(const char *fmt, ...) {
  fputs("quienny: error: ", stderr);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

static void parse_error(const char *fmt, ...) {
  fprintf(stderr, "quienny: parse error: at line %zu in '%s': ", lineno,
          input_path);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

static int verbosity;

static void verbose(const char *fmt, ...) {
  if (verbosity < 1)
    return;
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  fflush(stderr);
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
    else if (ch != '0') {
      if (ch == EOF)
        parse_error("unexpected end-of-file (expected '0' or '1' or new-line)");
      else if (isprint(ch))
        parse_error("expected '0' or '1' or new-line at '%c'", ch);
      else
        parse_error("expected '0' or '1' or new-line at caracter code '0x%02x'",
                    ch);
    } else if (variables == max_variables)
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
    else if (ch != '0') {
      if (ch == EOF)
        parse_error("unexpected end-of-file (expected '0' or '1')");
      else if (ch == '\n') {
        assert(lineno > 1);
        lineno--;
        parse_error("unexpected new-line (expected '0' or '1')");
      } else if (isprint(ch))
        parse_error("expected '0' or '1' at '%c'", ch);
      else
        parse_error("expected '0' or '1' at caracter code '0x%02x'", ch);
    }
    values.set(i, value);
    mask.set(i, true);
    ones += value;
    ch = read_char();
  }
  if (ch != '\n') {
    if (ch == EOF)
      parse_error("unexpected end-of-file (expected new-line)");
    else if (isprint(ch))
      parse_error("expected new-line at '%c'", ch);
    else
      parse_error("expected new-line at caracter code '0x%02x'", ch);
  }
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

// The less-than operator '<' is used to sort and normalize the polynomial.
// Sorting is first done with respect to the number of (valid) '1' bits
// 'ones', then with respect to the 'mask' , and finally the 'values'.

// This order is required for the optimized algorithm to work.

bool monomial::operator<(const monomial &other) const {
  if (ones < other.ones)
    return true;
  if (ones > other.ones)
    return false;
  if (mask < other.mask)
    return true;
  if (mask > other.mask)
    return false;
  return values < other.values;
}

// Check whether the 'other' monomial differs in exactly one valid bit. If this
// is the case the result parameter 'where' is set to that bit position.  We
// specialize this for the fixed maximum variable sizes and therefore have
// two versions of the code (one with '#ifddef FIXED ... #else' and one for
// the general case withing '#else ... #endif').

static size_t compared; // Number of compared monomials.

bool monomial::match(const monomial &other, size_t &where) const {

  compared++;

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

// A polynomial is in essence a vector of monomials.

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

// Normalize the polynomial by sorting and removing duplicates.

void polynomial::normalize() {
  stable_sort(monomials.begin(), monomials.end());
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

// This the kernel of the Quine-McCluskey algorithm.  It tries to determine
// whether two monomials can be merged, i.e., only different in exactly one
// variable.  If this is the case the monomials are merged, the result goes
// to 'next' and the function returns 'true'. On failure return 'false'.

static inline bool consensus(const monomial &mi, const monomial &mj,
                             polynomial &next) {
  size_t k;
  if (!mi.match(mj, k))
    return false;
  monomial m = mi;          // Copy values and mask of 'mi'.
  assert(!m.values.get(k)); // As we have 'mi < mj' due to sorting.
  m.mask.set(k, false);     // Clear mask-bit at position 'k'.
  next.add(m);
  return true;
}

// Generate a normalized 'primes' polynomial of 'p' (destroys 'p') based on
// the Quine-McCluskey algorithm in an non-optimzed and optimized variant.

void generate(polynomial &p, polynomial &primes) {

  // The following two vectors are declared outside the main loop in order
  // to avoid allocating and deallocating them.  Instead they are cleared.

  vector<bool> prime; // Which monomials have been merged?
  polynomial next;    // Monomials kept for next round.

  size_t round = 0;

  while (!p.empty()) { // As long monomials were merged.

    round++;
    verbose("round %zu polynomial with %zu monomials", round, p.size());

    prime.clear();
    const size_t size = p.size();
    for (size_t i = 0; i != size; i++)
      prime.push_back(true);

    next.clear();

#ifdef NOPTIMIZE

    // This is the simple unoptimized version, which compares all pairs
    // (if enabled with './configure -n' or './configure --no-optimize').

    for (size_t i = 0; i + 1 != size; i++)
      for (size_t j = i + 1; j != size; j++)
        if (consensus(p[i], p[j], next))
          prime[i] = prime[j] = false;

#else

    // This is the optimized version (enabled by default).  It uses sorting
    // by normalization to avoid a quadratic number of 'match' comparisons,
    // similarly to one pass in merge-sort. But otherwise it relies on the
    // same 'consensus' kernel.

    // A 'block' is an interval of monomials with the same number 'ones' of
    // valid true bits'.  A 'slice' is an interval of monomials with the
    // same number 'ones' of true bits (thus a sub-interval of a block) and
    // also exactly the same valid bits in 'mask' set to true.

    // Only slices with the same 'mask' have to be compared in consecutive
    // blocks.  Therefore we go over all pairs of subsequent blocks and
    // compare corresponding slices within them.

    // As blocks are ordered by 'ones' and within blocks slices are ordered
    // by 'mask' and we only need to compare monomials with the same number
    // of ones and the same mask, the overall complexity of one outer main
    // loop round becomes linear in the size of the outer polynomial 'p'.

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
                if (consensus(p[i], p[j], next))
                  prime[i] = prime[j] = false;
          }

          begin_first_slice = end_first_slice;
        }
      }

      begin_first_block = begin_second_block;
      end_first_block = end_second_block;
    }

#endif

    // All the monomials which were not merged are prime implicants.

    for (size_t i = 0; i != size; i++)
      if (prime[i])
        primes.add(p[i]);

    next.normalize(); // Sort and remove duplicates.
    p = next;         // Now 'next' becomes new polynomial 'p'.
  }
}

//------------------------------------------------------------------------//

// Parse command line options and set/reset input and output files.

static void init(int argc, char **argv) {

  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp(arg, "-h")) {
      fputs(usage, stderr);
      exit(1);
    } else if (!strcmp(arg, "-v"))
      verbosity += verbosity >= 0 && (verbosity < INT_MAX);
    else if (arg[0] == '-' && arg[1])
      die("invalid option '%s' (try '-h')", arg);
    else if (!input_path)
      input_path = arg;
    else if (!output_path)
      output_path = arg;
    else
      die("too many files '%s', '%s', and '%s' (try '-h')", input_path,
          output_path, arg);
  }

  if (!input_path || (input_path && !strcmp(input_path, "-")))
    input_path = "<stdin>", input_file = stdin;
  else if (!(input_file = fopen(input_path, "r")))
    die("can not read '%s'", input_path);
  else
    close_input = true;

  if (!output_path || (output_path && !strcmp(output_path, "-")))
    output_path = "<stdin>", output_file = stdout;
  else if (!(output_file = fopen(output_path, "w")))
    die("can not write '%s'", output_path);
  else
    close_output = true;
}

static void reset(int argc) {
  if (close_input)
    fclose(input_file);
  if (close_output)
    fclose(output_file);
}

/*------------------------------------------------------------------------*/

int main(int argc, char **argv) {
  init(argc, argv);
  polynomial minterms;
  minterms.parse();
  minterms.normalize();
  polynomial primes, tmp = minterms;
  generate(tmp, primes);
  verbose("compared %zu monomials", compared);
  primes.normalize();
  verbose("primes polynomial with %zu monomials", primes.size());
  primes.print(output_file);
  reset(argc);
  return 0;
}
