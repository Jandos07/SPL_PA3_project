#include <arpa/inet.h>
#include <ctype.h>
#include <editline/readline.h>
#include <helper.h>
#include <netinet/in.h>
#include <pa3_error.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "handle_response.h"
#include "helper.h"

#define CLEAR_SCREEN "\033[H\033[J"

const char* active_user = nullptr;
bool sigint_received = false;

int32_t get_socket(char* hostname, uint64_t port) {
  int32_t sockfd;
  struct sockaddr_in servaddr;

  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(port);

  if (inet_pton(AF_INET, hostname, &servaddr.sin_addr) <= 0) {
    perror("inet_pton failed");
    exit(EXIT_FAILURE);
  }

  // FIXED: Added missing comparison
  if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
    perror("connect failed");
    exit(EXIT_FAILURE);
  }

  return sockfd;
}

void send_request(int32_t sockfd, Request* request) {
  // Send action
  if (sigint_safe_write(sockfd, &request->action, sizeof(Action)) < 0) {
    perror("write action failed");
    exit(EXIT_FAILURE);
  }

  // Send username length and username if exists
  if (sigint_safe_write(sockfd, &request->username_length, sizeof(uint64_t)) < 0) {
    perror("write username length failed");
    exit(EXIT_FAILURE);
  }

  if (request->username_length > 0) {
    if (sigint_safe_write(sockfd, request->username, request->username_length) < 0) {
      perror("write username failed");
    exit(EXIT_FAILURE);
    }
  }

  // Send data size and data if exists
  if (sigint_safe_write(sockfd, &request->data_size, sizeof(uint64_t)) < 0) {
    perror("write data size failed");
    exit(EXIT_FAILURE);
  }

  if (request->data_size > 0) {
    if (sigint_safe_write(sockfd, request->data, request->data_size) < 0) {
      perror("write data failed");
      exit(EXIT_FAILURE);
    }
  }
}

void receive_response(int32_t sockfd, Response* response) {
  // Receive response code
  if (sigint_safe_read(sockfd, &response->code, sizeof(int32_t)) < 0) {
    perror("read response code failed");
    exit(EXIT_FAILURE);
  }

  // Receive data size
  if (sigint_safe_read(sockfd, &response->data_size, sizeof(uint64_t)) < 0) {
    perror("read data size failed");
    exit(EXIT_FAILURE);
  }

  // Receive data if exists
  if (response->data_size > 0) {
    response->data = malloc(response->data_size);
    if (response->data == nullptr) {
      perror("malloc for response data failed");
      exit(EXIT_FAILURE);
    }

    if (sigint_safe_read(sockfd, response->data, response->data_size) < 0) {
      perror("read data failed");
      exit(EXIT_FAILURE);
    }
  } else {
    response->data = nullptr;
  }
}

void terminate(int32_t sockfd, const char* active_user) {
  if (active_user != nullptr) {
    Request logout_request;
    default_request(&logout_request);
    logout_request.action = ACTION_LOGOUT;
    logout_request.username = strdup(active_user);
    logout_request.username_length = strlen(active_user);

    send_request(sockfd, &logout_request);
    
    Response logout_response;
    receive_response(sockfd, &logout_response);
    handle_response(ACTION_LOGOUT, &logout_request, &logout_response, &active_user);
    
    free_request(&logout_request);
    free_response(&logout_response);
  }
}

int main(int argc, char* argv[]) {
  setup_sigint_handler();

  if (argc != 3 && argc != 4) {
    fprintf(stderr, "usage: %s <IP address> <port> [file]\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  int32_t sockfd = get_socket(argv[1], strtoull(argv[2], nullptr, 10));

  if (argc == 4) {
    // File mode
    FILE* file = fopen(argv[3], "r");
    if (!file) {
      perror("fopen failed");
      exit(EXIT_FAILURE);
    }

    char* line = nullptr;
    size_t len = 0;
    while (getline(&line, &len, file) != -1 && !sigint_received) {
      line[strcspn(line, "\n")] = 0; // Remove newline

      Request request;
      ParsingError parsing_error = parse_request(&request, line, &active_user);
      free_input(&line);

      if (parsing_error != PARSING_SUCCESS)
        continue;

      send_request(sockfd, &request);

      Response response;
      receive_response(sockfd, &response);
      handle_response(request.action, &request, &response, &active_user);
      free_request(&request);
      free_response(&response);
    }
    fclose(file);
    free(line);
  } else {
    // Interactive mode
    char* input = nullptr;
    while (((input = readline("> ")) != nullptr) && !sigint_received) {
      add_history(input);

      if (strncmp(input, "exit", 4) == 0 || strncmp(input, "quit", 4) == 0) {
        free_input(&input);
        break;
      } else if (strncmp(input, "clear", 5) == 0) {
        printf(CLEAR_SCREEN);
        free_input(&input);
        continue;
      }

      Request request;
      ParsingError parsing_error = parse_request(&request, input, &active_user);
      free_input(&input);

      if (parsing_error != PARSING_SUCCESS)
        continue;

      send_request(sockfd, &request);

      Response response;
      receive_response(sockfd, &response);
      handle_response(request.action, &request, &response, &active_user);
      free_request(&request);
      free_response(&response);
    }
    free_input(&input);
  }

  terminate(sockfd, active_user);
  close(sockfd);
  return 0;
}