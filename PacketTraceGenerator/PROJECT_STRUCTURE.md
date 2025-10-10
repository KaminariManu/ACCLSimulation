# ACCL Packet Trace Generator - Project Structure

## 📁 Directory Organization

```
CodiceElaborato/
│
├── 📂 PacketTraceGenerator/             # Python trace generation
│   ├── 📄 generate_packet_trace.py      # Main Python script to generate packet traces
│   ├── 📄 test_packet.py                # Test script to verify packet structure
│   ├── 📄 environment.yml               # Conda environment specification
│   ├── 📄 README.md                     # Project documentation
│   ├── 📄 QUICK_REFERENCE.md           # Quick reference guide
│   ├── 📄 OUTPUT_STRUCTURE.md          # Documentation of output organization
│   │
│   ├── 📂 output_all_nodes/            # Generated traces - complete network view
│   │   ├── accl_packet_trace.bin       # Binary format (all packets)
│   │   ├── accl_packet_trace.txt       # Hex format (all packets)
│   │   └── accl_packet_trace_raw_hex.txt # Raw hex format (all packets)
│   │
│   └── 📂 output_single_node/          # Generated traces - single node view
│       ├── accl_packet_trace_nodeN.bin # Binary format (node N packets only)
│       ├── accl_packet_trace_nodeN.txt # Hex format (node N packets only)
│       └── accl_packet_trace_nodeN_raw_hex.txt  # Raw hex (node N packets only)
│
└── 📂 CollectiveOperations/            # C implementation for processing traces
    ├── 📄 accl_packet_processor.h      # Header file (structures, enums, API)
    ├── 📄 accl_packet_processor.c      # Main implementation (~600 lines)
    ├── 📄 demo_packet_processor.c      # Demo program showing usage
    ├── 📄 Makefile                     # Build system (Linux/Unix/WSL)
    ├── 📄 build.bat                    # Windows batch build script
    ├── 📄 build.ps1                    # Windows PowerShell build script
    ├── 📄 run_demo.bat                 # Complete demo automation
    ├── 📄 README.md                    # CollectiveOperations overview
    ├── 📄 README_C_PROCESSOR.md        # Detailed C implementation docs
    └── 📄 BUILD_WINDOWS.md             # Windows build instructions
```

## 🔄 Workflow

### Step 1: Generate Packet Traces (Python)
```bash
# In PacketTraceGenerator/
python generate_packet_trace.py
```

**Generates:**
- `output_all_nodes/` - Complete network traffic
- `output_single_node/` - Per-node filtered traffic

### Step 2: Process Traces (C)
```bash
# On Windows
cd ..\CollectiveOperations
build.bat
demo_packet_processor.exe 3 ..\PacketTraceGenerator\output_single_node\accl_packet_trace_node3.bin

# On Linux/WSL
cd ../CollectiveOperations
make
./demo_packet_processor 3 ../PacketTraceGenerator/output_single_node/accl_packet_trace_node3.bin
```

## 📝 File Descriptions

### Root Directory Files

| File | Purpose |
|------|---------|
| `generate_packet_trace.py` | Generates ACCL packet traces for various collective operations |
| `test_packet.py` | Verifies packet structure is exactly 64 bytes |
| `environment.yml` | Conda environment with dependencies |
| `README.md` | Main project documentation |
| `QUICK_REFERENCE.md` | Quick reference for common operations |
| `OUTPUT_STRUCTURE.md` | Documentation of output file organization |

### CollectiveOperations Directory

| File | Purpose |
|------|---------|
| `accl_packet_processor.h` | C header with ACCLPacketHeader struct and API |
| `accl_packet_processor.c` | Implementation of all operation handlers |
| `demo_packet_processor.c` | Example program using the processor |
| `Makefile` | Unix/Linux build system |
| `build.bat` | Windows batch build script |
| `build.ps1` | Windows PowerShell build script |
| `run_demo.bat` | Full automation: generate + build + run |
| `README.md` | Overview and quick start guide |
| `README_C_PROCESSOR.md` | Comprehensive technical documentation |
| `BUILD_WINDOWS.md` | Windows-specific build guide |

## 🎯 Key Features by Directory

### Root (Python Generation)
- ✅ Generate traces for 11 ACCL operations
- ✅ Support for configurable ranks and operations
- ✅ Output in multiple formats (binary, hex, raw hex)
- ✅ Per-node filtering with random node selection
- ✅ Organized output directories

### CollectiveOperations (C Processing)
- ✅ Read binary and hex trace files
- ✅ Process all 11 ACCL operations
- ✅ Handle Eager and Rendezvous protocols
- ✅ Track comprehensive statistics
- ✅ Cross-platform support (Windows/Linux/macOS)
- ✅ Educational code structure

## 🔗 Dependencies

### Python Requirements
- Python 3.6+
- struct (built-in)
- random (built-in)
- os (built-in)

### C Requirements
- GCC or compatible C compiler
- C11 standard library
- POSIX headers (or Windows equivalents)

## 🚀 Quick Commands

### Generate Traces
```bash
python generate_packet_trace.py
```

### Build C Processor (Windows)
```bash
cd CollectiveOperations
build.bat
```

### Build C Processor (Linux)
```bash
cd CollectiveOperations
make
```

### Run Full Demo (Windows)
```bash
cd CollectiveOperations
run_demo.bat
```

### Run Full Demo (Linux)
```bash
cd CollectiveOperations
make full
make run-binary
```

## 📊 Output Formats

All traces are generated in three formats:

1. **Binary (`.bin`)** - Compact binary format with file header
2. **Hex (`.txt`)** - Human-readable hex with structure
3. **Raw Hex (`.txt`)** - Pure hex strings, one packet per line

## 🎓 Educational Value

This project demonstrates:
- **Network Protocol Implementation** - ACCL collective operations
- **Binary Data Handling** - Struct packing, endianness
- **Multi-language Integration** - Python generation, C processing
- **Cross-platform Development** - Windows/Linux compatibility
- **Build Systems** - Make, batch scripts, PowerShell

## 📖 Documentation Index

- **Quick Start**: See `README.md` (root)
- **Python Details**: See `QUICK_REFERENCE.md`
- **Output Organization**: See `OUTPUT_STRUCTURE.md`
- **C Implementation**: See `CollectiveOperations/README.md`
- **C Technical Docs**: See `CollectiveOperations/README_C_PROCESSOR.md`
- **Windows Build**: See `CollectiveOperations/BUILD_WINDOWS.md`

## 🔧 Troubleshooting

### Python Issues
- **Import errors**: Check Python version (3.6+ required)
- **File not found**: Ensure you're in `PacketTraceGenerator/` directory

### C Build Issues
- **GCC not found**: Install MinGW (Windows) or build-essential (Linux)
- **File not found**: Use correct paths (`../` for parent directory)
- **Byte order issues**: Check endianness conversion functions

## 📈 Project Statistics

- **Python Code**: ~1200 lines (generate_packet_trace.py)
- **C Code**: ~900 lines (processor + demo)
- **Documentation**: ~1500 lines across all README files
- **Supported Operations**: 11 ACCL collective operations
- **File Formats**: 3 (binary, hex, raw hex)
- **Platforms**: 3+ (Windows, Linux, macOS)

---

**Last Updated**: October 9, 2025
**Version**: 1.0
**Status**: Production Ready ✅
