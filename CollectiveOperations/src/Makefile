# Makefile for ACCL Packet Processor (Modular Version)
# Supports Windows (MinGW/GCC), Linux, and macOS

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c99 -O2
INCLUDES = -Iinclude

# Directories
SRC_DIR = .
INCLUDE_DIR = include
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

# Source files (only .c files in current directory)
SRCS = $(wildcard *.c)
OBJS = $(SRCS:%.c=$(OBJ_DIR)/%.o)

# Demo program (in demo directory)
DEMO_SRC = ../demo/demo_packet_processor.c
DEMO_EXE = ../demo/demo_modular

# Default target
all: $(DEMO_EXE)

# Create build directories
$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR) 2>/dev/null || mkdir $(OBJ_DIR) >nul 2>&1

# Compile source files to object files
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Link demo executable
$(DEMO_EXE): $(OBJS) $(DEMO_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) $(DEMO_SRC) $(OBJS) -o $(DEMO_EXE)

# Clean build artifacts
clean:
	@rm -rf $(BUILD_DIR) $(DEMO_EXE) 2>/dev/null || (rmdir /s /q $(BUILD_DIR) 2>nul & del /q $(DEMO_EXE).exe 2>nul)

# Compile only (no linking)
compile: $(OBJS)

# Run demo with sample trace
run: $(DEMO_EXE)
	cd .. && ./demo_modular 3 ../PacketTraceGenerator/output_single_node/accl_packet_trace_node3.bin

# Show help
help:
	@echo "Available targets:"
	@echo "  all      - Build demo executable (default)"
	@echo "  compile  - Compile source files only"
	@echo "  clean    - Remove build artifacts"
	@echo "  run      - Build and run demo with sample trace"
	@echo "  help     - Show this help message"

.PHONY: all clean compile run help
