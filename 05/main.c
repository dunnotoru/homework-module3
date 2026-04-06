#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

uint32_t counter = 1;
uint8_t remains = 3;
int8_t sig_to_write = -1;

void handler(int sig) {
  if (sig == SIGINT) {
    remains--;
  }

  sig_to_write = sig;
}

void write_file(int sig, int fd) {
  char *str = NULL;
  switch (sig) {
  case SIGINT:
    str = "SIGINT ";
    break;
  case SIGQUIT:
    str = "SIGQUIT";
    break;
  default:
    break;
  }

  if (str == NULL) {
    return;
  }

  char buffer[64];
  sprintf(buffer, "%s signal received and processed.\n", str);
  
  int retval = write(fd, buffer, strlen(buffer));
  if (retval == -1) {
    perror("Couldn't write to file.\n");
  }
}

int main() {
  struct sigaction sa_int = {.sa_handler = handler, .sa_flags = SA_RESTART};
  struct sigaction sa_quit = {.sa_handler = handler, .sa_flags = SA_RESTART};

  if (sigaction(SIGINT, &sa_int, NULL) == -1) {
    perror("Couldn't register SIGINT handler");
    return 1;
  }

  if (sigaction(SIGQUIT, &sa_quit, NULL) == -1) {
    perror("Couldn't register SIGQUIT handler");
    return 1;
  }

  unlink("file.log");
  int fd = open("file.log", O_CREAT | O_RDWR | O_APPEND, S_IRUSR | S_IWUSR);

  while (1) {
    if (sig_to_write != -1) {
      write_file(sig_to_write, fd);
      sig_to_write = -1;
    }

    if (remains == 0) {
      break;
    }

    printf("Counter: %u\n", counter++);
    sleep(1);
  }

  printf("THE END\n");

  close(fd);
}