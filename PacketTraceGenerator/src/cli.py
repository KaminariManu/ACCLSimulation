"""
Command-Line Interface for ACCL Packet Trace Generator

This module handles command-line argument parsing and provides
the CLI interface for the trace generator.
"""

import argparse


def parse_arguments():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description='ACCL Packet Trace Generator - Generate packet traces for ACCL network simulation',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    
    # Basic configuration
    parser.add_argument('-r', '--ranks', type=int, default=8,
                       help='Number of ranks in the network')
    parser.add_argument('-n', '--num-operations', type=int, default=50,
                       help='Number of operations to generate')
    parser.add_argument('-s', '--seed', type=int, default=42,
                       help='Random seed for reproducible traces')
    
    # Node selection
    parser.add_argument('--node', type=int, default=3,
                       help='Specific node rank to generate single-node trace for')
    
    # Hardware parameters
    parser.add_argument('--max-packet-size', type=int, default=4096,
                       help='Maximum packet size in bytes (hardware constraint)')
    parser.add_argument('--datapath-width', type=int, default=64,
                       help='Datapath width in bytes')
    parser.add_argument('--eager-threshold', type=int, default=32768,
                       help='Eager protocol threshold in bytes (above this uses rendezvous)')
    
    # Operation weights (for traffic mix customization)
    weights_group = parser.add_argument_group('Operation Weights', 
                                              'Customize the probability distribution of operations')
    weights_group.add_argument('--weight-sendrecv', type=int, default=30,
                              help='Weight for point-to-point send/recv operations')
    weights_group.add_argument('--weight-broadcast', type=int, default=10,
                              help='Weight for broadcast operations')
    weights_group.add_argument('--weight-scatter', type=int, default=8,
                              help='Weight for scatter operations')
    weights_group.add_argument('--weight-gather', type=int, default=8,
                              help='Weight for gather operations')
    weights_group.add_argument('--weight-reduce', type=int, default=8,
                              help='Weight for reduce operations')
    weights_group.add_argument('--weight-allgather', type=int, default=8,
                              help='Weight for allgather operations')
    weights_group.add_argument('--weight-allreduce', type=int, default=10,
                              help='Weight for allreduce operations')
    weights_group.add_argument('--weight-reduce-scatter', type=int, default=6,
                              help='Weight for reduce-scatter operations')
    weights_group.add_argument('--weight-barrier', type=int, default=6,
                              help='Weight for barrier operations')
    weights_group.add_argument('--weight-alltoall', type=int, default=6,
                              help='Weight for alltoall operations')
    
    # Output directories
    parser.add_argument('--output-all', type=str, default='output_all_nodes',
                       help='Output directory for all nodes trace')
    parser.add_argument('--output-node', type=str, default='output_single_node',
                       help='Output directory for single node trace')
    
    # Output format options
    parser.add_argument('--skip-binary', action='store_true',
                       help='Skip binary output generation')
    parser.add_argument('--skip-hex', action='store_true',
                       help='Skip hexadecimal output generation')
    parser.add_argument('--verbose', action='store_true',
                       help='Print verbose output including sample packets')
    
    return parser.parse_args()


def get_operation_weights(args):
    """Extract operation weights from command-line arguments as a dictionary"""
    return {
        'sendrecv': args.weight_sendrecv,
        'broadcast': args.weight_broadcast,
        'scatter': args.weight_scatter,
        'gather': args.weight_gather,
        'reduce': args.weight_reduce,
        'allgather': args.weight_allgather,
        'allreduce': args.weight_allreduce,
        'reduce_scatter': args.weight_reduce_scatter,
        'barrier': args.weight_barrier,
        'alltoall': args.weight_alltoall,
    }


