#ifndef http
#define http

const static char *ACCEPT_METHODS = "GET";
const static char *ACCEPT_PROTO = "HTTP";
const static char *ACCEPT_PROTO_VER = "1.1 2.0 3.0";
const static char *ROOT_PATH = "../static";
const static char *ERROR_PAGE = "error.html";
const static char *INDEX_PAGE = "index.html";

struct HTTP_header {
  char method[8];
  char path[1024];
  char proto[8];
  char proto_ver[8];
};

int load_http_reqst(char *input, struct HTTP_header *reqst);

size_t load_http_resps(char *buffer, size_t size, struct HTTP_header *reqst, int scode);

size_t load_status_line(char *buffer, size_t size, int scode);

size_t load_fullpath(char *buffer, size_t size, char *path);

size_t load_header(char *buffer, size_t size, char *key, char *val);

ssize_t load_error_body(char *buffer, size_t size, int scode);

ssize_t load_error_resps(char *buffer, size_t size, int scode);

#endif