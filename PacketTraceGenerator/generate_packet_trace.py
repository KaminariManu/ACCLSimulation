#!/usr/bin/env python3
"""
ACCL Packet Trace Generator

This script generates a comprehensive trace of ACCL packets for various operations
including point-to-point (send/recv) and collective operations (broadcast, scatter,
gather, reduce, allgather, allreduce, reduce_scatter, barrier, alltoall).

The trace includes both eager and rendezvous protocols, and respects the proper
sequencing requirements for collective operations.
"""

import random
import sys
import struct
from dataclasses import dataclass
from typing import List, Tuple
from enum import Enum


class PacketType(Enum):
    """Types of ACCL packets"""
    DATA_EAGER = "DATA_EAGER"           # Eager protocol data packet
    RNDZV_ADDR = "RNDZV_ADDR"          # Rendezvous address exchange
    RNDZV_DATA = "RNDZV_DATA"          # Rendezvous RDMA data
    RNDZV_COMPLETE = "RNDZV_COMPLETE"  # Rendezvous completion notification
    COLLECTIVE_DATA = "COLLECTIVE_DATA" # Collective operation data


class Operation(Enum):
    """ACCL Operations"""
    SEND = "SEND"
    RECV = "RECV"
    BROADCAST = "BROADCAST"
    SCATTER = "SCATTER"
    GATHER = "GATHER"
    REDUCE = "REDUCE"
    ALLGATHER = "ALLGATHER"
    ALLREDUCE = "ALLREDUCE"
    REDUCE_SCATTER = "REDUCE_SCATTER"
    BARRIER = "BARRIER"
    ALLTOALL = "ALLTOALL"


class CompressionFlag(Enum):
    """Compression flags"""
    NO_COMPRESSION = 0x0
    OP0_COMPRESSED = 0x1
    OP1_COMPRESSED = 0x2
    RES_COMPRESSED = 0x4
    ETH_COMPRESSED = 0x8


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
        """Generate payload data (random for simulation purposes)"""
        # Generate random payload data
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


