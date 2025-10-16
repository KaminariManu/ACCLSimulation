"""
Unit tests for the Packet class

Tests packet creation, serialization to binary and hexadecimal formats,
and validation of packet structure.
"""

import unittest
import sys
from pathlib import Path

# Add parent directory to path to import src modules
sys.path.insert(0, str(Path(__file__).parent.parent))

from src.packet import Packet
from src.accl_types import PacketType, Operation


class TestPacket(unittest.TestCase):
    """Test cases for Packet class"""
    
    def setUp(self):
        """Set up test fixtures"""
        self.sample_packet = Packet(
            timestamp=1000,
            packet_type=PacketType.DATA_EAGER,
            operation=Operation.SEND,
            src_rank=0,
            dst_rank=1,
            tag=42,
            session_id=5,
            sequence_number=10,
            data_length=1024,
            compression=0,
            is_host_memory=False,
            address=0,
            payload_segment=0,
            total_segments=1
        )
    
    def test_packet_creation(self):
        """Test basic packet creation"""
        self.assertEqual(self.sample_packet.src_rank, 0)
        self.assertEqual(self.sample_packet.dst_rank, 1)
        self.assertEqual(self.sample_packet.data_length, 1024)
        self.assertEqual(self.sample_packet.tag, 42)
    
    def test_packet_binary_serialization(self):
        """Test packet serialization to binary format"""
        binary_data = self.sample_packet.to_binary()
        
        # Check that binary data is not empty
        self.assertIsNotNone(binary_data)
        self.assertIsInstance(binary_data, bytes)
        
        # Binary packet should have header (64 bytes) + payload
        # Header should be at least 64 bytes
        self.assertGreaterEqual(len(binary_data), 64)
    
    def test_packet_hex_serialization(self):
        """Test packet serialization to hexadecimal format"""
        hex_data = self.sample_packet.to_hex()
        
        # Check hex format
        self.assertIsNotNone(hex_data)
        self.assertIsInstance(hex_data, str)
        
        # Hex string should contain only valid hex characters and spaces
        hex_chars = hex_data.replace(' ', '').replace('\n', '')
        self.assertTrue(all(c in '0123456789abcdefABCDEF' for c in hex_chars))
    
    def test_packet_types(self):
        """Test different packet types"""
        packet_types = [
            PacketType.DATA_EAGER,
            PacketType.RNDZV_ADDR,
            PacketType.RNDZV_DATA,
            PacketType.COLLECTIVE_DATA,
        ]
        
        for ptype in packet_types:
            packet = Packet(
                timestamp=1000,
                packet_type=ptype,
                operation=Operation.SEND,
                src_rank=0,
                dst_rank=1,
                tag=0,
                session_id=0,
                sequence_number=0,
                data_length=100,
                compression=0,
                is_host_memory=False
            )
            self.assertEqual(packet.packet_type, ptype)
            # Should serialize without error
            _ = packet.to_binary()
    
    def test_operation_types(self):
        """Test different operation types"""
        operations = [
            Operation.SEND,
            Operation.BROADCAST,
            Operation.SCATTER,
            Operation.GATHER,
            Operation.REDUCE,
            Operation.ALLGATHER,
            Operation.ALLREDUCE,
            Operation.REDUCE_SCATTER,
            Operation.BARRIER,
            Operation.ALLTOALL,
        ]
        
        for op in operations:
            packet = Packet(
                timestamp=1000,
                packet_type=PacketType.COLLECTIVE_DATA,
                operation=op,
                src_rank=0,
                dst_rank=1,
                tag=0,
                session_id=0,
                sequence_number=0,
                data_length=100,
                compression=0,
                is_host_memory=False
            )
            self.assertEqual(packet.operation, op)
    
    def test_segmented_packets(self):
        """Test multi-segment packet handling"""
        num_segments = 5
        packets = []
        
        for seg in range(num_segments):
            packet = Packet(
                timestamp=1000 + seg * 100,
                packet_type=PacketType.DATA_EAGER,
                operation=Operation.SEND,
                src_rank=0,
                dst_rank=1,
                tag=100,
                session_id=1,
                sequence_number=seg,
                data_length=4096,
                compression=0,
                is_host_memory=False,
                payload_segment=seg,
                total_segments=num_segments
            )
            packets.append(packet)
        
        self.assertEqual(len(packets), num_segments)
        
        # Check segment numbering
        for i, packet in enumerate(packets):
            self.assertEqual(packet.payload_segment, i)
            self.assertEqual(packet.total_segments, num_segments)
    
    def test_rendezvous_address(self):
        """Test rendezvous packets with address"""
        address = 0x123456789ABCDEF0
        packet = Packet(
            timestamp=1000,
            packet_type=PacketType.RNDZV_ADDR,
            operation=Operation.SEND,
            src_rank=0,
            dst_rank=1,
            tag=200,
            session_id=10,
            sequence_number=0,
            data_length=100000,
            compression=0,
            is_host_memory=True,
            address=address
        )
        
        self.assertEqual(packet.address, address)
        self.assertTrue(packet.is_host_memory)
        
        # Binary should serialize address correctly
        binary = packet.to_binary()
        self.assertIsNotNone(binary)


class TestPacketValidation(unittest.TestCase):
    """Test packet validation and edge cases"""
    
    def test_zero_length_packet(self):
        """Test packet with zero data length"""
        packet = Packet(
            timestamp=0,
            packet_type=PacketType.COLLECTIVE_DATA,
            operation=Operation.BARRIER,
            src_rank=0,
            dst_rank=0,
            tag=0,
            session_id=0,
            sequence_number=0,
            data_length=0,
            compression=0,
            is_host_memory=False
        )
        
        # Should still serialize
        binary = packet.to_binary()
        self.assertIsNotNone(binary)
    
    def test_large_packet(self):
        """Test packet with large data length"""
        large_size = 1024 * 1024  # 1 MB
        packet = Packet(
            timestamp=5000,
            packet_type=PacketType.RNDZV_DATA,
            operation=Operation.ALLREDUCE,
            src_rank=3,
            dst_rank=4,
            tag=999,
            session_id=50,
            sequence_number=100,
            data_length=large_size,
            compression=0,
            is_host_memory=True
        )
        
        self.assertEqual(packet.data_length, large_size)
        binary = packet.to_binary()
        self.assertIsNotNone(binary)
    
    def test_all_ranks_communication(self):
        """Test packets between all rank pairs"""
        num_ranks = 8
        
        for src in range(num_ranks):
            for dst in range(num_ranks):
                if src != dst:
                    packet = Packet(
                        timestamp=1000,
                        packet_type=PacketType.DATA_EAGER,
                        operation=Operation.SEND,
                        src_rank=src,
                        dst_rank=dst,
                        tag=0,
                        session_id=0,
                        sequence_number=0,
                        data_length=100,
                        compression=0,
                        is_host_memory=False
                    )
                    self.assertEqual(packet.src_rank, src)
                    self.assertEqual(packet.dst_rank, dst)


if __name__ == '__main__':
    unittest.main()
