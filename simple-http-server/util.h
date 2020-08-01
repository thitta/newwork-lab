#ifndef util
#define util

void util_exit_on_error(int scode);

ssize_t load_file(char *buffer, size_t size, char *path);

ssize_t util_get_filesize(char *path);

#endif