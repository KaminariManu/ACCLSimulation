Original Monolithic Implementation Archive
==========================================

This directory contains the original, monolithic (single-file) implementation of the ACCL Packet Processor. These files are considered legacy and have been archived for historical reference and for comparison against the new version.

**This code is deprecated and should not be used for new development.**

The new, modular implementation is located in the `../src/` directory.

Contents of This Archive
-------------------------
- **accl_packet_processor.h** - Original header file with function declarations
- **accl_packet_processor.c** - Original monolithic implementation (1775 lines)
- **README_C_PROCESSOR.md** - Original documentation for the monolithic code
- **README.md** - This file

**Note**: The demonstration program (`demo_packet_processor.c`) has been moved to `../demo/` and is now used with the modular implementation in `../src/`.

Functionality of the Monolithic Implementation
----------------------------------------------
The `accl_packet_processor.c` file (1775 lines) is a self-contained C program that:

- Reads and processes ACCL (Alveo Collective Communication Library) packet traces.
- Supports both binary and hex-formatted trace files.
- Simulates network behavior by processing packets in a stream without buffering.
- Implements handlers for 11 different collective operations, including:
  - Point-to-point: Send/Receive
  - One-to-all: Broadcast, Scatter
  - All-to-one: Gather, Reduce
  - All-to-all: Allgather, Allreduce, Alltoall
- Manages both Eager (for small messages) and Rendezvous (for large messages) communication protocols.
- Tracks and displays comprehensive statistics about the processed trace, such as packet counts, data volumes, and operation types.

Reason for Archival
-------------------
This implementation was replaced by a modular version to improve maintainability, readability, and compilation speed. While functionally identical to the new version, its monolithic nature makes it difficult to extend and debug.

Rollback Instructions (Monolithic Implementation Only)
------------------------------------------------------
If you need to restore this original monolithic version for testing or comparison:

  1. Copy `accl_packet_processor.h` to a working directory.
  2. Copy `accl_packet_processor.c` to the same directory.
  3. Copy `../demo/demo_packet_processor.c` to the same directory.
  4. Modify the demo's include statement from `#include "../src/include/accl_packet_processor.h"` to `#include "accl_packet_processor.h"`.
  5. Compile using:
     ```bash
     gcc -o demo_original demo_packet_processor.c accl_packet_processor.c
     ```
  6. Run with a packet trace:
     ```bash
     ./demo_original 3 ../PacketTraceGenerator/output_single_node/accl_packet_trace_node3.bin
     ```

**For the current modular implementation**, see `../src/README.md` and use the demo in `../demo/`.

---
Archive created as part of a code modularization project.
Original code is preserved for historical reference and verification purposes.
