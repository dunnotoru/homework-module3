

#include "shared.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

int main() {
  printf("========== CLIENT ==========\n");
  pid_t pid = getpid();
  printf("Pid [%d] is used as client id.\n", pid);

  int msqid;
  key_t key = 123123;
  msqid = msgget(key, 0);
  if (msqid == -1) {
    perror("Failed to get system v message queue");
    return EXIT_FAILURE;
  }

  char buffer[BUFFER_SIZE];
  sprintf(buffer, "%d:", pid);
  buffer[BUFFER_SIZE - 1] = '\0';
  Message snd;
  snd.type = HANDSHAKE_MTYPE;
  strncpy(snd.text, buffer, TEXT_MAX);
  if (msgsnd(msqid, &snd, TEXT_MAX, 0) == -1) {
    perror("Failed to send message");
    return EXIT_FAILURE;
  }

  while (1) {
    Message rcv;
    ssize_t retval;
    do {
      retval = msgrcv(msqid, &rcv, TEXT_MAX, pid, IPC_NOWAIT);
      if (retval == -1 && errno != ENOMSG) {
        perror("Failed to read message");
        return EXIT_FAILURE;
      }

      if (retval > 0) {
        printf("%s\n", rcv.text);
      }
    } while (retval != -1 && buffer[0] == '\n');

    printf("> ");
    if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
      perror("Failed to read input");
      return EXIT_FAILURE;
    }

    if (buffer[0] == '\n') {
      continue;
    }

    buffer[strcspn(buffer, "\n")] = '\0';
    
    Message snd;
    snd.type = SERVER_MTYPE;
    snprintf(snd.text, TEXT_MAX,"%d:%s", pid, buffer);
    if (msgsnd(msqid, &snd, TEXT_MAX, 0) == -1) {
      perror("Failed to send message");
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}