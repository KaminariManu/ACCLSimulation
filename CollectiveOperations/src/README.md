# ACCL Packet Processor Modular Library

## 1. Overview

This directory contains the source code for the `accl_packet_processor`, a modular C library designed to simulate and process network packets for a custom accelerator (ACCL). The original monolithic C file has been refactored into a series of focused, maintainable, and cross-platform modules.

The library is designed to be highly portable and can be compiled on Windows (via MinGW), Linux, and macOS. It has no external dependencies beyond the standard C library and is suitable for bare-metal environments.

## 2. Directory Structure

The modular library is organized in the following structure:

```
CollectiveOperations/
├── src/                        # Modular library source files
│   ├── accl_byteorder.c       # Endianness management
│   ├── accl_memory.c          # SoC memory simulation
│   ├── accl_operations.c      # Collective operation handlers
│   ├── accl_packet_loader.c   # Packet trace loading
│   ├── accl_packet_processor.c # Main controller
│   ├── accl_utils.c           # Bare-metal utilities
│   ├── include/               # Header files
│   ├── Makefile               # Build system
│   └── README.md              # This file
├── demo/                       # Demonstration application
│   ├── demo_packet_processor.c # Demo program source
│   ├── demo_modular           # Compiled executable (after build)
│   └── README.md              # Demo documentation
└── archive_original/           # Original monolithic implementation (legacy)
    ├── accl_packet_processor.c # Monolithic C file (1775 lines)
    ├── accl_packet_processor.h # Monolithic header file
    └── README.md              # Archive documentation
```

## 3. Module Breakdown

The library is divided into several modules, each with a specific responsibility.

### 3.1. `accl_types.h` - Core Data Structures

This header file is the foundation of the library, providing all central data structures, enumerations, and constants used across the modules.

#### `ACCLPacketHeader` (64 bytes)
Defines the structure of a packet header, which is consistent for all ACCL packets.
```c
typedef struct {
    uint16_t protocol_number;    // Magic number: 0xACCE
    uint16_t version;            // Protocol version
    uint32_t packet_type;        // Type: EAGER, RNDZV_ADDR, etc.
    uint32_t operation;          // Operation: SEND, BROADCAST, etc.
    uint32_t src_rank;           // Source node rank
    uint32_t dst_rank;           // Destination node rank
    uint32_t tag;                // Message matching tag
    uint32_t session_id;         // Unique ID for an operation instance
    uint32_t sequence_number;    // Packet sequence number
    uint32_t data_length;        // Payload length in bytes
    uint32_t timestamp;          // Simulation timestamp
    uint32_t flags;              // Control flags
    uint64_t address;            // 64-bit address for RDMA
    uint32_t payload_segment;    // Current segment number for large messages
    uint32_t total_segments;     // Total segments in the message
    uint32_t checksum;           // Data integrity checksum
} ACCLPacketHeader;
```

#### `ACCLNodeState`
This structure holds the entire state of a single simulated node (rank).
```c
typedef struct {
    uint32_t rank;              // This node's rank
    uint32_t num_ranks;         // Total ranks in the communicator
    ACCLStatistics stats;       // Global statistics for this node
    
    // State tracking for ongoing collective operations
    CollectiveOpState collective_ops[MAX_COLLECTIVE_OPS];
    uint32_t num_collective_ops;
    
    // State tracking for multi-segment point-to-point messages
    PendingMessage pending_msgs[MAX_PENDING_MSGS];
    uint32_t num_pending_msgs;
    
    // Synchronization counters for BARRIER operations
    uint32_t barrier_entered;
    uint32_t barrier_exited;
    
    // Simulated on-chip memory system
    SoCMemory memory;
} ACCLNodeState;
```

### 3.2. `accl_utils.c` - Bare-metal Utilities
- **Purpose**: Provides custom, portable implementations of standard C library functions to ensure the code can run in bare-metal environments without a full `libc`.
- **Key Functions**:
    - `bm_memset()`: Fills a block of memory with a specific byte.
    - `bm_memcpy()`: Copies a block of memory.
    - `bm_strlen()`: Calculates the length of a string.
    - `bm_strncmp()`: Compares two strings up to a specified length.
    - `bm_strcspn()`: Finds the first character in a string that is part of a specified set of characters.

### 3.3. `accl_byteorder.c` - Endianness Management
- **Purpose**: Handles byte order conversions to ensure data from network traces (which is in big-endian format) is correctly interpreted on the host machine (which may be little-endian, like x86).
- **Key Functions**:
    - `is_little_endian()`: Runtime check for the host system's endianness.
    - `bm_ntohl()`, `bm_ntohs()`, `bm_be64toh()`: Convert 32-bit, 16-bit, and 64-bit values from network to host order.
    - `accl_ntoh_packet_header()`: A crucial function that safely converts all fields of an incoming packet header to the correct host byte order.

### 3.4. `accl_memory.c` - On-Chip Memory (SoC) Simulation
- **Purpose**: Simulates the fast, on-chip memory (SRAM or HBM) that an accelerator would use for temporary data storage during collective operations.
- **Features**:
    - **Memory Pool**: Manages a simulated 256 MB memory space.
    - **Buffer Management**: Handles dynamic allocation and deallocation of buffers for operations like `REDUCE` and `GATHER`.
    - **In-Memory Reduction**: Implements logic to perform reduction operations (e.g., SUM, MAX) directly on data stored in the simulated memory buffers.
- **Key Functions**:
    - `accl_init_memory()`: Initializes the memory simulation.
    - `accl_allocate_buffer()`: Allocates a new buffer for an operation.
    - `accl_find_buffer()`: Locates a buffer associated with a specific operation session.
    - `accl_reduce_buffer_data()`: Performs an in-place reduction (e.g., sum, max) on the data within a buffer.
    - `accl_print_memory_stats()`: Reports detailed statistics on memory usage.

