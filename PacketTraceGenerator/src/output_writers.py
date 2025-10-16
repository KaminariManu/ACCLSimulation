"""
Output Writers for ACCL Packet Traces

This module contains functions to write packet traces in various formats:
- Human-readable text
- Raw hexadecimal (ready for encapsulation)
- Detailed hexadecimal (with field breakdowns)
- Binary format
"""

import os
import struct
from typing import List
from .packet import Packet


def write_human_readable(packets: List[Packet], filename: str, generator, num_ranks: int, num_operations: int):
    """Write human-readable trace to file"""
    with open(filename, 'w') as f:
        f.write("=" * 100 + "\n")
        f.write("ACCL PACKET TRACE (Human-Readable)\n")
        f.write("=" * 100 + "\n")
        f.write(f"Configuration:\n")
        f.write(f"  Number of Ranks: {num_ranks}\n")
        f.write(f"  Number of Operations: {num_operations}\n")
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


def write_raw_hex(packets: List[Packet], filename: str):
    """Write raw hex trace to file (ready for encapsulation)"""
    # Create directory if it doesn't exist
    dirname = os.path.dirname(filename)
    if dirname:  # Only create if there's a directory component
        os.makedirs(dirname, exist_ok=True)
    
    with open(filename, 'w') as f:
        f.write("# ACCL Packet Trace - Raw Hexadecimal Format\n")
        f.write("# Ready for encapsulation in transport/ethernet protocols\n")
        f.write(f"# Total packets: {len(packets)}\n")
        f.write(f"# Each line represents one complete packet in hexadecimal\n")
        f.write("#\n")
        
        for i, packet in enumerate(packets):
            f.write(f"{packet.to_hex()}\n")


def write_detailed_hex(packets: List[Packet], filename: str, num_ranks: int, num_operations: int):
    """Write detailed hex trace to file"""
    with open(filename, 'w') as f:
        f.write("=" * 100 + "\n")
        f.write("ACCL PACKET TRACE (Hexadecimal Format - Detailed)\n")
        f.write("=" * 100 + "\n")
        f.write(f"Configuration:\n")
        f.write(f"  Number of Ranks: {num_ranks}\n")
        f.write(f"  Number of Operations: {num_operations}\n")
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


def write_binary(packets: List[Packet], filename: str, num_ranks: int):
    """Write binary trace to file"""
    # Create directory if it doesn't exist
    dirname = os.path.dirname(filename)
    if dirname:  # Only create if there's a directory component
        os.makedirs(dirname, exist_ok=True)
    
    with open(filename, 'wb') as f:
        # Write header
        header = struct.pack('>4sIII', 
                           b'ACCL',           # Protocol identifier
                           1,                 # Version
                           num_ranks,         # Number of ranks
                           len(packets))      # Number of packets
        f.write(header)
        
        # Write all packets
        for packet in packets:
            f.write(packet.to_binary())


def write_node_trace(packets: List[Packet], node: int, output_dir: str, generator, num_ranks: int, num_operations: int):
    """Write per-node traces for a specific node"""
    # Filter packets that involve the selected node (as source or destination)
    node_packets = [p for p in packets if p.src_rank == node or p.dst_rank == node]
    
    # Define per-node output files in the node-specific directory
    NODE_OUTPUT_FILE = os.path.join(output_dir, f"accl_packet_trace_node{node}.txt")
    NODE_OUTPUT_HEX_RAW_FILE = os.path.join(output_dir, f"accl_packets_raw_hex_node{node}.txt")
    NODE_OUTPUT_HEX_DETAILED_FILE = os.path.join(output_dir, f"accl_packets_hex_detailed_node{node}.txt")
    NODE_OUTPUT_BIN_FILE = os.path.join(output_dir, f"accl_packet_trace_node{node}.bin")
    
    # Write human-readable trace for selected node
    with open(NODE_OUTPUT_FILE, 'w') as f:
        f.write("=" * 100 + "\n")
        f.write(f"ACCL PACKET TRACE FOR NODE {node} (Human-Readable)\n")
        f.write("=" * 100 + "\n")
        f.write(f"Configuration:\n")
        f.write(f"  Selected Node: {node}\n")
        f.write(f"  Total Network Ranks: {num_ranks}\n")
        f.write(f"  Total Network Operations: {num_operations}\n")
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
        f.write(f"NODE {node} STATISTICS\n")
        f.write("=" * 100 + "\n")
        
        # Calculate statistics
        stats = {}
        for packet in node_packets:
            op = packet.operation.value
            stats[op] = stats.get(op, 0) + 1
        
        f.write(f"Total Packets involving Node {node}: {len(node_packets)}\n")
        f.write(f"Final Timestamp: {generator.timestamp}\n")
        f.write(f"\nPackets by Operation:\n")
        for op, count in sorted(stats.items()):
            f.write(f"  {op:20s}: {count:5d} packets\n")
        
        # Direction statistics
        sent_packets = [p for p in node_packets if p.src_rank == node]
        recv_packets = [p for p in node_packets if p.dst_rank == node]
        
        f.write(f"\nPackets by Direction:\n")
        f.write(f"  Sent by node {node:2d}:     {len(sent_packets):5d} packets\n")
        f.write(f"  Received by node {node:2d}: {len(recv_packets):5d} packets\n")
        
        # Data volume
        total_data = sum(p.data_length for p in node_packets)
        f.write(f"\nTotal Data Volume: {total_data:,} bytes ({total_data / 1024 / 1024:.2f} MB)\n")
        
        f.write("=" * 100 + "\n")
    
    # Write raw hex trace for selected node
    with open(NODE_OUTPUT_HEX_RAW_FILE, 'w') as f:
        f.write(f"# ACCL Packet Trace for Node {node} - Raw Hexadecimal Format\n")
        f.write("# Ready for encapsulation in transport/ethernet protocols\n")
        f.write(f"# Selected node: {node}\n")
        f.write(f"# Packets involving this node: {len(node_packets)}\n")
        f.write(f"# Each line represents one complete packet in hexadecimal\n")
        f.write("#\n")
        
        for packet in node_packets:
            f.write(f"{packet.to_hex()}\n")
    
    # Write detailed hex trace for selected node
    with open(NODE_OUTPUT_HEX_DETAILED_FILE, 'w') as f:
        f.write("=" * 100 + "\n")
        f.write(f"ACCL PACKET TRACE FOR NODE {node} (Hexadecimal Format - Detailed)\n")
        f.write("=" * 100 + "\n")
        f.write(f"Configuration:\n")
        f.write(f"  Selected Node: {node}\n")
        f.write(f"  Total Network Ranks: {num_ranks}\n")
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
                           num_ranks,            # Number of ranks
                           len(node_packets))    # Number of packets for this node
        f.write(header)
        
        # Write all packets for this node
        for packet in node_packets:
            f.write(packet.to_binary())
    
    return node_packets
