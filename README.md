Quienny Prime Implicant Generator
---------------------------------

Generate all prime implicants for given set of minterms using
an optimized version of the Quine-McCluskey algorithm

To build run `./configure && make` and optionally `make test`.
This produces `quienny` which can handle an arbitrary number
of variables but also `quienny<n>` for 'i' in '8, 16, 32, 64',
which can only handle a fixed maximum size of 'i' variables,
but is much faster (as it uses machine words instead of arrays).

By default an optimized version using the hamming distance between
cubes is used, which reduces the number of cubes compared.  This
optimization can be disabled by `./configure --no-optimization`.
