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
├── tests/                           # Comprehensive test suite (72 tests)
│   ├── __init__.py                  # Package initialization
│   ├── README.md                    # Testing documentation
│   ├── run_tests.py                 # Test runner with statistics
│   ├── test_packet.py               # Unit tests for Packet class
│   ├── test_trace_generator.py      # Unit tests for ACCLTraceGenerator
│   ├── test_collective_operations.py # Tests for all collective operations
│   ├── test_output_writers.py       # Tests for output file generation
│   └── test_integration.py          # End-to-end integration tests
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
5. **Testability:** Each module can be tested independently (>90% test coverage)

## Testing

The project includes a comprehensive test suite with 72 test methods across 20 test classes, achieving over 90% code coverage.

### Test Organization

- **Unit Tests:**
  - `test_packet.py`: Packet class, serialization, all packet types (10 tests)
  - `test_trace_generator.py`: Generator initialization, deterministic generation (14 tests)
  - `test_collective_operations.py`: All 10 collective operations validation (27 tests)
  - `test_output_writers.py`: File output, format validation (11 tests)

- **Integration Tests:**
  - `test_integration.py`: End-to-end workflows, scalability (10 tests)

### Running Tests

```bash
# Run all tests
python -m unittest discover tests -v

# Run specific test file
python -m unittest tests.test_packet -v

# Run with statistics (using test runner)
python tests/run_tests.py
```

### Test-Driven Development

When adding new features:
1. Write tests first in appropriate test file
2. Implement feature
3. Run tests to verify: `python -m unittest discover tests -v`
4. Ensure all 72 tests pass before committing

For detailed testing documentation, see `tests/README.md`.

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
- **Payload:** Variable length (up to 4KB per segment for regular packets)
- **Checksum:** Simple sum of all bytes (mod 2^32)

**Key Methods:**
- `to_binary()`: Pack to binary format for simulation
- `to_hex()`: Convert to hex string for encapsulation
- `_generate_payload()`: Generate packet payload (optimized for RDMA)
- `calculate_checksum()`: Compute integrity checksum
- `__str__()`: Human-readable representation

**Payload Generation Strategy:**

The `_generate_payload()` method uses different strategies based on packet type:

```python
def _generate_payload(self) -> bytes:
    # RDMA packets: zero-filled (fast, represents DMA transfer)
    if self.packet_type == PacketType.RNDZV_DATA:
        return bytes(self.data_length)  # Zero-filled placeholder
    
    # Regular packets: random data for realistic simulation
    return bytes([random.randint(0, 255) for _ in range(self.data_length)])
```

**Rationale:**
- **RDMA packets (`RNDZV_DATA`):** Represent logical direct memory access operations that bypass packet buffers. The payload is not actually transmitted through packet processing, so we use zero-filled placeholders. This provides significant performance improvement for large message traces.
- **Regular packets:** Generate random payload data to simulate realistic network traffic patterns.

**Extension Points:**
- Add custom header fields (update `to_binary()`)
- Implement alternative checksum algorithms
- Add encryption/compression layers
- Customize payload patterns for specific workload simulation

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
   
   **Eager Protocol:** Messages ≤32KB are sent directly using `DATA_EAGER` packets. Messages >4KB are automatically segmented into multiple packets.
   
   **Rendezvous Protocol:** Messages >32KB use a three-phase handshake for RDMA:
   - Phase 1: Receiver sends `RNDZV_ADDR` packet with buffer address (~32 bytes)
   - Phase 2: Sender generates `RNDZV_DATA` packet representing RDMA transfer (full message size, NOT segmented)
   - Phase 3: Sender sends `RNDZV_COMPLETE` notification
   
   **IMPORTANT:** `RNDZV_DATA` packets represent logical RDMA operations that bypass packet processing. The `data_length` field contains the full RDMA transfer size (can be >4KB), as these transfers write directly to the destination memory address via DMA hardware.

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
   - Uses segmentation for large messages

**Packet Segmentation:**

All collective operations MUST properly segment large messages to respect the `max_packet_size` limit (default: 4096 bytes). The generator provides a helper function for this:

