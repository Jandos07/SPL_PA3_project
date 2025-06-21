#include "helper.h"
#include <argon2.h>
#include <fcntl.h>
#include <sched.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <unistd.h>

// Password-related functions
void generate_salt(uint8_t* salt) {
  getrandom(salt, SALT_SIZE, 0);
}

void hash_password(const char* password, char* hashed_password) {
  uint8_t salt[SALT_SIZE];
  generate_salt(salt);
  char hash[HASHED_PASSWORD_SIZE];
  argon2id_hash_encoded(2, MEMORY_USAGE, 1, password, strlen(password), salt,
                        SALT_SIZE, HASH_SIZE, hash, HASHED_PASSWORD_SIZE);
  strncpy(hashed_password, hash, HASHED_PASSWORD_SIZE);
}

bool validate_password(const char* password_to_validate,
                       const char* hashed_password) {
  return argon2id_verify(hashed_password, password_to_validate,
                         strlen(password_to_validate)) == ARGON2_OK;
}

// User-related functions
User default_user() {
  return (User){
      .username = nullptr, .hashed_password = nullptr, .logged_in = false};
}

void setup_users(Users* users) {
  users->size = 0;
  users->capacity = NUM_USERS;
  users->array = malloc(sizeof(User) * users->capacity);
  for (int i = 0; i < NUM_USERS; i++) {
    users->array[i] = default_user();
  }
}
ssize_t find_user(const Users* users, const char* username) {
  for (size_t i = 0; i < users->size; i++)
    if (strcmp(users->array[i].username, username) == 0)
      return i;
  return -1;
}

size_t add_user(Users* users,
                const char* username,
                const char* hashed_password) {
  if (users->size >= users->capacity) {
    users->capacity *= 2;
    users->array = realloc(users->array, sizeof(User) * users->capacity);
  }
  size_t uid = users->size;

  users->array[uid].username = strdup(username);
  users->array[uid].hashed_password = strdup(hashed_password);
  users->size++;
  return uid;
}

void free_users(Users* users) {
  for (size_t i = 0; i < users->size; i++) {
    free((char*)users->array[i].username);
    free((char*)users->array[i].hashed_password);
  }
  free(users->array);
}

// Seat-related functions

Seat* default_seats() {
  Seat* seats = malloc(sizeof(Seat) * NUM_SEATS);

  for (int i = 0; i < NUM_SEATS; i++) {
    seats[i].id = i + 1;
    seats[i].amount_of_times_booked = 0;
    seats[i].amount_of_times_canceled = 0;
    seats[i].user_who_booked = nullptr;
    pthread_mutex_init(&seats[i].mutex, nullptr);
  }

  return seats;
}

// Poll set-related functions
PollSet* create_poll_set(int32_t self_pipe_fd) {
  PollSet* poll_set = calloc(1, sizeof(PollSet));
  pthread_mutex_init(&poll_set->mutex, nullptr);
  poll_set->set[0].fd = self_pipe_fd;
  poll_set->set[0].events = POLLIN;
  poll_set->size = 1;

  return poll_set;
}

ssize_t find_suitable_pollset(ThreadData* data_arr, int32_t n_cores) {
  ssize_t min_i = -1;
  for (int i = 0; i < n_cores; i++) {
    size_t poll_set_size = data_arr[i].poll_set->size;

    if (poll_set_size < CLIENTS_PER_THREAD) {
      if (min_i == -1 || poll_set_size < data_arr[min_i].poll_set->size) {
        min_i = i;
      }
    }
  }
  return min_i;
}

void notify_pollset(int32_t notification_fd) {
  write(notification_fd, "", 1);
}

// Others
int32_t get_num_cores() {
  cpu_set_t cpu_set;
  sched_getaffinity(0, sizeof(cpu_set), &cpu_set);
  return CPU_COUNT_S(sizeof(cpu_set), &cpu_set);
}

int32_t terminate_after_cleanup(int32_t (*pipe_fds)[2],
                                pthread_t* tid_arr,
                                ThreadData* data_arr,
                                int32_t n_cores,
                                int listenfd,
                                Users* users,
                                Seat* seats) {
  for (int i = 0; i < n_cores; i++) {
    write(pipe_fds[i][1], "", 1);
    close(pipe_fds[i][1]);
    pthread_join(tid_arr[i], nullptr);
    close(pipe_fds[i][0]);
    pthread_mutex_destroy(&data_arr[i].poll_set->mutex);
    free(data_arr[i].poll_set);
  }

  for (int i = 0; i < NUM_SEATS; i++) {
    pthread_mutex_destroy(&seats[i].mutex);
    if (seats[i].user_who_booked != nullptr) {
      free((void*)seats[i].user_who_booked);
    }
  }

  close(listenfd);
  free(tid_arr);
  free(pipe_fds);
  free(data_arr);
  free_users(users);
  puts("Server terminated!");
  return 0;
}

bool check_stdin_for_termination() {
  bool should_exit = false;

  char buffer[MAXLINE];
  ssize_t n_read = read(STDIN_FILENO, buffer, sizeof(buffer));
  if (n_read < 0) {
    perror("read");
    should_exit = true;
  } else {
    buffer[n_read - 1] = '\0';

    if (strncmp(buffer, "exit", 4) == 0 || strncmp(buffer, "quit", 4) == 0 ||
        strncmp(buffer, "\0", 1) == 0) {
      should_exit = true;
    }
  }
  return should_exit;
}
