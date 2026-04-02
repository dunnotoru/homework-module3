#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void parent_process_args(int argc, char **argv) {
  pid_t pid = getpid();
  for (int i = 1; i < argc; i++) {
    char *endptr = NULL;
    float value = strtof(argv[i], &endptr);
    if (*endptr == '\0') {
      printf("PARENT [%d]: \t\t", pid);
      printf("Arg[%d] is a number: arg = %g, arg * 2 = %g\n", i, value, value * 2);
    }
  }
}

void child_process_args(int argc, char **argv) {
  pid_t pid = getpid();
  pid_t ppid = getppid();
  for (int i = 1; i < argc; i++) {
    char *endptr = NULL;
    float value = strtof(argv[i], &endptr);
    if (*endptr != '\0') {
      printf("CHILD  [%d] of [%d]:  ", pid, ppid);
      printf("Arg[%d] Is not number: %s\n",i, argv[i]);
    }
  }
}

int main(int argc, char **argv) {

  pid_t pid = -1;
  int retval = 0;
  pid = fork();

  switch (pid) {
  case -1:
    perror("Fork error.");
    exit(EXIT_FAILURE);
  case 0:
    // printf("CHILD: PID - %d\n", getpid());
    // printf("CHILD: PPID - %d\n", getppid());
    child_process_args(argc, argv);
    _exit(EXIT_SUCCESS);
  default:
    // printf("PARENT: PID - %d\n", getpid());
    // printf("PARENT: CHILD PID - %d\n", pid);
    parent_process_args(argc, argv);
    wait(&retval);
    printf("PARENT: CHILD EXITED WITH STATUS - %d\n", WEXITSTATUS(retval));
    exit(EXIT_SUCCESS);
  }

  return EXIT_SUCCESS;
}