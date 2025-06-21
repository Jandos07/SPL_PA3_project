#ifndef CLIENT_HELPER_H
#define CLIENT_HELPER_H
#include "helper.h"

Action to_action(const char* action_str);
ParsingError parse_request(Request* request,
                           const char* input,
                           const char** active_user);
void free_input(char** input);
#endif
