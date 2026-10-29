# Makefile for Online Food Ordering System (C HTTP Web Server)

CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c99
TARGET = server

# Detect Operating System
ifeq ($(OS),Windows_NT)
    LIBS = -lws2_32
    TARGET_BIN = $(TARGET).exe
    RM = del /Q /F
else
    LIBS = 
    TARGET_BIN = $(TARGET)
    RM = rm -f
endif

all: $(TARGET_BIN)

$(TARGET_BIN): server.c server.h
	$(CC) $(CFLAGS) server.c -o $(TARGET_BIN) $(LIBS)
	@echo "Build successful! Run ./$(TARGET_BIN) to start the server."

clean:
	$(RM) $(TARGET_BIN)
	@echo "Cleaned build artifacts."

run: $(TARGET_BIN)
	./$(TARGET_BIN)

.PHONY: all clean run
