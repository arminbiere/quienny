Quienny Tests
-------------

After compilation `./run.sh` executes `quienny` on the `.pol` input
polynomials. Their primes are are written to `.out` and compared to the
'golden' set of primes in `.gld`.

There are also three simple generator of polynomials `all`, which produces
all minterms of the given size, `abo`, which generates all except the one
with only ones, and `abz` which also generates all minterms except the one
with only zeros.  The first set of polynomials produced by `all` have
worst-case expected running time, while the latter ones also require many
cube comparisons. These performance regression test generators are compiled
with `make all`, `make abo`, or `make abz`.
