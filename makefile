# project name
PROJ_NAME = mcmvs
CC = gcc
# CFLAGS
SRC = src
BIN = bin
INCLUDE = include

UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),x86_64)
    CFLAGS = -Wall -I./include -g
endif
ifeq ($(UNAME_M),arm64)
    CFLAGS = -Wall -I./include -arch arm64 -g
endif
INCLUDE_PATHS = -I/opt/homebrew/include
LINKER_FLAGS = -L/opt/homebrew/lib -lSDL2 -lSDL2_ttf
DEPS = $(wildcard $(INCLUDE)/*.h)
SRCS = $(wildcard $(SRC)/*.c)
OBJS = $(patsubst %.c, %.o, $(SRCS))

$(SRC)/%.o: $(SRC)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(INCLUDE_PATHS)

all: setup program

setup:
	@echo "[i] Setting up project..."
	@mkdir -p $(BIN)
	@mkdir -p $(INCLUDE)
	@echo "[i] Setup complete"

program: clean $(OBJS)
	@echo "[i] Compiling program..."
	$(CC) $(CFLAGS) $(OBJS) -o $(BIN)/$(PROJ_NAME) $(INCLUDE_PATHS) $(LINKER_FLAGS)
	@echo "[i] Compilation complete"

clean: setup
	@echo "[i] Cleaning up..."
	@$(RM) -rf $(BIN)/*
	@$(RM) -rf $(OBJS)
	@echo "[i] Cleanup complete"