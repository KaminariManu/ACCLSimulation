# Makefile for ACCL Packet Processor
# This builds the ACCL packet processor library and demo program

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11
LDFLAGS = 

# Target executables
DEMO = demo_packet_processor

# Object files
OBJS = accl_packet_processor.o demo_packet_processor.o

# Default target
all: $(DEMO)

# Build demo executable
$(DEMO): $(OBJS)
	$(CC) $(CFLAGS) -o $(DEMO) $(OBJS) $(LDFLAGS)
	@echo ""
	@echo "Build complete!"
	@echo "Run with: ./$(DEMO) <node_rank> <packet_file>"
	@echo "Example:  ./$(DEMO) 3 ../PacketTraceGenerator/output_single_node/accl_packet_trace_node3.bin"

# Compile source files
accl_packet_processor.o: accl_packet_processor.c accl_packet_processor.h
	$(CC) $(CFLAGS) -c accl_packet_processor.c

demo_packet_processor.o: demo_packet_processor.c accl_packet_processor.h
	$(CC) $(CFLAGS) -c demo_packet_processor.c

# Clean build artifacts
clean:
	rm -f $(OBJS) $(DEMO)
	@echo "Clean complete"

# Install (copy to system)
install: $(DEMO)
	@echo "Installing to /usr/local/bin (requires sudo)"
	sudo cp $(DEMO) /usr/local/bin/

# Uninstall
uninstall:
	@echo "Removing from /usr/local/bin (requires sudo)"
	sudo rm -f /usr/local/bin/$(DEMO)

# Generate packet traces first (requires Python)
generate-traces:
	@echo "Generating packet traces with Python..."
	cd ../PacketTraceGenerator && python3 generate_packet_trace.py
	@echo "Packet traces generated in PacketTraceGenerator/output_all_nodes/ and PacketTraceGenerator/output_single_node/"

# Full workflow: generate traces then build
full: generate-traces all

# Run demo with binary file
run-binary: $(DEMO)
	@echo "Running demo with binary file..."
	@if [ -f ../PacketTraceGenerator/output_single_node/*.bin ]; then \
		./$(DEMO) $$(ls ../PacketTraceGenerator/output_single_node/*.bin | head -1 | grep -oP 'node\K[0-9]+') ../PacketTraceGenerator/output_single_node/*.bin; \
	else \
		echo "Error: No binary files found. Run 'make generate-traces' first."; \
	fi

# Run demo with hex file
run-hex: $(DEMO)
	@echo "Running demo with hex file..."
	@if [ -f ../PacketTraceGenerator/output_single_node/*_raw_hex*.txt ]; then \
		./$(DEMO) $$(ls ../PacketTraceGenerator/output_single_node/*.bin | head -1 | grep -oP 'node\K[0-9]+') ../PacketTraceGenerator/output_single_node/*_raw_hex*.txt; \
	else \
		echo "Error: No hex files found. Run 'make generate-traces' first."; \
	fi

# Help
help:
	@echo "ACCL Packet Processor - Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all              - Build the demo program (default)"
	@echo "  clean            - Remove build artifacts"
	@echo "  generate-traces  - Generate packet traces using Python script"
	@echo "  full             - Generate traces and build"
	@echo "  run-binary       - Run demo with generated binary file"
	@echo "  run-hex          - Run demo with generated hex file"
	@echo "  install          - Install to /usr/local/bin (requires sudo)"
	@echo "  uninstall        - Remove from /usr/local/bin (requires sudo)"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  make full        # Generate traces and build"
	@echo "  make run-binary  # Run with binary file"

.PHONY: all clean install uninstall generate-traces full run-binary run-hex help
