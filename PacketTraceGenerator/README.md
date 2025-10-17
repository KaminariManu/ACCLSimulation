# ACCL Packet Trace Generator: A Tool for Network Traffic Simulation


## 1. Introduction



The ACCL Packet Trace Generator is a configurable software tool designed to produce realistic network traffic traces based on the Alveo Collective Communication Library (ACCL) protocol. It supports a comprehensive set of point-to-point and collective communication operations, with automatic packet segmentation to respect hardware constraints (default: 4096-byte maximum packet size).

The generator is implemented in Python 3 and is available in two versions: a modular, extensible version and a legacy single-file script. Both versions produce identical, byte-accurate output traces in multiple formats suitable for a range of simulation and analysis tools.



## 2. Core Functionality


### 2.1. Supported Communication Primitives 
The generator creates packets in multiple formats including hexadecimal format ready for encapsulation in transport protocols (TCP/UDP) or Ethernet frames.


The generator models the following ACCL communication operations:

- **Point-to-Point Operations:**# Generate trace with defaults (8 ranks, 50 operations)

  - `Send`/`Receive` using the Eager protocol for small messages (≤ 32 KB).

  - `Send`/`Receive` using the Rendezvous protocol for large messages (> 32 KB), which involves an address exchange followed by an RDMA (Remote Direct Memory Access) transfer.python main.py## Quick Start



- **Collective Operations:**

  - **Data Distribution:** `Broadcast`, `Scatter`

  - **Data Gathering:** `Gather`, `Allgather`

  - **Reduction Operations:** `Reduce`, `Allreduce`, `Reduce-Scatter`

  - **Synchronization:** `Barrier`

  - **Permutation:** `All-to-All`



### 2.2. Output Trace Formats

### Custom configuration

The tool generates four distinct output files, each serving a different purpose:

1.  **Human-Readable Text (`accl_packet_trace.txt`):** A detailed, formatted log containing configuration parameters, summary statistics, and a human-readable breakdown of each packet. Ideal for debugging and manual inspection.

2.  **Raw Hexadecimal (`accl_packets_raw_hex.txt`):** Contains one hexadecimal string per packet, representing the raw byte content. This format is designed for direct ingestion by network simulators or for encapsulation into transport layer (TCP/UDP) or link layer (Ethernet) frames.

3.  **Detailed Hexadecimal (`accl_packets_hex_detailed.txt`):** Provides a hexadecimal dump of each packet with annotated field breakdowns, facilitating low-level protocol analysis.

4.  **Binary (`accl_packet_trace.bin`):** A compact, byte-for-byte binary representation of the packet trace, suitable for high-performance simulation environments.



## 3. System Architecture

The primary version of the generator is architected in a modular fashion to promote maintainability, extensibility, and reusability. The source code is organized within the `src/` directory, with a comprehensive test suite in the `tests/` directory.

```
PacketTraceGenerator/
├── main.py                        # Main execution entry point
├── src/                           # Directory for modular source code
│   ├── __init__.py                # Initializes the 'src' package
│   ├── accl_types.py              # Defines enumerations for packet types and operations
│   ├── packet.py                  # Contains the Packet data structure and serialization logic
│   ├── trace_generator.py         # Core engine for trace generation
│   ├── collective_operations.py   # Implements logic for collective communication patterns
│   ├── output_writers.py          # Contains functions for writing different output formats
│   └── cli.py                     # Handles command-line argument parsing
├── tests/                         # Comprehensive test suite (72 tests, >90% coverage)
│   ├── __init__.py                # Package initialization
│   ├── README.md                  # Detailed testing documentation
│   ├── run_tests.py               # Test runner with statistics
│   ├── test_packet.py             # Unit tests for Packet class
│   ├── test_trace_generator.py    # Unit tests for ACCLTraceGenerator
│   ├── test_collective_operations.py  # Tests for all collective operations
│   ├── test_output_writers.py     # Tests for output file generation
│   └── test_integration.py        # End-to-end integration tests
├── generate_packet_trace.py       # Legacy single-file script for backward compatibility
├── README.md                      # This document
└── DEVELOPER.md                   # Guide for developers
```

This modular design allows individual components, such as the collective operation implementations or output formats, to be modified or extended with minimal impact on the rest of the system. The test suite ensures that all changes maintain correctness and compatibility.

## 4. Usage

### 4.1. System Requirements

-   Python 3.6 or higher.
-   No external libraries are required; the tool relies solely on the Python standard library.

### 4.2. Command-Line Interface

The generator is controlled via a command-line interface with a range of configurable parameters.

**Basic Usage:**

