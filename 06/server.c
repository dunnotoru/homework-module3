#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <sys/types.h>

#define MAX_TEXT 1024
#define SERVER_MTYPE 10

typedef struct {
  long type;
  char text[MAX_TEXT];
} Message;

int listen(int msqid) {
  while (true) {
    Message rcv;
    if (msgrcv(msqid, &rcv, MAX_TEXT, SERVER_MTYPE, 0) == -1) {
      perror("Failed to receive message");
      return -1;
    }

    if (strcmp())
  }
}

int main(int argc, char **argv) {
  int msqid;
  key_t key = 123123;
  msqid = msgget(key, IPC_CREAT | 0600);


  if (msqid == -1) {
    perror("Failed to get system v message queue");
    return 1;
  }

  Message msg = {.type = 10, .text = "skibidi"};
  if (msgsnd(msqid, &msg, MAX_TEXT, 0) == -1) {
    perror("Failed to send message");
    return 1;
  }

  Message rcv;
  if (msgrcv(msqid, &rcv, MAX_TEXT, 10, 0) == -1) {
    perror("Failed to receive message");
    return 1;
  }

  printf("Received message: %s\n", rcv.text);

  if (msgctl(msqid, IPC_RMID, NULL) == -1) {
    perror("Failed to remove queue");
    return 1;
  }
  return 0;
}