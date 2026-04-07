#include <stdio.h>
#include <unistd.h>

int main() {
  char *argv[2] = {0};
  int retval = execvp("ls", argv);
  printf("EXECV EXITED WITH %d\n", retval);
}