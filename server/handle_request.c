#include <helper.h>
#include <pa3_error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "helper.h"

LoginErrorCode handle_login_request(const Request* request,
                                    Response* response,
                                    Users* users) {
  if (request->data_size == 0) {
    response->code = LOGIN_ERROR_NO_PASSWORD;
    return LOGIN_ERROR_NO_PASSWORD;
  }

  ssize_t user_index = find_user(users, request->username);
  
  if (user_index == -1) {
    // New user
    char hashed_password[HASHED_PASSWORD_SIZE];
    hash_password(request->data, hashed_password);
    size_t new_user_index = add_user(users, request->username, hashed_password);
    users->array[new_user_index].logged_in = true;
    response->code = LOGIN_ERROR_SUCCESS;
    return LOGIN_ERROR_SUCCESS;
  } else {
    // Existing user
    if (users->array[user_index].logged_in) {
      response->code = LOGIN_ERROR_ACTIVE_USER;
      return LOGIN_ERROR_ACTIVE_USER;
    }

    if (!validate_password(request->data, users->array[user_index].hashed_password)) {
      response->code = LOGIN_ERROR_INCORRECT_PASSWORD;
      return LOGIN_ERROR_INCORRECT_PASSWORD;
    }

    users->array[user_index].logged_in = true;
    response->code = LOGIN_ERROR_SUCCESS;
    return LOGIN_ERROR_SUCCESS;
  }
}

BookErrorCode handle_book_request(const Request* request,
                                  Response* response,
                                  Users* users,
                                  Seat* seats) {
  if (request->data_size == 0) {
    response->code = BOOK_ERROR_NO_DATA;
    return BOOK_ERROR_NO_DATA;
  }

  ssize_t user_index = find_user(users, request->username);
  if (user_index == -1 || !users->array[user_index].logged_in) {
    response->code = BOOK_ERROR_USER_NOT_LOGGED_IN;
    return BOOK_ERROR_USER_NOT_LOGGED_IN;
  }

  char* endptr;
  long seat_num = strtol(request->data, &endptr, 10);
  if (*endptr != '\0' || seat_num < 1 || seat_num > NUM_SEATS) {
    response->code = BOOK_ERROR_SEAT_OUT_OF_RANGE;
    return BOOK_ERROR_SEAT_OUT_OF_RANGE;
  }

  Seat* seat = &seats[seat_num - 1];
  pthread_mutex_lock(&seat->mutex);

  if (seat->user_who_booked != nullptr) {
    pthread_mutex_unlock(&seat->mutex);
    response->code = BOOK_ERROR_SEAT_UNAVAILABLE;
    return BOOK_ERROR_SEAT_UNAVAILABLE;
  }

  seat->user_who_booked = strdup(request->username);
  seat->amount_of_times_booked++;
  pthread_mutex_unlock(&seat->mutex);

  response->code = BOOK_ERROR_SUCCESS;
  return BOOK_ERROR_SUCCESS;
}

ConfirmBookingErrorCode handle_confirm_booking_request(const Request* request,
                                                       Response* response,
                                                       Users* users,
                                                       Seat* seats) {
  if (request->data_size == 0) {
    response->code = CONFIRM_BOOKING_ERROR_NO_DATA;
    return CONFIRM_BOOKING_ERROR_NO_DATA;
  }

  ssize_t user_index = find_user(users, request->username);
  if (user_index == -1 || !users->array[user_index].logged_in) {
    response->code = CONFIRM_BOOKING_ERROR_USER_NOT_LOGGED_IN;
    return CONFIRM_BOOKING_ERROR_USER_NOT_LOGGED_IN;
  }

  bool show_available = strcmp(request->data, "available") == 0;
  bool show_booked = strcmp(request->data, "booked") == 0;

  if (!show_available && !show_booked) {
    response->code = CONFIRM_BOOKING_ERROR_INVALID_DATA;
    return CONFIRM_BOOKING_ERROR_INVALID_DATA;
  }

  pa3_seat_t* result_seats = malloc(NUM_SEATS * sizeof(pa3_seat_t));
  size_t count = 0;

  for (int i = 0; i < NUM_SEATS; i++) {
    pthread_mutex_lock(&seats[i].mutex);
    bool is_available = seats[i].user_who_booked == nullptr;
    bool is_booked_by_user = !is_available && strcmp(seats[i].user_who_booked, request->username) == 0;
    pthread_mutex_unlock(&seats[i].mutex);

    if ((show_available && is_available) || (show_booked && is_booked_by_user)) {
      result_seats[count++] = i + 1;
    }
  }

  if (count > 0) {
    response->data = (uint8_t*)result_seats;
    response->data_size = count * sizeof(pa3_seat_t);
  } else {
    free(result_seats);
    response->data = nullptr;
    response->data_size = 0;
  }

  response->code = CONFIRM_BOOKING_ERROR_SUCCESS;
  return CONFIRM_BOOKING_ERROR_SUCCESS;
}

