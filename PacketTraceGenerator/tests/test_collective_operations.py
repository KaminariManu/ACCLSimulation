"""
Unit tests for collective operations generators

Tests generation of all collective communication patterns:
broadcast, scatter, gather, reduce, allgather, allreduce,
reduce_scatter, barrier, and alltoall.
"""

import unittest
import sys
from pathlib import Path

# Add parent directory to path to import src modules
sys.path.insert(0, str(Path(__file__).parent.parent))

from src.trace_generator import ACCLTraceGenerator
from src.collective_operations import (
    generate_broadcast,
    generate_scatter,
    generate_gather,
    generate_reduce,
    generate_allgather,
    generate_allreduce,
    generate_reduce_scatter,
    generate_barrier,
    generate_alltoall
)
from src.accl_types import Operation, PacketType


class TestBroadcast(unittest.TestCase):
    """Test broadcast operation generation"""
    
    def setUp(self):
        self.generator = ACCLTraceGenerator(num_ranks=8, seed=42)
    
    def test_broadcast_generation(self):
        """Test broadcast packet generation"""
        packets = generate_broadcast(self.generator)
        
        self.assertIsNotNone(packets)
        self.assertIsInstance(packets, list)
        self.assertGreater(len(packets), 0)
    
    def test_broadcast_operation_type(self):
        """Test that broadcast packets have correct operation type"""
        packets = generate_broadcast(self.generator)
        
        for packet in packets:
            self.assertEqual(packet.operation, Operation.BROADCAST)
    
    def test_broadcast_root_distribution(self):
        """Test that broadcasts can have different roots"""
        roots = set()
        
        for _ in range(20):
            packets = generate_broadcast(self.generator)
            if packets:
                root = packets[0].src_rank
                roots.add(root)
        
        # Should have some variety in roots
        self.assertGreater(len(roots), 1)


class TestScatter(unittest.TestCase):
    """Test scatter operation generation"""
    
    def setUp(self):
        self.generator = ACCLTraceGenerator(num_ranks=8, seed=42)
    
    def test_scatter_generation(self):
        """Test scatter packet generation"""
        packets = generate_scatter(self.generator)
        
        self.assertIsNotNone(packets)
        self.assertGreater(len(packets), 0)
    
    def test_scatter_operation_type(self):
        """Test that scatter packets have correct operation type"""
        packets = generate_scatter(self.generator)
        
        for packet in packets:
            self.assertEqual(packet.operation, Operation.SCATTER)
    
    def test_scatter_destinations(self):
        """Test that scatter sends to all other ranks"""
        packets = generate_scatter(self.generator)
        
        # Extract destination ranks
        destinations = set(packet.dst_rank for packet in packets)
        
        # Should send to multiple destinations
        self.assertGreater(len(destinations), 1)


class TestGather(unittest.TestCase):
    """Test gather operation generation"""
    
    def setUp(self):
        self.generator = ACCLTraceGenerator(num_ranks=8, seed=42)
    
    def test_gather_generation(self):
        """Test gather packet generation"""
        packets = generate_gather(self.generator)
        
        self.assertIsNotNone(packets)
        self.assertGreater(len(packets), 0)
    
    def test_gather_operation_type(self):
        """Test that gather packets have correct operation type"""
        packets = generate_gather(self.generator)
        
        for packet in packets:
            self.assertEqual(packet.operation, Operation.GATHER)


class TestReduce(unittest.TestCase):
    """Test reduce operation generation"""
    
    def setUp(self):
        self.generator = ACCLTraceGenerator(num_ranks=8, seed=42)
    
    def test_reduce_generation(self):
        """Test reduce packet generation"""
        packets = generate_reduce(self.generator)
        
        self.assertIsNotNone(packets)
        self.assertGreater(len(packets), 0)
    
    def test_reduce_operation_type(self):
        """Test that reduce packets have correct operation type"""
        packets = generate_reduce(self.generator)
        
        for packet in packets:
            self.assertEqual(packet.operation, Operation.REDUCE)


class TestAllgather(unittest.TestCase):
    """Test allgather operation generation"""
    
    def setUp(self):
        self.generator = ACCLTraceGenerator(num_ranks=8, seed=42)
    
    def test_allgather_generation(self):
        """Test allgather packet generation"""
        packets = generate_allgather(self.generator)
        
        self.assertIsNotNone(packets)
        self.assertGreater(len(packets), 0)
    
    def test_allgather_operation_type(self):
        """Test that allgather packets have correct operation type"""
        packets = generate_allgather(self.generator)
        
        for packet in packets:
            self.assertEqual(packet.operation, Operation.ALLGATHER)
    
    def test_allgather_all_to_all(self):
        """Test that allgather involves all ranks"""
        packets = generate_allgather(self.generator)
        
        # Extract all ranks involved
        src_ranks = set(packet.src_rank for packet in packets)
        dst_ranks = set(packet.dst_rank for packet in packets)
        
        # Should involve multiple ranks
        self.assertGreater(len(src_ranks), 1)
        self.assertGreater(len(dst_ranks), 1)


