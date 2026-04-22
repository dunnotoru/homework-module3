#include <arpa/inet.h>
#include <bits/getopt_core.h>
#include <bits/pthreadtypes.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define D_PORT 65525
#define D_BUFFER_SIZE 1024

struct writer_thread_args {
  int sockfd;
};

void *recv_thread_func(void *arg) {
  struct writer_thread_args *casted_arg = arg;
  int sockfd = casted_arg->sockfd;
  int running = 1;
  while (running) {
    char buffer[D_BUFFER_SIZE] = {0};
    int bytes_read = recvfrom(sockfd, buffer, D_BUFFER_SIZE, 0, NULL, NULL);
    if (bytes_read == -1) {
      perror("Zero byres read from socket");
      pthread_exit(NULL);
    }

    buffer[bytes_read] = '\0';
    if (strcmp(buffer, "disconnect") == 0) {
      running = 0;
    }

    printf("\033[A");
    printf("\r\033[K");
    printf("# %s\n> ", buffer);
    fflush(stdout);
  }

  pthread_exit(NULL);
  return NULL;
}

int chat_loop(int sockfd, struct sockaddr_in send_addr) {
  struct writer_thread_args thread_arg = {
      .sockfd = sockfd,
  };

  pthread_t recv_thread;
  pthread_create(&recv_thread, NULL, recv_thread_func, &thread_arg);

  struct sockaddr *send_ptr = (struct sockaddr *)&send_addr;

  int running = 1;
  while (running) {
    char buffer[D_BUFFER_SIZE] = {0};
    printf("> ");
    if (fgets(buffer, D_BUFFER_SIZE, stdin) == NULL) {
      perror("Failed to read input");
      pthread_exit(NULL);
    }

    buffer[strcspn(buffer, "\n")] = '\0';
    if (strcmp(buffer, "disconnect") == 0) {
      running = 0;
    }

    int bytes_sent =
        sendto(sockfd, buffer, strlen(buffer), 0, send_ptr, sizeof(send_addr));
    if (bytes_sent == -1) {
      perror("Zero bytes sent to socket");
      pthread_exit(NULL);
    }
  }

  pthread_cancel(recv_thread);
  pthread_join(recv_thread, NULL);

  return 0;
}

int server_listen_connection(int sockfd, struct sockaddr_in listen_addr) {
  if (bind(sockfd, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) ==
      -1) {
    perror("Failed to bind address");
    return -1;
  }

  socklen_t size = sizeof(listen_addr);
  getsockname(sockfd, (struct sockaddr *)&listen_addr, &size);

  printf("Listening address %s:%d\n", inet_ntoa(listen_addr.sin_addr),
         ntohs(listen_addr.sin_port));

  char buffer[D_BUFFER_SIZE] = {0};
  struct sockaddr_in client;
  socklen_t client_size = sizeof(client);

  printf("Waiting for connection...\n");

  int bytes_read = recvfrom(sockfd, buffer, D_BUFFER_SIZE, 0,
                            (struct sockaddr *)&client, &client_size);
  if (bytes_read == -1) {
    perror("Zero byres read from socket");
    return -1;
  }

  printf("Client connected: %s:%d\n", inet_ntoa(client.sin_addr),
         client.sin_port);

  return chat_loop(sockfd, client);
}

int server_start(struct sockaddr_in addr) {
  int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    perror("Failed to create socket");
    return -1;
  }

  int status = server_listen_connection(sockfd, addr);

  if (close(sockfd) == -1) {
    perror("Failed to close socket");
    status = -1;
  }

  return status;
}

int client_connect(int sockfd, struct sockaddr_in send_addr) {
  struct sockaddr_in listen_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(0),
      .sin_addr.s_addr = INADDR_ANY,
  };

  if (bind(sockfd, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) ==
      -1) {
    perror("Failed to bind address");
    return -1;
  }

  printf("Listening address %s:%d\n", inet_ntoa(listen_addr.sin_addr),
         ntohs(listen_addr.sin_port));
  printf("Sending to address %s:%d\n", inet_ntoa(send_addr.sin_addr),
         ntohs(send_addr.sin_port));

  struct writer_thread_args thread_arg = {
      .sockfd = sockfd,
  };

  return chat_loop(sockfd, send_addr);

  return 0;
}

int client_start(struct sockaddr_in addr) {
  int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    perror("Failed to create socket");
    return -1;
  }

  int status = client_connect(sockfd, addr);

  if (close(sockfd) == -1) {
    perror("Failed to close socket");
    status = -1;
  }

  return status;
}

void print_usage_error(int argc, char **argv) {
  fprintf(stderr, "Usage: %s <mode> [options]\n", argv[0]);
  fprintf(stderr, "Mode:\n");
  fprintf(stderr, "  -s\tStart server\n");
  fprintf(stderr, "  -c\tStart client (requires -a key)\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -a\tUse address to listen/connect in x.x.x.x format\n");
  fprintf(stderr, "  -p\tUse port (required for client)\n");
}

int main(int argc, char **argv) {
  char c = getopt(argc, argv, "sc");
  if (c == -1 || c == '?') {
    print_usage_error(argc, argv);
    return EXIT_FAILURE;
  }

  bool is_server = c == 's' ? true : false;
  char ip_str[16] = {0};
  char port_str[16] = {0};

  struct sockaddr_in addr = {
      addr.sin_family = AF_INET,
      addr.sin_addr.s_addr = INADDR_ANY,
      addr.sin_port = htons(0),
  };

  do {
    c = getopt(argc, argv, "a:p:");
    if (c == 'a') {
      if (inet_aton(optarg, &addr.sin_addr) == -1) {
        fprintf(stderr, "Incorrect ip address\n");
        return EXIT_FAILURE;
      }
    }

    if (c == 'p') {
      char buffer[16];
      strncpy(buffer, optarg, 16);
      char *endptr;
      int port = strtol(buffer, &endptr, 10);
      if (*endptr != '\0' || port < 0 || port > UINT16_MAX) {
        fprintf(stderr, "Incorrect port\n");
        return EXIT_FAILURE;
      }

      addr.sin_port = htons(port);
    }
  } while (c != -1);

  if (is_server == 0 && addr.sin_port == 0) {
    fprintf(stderr, "Port required for client to connect\n");
    return EXIT_FAILURE;
  }

  int status = is_server ? server_start(addr) : client_start(addr);
  return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
