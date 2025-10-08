# Quick Reference - ACCL Packet Trace Generator

## Generate Packets
```bash
python generate_packet_trace.py
```

## Output Files Summary

| File | Purpose | Format |
|------|---------|--------|
| `accl_packet_trace.txt` | Human-readable descriptions | Text |
| `accl_packets_raw_hex.txt` | **Raw hex for encapsulation** ⭐ | Hex (one per line) |
| `accl_packets_hex_detailed.txt` | Detailed hex breakdown | Hex + annotations |
| `accl_packet_trace.bin` | Binary format | Binary |

## Quick Usage Examples

### Read Raw Hex (Python)
```python
with open('accl_packets_raw_hex.txt', 'r') as f:
    for line in f:
        if not line.startswith('#'):
            packet_bytes = bytes.fromhex(line.strip())
            # Ready to encapsulate in TCP/UDP/Ethernet
```

### Encapsulate in Ethernet
```python
def encapsulate_ethernet(packet_hex):
    payload = bytes.fromhex(packet_hex)
    dst_mac = bytes.fromhex('ffffffffffff')
    src_mac = bytes.fromhex('001122334455')
    ethertype = bytes.fromhex('0800')
    return dst_mac + src_mac + ethertype + payload
```

## Packet Structure (64-byte header + payload)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Protocol Number (0xACCE)      |    Version    |   Reserved    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          Packet Type                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          Operation                            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Source Rank                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Destination Rank                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                             Tag                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          Session ID                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Sequence Number                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Data Length                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Timestamp                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                            Flags                              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Address (8 bytes)                     |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Reserved (4 bytes)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Payload Seg  | Total Segs    |          Checksum             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Payload Data                          |
|                         (N bytes)                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

## Configuration
```python
NUM_RANKS = 8           # Number of ranks
NUM_OPERATIONS = 50     # Number of operations
```

## More Information
See `README.md` for complete documentation.
