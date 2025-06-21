SERVER_SRCS := $(wildcard server/*.c)
SERVER_OBJS := $(SERVER_SRCS:.c=.o)

pa3_server: LDFLAGS += -largon2
pa3_server: $(SERVER_OBJS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean_pa3_server:
	rm -f $(SERVER_OBJS) pa3_server