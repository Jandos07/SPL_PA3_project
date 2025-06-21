#include "helper.h"
#include <ctype.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>

void sigint_handler(int32_t signum) {
  if (signum == SIGINT)
    sigint_received = true;
}

void setup_sigint_handler() {
  struct sigaction action = {.sa_handler = sigint_handler, .sa_flags = 0};
  sigemptyset(&action.sa_mask);
  sigaction(SIGINT, &action, nullptr);
}

void default_request(Request* request) {
  request->username = nullptr;
  request->username_length = 0;
  request->data = nullptr;
  request->data_size = 0;
  request->action = ACTION_INVALID;
}

void free_request(Request* request) {
  if (request->username != nullptr) {
    free(request->username);
    request->username = nullptr;
  }
  if (request->data != nullptr) {
    free(request->data);
    request->data = nullptr;
  }
}

void free_response(Response* response) {
  if (response->data != nullptr) {
    free(response->data);
    response->data = nullptr;
  }
}

ssize_t sigint_safe_write(int32_t fd, void* buf, size_t count) {
  ssize_t n_written;
  do {
    n_written = write(fd, buf, count);
  } while (n_written < 0 && errno == EINTR);
  return n_written;
}

ssize_t sigint_safe_read(int32_t fd, void* buf, size_t count) {
  ssize_t n_read;
  do {
    n_read = read(fd, buf, count);
  } while (n_read < 0 && errno == EINTR);
  return n_read;
}
