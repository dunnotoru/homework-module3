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
#include <sys/types.h>
#include <unistd.h>

int get_min_max(char text[D_LINE_MAX], int *min, int *max);
int reader_loop(int fd, sem_t *mutex, sem_t *empty, sem_t *full);
int semaphore_init(char *filename);
void handle_int(int sig);

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
    return 1;
  }

  char *filename = argc == 2 ? argv[1] : D_FILENAME;
  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    perror("Failed to open file");
    return -1;
  }

  int status = EXIT_SUCCESS;

  char buffer[64];
  int oflags = 0;
  sprintf(buffer, "/%s-%s", filename, "-mutex");
  sem_t *mutex = sem_open(buffer, oflags);
  sprintf(buffer, "/%s-%s", filename, "-empty");
  sem_t *empty = sem_open(buffer, oflags);
  sprintf(buffer, "/%s-%s", filename, "-full");
  sem_t *full = sem_open(buffer, oflags);

  if (mutex == SEM_FAILED || empty == SEM_FAILED || full == SEM_FAILED) {
    status = EXIT_FAILURE;
  }

  if (status == EXIT_SUCCESS && reader_loop(fd, mutex, empty, full) == -1) {
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

int get_min_max(char text[D_LINE_MAX], int *min, int *max) {
  char buffer[D_LINE_MAX];
  strncpy_s(buffer, text, D_LINE_MAX);
  char *token = strtok(buffer, " ");
  *min = INT_MAX;
  *max = -1;
  do {
    char *endptr;
    int num = strtol(token, &endptr, 10);
    if (*endptr == '\0') {
      if (num < *min) {
        *min = num;
      }

      if (num > *max) {
        *max = num;
      }
    }
    token = strtok(NULL, " ");
  } while (token != NULL);

  return 0;
}

int get_data(int fd, char buffer[D_LINE_MAX], sem_t *mutex, sem_t *empty,
             sem_t *full) {
  if (sem_trywait(full) == -1) {
    if (errno == EAGAIN) {
      printf("Buffer empty\n");
    } else {
      perror("Failed to lock empty-full semaphore");
    }

    return errno == EAGAIN ? 1 : -1;
  }

  if (sem_wait(mutex) == -1) {
    perror("Failed to lock mutex semaphore");
    return -1;
  }

  ssize_t bytes_read = read(fd, buffer, D_LINE_MAX);
  if (sem_post(mutex) == -1) {
    perror("Failed to release mutex semaphore");
    return -1;
  }

  if (sem_post(empty) == -1) {
    perror("Failed to post empty semaphore");
    return -1;
  }

  if (bytes_read == -1) {
    perror("Failed to read file");
    return -1;
  }

  return 0;
}

int reader_loop(int fd, sem_t *mutex, sem_t *empty, sem_t *full) {
  int line_number = 0;
  while (is_running) {
    char buffer[D_LINE_MAX];

    int status = get_data(fd, buffer, mutex, empty, full);
    if (status == -1) {
      return -1;
    }

    if (status == 1) {
      usleep(500000);
      continue;
    }

    int min, max;
    if (get_min_max(buffer, &min, &max) == -1) {
      perror("Failed to get min max");
      return -1;
    }

    line_number++;
    printf("Min: %d, max %d in line no. %d\n", min, max, line_number);
  }

  return 0;
}

void handle_int(int sig) {
  if (sig == SIGINT) {
    is_running = 0;
  }
}