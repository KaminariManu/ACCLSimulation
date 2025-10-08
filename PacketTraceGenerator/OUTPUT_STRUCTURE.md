# ACCL Packet Trace Generator - Output Structure

## Directory Organization

After running `generate_packet_trace.py`, the following directory structure will be created:

```
PacketTraceGenerator/
├── generate_packet_trace.py          (main script)
├── output_all_nodes/                 (all network traffic)
│   ├── accl_packet_trace.txt         (human-readable format)
│   ├── accl_packets_raw_hex.txt      (raw hex for encapsulation)
│   ├── accl_packets_hex_detailed.txt (detailed hex with field breakdown)
│   └── accl_packet_trace.bin         (binary format)
│
└── output_single_node/                (single node traffic)
    ├── accl_packet_trace_nodeN.txt         (human-readable format)
    ├── accl_packets_raw_hex_nodeN.txt      (raw hex for encapsulation)
    ├── accl_packets_hex_detailed_nodeN.txt (detailed hex with field breakdown)
    └── accl_packet_trace_nodeN.bin         (binary format)
```

Where `N` is a randomly selected node number (0 to NUM_RANKS-1).

## Directory Descriptions

### `output_all_nodes/`
Contains packet traces for **all nodes in the network**.
- Shows complete network traffic
- All send/receive operations between all ranks
- All collective operations with all participants
- Useful for: network-wide analysis, protocol validation, full system simulation

### `output_single_node/`
Contains packet traces for a **single randomly selected node**.
- Only includes packets where the node is source OR destination
- Shows what a single node "sees" in the network
- Includes both sent and received packets
- Useful for: per-node simulation, node-specific testing, memory/buffer analysis

## File Formats

### `.txt` (Human-Readable)
- Easy to read and debug
- Contains packet metadata
- Includes statistics and summaries
- Format: `[T=timestamp] PacketType Operation SRC=X DST=Y TAG=Z ...`

### `_raw_hex.txt` (Raw Hexadecimal)
- One packet per line in hex format
- Ready for encapsulation in transport/ethernet protocols
- No extra formatting, just hex strings
- Useful for: PCAP creation, hardware testing, protocol analyzers

### `_hex_detailed.txt` (Detailed Hexadecimal)
- Hex dump with byte offsets
- Field-by-field breakdown
- Includes ASCII representation
- Useful for: debugging, learning packet structure, manual analysis

### `.bin` (Binary)
- Pure binary format
- Smallest file size
- Includes file header with metadata
- Useful for: fast parsing, hardware simulation, programmatic processing

## Example Output

Running with default settings (8 ranks, 50 operations):
```
ACCL Packet Trace Generator
================================================================================
Configuration:
  Number of ranks: 8
  Number of operations: 50
  Output directory (all nodes): output_all_nodes/
  Output directory (single node): output_single_node/
================================================================================

Generating packet trace...
Generating per-node trace for randomly selected node: 3

Trace generation complete!
================================================================================
Generated 458 total packets
Generated 127 packets for node 3

Output directory structure:
  output_all_nodes/
    ├── accl_packet_trace.txt (human-readable)
    ├── accl_packets_raw_hex.txt (raw hex - ready for encapsulation)
    ├── accl_packets_hex_detailed.txt (detailed hex with breakdown)
    └── accl_packet_trace.bin (binary)
  output_single_node/
    ├── accl_packet_trace_node3.txt (human-readable)
    ├── accl_packets_raw_hex_node3.txt (raw hex - ready for encapsulation)
    ├── accl_packets_hex_detailed_node3.txt (detailed hex with breakdown)
    └── accl_packet_trace_node3.bin (binary)
```

## Customization

To change the configuration, edit these variables in `generate_packet_trace.py`:

```python
NUM_RANKS = 8           # Number of nodes in the network
NUM_OPERATIONS = 50     # Number of operations to simulate
OUTPUT_DIR_ALL = "output_all_nodes"        # Directory for all-network traces
OUTPUT_DIR_NODE = "output_single_node"     # Directory for per-node traces
```

## Notes

- Directories are created automatically if they don't exist
- Each run randomly selects a new node for per-node traces
- Node selection is uniformly random from 0 to (NUM_RANKS - 1)
- All timestamps are in arbitrary simulation time units
- Packet header size is always 64 bytes (DATAPATH_WIDTH_BYTES)
