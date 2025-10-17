# ACCL Collective Operations - Modular C Implementation

This directory contains a modular C implementation for processing ACCL (Alveo Collective Communication Library) packet traces. It simulates the behavior of a network-connected System-on-Chip (SoC) processing engine for high-performance computing collective operations.

The implementation is designed to be **bare-metal compatible**, with no external dependencies, making it suitable for embedded systems, FPGAs, and educational purposes.

## Project Structure

The project is organized into a modular library (`src`), a demonstration application (`demo`), and the original monolithic code (`archive_original`).

```
CodiceElaborato/
├── PacketTraceGenerator/
│   ├── generate_packet_trace.py      # Python script to generate packet traces
│   └── output_single_node/           # Default output for trace files
│
└── CollectiveOperations/             # ← You are here
    ├── src/                          # The core modular library
    │   ├── accl_byteorder.c
    │   ├── accl_memory.c
    │   ├── accl_operations.c
    │   ├── accl_packet_loader.c
    │   ├── accl_packet_processor.c
    │   ├── accl_utils.c
    │   ├── include/                  # All public and private header files
    │   └── Makefile                  # The build system for the library
    │
    ├── demo/                         # Demonstration application
    │   ├── demo_packet_processor.c   # Demo source code
    │   ├── demo_modular.exe          # Compiled executable (build artifact)
    │   └── README.md                 # README for the demo
    │
    ├── build/                        # Directory for build artifacts
    │   └── obj/                      # Compiled object files (*.o)
    │
    ├── archive_original/             # Legacy monolithic implementation
    │
    └── README.md                     # This file
```

## Core Library Modules (`src/`)

The functionality is broken down into several focused modules:

- **`accl_packet_processor.c`**: The main engine. It initializes a node, processes packets sequentially, and dispatches tasks to the appropriate handlers.
- **`accl_operations.c`**: Implements the logic for all 11 MPI-style collective operations (e.g., `Allreduce`, `Broadcast`, `Gather`).
- **`accl_memory.c`**: Simulates a 256MB on-chip memory space. It manages buffer allocation, deallocation, and reduction operations for collective algorithms.
- **`accl_packet_loader.c`**: Handles loading packet traces from binary or hex-formatted files.
- **`accl_byteorder.c`**: Manages endianness detection and conversion (host-to-network and network-to-host) to ensure data portability.
- **`accl_utils.c`**: Contains bare-metal compatible utility functions (`memcpy`, `strlen`, etc.) to eliminate dependencies on standard libraries.

## Quick Start: Build and Run

Follow these steps to build the library and run the demonstration.

### Prerequisites

- **GCC Compiler**: Ensure `gcc` is installed and in your system's PATH. On Windows, this is typically provided by **MinGW** or **MSYS2**.
- **Make Tool**:
  - On Linux/macOS: `make`
  - On Windows (with MinGW/MSYS2): `mingw32-make`

### 1. Generate Packet Traces

If you haven't already, generate the simulation input files.

```sh
cd ../PacketTraceGenerator
# Ensure you have a Python environment with dependencies from environment.yml
python generate_packet_trace.py
```

### 2. Build the Executable

Navigate to the `src` directory and run the make command.

```sh
cd src
# On Windows
mingw32-make

# On Linux/macOS
make
```

This will:
1. Compile all `.c` files in `src/` into object files in `build/obj/`.
2. Link the object files with `demo/demo_packet_processor.c`.
3. Create the final executable at `demo/demo_modular` (or `demo_modular.exe`).

### 3. Run the Demo

From the `demo` directory, run the executable with the desired node rank and trace file.

```sh
cd ../demo

# Example: Run simulation for node 3 using its binary trace file
./demo_modular 3 ../../PacketTraceGenerator/output_single_node/accl_packet_trace_node3.bin
```

The output will show a detailed log of packet processing, memory usage, and final statistics.

## Key Features

- **Modular Architecture**: Clean separation of concerns between packet processing, collective operations, memory management, and utilities.
- **Bare-Metal Ready**: Zero dependencies on standard libraries like `string.h` or `endian.h`, making it highly portable for embedded targets.
- **Accurate Protocol Simulation**: Faithfully implements ACCL's Eager and Rendezvous protocols for different message sizes.
- **Comprehensive Statistics**: Provides detailed reports on packet counts, data volumes, protocol usage, and memory utilization.
- **Stateful Processing**: Tracks the state of concurrent collective operations, simulating a real hardware processor.
- **Cross-Platform Build**: A single `Makefile` supports Windows, Linux, and macOS.

---

**For detailed API documentation, operation descriptions, and troubleshooting, see `README_C_PROCESSOR.md`**
