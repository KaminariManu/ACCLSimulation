"""
Unit tests for the ACCLTraceGenerator class

Tests trace generation, session management, sequence numbering,
and overall packet generation functionality.
"""

import unittest
import sys
from pathlib import Path

# Add parent directory to path to import src modules
sys.path.insert(0, str(Path(__file__).parent.parent))

from src.trace_generator import ACCLTraceGenerator
from src.accl_types import Operation


class TestACCLTraceGenerator(unittest.TestCase):
    """Test cases for ACCLTraceGenerator class"""
    
    def setUp(self):
        """Set up test fixtures"""
        self.generator = ACCLTraceGenerator(
            num_ranks=8,
            seed=42,
            max_packet_size=4096,
            datapath_width=64,
            eager_threshold=32768
        )
    
    def test_generator_initialization(self):
        """Test generator initialization"""
        self.assertEqual(self.generator.num_ranks, 8)
        self.assertEqual(self.generator.MAX_PACKETSIZE, 4096)
        self.assertEqual(self.generator.DATAPATH_WIDTH_BYTES, 64)
        self.assertEqual(self.generator.MAX_EAGER_SIZE, 32768)
        self.assertEqual(self.generator.timestamp, 0)
    
    def test_session_id_generation(self):
        """Test session ID generation"""
        rank = 0
        session_ids = set()
        
        # Generate multiple session IDs
        for _ in range(10):
            sid = self.generator.get_next_session_id(rank)
            session_ids.add(sid)
        
        # All session IDs should be unique
        self.assertEqual(len(session_ids), 10)
        
        # Session IDs should be sequential
        self.assertEqual(session_ids, set(range(10)))
    
    def test_sequence_number_generation(self):
        """Test sequence number generation"""
        src, dst = 0, 1
        sequences = []
        
        # Generate multiple sequence numbers
        for _ in range(5):
            seq = self.generator.get_next_sequence(src, dst)
            sequences.append(seq)
        
        # Sequence numbers should be sequential
        self.assertEqual(sequences, [0, 1, 2, 3, 4])
        
        # Different src-dst pairs should have independent sequences
        seq_other = self.generator.get_next_sequence(1, 2)
        self.assertEqual(seq_other, 0)
    
    def test_timestamp_advancement(self):
        """Test timestamp advancement"""
        initial_timestamp = self.generator.timestamp
        
        # Advance with specific cycles
        self.generator.advance_time(100)
        self.assertEqual(self.generator.timestamp, initial_timestamp + 100)
        
        # Advance with random cycles
        self.generator.advance_time()
        self.assertGreater(self.generator.timestamp, initial_timestamp + 100)
    
    def test_trace_generation_basic(self):
        """Test basic trace generation"""
        packets = self.generator.generate_trace(num_operations=10)
        
        # Should generate packets
        self.assertIsNotNone(packets)
        self.assertIsInstance(packets, list)
        self.assertGreater(len(packets), 0)
    
    def test_trace_generation_with_custom_weights(self):
        """Test trace generation with custom operation weights"""
        custom_weights = {
            'sendrecv': 50,
            'broadcast': 10,
            'scatter': 5,
            'gather': 5,
            'reduce': 5,
            'allgather': 5,
            'allreduce': 10,
            'reduce_scatter': 3,
            'barrier': 3,
            'alltoall': 4
        }
        
        packets = self.generator.generate_trace(
            num_operations=20,
            operation_weights=custom_weights
        )
        
        # Should generate packets
        self.assertGreater(len(packets), 0)
    
    def test_trace_generation_different_sizes(self):
        """Test trace generation with different operation counts"""
        for num_ops in [5, 10, 50, 100]:
            packets = self.generator.generate_trace(num_operations=num_ops)
            self.assertGreater(len(packets), 0)
    
    def test_all_operations_generated(self):
        """Test that all operation types can be generated"""
        # Generate a large trace to ensure all operations appear
        packets = self.generator.generate_trace(num_operations=200)
        
        # Extract unique operations
        operations = set(packet.operation for packet in packets)
        
        # Should have multiple operation types
        self.assertGreater(len(operations), 5)
    
    def test_packet_validity(self):
        """Test that generated packets are valid"""
        packets = self.generator.generate_trace(num_operations=20)
        
        for packet in packets:
            # Check rank validity
            self.assertGreaterEqual(packet.src_rank, 0)
            self.assertLess(packet.src_rank, self.generator.num_ranks)
            self.assertGreaterEqual(packet.dst_rank, 0)
            self.assertLess(packet.dst_rank, self.generator.num_ranks)
            
            # Check data length is positive
            self.assertGreaterEqual(packet.data_length, 0)
            
            # Check timestamp is non-negative
            self.assertGreaterEqual(packet.timestamp, 0)
    
    def test_deterministic_generation(self):
        """Test that generation is deterministic with same seed"""
        import random
        
        # First run
        random.seed(100)  # Reset global random state
        gen1 = ACCLTraceGenerator(num_ranks=8, seed=100)
        packets1 = gen1.generate_trace(num_operations=10)
        
        # Second run with same seed
        random.seed(100)  # Reset global random state again
        gen2 = ACCLTraceGenerator(num_ranks=8, seed=100)
        packets2 = gen2.generate_trace(num_operations=10)
        
        # With same seed, should generate exactly the same packets
        self.assertEqual(len(packets1), len(packets2),
                        "Same seed should produce same number of packets")
        
        # Check first several packets are identical
        for i in range(min(20, len(packets1))):
            self.assertEqual(packets1[i].operation, packets2[i].operation,
                           f"Packet {i}: operation mismatch")
            self.assertEqual(packets1[i].src_rank, packets2[i].src_rank,
                           f"Packet {i}: src_rank mismatch")
            self.assertEqual(packets1[i].dst_rank, packets2[i].dst_rank,
                           f"Packet {i}: dst_rank mismatch")
    
    def test_different_ranks(self):
        """Test generation with different numbers of ranks"""
        for num_ranks in [4, 8, 16, 32]:
            gen = ACCLTraceGenerator(num_ranks=num_ranks)
            packets = gen.generate_trace(num_operations=10)
            
            # All packets should have valid ranks
            for packet in packets:
                self.assertLess(packet.src_rank, num_ranks)
                self.assertLess(packet.dst_rank, num_ranks)


