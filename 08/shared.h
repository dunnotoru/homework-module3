#ifndef SHARED_H
#define SHARED_H

#include <string.h>
#include <sys/sem.h>

#define D_FILENAME "resource_file.txt"
#define D_LINE_MAX 1024
#define D_PROJ_ID 123123

struct sembuf lock = {0, -1, 0};
struct sembuf unlock = {0, 1, 0};

static char *strncpy_s(char *dest, const char *src, size_t n) {
  char *retval = strncpy(dest, src, n);
  dest[n - 1] = '\0';
  return retval;
}

#endif