#ifndef SERVER_HELPER_H
#define SERVER_HELPER_H
#include <helper.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>

// change to 10000 if you're facing an error here
#define NUM_USERS 10'000

#define MAXLINE 120
#define CLIENTS_PER_THREAD 1000
#define NUM_SEATS 100

#define MEMORY_USAGE 512
#define SALT_SIZE 16
#define HASH_SIZE 32
#define HASHED_PASSWORD_SIZE 128

typedef ssize_t pa3_uid_t;

typedef struct {
  const char* username;
  const char* hashed_password;
  bool logged_in;
} User;

typedef struct {
  User* array;
  size_t size;
  size_t capacity;
} Users;

typedef struct {
  struct pollfd set[CLIENTS_PER_THREAD];
  size_t size;
  pthread_mutex_t mutex;
} PollSet;

typedef struct {
  size_t thread_index;
  PollSet* poll_set;
  Users* users;
  Seat* seats;
  int32_t pipe_out_fd;
} ThreadData;

// Password-related functions
void hash_password(const char* password, char* hashed_password);
bool validate_password(const char* password_to_validate,
                       const char* hashed_password);

// User-related functions
User default_user();
void setup_users(Users* users);
void free_users(Users* users);
ssize_t find_user(const Users* users, const char* username);
size_t add_user(Users* users,
                const char* username,
                const char* hashed_password);

// Seat-related functions
Seat* default_seats();

// Poll set-related functions
PollSet* create_poll_set(int32_t self_pipe_fd);
ssize_t find_suitable_pollset(ThreadData* data_arr, int32_t n_cores);
void notify_pollset(int32_t notification_fd);

// Other functions
int32_t get_num_cores();
int32_t terminate_after_cleanup(int32_t (*pipe_fds)[2],
                                pthread_t* tid_arr,
                                ThreadData* data_arr,
                                int32_t n_cores,
                                int32_t listenfd,
                                Users* users,
                                Seat* seats);

int32_t handle_request(const Request* request,
                       Response* response,
                       Users* users,
                       Seat* seats);

bool check_stdin_for_termination();
#endif