### 3.5. `accl_packet_loader.c` - Packet Trace Loading
- **Purpose**: Responsible for reading packet traces from files and feeding them to the processor.
- **Features**:
    - **Dual Format Support**: Loads packets from both compact binary (`.bin`) files and human-readable hexadecimal (`.txt`) files.
    - **Stream Processing**: Processes packets one by one, simulating how a real network interface would handle incoming traffic. This is highly memory-efficient (O(1) memory usage).
    - **RDMA Simulation**: Correctly handles RDMA (Remote Direct Memory Access) packets by skipping their payload, mimicking how a real DMA engine would bypass the main processor.

### 3.6. `accl_operations.c` - Collective & P2P Operation Logic
- **Purpose**: This is the core logic engine of the simulator. It contains handlers that implement the specific communication patterns for each ACCL operation.
- **Operations Handled**:
    - **Point-to-Point (`accl_handle_send_recv`)**: Manages `SEND`/`RECV` operations, including the eager protocol for small messages and the three-phase rendezvous protocol for large messages.
    - **One-to-All (`accl_handle_broadcast`, `accl_handle_scatter`)**: Implements data distribution from a root node to all other nodes.
    - **All-to-One (`accl_handle_gather`, `accl_handle_reduce`)**: Implements data collection at a root node. The `REDUCE` handler uses the SoC memory module to perform in-place data reduction.
    - **All-to-All (`accl_handle_allgather`, `accl_handle_allreduce`, etc.)**: Implements complex patterns where all nodes participate in a global data exchange. `ALLREDUCE` is a two-phase operation combining a reduce-scatter and an all-gather.
    - **Synchronization (`accl_handle_barrier`)**: Implements a barrier to synchronize all nodes.
- **State Management**: Uses helper functions (`accl_find_or_create_collective_op`) to track the state of operations that span multiple packets.

### 3.7. `accl_packet_processor.c` - Main Controller
- **Purpose**: Acts as the top-level coordinator for the entire library.
- **Responsibilities**:
    - **Node Lifecycle**: Initializes the node state (`accl_init_node`) and cleans up resources (`accl_free_node`).
    - **Packet Dispatch**: The main `accl_process_packet` function acts as a dispatcher, inspecting each packet's header and routing it to the appropriate handler in the `accl_operations.c` module.
    - **Statistics**: Aggregates and prints detailed statistics about the simulation run, including packet counts, data volumes, and protocol usage.

## 4. How It Works: The Packet Processing Pipeline

1.  **Initialization**: A user application calls `accl_init_node()` to create a state object for the node being simulated.
2.  **Packet Loading**: The application then calls `accl_load_packets_from_binary()` or `accl_load_packets_from_hex()`.
3.  **Stream Processing**: The loader reads one packet at a time from the file. For each packet, it:
    a. Reads the binary data into an `ACCLPacket` struct.
    b. Calls `accl_ntoh_packet_header()` to ensure all header fields are in the correct byte order for the host system.
    c. Calls `accl_process_packet()`.
4.  **Dispatch**: `accl_process_packet()` updates global statistics and uses a `switch` statement on the packet's `operation` field to call the correct handler (e.g., `accl_handle_broadcast`).
5.  **Operation Handling**: The specific handler updates the state of the ongoing operation. For multi-packet operations, it tracks progress (e.g., segments received, participants arrived). For reduction operations, it uses the `accl_memory` module to combine data.
6.  **Finalization**: After all packets are processed, the application calls `accl_print_statistics()` and `accl_print_memory_stats()` to see the results of the simulation, and `accl_free_node()` to release all memory.

## 5. Build and Run

A `Makefile` is provided for easy compilation. The demo application (`demo_packet_processor.c`) is located in the `demo/` directory and uses the modular library in `src/`.

### Quick Start

From the `src/` directory:

```bash
# Build the library and demo executable
make

# Run the demo with a sample packet trace (simulates node 3)
make run
```

### Makefile Targets
-   **`make` or `make all`**: Compiles all library source files and links them with `../demo/demo_packet_processor.c` to create the executable `demo_modular` in the `demo/` directory.
-   **`make run`**: Builds and executes the demo program with a sample packet trace for node 3.
-   **`make compile`**: Compiles the library modules to object files without linking.
-   **`make clean`**: Removes all build artifacts (object files and the executable).
-   **`make help`**: Displays available make targets.

### Manual Compilation

To compile the library and link it with the demo program manually:

```bash
# From the src/ directory, compile all library modules
gcc -Wall -Wextra -Wpedantic -std=c99 -Iinclude -c *.c

# Link with the demo application
gcc -Wall -Wextra -Wpedantic -std=c99 -Iinclude ../demo/demo_packet_processor.c *.o -o ../demo/demo_modular

# Run the demo (example for node 3 of an 8-node system)
cd ../demo
./demo_modular 3 ../../PacketTraceGenerator/output_single_node/accl_packet_trace_node3.bin
```

### Running the Demo

The demo program requires two arguments:
1. **Node rank**: The rank of the node to simulate (0 to N-1)
2. **Trace file**: Path to the packet trace file (binary `.bin` or hex `.txt` format)

```bash
# From the demo/ directory:
# Simulate node 3 processing a binary trace file
./demo_modular 3 ../../PacketTraceGenerator/output_single_node/accl_packet_trace_node3.bin

# Simulate node 0 processing a hex trace file
./demo_modular 0 ../../PacketTraceGenerator/output_single_node/accl_packets_raw_hex_node0.txt
```
