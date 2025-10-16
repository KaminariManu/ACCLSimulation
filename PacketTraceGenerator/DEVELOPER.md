# ACCL Packet Trace Generator - Developer Guide

This document provides detailed information about the modular architecture, extension points, and development guidelines.

## Architecture Overview

The generator uses a **modular design** with clear separation of concerns:

```
PacketTraceGenerator/
├── main.py                          # Entry point
├── src/                             # Modular source code
│   ├── __init__.py                  # Package initialization
│   ├── accl_types.py                # Enumerations and constants
│   ├── packet.py                    # Packet data structure
│   ├── trace_generator.py           # Core generation logic
│   ├── collective_operations.py     # Collective communication operations
│   ├── output_writers.py            # Multiple output format writers
│   └── cli.py                       # Command-line argument parsing
├── generate_packet_trace.py         # Legacy version (backward compatible)
├── README.md                        # User documentation
├── DEVELOPER.md                     # This file
└── environment.yml                  # Conda environment
```

### Design Principles

1. **Single Responsibility:** Each module has one clear purpose
2. **Loose Coupling:** Modules communicate through well-defined interfaces
3. **High Cohesion:** Related functionality grouped together
4. **Extensibility:** Easy to add new operations or formats
5. **Testability:** Each module can be tested independently

## Module Details

### 1. `accl_types.py` (42 lines)

**Purpose:** Define protocol constants and enumerations

**Location:** `src/accl_types.py`

```python
from src.accl_types import AcclPacketType, AcclOperation, AcclProtocol

# Usage
packet_type = AcclPacketType.DATA_EAGER
operation = AcclOperation.SEND
protocol = AcclProtocol.EAGER
```

**Key Components:**
- `AcclPacketType`: DATA_EAGER, RNDZV_ADDR, RNDZV_DATA, RNDZV_COMPLETE, COLLECTIVE_DATA
- `AcclOperation`: SEND, RECV, BROADCAST, SCATTER, GATHER, REDUCE, etc.
- `AcclProtocol`: EAGER (≤32KB), RENDEZVOUS (>32KB)

**Extension Points:**
- Add new packet types for custom protocols
- Add new operations for domain-specific patterns
- Define custom protocol variants

### 2. `packet.py` (161 lines)

**Purpose:** Represent individual ACCL packets with serialization

**Location:** `src/packet.py`

```python
from src.packet import Packet

# Create packet
p = Packet(
    packet_type=AcclPacketType.DATA_EAGER,
    operation=AcclOperation.SEND,
    src_rank=0,
    dst_rank=1,
    tag=12345,
    session_id=1,
    sequence_num=0,
    data_length=4096,
    timestamp=0,
    payload=b'\x00' * 4096
)

# Serialize
binary = p.to_binary()      # 64-byte header + payload
hex_str = p.to_hex()        # Hex string (no prefix)
readable = str(p)           # Human-readable
```

**Packet Structure:**
- **Header:** 64 bytes, all fields in network byte order (big-endian)
- **Payload:** Variable length (up to 4KB per segment)
- **Checksum:** Simple sum of all bytes (mod 2^32)

**Key Methods:**
- `to_binary()`: Pack to binary format for simulation
- `to_hex()`: Convert to hex string for encapsulation
- `calculate_checksum()`: Compute integrity checksum
- `__str__()`: Human-readable representation

**Extension Points:**
- Add custom header fields (update `to_binary()`)
- Implement alternative checksum algorithms
- Add encryption/compression layers

### 3. `trace_generator.py` (239 lines)

**Purpose:** Core generation logic with operation scheduling

**Location:** `src/trace_generator.py`

```python
from src.trace_generator import ACCLTraceGenerator

# Create generator
gen = ACCLTraceGenerator(
    num_ranks=8,
    max_packet_size=4096,
    datapath_width=64,
    eager_threshold=32768,
    seed=42
)

# Generate trace
packets = gen.generate_trace(num_operations=100)
```

**Key Components:**

1. **Operation Scheduler:**
   - Weighted random selection of operations
   - Maintains operation history
   - Ensures valid network state

2. **Protocol Selection:**
   ```python
   def select_protocol(data_size):
       return EAGER if data_size <= 32768 else RENDEZVOUS
   ```

3. **State Management:**
   - Sequence numbers per connection pair
   - Session ID tracking
   - Timestamp generation

