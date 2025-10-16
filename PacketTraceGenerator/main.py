#!/usr/bin/env python3
"""
ACCL Packet Trace Generator - Main Entry Point

This is the main script that orchestrates the packet trace generation
by using modular components.

Usage:
    python main.py [options]
    
Example:
    python main.py --ranks 16 --num-operations 100 --node 5
    python main.py --help
"""

import os
import sys

# Import modular components from src folder
from src.cli import parse_arguments, print_configuration, print_summary, print_verbose_samples, get_operation_weights
from src.trace_generator import ACCLTraceGenerator
from src.output_writers import (
    write_human_readable, write_raw_hex, write_detailed_hex,
    write_binary, write_node_trace
)


def main():
    """Main function to generate packet trace"""
    
    # Parse command line arguments
    args = parse_arguments()
    
    # Configuration from arguments
    NUM_RANKS = args.ranks
    NUM_OPERATIONS = args.num_operations
    selected_node = args.node
    
    # Validate arguments
    if selected_node < 0 or selected_node >= NUM_RANKS:
        print(f"Error: Node {selected_node} is out of range [0, {NUM_RANKS-1}]")
        sys.exit(1)
    
    # Create output directories
    OUTPUT_DIR_ALL = args.output_all
    OUTPUT_DIR_NODE = args.output_node
    
    os.makedirs(OUTPUT_DIR_ALL, exist_ok=True)
    os.makedirs(OUTPUT_DIR_NODE, exist_ok=True)
    
    # Define output files with directory paths
    OUTPUT_FILE = os.path.join(OUTPUT_DIR_ALL, "accl_packet_trace.txt")
    OUTPUT_HEX_RAW_FILE = os.path.join(OUTPUT_DIR_ALL, "accl_packets_raw_hex.txt")
    OUTPUT_HEX_DETAILED_FILE = os.path.join(OUTPUT_DIR_ALL, "accl_packets_hex_detailed.txt")
    OUTPUT_BIN_FILE = os.path.join(OUTPUT_DIR_ALL, "accl_packet_trace.bin")
    
    # Print configuration
    print_configuration(args, None, OUTPUT_DIR_ALL, OUTPUT_DIR_NODE)
    
    # Generate trace with configurable parameters
    print("Generating packet trace...")
    generator = ACCLTraceGenerator(
        num_ranks=NUM_RANKS,
        seed=args.seed,
        max_packet_size=args.max_packet_size,
        datapath_width=args.datapath_width,
        eager_threshold=args.eager_threshold
    )
    
    # Get operation weights from command-line arguments
    weights = get_operation_weights(args)
    packets = generator.generate_trace(num_operations=NUM_OPERATIONS, operation_weights=weights)
    
    print(f"Generated {len(packets)} packets")
    
    # Write human-readable trace to file
    print("Writing human-readable trace...")
    write_human_readable(packets, OUTPUT_FILE, generator, NUM_RANKS, NUM_OPERATIONS)
    
    # Write hex files (optional)
    if not args.skip_hex:
        print("Generating hexadecimal packet traces...")
        write_raw_hex(packets, OUTPUT_HEX_RAW_FILE)
        
        print("Generating detailed hexadecimal packet trace...")
        write_detailed_hex(packets, OUTPUT_HEX_DETAILED_FILE, NUM_RANKS, NUM_OPERATIONS)
    
    # Write binary trace to file (optional)
    if not args.skip_binary:
        print("Generating binary packet trace...")
        write_binary(packets, OUTPUT_BIN_FILE, NUM_RANKS)
    
    # Generate per-node traces for the selected node
    print(f"\nGenerating per-node trace for selected node: {selected_node}")
    node_packets = write_node_trace(
        packets, selected_node, OUTPUT_DIR_NODE, 
        generator, NUM_RANKS, NUM_OPERATIONS
    )
    
    # Print summary
    print_summary(args, packets, node_packets, OUTPUT_DIR_ALL, OUTPUT_DIR_NODE)
    
    # Print sample packets (if verbose mode)
    print_verbose_samples(args, packets, node_packets)


if __name__ == "__main__":
    main()