```python
def generate_collective_packet_segments(generator, operation, src, dst, 
                                       total_size, tag, session_id):
    """Helper to generate segmented packets for collective operations.
    
    Automatically breaks large data into MAX_PACKETSIZE chunks to match
    hardware constraints. Each packet includes proper segment numbering.
    
    Args:
        generator: ACCLTraceGenerator instance
        operation: AcclOperation type
        src: Source rank
        dst: Destination rank
        total_size: Total message size in bytes
        tag: Message tag
        session_id: Session identifier
    
    Returns:
        List of segmented Packet objects
    """
    packets = []
    num_segments = (total_size + generator.max_packet_size - 1) // generator.max_packet_size
    
    for seg in range(num_segments):
        seg_size = min(generator.max_packet_size, total_size - seg * generator.max_packet_size)
        packet = Packet(
            packet_type=AcclPacketType.COLLECTIVE_DATA,
            operation=operation,
            src_rank=src,
            dst_rank=dst,
            tag=tag,
            session_id=session_id,
            sequence_num=generator.next_sequence(src, dst),
            data_length=seg_size,
            payload_segment=seg,
            total_segments=num_segments,
            # ... other fields
        )
        packets.append(packet)
    
    return packets
```

**IMPORTANT:** When implementing new collective operations, always use the segmentation helper for messages that may exceed `max_packet_size`. Direct packet creation without segmentation will cause failures in the C packet processor.

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
    tag = generator.TAG_ANY
    
    # For each communication pair, use segmentation helper
    for rank in range(generator.num_ranks):
        if rank != root:
            session_id = generator.next_session_id(root)
            # Use segmentation helper to ensure packets don't exceed max size
            segments = generate_collective_packet_segments(
                generator, AcclOperation.MY_OPERATION, 
                root, rank, data_size, tag, session_id
            )
            packets.extend(segments)
    
    return packets
```

**Extension Points:**
- Add new collective algorithms (e.g., tree-based broadcast)
- Implement custom reduction operations
- Optimize for specific network topologies

### 5. `output_writers.py` (314 lines)

**Purpose:** Write packets in multiple formats with automatic directory creation

**Location:** `src/output_writers.py`

**Output Formats:**

1. **Human-Readable Text** (`write_human_readable`):
   - Human-readable with statistics
   - Includes header with configuration
   - Full packet details for debugging

2. **Raw Hex Format** (`write_raw_hex`):
   - **Primary format for encapsulation**
   - One packet per line (hex string only)
   - Ready for TCP/UDP/Ethernet wrapping
   - Automatically creates output directories

3. **Detailed Hex Format** (`write_detailed_hex`):
   - Hex with field breakdowns
   - Shows header structure
   - Useful for debugging

4. **Binary Format** (`write_binary`):
   - Native binary packets
   - For simulation tools
   - Efficient storage
   - Automatically creates output directories

**Usage:**

```python
from src.output_writers import (
    write_human_readable,
    write_raw_hex,
    write_detailed_hex,
    write_binary
)

packets = generator.generate_trace(100)

# Write all formats (directories are created automatically)
write_human_readable(packets, "output/trace.txt", generator, num_ranks, num_operations)
write_raw_hex(packets, "output/packets_raw.txt")
write_detailed_hex(packets, "output/packets_detailed.txt", num_ranks, num_operations)
write_binary(packets, "output/trace.bin", num_ranks)
```

**Key Features:**

- **Automatic Directory Creation:** `write_binary()` and `write_raw_hex()` automatically create parent directories if they don't exist using `os.makedirs(dirname, exist_ok=True)`. This ensures files can be written to nested paths without manual directory creation.

- **Format Flexibility:** Each format serves a specific purpose:
  - Human-readable for debugging and manual inspection
  - Raw hex for direct protocol encapsulation
  - Detailed hex for protocol analysis
  - Binary for efficient simulation input

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
    
    # Clean output directories
    # (Automatically removes and recreates output directories)
    
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
2. **Automatically clean output directories** (`output_all_nodes/`, `output_single_node/`) and Python cache (`src/__pycache__/`)
3. Create generator with configuration
4. Generate packet trace
5. Filter for single node (if requested)
6. Write all output formats

**Directory Cleanup Behavior:**
The generator automatically performs the following cleanup operations at the start of each run:
- Removes and recreates `output_all_nodes/` directory
- Removes and recreates `output_single_node/` directory  
- Removes `src/__pycache__/` directory to prevent stale bytecode issues

This ensures fresh trace generation without interference from previous runs and prevents issues with cached Python modules.

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

## Test Suite Architecture

The test suite is comprehensive, well-organized, and validates all aspects of the packet trace generator.

### Test Module Details

#### 1. `test_packet.py` (10 tests)

**Purpose:** Validate Packet class functionality

**Test Classes:**
- `TestPacketCreation`: Basic packet instantiation
- `TestPacketSerialization`: Binary and hex serialization
- `TestPacketTypes`: All packet types (DATA_EAGER, RNDZV_ADDR, RNDZV_DATA, COLLECTIVE_DATA)

**Key Tests:**
```python
# Test binary serialization
def test_binary_serialization(self):
    packet = Packet(...)
    binary = packet.to_binary()
    self.assertEqual(len(binary), 64 + packet.data_length)