```bash
# Generate a default trace (8 ranks, 50 operations)
python main.py

# Display all available options
python main.py --help
```

**Configurable Parameters:**

-   **Network Configuration:**
    -   `--ranks`: Number of network nodes (ranks).
    -   `--num-operations`: Total number of communication operations to generate.
    -   `--seed`: Seed for the random number generator to ensure reproducibility.
-   **Hardware and Protocol Parameters:**
    -   `--max-packet-size`: The maximum size of a packet data payload in bytes (default: 4096).
    -   `--eager-threshold`: The message size threshold for switching between the Eager and Rendezvous protocols (default: 32768 bytes).
-   **Operation Weights (Traffic Mix Customization):**
    -   `--weight-sendrecv`: Relative frequency of point-to-point send/recv operations (default: 30).
    -   `--weight-broadcast`: Relative frequency of broadcast operations (default: 10).
    -   `--weight-scatter`: Relative frequency of scatter operations (default: 8).
    -   `--weight-gather`: Relative frequency of gather operations (default: 8).
    -   `--weight-reduce`: Relative frequency of reduce operations (default: 8).
    -   `--weight-allgather`: Relative frequency of allgather operations (default: 8).
    -   `--weight-allreduce`: Relative frequency of allreduce operations (default: 10).
    -   `--weight-reduce-scatter`: Relative frequency of reduce-scatter operations (default: 6).
    -   `--weight-barrier`: Relative frequency of barrier operations (default: 6).
    -   `--weight-alltoall`: Relative frequency of alltoall operations (default: 6).
    
    **Note:** Weights are relative values that determine the probability distribution of operations. The actual percentage for each operation is calculated as `(weight / sum_of_all_weights) × 100`. Default values simulate a typical general-purpose HPC/ML workload.
-   **Output Configuration:**
    -   `--output-all`: Directory for storing the complete network trace (default: `output_all_nodes`).
    -   `--output-node`: Directory for storing a trace filtered for a single node (default: `output_single_node`).
    -   `--node`: The specific rank for which to generate a single-node trace.
    -   `--skip-binary`, `--skip-hex`: Flags to disable generation of specific output formats.
    
    **Important:** The generator automatically cleans (removes and recreates) the output directories before each run to ensure fresh trace generation without stale files. Additionally, the `src/__pycache__/` directory is cleaned to prevent issues with outdated bytecode.

**Example with Custom Parameters:**

```bash
# Generate a trace for a 32-node network with 1000 operations and a specific seed
python main.py --ranks 32 --num-operations 1000 --seed 12345

# Generate a trace simulating an ML training workload (allreduce-heavy)
python main.py --ranks 16 --num-operations 500 \
    --weight-allreduce 60 \
    --weight-sendrecv 20 \
    --weight-broadcast 10

# Generate a trace with balanced collective operations
python main.py --ranks 8 --num-operations 200 \
    --weight-sendrecv 20 \
    --weight-broadcast 10 \
    --weight-scatter 10 \
    --weight-gather 10 \
    --weight-allreduce 15 \
    --weight-allgather 10
```

### 4.3. Customizing Traffic Patterns

The generator allows you to customize the mix of communication operations to simulate different application workloads:

-   **Default Workload (General HPC/ML):** The default weights represent a typical mixed workload with dominant point-to-point communication (30%), balanced collectives, and frequent allreduce operations (10%) common in machine learning.

-   **ML Training Workload:** For distributed deep learning, increase `--weight-allreduce` significantly (e.g., 60%) as gradient aggregation dominates communication.

-   **Data Distribution Workload:** For applications focused on data distribution and collection, increase `--weight-scatter` and `--weight-gather`.

-   **Point-to-Point Heavy:** For message-passing applications, increase `--weight-sendrecv` to 70-80%.

For detailed guidance on choosing appropriate weights, including rationale and examples, refer to **Section 5.3: Operation Weight Distribution** below.

## 5. Protocol and Packet Structure

### 5.1. ACCL Packet Header

Each ACCL packet is preceded by a 64-byte header containing metadata essential for routing, sequencing, and processing. All integer fields are encoded in network byte order (big-endian).

