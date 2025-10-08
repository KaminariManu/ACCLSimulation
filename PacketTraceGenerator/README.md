# ACCL Packet Trace Generator

## Overview

This Python script (`generate_packet_trace.py`) generates a comprehensive trace of ACCL (Accelerated Collective Communication Library) packets for various communication operations. The generator creates packets in multiple formats including **hexadecimal format ready for encapsulation** in transport protocols (TCP/UDP) or Ethernet frames.

The trace includes both point-to-point and collective operations, properly sequenced according to ACCL protocol requirements.

## Requirements

- Python 3.6 or higher
- No external dependencies (uses only standard library)

## Quick Start

```bash
python generate_packet_trace.py
```

This generates 4 output files with 50 operations across 8 ranks by default.

---

## Output Files

When you run the generator, it creates **4 output files**:

### 1. `accl_packet_trace.txt` (Human-Readable)
- Contains packet descriptions in text format
- Easy to read and understand
- Shows timestamps, operations, source/destination, etc.
- Includes statistics summary
- **Use for**: Analysis, debugging, and understanding packet flow

**Example output:**
```
[T=001250] DATA_EAGER           OP=SEND            SRC= 3 DST= 5 TAG=  457 SES= 12 SEQ=  23 LEN=  4096 SEG=1/2 [HOST, COMPRESS=0x8]
```

### 2. `accl_packets_raw_hex.txt` (Ready for Encapsulation) ⭐
- **One packet per line** in pure hexadecimal format
- No formatting, no descriptions, just hex
- Ready to be read and encapsulated in transport/link layer protocols
- **Use for**: Direct reading by your encapsulation software/hardware

**Example format:**
```
acce0001000000000000000000000002000000000000000500000000000002e7000000000000000000100000000fa00000...
acce000100000000000000000000000200000000000000050000000000000000000000010000010100000000012c0000...
```

### 3. `accl_packets_hex_detailed.txt` (Detailed Breakdown)
- Each packet shown with complete breakdown
- Header fields separated and labeled
- Payload data displayed in formatted hex dump
- Shows byte offsets and field meanings
- **Use for**: Understanding packet structure, debugging, learning the protocol

**Example format:**
```
================================================================================
PACKET 1/142
================================================================================
Description: [T=000000] DATA_EAGER OP=SEND SRC=2 DST=5...

Header Breakdown (64 bytes):
  Protocol Number:    acce                  (bytes 0-1)
  Version/Reserved:   0100                  (bytes 2-3)
  Packet Type:        00000000              (bytes 4-7)
  Operation:          00000000              (bytes 8-11)
  Source Rank:        00000002              (bytes 12-15)
  Destination Rank:   00000005              (bytes 16-19)
  ...

Payload Data (4096 bytes):
00000000  ac ce 00 01 00 00 00 00 00 00 00 00 00 00 00 02  |................|
...

Complete Packet (Hex - Continuous):
acce0001000000000000000000000002000000000000000500000000000002e7
...
```

### 4. `accl_packet_trace.bin` (Binary Format)
- Pure binary file
- Contains file header + all packets in binary form
- Most efficient storage
- **Use for**: Binary processing, simulation tools, direct memory loading

---

## Features

### Supported Operations

#### 1. Point-to-Point Operations
- **SEND/RECV**: Both eager protocol (for small messages) and rendezvous protocol (for large messages)
  - **Eager**: Direct data transfer with segmentation (≤32KB)
  - **Rendezvous**: Three-phase protocol (address exchange, RDMA transfer, completion notification) (>32KB)

#### 2. Collective Operations
- **BROADCAST**: Root sends same data to all ranks (flat tree or binary tree)
- **SCATTER**: Root sends different data chunks to each rank
- **GATHER**: All ranks send data to root (ring-based)
- **REDUCE**: Combine data from all ranks using reduction operation (ring-based)
- **ALLGATHER**: Each rank gathers data from all other ranks (ring-based)
- **ALLREDUCE**: Reduce and broadcast combined (reduce-scatter + allgather)
- **REDUCE_SCATTER**: Reduce and scatter combined (ring-based)
- **BARRIER**: Synchronization barrier (gather + scatter notifications)
- **ALLTOALL**: Each rank sends unique data to every other rank

### Packet Types

