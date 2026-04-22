/* Пример простого TCP сервера
   Порт является аргументом, для запуска сервера неободимо ввести:
   # ./[имя_скомпилированного_файла] [номер порта]
   # ./server 57123
*/
#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <complex.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// прототип функции, обслуживающей
// подключившихся пользователей
void dostuff(int);

// Функция обработки ошибок
void error(const char *msg) {
  perror(msg);
  exit(1);
}

// глобальная переменная – количество
// активных пользователей

sem_t *nclients_sem = SEM_FAILED;
int nclients = 0;

// макрос для печати количества активных
// пользователей
void printusers() {
  sem_wait(nclients_sem);
  if (nclients) {
    printf("%d user on-line\n", nclients);
  } else {
    printf("No User on line\n");
  }
  sem_post(nclients_sem);
}

int sum(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }
int divide(int a, int b) { return a / b; }

int main(int argc, char *argv[]) {
  printf("TCP SERVER DEMO\n");

  char sem_name[32];
  snprintf(sem_name, sizeof(sem_name), "/server_%d_sem", getpid());
  nclients_sem = sem_open(sem_name, O_CREAT | O_EXCL, 0600, 1);
  if (nclients_sem == SEM_FAILED) {
    fprintf(stderr, "%s\n", sem_name);
    perror("Failed to open semaphore");
    return EXIT_FAILURE;
  }

  if (sem_unlink(sem_name) == -1) {
    perror("Failed to unink semaphore");
    return EXIT_FAILURE;
  }

  int sockfd, newsockfd; // дескрипторы сокетов
  int portno;            // номер порта
  int pid;               // id номер потока
  socklen_t clilen;      // размер адреса клиента типа socklen_t
  struct sockaddr_in serv_addr, cli_addr; // структура сокета сервера и клиента

  // ошибка в случае если мы не указали порт
  if (argc < 2) {
    fprintf(stderr, "ERROR, no port provided\n");
    exit(1);
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    error("ERROR opening socket");
  }

  bzero((char *)&serv_addr, sizeof(serv_addr));
  portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    error("ERROR on binding");
  }

  listen(sockfd, 5);
  clilen = sizeof(cli_addr);

  while (1) {
    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (newsockfd < 0) {
      error("ERROR on accept");
    }

    // увеличиваем счетчик
    // подключившихся клиентов
    sem_wait(nclients_sem);
    nclients++;
    sem_post(nclients_sem);

    // вывод сведений о клиенте
    struct hostent *hst;
    hst = gethostbyaddr((void *)&cli_addr.sin_addr, 4, AF_INET);
    printf("+%s [%s] new connect!\n", hst ? hst->h_name : "Unknown host",
           inet_ntoa(cli_addr.sin_addr));

    printusers();

    pid = fork();
    if (pid < 0) {
      error("ERROR on fork");
    }

    if (pid == 0) {
      close(sockfd);
      dostuff(newsockfd);
      _exit(0);
    } else {
      close(newsockfd);
    }
  }

  close(sockfd);

  return 0;
}

int (*operations[])(int, int) = {sum, sub, mul, divide};

void do_math(int sockfd) {
#define str1 "Enter 1 parameter\r\n"
#define str2 "Enter 2 parameter\r\n"
#define str3 "Enter op (+,-,*,/)\r\n"

  // int n,a,b;
  int a, b; // переменные для myfunc
  char buff[1024];

  if (write(sockfd, str1, sizeof(str1)) == -1) {
    error("Failed to write to socket");
  }

  if (read(sockfd, buff, sizeof(buff)) == -1) {
    error("Failed to read from socket");
  }

  a = atoi(buff);

  if (write(sockfd, str3, sizeof(str3)) == -1) {
    error("Failed to write to socket");
  }

  if (read(sockfd, buff, sizeof(buff)) == -1) {
    error("Failed to read from socket");
  }

  int (*op)(int, int) = NULL;
  switch (buff[0]) {
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
    fprintf(stderr, "ERROR invalid input");
    _exit(1);
    break;
  }

  if (write(sockfd, str2, sizeof(str2)) == -1) {
    error("Failed to write to socket");
  }

  if (read(sockfd, buff, sizeof(buff)) == -1) {
    error("Failed to read from socket");
  }

  b = atoi(buff);

  int result = op(a, b); // вызов пользовательской функции

  snprintf(buff, strlen(buff), "%d", result);
  buff[strlen(buff)] = '\n';

  if (write(sockfd, buff, sizeof(buff)) == -1) {
    error("Failed to write to socket");
  }

  close(sockfd);

  sem_wait(nclients_sem);
  nclients--;
  sem_post(nclients_sem);

  printf("-disconnect\n");

  printusers();
}

void receive_file(int sockfd) {
  char buffer[1024];

  if (read(sockfd, buffer, sizeof(buffer)) == -1) {
    close(sockfd);
    error("Failed to read from socket");
  }

  char *name_token = strtok(buffer, ":");
  char *size_token = strtok(NULL, ":");

  if (size_token == NULL) {
    fprintf(stderr,
            "Failed to params, invalid format (filename:size required)\n");
    exit(EXIT_FAILURE);
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
    error("Invalid size");
  }
  
  if (mkdir("storage", 0755) == -1 && errno != EEXIST) {
    error("Failed to create storage directory");
  }
  
  char filename[256] = {0};
  snprintf(filename, sizeof(filename), "storage/%d-%s",getpid(), name);
  printf("Filename: %s\n", filename);

  int fd = open(filename, O_CREAT | O_WRONLY | O_EXCL, 0600);
  if (fd == -1) {
    close(sockfd);
    error("Failed to open file");
  }

  printf("Receiving file with name %s and size %zu bytes\n", filename, size);
  size_t total_read = 0;
  ssize_t bytes_read = 0;
  while (size > total_read) {
    bytes_read = read(sockfd, buffer, sizeof(buffer));
    if (bytes_read == -1) {
      close(fd);
      close(sockfd);
      error("Failed to read from socket");
    }

    if (bytes_read == 0) {
      break;
    }

    if (write(fd, buffer, bytes_read) == -1) {
      error("Failed to write file");
    }

    total_read += bytes_read;

    printf("loading... [%.0f%%]\n", 100 * (float)total_read / size);
  }

  printf("loading... [100%%]\n");
  printf("File with name %s received", filename);
  close(sockfd);
  close(fd);
}

void dostuff(int sockfd) {

#define NOT_FOUND "No such operation available.\r\n"

  char buff[20 * 1024];
  if (read(sockfd, buff, sizeof(buff)) == -1) {
    error("Failed to read from socket");
  }

  if (buff[0] == '1') {
    do_math(sockfd);
  } else if (buff[0] == '2') {
    receive_file(sockfd);
  } else {
    write(sockfd, NOT_FOUND, sizeof(NOT_FOUND));
  }
}
