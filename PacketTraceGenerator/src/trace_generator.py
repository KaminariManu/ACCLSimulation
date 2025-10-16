"""
ACCL Trace Generator - Core Generator Class

This module contains the main ACCLTraceGenerator class that generates
packet traces for all ACCL operations including point-to-point and
collective communications.
"""

import random
from typing import List
from .accl_types import PacketType, Operation, CompressionFlag
from .packet import Packet


class ACCLTraceGenerator:
    """Generates ACCL packet traces"""
    
    def __init__(self, num_ranks: int = 8, seed: int = 42, 
                 max_packet_size: int = 4096, datapath_width: int = 64, 
                 eager_threshold: int = 32768):
        self.num_ranks = num_ranks
        self.timestamp = 0
        self.session_counters = [0] * num_ranks
        self.sequence_counters = [[0] * num_ranks for _ in range(num_ranks)]
        random.seed(seed)
        
        # ACCL constants (configurable)
        self.MAX_PACKETSIZE = max_packet_size
        self.DATAPATH_WIDTH_BYTES = datapath_width
        self.MAX_EAGER_SIZE = eager_threshold
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
    
    def generate_collective_packet_segments(self, operation: Operation, src: int, dst: int, 
                                           total_size: int, tag: int, session_id: int) -> List[Packet]:
        """
        Helper to generate segmented packets for collective operations.
        Breaks large data into MAX_PACKETSIZE chunks to match hardware constraints.
        """
        packets = []
        num_segments = (total_size + self.MAX_PACKETSIZE - 1) // self.MAX_PACKETSIZE
        
        for seg in range(num_segments):
            seg_size = min(self.MAX_PACKETSIZE, total_size - seg * self.MAX_PACKETSIZE)
            seq = self.get_next_sequence(src, dst)
            
            packet = Packet(
                timestamp=self.timestamp,
                packet_type=PacketType.COLLECTIVE_DATA,
                operation=operation,
                src_rank=src,
                dst_rank=dst,
                tag=tag,
                session_id=session_id,
                sequence_number=seq,
                data_length=seg_size,
                compression=0,
                is_host_memory=False,
                payload_segment=seg,
                total_segments=num_segments
            )
            packets.append(packet)
            self.advance_time(seg_size // 64 + 10)
        
        return packets
    
    def generate_trace(self, num_operations: int = 50, operation_weights: dict = None) -> List[Packet]:
        """
        Generate a complete trace with mixed operations
        
        Args:
            num_operations: Number of operations to generate
            operation_weights: Dictionary of operation weights. Keys: 'sendrecv', 'broadcast',
                             'scatter', 'gather', 'reduce', 'allgather', 'allreduce',
                             'reduce_scatter', 'barrier', 'alltoall'. If None, uses defaults.
        
        Returns:
            List of generated Packet objects
        """
        all_packets = []
        
        # Import collective operation generators
        from .collective_operations import (
            generate_broadcast, generate_scatter, generate_gather,
            generate_reduce, generate_allgather, generate_allreduce,
            generate_reduce_scatter, generate_barrier, generate_alltoall
        )
        
        # Default operation weights if not provided
        if operation_weights is None:
            operation_weights = {
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
        
        # Define operation weights. These are chosen to simulate a typical, general-purpose
        # workload you might find in High-Performance Computing (HPC) or distributed
        # machine learning applications. They are a reasonable approximation based on
        # common communication patterns.
        #
        # 1. Highest Weight: Point-to-Point (default 30%) - Foundational and frequent.
        # 2. High Weight: Common Collectives (default 10% each) - `broadcast` and `allreduce`
        #    are cornerstones of ML and data distribution.
        # 3. Medium Weight: Standard Building Blocks (default 8% each) - Essential MPI-style
        #    collectives like `scatter`, `gather`, etc.
        # 4. Lower Weight: Specialized/Less Frequent (default 6% each) - `barrier`, `alltoall`, etc.
        #
        # These values can be configured via CLI arguments to simulate different application profiles.
        operations = [
            (self.generate_send_recv_pair, operation_weights['sendrecv']),
            (lambda: generate_broadcast(self), operation_weights['broadcast']),
            (lambda: generate_scatter(self), operation_weights['scatter']),
            (lambda: generate_gather(self), operation_weights['gather']),
            (lambda: generate_reduce(self), operation_weights['reduce']),
            (lambda: generate_allgather(self), operation_weights['allgather']),
            (lambda: generate_allreduce(self), operation_weights['allreduce']),
            (lambda: generate_reduce_scatter(self), operation_weights['reduce_scatter']),
            (lambda: generate_barrier(self), operation_weights['barrier']),
            (lambda: generate_alltoall(self), operation_weights['alltoall']),
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
