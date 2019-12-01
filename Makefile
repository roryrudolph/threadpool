EXENAME := threadpool
CC := gcc

BIN_DIR := bin
SRC_DIR := src
INC_DIR := src
OBJ_DIR := obj

BINS := $(BIN_DIR)/$(EXENAME)
SRCS := $(shell find $(SRC_DIR) -type f -iname "*.c")
INCS := $(shell find $(INC_DIR) -type f -iname "*.h")
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

CFLAGS := -g -Wall -Wextra -Werror
LDFLAGS := -lpthread


.PHONY: all clean distclean

all: $(BINS)

release: CFLAGS := $(filter-out -g,$(CFLAGS))
release: CFLAGS := -O3 $(CFLAGS)
release: all

$(BINS): $(OBJS) | $(BIN_DIR)/
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(INCS) | $(OBJ_DIR)/
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN_DIR)/ $(OBJ_DIR)/:
	mkdir -p $@

clean:
	rm -f $(BINS)
	rm -f $(OBJS)

distclean:
ifneq ("$(BIN_DIR)",".")
	rm -rf $(BIN_DIR)
endif
ifneq ("$(OBJ_DIR)",",")
	rm -rf $(OBJ_DIR)
endif

