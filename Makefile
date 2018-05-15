CC := gcc

BIN_DIR := bin
SRC_DIR := src
INC_DIR := src
OBJ_DIR := obj

BINS := $(BIN_DIR)/threadpool
SRCS := $(shell find $(SRC_DIR) -type f -iname "*.c")
INCS := $(shell find $(INC_DIR) -type f -iname "*.h")
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

CFLAGS := -Wall -Wextra
LDFLAGS := -lpthread


all: $(BINS)

$(BINS): $(OBJS) | $(BIN_DIR)/
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(INCS) | $(OBJ_DIR)/
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN_DIR)/ $(OBJ_DIR)/:
	mkdir -p $@

.PHONY: all clean distclean

clean:
	rm -f $(BINS)
	rm -f $(OBJS)

distclean:
	rm -rf $(BIN_DIR)
	rm -rf $(OBJ_DIR)