def print_configuration(args, num_packets: int, output_dir_all: str, output_dir_node: str):
    """Print configuration summary"""
    weights = get_operation_weights(args)
    total_weight = sum(weights.values())
    
    print(f"ACCL Packet Trace Generator")
    print(f"=" * 80)
    print(f"Configuration:")
    print(f"  Number of ranks: {args.ranks}")
    print(f"  Number of operations: {args.num_operations}")
    print(f"  Random seed: {args.seed}")
    print(f"  Selected node: {args.node}")
    print(f"  Max packet size: {args.max_packet_size} bytes")
    print(f"  Datapath width: {args.datapath_width} bytes")
    print(f"  Eager threshold: {args.eager_threshold} bytes")
    print(f"  Output directory (all nodes): {output_dir_all}/")
    print(f"  Output directory (single node): {output_dir_node}/")
    print(f"\nOperation Weights (percentages):")
    print(f"  Send/Recv:       {weights['sendrecv']:3d} ({weights['sendrecv']/total_weight*100:5.1f}%)")
    print(f"  Broadcast:       {weights['broadcast']:3d} ({weights['broadcast']/total_weight*100:5.1f}%)")
    print(f"  Scatter:         {weights['scatter']:3d} ({weights['scatter']/total_weight*100:5.1f}%)")
    print(f"  Gather:          {weights['gather']:3d} ({weights['gather']/total_weight*100:5.1f}%)")
    print(f"  Reduce:          {weights['reduce']:3d} ({weights['reduce']/total_weight*100:5.1f}%)")
    print(f"  Allgather:       {weights['allgather']:3d} ({weights['allgather']/total_weight*100:5.1f}%)")
    print(f"  Allreduce:       {weights['allreduce']:3d} ({weights['allreduce']/total_weight*100:5.1f}%)")
    print(f"  Reduce-Scatter:  {weights['reduce_scatter']:3d} ({weights['reduce_scatter']/total_weight*100:5.1f}%)")
    print(f"  Barrier:         {weights['barrier']:3d} ({weights['barrier']/total_weight*100:5.1f}%)")
    print(f"  Alltoall:        {weights['alltoall']:3d} ({weights['alltoall']/total_weight*100:5.1f}%)")
    print(f"=" * 80)
    print()


def print_summary(args, packets, node_packets, output_dir_all: str, output_dir_node: str):
    """Print generation summary"""
    print(f"\nTrace generation complete!")
    print(f"=" * 80)
    print(f"Generated {len(packets)} total packets")
    print(f"Generated {len(node_packets)} packets for node {args.node}")
    print(f"\nOutput directory structure:")
    print(f"  {output_dir_all}/")
    print(f"    ├── accl_packet_trace.txt (human-readable)")
    if not args.skip_hex:
        print(f"    ├── accl_packets_raw_hex.txt (raw hex - ready for encapsulation)")
        print(f"    ├── accl_packets_hex_detailed.txt (detailed hex with breakdown)")
    if not args.skip_binary:
        print(f"    └── accl_packet_trace.bin (binary)")
    print(f"  {output_dir_node}/")
    print(f"    ├── accl_packet_trace_node{args.node}.txt (human-readable)")
    print(f"    ├── accl_packets_raw_hex_node{args.node}.txt (raw hex - ready for encapsulation)")
    print(f"    ├── accl_packets_hex_detailed_node{args.node}.txt (detailed hex with breakdown)")
    print(f"    └── accl_packet_trace_node{args.node}.bin (binary)")
    print()


def print_verbose_samples(args, packets, node_packets):
    """Print sample packets in verbose mode"""
    if args.verbose:
        print("Sample packets from full trace (first 3):")
        print("-" * 100)
        for i, packet in enumerate(packets[:3]):
            print(f"\nPacket {i+1}:")
            print(packet)
            print(f"Hex (first 128 bytes): {packet.to_hex()[:256]}...")
        print("-" * 100)
        
        if len(node_packets) > 0:
            print(f"\nSample packets for node {args.node} (first 3):")
            print("-" * 100)
            for i, packet in enumerate(node_packets[:3]):
                print(f"\nPacket {i+1}:")
                print(packet)
                print(f"Hex (first 128 bytes): {packet.to_hex()[:256]}...")
            print("-" * 100)
