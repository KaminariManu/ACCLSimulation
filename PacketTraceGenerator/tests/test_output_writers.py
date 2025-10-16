"""
Unit tests for output writers

Tests binary and text output file generation, format validation,
and file I/O operations.
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
from src.packet import Packet
from src.accl_types import PacketType, Operation


class TestOutputWriters(unittest.TestCase):
    """Test output file writing functionality"""
    
    def setUp(self):
        """Set up test fixtures"""
        # Create temporary directory for test outputs
        self.test_dir = tempfile.mkdtemp()
        
        # Create sample packets
        self.generator = ACCLTraceGenerator(num_ranks=8, seed=42)
        self.packets = self.generator.generate_trace(num_operations=5)
    
    def tearDown(self):
        """Clean up test directory"""
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir)
    
    def test_write_binary_trace(self):
        """Test writing binary trace file"""
        output_file = os.path.join(self.test_dir, "test_trace.bin")
        
        write_binary(self.packets, output_file, num_ranks=8)
        
        # Check file was created
        self.assertTrue(os.path.exists(output_file))
        
        # Check file has content
        self.assertGreater(os.path.getsize(output_file), 0)
    
    def test_write_text_trace(self):
        """Test writing text (hex) trace file"""
        output_file = os.path.join(self.test_dir, "test_trace.txt")
        
        write_raw_hex(self.packets, output_file)
        
        # Check file was created
        self.assertTrue(os.path.exists(output_file))
        
        # Check file has content
        self.assertGreater(os.path.getsize(output_file), 0)
        
        # Check file is readable text
        with open(output_file, 'r') as f:
            content = f.read()
            self.assertGreater(len(content), 0)
    
    def test_binary_trace_format(self):
        """Test binary trace file format"""
        output_file = os.path.join(self.test_dir, "test_format.bin")
        
        write_binary(self.packets, output_file, num_ranks=8)
        
        # Read and check header
        with open(output_file, 'rb') as f:
            # Should start with version and rank count
            header = f.read(16)
            self.assertEqual(len(header), 16)
    
    def test_text_trace_format(self):
        """Test text trace file format"""
        output_file = os.path.join(self.test_dir, "test_format.txt")
        
        write_raw_hex(self.packets, output_file)
        
        # Read and check content
        with open(output_file, 'r') as f:
            lines = f.readlines()
            
            # Should have header and packet data
            self.assertGreater(len(lines), 1)
            
            # Check for hex characters
            for line in lines[5:]:  # Skip header lines
                if line.strip():
                    # Should contain hex data
                    self.assertTrue(any(c in '0123456789abcdefABCDEF' 
                                      for c in line))
    
    def test_empty_packet_list(self):
        """Test writing empty packet list"""
        output_file = os.path.join(self.test_dir, "empty.bin")
        
        write_binary([], output_file, num_ranks=8)
        
        # File should still be created with header
        self.assertTrue(os.path.exists(output_file))
    
    def test_single_node_vs_all_nodes(self):
        """Test single node output vs all nodes output"""
        packets = self.generator.generate_trace(num_operations=10)
        
        # Write all nodes (node 0)
        all_nodes_file = os.path.join(self.test_dir, "all_nodes.bin")
        write_binary(packets, all_nodes_file, num_ranks=8)
        
        # Write single node (node 3)
        single_node_packets = [p for p in packets if p.src_rank == 3]
        single_node_file = os.path.join(self.test_dir, "single_node.bin")
        write_binary(single_node_packets, single_node_file, num_ranks=8)
        
        # Both files should exist
        self.assertTrue(os.path.exists(all_nodes_file))
        self.assertTrue(os.path.exists(single_node_file))
        
        # Single node file should be smaller (or empty if no packets from that node)
        all_size = os.path.getsize(all_nodes_file)
        single_size = os.path.getsize(single_node_file)
        self.assertLessEqual(single_size, all_size)
    
    def test_large_trace_output(self):
        """Test writing large trace file"""
        # Generate large trace
        large_packets = self.generator.generate_trace(num_operations=100)
        
        output_file = os.path.join(self.test_dir, "large_trace.bin")
        write_binary(large_packets, output_file, num_ranks=8)
        
        self.assertTrue(os.path.exists(output_file))
        self.assertGreater(os.path.getsize(output_file), 1000)
    
    def test_output_directory_creation(self):
        """Test that output directories are created if needed"""
        nested_dir = os.path.join(self.test_dir, "nested", "output")
        output_file = os.path.join(nested_dir, "trace.bin")
        
        # Directory doesn't exist yet
        self.assertFalse(os.path.exists(nested_dir))
        
        write_binary(self.packets, output_file, num_ranks=8)
        
        # Directory should be created
        self.assertTrue(os.path.exists(nested_dir))
        self.assertTrue(os.path.exists(output_file))


class TestFileFormats(unittest.TestCase):
    """Test file format compatibility"""
    
    def setUp(self):
        """Set up test fixtures"""
        self.test_dir = tempfile.mkdtemp()
        self.generator = ACCLTraceGenerator(num_ranks=8, seed=42)
    
    def tearDown(self):
        """Clean up test directory"""
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir)
    
    def test_binary_text_consistency(self):
        """Test that binary and text outputs contain same data"""
        packets = self.generator.generate_trace(num_operations=10)
        
        binary_file = os.path.join(self.test_dir, "trace.bin")
        text_file = os.path.join(self.test_dir, "trace.txt")
        
        write_binary(packets, binary_file, num_ranks=8)
        write_raw_hex(packets, text_file)
        
        # Both files should exist
        self.assertTrue(os.path.exists(binary_file))
        self.assertTrue(os.path.exists(text_file))
        
        # Both should have content
        self.assertGreater(os.path.getsize(binary_file), 0)
        self.assertGreater(os.path.getsize(text_file), 0)
    
    def test_multiple_traces_different_files(self):
        """Test writing multiple different traces"""
        for i in range(3):
            packets = self.generator.generate_trace(num_operations=5)
            output_file = os.path.join(self.test_dir, f"trace_{i}.bin")
            write_binary(packets, output_file, num_ranks=8)
            
            self.assertTrue(os.path.exists(output_file))


if __name__ == '__main__':
    unittest.main()
