#ifndef HANDLE_RESPONSE_H
#define HANDLE_RESPONSE_H

#include <helper.h>

int32_t handle_response(Action action,
                        const Request* request,
                        const Response* response,
                        const char** active_user);
#endif