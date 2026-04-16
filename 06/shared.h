#ifndef SHARED_H
#define SHARED_H

#define TEXT_MAX 1024
#define SERVER_MTYPE 10
#define HANDSHAKE_MTYPE 11
#define SHUTDOWN_CMD "shutdown"
#define NAME_SIZE 32
#define BUFFER_SIZE TEXT_MAX - NAME_SIZE - 3

typedef struct {
  long type;
  char text[TEXT_MAX];
} Message;

#endif