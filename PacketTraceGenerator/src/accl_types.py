"""
ACCL Types and Enumerations

This module contains the core data types, enumerations, and constants
used throughout the ACCL packet trace generator.
"""

from enum import Enum
from dataclasses import dataclass


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
