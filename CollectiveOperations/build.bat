@echo off
REM Build script for ACCL Packet Processor on Windows

echo ========================================
echo ACCL Packet Processor - Build Script
echo ========================================
echo.

REM Check if gcc is available
gcc --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: GCC not found!
    echo Please install MinGW-w64 from https://www.mingw-w64.org/
    echo.
    pause
    exit /b 1
)

echo [1/3] Compiling accl_packet_processor.c...
gcc -Wall -O2 -c accl_packet_processor.c
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile accl_packet_processor.c
    pause
    exit /b 1
)
echo       Done!

echo [2/3] Compiling demo_packet_processor.c...
gcc -Wall -O2 -c demo_packet_processor.c
if %errorlevel% neq 0 (
    echo ERROR: Failed to compile demo_packet_processor.c
    pause
    exit /b 1
)
echo       Done!

echo [3/3] Linking...
gcc -o demo_packet_processor.exe accl_packet_processor.o demo_packet_processor.o -lws2_32
if %errorlevel% neq 0 (
    echo ERROR: Failed to link
    pause
    exit /b 1
)
echo       Done!

echo.
echo ========================================
echo Build SUCCESSFUL!
echo ========================================
echo.
echo Executable created: demo_packet_processor.exe
echo.
echo Usage:
echo   demo_packet_processor.exe ^<node_rank^> ^<packet_file^>
echo.
echo Example:
echo   demo_packet_processor.exe 3 ..\PacketTraceGenerator\output_single_node\accl_packet_trace_node3.bin
echo.
pause
