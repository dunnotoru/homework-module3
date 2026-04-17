#ifndef SHARED_H
#define SHARED_H

#include <string.h>
#include <sys/sem.h>

#define D_FILENAME "resource_file.txt"
#define D_LINE_MAX 1024
#define D_PROJ_ID 123123

union semun {
  int val;
  struct semid_ds *buf;
  unsigned short *array;
};

struct sembuf lock_res = {0, -1, 0};
struct sembuf rel_res = {0, 1, 0};
struct sembuf push[2] = {{1, -1, IPC_NOWAIT}, {2, 1, IPC_NOWAIT}};
struct sembuf pop[2] = {{1, 1, IPC_NOWAIT}, {2, -1, IPC_NOWAIT}};

char *strncpy_s(char *dest, const char *src, size_t n);

#endif