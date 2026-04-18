#include "shared.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>

int get_min_max(int text[D_LINE_MAX], int size, int *min, int *max);
int reader_loop();

void handle_int(int sig);
int sem_init(const char *filename);
int shm_init(const char *filename);

int init(char *filename);
int cleanup();

uint8_t is_running = 1;

int semid = -1;
int shmid = -1;
SharedData *shared = NULL;

int main(int argc, char **argv) {
  if (argc > 2) {
    fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
    fprintf(stderr, "     filename - optional\n");
    return EXIT_FAILURE;
  }

  struct sigaction sa_int = {.sa_handler = handle_int, .sa_flags = SA_RESTART};
  if (sigaction(SIGINT, &sa_int, NULL) == -1) {
    perror("Failed to register SIGINT handler");
    return 1;
  }

  int status = EXIT_SUCCESS;

  char *filename = argc == 2 ? argv[1] : D_FILENAME;

  if (init(filename) != -1) {
    status = reader_loop() == -1 ? EXIT_FAILURE : EXIT_SUCCESS;
  }

  if (cleanup() == -1) {
    status = EXIT_FAILURE;
  }

  return status;
}

int cleanup() {
  int status = 0;
  if (shared != (void *)-1 && shmdt(shared) == -1) {
    perror("Failed to detach shared memory");
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

  int shmid = shmget(key, sizeof(SharedData), 0);
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
  int semid = semget(key, nsems, 0);
  if (semid == -1) {
    perror("Failed to create/open semaphore");
    return -1;
  }

  return semid;
}

int get_min_max(int numbers[D_LINE_MAX], int size, int *min, int *max) {
  *min = INT_MAX;
  *max = -1;

  for (int i = 0; i < size; i++) {
    int num = numbers[i];
    if (num < *min) {
      *min = num;
    }

    if (num > *max) {
      *max = num;
    }
  }

  return 0;
}

int try_get() {
  if (shared->size > 0) {
    get_min_max(shared->numbers, shared->size, &shared->min, &shared->max);
    shared->size = -1;
    printf("Found min: %d, max: %d\n", shared->min, shared->max);
  }

  return 0;
}

int get_data() {
  if (semop(semid, pop, 2) == -1) {
    if (errno == EAGAIN) {
      printf("Buffer empty\n");
    } else {
      perror("Failed to lock empty-full semaphore");
    }

    return errno == EAGAIN ? 1 : -1;
  }

  if (semop(semid, &lock_res, 1) == -1) {
    perror("Failed to lock mutex semaphore");
    return -1;
  }

  int status = try_get();

  if (semop(semid, &rel_res, 1) == -1) {
    perror("Failed to release mutex semaphore");
    return -1;
  }

  if (status == -1) {
    perror("Failed to read/write data");
  }

  return status;
}

int reader_loop() {
  int line_number = 0;
  while (is_running) {
    int status = get_data();
    if (status == -1) {
      return -1;
    }

    if (status == 1) {
      usleep(500000);
      continue;
    }

    line_number++;
  }

  return 0;
}

void handle_int(int sig) {
  if (sig == SIGINT) {
    is_running = 0;
  }
}
