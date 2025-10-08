#!/usr/bin/env python3
"""Quick test to verify packet structure"""

import struct

# Test the format
format_str = '>HHIIIIIIIIIIQIII'
test_values = (
    0xACCE, 0x01,  # HH: protocol, version
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,  # IIIIIIIIII: 10 integers (ptype through flags)
    0x1234567890ABCDEF,  # Q: address
    10, 11, 12  # III: segment, total_segments, checksum
)

print(f"Format string: {format_str}")
print(f"Number of format items: {len([c for c in format_str if c.isalpha()])}")
print(f"Number of values: {len(test_values)}")

try:
    packed = struct.pack(format_str, *test_values)
    print(f"✓ Packing successful!")
    print(f"  Packed size: {len(packed)} bytes")
    
    if len(packed) == 64:
        print(f"✓ Header is exactly 64 bytes!")
    else:
        print(f"✗ Header is {len(packed)} bytes, expected 64")
        
except struct.error as e:
    print(f"✗ Packing failed: {e}")
