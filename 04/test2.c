#include <string.h>
#include <stdio.h>

int main(int argc, char** argv) {
  char str[] = "ASDASD|ASDASD|SDASDA@|@#!#";
  char* token = strtok(str, "|");
  do {
    printf("token: %s\n", token);
    token = strtok(NULL, "|");
  } while(token != NULL);
}