The generator creates the following packet types:

| Type ID | Name | Description |
|---------|------|-------------|
| 0 | DATA_EAGER | Eager protocol data packet (≤32KB) |
| 1 | RNDZV_ADDR | Rendezvous address exchange |
| 2 | RNDZV_DATA | Rendezvous RDMA data transfer |
| 3 | RNDZV_COMPLETE | Rendezvous completion notification |
| 4 | COLLECTIVE_DATA | Collective operation data |

### Operation Types

| Op ID | Name | Description |
|-------|------|-------------|
| 0 | SEND | Point-to-point send |
| 1 | RECV | Point-to-point receive |
| 2 | BROADCAST | Broadcast to all ranks |
| 3 | SCATTER | Scatter data to all ranks |
| 4 | GATHER | Gather data from all ranks |
| 5 | REDUCE | Reduce operation |
| 6 | ALLGATHER | All-to-all gather |
| 7 | ALLREDUCE | All-reduce operation |
| 8 | REDUCE_SCATTER | Reduce-scatter operation |
| 9 | BARRIER | Synchronization barrier |
| 10 | ALLTOALL | All-to-all communication |

---

## Binary Packet Structure

Each packet has a **64-byte header** followed by the **payload**:

### Header Structure (64 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0-1    | 2    | Protocol Number | 0xACCE (ACCL Communication) |
| 2-3    | 2    | Version | Protocol version (0x0001) |
| 4-7    | 4    | Packet Type | Type of packet (eager, rendezvous, etc.) |
| 8-11   | 4    | Operation | ACCL operation (SEND, RECV, BROADCAST, etc.) |
| 12-15  | 4    | Source Rank | Source rank ID |
| 16-19  | 4    | Destination Rank | Destination rank ID |
| 20-23  | 4    | Tag | Message tag for matching (0xFFFFFFFF = TAG_ANY) |
| 24-27  | 4    | Session ID | Communication session identifier |
| 28-31  | 4    | Sequence Number | Packet sequence number |
| 32-35  | 4    | Data Length | Payload size in bytes |
| 36-39  | 4    | Timestamp | Simulation timestamp (lower 32 bits) |
| 40-43  | 4    | Flags | Compression, memory type, etc. |
| 44-51  | 8    | Address | Memory address (for RDMA operations) |
| 52-55  | 4    | Reserved | Reserved for future use |
| 56-57  | 2    | Payload Segment | Current segment number |
| 58-59  | 2    | Total Segments | Total number of segments |
| 60-63  | 4    | Checksum | Packet checksum |
| 64+    | N    | Payload | Actual data (N = Data Length bytes) |

**Important**: All fields are in **big-endian** (network byte order)

### Flags Field (Bits 40-43)

| Bit | Meaning |
|-----|---------|
| 0 | Host Memory (1 = host, 0 = device) |
| 1-3 | Reserved |
| 4-7 | Compression flags (0x0 = none, 0x8 = Ethernet compressed, etc.) |

### Packet Metadata
- **Timestamp**: Simulation time in cycles
- **Packet Type**: Type of packet (eager, rendezvous, etc.)
- **Operation**: Associated ACCL operation
- **Source Rank**: Originating rank (0 to N-1)
- **Destination Rank**: Target rank (0 to N-1)
- **Tag**: Message tag for matching (or TAG_ANY = 0xFFFFFFFF)
- **Session ID**: Communication session identifier
- **Sequence Number**: Per-connection sequence number
- **Data Length**: Payload size in bytes
- **Compression**: Compression flags (0x0 to 0xF)
- **Host Memory Flag**: Indicates if buffer is in host or device memory
- **Address**: 64-bit address (for rendezvous operations)
- **Segment Info**: Current segment and total segments (for multi-packet transfers)
- **Checksum**: Sum of all header+payload bytes mod 2³²

---

## Configuration

Edit the `main()` function in `generate_packet_trace.py` to customize:

```python
NUM_RANKS = 8           # Number of ranks in the communicator
NUM_OPERATIONS = 50     # Number of operations to generate
```

### Advanced Customization

Modify the `generate_trace()` method to adjust operation weights:

```python
operations = [
    (self.generate_send_recv_pair, 30),  # 30% point-to-point
    (self.generate_broadcast, 10),       # 10% broadcast
    (self.generate_scatter, 8),          # 8% scatter
    (self.generate_gather, 8),           # 8% gather
    (self.generate_reduce, 8),           # 8% reduce
    (self.generate_allgather, 8),        # 8% allgather
    (self.generate_allreduce, 10),       # 10% allreduce
    (self.generate_reduce_scatter, 6),   # 6% reduce-scatter
    (self.generate_barrier, 6),          # 6% barrier
    (self.generate_alltoall, 6),         # 6% alltoall
]
```

---

## How to Use the Hex Packets

### Reading Raw Hex File (Python)

```python
# Read raw hex packets ready for encapsulation
with open('accl_packets_raw_hex.txt', 'r') as f:
    for line in f:
        line = line.strip()
        if line.startswith('#'):
            continue  # Skip comments
        
        # Convert hex string to bytes
        packet_bytes = bytes.fromhex(line)
        
        # Now you can encapsulate this in TCP/UDP/Ethernet
        # packet_bytes is ready to be wrapped in transport layer
```

### Encapsulating in Ethernet Frame

```python
def encapsulate_ethernet(packet_hex):
    """Encapsulate ACCL packet in Ethernet frame"""
    # Parse hex to bytes
    payload = bytes.fromhex(packet_hex)
    
    # Create Ethernet frame
    dst_mac = bytes.fromhex('ffffffffffff')  # Broadcast MAC
    src_mac = bytes.fromhex('001122334455')  # Source MAC
    ethertype = bytes.fromhex('0800')        # IPv4 (or use custom)
    
    ethernet_frame = dst_mac + src_mac + ethertype + payload
    return ethernet_frame

# Read and encapsulate all packets
with open('accl_packets_raw_hex.txt', 'r') as f:
    for line in f:
        if not line.startswith('#'):
            frame = encapsulate_ethernet(line.strip())
            # Send frame to network or save to PCAP
```

### Encapsulating in UDP/IP

```python
import socket

def encapsulate_udp(packet_bytes, src_ip, dst_ip, src_port, dst_port):
    """Encapsulate ACCL packet in UDP/IP"""
    # In practice, you'd use socket.sendto()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(packet_bytes, (dst_ip, dst_port))
    sock.close()

# Read and send packets
with open('accl_packets_raw_hex.txt', 'r') as f:
    for line in f:
        if not line.startswith('#'):
            packet_bytes = bytes.fromhex(line.strip())
            encapsulate_udp(packet_bytes, '192.168.1.1', '192.168.1.2', 5000, 5000)
```

### Creating PCAP File

```python
import struct

def create_pcap_header():
    """Create PCAP global header"""
    protocol_id = 0xa1b2c3d4
    version_major = 2
    version_minor = 4
    thiszone = 0
    sigfigs = 0
    snaplen = 65535
    network = 1  # Ethernet
    
    return struct.pack('<IHHIIII', protocol_id, version_major, version_minor, 
                       thiszone, sigfigs, snaplen, network)

def create_pcap_packet(packet_data, timestamp=0):
    """Create PCAP packet header + data"""
    ts_sec = timestamp // 1000000
    ts_usec = timestamp % 1000000
    incl_len = len(packet_data)
    orig_len = len(packet_data)
    
    header = struct.pack('<IIII', ts_sec, ts_usec, incl_len, orig_len)
    return header + packet_data

# Create PCAP file
with open('accl_packets.pcap', 'wb') as pcap:
    pcap.write(create_pcap_header())
    
    timestamp = 0
    with open('accl_packets_raw_hex.txt', 'r') as f:
        for line in f:
            if not line.startswith('#'):
                packet_bytes = bytes.fromhex(line.strip())
                # Optionally add Ethernet encapsulation here
                pcap.write(create_pcap_packet(packet_bytes, timestamp))
                timestamp += 1000  # 1ms between packets

print("PCAP file created! You can open it in Wireshark.")
```

### Parsing Binary Packets

