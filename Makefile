.PHONY: all clean

.ONESHELL:

SOURCE_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

TARGET_DIR ?= $(SOURCE_DIR)/target

THREAD_DEBUG ?=

COMMON_FLAGS += -Wall -Wextra -Werror -ggdb -Wno-unused-result \
				-I$(SOURCE_DIR)

ifneq ($(THREAD_DEBUG),)
	COMMON_FLAGS += -DTHREAD_DEBUG -fsanitize=address -lasan
endif

CFLAGS += -std=c11 $(COMMON_FLAGS)
CXXFLAGS += -std=c++17 $(COMMON_FLAGS)
LDFLAGS ?=

export

$(shell mkdir -p $(TARGET_DIR))

all: $(TARGET_DIR)/threadpool.c.o $(TARGET_DIR)/example

sources_c = $(wildcard *.c)
objs_c = $(patsubst %.c,$(TARGET_DIR)/$(sub_name)/%.c.o,$(sources_c))

$(TARGET_DIR)/%.c.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_DIR)/example: $(TARGET_DIR)/threadpool.c.o $(TARGET_DIR)/example.c.o
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf $(TARGET_DIR)
