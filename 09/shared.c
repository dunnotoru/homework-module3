#include <string.h>

char *strncpy_s(char *dest, const char *src, size_t n) {
  char *retval = strncpy(dest, src, n);
  dest[n - 1] = '\0';
  return retval;
}