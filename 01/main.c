#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void process_args(bool is_child, int argc, char **argv) {
  int start = 1;
  if (is_child) {
    start = 2;
  }

  pid_t pid = getpid();
  pid_t ppid = getppid();

  for (int i = start; i < argc; i += 2) {
    if (is_child) {
      printf("CHILD  [%d] of [%d]:  ", pid, ppid);
    } else {
      printf("PARENT [%d]: \t\t", pid);
    }

    printf("Arg[%d] is ", i);

    char *endptr = NULL;
    int value_int = strtol(argv[i], &endptr, 10);
    if (*endptr == '\0') {
      printf("an INTEGER number: arg = %d, arg * 2 = %d\n", value_int,
             value_int * 2);
      continue;
    }

    float value_float = strtof(argv[i], &endptr);
    if (*endptr == '\0') {
      printf("a FLOAT number: arg = %f, arg * 2 = %f\n", value_float,
             value_float * 2);
      continue;
    }

    printf("NOT a number: %s\n", argv[i]);
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
    process_args(true, argc, argv);
    _exit(EXIT_SUCCESS);
  default:
    // printf("PARENT: PID - %d\n", getpid());
    // printf("PARENT: CHILD PID - %d\n", pid);
    process_args(false, argc, argv);
    wait(&retval);
    printf("PARENT: CHILD EXITED WITH STATUS - %d\n", WEXITSTATUS(retval));
    exit(EXIT_SUCCESS);
  }

  return EXIT_SUCCESS;
}