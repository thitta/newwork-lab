#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "util.h"
#include "http.h"

#define PORT 8080
#define MAX_CONNECTION 5
#define MAX_REQST_SIZE 32*1024
#define MAX_RESPS_SIZE 32*1024
#define IO_TIMEOUT 2     // ms
#define RECV_TIMEOUT 20  // second
#define SEND_TIMEOUT 20  // second

extern int errno;

ssize_t recv_reqst(int sock, char *buffer, size_t size, int *scode_buf, int timeout) {
  /* this function loads data from socket into <buffer> and nul-terminate; ends on CRLF X2 (ends of http header)
   * if socket error happens, load http status code into <scode_buf>, otherwise, load 0
   *
   * sock     : socket descriptor
   * buffer   : space for received data
   * scode_buf: space for status code
   * timeout  : the seconds that server is willing to spend on receiving
   *
   * return   : size received or -1 on error
   *
   * */
  ssize_t this_size = 0;
  size_t recv_size = 0;
  size_t left_size = size - 1;
  time_t deadline = time(NULL) + timeout;
  while (true) {
    this_size = recv(sock, buffer + recv_size, left_size, 0);
    recv_size = this_size >= 0 ? recv_size + this_size : recv_size;
    left_size = this_size >= 0 ? left_size - this_size : left_size;
    if (strstr(buffer, "\r\n\r\n") != NULL) {
      *(buffer + recv_size) = '\0';
      *scode_buf = 0;
      return 0;
    }
    // errors
    if (time(NULL) > deadline) { // 408 Request Time-out
      *scode_buf = 408;
      return -1;
    }
    if (left_size <= 0) { // 431 Request Header Fields Too Large
      *scode_buf = 431;
      return -1;
    }
    usleep(IO_TIMEOUT * 1000);
  }
}

ssize_t send_resps(int sock, char *data, size_t size, int timeout) {
  /* this function sends data via socket
   *
   * sock   : socket descriptor
   * data   : bytes to send
   * size   : length of the data
   * timeout: the seconds that server is willing to spend on sending
   *
   * return : size sent or -1 on error
   *
   * */
  ssize_t this_size = 0;
  size_t sent_size = 0;
  time_t deadline = time(NULL) + timeout;  // the deadline that server is willing to work on sending
  while (true) {
    this_size = send(sock, data, size, 0);
    sent_size = this_size >= 0 ? sent_size + this_size : sent_size;
    if (sent_size >= size) { return sent_size; }
    if (time(NULL) > deadline) { return -1; }
    usleep(IO_TIMEOUT * 1000);
  }
}

struct Thread_args {
  int ct_sock;
  struct sockaddr_in *ct_addr;
};

void *handle_connection(void *input) {
  /* this function runs in the child thread, responsible for socket recv(), send(), and shutdown()
   *
   * input: see struct Thread_args
   *
   * */
  struct Thread_args *args = (struct Thread_args *) input;
  // receive data
  char *recv_buffer = calloc(1, MAX_REQST_SIZE);
  int scode = 0; // http status code
  recv_reqst(args->ct_sock, recv_buffer, MAX_REQST_SIZE, &scode, RECV_TIMEOUT);
  struct HTTP_header reqst;
  if (scode == 0) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(args->ct_addr->sin_family, &(args->ct_addr->sin_addr), client_ip, sizeof(client_ip));
    int l = load_http_reqst(recv_buffer, &reqst);
    if (l == 0) {
      printf("%s %s %s/%s from %s\n", reqst.method, reqst.path, reqst.proto, reqst.proto_ver, client_ip);
    } else {
      // fail to parse received data into http header, set 400 Bad Request
      scode = 400;
    }
  }
  // build response and send
  char resps[MAX_RESPS_SIZE];
  load_http_resps(resps, MAX_RESPS_SIZE, &reqst, scode);
  send_resps(args->ct_sock, resps, strlen(resps), SEND_TIMEOUT);
  // shutdown()
  free(recv_buffer);
  free(args->ct_addr);
  free(args);
  close(args->ct_sock);
  pthread_exit(NULL);
}

int main() {
  signal(SIGPIPE, SIG_IGN);
  // check static dir
  DIR *dir = opendir(ROOT_PATH);
  if (dir == NULL) {
    printf("...can't access static directory: %s, you can set the path in http.h\n", ROOT_PATH);
    util_exit_on_error(-1);
  }
  closedir(dir);
  // socket create()
  int sv_sock;
  sv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  // (IPv4, Stream, TCP)
  util_exit_on_error(sv_sock);
  // socket bind()
  struct sockaddr_in sv_addr;
  sv_addr.sin_family = AF_INET;          // IPv4
  sv_addr.sin_port = htons(PORT);        // "H"ost endian "TO" "N"etwork endian "S"hort = htons()
  sv_addr.sin_addr.s_addr = INADDR_ANY;  // accept all source IP
  int b = bind(sv_sock, (struct sockaddr *) &sv_addr, sizeof(sv_addr));
  util_exit_on_error(b);
  // socket listen()
  int l = listen(sv_sock, MAX_CONNECTION);
  util_exit_on_error(l);
  printf("Serving HTTP on port %d (static dir: %s)...\n", PORT, ROOT_PATH);
  while (true) {
    // socket accept()
    int ct_sock;
    struct sockaddr_in *ct_addr = malloc(sizeof(struct sockaddr_in));
    socklen_t ct_addr_len = sizeof(struct sockaddr_in);
    ct_sock = accept(sv_sock, (struct sockaddr *) ct_addr, &ct_addr_len);
    if (ct_sock == -1) {
      printf("...unable to accept request, errno: %d (%s)", errno, strerror(errno));
      continue;
    } else {
      // set socket to non-block mode
      fcntl(ct_sock, F_SETFL, O_NONBLOCK);
      // create a child thread to handle the recv(), send(), and shutdown()
      // FUTURE: use thread pool can improve performance
      struct Thread_args *args = malloc(sizeof(struct Thread_args));
      args->ct_sock = ct_sock;
      args->ct_addr = ct_addr;
      pthread_t t;
      pthread_create(&t, NULL, handle_connection, (void *) args);
    }
  }
  return 0;
}