| Offset (Bytes) | Size (Bytes) | Field          | Description                                      |
| :------------- | :----------- | :------------- | :----------------------------------------------- |
| 0-1            | 2            | Protocol       | Protocol identifier (0xACCE).                    |
| 2-3            | 2            | Version        | Protocol version.                                |
| 4-7            | 4            | Packet Type    | The type of packet (e.g., `DATA_EAGER`, `RNDZV_ADDR`). |
| 8-11           | 4            | Operation      | The communication primitive (e.g., `SEND`, `BROADCAST`). |
| 12-15          | 4            | Source Rank    | The originating node's rank.                     |
| 16-19          | 4            | Dest Rank      | The destination node's rank.                     |
| 20-23          | 4            | Tag            | Message matching identifier.                     |
| 24-27          | 4            | Session ID     | Identifier for a multi-packet communication session. |
| 28-31          | 4            | Sequence #     | Per-connection sequence number for ordering.     |
| 32-35          | 4            | Data Length    | Length of the payload in bytes.                  |
| 36-39          | 4            | Timestamp      | Simulation timestamp.                            |
| 40-43          | 4            | Flags          | Flags for special handling (e.g., compression).  |
| 44-51          | 8            | Address        | 64-bit address for Rendezvous (RDMA) transfers.  |
| 52-55          | 4            | Reserved       | Reserved for future use.                         |
| 56-57          | 2            | Segment #      | The sequence number of the current packet segment. |
| 58-59          | 2            | Total Segments | The total number of segments for the message.    |
| 60-63          | 4            | Checksum       | A simple checksum for data integrity.            |

### 5.2. Communication Protocols

-   **Eager Protocol:** For messages up to the `eager-threshold`, data is sent directly in one or more `DATA_EAGER` packets. Messages larger than `max-packet-size` are automatically segmented into multiple packets.
-   **Rendezvous Protocol:** For larger messages, a three-phase handshake is used:
    1.  The receiver sends its memory buffer address to the sender in an `RNDZV_ADDR` packet.
    2.  The sender performs an RDMA write and sends the data in an `RNDZV_DATA` packet.
    3.  The sender concludes the transfer with an `RNDZV_COMPLETE` notification.

**Packet Segmentation:**

All communication operations (point-to-point and collective) automatically segment large messages into chunks that respect the `max-packet-size` limit (default: 4096 bytes). This ensures compatibility with realistic network hardware constraints:

- **Point-to-Point Operations:** Eager send/recv operations segment messages using the `DATA_EAGER` packet type with proper segment numbering.
- **Collective Operations:** All collectives (broadcast, scatter, gather, reduce, allgather, allreduce, reduce-scatter, and alltoall) use the `COLLECTIVE_DATA` packet type and segment their data transfers when necessary.
- **Segment Tracking:** Each packet header contains the current segment number (`payload_segment`) and total number of segments (`total_segments`) to enable proper reassembly at the receiver.

This segmentation is handled transparently by the generator and ensures that all generated packets conform to the specified maximum packet size, making the traces suitable for hardware simulation and actual network deployment.

### 5.3. Operation Weight Distribution

The generator uses a weighted random selection mechanism to determine which communication operation to execute next. This approach allows realistic simulation of different application workload profiles.

**Default Weight Distribution:**

The default weights simulate a typical general-purpose HPC/ML workload:

| Operation       | Default Weight | Typical Percentage | Rationale                                        |
| :-------------- | :------------- | :----------------- | :----------------------------------------------- |
| Send/Recv       | 30             | 30%                | Foundational point-to-point communication        |
| Broadcast       | 10             | 10%                | Common for data distribution                     |
| Allreduce       | 10             | 10%                | Critical for ML gradient aggregation             |
| Scatter         | 8              | 8%                 | Standard data distribution pattern               |
| Gather          | 8              | 8%                 | Standard data collection pattern                 |
| Reduce          | 8              | 8%                 | Common reduction operation                       |
| Allgather       | 8              | 8%                 | Frequent all-to-all data exchange                |
| Reduce-Scatter  | 6              | 6%                 | Specialized collective operation                 |
| Barrier         | 6              | 6%                 | Synchronization (less data-intensive)            |
| Alltoall        | 6              | 6%                 | Personalized communication pattern               |

**Customization Philosophy:**

1.  **Highest Weight (Point-to-Point):** Direct node-to-node communication forms the foundation of most distributed applications and receives the highest default weight (30%).

2.  **High Weight (Common Collectives):** Operations like `broadcast` and `allreduce` are cornerstones of modern distributed machine learning for parameter distribution and gradient aggregation, warranting significant weights (10% each).

3.  **Medium Weight (Standard Building Blocks):** Essential MPI-style collective operations such as `scatter`, `gather`, `reduce`, and `allgather` are fundamental building blocks used across diverse parallel algorithms (8% each).

4.  **Lower Weight (Specialized Operations):** More specialized operations like `reduce_scatter`, synchronization primitives like `barrier`, and complex patterns like `alltoall` are used more sparingly in typical workloads (6% each).

