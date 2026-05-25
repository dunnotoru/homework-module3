/* Пример простого TCP сервера
   Порт является аргументом, для запуска сервера неободимо ввести:
   # ./[имя_скомпилированного_файла] [номер порта]
   # ./server 57123
*/
#include <arpa/inet.h>
#include <complex.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define str1 "Enter 1 parameter\r\n"
#define str2 "Enter 2 parameter\r\n"
#define str3 "Enter op (+,-,*,/)\r\n"
#define NOT_FOUND "No such operation available.\r\n"

typedef enum {
  STATUS_OK = 0,
  STATUS_FATAL,
  STATUS_FIN,
} StatusCode;

typedef enum {
  ST_UNASSIGNED = 0,

  MATH_WAIT_A,
  MATH_WAIT_OP,
  MATH_WAIT_B,
  MATH_DONE,

  FILE_WAIT_NAME,
  FILE_RECEIVE,
  FILE_DONE,

} ClientState;

typedef int (*operation)(int, int);

typedef struct {
  int sockfd;
  ClientState state;
  int math_a;
  int math_b;
  operation math_op;

  char filename[256];
  size_t file_size;
  size_t file_received;
  int fd;
  char file_buffer[1024];
} Client;

Client clients[100] = {0};
int nclients = 0;

void add_client(int sockfd) {
  int idx = nclients;
  memset(&clients[idx], 0, sizeof(Client));
  clients[idx].sockfd = sockfd;
  nclients++;
}

int get_client_idx(int sockfd) {
  for (int i = 0; i < nclients; i++) {
    if (clients[i].sockfd == sockfd) {
      return i;
    }
  }

  return -1;
}

void remove_client(int sockfd) {
  int idx = get_client_idx(sockfd);
  memcpy(&clients[nclients - 1], &clients[idx], sizeof(Client));
  nclients--;
}

StatusCode dostuff(Client *);

void set_nonblock(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl(fd, F_SETFL, flags);
}

void printusers() {
  if (nclients) {
    printf("%d user on-line\n", nclients);
  } else {
    printf("No User on line\n");
  }
}

int sum(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }
int divide(int a, int b) { return a / b; }

int main(int argc, char *argv[]) {
  printf("TCP SERVER DEMO\n");

  int sockfd, newsockfd; // дескрипторы сокетов
  int portno;            // номер порта
  socklen_t clilen;      // размер адреса клиента типа socklen_t
  struct sockaddr_in serv_addr = {0},
                     cli_addr = {0}; // структура сокета сервера и клиента

  int epollfd = epoll_create1(0);
  if (epollfd == -1) {
    perror("Failed to create epoll");
    return EXIT_FAILURE;
  }

  if (argc < 2) {
    fprintf(stderr, "ERROR, no port provided\n");
    return EXIT_FAILURE;
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLET;
  ev.data.fd = sockfd;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev);

  if (sockfd < 0) {
    perror("Failed to open socket");
    return EXIT_FAILURE;
  }

  portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("Failed to bind socket");
    return EXIT_FAILURE;
  }

  listen(sockfd, 5);
  clilen = sizeof(cli_addr);

  struct epoll_event events[6];
  while (1) {
    int nfds = epoll_wait(epollfd, events, 6, -1);
    for (int i = 0; i < nfds; i++) {
      if (events[i].data.fd == sockfd) {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        ev.events = EPOLLIN;
        ev.data.fd = newsockfd;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, newsockfd, &ev);

        add_client(newsockfd);

        struct hostent *hst;
        hst = gethostbyaddr((void *)&cli_addr.sin_addr, 4, AF_INET);
        printf("+%s [%s] new connect!\n", hst ? hst->h_name : "Unknown host",
               inet_ntoa(cli_addr.sin_addr));
      } else {
        int idx = get_client_idx(events[i].data.fd);
        Client *client = &clients[idx];
        StatusCode status = dostuff(client);
        if (status == STATUS_FATAL || status == STATUS_FIN) {
          close(client->sockfd);
          if (client->fd != 0) {
            close(client->fd);
          }
          remove_client(client->sockfd);
        }
      }
    }

    printusers();
  }

  close(epollfd);
  close(sockfd);

  return 0;
}

operation operations[] = {sum, sub, mul, divide};

#define MESSAGE_SIZE 1024

