#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


#define BUFFER_SIZE 512

int main(int argc, char *argv[]) {
  bool has_pid = false;
  bool has_filename = false;
  int pid;
  char filename[BUFFER_SIZE];
  
  if (argc > 2) {
    printf("Too many args\n");
    exit(1);
  }
  if (argc == 1) {
    printf("Not enough args\n");
    exit(1);
  }
  
  if (sscanf(argv[1], "--pid=%d", &pid) == 1) has_pid = true;
  if (sscanf(argv[1], "--filename=%s", filename) == 1) has_filename = true;
  
  FILE *file = fopen("/sys/kernel/debug/labmod/labmod_io", "r+");
  if (file == NULL) {
    printf("Can't open file\n");
    exit(1);
  }
  clearerr(file);

  if (has_pid) {
    char buffer[BUFFER_SIZE];
    fprintf(file, "pid: %d", pid);
    while (true) {
      char *msg = fgets(buffer, BUFFER_SIZE, file);
      if (msg == NULL) {
        if (feof(file)) break;
        fprintf(stderr, "VM area struct reading failed with errno code: %d\n", errno);
      } else {
        printf("%s", msg);
      }
    }
  }

  if (has_filename) {
    char buffer[BUFFER_SIZE];
    fprintf(file, "filename: %s", filename);
    while (true) {
      char *msg = fgets(buffer, BUFFER_SIZE, file);
      if (msg == NULL) {
        if (feof(file)) break;
        fprintf(stderr, "Address space struct reading failed with errno code: %d\n", errno);
      } else {
        printf("%s", msg);
      }
    }
  }

  
  if (!has_pid && !has_filename) {
    printf("Wrong params provided\n");
  }

  fclose(file);
  return 0;
}