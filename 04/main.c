#include <fcntl.h>
#include <limits.h>
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
#define MAX_COMMANDS 8192
#define SPACE_DELIM " \t\f\n\r\v"
#define PIPE_DELIM "|"
#define REDIR_DELIM "<>"

int parse_command(char command[MAX_COMMAND_SIZE], char *argv[MAX_ARGV_SIZE],
                  int *argc, char *input_filename, char *output_filename,
                  int *append) {
  char cmd_buffer[MAX_COMMAND_SIZE] = {0};
  strncpy(cmd_buffer, command, MAX_COMMAND_SIZE);

  char *token = NULL;
  token = strtok(command, SPACE_DELIM);
  do {
    if (strcmp(token, "<") == 0) {
      token = strtok(NULL, SPACE_DELIM);
      strcpy(input_filename, token);
    } else if (strcmp(token, ">") == 0) {
      token = strtok(NULL, SPACE_DELIM);
      *append = 0;
      strcpy(output_filename, token);
    } else if (strcmp(token, ">>") == 0) {
      token = strtok(NULL, SPACE_DELIM);
      *append = 1;
      strcpy(output_filename, token);
    } else {
      argv[(*argc)++] = token;
    }

    token = strtok(NULL, SPACE_DELIM);
  } while (token != NULL && *argc < MAX_ARGV_SIZE);

  if (argv[0] == NULL) {
    fprintf(stderr, "No command provided.\n");
    return -1;
  }

  return 0;
}

int execute_command(char *argv[MAX_ARGV_SIZE], char *input_filename,
                    char *output_filename, int append, int prev_pipefd,
                    int infd, int outfd, int not_last) {

  pid_t pid = fork();
  if (pid == -1) {
    perror("Fork failed.");
    return -1;
  }

  if (pid == 0) {
    if (prev_pipefd != -1) {
      dup2(prev_pipefd, STDIN_FILENO);
      close(prev_pipefd);
    }

    if (not_last == 1) {
      dup2(outfd, STDOUT_FILENO);
      close(infd);
      close(outfd);
    }

    if (input_filename[0] != '\0') {
      int fd = open(input_filename, O_RDONLY);
      if (fd == -1) {
        perror("Failed to open file to read");
        exit(1);
      }

      dup2(fd, STDIN_FILENO);
      close(fd);
    }

    if (output_filename[0] != '\0') {
      int flag = append == 1 ? O_APPEND : O_TRUNC;
      int fd = open(output_filename, O_WRONLY | O_CREAT | flag, 0644);
      if (fd == -1) {
        perror("Failed to open file to write");
        exit(1);
      }

      dup2(fd, STDOUT_FILENO);
      close(fd);
    }

    execvp(argv[0], argv);
    perror("Failed to exec");
    exit(1);
  }

  return 0;
}

void run(char input[MAX_COMMAND_SIZE]) {
  char *commands[MAX_COMMANDS];
  char *token = strtok(input, PIPE_DELIM);
  int commands_count = 0;

  do {
    commands[commands_count++] = token;
    token = strtok(NULL, PIPE_DELIM);
  } while (token != NULL && commands_count < MAX_ARGV_SIZE);

  int pipefd[2];
  int prev_pipefd = -1;
  for (int i = 0; i < commands_count; i++) {

    int argc = 0;
    char *argv[MAX_ARGV_SIZE];
    char input_filename[FILENAME_MAX] = {0};
    char output_filename[FILENAME_MAX] = {0};
    int append = 0;

    if (parse_command(commands[i], argv, &argc, input_filename, output_filename,
                      &append) == -1) {
      fprintf(stderr, "Failed to parse input.\n");
      return;
    }

    int not_last = i < commands_count - 1;
    if (not_last) {
      pipe(pipefd);
    }

    if (execute_command(argv, input_filename, output_filename, append,
                        prev_pipefd, pipefd[0], pipefd[1], not_last) == -1) {
      fprintf(stderr, "Failed to execute command.\n");
    }

    if (prev_pipefd != -1) {
      close(prev_pipefd);
    }

    if (not_last) {
      close(pipefd[1]);
      prev_pipefd = pipefd[0];
    }
  }

  for (int i = 0; i < commands_count; i++) {
    wait(NULL);
  }
}

int main() {
  char hostname[HOST_NAME_MAX];
  gethostname(hostname, sizeof(hostname));
  const char *login = getlogin();
  while (1) {
    printf("[%s@%s] $ ", login, hostname);
    char input[MAX_COMMAND_SIZE];

    if (fgets(input, MAX_COMMAND_SIZE, stdin) == NULL) {
      return EXIT_FAILURE;
    }

    if (input[0] == '\n') {
      continue;
    }

    input[strcspn(input, "\r\n")] = '\0';

    if (strcmp(input, "exit") == 0) {
      exit(EXIT_SUCCESS);
    }

    run(input);
  }
}