# Test hex format
def test_hex_format(self):
    packet = Packet(...)
    hex_str = packet.to_hex()
    self.assertEqual(len(hex_str), 2 * (64 + packet.data_length))
```

#### 2. `test_trace_generator.py` (14 tests)

**Purpose:** Validate ACCLTraceGenerator core functionality

**Test Classes:**
- `TestGeneratorInitialization`: Configuration validation
- `TestSessionManagement`: Session ID and sequence number tracking
- `TestDeterministicGeneration`: Reproducibility with seeds

**Key Tests:**
```python
# Test deterministic generation
def test_deterministic_generation(self):
    random.seed(100)
    gen1 = ACCLTraceGenerator(num_ranks=4, seed=100)
    packets1 = gen1.generate_trace(10)
    
    random.seed(100)  # Reset global state
    gen2 = ACCLTraceGenerator(num_ranks=4, seed=100)
    packets2 = gen2.generate_trace(10)
    
    self.assertEqual(len(packets1), len(packets2))
    # Compare packet contents...
```

#### 3. `test_collective_operations.py` (27 tests)

**Purpose:** Validate all 10 collective communication operations

**Test Classes:**
- `TestBroadcast`: Broadcast operation patterns
- `TestScatter`: Scatter operation patterns
- `TestGather`: Gather operation patterns
- `TestReduce`: Reduce operation patterns
- `TestAllgather`: Allgather operation patterns
- `TestAllreduce`: Allreduce operation patterns
- `TestReduceScatter`: Reduce-scatter operation patterns
- `TestBarrier`: Barrier synchronization (validates 2*(num_ranks-1) packets)
- `TestAlltoall`: All-to-all communication

**Key Tests:**
```python
# Test broadcast packet count
def test_broadcast_packet_count(self):
    generator = ACCLTraceGenerator(num_ranks=8)
    packets = generate_broadcast(generator)
    # Should be num_ranks - 1 packets from root to others
    self.assertEqual(len(packets), 7)

# Test barrier synchronization
def test_barrier_basic(self):
    generator = ACCLTraceGenerator(num_ranks=4)
    packets = generate_barrier(generator)
    # Barrier: gather (N-1) + scatter (N-1) = 2*(N-1)
    self.assertEqual(len(packets), 6)
```

#### 4. `test_output_writers.py` (11 tests)

**Purpose:** Validate file output functionality

**Test Classes:**
- `TestBinaryOutput`: Binary file format
- `TestHexOutput`: Hexadecimal file formats
- `TestDirectoryCreation`: Automatic directory creation

**Key Tests:**
```python
# Test automatic directory creation
def test_binary_creates_directory(self):
    nested_path = "test_outputs/nested/dir/trace.bin"
    write_binary(packets, nested_path, num_ranks=4)
    self.assertTrue(os.path.exists(nested_path))

# Test binary format correctness
def test_binary_format(self):
    write_binary(packets, "test.bin", num_ranks=4)
    with open("test.bin", "rb") as f:
        header = f.read(16)
        magic, version, ranks, count = struct.unpack('>4sIII', header)
        self.assertEqual(magic, b'ACCL')
        self.assertEqual(ranks, 4)
```

#### 5. `test_integration.py` (10 tests)

**Purpose:** End-to-end integration tests

**Test Classes:**
- `TestCompleteWorkflow`: Full generation pipeline
- `TestMultiNodeGeneration`: Large-scale simulations
- `TestScalability`: Performance with many ranks

**Key Tests:**
```python
# Test complete workflow
def test_complete_workflow(self):
    generator = ACCLTraceGenerator(num_ranks=8, seed=42)
    packets = generator.generate_trace(50)
    
    # Write all formats
    write_binary(packets, "output/trace.bin", 8)
    write_raw_hex(packets, "output/trace_hex.txt")
    
    # Verify outputs exist and are valid
    self.assertTrue(os.path.exists("output/trace.bin"))
    self.assertTrue(os.path.exists("output/trace_hex.txt"))

# Test scalability
def test_large_scale_generation(self):
    generator = ACCLTraceGenerator(num_ranks=64, seed=100)
    packets = generator.generate_trace(1000)
    self.assertGreater(len(packets), 1000)