CancelBookingErrorCode handle_cancel_booking_request(const Request* request,
                                                     Response* response,
                                                     Users* users,
                                                     Seat* seats) {
  if (request->data_size == 0) {
    response->code = CANCEL_BOOKING_ERROR_NO_DATA;
    return CANCEL_BOOKING_ERROR_NO_DATA;
  }

  ssize_t user_index = find_user(users, request->username);
  if (user_index == -1 || !users->array[user_index].logged_in) {
    response->code = CANCEL_BOOKING_ERROR_USER_NOT_LOGGED_IN;
    return CANCEL_BOOKING_ERROR_USER_NOT_LOGGED_IN;
  }

  char* endptr;
  long seat_num = strtol(request->data, &endptr, 10);
  if (*endptr != '\0' || seat_num < 1 || seat_num > NUM_SEATS) {
    response->code = CANCEL_BOOKING_ERROR_SEAT_OUT_OF_RANGE;
    return CANCEL_BOOKING_ERROR_SEAT_OUT_OF_RANGE;
  }

  Seat* seat = &seats[seat_num - 1];
  pthread_mutex_lock(&seat->mutex);

  if (seat->user_who_booked == nullptr || strcmp(seat->user_who_booked, request->username) != 0) {
    pthread_mutex_unlock(&seat->mutex);
    response->code = CANCEL_BOOKING_ERROR_SEAT_NOT_BOOKED_BY_USER;
    return CANCEL_BOOKING_ERROR_SEAT_NOT_BOOKED_BY_USER;
  }

  free((void*)seat->user_who_booked);
  seat->user_who_booked = nullptr;
  seat->amount_of_times_canceled++;
  pthread_mutex_unlock(&seat->mutex);

  response->code = CANCEL_BOOKING_ERROR_SUCCESS;
  return CANCEL_BOOKING_ERROR_SUCCESS;
}

LogoutErrorCode handle_logout_request(const Request* request,
                                      Response* response,
                                      Users* users) {
  ssize_t user_index = find_user(users, request->username);
  if (user_index == -1) {
    response->code = LOGOUT_ERROR_USER_NOT_FOUND;
    return LOGOUT_ERROR_USER_NOT_FOUND;
  }

  if (!users->array[user_index].logged_in) {
    response->code = LOGOUT_ERROR_USER_NOT_LOGGED_IN;
    return LOGOUT_ERROR_USER_NOT_LOGGED_IN;
  }

  users->array[user_index].logged_in = false;
  response->code = LOGOUT_ERROR_SUCCESS;
  return LOGOUT_ERROR_SUCCESS;
}

QueryErrorCode handle_query_request(const Request* request,
                                    Response* response,
                                    Seat* seats) {
  if (request->data_size == 0) {
    response->code = QUERY_ERROR_NO_DATA;
    return QUERY_ERROR_NO_DATA;
  }

  char* endptr;
  long seat_num = strtol(request->data, &endptr, 10);
  if (*endptr != '\0' || seat_num < 1 || seat_num > NUM_SEATS) {
    response->code = QUERY_ERROR_SEAT_OUT_OF_RANGE;
    return QUERY_ERROR_SEAT_OUT_OF_RANGE;
  }

  Seat* seat = &seats[seat_num - 1];
  pthread_mutex_lock(&seat->mutex);
  
  Seat* seat_copy = malloc(sizeof(Seat));
  if (seat_copy == nullptr) {
    pthread_mutex_unlock(&seat->mutex);
    perror("malloc failed");
    exit(EXIT_FAILURE);
  }
  
  memcpy(seat_copy, seat, sizeof(Seat));
  pthread_mutex_unlock(&seat->mutex);

  response->data = (uint8_t*)seat_copy;
  response->data_size = sizeof(Seat);
  response->code = QUERY_ERROR_SUCCESS;
  return QUERY_ERROR_SUCCESS;
}

int32_t handle_request(const Request* request,
                       Response* response,
                       Users* users,
                       Seat* seats) {
  switch (request->action) {
    case ACTION_LOGIN:
      return handle_login_request(request, response, users);
    case ACTION_BOOK:
      return handle_book_request(request, response, users, seats);
    case ACTION_CONFIRM_BOOKING:
      return handle_confirm_booking_request(request, response, users, seats);
    case ACTION_CANCEL_BOOKING:
      return handle_cancel_booking_request(request, response, users, seats);
    case ACTION_LOGOUT:
      return handle_logout_request(request, response, users);
    case ACTION_QUERY:
      return handle_query_request(request, response, seats);
    case ACTION_TERMINATION:
      response->code = -1;
      return -1;
    default:
      fprintf(stderr, "Invalid action received: %d\n", request->action);
      response->code = -1;
      return -1;
  }
}