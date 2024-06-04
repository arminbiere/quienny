#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc != 2)
    return 1;
  int n = atoi(argv[1]);
  if (n <= 0)
    return 1;
  if (n > 31)
    return 1;
  unsigned w = 0, end = (1u << n) - 1;
  while (w != end) {
    for (int i = 0; i != n; i++)
      fputc('0' + !!(w & (1u << (n - i - 1))), stdout);
    fputc('\n', stdout);
    w++;
  }
  return 0;
}
