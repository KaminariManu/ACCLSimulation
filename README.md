# ACCL Packet Trace Generator & Processor

Complete implementation of ACCL (Alveo Collective Communication Library) packet trace generation and processing.

## Project Structure

```
CodiceElaborato/
│
├── ACCL code/                          # Reference ACCL implementation from https://github.com/Xilinx/ACCL
│   ├── ccl_offload_control.c          # Original ACCL hardware implementation
│   └── ccl_offload_control.h          # Reference ACCL header
│
├── PacketTraceGenerator/               # Python trace generation
│   ├── generate_packet_trace.py       # Main generator script
│   ├── test_packet.py                 # Verification script
│   ├── output_all_nodes/              # Complete network traces
│   └── output_single_node/            # Per-node filtered traces
│
└── 📂 CollectiveOperations/            # C trace processor
    ├── accl_packet_processor.c        # Implementation (~600 lines)
    ├── accl_packet_processor.h        # Header file
    ├── demo_packet_processor.c        # Demo program
    ├── build.bat / build.ps1          # Windows build scripts
    ├── Makefile                       # Unix/Linux build
    └── Documentation files
```

## 🚀 Quick Start

### 1. Generate Packet Traces

```bash
cd PacketTraceGenerator
python generate_packet_trace.py
```

This creates packet traces for all ACCL collective operations in both binary and hex formats.

### 2. Build C Processor

**Windows:**
```cmd
cd ..\CollectiveOperations
build.bat
```

**Linux/WSL:**
```bash
cd ../CollectiveOperations
make
```

### 3. Process Traces

**Windows:**
```cmd
demo_packet_processor.exe 3 ..\PacketTraceGenerator\output_single_node\accl_packet_trace_node3.bin
```

**Linux/WSL:**
```bash
./demo_packet_processor 3 ../PacketTraceGenerator/output_single_node/accl_packet_trace_node3.bin
```

## Components

### PacketTraceGenerator
Python-based trace generation system that creates realistic ACCL packet traces:
- Supports all 11 ACCL collective operations: Send/Recv, Broadcast, Scatter, Gather, Reduce, Allgather, Allreduce, Reduce_Scatter, Barrier, Alltoall
- Generates traces in multiple formats: Binary (.bin), Hex (.txt), Raw Hex (for encapsulation)
- Creates both complete network traces and per-node filtered traces
- Configurable parameters: NUM_RANKS and NUM_OPERATIONS
- No external dependencies, uses Python standard library only

### CollectiveOperations
C-based packet processor with stateful simulation capabilities that mirrors ccl_offload_control.c functionality:

**Core Features:**
- Stream processing: Processes packets one-at-a-time as they arrive (like real network)
- Memory efficient: O(1) memory usage with no buffering
- Format support: Reads both binary and hex trace files
- Protocol handling: Eager protocol (≤32KB) and Rendezvous protocol (>32KB)
- Cross-platform: Windows, Linux, macOS support

**State Tracking Architecture:**
- Session-based tracking: Monitors up to 128 concurrent collective operations
- Message tracking: Manages up to 64 pending point-to-point messages
- Progress monitoring: Tracks segments received and bytes processed per operation
- Completion detection: Identifies when multi-packet operations finish
- Phase identification: Distinguishes operation phases (e.g., reduce-scatter vs allgather in allreduce)

**Operation Handlers:**
Each collective operation implements accurate ACCL semantics:
- Point-to-Point: Send/Recv with tag matching and segment tracking
- Broadcast: Root-to-all distribution with cumulative byte tracking
- Scatter: Chunk distribution monitoring
- Gather: Ring-based collection with root/relay identification
- Reduce: Ring-based reduction progress tracking
- Allgather: Ring circulation tracking
- Allreduce: Two-phase operation (reduce-scatter + allgather) with phase detection
- Reduce_Scatter: Combined reduction and scattering
- Barrier: Entry/exit synchronization with rank counting
- Alltoall: All-to-all personalized exchange tracking

**Statistics:**
- Packet counts by operation and protocol type
- Total data volume tracking
- Directional byte counters (bytes sent vs bytes received)
- Per-operation completion tracking
- Protocol efficiency metrics

## Documentation

- **PacketTraceGenerator/README.md** - Comprehensive generator documentation with encapsulation examples
- **CollectiveOperations/README.md** - Quick start guide for the C processor
- **CollectiveOperations/README_C_PROCESSOR.md** - Detailed technical documentation including state tracking architecture and handler implementations

