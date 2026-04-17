#include "shared.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define D_INTEGER_LENGTH_MAX 13
#define D_EMPTY_MAX 1000

int put_data(int fd, int semid);
int writer_loop(int fd, int semid);
void handle_int(int sig);
int semaphore_init(const char *filename);

uint8_t is_running = 1;

int main(int argc, char **argv) {
  if (argc > 2) {
    fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
    fprintf(stderr, "     filename - optional\n");
    return EXIT_FAILURE;
  }

  struct sigaction sa_int = {.sa_handler = handle_int, .sa_flags = SA_RESTART};
  if (sigaction(SIGINT, &sa_int, NULL) == -1) {
    perror("Failed to register SIGINT handler");
    return EXIT_FAILURE;
  }

  srand(time(NULL));

  char *filename = argc == 2 ? argv[1] : D_FILENAME;
  int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0600);
  if (fd == -1) {
    perror("Failed to open file");
    return EXIT_FAILURE;
  }

  int status = EXIT_SUCCESS;
  int semid = semaphore_init(filename);
  if (semid == -1) {
    status = EXIT_FAILURE;
  }

  if (status == EXIT_SUCCESS && writer_loop(fd, semid) == -1) {
    status = EXIT_FAILURE;
  }

  if (semid != -1 && semctl(semid, 0, IPC_RMID) == -1) {
    perror("Failed to remove semaphore");
    status = EXIT_FAILURE;
  }

  if (close(fd) == -1) {
    perror("Failed to close file");
    status = EXIT_FAILURE;
  }

  return status;
}

int semaphore_init(const char *filename) {
  key_t key = ftok(filename, D_PROJ_ID);
  if (key == -1) {
    perror("Failed to generate key");
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

int writer_loop(int fd, int semid) {
  int line_number = 0;

  while (is_running) {
    int status = put_data(fd, semid);
    if (status == -1) {
      close(fd);
      return -1;
    }

    if (status == 0) {
      line_number++;
      printf("Line number %d\n", line_number);
    }

    if (status == 1) {
      usleep(500000);
    }
  }

  return 0;
}

int put_data(int fd, int semid) {
  int num_count = rand() % 100 + 50;
  int buffer_length = 0;
  char buffer[D_LINE_MAX] = {0};
  char number_str[D_INTEGER_LENGTH_MAX];
  for (int i = 0; i < num_count; i++) {
    uint32_t number = rand();
    snprintf(number_str, D_INTEGER_LENGTH_MAX, "%d ", number);
    int len = strlen(number_str);
    if (buffer_length + len > D_LINE_MAX) {
      break;
    }

    strncat(buffer, number_str, D_LINE_MAX - buffer_length);
    buffer_length += len;
  }

  buffer[D_LINE_MAX - 1] = '\n';

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

  ssize_t bytes_write = write(fd, buffer, D_LINE_MAX);
  if (semop(semid, &rel_res, 1) == -1) {
    perror("Failed to release mutex semaphore");
    return -1;
  }

  if (bytes_write == -1) {
    perror("Failed to write file");
    return -1;
  }

  return 0;
}

void handle_int(int sig) {
  if (sig == SIGINT) {
    is_running = 0;
  }
}