These weights can be fully customized via command-line arguments to match specific application profiles. For example, distributed deep learning training would significantly increase the `allreduce` weight, while data-parallel preprocessing might emphasize `scatter` and `gather` operations.

## 6. Testing

The ACCL Packet Trace Generator includes a comprehensive test suite to ensure correctness, reliability, and maintainability. The test suite achieves over 90% code coverage and validates all core functionality.

### 6.1. Test Structure

The test suite is organized in the `tests/` directory:

```
tests/
├── __init__.py                    # Package initialization
├── README.md                      # Detailed testing documentation
├── run_tests.py                   # Test runner script
├── .gitignore                     # Ignores test outputs and cache
├── test_packet.py                 # Unit tests for Packet class (10 tests)
├── test_trace_generator.py        # Unit tests for ACCLTraceGenerator (14 tests)
├── test_collective_operations.py  # Tests for all collective operations (27 tests)
├── test_output_writers.py         # Tests for file output functions (11 tests)
└── test_integration.py            # End-to-end integration tests (10 tests)
```

**Total Test Coverage:** 72 test methods across 20 test classes

### 6.2. Running Tests

#### Run All Tests

```bash
# Using unittest discovery
python -m unittest discover tests -v

# Using the provided test runner (with summary statistics)
python tests/run_tests.py
```

#### Run Specific Test Files

```bash
# Test only the Packet class
python -m unittest tests.test_packet -v

# Test only collective operations
python -m unittest tests.test_collective_operations -v

# Test only output writers
python -m unittest tests.test_output_writers -v
```

#### Run Specific Test Classes or Methods

```bash
# Run a specific test class
python -m unittest tests.test_packet.TestPacketCreation -v

# Run a specific test method
python -m unittest tests.test_packet.TestPacketCreation.test_basic_packet_creation -v
```

### 6.3. Test Categories

#### Unit Tests

- **Packet Tests (`test_packet.py`)**: Validates packet creation, serialization to binary and hexadecimal formats, all packet types (DATA_EAGER, RNDZV_ADDR, RNDZV_DATA, COLLECTIVE_DATA), segmentation, and field encoding.

- **Trace Generator Tests (`test_trace_generator.py`)**: Tests generator initialization, session/sequence ID management, timestamp generation, deterministic trace generation with seeds, and custom operation weights.

- **Collective Operations Tests (`test_collective_operations.py`)**: Comprehensive validation of all 10 collective communication patterns: broadcast, scatter, gather, reduce, allgather, allreduce, reduce-scatter, barrier, and alltoall. Verifies packet counts, routing patterns, and protocol correctness.

- **Output Writers Tests (`test_output_writers.py`)**: Tests binary and hexadecimal file output, automatic directory creation, file format validation, and large trace handling.

#### Integration Tests

- **End-to-End Tests (`test_integration.py`)**: Validates complete workflows including multi-node trace generation, scalability testing (up to 64 ranks), different configuration combinations, and realistic workload simulations.

### 6.4. Test Features

- **Deterministic Testing**: All tests use fixed random seeds to ensure reproducible results across runs and environments.

- **Edge Case Coverage**: Tests include boundary conditions such as single-rank systems, maximum packet sizes, empty operations, and extreme configurations.

- **Performance Testing**: Scalability tests validate generator performance with large numbers of ranks (64+) and operations (1000+).

- **Output Validation**: File format tests verify byte-accurate binary serialization and correct hexadecimal encoding.

- **Automatic Cleanup**: Test outputs are automatically cleaned up and ignored via `.gitignore`.

### 6.5. Test Execution Time

- **Full Test Suite**: ~45-50 seconds
- **Unit Tests Only**: ~20-25 seconds
- **Integration Tests**: ~25-30 seconds

### 6.6. Continuous Validation

Developers are encouraged to run the test suite after making changes:

```bash
# Quick validation during development
python -m unittest discover tests

# Full validation with coverage details
python tests/run_tests.py
```

For detailed information about test implementation, common issues, and troubleshooting, refer to `tests/README.md`.

## 7. Conclusion

The ACCL Packet Trace Generator is a versatile and extensible tool for researchers and engineers working on network simulation, performance analysis, and protocol design. Its ability to generate a wide variety of realistic traffic patterns in multiple formats makes it a valuable asset for understanding and optimizing high-performance communication systems. The modular architecture ensures that the tool can be readily adapted to future research needs and evolving network protocols.

The comprehensive test suite provides confidence in the correctness and reliability of the generator, with over 90% code coverage and validation of all communication primitives, protocols, and output formats.

