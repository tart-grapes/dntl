# Makefile for Ring-Switching System Library

CC = gcc
CFLAGS = -Wall -Wextra -O3 -march=native
LDFLAGS = -lcrypto -lm

# Source files
SRCS = rs_prf.c rs_params.c rs_mats.c rs_lwr.c rs_test.c
OBJS = $(SRCS:.c=.o)

# Headers
HEADERS = rs_config.h rs_prf.h rs_params.h rs_mats.h rs_lwr.h

# Target executable
TARGET = rs_test

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $<

# Clean
clean:
	rm -f $(OBJS) $(TARGET)

# Run tests
test: $(TARGET)
	./$(TARGET)

.PHONY: all clean test
