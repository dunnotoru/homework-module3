#include <fcntl.h>
#include <mqueue.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MESSAGE_SIZE 256
#define QUEUE_PERMISSION 0600

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stdin, "Usage: client <queue-name>\n");
    return 1;
  }

  mqd_t qds = mq_open(argv[1], O_RDWR, QUEUE_PERMISSION, NULL);
  if (qds == (mqd_t)-1) {
    perror("Failed to open message queue");
    return 1;
  }

  char buffer[MESSAGE_SIZE];
  uint8_t retval = 0;
  while (true) {
    printf("> ");
    unsigned int prio = 1;
    if (fgets(buffer, MESSAGE_SIZE, stdin) == NULL) {
      perror("Failed to read input");
      break;
    }

    if (buffer[0] == '\n') {
      prio = 0;
    }

    buffer[strcspn(buffer, "\n")] = '\0';
    int a =0;

    if (mq_send(qds, buffer, MESSAGE_SIZE, prio) == -1) {
      perror("Failed to send message");
      retval = 1;
      break;
    }
    
    if (prio == 0) {
      printf("Closing connection...\n");
      break;
    }

    if (mq_receive(qds, buffer, MESSAGE_SIZE, &prio) == -1) {
      perror("Failed to receive message");
      retval = 1;
      break;
    }

    if (prio == 0) {
      printf("Connection closed\n");
      break;
    }

    printf("Server: %s\n", buffer);
  }

  if (mq_close(qds) == -1) {
    perror("Failed to close queue");
  }

  printf("===END===\n");

  return retval;
}