```python
import struct

def parse_accl_packet(hex_string):
    """Parse hexadecimal ACCL packet into fields"""
    packet_bytes = bytes.fromhex(hex_string)
    
    # Unpack header (64 bytes)
    header = struct.unpack('>HHBBIIIIIIIIQQHHHI', packet_bytes[0:64])
    
    protocol_number = header[0]
    version = header[1]
    packet_type = header[4]
    operation = header[5]
    src_rank = header[6]
    dst_rank = header[7]
    tag = header[8]
    session_id = header[9]
    seq_num = header[10]
    data_len = header[11]
    timestamp = header[12]
    flags = header[13]
    address = header[14]
    payload_seg = header[16]
    total_segs = header[17]
    checksum = header[18]
    
    # Extract payload
    payload = packet_bytes[64:64+data_len]
    
    return {
        'protocol_number': hex(protocol_number),
        'version': version,
        'packet_type': packet_type,
        'operation': operation,
        'src_rank': src_rank,
        'dst_rank': dst_rank,
        'tag': tag,
        'session_id': session_id,
        'sequence_number': seq_num,
        'data_length': data_len,
        'timestamp': timestamp,
        'flags': flags,
        'address': hex(address),
        'payload_segment': payload_seg,
        'total_segments': total_segs,
        'checksum': checksum,
        'payload': payload
    }

# Example usage
with open('accl_packets_raw_hex.txt', 'r') as f:
    for line in f:
        if not line.startswith('#'):
            packet_info = parse_accl_packet(line.strip())
            print(f"Packet: {packet_info['operation']} from rank {packet_info['src_rank']} "
                  f"to rank {packet_info['dst_rank']}, length={packet_info['data_length']}")
```

---

## Protocol Details

### Eager Protocol (Messages ≤ 32KB)

1. Sender directly transmits data in packets
2. Data is segmented into 4KB chunks (MAX_PACKETSIZE)
3. Each segment gets a sequence number
4. Receiver buffers packets and matches by tag

**Flow:**
```
Sender → [DATA_EAGER segment 1] → Receiver
Sender → [DATA_EAGER segment 2] → Receiver
...
```

### Rendezvous Protocol (Messages > 32KB)

1. **Phase 1**: Receiver sends buffer address to sender
2. **Phase 2**: Sender performs RDMA write to receiver's buffer
3. **Phase 3**: Sender sends completion notification

**Flow:**
```
Receiver → [RNDZV_ADDR with address] → Sender
Sender → [RNDZV_DATA via RDMA] → Receiver's buffer
Sender → [RNDZV_COMPLETE] → Receiver
```

### Collective Operations

Each collective operation follows specific communication patterns:

- **Ring-based**: Operations like allgather, reduce-scatter use ring topology
- **Tree-based**: Broadcast can use binary tree for large communicators
- **Flat-tree**: Small collectives use direct communication
- **Two-phase**: Allreduce combines reduce-scatter and allgather

---

## Example Workflow

1. **Generate packets**: 
   ```bash
   python generate_packet_trace.py
   ```

2. **Choose output file**:
   - Use `accl_packets_raw_hex.txt` for encapsulation
   - Use `accl_packets_hex_detailed.txt` for analysis
   - Use `accl_packet_trace.txt` for human reading
   - Use `accl_packet_trace.bin` for binary processing

3. **Read and parse**: Convert hex strings to bytes

4. **Encapsulate**: Add TCP/UDP/Ethernet headers as needed

5. **Transmit**: Send over network or save to PCAP for Wireshark

---

## Validation

The script ensures:
- Proper sequencing of collective operations
- Unique sequence numbers per source-destination pair
- Increasing session IDs per rank
- Correct segmentation for large messages
- Valid compression flags
- Proper protocol selection based on message size
- Checksums for data integrity

---

## Statistics

The trace file includes:
- Total number of packets
- Packets per operation type
- Packets per packet type
- Total data volume (in bytes and MB)
- Final simulation timestamp

---

## Important Notes

- **Byte Order**: All multi-byte fields use **big-endian** (network byte order)
- **Protocol Number**: `0xACCE` identifies ACCL packets
- **Checksum**: Calculated as sum of all header+payload bytes mod 2³²
- **Payload Data**: Randomly generated for simulation purposes
- **Timestamps**: Simulated based on packet size and transfer time
- **Compression**: Randomly applied to demonstrate flag usage
- **Memory**: Host/device memory placement is randomly selected
- **Protocol Constraints**: The trace respects ACCL protocol rules (e.g., rendezvous for large messages)

---

## License

Refer to the main ACCL project license.