StatusCode do_math(Client *client, char buffer[MESSAGE_SIZE]) {
  if (client->state == MATH_WAIT_A) {
    client->math_a = atoi(buffer);
    client->state = MATH_WAIT_OP;
    if (write(client->sockfd, str3, sizeof(str3)) == -1) {
      perror("Failed to write to socket");
      return STATUS_FATAL;
    }
    return STATUS_OK;
  }

  if (client->state == MATH_WAIT_OP) {
    int (*op)(int, int) = NULL;
    switch (buffer[0]) {
    case '+':
      op = operations[0];
      break;
    case '-':
      op = operations[1];
      break;
    case '*':
      op = operations[2];
      break;
    case '/':
      op = operations[3];
      break;
    default:
      fprintf(stderr, "ERROR invalid input.\n");
      return STATUS_FATAL;
    }

    client->math_op = op;
    client->state = MATH_WAIT_B;
    if (write(client->sockfd, str2, sizeof(str2)) == -1) {
      perror("Failed to write to socket");
      return STATUS_FATAL;
    }
    return STATUS_OK;
  }

  if (client->state == MATH_WAIT_B) {
    client->math_b = atoi(buffer);
    client->state = MATH_DONE;
  }

  if (client->math_op == operations[3] && client->math_b == 0) {
    snprintf(buffer, MESSAGE_SIZE, "error: zero division\n");
  } else {
    int result = client->math_op(client->math_a, client->math_b);
    snprintf(buffer, MESSAGE_SIZE, "%d\n", result);
  }

  if (write(client->sockfd, buffer, strlen(buffer)) == -1) {
    perror("Failed to write to socket");
    return STATUS_FATAL;
  }

  return STATUS_FIN;
}

StatusCode receive_file(Client *client, char buffer[MESSAGE_SIZE],
                        ssize_t bytes_read) {
  if (client->state == FILE_WAIT_NAME) {
    char *name_token = strtok(buffer, ":");
    char *size_token = strtok(NULL, ":");

    if (size_token == NULL) {
      fprintf(
          stderr,
          "Failed to parse params, invalid format (filename:size required)\n");
      return STATUS_FATAL;
    }

    char *name = NULL;
    char *next = strtok(name_token, "/");
    do {
      name = next;
      next = strtok(NULL, "/");
    } while (next != NULL);

    char *endptr;
    size_t size = strtol(size_token, &endptr, 10);
    if (*endptr != '\0') {
      fprintf(stderr, "Failed to parse params, invlid file size\n");
      return STATUS_FATAL;
    }

    if (mkdir("storage", 0755) == -1 && errno != EEXIST) {
      perror("Failed to create storage directory");
      return STATUS_FATAL;
    }

    char filename[256] = {0};
    snprintf(filename, sizeof(filename), "storage/%d-%s", getpid(), name);
    printf("Filename: %s\n", filename);

    int fd = open(filename, O_CREAT | O_WRONLY | O_EXCL, 0600);
    if (fd == -1) {
      perror("Failed to open file");
      return STATUS_FATAL;
    }

    strncpy(client->filename, filename, sizeof(client->filename));
    client->fd = fd;
    client->file_size = size;
    client->state = FILE_RECEIVE;
    return STATUS_OK;
  }

  if (client->state == FILE_RECEIVE) {
    printf("Receiving file with name %s and size %zu bytes\n", client->filename,
           client->file_size);
    if (write(client->fd, buffer, bytes_read) == -1) {
      perror("Failed to write to file");
      return STATUS_FATAL;
    }

    client->file_received += bytes_read;

    if (client->file_received >= client->file_size) {
      printf("File with name %s received", client->filename);
    }

    client->state = FILE_DONE;

    return STATUS_OK;
  }

  return STATUS_FIN;
}

StatusCode dostuff(Client *client) {
  char buffer[1024];
  ssize_t bytes_read = read(client->sockfd, buffer, sizeof(buffer));
  if (bytes_read == -1) {
    perror("Failed to read from socket");
    return STATUS_FATAL;
  }

  if (client->state == ST_UNASSIGNED) {
    switch (buffer[0]) {
    case '1':
      client->state = MATH_WAIT_A;
      if (write(client->sockfd, str1, sizeof(str1)) == -1) {
        perror("Failed to write to socket");
        return STATUS_FATAL;
      }
      return STATUS_OK;
    case '2':
      client->state = FILE_WAIT_NAME;
      return STATUS_OK;
    default:
      write(client->sockfd, NOT_FOUND, sizeof(NOT_FOUND));
      return STATUS_FATAL;
    }
  }

  switch (client->state) {
  case MATH_WAIT_A:
  case MATH_WAIT_B:
  case MATH_WAIT_OP:
  case MATH_DONE:
    return do_math(client, buffer);

  case FILE_WAIT_NAME:
  case FILE_RECEIVE:
    return receive_file(client, buffer, bytes_read);

  default:
    write(client->sockfd, NOT_FOUND, sizeof(NOT_FOUND));
    return STATUS_FATAL;
  }
}
