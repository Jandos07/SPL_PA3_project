#include <arpa/inet.h>
#include <errno.h>
#include <helper.h>
#include <netinet/in.h>
#include <pa3_error.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "helper.h"

bool sigint_received = false;

void add_to_pollset(PollSet* poll_set,
                    int32_t notification_fd,
                    int32_t connfd) {
  pthread_mutex_lock(&poll_set->mutex);
  
  if (poll_set->size >= CLIENTS_PER_THREAD) {
    pthread_mutex_unlock(&poll_set->mutex);
    return;
  }

  poll_set->set[poll_set->size].fd = connfd;
  poll_set->set[poll_set->size].events = POLLIN;
  poll_set->size++;
  
  pthread_mutex_unlock(&poll_set->mutex);
  notify_pollset(notification_fd);
}

void remove_from_pollset(ThreadData* data, size_t* i_ptr) {
  PollSet* poll_set = data->poll_set;
  
  // Shift all elements after *i_ptr one position left
  for (size_t j = *i_ptr; j < poll_set->size - 1; j++) {
    poll_set->set[j] = poll_set->set[j + 1];
  }
  
  poll_set->size--;
  (*i_ptr)--;
}

void* thread_func(void* arg) {
  ThreadData* data = (ThreadData*)arg;
  PollSet* poll_set = data->poll_set;
  
  while (!sigint_received) {
    pthread_mutex_lock(&poll_set->mutex);
    int ready = poll(poll_set->set, poll_set->size, 100);
    pthread_mutex_unlock(&poll_set->mutex);
    
    if (ready < 0) {
      if (errno == EINTR) continue;
      perror("poll");
      break;
    }
    
    if (ready == 0) continue;
    
    pthread_mutex_lock(&poll_set->mutex);
    for (size_t i = 0; i < poll_set->size && ready > 0; i++) {
      if (poll_set->set[i].revents & POLLIN) {
        ready--;
        
        if (poll_set->set[i].fd == data->pipe_out_fd) {
          // Notification from main thread
          char buf;
          read(data->pipe_out_fd, &buf, 1);
          continue;
        }
        
        // Handle client request
        Request request;
        Response response;
        
        // Receive request
        if (sigint_safe_read(poll_set->set[i].fd, &request.action, sizeof(Action)) <= 0) {
          close(poll_set->set[i].fd);
          remove_from_pollset(data, &i);
          continue;
        }
        
        if (sigint_safe_read(poll_set->set[i].fd, &request.username_length, sizeof(uint64_t)) <= 0) {
          close(poll_set->set[i].fd);
          remove_from_pollset(data, &i);
          continue;
        }
        
        if (request.username_length > 0) {
          request.username = malloc(request.username_length + 1);
          if (sigint_safe_read(poll_set->set[i].fd, request.username, request.username_length) <= 0) {
            free(request.username);
            close(poll_set->set[i].fd);
            remove_from_pollset(data, &i);
            continue;
          }
          request.username[request.username_length] = '\0';
        } else {
          request.username = nullptr;
        }
        
        if (sigint_safe_read(poll_set->set[i].fd, &request.data_size, sizeof(uint64_t)) <= 0) {
          if (request.username) free(request.username);
          close(poll_set->set[i].fd);
          remove_from_pollset(data, &i);
          continue;
        }
        
        if (request.data_size > 0) {
          request.data = malloc(request.data_size + 1);
          if (sigint_safe_read(poll_set->set[i].fd, request.data, request.data_size) <= 0) {
            free(request.username);
            free(request.data);
            close(poll_set->set[i].fd);
            remove_from_pollset(data, &i);
            continue;
          }
          request.data[request.data_size] = '\0';
        } else {
          request.data = nullptr;
        }
        
        // Process request
        handle_request(&request, &response, data->users, data->seats);
        
        // Send response
        if (sigint_safe_write(poll_set->set[i].fd, &response.code, sizeof(int32_t)) <= 0) {
          free_request(&request);
          free_response(&response);
          close(poll_set->set[i].fd);
          remove_from_pollset(data, &i);
          continue;
        }
        
        if (sigint_safe_write(poll_set->set[i].fd, &response.data_size, sizeof(uint64_t)) <= 0) {
          free_request(&request);
          free_response(&response);
          close(poll_set->set[i].fd);
          remove_from_pollset(data, &i);
          continue;
        }
        
        if (response.data_size > 0) {
          if (sigint_safe_write(poll_set->set[i].fd, response.data, response.data_size) <= 0) {
            free_request(&request);
            free_response(&response);
            close(poll_set->set[i].fd);
            remove_from_pollset(data, &i);
            continue;
          }
        }
        
        free_request(&request);
        free_response(&response);
        
        if (request.action == ACTION_TERMINATION) {
          close(poll_set->set[i].fd);
          remove_from_pollset(data, &i);
        }
      }
    }
    pthread_mutex_unlock(&poll_set->mutex);
  }
  
  pthread_exit(nullptr);
}

