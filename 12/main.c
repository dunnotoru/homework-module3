#include <arpa/inet.h>
#include <bits/getopt_core.h>
#include <bits/pthreadtypes.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdbool.h>
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

    printf("%s\n", buffer);
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

int server_listen_connection(int sockfd, struct in_addr addr) {
  struct sockaddr_in server = {
      .sin_family = AF_INET,
      .sin_port = htons(D_PORT),
      .sin_addr.s_addr = INADDR_ANY,
  };

  if (addr.s_addr == 0) {
    server.sin_addr = addr;
    // TODO: change to ptr because -1 or 0 are actual addresses
  }

  if (bind(sockfd, (struct sockaddr *)&server, sizeof(server)) == -1) {
    perror("Failed to bind address");
    return -1;
  }

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

int server_start(struct in_addr addr) {
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

int client_connect(int sockfd, struct in_addr addr) {
  struct sockaddr_in client = {
      .sin_family = AF_INET,
      .sin_port = htons(0),
      .sin_addr.s_addr = INADDR_ANY,
  };

  if (bind(sockfd, (struct sockaddr *)&client, sizeof(client)) == -1) {
    perror("Failed to bind address");
    return -1;
  }

  struct sockaddr_in server = {
      .sin_family = AF_INET,
      .sin_port = htons(D_PORT),
      .sin_addr = addr,
  };

  struct writer_thread_args thread_arg = {
      .sockfd = sockfd,
  };

  return chat_loop(sockfd, server);

  return 0;
}

int client_start(struct in_addr addr) {
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
  fprintf(stderr, "  -a\tAddress to listen/connect in x.x.x.x:x format\n");
}

int main(int argc, char **argv) {
  char c = getopt(argc, argv, "sc");
  if (c == -1 || c == '?') {
    print_usage_error(argc, argv);
    return EXIT_FAILURE;
  }

  bool is_server = c == 's' ? true : false;

  struct in_addr addr = {0};
  c = getopt(argc, argv, "a:");
  if (c == -1 && is_server) {
    addr.s_addr = INADDR_ANY;
  } else if (c == 'a') {
    if (inet_aton(optarg, &addr) == 0) {
      fprintf(stderr, "Incorrect ip format\n");
      return EXIT_FAILURE;
    }
  } else {
    print_usage_error(argc, argv);
    return EXIT_FAILURE;
  }

  int status = is_server ? server_start(addr) : client_start(addr);
  if (status == -1) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}