"""
ACCL Packet Trace Generator Package

A modular packet trace generator for the ACCL (Alveo Collective Communication Library) protocol.

Modules:
    - accl_types: Core data types and enumerations
    - packet: Packet class with serialization methods
    - trace_generator: Main trace generation logic
    - collective_operations: Collective communication operation generators
    - output_writers: Output file writers in various formats
    - cli: Command-line interface
    - main: Main entry point

Usage:
    python main.py [options]
    
    Or import as a library:
    from trace_generator import ACCLTraceGenerator
    
    generator = ACCLTraceGenerator(num_ranks=8)
    packets = generator.generate_trace(num_operations=50)
"""

__version__ = "2.0.0"
__author__ = "ACCL Team"

# Import main classes for easy access
from .accl_types import PacketType, Operation, CompressionFlag
from .packet import Packet
from .trace_generator import ACCLTraceGenerator

__all__ = [
    'PacketType',
    'Operation',
    'CompressionFlag',
    'Packet',
    'ACCLTraceGenerator',
]
