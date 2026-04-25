#include <arpa/inet.h>
#include <bits/types/siginfo_t.h>
#include <ctype.h>
#include <endian.h>
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

int main(int argc, char **argv) {
  int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
  if (sockfd == -1) {
    perror("Failed to open socket");
    return EXIT_FAILURE;
  }

  while (1) {
    uint8_t buffer[1024] = {0};
    struct sockaddr_in from = {0};
    socklen_t fromlen = sizeof(from);

    ssize_t bytes_read = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                  (struct sockaddr *)&from, &fromlen);
    if (bytes_read < 1) {
      break;
    }

    buffer[bytes_read] = '\0';
    printf(P_SEPARATOR);
    printf("Bytes read %zu\n\n", bytes_read);
    printf("Ip: %s\n", inet_ntoa(from.sin_addr));
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

    uint8_t ihl = *buffer;
    uint32_t source = *(uint32_t *)(buffer + 12);
    uint32_t dest = *(uint32_t *)(buffer + 16);
    struct in_addr addr = {source};

    printf("IP header\n");
    printf("Source IP: %s\n", inet_ntoa(addr));
    addr.s_addr = dest;
    printf("Dest IP:   %s\n", inet_ntoa(addr));

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

  close(sockfd);

  return EXIT_SUCCESS;
}