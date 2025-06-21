CLIENT_SRCS := $(wildcard client/*.c)
CLIENT_OBJS := $(CLIENT_SRCS:.c=.o)

pa3_client: LDFLAGS += -ledit

pa3_client: $(CLIENT_OBJS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean_pa3_client:
	rm -f $(CLIENT_OBJS) pa3_client