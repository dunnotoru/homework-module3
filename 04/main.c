#include <bits/posix1_lim.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string.h>

#define BUFFER_SIZE 64
#define MAX_COMMAND_SIZE 32000
#define MAX_ARGV_SIZE 8192
#define DEBUG
#define SPACE_DELIM " \t\f\n\r\v"
#define PIPE_DELIM "|"
#define REDIR_DELIM "<>"

void parse_by_pipe(char cmd_buffer[MAX_COMMAND_SIZE]) {
  char *commands[MAX_ARGV_SIZE];
  char *token = strtok(cmd_buffer, PIPE_DELIM);
  size_t i = 0;
  do {
    commands[i++] = token;
    token = strtok(NULL, PIPE_DELIM);
  } while (token != NULL && i < MAX_ARGV_SIZE);

  
}

bool run(char cmd_buffer[MAX_COMMAND_SIZE]) {
  // TODO: fix cringe
  char *argv[MAX_ARGV_SIZE];
  char *token = NULL;
  size_t i = 0;
  token = strtok(cmd_buffer, SPACE_DELIM);
  do {
    argv[i++] = token;
    token = strtok(NULL, SPACE_DELIM);
  } while (token != NULL && i < MAX_ARGV_SIZE);

  if (argv[0] == NULL) {
    return false;
  }

  pid_t pid = fork();
  switch (pid) {
  case -1:
    perror("Fork failed.");
    return false;
  case 0:
    execvp(argv[0], argv);
    break;
  default:
    return true;
  }

  return false;
}

int main() {

  char hostname[HOST_NAME_MAX];
  gethostname(hostname, sizeof(hostname));
  const char *login = getlogin();
  while (true) {
    printf("[%s@%s] $ ", login, hostname);
    char cmd_buffer[MAX_COMMAND_SIZE];

    if (fgets(cmd_buffer, MAX_COMMAND_SIZE, stdin) == NULL) {
      exit(EXIT_FAILURE);
    }

    if (cmd_buffer[0] == '\n') {
      continue;
    }

    cmd_buffer[strcspn(cmd_buffer, "\r\n")] = '\0';

    if (strcmp(cmd_buffer, "exit") == 0) {
      exit(EXIT_SUCCESS);
    }

    int retval;
    if (run(cmd_buffer)) {
      pid_t pid = wait(&retval);
      // printf("CHILD PROCESS [%d] EXEC EXITET WITH STATUS - %d\n", pid,
      //        WEXITSTATUS(retval));
    } else {
      printf("dsh: command not found: %s\n", cmd_buffer);
    }
  }
}