**Operation Weights:**
```python
operations = [
    (self.generate_send_recv_pair, 30),  # 30% point-to-point
    (lambda: generate_broadcast(self), 10),
    (lambda: generate_scatter(self), 8),
    (lambda: generate_gather(self), 8),
    (lambda: generate_reduce(self), 8),
    (lambda: generate_allgather(self), 8),
    (lambda: generate_allreduce(self), 10),
    (lambda: generate_reduce_scatter(self), 6),
    (lambda: generate_barrier(self), 6),
    (lambda: generate_alltoall(self), 6),
]
```

The weights in this list are chosen to simulate a **typical, general-purpose workload** you might find in High-Performance Computing (HPC) or distributed machine learning applications. They are not based on a single, strict benchmark but are a reasonable approximation based on common communication patterns. Here's a breakdown of the reasoning:

1.  **Highest Weight: Point-to-Point (30%)**
    *   `generate_send_recv_pair` is the most fundamental communication pattern. Many algorithms rely heavily on direct data exchanges between pairs of nodes. Giving it the highest weight reflects its foundational and frequent use.

2.  **High Weight: Common Collectives (10% each)**
    *   `generate_broadcast`: Extremely common for distributing initial data, configurations, or model parameters from a root node to all workers.
    *   `generate_allreduce`: This is the cornerstone of modern distributed machine learning for aggregating gradients. Its high frequency in ML workloads justifies a significant weight.

3.  **Medium Weight: Standard Building-Block Collectives (8% each)**
    *   `generate_scatter`, `generate_gather`, `generate_reduce`, and `generate_allgather` are all essential MPI-style collective operations. They are used in a wide variety of parallel algorithms but might appear slightly less frequently than the primary `broadcast` or `allreduce` in some common workloads. Grouping them with a similar, moderate weight makes sense.

4.  **Lower Weight: Specialized or Less Frequent Collectives (6% each)**
    *   `generate_reduce_scatter` and `generate_alltoall`: These are often more specialized or communication-intensive operations that may not be used as frequently in all applications.
    *   `generate_barrier`: This is for synchronization. While critical, it doesn't move data and is often used more sparingly than data communication calls (e.g., at the end of major computation phases).

These values are **configurable via command-line arguments**, allowing you to easily simulate different application workloads without modifying code. For example, to simulate a workload more focused on ML training, you can increase the weight for `allreduce`:

```bash
python main.py --ranks 8 --num-operations 100 --weight-allreduce 60 --weight-sendrecv 20
```

**Programmatic API:**

When using the generator as a library, you can also pass custom weights:

```python
from src.trace_generator import ACCLTraceGenerator

generator = ACCLTraceGenerator(num_ranks=8)

custom_weights = {
    'sendrecv': 30,
    'broadcast': 10,
    'scatter': 8,
    'gather': 8,
    'reduce': 8,
    'allgather': 8,
    'allreduce': 10,
    'reduce_scatter': 6,
    'barrier': 6,
    'alltoall': 6,
}

packets = generator.generate_trace(num_operations=100, operation_weights=custom_weights)
```

For comprehensive documentation on customizing operation weights, including detailed examples and workload profiles, see the **Operation Weights** sections in this guide and in the main README.md.

**Extension Points:**
- Add new operations to the operation list
- Implement custom scheduling policies
- Add application-specific traffic patterns

### 4. `collective_operations.py` (316 lines)

**Purpose:** Implement all collective communication patterns

**Location:** `src/collective_operations.py`

**Operations Implemented:**

1. **Broadcast** (`generate_broadcast`):
   - Pattern: Root → All ranks
   - Segments: Ring-based for large data

2. **Scatter** (`generate_scatter`):
   - Pattern: Root sends unique data to each rank
   - Chunk size: data_size / num_ranks

3. **Gather** (`generate_gather`):
   - Pattern: All ranks → Root
   - Assembles data at root

4. **Reduce** (`generate_reduce`):
   - Pattern: All ranks → Root (with reduction)
   - Ring-based for large networks

5. **Allgather** (`generate_allgather`):
   - Pattern: All ranks exchange data
   - Ring-based: N-1 steps

6. **Allreduce** (`generate_allreduce`):
   - Pattern: Reduce-scatter + Allgather
   - Two-phase algorithm

7. **Reduce-Scatter** (`generate_reduce_scatter`):
   - Pattern: Reduce and distribute chunks
   - Ring-based

