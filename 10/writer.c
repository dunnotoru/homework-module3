#include "shared.h"
#include <bits/posix2_lim.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/select.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define D_INTEGER_LENGTH_MAX 13
#define D_EMPTY_MAX 1

int produce_data(int data[D_LINE_MAX]);
int put_data(int data[D_LINE_MAX], int size);
int writer_loop();

void handle_int(int sig);
int sem_init(const char *filename);
int shm_init(const char *filename);

int init(char *filename);
int cleanup();

uint8_t is_running = 1;

int shmid = -1;
int semid = -1;
SharedData *shared;

int data_processed = 0;

int main(int argc, char **argv) {
  if (argc > 2) {
    fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
    fprintf(stderr,
            "     filename - optional, if not present /etc/passwd is used\n");
    return EXIT_FAILURE;
  }

  struct sigaction sa_int = {.sa_handler = handle_int, .sa_flags = SA_RESTART};
  if (sigaction(SIGINT, &sa_int, NULL) == -1) {
    perror("Failed to register SIGINT handler");
    return EXIT_FAILURE;
  }

  srand(time(NULL));

  int status = EXIT_SUCCESS;

  char *filename = argc == 2 ? argv[1] : D_FILENAME;

  if (init(filename) != -1) {
    status = writer_loop() == -1 ? EXIT_FAILURE : EXIT_SUCCESS;
  }

  printf("Data arrays processed: %d\n", data_processed);

  if (cleanup() == -1) {
    status = EXIT_FAILURE;
  }

  return status;
}

int cleanup() {
  int status = 0;
  if (semid != -1 && semctl(semid, 0, IPC_RMID) == -1) {
    perror("Failed to remove semaphore");
    status = -1;
  }

  if (shared != (void *)-1 && shmdt(shared) == -1) {
    perror("Failed to detach shared memory");
    status = -1;
  }

  if (shmid != -1 && shmctl(shmid, IPC_RMID, NULL) == -1) {
    perror("Failed to close shared memory");
    status = -1;
  }

  return status;
}

int init(char *filename) {
  shmid = shm_init(filename);
  if (shmid == -1) {
    return -1;
  }

  semid = sem_init(filename);
  if (semid == -1) {
    return -1;
  }

  return 0;
}

int shm_init(const char *filename) {
  key_t key = ftok(filename, D_SHM_PROJ_ID);
  if (key == -1) {
    perror("Failed to generate key for shm");
    return -1;
  }

  int shmid = shmget(key, sizeof(SharedData), IPC_CREAT | IPC_EXCL | 0600);
  if (shmid == -1) {
    perror("Failed to get shared memory");
    return -1;
  }

  shared = (SharedData *)shmat(shmid, NULL, 0);
  if (shared == (void *)-1) {
    perror("Failed to attach shared memory");
    return -1;
  }

  return shmid;
}

int sem_init(const char *filename) {
  key_t key = ftok(filename, D_SEM_PROJ_ID);
  if (key == -1) {
    perror("Failed to generate key for semaphore");
    return -1;
  }

  // 1mutex, 2empty - N, 3full - 0
  uint8_t nsems = 3;
  int semid = semget(key, nsems, IPC_CREAT | IPC_EXCL | 0600);
  if (semid == -1) {
    perror("Failed to create/open semaphore");
    return -1;
  }

  union semun arg;
  arg.val = 1;
  if (semctl(semid, 0, SETVAL, arg) == -1) {
    perror("Failed to set semaphore value");
    return -1;
  }

  arg.val = D_EMPTY_MAX;
  if (semctl(semid, 1, SETVAL, arg) == -1) {
    perror("Failed to set semaphore value");
    return -1;
  }

  arg.val = 0;
  if (semctl(semid, 2, SETVAL, arg) == -1) {
    perror("Failed to set semaphore value");
    return -1;
  }

  return semid;
}

int writer_loop() {
  while (is_running) {

    int data[D_LINE_MAX];
    int size = produce_data(data);
    int status = put_data(data, size);
    if (status == -1) {
      return -1;
    }

    if (status == 1) {
      usleep(500000);
    }
  }

  return 0;
}

int produce_data(int data[D_LINE_MAX]) {
  int num_count = rand() % (D_LINE_MAX / 2) + D_LINE_MAX / 2;
  for (int i = 0; i < num_count; i++) {
    data[i] = rand();
  }

  return num_count;
}

int try_put(int data[D_LINE_MAX], int size) {
  if (shared->size == -1) {
    printf("Received Min: %d, Max: %d\n", shared->min, shared->max);
    data_processed++;
  }

  shared->size = size;
  memcpy(shared->numbers, data, size * sizeof(int));

  return 0;
}

int put_data(int data[D_LINE_MAX], int size) {
  if (semop(semid, push, 2) == -1) {
    if (errno == EAGAIN) {
      printf("Buffer full\n");
    } else {
      perror("Failed to lock empty-full semaphore");
    }

    return errno == EAGAIN ? 1 : -1;
  }

  if (semop(semid, &lock_res, 1) == -1) {
    perror("Failed to lock mutex semaphore");
    return -1;
  }

  int status = try_put(data, size);

  if (semop(semid, &rel_res, 1) == -1) {
    perror("Failed to release mutex semaphore");
    return -1;
  }

  if (status == -1) {
    perror("Failed to write file");
  }

  return status;
}

void handle_int(int sig) {
  if (sig == SIGINT) {
    is_running = 0;
  }
}
