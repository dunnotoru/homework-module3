#include "shared.h"
#include <fcntl.h>
#include <limits.h>
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

int write_line(int fd, int semid) {
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

  if (semop(semid, &lock, 1) == -1) {
    perror("Failed to lock semaphore");
    return -1;
  }

  ssize_t bytes_write = write(fd, buffer, D_LINE_MAX);
  if (semop(semid, &unlock, 1) == -1) {
    perror("Failed to unlock semaphore");
    return -1;
  }

  if (bytes_write == -1) {
    perror("Failed to write file");
    return -1;
  }

  return 0;
}

int writer_loop(int semid) {
  int line_number = 0;
  int fd = open(D_FILENAME, O_WRONLY | O_CREAT | O_APPEND, 0600);
  if (fd == -1) {
    perror("Failed to open file");
    return -1;
  }

  while (1) {
    if (write_line(fd, semid) == -1) {
      close(fd);
      return -1;
    }

    line_number++;
    printf("Line number %d\n", line_number);
    sleep(1);
  }

  if (close(fd) == -1) {
    perror("Failed to close file");
    return -1;
  }

  return 0;
}

int main(int argc, char **argv) {
  srand(time(NULL));

  key_t key = 123123;
  uint8_t nsems = 1;
  int semid = semget(key, nsems, IPC_CREAT | 0600);
  if (semid == -1) {
    perror("Failed to create/open semaphore");
    return EXIT_FAILURE;
  }

  union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
  } arg;

  arg.val = 1;
  if (semctl(semid, 0, SETVAL, arg) == -1) {
    perror("Failed to initialize semaphore");
    return EXIT_FAILURE;
  }

  if (writer_loop(semid) == -1) {
    return EXIT_FAILURE;
  }

  if (semctl(semid, 0, IPC_RMID) == -1) {
    perror("Failed to remove semaphore");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}