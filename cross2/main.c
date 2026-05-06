#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 64
#define MAX_COMMAND_SIZE 64
#define MAX_ARGV_SIZE 8192
#define SPACE_DELIM " \t\f\n\r\v"
#define MAX_DRIVERS 128

typedef enum {
  AVAILABLE = 0,
  BUSY,
} DriverStatus;

typedef struct {
  pid_t pid;
  int to_driver_pipefd;
  int to_master_pipefd;
} Driver;

Driver drivers[MAX_DRIVERS] = {0};
size_t drivers_count = 0;

Driver *get_driver_by_pid(pid_t pid) {
  for (size_t i = 0; i < drivers_count; i++) {
    if (drivers[i].pid == pid) {
      return &drivers[i];
    }
  }

  return NULL;
}

void set_nonblock(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int driver_routine(int to_driver_pipefd, int to_master_pipefd) {
  pid_t pid = getpid();

  int epollfd = epoll_create1(0);
  if (epollfd == -1) {
    fprintf(stderr, "DRIVER [%d]\tFailed to create epoll\n", pid);
    return -1;
  }

  struct epoll_event ev, events[2];

  ev.events = EPOLLET | EPOLLIN;
  ev.data.fd = to_driver_pipefd;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, to_driver_pipefd, &ev);

  int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
  ev.data.fd = timerfd;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, timerfd, &ev);

  struct itimerspec ts = {0};

  DriverStatus status = AVAILABLE;

  while (1) {
    int nfds = epoll_wait(epollfd, events, 2, -1);
    if (nfds == -1) {
      perror("Failed to epoll wait");
      close(epollfd);
      close(timerfd);
      return -1;
    }

    for (int i = 0; i < nfds; i++) {
      if (events[i].data.fd == to_driver_pipefd) {
        char buffer[128] = {0};
        int bytes_read = read(to_driver_pipefd, buffer, sizeof(buffer));
        if (bytes_read == -1) {
          fprintf(stderr, "DRIVER [%d]\t", pid);
          perror("Failed to read from pipe");
          close(epollfd);
          close(timerfd);
          return -1;
        }

        // printf("DRIVER [%d] %s\n", pid, buffer);
        char *token = strtok(buffer, ":");
        if (strcmp(token, "status") == 0 || status == BUSY) {
          memset(buffer, 0, sizeof(buffer));
          if (status == AVAILABLE) {
            snprintf(buffer, sizeof(buffer), "Available");
          } else {
            snprintf(buffer, sizeof(buffer), "Busy %zu", ts.it_value.tv_sec);
          }
          int bytes_written = write(to_master_pipefd, buffer, sizeof(buffer));
          if (bytes_written == -1) {
            perror("Failed to write to named pipe");
            return -1;
          }
        }

        if (strcmp(token, "task") == 0) {
          token = strtok(NULL, ":");
          int time = strtol(token, NULL, 10);
          ts.it_value.tv_sec = time;
          if (timerfd_settime(timerfd, 0, &ts, NULL) == -1) {
            perror("Failed to set timer");
            return -1;
          }

          status = BUSY;
          int bytes_written = write(to_master_pipefd, "ok", sizeof("ok"));
        }

      } else {
        uint64_t res;
        read(timerfd, &res, sizeof(res));
        status = AVAILABLE;
      }
    }
  }

  return 0;
}

int create_driver() {
  char to_driver[32];
  char to_master[32];
  snprintf(to_driver, sizeof(to_driver), "/tmp/fifo-%d", rand());
  if (mkfifo(to_driver, O_RDWR | 0600)) {
    perror("Failed to create named pipe");
    return -1;
  }

  snprintf(to_master, sizeof(to_master), "/tmp/fifo-%d", rand());
  if (mkfifo(to_master, O_RDWR | 0600)) {
    perror("Failed to created named pipe");
    return -1;
  }

  int to_driver_pipefd = open(to_driver, O_RDWR);
  if (to_driver_pipefd == -1) {
    perror("Failed to open named pipe");
    return -1;
  }

  int to_master_pipefd = open(to_master, O_RDWR);
  if (to_master_pipefd == -1) {
    perror("Failed to open named pipe");
    return -1;
  }

  pid_t pid = fork();

  if (pid == -1) {
    unlink(to_driver);
    unlink(to_master);
    return -1;
  }

  if (pid == 0) {
    // im fork

    int status = driver_routine(to_driver_pipefd, to_master_pipefd);

    _exit(status == -1 ? EXIT_FAILURE : EXIT_SUCCESS);
  }

  Driver driver = {pid, to_driver_pipefd, to_master_pipefd};
  drivers[drivers_count++] = driver;

  printf("Created: driver [%d]\n", pid);
  return 0;
}

