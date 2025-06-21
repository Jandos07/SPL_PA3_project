COMMON_SRCS := $(wildcard common/*.c)
COMMON_OBJS := $(COMMON_SRCS:.c=.o)

clean_common:
	rm -f $(COMMON_OBJS)