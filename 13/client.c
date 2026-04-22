#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

void error(const char *msg) {
  perror(msg);
  exit(0);
}

void do_math(int sockfd) {
  if (write(sockfd, "1", sizeof("1")) == -1) {
    error("Failed to write to socket");
  }

  char buff[1024];
  ssize_t bytes_read = 1;
  while (bytes_read > 0) {
    bytes_read = read(sockfd, buff, sizeof(buff) - 1);
    if (bytes_read == -1) {
      error("Failed to read from socket");
    }

    // ставим завершающий ноль в конце строки
    buff[bytes_read] = '\0';

    // выводим на экран
    printf("S=>C:%s", buff);

    // читаем пользовательский ввод с клавиатуры
    printf("S<=C:");
    if (fgets(buff, sizeof(buff) - 1, stdin) == NULL) {
      close(sockfd);
      error("Failed to read input");
    }

    if (!strcmp(buff, "quit\n")) {
      printf("Exit...");
      close(sockfd);
      return;
    }

    if (write(sockfd, buff, strlen(buff)) == -1) {
      error("Failed to write to socket");
    }
  }

  printf("Recv error \n");
  close(sockfd);
}

void send_file(int sockfd) {
  char buffer[1024] = {0};

  if (write(sockfd, "2", sizeof("2")) == -1) {
    close(sockfd);
    error("Failed to write to socket");
  }

  printf("File to send: ");
  char filename[256] = {0};
  if (fgets(filename, sizeof(filename), stdin) == NULL) {
    close(sockfd);
    error("Failed to read input");
  }

  filename[strcspn(filename, "\n")] = '\0';

  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    close(sockfd);
    error("Failed to open file");
  }

  struct stat st;
  fstat(fd, &st);
  snprintf(buffer, sizeof(buffer), "%s:%zu", filename, st.st_size);
  printf("YO MISTER WHITE %s\n", buffer);

  if (write(sockfd, buffer, sizeof(buffer)) == -1) {
    close(fd);
    close(sockfd);
    error("Failed to write to socket");
  }

  ssize_t bytes_read = 0;
  do {
    bytes_read = read(fd, buffer, sizeof(buffer));
    if (bytes_read == -1) {
      perror("Failed to read socket");
      break;
    }

    ssize_t bytes_written = write(sockfd, buffer, bytes_read);
    if (bytes_read == -1) {
      perror("Failed to write to scoket");
      break;
    }
    
    if (bytes_written == 0) {
      perror("Server closed connection");
      break;
    }

    printf("SEND\n");
  } while (bytes_read > 0);

  close(sockfd);
  close(fd);
}

int main(int argc, char *argv[]) {
  int sockfd, portno;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  char buff[1024];
  printf("TCP DEMO CLIENT\n");

  if (argc < 3) {
    fprintf(stderr, "usage %s hostname port\n", argv[0]);
    exit(0);
  }

  portno = atoi(argv[2]);

  // Шаг 1 - создание сокета
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    error("ERROR opening socket");
  }

  // извлечение хоста
  server = gethostbyname(argv[1]);
  if (server == NULL) {
    fprintf(stderr, "ERROR, no such host\n");
    exit(0);
  }
  // заполенние структуры serv_addr
  bzero((char *)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
  serv_addr.sin_port = htons(portno);

  // Шаг 2 - установка соединения
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    error("ERROR connecting");
  }

#define HELLO_MESSAGE                                                          \
  "\n[1] Do math\r\n"                                                          \
  "[2] Transmit file\r\n"

  printf(HELLO_MESSAGE);

  printf("> ");
  if (fgets(buff, sizeof(buff) - 1, stdin) == NULL) {
    error("Failed to read input");
  }

  if (buff[0] == '1') {
    do_math(sockfd);
  } else if (buff[0] == '2') {
    send_file(sockfd);
  }

  return EXIT_SUCCESS;
}
