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

int run(int sockfd, struct sockaddr_in listen_addr) {
  struct sockaddr_in dest_addr = {0};
  dest_addr.sin_family = AF_INET;

  while (running) {
    uint8_t buffer[D_BUFFER_SIZE] = {0};
    int bytes_read = read(sockfd, buffer, sizeof(buffer));
    if (bytes_read == -1) {
      perror("Failed to read from socket");
      return -1;
    }

    struct ip ip_header;
    struct udphdr udp_header;

    memcpy(&ip_header, buffer, sizeof(struct ip));
    uint16_t offset = ip_header.ip_hl * 4;
    memcpy(&udp_header, buffer + offset, sizeof(struct udphdr));

    if (udp_header.dest != listen_addr.sin_port) {
      continue;
    }

    printf("Received request \n");
    printf("From: %s:%d\n", inet_ntoa(ip_header.ip_src),
           ntohs(udp_header.source));
    printf("To: %s:%d\n", inet_ntoa(ip_header.ip_dst), ntohs(udp_header.dest));
    printf("Size: %d\n", bytes_read);
    printf("Message: %s\n",
           buffer + ip_header.ip_hl * 4 + sizeof(struct udphdr));

    uint8_t response_buffer[D_BUFFER_SIZE] = {0};

    struct udphdr response_udp_header;
    response_udp_header.dest = udp_header.source;
    response_udp_header.source = listen_addr.sin_port;
    response_udp_header.len = htons(ntohs(udp_header.len) + 16);
    response_udp_header.check = 0;

    memcpy(response_buffer, &response_udp_header, sizeof(response_udp_header));
    snprintf((char *)(response_buffer + 8), D_BUFFER_SIZE - 20, "%s %d",
             buffer + ip_header.ip_hl * 4 + sizeof(udp_header), 1235);

    dest_addr.sin_addr = ip_header.ip_src;
    int bytes_sent = sendto(sockfd, response_buffer, D_BUFFER_SIZE, 0,
                            (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (bytes_sent == -1) {
      perror("Failed to write to socket");
      return -1;
    }

    printf("Response sent: %s\n", response_buffer + sizeof(struct udphdr));
  }

  return 0;
}

int run_wrapper(struct sockaddr_in listen_addr) {
  int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
  if (sockfd == -1) {
    perror("Failed to open socket");
    return -1;
  }

  printf("Socket opened\n");
  int status = run(sockfd, listen_addr);

  if (sockfd != -1) {
    close(sockfd);
  }

  return status;
}

int main(int argc, char **argv) {
  if (argc == 1) {
    return EXIT_FAILURE;
  }
  struct sockaddr_in listen_addr = {0};
  listen_addr.sin_family = AF_INET;

  int c = 0;
  char *endptr;
  while ((c = getopt(argc, argv, "a:p:")) != -1) {
    switch (c) {
    case 'a':
      if (inet_aton(optarg, &listen_addr.sin_addr) == -1) {
        fprintf(stderr, "Invalid ip format\n");
        return EXIT_FAILURE;
      }
      break;
    case 'p':
      listen_addr.sin_port = htons(strtol(optarg, &endptr, 10));
      if (*endptr != '\0') {
        fprintf(stderr, "Invalid port format\n");
        return EXIT_FAILURE;
      }
      break;
    default:
      fprintf(stderr, "Usage: app -a <listen address> -p <listen port>\n");
      return EXIT_FAILURE;
    }
  }

  printf("================ SERVER ================\n");
  printf("Listening: %s:%d\n", inet_ntoa(listen_addr.sin_addr),
         ntohs(listen_addr.sin_port));

  int status = run_wrapper(listen_addr);

  return status == -1 ? EXIT_FAILURE : EXIT_SUCCESS;
}