8. **Barrier** (`generate_barrier`):
   - Pattern: Synchronization
   - Minimal packets (acknowledgments)

9. **Alltoall** (`generate_alltoall`):
   - Pattern: All-to-all exchange
   - Personalized communication

**Implementation Pattern:**

```python
def generate_my_collective(generator):
    """Generate packets for custom collective operation.
    
    Args:
        generator: ACCLTraceGenerator instance
    
    Returns:
        List of Packet objects
    """
    packets = []
    root = random.randint(0, generator.num_ranks - 1)
    data_size = random.randint(1024, 32768)
    session_id = generator.next_session_id()
    
    # Implement communication pattern
    for rank in range(generator.num_ranks):
        if rank != root:
            packet = create_packet(...)
            packets.append(packet)
    
    return packets
```

**Extension Points:**
- Add new collective algorithms (e.g., tree-based broadcast)
- Implement custom reduction operations
- Optimize for specific network topologies

### 5. `output_writers.py` (338 lines)

**Purpose:** Write packets in multiple formats

**Location:** `src/output_writers.py`

**Output Formats:**

1. **Text Format** (`write_text_trace`):
   - Human-readable with statistics
   - Includes header with configuration
   - Sample packets for debugging

2. **Raw Hex Format** (`write_raw_hex_trace`):
   - **Primary format for encapsulation**
   - One packet per line (hex string only)
   - No headers, comments, or separators
   - Ready for TCP/UDP/Ethernet wrapping

3. **Detailed Hex Format** (`write_detailed_hex_trace`):
   - Hex with field breakdowns
   - Shows header structure
   - Useful for debugging

4. **Binary Format** (`write_binary_trace`):
   - Native binary packets
   - For simulation tools
   - Efficient storage

**Usage:**

```python
from src.output_writers import (
    write_text_trace,
    write_raw_hex_trace,
    write_detailed_hex_trace,
    write_binary_trace
)

packets = generator.generate_trace(100)

# Write all formats
write_text_trace(packets, "trace.txt", config)
write_raw_hex_trace(packets, "packets_raw.txt")
write_detailed_hex_trace(packets, "packets_detailed.txt")
write_binary_trace(packets, "trace.bin")
```

**Extension Points:**
- Add new output formats (JSON, CSV, PCAP, etc.)
- Implement compression
- Add metadata/annotations

### 6. `cli.py` (108 lines)

**Purpose:** Command-line interface and argument parsing

**Location:** `src/cli.py`

```python
from src.cli import parse_arguments

args = parse_arguments()
# args.ranks, args.num_operations, args.seed, etc.
```

**Argument Groups:**

1. **Network Configuration:**
   - `--ranks`: Number of nodes
   - `--num-operations`: Operations to generate
   - `--seed`: Random seed

2. **Hardware Parameters:**
   - `--max-packet-size`: Maximum packet size
   - `--datapath-width`: Hardware datapath width
   - `--eager-threshold`: Protocol threshold

3. **Operation Weights:**
   - `--weight-sendrecv`: Weight for point-to-point operations (default: 30)
   - `--weight-broadcast`: Weight for broadcast operations (default: 10)
   - `--weight-scatter`: Weight for scatter operations (default: 8)
   - `--weight-gather`: Weight for gather operations (default: 8)
   - `--weight-reduce`: Weight for reduce operations (default: 8)
   - `--weight-allgather`: Weight for allgather operations (default: 8)
   - `--weight-allreduce`: Weight for allreduce operations (default: 10)
   - `--weight-reduce-scatter`: Weight for reduce-scatter operations (default: 6)
   - `--weight-barrier`: Weight for barrier operations (default: 6)
   - `--weight-alltoall`: Weight for alltoall operations (default: 6)

4. **Output Control:**
   - `--output-all`: All nodes directory
   - `--output-node`: Single node directory
   - `--node`: Node to filter
   - `--skip-binary`, `--skip-hex`: Skip formats
   - `--verbose`: Show sample packets

**Key Functions:**
- `parse_arguments()`: Parses all CLI arguments
- `get_operation_weights(args)`: Extracts operation weights from arguments as a dictionary
- `print_configuration()`: Displays configuration including operation weight percentages
- `print_summary()`: Shows generation summary and output files
- `print_verbose_samples()`: Prints sample packets in verbose mode