## Requirements

### For Trace Generation (Python)
- Python 3.6+
- Standard library only (no external dependencies)

### For Trace Processing (C)
- C compiler (GCC, Clang, MSVC)
- C11 standard support
- Windows: MinGW recommended
- Linux: build-essential package

## Example Workflow

```bash
# 1. Generate traces
cd PacketTraceGenerator
python generate_packet_trace.py

# 2. Build processor
cd ../CollectiveOperations
make  # or build.bat on Windows

# 3. Process binary trace (recommended - faster)
./demo_packet_processor 3 ../PacketTraceGenerator/output_single_node/accl_packet_trace_node3.bin

# 4. Process hex trace (for debugging)
./demo_packet_processor 3 ../PacketTraceGenerator/output_single_node/accl_packet_trace_node3.txt

# 5. Process all nodes trace
./demo_packet_processor 0 ../PacketTraceGenerator/output_all_nodes/accl_packet_trace.bin
```

## Technical Details

### Packet Structure
- **Header Size**: 64 bytes (exactly matching DATAPATH_WIDTH_BYTES)
- **Byte Order**: Big-endian (network order)
- **Protocol Number**: 0xACCE

### Operations Implemented
1. **Send/Receive** - Point-to-point communication
2. **Broadcast** - One-to-all distribution
3. **Scatter** - Distribute chunks to all ranks
4. **Gather** - Collect chunks from all ranks
5. **Reduce** - Aggregation with operation
6. **Allgather** - Gather + broadcast result
7. **Allreduce** - Reduce + broadcast result
8. **Reduce_Scatter** - Reduce + scatter result
9. **Barrier** - Synchronization
10. **Alltoall** - Complete exchange
11. **Combined** - Multiple operations

### Protocols
- **Eager**: Messages ≤32KB, direct transfer with segmentation
- **Rendezvous**: Messages >32KB, 3-phase handshake (address exchange → RDMA transfer → completion)

### State Tracking Structures

**CollectiveOpState**: Tracks each collective operation with session ID, operation type, progress counters, and completion status

**PendingMessage**: Manages point-to-point messages with tag matching, peer identification, and segment counting

**ACCLNodeState**: Maintains per-node state including statistics, collective operation array (128 max), pending message array (64 max), and barrier counters

## Key Features

- Exact ACCL compatibility: Matches hardware packet format precisely
- Binary compatible: Generated traces work with real ACCL implementations
- Stateful simulation: Tracks operation progress and completion like real hardware
- Semantic accuracy: Handlers implement actual collective communication patterns
- Comprehensive testing: Test script verifies 64-byte header structure
- Well documented: Extensive README files with implementation details
- Cross-platform support: Works on Windows, Linux, macOS
- Educational value: Clear code structure demonstrating ACCL internals

## Integration with ACCL

This project is designed to work with the ACCL library:
- Reference implementation in `ACCL code/ccl_offload_control.c`
- Packet format matches ACCL hardware exactly
- Can generate test vectors for ACCL testing
- Useful for understanding ACCL internals

## Project Statistics

- **Python Code**: ~1200 lines (trace generation)
- **C Code**: ~900 lines (trace processing with state tracking)
- **Documentation**: ~2000 lines (comprehensive README files)
- **Supported Operations**: 11 collective operations
- **File Formats**: 3 (binary, hex, raw hex)
- **Platforms**: Windows, Linux, macOS, WSL
- **State Capacity**: 128 concurrent collective operations, 64 pending messages

## Development

### Modify Trace Generation
Edit `PacketTraceGenerator/generate_packet_trace.py`:
- Change `NUM_RANKS` - Number of participating nodes
- Change `NUM_OPERATIONS` - Operations per type
- Modify operation generators for custom patterns

### Modify Processor
Edit `CollectiveOperations/accl_packet_processor.c`:
- Add custom operation handlers with state tracking
- Modify statistics collection
- Extend packet processing logic
- Adjust state capacity limits (MAX_COLLECTIVE_OPS, MAX_PENDING_MSGS)

## License

This project is part of an educational assignment for embedded systems.

## Acknowledgments

This project is based on the ACCL (Alveo Collective Communication Library) protocol specification. The reference ACCL implementation files in the `ACCL code` directory are sourced from the official Xilinx repository: \url{https://github.com/Xilinx/ACCL}.

---

For detailed usage instructions, see the README files in each directory.
