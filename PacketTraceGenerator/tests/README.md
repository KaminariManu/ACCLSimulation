# ACCL Packet Trace Generator - Test Suite

This directory contains comprehensive unit and integration tests for the ACCL Packet Trace Generator.

## Test Structure

### Unit Tests

- **`test_packet.py`** - Tests for the `Packet` class
  - Packet creation and validation
  - Binary serialization
  - Hexadecimal format conversion
  - Different packet types and operations
  - Segmented packet handling
  - Rendezvous address handling

- **`test_trace_generator.py`** - Tests for the `ACCLTraceGenerator` class
  - Generator initialization
  - Session ID and sequence number generation
  - Timestamp management
  - Trace generation with default and custom weights
  - Deterministic generation with seeds
  - Different rank configurations

- **`test_collective_operations.py`** - Tests for collective operation generators
  - Broadcast operation
  - Scatter operation
  - Gather operation
  - Reduce operation
  - Allgather operation
  - Allreduce operation
  - Reduce-scatter operation
  - Barrier synchronization
  - All-to-all communication
  - Collective communication patterns

- **`test_output_writers.py`** - Tests for output file generation
  - Binary trace file writing
  - Text (hexadecimal) trace file writing
  - File format validation
  - Single-node vs all-nodes output
  - Large trace file handling
  - Directory creation

### Integration Tests

- **`test_integration.py`** - End-to-end integration tests
  - Complete trace generation workflow
  - Multi-node trace generation
  - Different configuration testing
  - Custom weight workflows
  - Operation distribution validation
  - Scalability with large rank counts
  - Memory efficiency with large traces
  - Deterministic behavior verification

## Running Tests

### Run All Tests

```bash
# From the PacketTraceGenerator directory
python -m unittest discover tests
```

### Run Specific Test File

```bash
# Run packet tests
python -m unittest tests.test_packet

# Run trace generator tests
python -m unittest tests.test_trace_generator

# Run collective operations tests
python -m unittest tests.test_collective_operations

# Run output writer tests
python -m unittest tests.test_output_writers

# Run integration tests
python -m unittest tests.test_integration
```

### Run Specific Test Class

```bash
python -m unittest tests.test_packet.TestPacket
```

### Run Specific Test Method

```bash
python -m unittest tests.test_packet.TestPacket.test_packet_creation
```

### Run with Verbose Output

```bash
python -m unittest discover tests -v
```

## Test Coverage

The test suite covers:

- ✅ **Packet Serialization**: Binary and hex format conversion using `DATA_EAGER`, `RNDZV_ADDR`, `RNDZV_DATA`, `COLLECTIVE_DATA` packet types
- ✅ **All Operation Types**: Send/recv, broadcast, scatter, gather, reduce, allgather, allreduce, reduce-scatter, barrier, alltoall
- ✅ **Session Management**: Session ID and sequence number generation
- ✅ **Timestamp Handling**: Time advancement and tracking
- ✅ **Weight Configuration**: Default and custom operation weights
- ✅ **File Output**: Binary (`write_binary`) and raw hex (`write_raw_hex`) file generation with automatic directory creation
- ✅ **Multi-node Traces**: Single and all-node trace generation
- ✅ **Determinism**: Seed-based reproducible generation (with proper random state management)
- ✅ **Scalability**: Large rank counts (up to 64) and operation counts (500+)
- ✅ **Edge Cases**: Zero-length packets, large packets, empty traces, nested directory creation

## Test Requirements

The tests use Python's built-in `unittest` framework and require:

- Python 3.6+
- All dependencies from `environment.yml`

No additional test-specific dependencies are needed.

### Output Functions
Tests use the actual API function names:
- `write_binary(packets, filename, num_ranks)` - Binary trace output
- `write_raw_hex(packets, filename)` - Raw hexadecimal output

Both functions automatically create parent directories if they don't exist.

### Deterministic Generation
The deterministic test properly resets Python's global random state before each generator creation to ensure reproducible results:
```python
random.seed(100)
gen = ACCLTraceGenerator(num_ranks=8, seed=100)
packets = gen.generate_trace(num_operations=10)
```

### Barrier Operation
The barrier test expects `2 * (num_ranks - 1)` packets (gather phase + scatter phase), not `num_ranks` packets.

## Expected Results

All 72 tests should pass when run on a properly configured system. If tests fail:

1. Check that all required dependencies are installed
2. Verify Python version is 3.6 or higher
3. Ensure the `src` directory and all modules are present
4. Check file permissions for writing test output files
5. Verify you're running from the `PacketTraceGenerator` directory (not from `tests/`)

## Test Data

Tests generate temporary files in system temp directories and clean up automatically. No manual cleanup is required.

## Test Statistics

- **Total Test Files**: 5 (+ 1 runner script)
- **Total Test Classes**: 20
- **Total Test Methods**: 72
- **Coverage**: Core functionality >90%
- **Average Test Run Time**: ~45-50 seconds for full suite

## Contributing

When adding new features to the PacketTraceGenerator:

1. Add corresponding unit tests in the appropriate test file
2. Update integration tests if the feature affects the complete workflow
3. Ensure all 72 tests pass before submitting changes
4. Update this README if you add new test files or significantly change test behavior
5. Follow the existing test patterns and naming conventions
