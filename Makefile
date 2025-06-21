CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -Werror -ggdb -std=gnu2x -D_GNU_SOURCE -Iinclude \
         -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fsanitize=address
LDFLAGS = -fsanitize=address

COMMON_SRCS = common/helper.c
COMMON_OBJS = $(COMMON_SRCS:.c=.o)

SERVER_SRCS = $(wildcard server/*.c)
SERVER_OBJS = $(SERVER_SRCS:.c=.o)

CLIENT_SRCS = $(wildcard client/*.c)
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

all: pa3_server pa3_client

pa3_server: $(SERVER_OBJS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -largon2 -pthread

pa3_client: $(CLIENT_OBJS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -ledit

clean:
	rm -f $(COMMON_OBJS) $(SERVER_OBJS) $(CLIENT_OBJS) pa3_server pa3_client

test: all
	./test_pa3.sh

.PHONY: all clean test