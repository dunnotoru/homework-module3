#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define D_BUFFER_SIZE 4096

int running = 1;

int run(int sockfd, struct sockaddr_in server_addr) {
  while (running) {
    uint8_t buffer[D_BUFFER_SIZE] = {0};

    char input[256] = {0};

    if (fgets(input, sizeof(input), stdin) == NULL) {
      perror("Failed to read input");
      return -1;
    }

    input[strcspn(input, "\n")] = '\0';

    struct udphdr request_udphdr = {0};
    request_udphdr.source = htons(55555);
    request_udphdr.dest = server_addr.sin_port;
    request_udphdr.len = htons(8 + (uint16_t)strlen(input));
    request_udphdr.check = 0;

    memcpy(buffer, &request_udphdr, sizeof(struct udphdr));
    strncpy((char *)(buffer + sizeof(struct udphdr)), input, D_BUFFER_SIZE - 8);

    int bytes_sent =
        sendto(sockfd, buffer, D_BUFFER_SIZE, 0,
               (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bytes_sent == -1) {
      perror("Failer to write to socket");
      return -1;
    }

    uint8_t response[D_BUFFER_SIZE] = {0};
    int bytes_read = -1;
    while (1) {
      memset(response, 0, D_BUFFER_SIZE);
      bytes_read = read(sockfd, response, sizeof(response));
      if (bytes_read == -1) {
        perror("Failed to read from socket");
        return -1;
      }

      struct ip resp_iphdr = {0};
      struct udphdr resp_udphdr = {0};

      memcpy(&resp_iphdr, response, sizeof(struct ip));
      memcpy(&resp_udphdr, response + resp_iphdr.ip_hl * 4,
             sizeof(struct udphdr));

      if (resp_iphdr.ip_src.s_addr == server_addr.sin_addr.s_addr &&
          resp_udphdr.dest == htons(55555)) {
        break;
      }

      // response[bytes_read - 1] = '\0';
    }

    printf("bytes_read:%d %s\n", bytes_read,
           response + sizeof(struct ip) + sizeof(struct udphdr));
  }
  return 0;
}

int run_wrapper(struct sockaddr_in server_addr) {
  int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
  if (sockfd == -1) {
    perror("Failed to open socket");
    return -1;
  }

  int status = run(sockfd, server_addr);

  if (sockfd != -1) {
    close(sockfd);
  }

  return status;
}

int main(int argc, char **argv) {
  struct sockaddr_in server_addr = {0};
  char c = 0;
  char *endptr;
  while ((c = getopt(argc, argv, "r:a:p:")) != -1) {
    switch (c) {
    case 'a':

      if (inet_aton(optarg, &server_addr.sin_addr) == -1) {
        fprintf(stderr, "Invalid ip format\n");
        return EXIT_FAILURE;
      }

      break;
    case 'p':
      server_addr.sin_port = htons(strtol(optarg, &endptr, 10));
      if (*endptr != '\0') {
        fprintf(stderr, "Invalid port format\n");
        return EXIT_FAILURE;
      }
      break;
    default:
      fprintf(stderr, "Usage: app -a <server address> -p <server port>\n");
      return EXIT_FAILURE;
    }
  }

  int status = run_wrapper(server_addr);

  return EXIT_SUCCESS;
}