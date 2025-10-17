# ACCL Packet Trace Generator & Modular C Processor

This project provides a complete ecosystem for simulating and analyzing ACCL (Alveo Collective Communication Library) network traffic. It consists of two main components:
1.  **A Python-based Packet Trace Generator** that produces realistic, configurable ACCL traffic.
2.  **A modular, bare-metal compatible C implementation** that processes these traces, simulating a stateful hardware engine.

This combination allows for end-to-end testing, from traffic generation to processing and analysis, and serves as an educational tool for understanding high-performance computing collective operations.

## Project Structure

```
CodiceElaborato/
│
├── PacketTraceGenerator/               # Python-based trace generation system
│   ├── main.py                         # Main execution entry point for the modular generator
│   ├── src/                            # Modular source code for the generator
│   ├── generate_packet_trace.py        # Legacy single-file generator script
│   ├── output_single_node/             # Default output for per-node filtered traces
│   └── README.md                       # Detailed documentation for the generator
│
└── CollectiveOperations/               # C-based modular packet processor
    ├── src/                            # The core modular library
    │   ├── accl_operations.c           # Implements all 11 collective operations
    │   ├── accl_packet_processor.c     # The main processing engine
    │   ├── accl_memory.c               # Simulates 256MB of on-chip memory
    │   └── Makefile                    # Build system for the library and demo
    │
    ├── demo/                           # Demonstration application
    │   └── demo_packet_processor.c     # Demo source code that uses the library
    │
    ├── build/                          # Directory for build artifacts (*.o files)
    │
    ├── archive_original/               # Legacy monolithic C implementation
    │
    └── README.md                       # Detailed documentation for the C processor
```

## End-to-End Workflow

The project follows a simple, two-stage workflow:

1.  **Generate Traces**: The `PacketTraceGenerator` creates binary trace files (`.bin`) that represent the sequence of network packets for a simulated workload.
2.  **Process Traces**: The `CollectiveOperations` C application reads a binary trace file, processes each packet sequentially, and simulates the behavior of an ACCL-aware hardware engine, including memory management and state tracking.

```
PacketTraceGenerator/              CollectiveOperations/
─────────────────────              ─────────────────────
      main.py           →         demo/demo_modular
     (generates)                         (processes)
         ↓                                    ↑
   output_single_node/                        │
   (packet traces)       ─────────────────────┘
```

## 🚀 Quick Start

Follow these steps to generate traces and run the C processor.

### 1. Generate Packet Traces

Navigate to the `PacketTraceGenerator` directory and run the Python script.

```sh
cd PacketTraceGenerator

# Generate a default trace (8 ranks, 50 operations)
# This creates trace files in the output_single_node/ directory
python main.py
```
*For more advanced generation options (e.g., changing the number of ranks or traffic mix), see the `PacketTraceGenerator/README.md`.*

### 2. Build the C Processor

Navigate to the `src` directory within `CollectiveOperations` and use the provided `Makefile`.

```sh
cd ../CollectiveOperations/src

# On Windows (with MinGW or MSYS2)
mingw32-make

# On Linux/macOS
make
```
This compiles the library and links it with the demo application, creating an executable at `../demo/demo_modular`.

### 3. Run the Simulation

From the `demo` directory, run the executable, pointing it to a trace file.

```sh
cd ../demo

# Example: Run simulation for node 3 using its binary trace file
./demo_modular 3 ../../PacketTraceGenerator/output_single_node/accl_packet_trace_node3.bin
```
The processor will execute the trace and print detailed statistics about the operations performed, data transferred, and memory used.

## Components in Detail

### Packet Trace Generator (`PacketTraceGenerator/`)

A highly configurable tool for creating realistic ACCL network traffic.

-   **Modular & Extensible**: Built with a clean architecture in `src/` for easy maintenance and extension.
-   **Supports All 11 ACCL Operations**: Models everything from `Send`/`Recv` to complex collectives like `Allreduce`.
-   **Multiple Output Formats**: Generates human-readable logs, raw hex for encapsulation, and compact binary files for efficient processing.
-   **Configurable Traffic Mix**: Adjust command-line weights (e.g., `--weight-allreduce`) to simulate different workloads, from general HPC to ML training.
-   **Reproducible**: Use the `--seed` argument to generate the exact same trace every time.

### Modular C Processor (`CollectiveOperations/`)

A bare-metal compatible C application that simulates a stateful ACCL packet processing engine.

-   **Modular Architecture**: Functionality is cleanly separated into a core library (`src/`) and a demonstration app (`demo/`). Key modules handle packet processing, collective operation logic, memory simulation, and low-level utilities.
-   **Bare-Metal Ready**: Written in pure C99 with **zero external dependencies** (no `string.h`, `endian.h`, etc.), making it suitable for embedded systems and FPGAs.
-   **SoC Memory Simulation**: Features a simulated 256MB on-chip memory for managing operation buffers and performing actual in-memory reduction operations.
-   **Stateful Processing**: Tracks the state of up to 128 concurrent collective operations, just like real hardware.
-   **Cross-Platform**: A single `Makefile` supports building on Windows (MinGW), Linux, and macOS.

## Requirements

### For Trace Generation (Python)
-   Python 3.6+
-   No external libraries required.

### For Trace Processing (C)
-   A C99-compatible compiler (GCC recommended).
-   `make` (or `mingw32-make` on Windows).

## License

This project is part of an educational assignment for embedded systems.

## Acknowledgments

This project is based on the ACCL (Alveo Collective Communication Library) protocol specification. The reference ACCL implementation files that inspired this work can be found in the official Xilinx repository: <https://github.com/Xilinx/ACCL>.
