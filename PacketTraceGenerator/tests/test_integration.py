"""
Integration tests for the complete packet trace generation system

Tests end-to-end workflows including trace generation,
file output, and format validation.
"""

import unittest
import sys
import os
import tempfile
import shutil
from pathlib import Path

# Add parent directory to path to import src modules
sys.path.insert(0, str(Path(__file__).parent.parent))

from src.trace_generator import ACCLTraceGenerator
from src.output_writers import write_binary, write_raw_hex
from src.accl_types import Operation


class TestEndToEndGeneration(unittest.TestCase):
    """Test complete trace generation workflow"""
    
    def setUp(self):
        """Set up test fixtures"""
        self.test_dir = tempfile.mkdtemp()
    
    def tearDown(self):
        """Clean up test directory"""
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir)
    
    def test_complete_workflow(self):
        """Test complete trace generation and output workflow"""
        # Initialize generator
        generator = ACCLTraceGenerator(
            num_ranks=8,
            seed=42,
            max_packet_size=4096
        )
        
        # Generate trace
        packets = generator.generate_trace(num_operations=50)
        self.assertGreater(len(packets), 0)
        
        # Write binary output
        binary_file = os.path.join(self.test_dir, "trace.bin")
        write_binary(packets, binary_file, num_ranks=8)
        self.assertTrue(os.path.exists(binary_file))
        
        # Write text output
        text_file = os.path.join(self.test_dir, "trace.txt")
        write_raw_hex(packets, text_file)
        self.assertTrue(os.path.exists(text_file))
    
    def test_multi_node_generation(self):
        """Test generating traces for multiple nodes"""
        generator = ACCLTraceGenerator(num_ranks=8, seed=42)
        all_packets = generator.generate_trace(num_operations=30)
        
        # Generate per-node traces
        for node in range(8):
            node_packets = [p for p in all_packets if p.src_rank == node]
            
            output_file = os.path.join(self.test_dir, f"node_{node}.bin")
            write_binary(node_packets, output_file, num_ranks=8)
            
            self.assertTrue(os.path.exists(output_file))
    
    def test_different_configurations(self):
        """Test generation with different configurations"""
        configs = [
            {'num_ranks': 4, 'seed': 10},
            {'num_ranks': 8, 'seed': 20},
            {'num_ranks': 16, 'seed': 30},
        ]
        
        for i, config in enumerate(configs):
            generator = ACCLTraceGenerator(**config)
            packets = generator.generate_trace(num_operations=20)
            
            output_file = os.path.join(self.test_dir, f"config_{i}.bin")
            write_binary(packets, output_file, num_ranks=config['num_ranks'])
            
            self.assertTrue(os.path.exists(output_file))
    
    def test_custom_weights_workflow(self):
        """Test workflow with custom operation weights"""
        generator = ACCLTraceGenerator(num_ranks=8, seed=42)
        
        custom_weights = {
            'sendrecv': 20,
            'broadcast': 20,
            'allreduce': 30,
            'allgather': 20,
            'scatter': 2,
            'gather': 2,
            'reduce': 2,
            'reduce_scatter': 1,
            'barrier': 1,
            'alltoall': 2
        }
        
        packets = generator.generate_trace(
            num_operations=50,
            operation_weights=custom_weights
        )
        
        # Write output
        output_file = os.path.join(self.test_dir, "custom_weights.bin")
        write_binary(packets, output_file, num_ranks=8)
        
        self.assertTrue(os.path.exists(output_file))
        self.assertGreater(os.path.getsize(output_file), 0)