class ACCLTraceGenerator:
    """Generates ACCL packet traces"""
    
    def __init__(self, num_ranks: int = 8, seed: int = 42):
        self.num_ranks = num_ranks
        self.timestamp = 0
        self.session_counters = [0] * num_ranks
        self.sequence_counters = [[0] * num_ranks for _ in range(num_ranks)]
        random.seed(seed)
        
        # ACCL constants
        self.MAX_PACKETSIZE = 4096
        self.DATAPATH_WIDTH_BYTES = 64
        self.MAX_EAGER_SIZE = 32768  # 32KB
        self.TAG_ANY = 0xFFFFFFFF
        
    def get_next_session_id(self, rank: int) -> int:
        """Get next session ID for a rank"""
        session_id = self.session_counters[rank]
        self.session_counters[rank] += 1
        return session_id
    
    def get_next_sequence(self, src: int, dst: int) -> int:
        """Get next sequence number for src->dst communication"""
        seq = self.sequence_counters[src][dst]
        self.sequence_counters[src][dst] += 1
        return seq
    
    def advance_time(self, cycles: int = None):
        """Advance timestamp"""
        if cycles is None:
            cycles = random.randint(10, 100)
        self.timestamp += cycles
    
    def generate_eager_send(self, src: int, dst: int, size: int, tag: int) -> List[Packet]:
        """Generate eager protocol send packets"""
        packets = []
        session_id = self.get_next_session_id(src)
        compression = random.choice([0, CompressionFlag.ETH_COMPRESSED.value])
        
        # Calculate segments
        num_segments = (size + self.MAX_PACKETSIZE - 1) // self.MAX_PACKETSIZE
        
        for seg in range(num_segments):
            seg_size = min(self.MAX_PACKETSIZE, size - seg * self.MAX_PACKETSIZE)
            seq = self.get_next_sequence(src, dst)
            
            packet = Packet(
                timestamp=self.timestamp,
                packet_type=PacketType.DATA_EAGER,
                operation=Operation.SEND,
                src_rank=src,
                dst_rank=dst,
                tag=tag,
                session_id=session_id,
                sequence_number=seq,
                data_length=seg_size,
                compression=compression,
                is_host_memory=random.choice([True, False]),
                payload_segment=seg,
                total_segments=num_segments
            )
            packets.append(packet)
            self.advance_time(seg_size // 64 + 10)  # Approximate transmission time
        
        return packets
    
    def generate_rendezvous_send(self, src: int, dst: int, size: int, tag: int) -> List[Packet]:
        """Generate rendezvous protocol send packets"""
        packets = []
        session_id = self.get_next_session_id(src)
        address = random.randint(0x1000000, 0xFFFFFFFF) << 32 | random.randint(0, 0xFFFFFFFF)
        
        # 1. Receiver sends address
        self.advance_time(50)
        addr_packet = Packet(
            timestamp=self.timestamp,
            packet_type=PacketType.RNDZV_ADDR,
            operation=Operation.RECV,
            src_rank=dst,
            dst_rank=src,
            tag=tag,
            session_id=self.get_next_session_id(dst),
            sequence_number=self.get_next_sequence(dst, src),
            data_length=32,  # Control packet
            compression=0,
            is_host_memory=random.choice([True, False]),
            address=address
        )
        packets.append(addr_packet)
        
        # 2. Sender performs RDMA write
        self.advance_time(100)
        rdma_packet = Packet(
            timestamp=self.timestamp,
            packet_type=PacketType.RNDZV_DATA,
            operation=Operation.SEND,
            src_rank=src,
            dst_rank=dst,
            tag=tag,
            session_id=session_id,
            sequence_number=self.get_next_sequence(src, dst),
            data_length=size,
            compression=0,
            is_host_memory=False,
            address=address
        )
        packets.append(rdma_packet)
        
        # 3. Completion notification
        self.advance_time(size // 1000 + 50)
        complete_packet = Packet(
            timestamp=self.timestamp,
            packet_type=PacketType.RNDZV_COMPLETE,
            operation=Operation.SEND,
            src_rank=src,
            dst_rank=dst,
            tag=tag,
            session_id=session_id,
            sequence_number=self.get_next_sequence(src, dst),
            data_length=16,  # Control packet
            compression=0,
            is_host_memory=False,
            address=address
        )
        packets.append(complete_packet)
        
        return packets
    
    def generate_send_recv_pair(self) -> List[Packet]:
        """Generate a send/recv pair"""
        src = random.randint(0, self.num_ranks - 1)
        dst = random.randint(0, self.num_ranks - 1)
        while dst == src:
            dst = random.randint(0, self.num_ranks - 1)
        
        size = random.randint(100, 128000)
        tag = random.randint(0, 1000)
        
        if size <= self.MAX_EAGER_SIZE:
            return self.generate_eager_send(src, dst, size, tag)
        else:
            return self.generate_rendezvous_send(src, dst, size, tag)
    
    def generate_broadcast(self) -> List[Packet]:
        """Generate broadcast packets"""
        packets = []
        root = random.randint(0, self.num_ranks - 1)
        size = random.randint(1000, 64000)
        tag = self.TAG_ANY
        compression = random.choice([0, CompressionFlag.ETH_COMPRESSED.value])
        
        if size <= self.MAX_EAGER_SIZE:
            # Eager broadcast - root sends to all
            session_id = self.get_next_session_id(root)
            num_segments = (size + self.MAX_PACKETSIZE - 1) // self.MAX_PACKETSIZE
            
            for seg in range(num_segments):
                seg_size = min(self.MAX_PACKETSIZE, size - seg * self.MAX_PACKETSIZE)
                
                for dst in range(self.num_ranks):
                    if dst != root:
                        packet = Packet(
                            timestamp=self.timestamp,
                            packet_type=PacketType.COLLECTIVE_DATA,
                            operation=Operation.BROADCAST,
                            src_rank=root,
                            dst_rank=dst,
                            tag=tag,
                            session_id=session_id,
                            sequence_number=self.get_next_sequence(root, dst),
                            data_length=seg_size,
                            compression=compression,
                            is_host_memory=False,
                            payload_segment=seg,
                            total_segments=num_segments
                        )
                        packets.append(packet)
                
                self.advance_time(seg_size // 32)
        else:
            # Rendezvous broadcast - address exchange then RDMA
            # Store addresses for each destination
            dest_addresses = {}
            
            # Phase 1: Each destination sends its buffer address to root
            for dst in range(self.num_ranks):
                if dst != root:
                    address = random.randint(0x1000000, 0xFFFFFFFF) << 32 | random.randint(0, 0xFFFFFFFF)
                    dest_addresses[dst] = address  # Store for later use
                    
                    # Address packet from dst to root
                    self.advance_time(20)
                    addr_packet = Packet(
                        timestamp=self.timestamp,
                        packet_type=PacketType.RNDZV_ADDR,
                        operation=Operation.BROADCAST,
                        src_rank=dst,
                        dst_rank=root,
                        tag=tag,
                        session_id=self.get_next_session_id(dst),
                        sequence_number=self.get_next_sequence(dst, root),
                        data_length=32,
                        compression=0,
                        is_host_memory=False,
                        address=address
                    )
                    packets.append(addr_packet)
            
            # Phase 2: Root sends RDMA data to all destinations using their addresses
            for dst in range(self.num_ranks):
                if dst != root:
                    self.advance_time(30)
                    
                    rdma_packet = Packet(
                        timestamp=self.timestamp,
                        packet_type=PacketType.RNDZV_DATA,
                        operation=Operation.BROADCAST,
                        src_rank=root,
                        dst_rank=dst,
                        tag=tag,
                        session_id=self.get_next_session_id(root),
                        sequence_number=self.get_next_sequence(root, dst),
                        data_length=size,
                        compression=0,
                        is_host_memory=False,
                        address=dest_addresses[dst]  # Use the stored address!
                    )
                    packets.append(rdma_packet)
        
        return packets
    
    def generate_scatter(self) -> List[Packet]:
        """Generate scatter packets"""
        packets = []
        root = random.randint(0, self.num_ranks - 1)
        chunk_size = random.randint(500, 8000)
        tag = self.TAG_ANY
        
        # Root sends different chunks to each rank
        for dst in range(self.num_ranks):
            session_id = self.get_next_session_id(root)
            
            packet = Packet(
                timestamp=self.timestamp,
                packet_type=PacketType.COLLECTIVE_DATA,
                operation=Operation.SCATTER,
                src_rank=root,
                dst_rank=dst,
                tag=tag,
                session_id=session_id,
                sequence_number=self.get_next_sequence(root, dst),
                data_length=chunk_size,
                compression=0,
                is_host_memory=False
            )
            packets.append(packet)
            self.advance_time(chunk_size // 64 + 20)
        
        return packets
    
    def generate_gather(self) -> List[Packet]:
        """Generate gather packets (ring-based)"""
        packets = []
        root = random.randint(0, self.num_ranks - 1)
        chunk_size = random.randint(500, 8000)
        tag = self.TAG_ANY
        
        # Ring gather - each rank sends to next, root receives from all
        for i in range(self.num_ranks):
            if i != root:
                next_rank = (i + 1) % self.num_ranks
                
                packet = Packet(
                    timestamp=self.timestamp,
                    packet_type=PacketType.COLLECTIVE_DATA,
                    operation=Operation.GATHER,
                    src_rank=i,
                    dst_rank=next_rank,
                    tag=tag,
                    session_id=self.get_next_session_id(i),
                    sequence_number=self.get_next_sequence(i, next_rank),
                    data_length=chunk_size,
                    compression=0,
                    is_host_memory=False
                )
                packets.append(packet)
                self.advance_time(chunk_size // 64 + 15)
        
        return packets
    
    def generate_reduce(self) -> List[Packet]:
        """Generate reduce packets (ring-based)"""
        packets = []
        root = random.randint(0, self.num_ranks - 1)
        size = random.randint(1000, 16000)
        tag = self.TAG_ANY
        func_id = random.randint(0, 5)  # Reduction function ID
        
        # Ring reduce - each rank reduces and forwards
        for i in range(self.num_ranks - 1):
            src = (root + i + 1) % self.num_ranks
            dst = (root + i + 2) % self.num_ranks
            if dst == root:
                dst = root  # Last one goes to root
            
            packet = Packet(
                timestamp=self.timestamp,
                packet_type=PacketType.COLLECTIVE_DATA,
                operation=Operation.REDUCE,
                src_rank=src,
                dst_rank=dst,
                tag=tag | (func_id << 16),  # Encode function in tag
                session_id=self.get_next_session_id(src),
                sequence_number=self.get_next_sequence(src, dst),
                data_length=size,
                compression=0,
                is_host_memory=False
            )
            packets.append(packet)
            self.advance_time(size // 64 + 30)
        
        return packets
    
    def generate_allgather(self) -> List[Packet]:
        """Generate allgather packets (ring-based)"""
        packets = []
        chunk_size = random.randint(500, 8000)
        tag = self.TAG_ANY
        
        # Ring allgather - P-1 steps, each rank sends and receives
        for step in range(self.num_ranks - 1):
            for rank in range(self.num_ranks):
                next_rank = (rank + 1) % self.num_ranks
                
                packet = Packet(
                    timestamp=self.timestamp,
                    packet_type=PacketType.COLLECTIVE_DATA,
                    operation=Operation.ALLGATHER,
                    src_rank=rank,
                    dst_rank=next_rank,
                    tag=tag,
                    session_id=self.get_next_session_id(rank),
                    sequence_number=self.get_next_sequence(rank, next_rank),
                    data_length=chunk_size,
                    compression=0,
                    is_host_memory=False,
                    payload_segment=step,
                    total_segments=self.num_ranks - 1
                )
                packets.append(packet)
            
            self.advance_time(chunk_size // 64 + 25)
        
        return packets
    
    def generate_allreduce(self) -> List[Packet]:
        """Generate allreduce packets (reduce-scatter + allgather)"""
        packets = []
        chunk_size = random.randint(500, 8000)
        tag = self.TAG_ANY
        func_id = random.randint(0, 5)
        
        # Phase 1: Reduce-scatter (P-1 steps)
        for step in range(self.num_ranks - 1):
            for rank in range(self.num_ranks):
                next_rank = (rank + 1) % self.num_ranks
                
                packet = Packet(
                    timestamp=self.timestamp,
                    packet_type=PacketType.COLLECTIVE_DATA,
                    operation=Operation.ALLREDUCE,
                    src_rank=rank,
                    dst_rank=next_rank,
                    tag=tag | (func_id << 16) | (1 << 24),  # Phase 1 marker
                    session_id=self.get_next_session_id(rank),
                    sequence_number=self.get_next_sequence(rank, next_rank),
                    data_length=chunk_size,
                    compression=0,
                    is_host_memory=False,
                    payload_segment=step,
                    total_segments=(self.num_ranks - 1) * 2
                )
                packets.append(packet)
            
            self.advance_time(chunk_size // 64 + 30)
        
        # Phase 2: Allgather (P-1 steps)
        for step in range(self.num_ranks - 1):
            for rank in range(self.num_ranks):
                next_rank = (rank + 1) % self.num_ranks
                
                packet = Packet(
                    timestamp=self.timestamp,
                    packet_type=PacketType.COLLECTIVE_DATA,
                    operation=Operation.ALLREDUCE,
                    src_rank=rank,
                    dst_rank=next_rank,
                    tag=tag | (2 << 24),  # Phase 2 marker
                    session_id=self.get_next_session_id(rank),
                    sequence_number=self.get_next_sequence(rank, next_rank),
                    data_length=chunk_size,
                    compression=0,
                    is_host_memory=False,
                    payload_segment=self.num_ranks - 1 + step,
                    total_segments=(self.num_ranks - 1) * 2
                )
                packets.append(packet)
            
            self.advance_time(chunk_size // 64 + 25)
        
        return packets
    
    def generate_reduce_scatter(self) -> List[Packet]:
        """Generate reduce-scatter packets"""
        packets = []
        chunk_size = random.randint(500, 8000)
        tag = self.TAG_ANY
        func_id = random.randint(0, 5)
        
        # Ring reduce-scatter - P-1 steps
        for step in range(self.num_ranks - 1):
            for rank in range(self.num_ranks):
                next_rank = (rank + 1) % self.num_ranks
                
                packet = Packet(
                    timestamp=self.timestamp,
                    packet_type=PacketType.COLLECTIVE_DATA,
                    operation=Operation.REDUCE_SCATTER,
                    src_rank=rank,
                    dst_rank=next_rank,
                    tag=tag | (func_id << 16),
                    session_id=self.get_next_session_id(rank),
                    sequence_number=self.get_next_sequence(rank, next_rank),
                    data_length=chunk_size,
                    compression=0,
                    is_host_memory=False,
                    payload_segment=step,
                    total_segments=self.num_ranks - 1
                )
                packets.append(packet)
            
            self.advance_time(chunk_size // 64 + 30)
        
        return packets
    
    def generate_barrier(self) -> List[Packet]:
        """Generate barrier packets"""
        packets = []
        root = 0  # Barrier typically uses rank 0
        tag = self.TAG_ANY
        
        # Phase 1: Gather notifications to rank 0
        for src in range(1, self.num_ranks):
            packet = Packet(
                timestamp=self.timestamp,
                packet_type=PacketType.RNDZV_ADDR,
                operation=Operation.BARRIER,
                src_rank=src,
                dst_rank=root,
                tag=tag,
                session_id=self.get_next_session_id(src),
                sequence_number=self.get_next_sequence(src, root),
                data_length=32,  # Small control packet
                compression=0,
                is_host_memory=False,
                address=0
            )
            packets.append(packet)
            self.advance_time(20)
        
        # Phase 2: Scatter notifications from rank 0
        for dst in range(1, self.num_ranks):
            packet = Packet(
                timestamp=self.timestamp,
                packet_type=PacketType.RNDZV_ADDR,
                operation=Operation.BARRIER,
                src_rank=root,
                dst_rank=dst,
                tag=tag,
                session_id=self.get_next_session_id(root),
                sequence_number=self.get_next_sequence(root, dst),
                data_length=32,  # Small control packet
                compression=0,
                is_host_memory=False,
                address=0
            )
            packets.append(packet)
            self.advance_time(20)
        
        return packets
    
    def generate_alltoall(self) -> List[Packet]:
        """Generate alltoall packets"""
        packets = []
        chunk_size = random.randint(500, 4000)
        tag = self.TAG_ANY
        
        # Each rank sends to every other rank
        for src in range(self.num_ranks):
            for dst in range(self.num_ranks):
                if src != dst:
                    packet = Packet(
                        timestamp=self.timestamp,
                        packet_type=PacketType.COLLECTIVE_DATA,
                        operation=Operation.ALLTOALL,
                        src_rank=src,
                        dst_rank=dst,
                        tag=tag,
                        session_id=self.get_next_session_id(src),
                        sequence_number=self.get_next_sequence(src, dst),
                        data_length=chunk_size,
                        compression=0,
                        is_host_memory=False
                    )
                    packets.append(packet)
            
            self.advance_time(chunk_size // 32)
        
        return packets
    
    def generate_trace(self, num_operations: int = 50) -> List[Packet]:
        """Generate a complete trace with mixed operations"""
        all_packets = []
        
        # Define operation weights (collective operations are less frequent)
        operations = [
            (self.generate_send_recv_pair, 30),  # 30% point-to-point
            (self.generate_broadcast, 10),
            (self.generate_scatter, 8),
            (self.generate_gather, 8),
            (self.generate_reduce, 8),
            (self.generate_allgather, 8),
            (self.generate_allreduce, 10),
            (self.generate_reduce_scatter, 6),
            (self.generate_barrier, 6),
            (self.generate_alltoall, 6),
        ]
        
        # Flatten operations based on weights
        weighted_ops = []
        for op_func, weight in operations:
            weighted_ops.extend([op_func] * weight)
        
        for _ in range(num_operations):
            op_func = random.choice(weighted_ops)
            packets = op_func()
            all_packets.extend(packets)
            
            # Add some idle time between operations
            self.advance_time(random.randint(100, 500))
        
        return all_packets


def main():
    """Main function to generate packet trace"""
    import os
    
    # Configuration
    NUM_RANKS = 8
    NUM_OPERATIONS = 50
    
    # Create output directories
    OUTPUT_DIR_ALL = "output_all_nodes"
    OUTPUT_DIR_NODE = "output_single_node"
    
    os.makedirs(OUTPUT_DIR_ALL, exist_ok=True)
    os.makedirs(OUTPUT_DIR_NODE, exist_ok=True)
    
    # Define output files with directory paths
    OUTPUT_FILE = os.path.join(OUTPUT_DIR_ALL, "accl_packet_trace.txt")
    OUTPUT_HEX_RAW_FILE = os.path.join(OUTPUT_DIR_ALL, "accl_packets_raw_hex.txt")
    OUTPUT_HEX_DETAILED_FILE = os.path.join(OUTPUT_DIR_ALL, "accl_packets_hex_detailed.txt")
    OUTPUT_BIN_FILE = os.path.join(OUTPUT_DIR_ALL, "accl_packet_trace.bin")
    
    print(f"ACCL Packet Trace Generator")
    print(f"=" * 80)
    print(f"Configuration:")
    print(f"  Number of ranks: {NUM_RANKS}")
    print(f"  Number of operations: {NUM_OPERATIONS}")
    print(f"  Output directory (all nodes): {OUTPUT_DIR_ALL}/")
    print(f"  Output directory (single node): {OUTPUT_DIR_NODE}/")
    print(f"=" * 80)
    print()
    
    # Generate trace
    generator = ACCLTraceGenerator(num_ranks=NUM_RANKS)
    packets = generator.generate_trace(num_operations=NUM_OPERATIONS)
    
    # Write human-readable trace to file
    with open(OUTPUT_FILE, 'w') as f:
        f.write("=" * 100 + "\n")
        f.write("ACCL PACKET TRACE (Human-Readable)\n")
        f.write("=" * 100 + "\n")
        f.write(f"Configuration:\n")
        f.write(f"  Number of Ranks: {NUM_RANKS}\n")
        f.write(f"  Number of Operations: {NUM_OPERATIONS}\n")
        f.write(f"  Total Packets: {len(packets)}\n")
        f.write(f"  Max Packet Size: {generator.MAX_PACKETSIZE} bytes\n")
        f.write(f"  Datapath Width: {generator.DATAPATH_WIDTH_BYTES} bytes\n")
        f.write(f"  Eager Threshold: {generator.MAX_EAGER_SIZE} bytes\n")
        f.write("=" * 100 + "\n")
        f.write("\n")
        f.write("Packet Format:\n")
        f.write("  [T=timestamp] PacketType Operation SRC=src DST=dst TAG=tag SES=session SEQ=seq LEN=length\n")
        f.write("  Optional: SEG=segment/total ADDR=address [FLAGS]\n")
        f.write("=" * 100 + "\n")
        f.write("\n")
        
        # Write packets
        for packet in packets:
            f.write(str(packet) + "\n")
        
        f.write("\n")
        f.write("=" * 100 + "\n")
        f.write("TRACE STATISTICS\n")
        f.write("=" * 100 + "\n")
        
        # Calculate statistics
        stats = {}
        for packet in packets:
            op = packet.operation.value
            stats[op] = stats.get(op, 0) + 1
        
        f.write(f"Total Packets: {len(packets)}\n")
        f.write(f"Final Timestamp: {generator.timestamp}\n")
        f.write("\nPackets by Operation:\n")
        for op, count in sorted(stats.items()):
            f.write(f"  {op:20s}: {count:5d} packets\n")
        
        # Packet type statistics
        type_stats = {}
        for packet in packets:
            ptype = packet.packet_type.value
            type_stats[ptype] = type_stats.get(ptype, 0) + 1
        
        f.write("\nPackets by Type:\n")
        for ptype, count in sorted(type_stats.items()):
            f.write(f"  {ptype:20s}: {count:5d} packets\n")
        
        # Data volume
        total_data = sum(p.data_length for p in packets)
        f.write(f"\nTotal Data Volume: {total_data:,} bytes ({total_data / 1024 / 1024:.2f} MB)\n")
        
        f.write("=" * 100 + "\n")
    
    # Write raw hex trace to file (ready for encapsulation)
    print("Generating raw hexadecimal packet trace...")
    with open(OUTPUT_HEX_RAW_FILE, 'w') as f:
        f.write("# ACCL Packet Trace - Raw Hexadecimal Format\n")
        f.write("# Ready for encapsulation in transport/ethernet protocols\n")
        f.write(f"# Total packets: {len(packets)}\n")
        f.write(f"# Each line represents one complete packet in hexadecimal\n")
        f.write("#\n")
        
        for i, packet in enumerate(packets):
            f.write(f"{packet.to_hex()}\n")
    
    # Write detailed hex trace to file
    print("Generating detailed hexadecimal packet trace...")
    with open(OUTPUT_HEX_DETAILED_FILE, 'w') as f:
        f.write("=" * 100 + "\n")
        f.write("ACCL PACKET TRACE (Hexadecimal Format - Detailed)\n")
        f.write("=" * 100 + "\n")
        f.write(f"Configuration:\n")
        f.write(f"  Number of Ranks: {NUM_RANKS}\n")
        f.write(f"  Number of Operations: {NUM_OPERATIONS}\n")
        f.write(f"  Total Packets: {len(packets)}\n")
        f.write(f"  Packet Header Size: 64 bytes\n")
        f.write("=" * 100 + "\n")
        f.write("\n")
        f.write("Packet Binary Structure (64-byte header + payload):\n")
        f.write("  Offset 0-1:   Protocol number (0xACCE)\n")
        f.write("  Offset 2-3:   Version (0x01) + reserved\n")
        f.write("  Offset 4-7:   Packet type\n")
        f.write("  Offset 8-11:  Operation type\n")
        f.write("  Offset 12-15: Source rank\n")
        f.write("  Offset 16-19: Destination rank\n")
        f.write("  Offset 20-23: Tag\n")
        f.write("  Offset 24-27: Session ID\n")
        f.write("  Offset 28-31: Sequence number\n")
        f.write("  Offset 32-35: Data length\n")
        f.write("  Offset 36-39: Timestamp\n")
        f.write("  Offset 40-43: Flags (compression, memory type)\n")
        f.write("  Offset 44-51: Address (8 bytes)\n")
        f.write("  Offset 52-55: Reserved (8 bytes)\n")
        f.write("  Offset 56-57: Payload segment\n")
        f.write("  Offset 58-59: Total segments\n")
        f.write("  Offset 60-63: Checksum\n")
        f.write("  Offset 64+:   Payload data\n")
        f.write("=" * 100 + "\n")
        f.write("\n")
        
        for i, packet in enumerate(packets):
            f.write("=" * 100 + "\n")
            f.write(f"PACKET {i+1}/{len(packets)}\n")
            f.write("=" * 100 + "\n")
            f.write(f"Description: {packet}\n")
            f.write("\n")
            
            # Parse the binary packet to show detailed breakdown
            binary_data = packet.to_binary()
            
            f.write("Header Breakdown (64 bytes):\n")
            f.write("-" * 100 + "\n")
            f.write(f"  Protocol Number:    {binary_data[0:2].hex():20s}  (bytes 0-1)\n")
            f.write(f"  Version/Reserved:   {binary_data[2:4].hex():20s}  (bytes 2-3)\n")
            f.write(f"  Packet Type:        {binary_data[4:8].hex():20s}  (bytes 4-7)\n")
            f.write(f"  Operation:          {binary_data[8:12].hex():20s}  (bytes 8-11)\n")
            f.write(f"  Source Rank:        {binary_data[12:16].hex():20s}  (bytes 12-15)\n")
            f.write(f"  Destination Rank:   {binary_data[16:20].hex():20s}  (bytes 16-19)\n")
            f.write(f"  Tag:                {binary_data[20:24].hex():20s}  (bytes 20-23)\n")
            f.write(f"  Session ID:         {binary_data[24:28].hex():20s}  (bytes 24-27)\n")
            f.write(f"  Sequence Number:    {binary_data[28:32].hex():20s}  (bytes 28-31)\n")
            f.write(f"  Data Length:        {binary_data[32:36].hex():20s}  (bytes 32-35)\n")
            f.write(f"  Timestamp:          {binary_data[36:40].hex():20s}  (bytes 36-39)\n")
            f.write(f"  Flags:              {binary_data[40:44].hex():20s}  (bytes 40-43)\n")
            f.write(f"  Address:            {binary_data[44:52].hex():20s}  (bytes 44-51)\n")
            f.write(f"  Reserved:           {binary_data[52:56].hex():20s}  (bytes 52-55)\n")
            f.write(f"  Payload Segment:    {binary_data[56:58].hex():20s}  (bytes 56-57)\n")
            f.write(f"  Total Segments:     {binary_data[58:60].hex():20s}  (bytes 58-59)\n")
            f.write(f"  Checksum:           {binary_data[60:64].hex():20s}  (bytes 60-63)\n")
            f.write("\n")
            
            if len(binary_data) > 64:
                payload_size = len(binary_data) - 64
                f.write(f"Payload Data ({payload_size} bytes):\n")
                f.write("-" * 100 + "\n")
                f.write(packet.to_hex_formatted() + "\n")
            
            f.write("\n")
            f.write("Complete Packet (Hex - Continuous):\n")
            f.write("-" * 100 + "\n")
            hex_str = packet.to_hex()
            # Write in lines of 64 hex chars (32 bytes per line)
            for j in range(0, len(hex_str), 64):
                f.write(hex_str[j:j+64] + "\n")
            f.write("\n")
    
    # Write binary trace to file
    print("Generating binary packet trace...")
    with open(OUTPUT_BIN_FILE, 'wb') as f:
        # Write header
        header = struct.pack('>4sIII', 
                           b'ACCL',           # Protocol identifier
                           1,                 # Version
                           NUM_RANKS,         # Number of ranks
                           len(packets))      # Number of packets
        f.write(header)
        
        # Write all packets
        for packet in packets:
            f.write(packet.to_binary())
    
    # Generate per-node traces for a randomly selected node
    selected_node = random.randint(0, NUM_RANKS - 1)
    print(f"\nGenerating per-node trace for randomly selected node: {selected_node}")
    
    # Filter packets that involve the selected node (as source or destination)
    node_packets = [p for p in packets if p.src_rank == selected_node or p.dst_rank == selected_node]
    
    # Define per-node output files in the node-specific directory
    NODE_OUTPUT_FILE = os.path.join(OUTPUT_DIR_NODE, f"accl_packet_trace_node{selected_node}.txt")
    NODE_OUTPUT_HEX_RAW_FILE = os.path.join(OUTPUT_DIR_NODE, f"accl_packets_raw_hex_node{selected_node}.txt")
    NODE_OUTPUT_HEX_DETAILED_FILE = os.path.join(OUTPUT_DIR_NODE, f"accl_packets_hex_detailed_node{selected_node}.txt")
    NODE_OUTPUT_BIN_FILE = os.path.join(OUTPUT_DIR_NODE, f"accl_packet_trace_node{selected_node}.bin")
    
    # Write human-readable trace for selected node
    with open(NODE_OUTPUT_FILE, 'w') as f:
        f.write("=" * 100 + "\n")
        f.write(f"ACCL PACKET TRACE FOR NODE {selected_node} (Human-Readable)\n")
        f.write("=" * 100 + "\n")
        f.write(f"Configuration:\n")
        f.write(f"  Selected Node: {selected_node}\n")
        f.write(f"  Total Network Ranks: {NUM_RANKS}\n")
        f.write(f"  Total Network Operations: {NUM_OPERATIONS}\n")
        f.write(f"  Packets involving this node: {len(node_packets)}\n")
        f.write(f"  Max Packet Size: {generator.MAX_PACKETSIZE} bytes\n")
        f.write(f"  Datapath Width: {generator.DATAPATH_WIDTH_BYTES} bytes\n")
        f.write(f"  Eager Threshold: {generator.MAX_EAGER_SIZE} bytes\n")
        f.write("=" * 100 + "\n")
        f.write("\n")
        f.write("Packet Format:\n")
        f.write("  [T=timestamp] PacketType Operation SRC=src DST=dst TAG=tag SES=session SEQ=seq LEN=length\n")
        f.write("  Optional: SEG=segment/total ADDR=address [FLAGS]\n")
        f.write("=" * 100 + "\n")
        f.write("\n")
        
        # Write packets
        for packet in node_packets:
            f.write(str(packet) + "\n")
        
        f.write("\n")
        f.write("=" * 100 + "\n")
        f.write(f"NODE {selected_node} STATISTICS\n")
        f.write("=" * 100 + "\n")
        
        # Calculate statistics
        stats = {}
        for packet in node_packets:
            op = packet.operation.value
            stats[op] = stats.get(op, 0) + 1
        
        f.write(f"Total Packets involving Node {selected_node}: {len(node_packets)}\n")
        f.write(f"Final Timestamp: {generator.timestamp}\n")
        f.write(f"\nPackets by Operation:\n")
        for op, count in sorted(stats.items()):
            f.write(f"  {op:20s}: {count:5d} packets\n")
        
        # Direction statistics
        sent_packets = [p for p in node_packets if p.src_rank == selected_node]
        recv_packets = [p for p in node_packets if p.dst_rank == selected_node]
        
        f.write(f"\nPackets by Direction:\n")
        f.write(f"  Sent by node {selected_node:2d}:     {len(sent_packets):5d} packets\n")
        f.write(f"  Received by node {selected_node:2d}: {len(recv_packets):5d} packets\n")
        
        # Data volume
        total_data = sum(p.data_length for p in node_packets)
        f.write(f"\nTotal Data Volume: {total_data:,} bytes ({total_data / 1024 / 1024:.2f} MB)\n")
        
        f.write("=" * 100 + "\n")
    
    # Write raw hex trace for selected node
    with open(NODE_OUTPUT_HEX_RAW_FILE, 'w') as f:
        f.write(f"# ACCL Packet Trace for Node {selected_node} - Raw Hexadecimal Format\n")
        f.write("# Ready for encapsulation in transport/ethernet protocols\n")
        f.write(f"# Selected node: {selected_node}\n")
        f.write(f"# Packets involving this node: {len(node_packets)}\n")
        f.write(f"# Each line represents one complete packet in hexadecimal\n")
        f.write("#\n")
        
        for packet in node_packets:
            f.write(f"{packet.to_hex()}\n")
    
    # Write detailed hex trace for selected node
    with open(NODE_OUTPUT_HEX_DETAILED_FILE, 'w') as f:
        f.write("=" * 100 + "\n")
        f.write(f"ACCL PACKET TRACE FOR NODE {selected_node} (Hexadecimal Format - Detailed)\n")
        f.write("=" * 100 + "\n")
        f.write(f"Configuration:\n")
        f.write(f"  Selected Node: {selected_node}\n")
        f.write(f"  Total Network Ranks: {NUM_RANKS}\n")
        f.write(f"  Packets involving this node: {len(node_packets)}\n")
        f.write(f"  Packet Header Size: 64 bytes\n")
        f.write("=" * 100 + "\n")
        f.write("\n")
        
        for i, packet in enumerate(node_packets, 1):
            f.write(f"Packet {i} (TS={packet.timestamp}):\n")
            f.write(f"  {packet.packet_type.value} | {packet.operation.value} | ")
            f.write(f"SRC={packet.src_rank} DST={packet.dst_rank} | ")
            f.write(f"TAG={packet.tag} SES={packet.session_id} SEQ={packet.sequence_number} | ")
            f.write(f"LEN={packet.data_length}\n")
            f.write(f"  Hex:\n")
            f.write(packet.to_hex_formatted(bytes_per_line=16))
            f.write("\n" + "-" * 100 + "\n")
    
    # Write binary trace for selected node
    with open(NODE_OUTPUT_BIN_FILE, 'wb') as f:
        # Write header
        header = struct.pack('>4sIII', 
                           b'ACCL',              # Protocol identifier
                           1,                    # Version
                           NUM_RANKS,            # Number of ranks
                           len(node_packets))    # Number of packets for this node
        f.write(header)
        
        # Write all packets for this node
        for packet in node_packets:
            f.write(packet.to_binary())
    
    print(f"\nTrace generation complete!")
    print(f"=" * 80)
    print(f"Generated {len(packets)} total packets")
    print(f"Generated {len(node_packets)} packets for node {selected_node}")
    print(f"\nOutput directory structure:")
    print(f"  {OUTPUT_DIR_ALL}/")
    print(f"    ├── accl_packet_trace.txt (human-readable)")
    print(f"    ├── accl_packets_raw_hex.txt (raw hex - ready for encapsulation)")
    print(f"    ├── accl_packets_hex_detailed.txt (detailed hex with breakdown)")
    print(f"    └── accl_packet_trace.bin (binary)")
    print(f"  {OUTPUT_DIR_NODE}/")
    print(f"    ├── accl_packet_trace_node{selected_node}.txt (human-readable)")
    print(f"    ├── accl_packets_raw_hex_node{selected_node}.txt (raw hex - ready for encapsulation)")
    print(f"    ├── accl_packets_hex_detailed_node{selected_node}.txt (detailed hex with breakdown)")
    print(f"    └── accl_packet_trace_node{selected_node}.bin (binary)")
    print()
    
    # Print sample packets
    print("Sample packets from full trace (first 3):")
    print("-" * 100)
    for i, packet in enumerate(packets[:3]):
        print(f"\nPacket {i+1}:")
        print(packet)
        print(f"Hex (first 128 bytes): {packet.to_hex()[:256]}...")
    print("-" * 100)
    
    if len(node_packets) > 0:
        print(f"\nSample packets for node {selected_node} (first 3):")
        print("-" * 100)
        for i, packet in enumerate(node_packets[:3]):
            print(f"\nPacket {i+1}:")
            print(packet)
            print(f"Hex (first 128 bytes): {packet.to_hex()[:256]}...")
        print("-" * 100)


if __name__ == "__main__":
    main()
