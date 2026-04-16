#include <linux/limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <sys/types.h>

#include "shared.h"

#define CLIENTS_MAX 100

long clients[CLIENTS_MAX];
uint8_t client_count = 0;

int try_parse_message(Message rcv, char text[TEXT_MAX], long *id) {
  char buffer[TEXT_MAX];
  strncpy(buffer, rcv.text, TEXT_MAX);
  buffer[TEXT_MAX - 1] = '\0';
  char *token = strtok(buffer, ":");
  char *endptr;
  *id = strtol(token, &endptr, 10);
  if (*endptr != '\0') {
    printf("Failed to parse id.\n");
    return -1;
  }

  token = strtok(NULL, ":");
  if (token == NULL) {
    return 0;
  }

  strncpy(text, token, TEXT_MAX);
  text[TEXT_MAX - 1] = '\0';
  return 1;
}

int try_add_client(long id) {
  clients[client_count++] = id;
  printf("Client added: [%ld]\n", id);
  return 0;
}

void delete_client(long id) {
  int idx = -1;
  for (uint8_t i = 0; i < client_count; i++) {
    if (clients[i] == id) {
      idx = i;
      clients[i] = 0;
      client_count--;
      break;
    }
  }

  if (idx == -1) {
    return;
  }

  for (uint8_t i = idx; i < client_count; i++) {
    clients[i] = clients[i + 1];
  }
}

int broadcast_message(int msqid, long client_id, char text[TEXT_MAX]) {
  printf("Sending to...\n");
  for (uint8_t i = 0; i < client_count; i++) {
    if (clients[i] == client_id) {
      continue;
    }

    Message msg;
    msg.type = clients[i];
    strncpy(msg.text, text, TEXT_MAX);
    printf("msg: %ld %s", msg.type, msg.text);
    if (msgsnd(msqid, &msg, TEXT_MAX, 0) == -1) {
      perror("Failed to send message");
      printf("\n");
      return -1;
    }

    printf("Message sent for %ld\n", clients[i]);
  }

  return 0;
}

int listen(int msqid) {
  while (true) {
    Message rcv;
    if (msgrcv(msqid, &rcv, TEXT_MAX, -HANDSHAKE_MTYPE, 0) == -1) {
      perror("Failed to receive message");
      return -1;
    }

    printf("Received message %s.\n", rcv.text);
    char text[TEXT_MAX];
    long id;
    if (try_parse_message(rcv, text, &id) == -1) {
      continue;
    }

    if (rcv.type == HANDSHAKE_MTYPE) {
      try_add_client(id);
      fprintf(stderr, "Failed to add client.\n");
      continue;
    }

    if (strncmp(text, "shutdown", TEXT_MAX) == 0) {
      delete_client(id);
      printf("Client %ld disconnected", id);
      continue;
    }

    if (broadcast_message(msqid, id, rcv.text) == -1) {
      perror("Failed to broadcast message");
      return -1;
    }
  }
}

int msqid_cached = -1;
void clear(int sig) {
  if (msqid_cached != -1) {
    if (msgctl(msqid_cached, IPC_RMID, NULL) == -1) {
      perror("Failed to remove queue");
    }
  }
}

int main() {
  struct sigaction sa_int = {.sa_handler = clear, .sa_flags = SA_RESTART};
  if (sigaction(SIGINT, &sa_int, NULL) == -1) {
    perror("Failed to register SIGINT handler");
    return 1;
  }

  printf("========== SERVER ==========\n");
  int msqid;
  key_t key = 123123;
  msqid = msgget(key, IPC_CREAT | 0600);
  if (msqid == -1) {
    perror("Failed to get system v message queue");
    return EXIT_FAILURE;
  }

  listen(msqid);

  if (msgctl(msqid, IPC_RMID, NULL) == -1) {
    perror("Failed to remove queue");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}