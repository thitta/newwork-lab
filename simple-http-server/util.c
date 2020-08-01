#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "util.h"

int extern errno;

void util_exit_on_error(int scode) {
  if (scode == -1) {
    fprintf(stderr, "Value of errno: %d\n", errno);
    perror("Error: ");
    exit(1);
  }
}

ssize_t load_file(char *buffer, size_t size, char *path) {
  /* read at most <size> bytes from <path> into <buffer>. NO nul-terminator will be appended.
   *
   * return: bytes loaded, or -1 on error
   * */
  FILE *fp = fopen(path, "r");
  ssize_t result;
  if (fp) {
    fseek(fp, 0, SEEK_END);
    const size_t filesize = (size_t) ftell(fp);
    fseek(fp, 0, SEEK_SET);
    const size_t readsize = size <= filesize ? size : filesize;
    fread(buffer, 1, readsize, fp);
    result = readsize;
  } else {
    result = -1;
  }
  fclose(fp);
  return result;
}

ssize_t util_get_filesize(char *path) {
  // get filesize in byte, return -1 on error
  FILE *fp = fopen(path, "r");
  ssize_t result;
  if (fp) {
    fseek(fp, 0, SEEK_END);
    result = ftell(fp);
  } else {
    result = -1;
  }
  fclose(fp);
  return result;
}
