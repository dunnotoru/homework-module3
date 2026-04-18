#ifndef SHARED_H
#define SHARED_H

#include <string.h>
#include <sys/sem.h>

#define D_FILENAME "/etc/passwd"
#define D_LINE_MAX 1024
#define D_SHM_SIZE D_LINE_MAX + 2
#define D_SEM_PROJ_ID 123123
#define D_SHM_PROJ_ID 321321

typedef struct SharedData {
  int numbers[D_LINE_MAX];
  int size;
  int min;
  int max;
} SharedData;

union semun {
  int val;
  struct semid_ds *buf;
  unsigned short *array;
};

struct sembuf lock_res = {0, -1, 0};
struct sembuf rel_res = {0, 1, 0};
struct sembuf push[2] = {{1, -1, IPC_NOWAIT}, {2, 1, IPC_NOWAIT}};
struct sembuf pop[2] = {{1, 1, IPC_NOWAIT}, {2, -1, IPC_NOWAIT}};

#endif