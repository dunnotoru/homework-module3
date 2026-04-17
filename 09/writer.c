#include "shared.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define D_INTEGER_LENGTH_MAX 13

int put_data(int fd, sem_t *mutex, sem_t *empty, sem_t *full);
int writer_loop(int fd, sem_t *mutex, sem_t *empty, sem_t *full);
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

  char buffer[64];
  int oflags = O_CREAT | O_EXCL;
  sprintf(buffer, "/%s-%s", filename, "-mutex");
  sem_t *mutex = sem_open(buffer, oflags, 0600, 1);
  sprintf(buffer, "/%s-%s", filename, "-empty");
  sem_t *empty = sem_open(buffer, oflags, 0600, D_EMPTY_MAX);
  sprintf(buffer, "/%s-%s", filename, "-full");
  sem_t *full = sem_open(buffer, oflags, 0600, 0);

  if (mutex == SEM_FAILED || empty == SEM_FAILED || full == SEM_FAILED) {
    status = EXIT_FAILURE;
  }

  if (status == EXIT_SUCCESS && writer_loop(fd, mutex, empty, full) == -1) {
    status = EXIT_FAILURE;
  }

  if (mutex != SEM_FAILED && sem_close(mutex) == -1) {
    perror("Failed to remove mutex semaphore");
    status = EXIT_FAILURE;
  }

  if (empty != SEM_FAILED && sem_close(empty) == -1) {
    perror("Failed to remove empty semaphore");
    status = EXIT_FAILURE;
  }

  if (full != SEM_FAILED && sem_close(empty) == -1) {
    perror("Failed to remove empty semaphore");
    status = EXIT_FAILURE;
  }

  if (fd != -1 && close(fd) == -1) {
    perror("Failed to close file");
    status = EXIT_FAILURE;
  }

  return status;
}

int writer_loop(int fd, sem_t *mutex, sem_t *empty, sem_t *full) {
  int line_number = 0;

  while (is_running) {
    int status = put_data(fd, mutex, empty, full);
    if (status == -1) {
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

int put_data(int fd, sem_t *mutex, sem_t *empty, sem_t *full) {
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

  if (sem_trywait(empty) == -1) {
    if (errno == EAGAIN) {
      printf("Buffer full\n");
    } else {
      perror("Failed to lock empty-full semaphore");
    }

    return errno == EAGAIN ? 1 : -1;
  }

  if (sem_wait(mutex) == -1) {
    perror("Failed to lock mutex semaphore");
    return -1;
  }

  ssize_t bytes_write = write(fd, buffer, D_LINE_MAX);
  if (sem_post(mutex) == -1) {
    perror("Failed to release mutex semaphore");
    return -1;
  }

  if (sem_post(full) == -1) {
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
