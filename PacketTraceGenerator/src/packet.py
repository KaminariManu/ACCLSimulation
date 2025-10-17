"""
ACCL Packet Class

This module contains the Packet class which represents an ACCL packet
and provides methods for serialization to binary and hexadecimal formats.
"""

import random
import struct
from dataclasses import dataclass
from .accl_types import PacketType, Operation


@dataclass
class Packet:
    """Represents an ACCL packet"""
    timestamp: int
    packet_type: PacketType
    operation: Operation
    src_rank: int
    dst_rank: int
    tag: int
    session_id: int
    sequence_number: int
    data_length: int  # in bytes
    compression: int
    is_host_memory: bool
    address: int = 0  # For rendezvous
    payload_segment: int = 0  # Segment number for multi-packet transfers
    total_segments: int = 1
    
    def to_binary(self) -> bytes:
        """
        Convert packet to binary format suitable for network transmission.
        
        Binary packet structure (64 bytes header + payload):
        - Bytes 0-1:   Protocol number (0xACCE) 
        - Bytes 2-3:   Version (0x0001)
        - Bytes 4-7:   Packet type (4 bytes)
        - Bytes 8-11:  Operation type (4 bytes)
        - Bytes 12-15: Source rank (4 bytes)
        - Bytes 16-19: Destination rank (4 bytes)
        - Bytes 20-23: Tag (4 bytes)
        - Bytes 24-27: Session ID (4 bytes)
        - Bytes 28-31: Sequence number (4 bytes)
        - Bytes 32-35: Data length (4 bytes)
        - Bytes 36-39: Timestamp (4 bytes)
        - Bytes 40-43: Flags (compression, host_memory, etc.)
        - Bytes 44-51: Address (8 bytes)
        - Bytes 52-55: Payload segment (4 bytes)
        - Bytes 56-59: Total segments (4 bytes)
        - Bytes 60-63: Checksum (4 bytes)
        - Bytes 64+:   Payload data (data_length bytes)
        
        Total header: exactly 64 bytes (matching DATAPATH_WIDTH_BYTES)
        """
        # Create header
        protocol_number = 0xACCE  # ACCE (ACCL communication)
        version = 0x01
        reserved = 0x00
        
        # Map enums to integers
        ptype = list(PacketType).index(self.packet_type)
        optype = list(Operation).index(self.operation)
        
        # Create flags byte
        flags = 0
        if self.is_host_memory:
            flags |= (1 << 0)  # Bit 0: host memory
        flags |= ((self.compression & 0x0F) << 4)  # Bits 4-7: compression
        
        # Pack header (60 bytes) + 4 bytes padding = 64 bytes total
        # Format: HH (4) + IIIIIIIIII (40) + Q (8) + III (12) = 64 bytes!
        header = struct.pack(
            '>HHIIIIIIIIIIQIII',  # Big-endian format (64 bytes exactly!)
            protocol_number,          # 2 bytes: protocol number
            version,                  # 2 bytes: version
            ptype,                    # 4 bytes: packet type
            optype,                   # 4 bytes: operation
            self.src_rank,            # 4 bytes: source rank
            self.dst_rank,            # 4 bytes: destination rank
            self.tag,                 # 4 bytes: tag
            self.session_id,          # 4 bytes: session ID
            self.sequence_number,     # 4 bytes: sequence number
            self.data_length,         # 4 bytes: data length
            self.timestamp & 0xFFFFFFFF,  # 4 bytes: timestamp
            flags,                    # 4 bytes: flags
            self.address,             # 8 bytes: address
            self.payload_segment,     # 4 bytes: segment
            self.total_segments,      # 4 bytes: total segments
            0                         # 4 bytes: checksum placeholder
        )
        
        # Generate payload (random data for simulation)
        payload = self._generate_payload()
        
        # Calculate checksum (simple sum of all bytes mod 2^32)
        checksum = sum(header) + sum(payload)
        checksum = checksum & 0xFFFFFFFF
        
        # Re-pack header with checksum (exactly 64 bytes)
        header = struct.pack(
            '>HHIIIIIIIIIIQIII',
            protocol_number, version,
            ptype, optype,
            self.src_rank, self.dst_rank,
            self.tag, self.session_id,
            self.sequence_number, self.data_length,
            self.timestamp & 0xFFFFFFFF, flags,
            self.address,
            self.payload_segment, self.total_segments,
            checksum
        )
        
        return header + payload
    
    def _generate_payload(self) -> bytes:
        """Generate payload data (random for simulation purposes)
        
        For RDMA packets (RNDZV_DATA), we don't generate actual payload since
        it would be transferred directly via DMA in hardware, not through packet buffers.
        """
        # Skip payload generation for RDMA packets (they use DMA, not packet payload)
        if self.packet_type == PacketType.RNDZV_DATA:
            return bytes(self.data_length)  # Zero-filled placeholder (fast)
        
        # Generate random payload data for regular packets
        # In real implementation, this would be actual application data
        return bytes([random.randint(0, 255) for _ in range(self.data_length)])
    
    def to_hex(self) -> str:
        """Convert packet to hexadecimal string"""
        binary_data = self.to_binary()
        return binary_data.hex()
    
    def to_hex_formatted(self, bytes_per_line: int = 16) -> str:
        """Convert packet to formatted hexadecimal string with offset"""
        binary_data = self.to_binary()
        hex_lines = []
        
        for i in range(0, len(binary_data), bytes_per_line):
            chunk = binary_data[i:i+bytes_per_line]
            hex_bytes = ' '.join(f'{b:02x}' for b in chunk)
            ascii_repr = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)
            hex_lines.append(f'{i:08x}  {hex_bytes:<{bytes_per_line*3}}  |{ascii_repr}|')
        
        return '\n'.join(hex_lines)
    
    def __str__(self):
        flags = []
        if self.is_host_memory:
            flags.append("HOST")
        if self.compression != 0:
            flags.append(f"COMPRESS=0x{self.compression:X}")
        
        base = (f"[T={self.timestamp:06d}] {self.packet_type.value:20s} "
                f"OP={self.operation.value:15s} "
                f"SRC={self.src_rank:2d} DST={self.dst_rank:2d} "
                f"TAG={self.tag:5d} SES={self.session_id:3d} "
                f"SEQ={self.sequence_number:4d} LEN={self.data_length:6d}")
        
        if self.total_segments > 1:
            base += f" SEG={self.payload_segment + 1}/{self.total_segments}"
        
        if self.address != 0:
            base += f" ADDR=0x{self.address:016X}"
        
        if flags:
            base += f" [{', '.join(flags)}]"
        
        return base
