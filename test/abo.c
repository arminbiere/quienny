#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc != 2)
    return 1;
  int n = atoi(argv[1]);
  if (n <= 0)
    return 1;
  if (n > 64)
    return 1;
  const uint64_t first = 0;
  const uint64_t last = n < 64 ? ((uint64_t)1 << n) - 1 : ~(uint64_t)0;
  uint64_t w = first;
  do {
    for (int i = 0; i != n; i++)
      fputc('0' + !!(w & ((uint64_t)1 << (n - i - 1))), stdout);
    fputc('\n', stdout);
  } while (++w != last);
  return 0;
}
