#include <arpa/inet.h>
#include <bits/getopt_core.h>
#include <ctype.h>
#include <endian.h>
#include <fcntl.h>
#include <getopt.h>
#include <iso646.h>
#include <linux/limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define P_SEPARATOR                                                            \
  "==========================================================================" \
  "================\n"

static struct option long_options[] = {
    {"from", required_argument, NULL, 0},
    {"to", required_argument, NULL, 0},
    {NULL, 0, NULL, 0},
};

void usage_error(const char *name) {
  fprintf(stderr, "Usage: %s --from <source ip> --to <destination ip>\n", name);
  exit(EXIT_FAILURE);
}

int run(int fd, int sockfd, struct in_addr required_src_ip,
        struct in_addr required_dest_ip) {
  while (1) {
    uint8_t buffer[1024] = {0};
    struct sockaddr_in from = {0};
    socklen_t fromlen = sizeof(from);

    ssize_t bytes_read = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                  (struct sockaddr *)&from, &fromlen);
    if (bytes_read < 1) {
      break;
    }

    struct in_addr src_ip = {*(in_addr_t *)(buffer + 12)};
    struct in_addr dest_ip = {*(in_addr_t *)(buffer + 16)};

    if ((src_ip.s_addr != required_src_ip.s_addr && required_src_ip.s_addr != 0) ||
        (dest_ip.s_addr != required_dest_ip.s_addr && required_dest_ip.s_addr != 0)) {
      // printf("%b ? %b\n", required_src_ip.s_addr, src_ip.s_addr);
      // printf("%b ? %b\n", required_dest_ip.s_addr, dest_ip.s_addr);
      continue;
    }
    
    if (write(fd, buffer, bytes_read) == -1) {
      return -1;
    }
    
    buffer[bytes_read] = '\0';
    printf(P_SEPARATOR);
    printf("Bytes read %zu\n\n", bytes_read);
    printf("########  ");
    for (ssize_t i = 0; i < 16; i++) {
      printf("%02zx ", i);
    }

    printf(" Decoded text \n\n");

    for (ssize_t i = 0; i < bytes_read / 16 + 1; i++) {
      printf("%08zx: ", i * 16);

      for (ssize_t j = 0; j < 16; j++) {
        printf("%02x ", buffer[i * 16 + j]);
      }

      printf(" ");

      for (ssize_t j = 0; j < 16; j++) {
        uint8_t c = buffer[i * 16 + j];
        printf("%c ", isprint(c) ? c : '.');
      }

      printf("\n");
    }

    printf("\n");

    printf("IP header\n");
    printf("Source IP: %s\n", inet_ntoa(src_ip));
    printf("Dest IP:   %s\n", inet_ntoa(dest_ip));

    uint8_t udp_offset = 20;
    uint16_t source_port = *(uint16_t *)(buffer + udp_offset);
    uint16_t dest_port = *(uint16_t *)(buffer + udp_offset + 2);
    uint16_t length = *(uint16_t *)(buffer + udp_offset + 4);
    printf("UDP header\n");
    printf("Source port: %d\n", ntohs(source_port));
    printf("Dest port: %d\n", ntohs(dest_port));
    printf("Length: %d\n", ntohs(length));
    printf("Data: ");
    for (ssize_t byte_id = udp_offset + 8;
         byte_id < length && byte_id < bytes_read; byte_id++) {
      printf("%c", buffer[byte_id]);
    }

    printf("\n");

    printf(P_SEPARATOR);
  }

  return 0;
}

int run_wrapper(struct in_addr required_src_ip,
                struct in_addr required_dest_ip) {
  int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
  if (sockfd == -1) {
    perror("Failed to open socket");
  }

  int fd = open(".dump", O_CREAT | O_WRONLY, 0644);
  if (fd == -1) {
    perror("Failed to open file");
  }

  int status = 0;
  if (fd != -1 && sockfd != 1) {
    status = run(fd, sockfd, required_src_ip, required_dest_ip);
  }

  if (fd != 1) {
    close(fd);
  }

  if (sockfd != 1) {
    close(sockfd);
  }

  return status;
}

int main(int argc, char **argv) {
  struct in_addr required_src_ip = {0};
  struct in_addr required_dest_ip = {0};

  if (argc != 1) {
    int opt_idx = 0;
    int c = getopt_long(argc, argv, "", long_options, &opt_idx);
    if (c == -1 || optarg == NULL) {
      return EXIT_FAILURE;
    }

    if (inet_aton(optarg, &required_src_ip) == 0) {
      usage_error(argv[0]);
    }

    c = getopt_long(argc, argv, "", long_options, &opt_idx);
    if (c == -1 || optarg == NULL) {
      return EXIT_FAILURE;
    }

    if (inet_aton(optarg, &required_dest_ip) == 0) {
      usage_error(argv[0]);
    }
  }

  printf("Listening...\n");
  printf("from:     %s\n", inet_ntoa(required_src_ip));
  printf("To:       %s\n", inet_ntoa(required_dest_ip));

  int status = run_wrapper(required_src_ip, required_dest_ip);

  return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}