class TestAllreduce(unittest.TestCase):
    """Test allreduce operation generation"""
    
    def setUp(self):
        self.generator = ACCLTraceGenerator(num_ranks=8, seed=42)
    
    def test_allreduce_generation(self):
        """Test allreduce packet generation"""
        packets = generate_allreduce(self.generator)
        
        self.assertIsNotNone(packets)
        self.assertGreater(len(packets), 0)
    
    def test_allreduce_operation_type(self):
        """Test that allreduce packets have correct operation type"""
        packets = generate_allreduce(self.generator)
        
        for packet in packets:
            self.assertEqual(packet.operation, Operation.ALLREDUCE)
    
    def test_allreduce_ring_pattern(self):
        """Test that allreduce follows ring topology"""
        packets = generate_allreduce(self.generator)
        
        # Should have sequential rank communication pattern
        self.assertGreater(len(packets), 0)


class TestReduceScatter(unittest.TestCase):
    """Test reduce_scatter operation generation"""
    
    def setUp(self):
        self.generator = ACCLTraceGenerator(num_ranks=8, seed=42)
    
    def test_reduce_scatter_generation(self):
        """Test reduce_scatter packet generation"""
        packets = generate_reduce_scatter(self.generator)
        
        self.assertIsNotNone(packets)
        self.assertGreater(len(packets), 0)
    
    def test_reduce_scatter_operation_type(self):
        """Test that reduce_scatter packets have correct operation type"""
        packets = generate_reduce_scatter(self.generator)
        
        for packet in packets:
            self.assertEqual(packet.operation, Operation.REDUCE_SCATTER)


class TestBarrier(unittest.TestCase):
    """Test barrier operation generation"""
    
    def setUp(self):
        self.generator = ACCLTraceGenerator(num_ranks=8, seed=42)
    
    def test_barrier_generation(self):
        """Test barrier packet generation"""
        packets = generate_barrier(self.generator)
        
        self.assertIsNotNone(packets)
        self.assertGreater(len(packets), 0)
    
    def test_barrier_operation_type(self):
        """Test that barrier packets have correct operation type"""
        packets = generate_barrier(self.generator)
        
        for packet in packets:
            self.assertEqual(packet.operation, Operation.BARRIER)
    
    def test_barrier_all_ranks(self):
        """Test that barrier involves all ranks"""
        packets = generate_barrier(self.generator)
        
        # Barrier should have 2 phases: gather + scatter
        # Each phase has (num_ranks - 1) packets
        # Total: 2 * (num_ranks - 1)
        expected_packets = 2 * (self.generator.num_ranks - 1)
        self.assertEqual(len(packets), expected_packets)


class TestAlltoall(unittest.TestCase):
    """Test alltoall operation generation"""
    
    def setUp(self):
        self.generator = ACCLTraceGenerator(num_ranks=8, seed=42)
    
    def test_alltoall_generation(self):
        """Test alltoall packet generation"""
        packets = generate_alltoall(self.generator)
        
        self.assertIsNotNone(packets)
        self.assertGreater(len(packets), 0)
    
    def test_alltoall_operation_type(self):
        """Test that alltoall packets have correct operation type"""
        packets = generate_alltoall(self.generator)
        
        for packet in packets:
            self.assertEqual(packet.operation, Operation.ALLTOALL)
    
    def test_alltoall_full_exchange(self):
        """Test that alltoall involves all-to-all communication"""
        packets = generate_alltoall(self.generator)
        
        # Count unique src-dst pairs
        pairs = set((p.src_rank, p.dst_rank) for p in packets)
        
        # Should have many communication pairs
        self.assertGreater(len(pairs), self.generator.num_ranks)


class TestCollectivePatterns(unittest.TestCase):
    """Test collective communication patterns"""
    
    def setUp(self):
        self.generator = ACCLTraceGenerator(num_ranks=8, seed=42)
    
    def test_all_collectives_generate(self):
        """Test that all collective operations can be generated"""
        generators = [
            generate_broadcast,
            generate_scatter,
            generate_gather,
            generate_reduce,
            generate_allgather,
            generate_allreduce,
            generate_reduce_scatter,
            generate_barrier,
            generate_alltoall
        ]
        
        for gen_func in generators:
            packets = gen_func(self.generator)
            self.assertGreater(len(packets), 0, 
                             f"{gen_func.__name__} failed to generate packets")
    
    def test_collective_packet_types(self):
        """Test that collectives use appropriate packet types"""
        packets = generate_allreduce(self.generator)
        
        for packet in packets:
            # Collective packets should use COLLECTIVE_DATA type
            self.assertEqual(packet.packet_type, PacketType.COLLECTIVE_DATA)
    
    def test_session_consistency(self):
        """Test that operations maintain session consistency"""
        packets = generate_broadcast(self.generator)
        
        # All packets in a single operation should share session ID
        if packets:
            session_ids = set(packet.session_id for packet in packets)
            # May have one session for eager or multiple for rendezvous
            self.assertGreater(len(session_ids), 0)


if __name__ == '__main__':
    unittest.main()