**Extension Points:**
- Add new command-line options
- Implement configuration files (JSON/YAML)
- Add interactive mode

### 7. `main.py` (93 lines)

**Purpose:** Entry point that orchestrates generation

```python
def main():
    # Parse arguments
    args = parse_arguments()
    
    # Create generator
    generator = ACCLTraceGenerator(...)
    
    # Generate packets
    packets = generator.generate_trace(...)
    
    # Write outputs
    write_text_trace(...)
    write_raw_hex_trace(...)
    # etc.
```

**Flow:**
1. Parse command-line arguments
2. Create generator with configuration
3. Generate packet trace
4. Filter for single node (if requested)
5. Write all output formats
6. Create output directories

**Extension Points:**
- Add pipeline stages (validation, optimization)
- Add progress reporting

### 8. `__init__.py` (35 lines)

**Purpose:** Package initialization and public API

**Location:** `src/__init__.py`

```python
# Public API
from src.trace_generator import ACCLTraceGenerator
from src.packet import Packet
from src.accl_types import AcclPacketType, AcclOperation, AcclProtocol

# Create generator
gen = ACCLTraceGenerator()
packets = gen.generate_trace(50)
```

**Exports:**
- `ACCLTraceGenerator`: Main class
- `Packet`: Packet representation
- All enumerations from `accl_types`

## Common Extension Patterns

### Adding a New Collective Operation

**Step 1:** Implement in `src/collective_operations.py`

```python
def generate_my_operation(generator):
    """My custom collective operation."""
    packets = []
    root = random.randint(0, generator.num_ranks - 1)
    data_size = random.randint(1024, 65536)
    session_id = generator.next_session_id()
    
    # Phase 1: Root broadcasts metadata
    for rank in range(generator.num_ranks):
        if rank != root:
            packet = Packet(
                packet_type  = AcclPacketType.COLLECTIVE_DATA,
                operation    = AcclOperation.BROADCAST,
                src_rank     = root,
                dst_rank     = rank,
                tag          = 0xFFFFFFFF,
                session_id   = session_id,
                sequence_num = generator.get_next_sequence(root, rank),
                data_length  = 64,
                timestamp    = generator.next_timestamp(),
                payload      = b'\x00' * 64
            )
            packets.append(packet)
    
    # Phase 2: Your custom logic
    # ...
    
    return packets
```

**Step 2:** Add to operation list in `src/trace_generator.py`

```python
operations = [
    # Existing operations...
    (lambda: generate_my_operation(self), 5),  # 5% probability
]
```

## Performance Considerations

### Memory Usage

- Each packet: ~64 bytes (header) + payload size
- 10,000 packets with 4KB payloads ≈ 40 MB
- Consider streaming output for large traces

### Generation Speed

- ~100,000 packets/second on typical hardware
- Bottleneck: Random number generation
- Optimization: Pre-generate random choices

### Output File Sizes

| Format | Overhead | Use Case |
|--------|----------|----------|
| Text | 5-10x | Human debugging |
| Raw Hex | 2x | Encapsulation |
| Detailed Hex | 8-15x | Protocol analysis |
| Binary | 1x | Simulation (smallest) |

## Migration Guide

### From Legacy to Modular

**Legacy code:**
```bash
python generate_packet_trace.py --ranks 8 --num-operations 50
```

**Modular equivalent:**
```bash
python main.py --ranks 8 --num-operations 50
```

**As library (modular):**
```python
from src.trace_generator import ACCLTraceGenerator

gen = ACCLTraceGenerator(num_ranks=8)
packets = gen.generate_trace(50)
```

### Compatibility

- Both versions accept same command-line arguments
- Output files are identical
- Legacy version maintained for backward compatibility
- **Recommendation:** Use modular version for new projects


### Error Handling

```python
def generate_trace(self, num_operations):
    if num_operations <= 0:
        raise ValueError("num_operations must be positive")
    
    if self.num_ranks < 2:
        raise ValueError("num_ranks must be at least 2")
    
    # Generation logic...
```

### Documentation

- Document all public functions/classes
- Include usage examples
- Explain non-obvious design decisions
- Keep README.md updated

## Roadmap

## References

- **ACCL Protocol:** [Xilinx ACCL Documentation]
- **MPI Collective Operations:** MPI Standard 4.0
- **Packet Encapsulation:** RFC 894 (Ethernet), RFC 791 (IP)

For user documentation, see [README.md](README.md)