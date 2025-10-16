"""
ACCL Collective Operations Generators

This module contains functions to generate packet traces for all
ACCL collective communication operations.
"""

import random
from typing import List
from .accl_types import PacketType, Operation, CompressionFlag
from .packet import Packet


def generate_broadcast(generator) -> List[Packet]:
    """Generate broadcast packets"""
    packets = []
    root = random.randint(0, generator.num_ranks - 1)
    size = random.randint(1000, 64000)
    tag = generator.TAG_ANY
    compression = random.choice([0, CompressionFlag.ETH_COMPRESSED.value])
    
    if size <= generator.MAX_EAGER_SIZE:
        # Eager broadcast - root sends to all
        session_id = generator.get_next_session_id(root)
        num_segments = (size + generator.MAX_PACKETSIZE - 1) // generator.MAX_PACKETSIZE
        
        for seg in range(num_segments):
            seg_size = min(generator.MAX_PACKETSIZE, size - seg * generator.MAX_PACKETSIZE)
            
            for dst in range(generator.num_ranks):
                if dst != root:
                    packet = Packet(
                        timestamp=generator.timestamp,
                        packet_type=PacketType.COLLECTIVE_DATA,
                        operation=Operation.BROADCAST,
                        src_rank=root,
                        dst_rank=dst,
                        tag=tag,
                        session_id=session_id,
                        sequence_number=generator.get_next_sequence(root, dst),
                        data_length=seg_size,
                        compression=compression,
                        is_host_memory=False,
                        payload_segment=seg,
                        total_segments=num_segments
                    )
                    packets.append(packet)
            
            generator.advance_time(seg_size // 32)
    else:
        # Rendezvous broadcast - address exchange then RDMA
        # Store addresses for each destination
        dest_addresses = {}
        
        # Phase 1: Each destination sends its buffer address to root
        for dst in range(generator.num_ranks):
            if dst != root:
                address = random.randint(0x1000000, 0xFFFFFFFF) << 32 | random.randint(0, 0xFFFFFFFF)
                dest_addresses[dst] = address  # Store for later use
                
                # Address packet from dst to root
                generator.advance_time(20)
                addr_packet = Packet(
                    timestamp=generator.timestamp,
                    packet_type=PacketType.RNDZV_ADDR,
                    operation=Operation.BROADCAST,
                    src_rank=dst,
                    dst_rank=root,
                    tag=tag,
                    session_id=generator.get_next_session_id(dst),
                    sequence_number=generator.get_next_sequence(dst, root),
                    data_length=32,
                    compression=0,
                    is_host_memory=False,
                    address=address
                )
                packets.append(addr_packet)
        
        # Phase 2: Root sends RDMA data to all destinations using their addresses
        for dst in range(generator.num_ranks):
            if dst != root:
                generator.advance_time(30)
                
                rdma_packet = Packet(
                    timestamp=generator.timestamp,
                    packet_type=PacketType.RNDZV_DATA,
                    operation=Operation.BROADCAST,
                    src_rank=root,
                    dst_rank=dst,
                    tag=tag,
                    session_id=generator.get_next_session_id(root),
                    sequence_number=generator.get_next_sequence(root, dst),
                    data_length=size,
                    compression=0,
                    is_host_memory=False,
                    address=dest_addresses[dst]  # Use the stored address!
                )
                packets.append(rdma_packet)
    
    return packets


def generate_scatter(generator) -> List[Packet]:
    """Generate scatter packets"""
    packets = []
    root = random.randint(0, generator.num_ranks - 1)
    chunk_size = random.randint(500, 8000)
    tag = generator.TAG_ANY
    
    # Root sends different chunks to each rank (segmented if needed)
    for dst in range(generator.num_ranks):
        session_id = generator.get_next_session_id(root)
        segments = generator.generate_collective_packet_segments(
            Operation.SCATTER, root, dst, chunk_size, tag, session_id
        )
        packets.extend(segments)
    
    return packets


def generate_gather(generator) -> List[Packet]:
    """Generate gather packets (ring-based)"""
    packets = []
    root = random.randint(0, generator.num_ranks - 1)
    chunk_size = random.randint(500, 8000)
    tag = generator.TAG_ANY
    
    # Ring gather - each rank sends to next, root receives from all (segmented)
    for i in range(generator.num_ranks):
        if i != root:
            next_rank = (i + 1) % generator.num_ranks
            session_id = generator.get_next_session_id(i)
            segments = generator.generate_collective_packet_segments(
                Operation.GATHER, i, next_rank, chunk_size, tag, session_id
            )
            packets.extend(segments)
    
    return packets


def generate_reduce(generator) -> List[Packet]:
    """Generate reduce packets (ring-based)"""
    packets = []
    root = random.randint(0, generator.num_ranks - 1)
    size = random.randint(1000, 16000)
    tag = generator.TAG_ANY
    func_id = random.randint(0, 5)  # Reduction function ID
    
    # Ring reduce - each rank reduces and forwards (segmented)
    for i in range(generator.num_ranks - 1):
        src = (root + i + 1) % generator.num_ranks
        dst = (root + i + 2) % generator.num_ranks
        if dst == root:
            dst = root  # Last one goes to root
        
        session_id = generator.get_next_session_id(src)
        segments = generator.generate_collective_packet_segments(
            Operation.REDUCE, src, dst, size, tag | (func_id << 16), session_id
        )
        packets.extend(segments)
    
    return packets


def generate_allgather(generator) -> List[Packet]:
    """Generate allgather packets (ring-based)"""
    packets = []
    chunk_size = random.randint(500, 8000)
    tag = generator.TAG_ANY
    
    # Ring allgather - P-1 steps, each rank sends and receives (segmented)
    for step in range(generator.num_ranks - 1):
        for rank in range(generator.num_ranks):
            next_rank = (rank + 1) % generator.num_ranks
            session_id = generator.get_next_session_id(rank)
            segments = generator.generate_collective_packet_segments(
                Operation.ALLGATHER, rank, next_rank, chunk_size, tag, session_id
            )
            packets.extend(segments)
        
        generator.advance_time(25)
    
    return packets


def generate_allreduce(generator) -> List[Packet]:
    """Generate allreduce packets (reduce-scatter + allgather)"""
    packets = []
    chunk_size = random.randint(500, 8000)
    tag = generator.TAG_ANY
    func_id = random.randint(0, 5)
    
    # Phase 1: Reduce-scatter (P-1 steps, segmented)
    for step in range(generator.num_ranks - 1):
        for rank in range(generator.num_ranks):
            next_rank = (rank + 1) % generator.num_ranks
            session_id = generator.get_next_session_id(rank)
            segments = generator.generate_collective_packet_segments(
                Operation.ALLREDUCE, rank, next_rank, chunk_size, 
                tag | (func_id << 16) | (1 << 24), session_id
            )
            packets.extend(segments)
        
        generator.advance_time(30)
    
    # Phase 2: Allgather (P-1 steps, segmented)
    for step in range(generator.num_ranks - 1):
        for rank in range(generator.num_ranks):
            next_rank = (rank + 1) % generator.num_ranks
            session_id = generator.get_next_session_id(rank)
            segments = generator.generate_collective_packet_segments(
                Operation.ALLREDUCE, rank, next_rank, chunk_size,
                tag | (2 << 24), session_id
            )
            packets.extend(segments)
        
        generator.advance_time(25)
    
    return packets


def generate_reduce_scatter(generator) -> List[Packet]:
    """Generate reduce-scatter packets"""
    packets = []
    chunk_size = random.randint(500, 8000)
    tag = generator.TAG_ANY
    func_id = random.randint(0, 5)
    
    # Ring reduce-scatter - P-1 steps (segmented)
    for step in range(generator.num_ranks - 1):
        for rank in range(generator.num_ranks):
            next_rank = (rank + 1) % generator.num_ranks
            session_id = generator.get_next_session_id(rank)
            segments = generator.generate_collective_packet_segments(
                Operation.REDUCE_SCATTER, rank, next_rank, chunk_size,
                tag | (func_id << 16), session_id
            )
            packets.extend(segments)
        
        generator.advance_time(30)
    
    return packets


def generate_barrier(generator) -> List[Packet]:
    """Generate barrier packets"""
    packets = []
    root = 0  # Barrier typically uses rank 0
    tag = generator.TAG_ANY
    
    # Phase 1: Gather notifications to rank 0
    for src in range(1, generator.num_ranks):
        packet = Packet(
            timestamp=generator.timestamp,
            packet_type=PacketType.RNDZV_ADDR,
            operation=Operation.BARRIER,
            src_rank=src,
            dst_rank=root,
            tag=tag,
            session_id=generator.get_next_session_id(src),
            sequence_number=generator.get_next_sequence(src, root),
            data_length=32,  # Small control packet
            compression=0,
            is_host_memory=False,
            address=0
        )
        packets.append(packet)
        generator.advance_time(20)
    
    # Phase 2: Scatter notifications from rank 0
    for dst in range(1, generator.num_ranks):
        packet = Packet(
            timestamp=generator.timestamp,
            packet_type=PacketType.RNDZV_ADDR,
            operation=Operation.BARRIER,
            src_rank=root,
            dst_rank=dst,
            tag=tag,
            session_id=generator.get_next_session_id(root),
            sequence_number=generator.get_next_sequence(root, dst),
            data_length=32,  # Small control packet
            compression=0,
            is_host_memory=False,
            address=0
        )
        packets.append(packet)
        generator.advance_time(20)
    
    return packets


def generate_alltoall(generator) -> List[Packet]:
    """Generate alltoall packets"""
    packets = []
    chunk_size = random.randint(500, 4000)
    tag = generator.TAG_ANY
    
    # Each rank sends to every other rank
    for src in range(generator.num_ranks):
        for dst in range(generator.num_ranks):
            if src != dst:
                packet = Packet(
                    timestamp=generator.timestamp,
                    packet_type=PacketType.COLLECTIVE_DATA,
                    operation=Operation.ALLTOALL,
                    src_rank=src,
                    dst_rank=dst,
                    tag=tag,
                    session_id=generator.get_next_session_id(src),
                    sequence_number=generator.get_next_sequence(src, dst),
                    data_length=chunk_size,
                    compression=0,
                    is_host_memory=False
                )
                packets.append(packet)
        
        generator.advance_time(chunk_size // 32)
    
    return packets