class TestOperationDistribution(unittest.TestCase):
    """Test operation distribution in generated traces"""
    
    def test_operation_variety(self):
        """Test that traces contain variety of operations"""
        generator = ACCLTraceGenerator(num_ranks=8, seed=42)
        packets = generator.generate_trace(num_operations=100)
        
        # Count operation types
        op_counts = {}
        for packet in packets:
            op = packet.operation
            op_counts[op] = op_counts.get(op, 0) + 1
        
        # Should have at least 5 different operation types
        self.assertGreaterEqual(len(op_counts), 5)
    
    def test_weight_influence(self):
        """Test that weights influence operation distribution"""
        generator = ACCLTraceGenerator(num_ranks=8, seed=42)
        
        # Generate with heavy broadcast weight
        broadcast_weights = {
            'sendrecv': 10,
            'broadcast': 80,  # Heavy weight
            'scatter': 2,
            'gather': 2,
            'reduce': 1,
            'allgather': 1,
            'allreduce': 1,
            'reduce_scatter': 1,
            'barrier': 1,
            'alltoall': 1
        }
        
        packets = generator.generate_trace(
            num_operations=50,
            operation_weights=broadcast_weights
        )
        
        # Count broadcasts
        broadcast_count = sum(1 for p in packets if p.operation == Operation.BROADCAST)
        
        # Should have significant number of broadcasts
        # (Note: actual count depends on implementation)
        self.assertGreater(broadcast_count, 0)


class TestScalability(unittest.TestCase):
    """Test system scalability with different parameters"""
    
    def setUp(self):
        """Set up test fixtures"""
        self.test_dir = tempfile.mkdtemp()
    
    def tearDown(self):
        """Clean up test directory"""
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir)
    
    def test_large_rank_count(self):
        """Test generation with large number of ranks"""
        for num_ranks in [16, 32, 64]:
            generator = ACCLTraceGenerator(num_ranks=num_ranks, seed=42)
            packets = generator.generate_trace(num_operations=10)
            
            # Should generate valid packets
            self.assertGreater(len(packets), 0)
            
            # All ranks should be within bounds
            for packet in packets:
                self.assertLess(packet.src_rank, num_ranks)
                self.assertLess(packet.dst_rank, num_ranks)
    
    def test_large_operation_count(self):
        """Test generation with large number of operations"""
        generator = ACCLTraceGenerator(num_ranks=8, seed=42)
        
        for num_ops in [100, 500, 1000]:
            packets = generator.generate_trace(num_operations=num_ops)
            
            # Should generate packets
            self.assertGreater(len(packets), 0)
    
    def test_memory_efficiency(self):
        """Test that large traces can be generated efficiently"""
        generator = ACCLTraceGenerator(num_ranks=8, seed=42)
        
        # Generate large trace
        packets = generator.generate_trace(num_operations=500)
        
        # Write to file (tests memory handling)
        output_file = os.path.join(self.test_dir, "large_trace.bin")
        write_binary(packets, output_file, num_ranks=8)
        
        self.assertTrue(os.path.exists(output_file))


class TestDeterminism(unittest.TestCase):
    """Test deterministic behavior with seeds"""
    
    def test_same_seed_same_output(self):
        """Test that same seed produces identical output"""
        seed = 12345
        
        gen1 = ACCLTraceGenerator(num_ranks=8, seed=seed)
        packets1 = gen1.generate_trace(num_operations=20)
        
        gen2 = ACCLTraceGenerator(num_ranks=8, seed=seed)
        packets2 = gen2.generate_trace(num_operations=20)
        
        # Should have same number of packets
        self.assertEqual(len(packets1), len(packets2))
        
        # Packets should be identical
        for p1, p2 in zip(packets1, packets2):
            self.assertEqual(p1.operation, p2.operation)
            self.assertEqual(p1.src_rank, p2.src_rank)
            self.assertEqual(p1.dst_rank, p2.dst_rank)
            self.assertEqual(p1.data_length, p2.data_length)
    
    def test_different_seed_different_output(self):
        """Test that different seeds produce different output"""
        gen1 = ACCLTraceGenerator(num_ranks=8, seed=100)
        packets1 = gen1.generate_trace(num_operations=20)
        
        gen2 = ACCLTraceGenerator(num_ranks=8, seed=200)
        packets2 = gen2.generate_trace(num_operations=20)
        
        # Should produce different sequences
        # (very unlikely to be identical with different seeds)
        different = False
        for p1, p2 in zip(packets1, packets2):
            if (p1.operation != p2.operation or 
                p1.src_rank != p2.src_rank or 
                p1.dst_rank != p2.dst_rank):
                different = True
                break
        
        self.assertTrue(different, "Different seeds should produce different traces")


if __name__ == '__main__':
    unittest.main()
