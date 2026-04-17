#include "shared.h"
#include <errno.h>
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

int get_min_max(char text[D_LINE_MAX], int *min, int *max);
int reader_loop(int fd, int semid);
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

  int semid = semaphore_init(filename);
  if (semid == -1) {
    status = EXIT_FAILURE;
  }

  if (status == EXIT_SUCCESS && reader_loop(fd, semid) == -1) {
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

int get_data(int fd, int semid, char buffer[D_LINE_MAX]) {
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

  ssize_t bytes_read = read(fd, buffer, D_LINE_MAX);
  if (semop(semid, &rel_res, 1) == -1) {
    perror("Failed to release mutex semaphore");
    return -1;
  }

  if (bytes_read == -1) {
    perror("Failed to read file");
    return -1;
  }

  return 0;
}

int reader_loop(int fd, int semid) {
  int line_number = 0;
  while (is_running) {
    char buffer[D_LINE_MAX];

    int status = get_data(fd, semid, buffer);
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

int semaphore_init(char *filename) {
  key_t key = ftok(filename, D_PROJ_ID);
  if (key == -1) {
    perror("Failed to generate key");
    return -1;
  }

  uint8_t nsems = 1;
  int semid = semget(key, nsems, 0600);
  if (semid == -1) {
    perror("Failed to create/open semaphore");
    return -1;
  }

  return semid;
}