int main(int argc, char* argv[]) {
  setup_sigint_handler();

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    return 1;
  }

  int listenfd;
  struct sockaddr_in saddr, caddr;

  Users users;
  setup_users(&users);

  Seat* seats = default_seats();
  int32_t n_cores = get_num_cores();

  pthread_t* tid_arr = malloc(sizeof(pthread_t) * n_cores);
  ThreadData* data_arr = malloc(sizeof(ThreadData) * n_cores);
  int32_t (*pipe_fds)[2] = malloc(sizeof(int32_t[2]) * n_cores);

  for (int i = 0; i < n_cores; i++) {
    if (pipe(pipe_fds[i]) < 0) {
      perror("pipe");
      exit(EXIT_FAILURE);
    }

    data_arr[i].thread_index = i;
    data_arr[i].pipe_out_fd = pipe_fds[i][0];
    data_arr[i].poll_set = create_poll_set(pipe_fds[i][0]);
    data_arr[i].users = &users;
    data_arr[i].seats = seats;
    pthread_create(&tid_arr[i], nullptr, thread_func, &data_arr[i]);
  }

  listenfd = socket(AF_INET, SOCK_STREAM, 0);

  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  saddr.sin_port = htons(strtoull(argv[1], nullptr, 10));

  bind(listenfd, (struct sockaddr*)&saddr, sizeof(saddr));
  listen(listenfd, 10);

  struct pollfd main_thread_poll_set[2];
  memset(main_thread_poll_set, 0, sizeof(main_thread_poll_set));
  main_thread_poll_set[0].fd = STDIN_FILENO;
  main_thread_poll_set[0].events = POLLIN;
  main_thread_poll_set[1].fd = listenfd;
  main_thread_poll_set[1].events = POLLIN;

  while (!sigint_received) {
    if (poll(main_thread_poll_set, 2, -1) < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("poll");
      exit(EXIT_FAILURE);
    }

    if (main_thread_poll_set[0].revents & POLLIN) {
      if (check_stdin_for_termination() == true) {
        kill(getpid(), SIGINT);
        continue;
      }
    } else if (main_thread_poll_set[1].revents & POLLIN) {
      uint32_t caddrlen = sizeof(caddr);
      int connfd = accept(listenfd, (struct sockaddr*)&caddr, &caddrlen);
      if (connfd < 0) {
        if (errno == EINTR) {
          continue;
        }
        puts("accept() failed");
        exit(EXIT_FAILURE);
      }

      printf("Accepted connection from client\n");
      ssize_t pollset_i;
      do {
        pollset_i = find_suitable_pollset(data_arr, n_cores);
      } while (pollset_i == -1);
      add_to_pollset(data_arr[pollset_i].poll_set, pipe_fds[pollset_i][1],
                     connfd);
    }
  }

  return terminate_after_cleanup(pipe_fds, tid_arr, data_arr, n_cores, listenfd,
                                 &users, seats);
}