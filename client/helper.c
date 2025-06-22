#include <ctype.h>
#include <helper.h>
#include <pa3_error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argon2.h>


Action to_action(const char* action_str) {
  Action action = ACTION_INVALID;

  char* action_str_copy = strdup(action_str);
  char* action_str_pointer = (char*)action_str_copy;

  for (char* action_str_source = (char*)action_str; *action_str_source;
       ++action_str_source) {
    if (isspace((unsigned char)*action_str_source) ||
        ispunct((unsigned char)*action_str_source)) {
      continue;
    }
    *action_str_pointer = *action_str_source;
    action_str_pointer++;
  }
  *action_str_pointer = '\0';

  if (strcmp(action_str_copy, "login") == 0) {
    action = ACTION_LOGIN;
  } else if (strcmp(action_str_copy, "book") == 0) {
    action = ACTION_BOOK;
  } else if (strcmp(action_str_copy, "confirmbooking") == 0) {
    action = ACTION_CONFIRM_BOOKING;
  } else if (strcmp(action_str_copy, "cancelbooking") == 0) {
    action = ACTION_CANCEL_BOOKING;
  } else if (strcmp(action_str_copy, "logout") == 0) {
    action = ACTION_LOGOUT;
  } else if (strcmp(action_str_copy, "query") == 0) {
    action = ACTION_QUERY;
  }
  free(action_str_copy);
  return action;
}

ParsingError parse_request(Request* request,
                           const char* input,
                           const char** active_user) {
  ParsingError parsing_error = PARSING_SUCCESS;
  if (request == nullptr) {
    fprintf(stderr, "Request pointer is nullptr!\n");
    return PARSING_NO_REQUEST;
  }

  default_request(request);

  char* input_copy = strdup(input);
  char* saveptr;

  char* token = strtok_r(input_copy, " ", &saveptr);
  if (token != nullptr) {
    request->action = to_action(token);
    if (request->action == ACTION_INVALID) {
      // fprintf(stderr, "Please enter a valid action! (received: %s)\n",
      // token);
      fprintf(stderr, "Invalid action received!\n");
      parsing_error = PARSING_INVALID_ACTION;
      goto cleanup_error;
    }
    if (request->action != ACTION_LOGIN) {
      if (active_user == nullptr || *active_user == nullptr) {
        fprintf(stderr, "User is not logged in!\n");
        parsing_error = PARSING_NOT_LOGGED_IN;
        goto cleanup_error;
      }
      request->username = strdup(*active_user);
      request->username_length = strlen(*active_user);
    } else {
      if (active_user != nullptr && *active_user != nullptr) {
        fprintf(stderr, "Client is already serving user %s!\n", *active_user);
        parsing_error = PARSING_ALREADY_LOGGED_IN;
        goto cleanup_error;
      }
    }

    if (request->action == ACTION_LOGOUT)
      goto cleanup;
  } else {
    fprintf(stderr, "Please enter the action!\n");
    parsing_error = PARSING_NO_ACTION;
    goto cleanup;
  }

  token = strtok_r(nullptr, " ", &saveptr);

  if (token == nullptr) {
    fprintf(stderr, "Please enter the action and data!\n");
    parsing_error = PARSING_NO_DATA;
    goto cleanup_error;
  }

  if (request->action == ACTION_LOGIN) {
    request->username = strdup(token);
    request->username_length = strlen(token);
    // *active_user = strdup(request->username);
  } else {
    request->data = strdup(token);
    request->data_size = strlen(token);
    goto cleanup;
  }

  token = strtok_r(nullptr, " ", &saveptr);
  if (token != nullptr) {
    request->data = strdup(token);
    request->data_size = strlen(token);
    goto cleanup;
  } else {
    fprintf(stderr, "Please enter your password!\n");
    parsing_error = PARSING_NO_DATA;
  }

cleanup_error:
  free_request(request);
cleanup:
  free(input_copy);
  return parsing_error;
}

void free_input(char** input) {
  if (input == nullptr || *input == nullptr) {
    return;
  }
  free(*input);

  *input = nullptr;
}