class TestOperationWeights(unittest.TestCase):
    """Test operation weight distribution"""
    
    def test_default_weights(self):
        """Test trace generation with default weights"""
        generator = ACCLTraceGenerator(num_ranks=8, seed=42)
        packets = generator.generate_trace(num_operations=100)
        
        # Count operations
        op_counts = {}
        for packet in packets:
            op = packet.operation
            op_counts[op] = op_counts.get(op, 0) + 1
        
        # Should have variety of operations
        self.assertGreater(len(op_counts), 3)
    
    def test_single_operation_weight(self):
        """Test trace with single operation weighted heavily"""
        weights = {
            'sendrecv': 0,
            'broadcast': 100,  # Only broadcast
            'scatter': 0,
            'gather': 0,
            'reduce': 0,
            'allgather': 0,
            'allreduce': 0,
            'reduce_scatter': 0,
            'barrier': 0,
            'alltoall': 0
        }
        
        generator = ACCLTraceGenerator(num_ranks=8, seed=42)
        packets = generator.generate_trace(num_operations=20, operation_weights=weights)
        
        # Most packets should be broadcast-related
        broadcast_packets = [p for p in packets if p.operation == Operation.BROADCAST]
        self.assertGreater(len(broadcast_packets), 0)
    
    def test_zero_weight_operations(self):
        """Test that zero-weighted operations don't appear"""
        weights = {
            'sendrecv': 50,
            'broadcast': 50,
            'scatter': 0,  # Should not appear
            'gather': 0,   # Should not appear
            'reduce': 0,
            'allgather': 0,
            'allreduce': 0,
            'reduce_scatter': 0,
            'barrier': 0,
            'alltoall': 0
        }
        
        generator = ACCLTraceGenerator(num_ranks=8, seed=42)
        packets = generator.generate_trace(num_operations=50, operation_weights=weights)
        
        # Check no scatter or gather packets
        operations = set(packet.operation for packet in packets)
        self.assertNotIn(Operation.SCATTER, operations)
        self.assertNotIn(Operation.GATHER, operations)


if __name__ == '__main__':
    unittest.main()
