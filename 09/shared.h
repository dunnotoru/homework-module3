#ifndef SHARED_H
#define SHARED_H

#include <string.h>

#define D_FILENAME "resource_file.txt"
#define D_LINE_MAX 1024
#define D_EMPTY_MAX 1000

char *strncpy_s(char *dest, const char *src, size_t n);

#endif