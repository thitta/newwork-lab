#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "http.h"

int load_http_reqst(char *input, struct HTTP_header *reqst) {
  /* this function parses a string into struct HTTP_header
   * (at this stage, this function only parse http status line)
   *
   * input  : a string expected to be a legal HTTP header, but can be anything received from socket
   * reqst  : a data structure that hold HTTP header properties
   *
   * return : 0 on successful parsing; -1 for error
   *
   * */
  char *sline_delim = strstr(input, "\r\n");
  char *sline;
  if (sline_delim != NULL) {
    size_t sline_len = sline_delim - &input[0];
    sline = calloc(1, sline_len + 1);
    memcpy(sline, input, sline_len);
  } else {
    return -1;
  }
  // pointer for strtok_r()
  char *token;
  char *rest = &sline[0];
  // method
  token = strtok_r(sline, " ", &rest);
  if (token != NULL) {
    strlcpy(reqst->method, token, sizeof(reqst->method));
    if (strstr(ACCEPT_METHODS, reqst->method) == NULL) { goto FAIL; }
  } else { goto FAIL; }
  // path
  token = strtok_r(rest, " ", &rest);
  if (token != NULL) {
    strlcpy(reqst->path, token, sizeof(reqst->path));
  } else { goto FAIL; }
  // protocol
  token = strtok_r(rest, "/", &rest);
  if (token != NULL) {
    strlcpy(reqst->proto, token, sizeof(reqst->proto));
    if (strstr(ACCEPT_PROTO, reqst->proto) == NULL) { goto FAIL; }
  } else { goto FAIL; }
  // protocol version
  strlcpy(reqst->proto_ver, rest, sizeof(reqst->proto_ver));
  if (strstr(ACCEPT_PROTO_VER, reqst->proto_ver) == NULL) { goto FAIL; }
  // FUTURE: parse more http header
  free(sline);
  return 0;
  FAIL:
  free(sline);
  return -1;
}

size_t load_http_resps(char *buffer, size_t size, struct HTTP_header *reqst, int scode) {
  /* this function parses http request and load response(header+body) into buffer
   * FUTURE: this section can serve as an interface with something like httpd or WSGI
   *
   * buffer : buffer to store response string
   * size   : size of the buffer
   * reqst  : a data structure that hold HTTP header properties
   * scode  : status code pre-determined at socket receiving stage, e.g. 408 on request timeout
   *          should be 0 or 200 if no error happened
   *
   * return loaded length
   *
   * */
  const size_t header_buf_len = 4 * 1024;
  const size_t body_buf_len = size - header_buf_len;
  const char *header_buf = calloc(1, header_buf_len);
  const char *body_buf = calloc(1, body_buf_len);
  memset(buffer, 0, size);
  // when status code is not yet determined
  if (scode == 0 || scode == 200) {
    char path[1 * 1024];
    load_fullpath(path, sizeof(path), reqst->path);
    long filesize = util_get_filesize(path);
    if (filesize == -1) {
      // file not exists, report 404;
      load_error_resps(buffer, size, 404);
    } else if (size <= filesize + header_buf_len + 1) {
      // buffer too small, report 500;
      load_error_resps(buffer, size, 500);
    } else {
      // 200 OK
      char filesize_str[16] = "";
      snprintf(filesize_str, sizeof(filesize_str), "%ld", filesize);
      load_status_line(buffer, size, 200);
      load_header(buffer + strlen(buffer) - 2, size - strlen(buffer), "Content-Type", "text/html; charset=UTF-8");
      load_header(buffer + strlen(buffer) - 2, size - strlen(buffer), "Content-Length", filesize_str);
      load_file(buffer + strlen(buffer), size - strlen(buffer) - 1, path);
    }
  } else {
    // when status code is pre-determined
    load_error_resps(buffer, size, scode);
  }
  free((void *) header_buf);
  free((void *) body_buf);
  return strlen(buffer);
}

size_t load_header(char *buffer, size_t size, char *key, char *val) {
  /* this function loads header string into buffer, ends with CRLF X2 and NULL
   *
   * buffer : buffer to store header string
   * size   : size of the buffer
   * key    : header key, e.g. Connection
   * val    : header value, e.g. keep-alive
   *
   * return loaded length
   *
   * */
  snprintf(buffer, size, "%s: %s\r\n\r\n", key, val);
  return (strlen(buffer));
}


ssize_t load_error_resps(char *buffer, size_t size, int scode) {
  /* this function builds a http error response string (header + body) and save into buffer
   *
   * buffer : buffer to store the whole response
   * size   : size of the buffer
   * scode  : error http status code, e.g. 404
   *
   * return : loaded length, or -1 onerror
   *
   * */
  char body_buf[1024];
  ssize_t body_len;
  body_len = load_error_body(body_buf, sizeof(body_buf), scode);
  if (body_len >= 0) {
    char body_len_str[16] = "";
    snprintf(body_len_str, sizeof(body_len_str), "%zu", body_len);
    load_status_line(buffer, size, scode);
    load_header(buffer + strlen(buffer) - 2, size - strlen(buffer) + 2, "Content-Type", "text/html; charset=UTF-8");
    load_header(buffer + strlen(buffer) - 2, size - strlen(buffer) + 2, "Content-Length", body_len_str);
    strlcpy(buffer + strlen(buffer), body_buf, size - strlen(buffer));
    return strlen(buffer);
  } else {
    return -1;
  }
}

ssize_t load_error_body(char *buffer, size_t size, int scode) {
  // this function loads error page template, parse status code, and save nul-terminated html string into buffer
  // return loaded length, or -1 on error
  memset(buffer, 0, size);
  char error_page_path[1024];
  char temp_buffer[size];
  load_fullpath(error_page_path, sizeof(error_page_path), (char *) ERROR_PAGE);
  ssize_t l = load_file(temp_buffer, size - 1, (char *) error_page_path);
  if (l >= 0) {
    snprintf(buffer, size, temp_buffer, scode);
    return strlen(buffer);
  }
  return -1;
}

size_t load_status_line(char *buffer, size_t size, int scode) {
  // this function loads http status line into buffer, ends with CRLF X2 and NULL
  // return loaded length
  char *msg = NULL;
  switch (scode) {
    case 0:
    case 200 :
      msg = "200 OK";
      break;
    case 400 :
      msg = "400 Bad Request";
      break;
    case 404 :
      msg = "404 Not Found";
      break;
    case 408 :
      msg = "408 Request Time-out";
      break;
    case 431 :
      msg = "431 Request Header Fields Too Large";
      break;
    case 500 :
      msg = "500 Internal Server Error";
      break;
    default:
      msg = "500 Internal Server Error";
  }
  snprintf(buffer, size, "HTTP/1.1 %s \r\n\r\n", msg);
  return 0;
}

size_t load_fullpath(char *buffer, size_t size, char *path) {
  /* this function loads local filepath string into buffer
   *
   * path : extracted from http request
   * (static const) ROOT_PATH : hosted root directory path
   * (static const) INDEX_PAGE: default html page file
   *
   * return loaded length
   *
   * */
  if (strcmp(path, "/") == 0) {
    path = (char *) INDEX_PAGE;
  }
  path = *path == '/' ? path + 1 : path;
  snprintf(buffer, size, "%s/%s", ROOT_PATH, path);
  return strlen(buffer);
}