```

### Test Execution Strategy

1. **Fast Feedback Loop:**
   ```bash
   # Run tests for component you're working on
   python -m unittest tests.test_packet -v
   ```

2. **Pre-Commit Validation:**
   ```bash
   # Run full test suite before committing
   python -m unittest discover tests -v
   ```

3. **Continuous Integration Ready:**
   - All tests are deterministic (use fixed seeds)
   - No external dependencies required
   - Clean output directory management

### Writing New Tests

When adding a new feature, follow this pattern:

```python
import unittest
from src.trace_generator import ACCLTraceGenerator
from src.packet import Packet
from src.accl_types import AcclPacketType, AcclOperation

class TestMyNewFeature(unittest.TestCase):
    def setUp(self):
        """Set up test fixtures"""
        self.generator = ACCLTraceGenerator(num_ranks=4, seed=42)
    
    def tearDown(self):
        """Clean up after tests"""
        pass
    
    def test_feature_basic(self):
        """Test basic functionality"""
        result = self.generator.my_new_feature()
        self.assertIsNotNone(result)
    
    def test_feature_edge_case(self):
        """Test edge cases"""
        # Test boundary conditions
        pass

if __name__ == '__main__':
    unittest.main()
```

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

## Best Practices

### Code Quality

#### Testing
- **Write Tests First:** Follow TDD principles - write tests before implementing features
- **Run Tests Frequently:** Execute relevant test files during development
- **Maintain Coverage:** Keep test coverage above 90%
- **Test Edge Cases:** Include boundary conditions and error cases
- **Use Deterministic Seeds:** Ensure tests are reproducible with `seed` parameters

```python
# Example: Always use seeds in tests
generator = ACCLTraceGenerator(num_ranks=4, seed=42)
```

#### Packet Segmentation (CRITICAL)
- **Always Use Segmentation Helper:** When implementing new collective operations, ALWAYS use `generate_collective_packet_segments()` for messages that may exceed `max_packet_size`
- **Never Create Large Packets Directly:** Creating packets with `data_length > max_packet_size` will cause failures in the C packet processor
- **RDMA Exception:** `RNDZV_DATA` packets are exempt from the 4096-byte limit. These packets represent logical RDMA transfers that bypass packet buffers and write directly to memory addresses. The `data_length` field for RDMA packets contains the full transfer size.
- **Validate Packet Sizes:** Ensure all non-RDMA generated packets respect the hardware constraint (default: 4096 bytes)

```python
# CORRECT: Use segmentation helper for regular packets
def generate_my_collective(generator):
    data_size = random.randint(1000, 64000)  # May exceed max_packet_size
    for dst in range(generator.num_ranks):
        session_id = generator.next_session_id(0)
        # Segmentation helper handles large messages automatically
        segments = generate_collective_packet_segments(
            generator, AcclOperation.MY_OP, 0, dst, data_size, tag, session_id
        )
        packets.extend(segments)

# CORRECT: RDMA packets can be large (logical DMA operation)
def generate_rendezvous_send(self, src, dst, size, tag):
    # ... RNDZV_ADDR packet ...
    
    # RDMA data packet - represents logical DMA transfer
    rdma_packet = Packet(
        packet_type=PacketType.RNDZV_DATA,
        data_length=size,  # OK: Can be > 4096 for RDMA
        address=destination_address,
        # ... other fields
    )
    
    # ... RNDZV_COMPLETE packet ...

# INCORRECT: Direct packet creation without segmentation for regular packets
def generate_my_collective_wrong(generator):
    data_size = random.randint(1000, 64000)  # May exceed max_packet_size!
    packet = Packet(
        packet_type=PacketType.COLLECTIVE_DATA,  # Regular packet
        data_length=data_size,  # ERROR: Could be > 4096 bytes
        # ... other fields
    )
    # This will fail in the C processor if data_size > max_packet_size
```

#### Code Organization
- **Single Responsibility:** Each function should do one thing well
- **Clear Interfaces:** Use type hints and docstrings
- **Consistent Naming:** Follow Python conventions (PEP 8)

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
- Keep README.md and DEVELOPER.md updated
- Update test documentation in tests/README.md

### File Organization

- Keep modules under 500 lines
- Extract complex logic into helper functions
- Use clear, descriptive function names
- Group related functionality


## References

- **ACCL Protocol:** [Xilinx ACCL Documentation]
- **MPI Collective Operations:** MPI Standard 4.0
- **Packet Encapsulation:** RFC 894 (Ethernet), RFC 791 (IP)

For user documentation, see [README.md](README.md)