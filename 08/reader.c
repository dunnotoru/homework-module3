#include "shared.h"
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <unistd.h>

uint8_t is_running = 1;

int get_min_max(char text[D_LINE_MAX], int *min, int *max);
int reader_loop(int fd, int semid);
void handle_int(int sig);

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

int reader_loop(int fd, int semid) {
  int line_number = 0;
  while (is_running) {
    char buffer[D_LINE_MAX];
    if (semop(semid, &lock, 1) == -1) {
      perror("Failed to lock semaphore");
      return -1;
    }

    ssize_t bytes_read = read(fd, buffer, D_LINE_MAX);
    usleep(500000);
    if (semop(semid, &unlock, 1) == -1) {
      perror("Failed to unlock semaphore");
      return -1;
    }

    if (bytes_read == -1) {
      perror("Failed to read");
      return -1;
    }

    if (bytes_read == 0) {
      usleep(100000);
      continue;
    }

    int min;
    int max;
    if (get_min_max(buffer, &min, &max) == -1) {
      perror("Failed to get min max");
      return -1;
    }

    line_number++;

    printf("Min: %d, max %d in line no. %d\n", min, max, line_number);
  }

  if (close(fd) == -1) {
    perror("Failed to close file");
    return -1;
  }

  return 0;
}

void handle_int(int sig) {
  if (sig == SIGINT) {
    is_running = 0;
  }
}

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

  key_t key = ftok(filename, D_PROJ_ID);
  if (key == -1) {
    perror("Failed to generate key");
    return EXIT_FAILURE;
  }

  uint8_t nsems = 1;
  int semid = semget(key, nsems, 0600);
  if (semid == -1) {
    perror("Failed to create/open semaphore");
    return EXIT_FAILURE;
  }

  if (reader_loop(fd, semid) == -1) {
    return EXIT_FAILURE;
  }

  if (close(fd) == -1) {
    perror("Failed to close file");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}