int send_task(pid_t pid, int timer) {
  Driver *driver = get_driver_by_pid(pid);
  if (driver == NULL) {
    return -1;
  }

  char buff[128] = {0};
  snprintf(buff, sizeof(buff), "task:%d", timer);
  int bytes_written = write(driver->to_driver_pipefd, buff, sizeof(buff));
  if (bytes_written == -1) {
    perror("Failed to write to pipe");
    return -1;
  }

  memset(buff, 0, sizeof(buff));
  int bytes_read = read(driver->to_master_pipefd, buff, sizeof(buff));
  if (bytes_read == -1) {
    perror("Failed to read from pipe");
    return -1;
  }

  printf("Response from [%d]: %s\n", driver->pid, buff);
  return 0;
}

int get_status(pid_t pid) {
  Driver *driver = get_driver_by_pid(pid);

  int bytes_written =
      write(driver->to_driver_pipefd, "status:", sizeof("status:"));
  if (bytes_written == -1) {
    perror("Failed to write to pipe");
    return -1;
  }

  char buffer[128] = {0};
  int bytes_read = read(driver->to_master_pipefd, buffer, sizeof(buffer));
  if (bytes_read == -1) {
    perror("Failed to read from pipe");
    return -1;
  }

  printf("Response from [%d]: %s\n", pid, buffer);
  return 0;
}

int get_drivers() {
  for (size_t i = 0; i < drivers_count; i++) {
    get_status(drivers[i].pid);
  }

  return 0;
}

int run(const char *command) {
  char buffer[MAX_COMMAND_SIZE] = {0};
  strncpy(buffer, command, MAX_COMMAND_SIZE);

  char *token = strtok(buffer, SPACE_DELIM);
  if (strcmp(token, "create_driver") == 0) {
    if (strtok(NULL, SPACE_DELIM) == NULL) {
      return create_driver();
    } else {
      return -1;
    }
  }

  if (strcmp(token, "send_task") == 0) {
    token = strtok(NULL, SPACE_DELIM);
    if (token == NULL) {
      fprintf(stderr,
              "Missing argument for send_task, driver pid is required.\n");
      return -1;
    }

    char *endptr;
    pid_t pid = strtol(token, &endptr, 10);
    if (*endptr != '\0') {
      fprintf(stderr, "Invalid pid.\n");
      return -1;
    }

    token = strtok(NULL, SPACE_DELIM);
    if (token == NULL) {
      fprintf(stderr, "Missing argument for send_task, timer is required.\n");
      return -1;
    }

    int timer = strtol(token, &endptr, 10);
    if (*endptr != '\0') {
      fprintf(stderr, "Invalid timer (int).\n");
      return -1;
    }

    return send_task(pid, timer);
  }

  if (strcmp(token, "get_status") == 0) {
    token = strtok(NULL, SPACE_DELIM);
    if (token == NULL) {
      fprintf(stderr,
              "Missing argument for get_status, driver pid is required.\n");
      return -1;
    }

    char *endptr;
    pid_t pid = strtol(token, &endptr, 10);
    if (*endptr != '\0') {
      fprintf(stderr, "Invalid pid.\n");
      return -1;
    }

    return get_status(pid);
  }

  if (strcmp(token, "get_drivers") == 0) {
    token = strtok(NULL, SPACE_DELIM);
    if (token != NULL) {
      fprintf(stderr, "To many arguments for get_drivers.\n");
      return -1;
    }

    return get_drivers();
  }

  return -1;
}

int main() {
  srand(time(NULL));

  while (1) {
    printf("$> ");
    char cmd_buffer[MAX_COMMAND_SIZE];

    if (fgets(cmd_buffer, MAX_COMMAND_SIZE, stdin) == NULL) {
      return EXIT_FAILURE;
    }

    if (cmd_buffer[0] == '\n') {
      continue;
    }

    cmd_buffer[strcspn(cmd_buffer, "\r\n")] = '\0';

    if (strcmp(cmd_buffer, "exit") == 0) {
      return EXIT_SUCCESS;
    }

    run(cmd_buffer);
    memset(cmd_buffer, 0, sizeof(cmd_buffer));
  }

  return EXIT_SUCCESS;
}