/**
 * @file accl_memory.c
 * @brief SoC Memory Simulation Implementation
 * 
 * This file implements a simulation of on-chip (System-on-Chip) memory for
 * the ACCL accelerator. In real hardware, this would be high-speed SRAM or
 * HBM (High Bandwidth Memory) located on the accelerator chip, providing
 * fast temporary storage for collective operations.
 * 
 * Key Features:
 * - Fixed capacity: 256 MB (configurable via SOC_MEMORY_SIZE)
 * - Maximum buffers: 128 simultaneous allocations (SOC_MAX_BUFFERS)
 * - Zero-copy operations: Simulates direct memory access for RDMA
 * - Reduction operations: Supports in-place data reductions (SUM, MAX, etc.)
 * - Usage tracking: Monitors current and peak memory consumption
 * 
 * Use Cases:
 * - Temporary storage for collective operation intermediate results
 * - RDMA target buffers for rendezvous protocol transfers
 * - Data accumulation for reduction operations (REDUCE, ALLREDUCE)
 */

#include "accl_memory.h"
#include "accl_utils.h"
#include "accl_byteorder.h"
#include <stdlib.h>
#include <stdio.h>

/* ============================================================================
 * Memory Management Implementation
 * ============================================================================ */

/**
 * @brief Initialize the SoC memory simulation
 * 
 * Sets up the memory management structure with initial values:
 * - Total capacity set to SOC_MEMORY_SIZE (256 MB by default)
 * - All usage counters reset to zero
 * - Buffer ID counter initialized to 1 (0 reserved for invalid)
 * - Base address set to 0x80000000 (simulated hardware memory region)
 * - All buffer slots marked as free
 * 
 * The base address 0x80000000 is chosen to simulate a typical memory-mapped
 * region for on-chip accelerator memory, separate from host system RAM.
 * 
 * @param memory Pointer to SoCMemory structure to initialize
 */
void accl_init_memory(SoCMemory *memory) {
    if (!memory) return;  // Safety check
    
    // Set total available memory (default: 256 MB)
    memory->total_memory = SOC_MEMORY_SIZE;
    memory->used_memory = 0;       // No memory allocated yet
    memory->peak_memory = 0;       // Track maximum usage over time
    memory->next_buffer_id = 1;    // Start buffer IDs at 1 (0 = invalid)
    memory->next_address = 0x80000000;  // Simulated base address for on-chip memory
    memory->num_buffers = 0;       // No active buffers yet
    
    // Initialize all buffer slots as free (empty pool)
    for (int i = 0; i < SOC_MAX_BUFFERS; i++) {
        memory->buffers[i].buffer_id = 0;      // Invalid ID indicates free slot
        memory->buffers[i].in_use = false;     // Not currently allocated
        memory->buffers[i].data = NULL;        // No memory allocated
    }
}

/**
 * @brief Free all allocated buffers and reset memory state
 * 
 * Releases all dynamically allocated memory for all buffers and resets
 * the memory management state. This should be called during cleanup or
 * when resetting the accelerator state.
 * 
 * For each active buffer:
 * 1. Free the data allocation
 * 2. Mark the buffer slot as free
 * 3. Reset usage counters
 * 
 * @param memory Pointer to SoCMemory structure to clean up
 */
void accl_free_memory(SoCMemory *memory) {
    if (!memory) return;  // Safety check
    
    // Iterate through all buffer slots
    for (int i = 0; i < SOC_MAX_BUFFERS; i++) {
        // Check if this slot contains an active buffer
        if (memory->buffers[i].in_use && memory->buffers[i].data) {
            free(memory->buffers[i].data);     // Release the memory
            memory->buffers[i].data = NULL;    // Clear pointer (safety)
            memory->buffers[i].in_use = false; // Mark slot as free
        }
    }
    // Reset global counters
    memory->num_buffers = 0;  // No active buffers remain
    memory->used_memory = 0;  // All memory freed
}

/**
 * @brief Allocate a new buffer in SoC memory
 * 
 * Allocates a buffer for temporary data storage during collective operations.
 * Each buffer is assigned:
 * - A unique buffer ID for tracking
 * - A simulated memory address (for RDMA operations)
 * - Association with a specific session and operation type
 * 
 * Allocation process:
 * 1. Check if sufficient memory is available (capacity check)
 * 2. Find a free buffer slot (max 128 concurrent buffers)
 * 3. Allocate zero-initialized memory using calloc
 * 4. Initialize buffer metadata (ID, address, size, etc.)
 * 5. Update memory usage statistics
 * 
 * Common use cases:
 * - REDUCE/ALLREDUCE: Store accumulation results
 * - GATHER: Collect data from multiple sources
 * - Rendezvous protocol: Target buffer for large RDMA transfers
 * 
 * @param memory Pointer to SoCMemory management structure
 * @param size Size of buffer to allocate in bytes
 * @param session_id Session ID to associate with this buffer
 * @param op Operation type that will use this buffer (for debugging)
 * @return Pointer to allocated buffer, or NULL if allocation fails
 */
SoCMemoryBuffer* accl_allocate_buffer(SoCMemory *memory, uint32_t size, uint32_t session_id, OperationType op) {
    if (!memory || size == 0) return NULL;  // Validate inputs
    
    // Check if we have enough total memory capacity
    if (memory->used_memory + size > memory->total_memory) {
        fprintf(stderr, "SoC Memory: Out of memory (requested=%u, available=%llu)\n",
                size, (unsigned long long)(memory->total_memory - memory->used_memory));
        return NULL;  // Allocation would exceed capacity
    }
    
    // Find a free buffer slot in the buffer pool
    int slot = -1;
    for (int i = 0; i < SOC_MAX_BUFFERS; i++) {
        if (!memory->buffers[i].in_use) {
            slot = i;  // Found a free slot
            break;
        }
    }
    
    // Check if we found a free slot
    if (slot == -1) {
        fprintf(stderr, "SoC Memory: No free buffer slots available\n");
        return NULL;  // All 128 buffer slots are in use
    }
    
    // Allocate the actual memory for the buffer
    SoCMemoryBuffer *buffer = &memory->buffers[slot];
    buffer->data = (uint8_t*)calloc(size, 1);  // Zero-initialized for clean state
    if (!buffer->data) {
        fprintf(stderr, "SoC Memory: Failed to allocate %u bytes\n", size);
        return NULL;  // System memory allocation failed
    }
    
    // Initialize buffer metadata
    buffer->buffer_id = memory->next_buffer_id++;  // Assign unique ID
    buffer->session_id = session_id;               // Link to operation session
    buffer->address = memory->next_address;        // Assign simulated address
    buffer->size = size;                           // Total buffer capacity
    buffer->used = 0;                              // No data written yet
    buffer->in_use = true;                         // Mark as allocated
    buffer->operation = op;                        // Track operation type
    
    // Update memory management state
    memory->next_address += size;       // Advance address for next allocation
    memory->used_memory += size;        // Update current usage
    memory->num_buffers++;              // Increment active buffer count
    
    // Track peak memory usage for statistics
    if (memory->used_memory > memory->peak_memory) {
        memory->peak_memory = memory->used_memory;
    }
    
    // Log the allocation for debugging
    printf("  [SoC Memory] Allocated buffer %u: %u bytes at 0x%016llX for session %u (%s)\n",
           buffer->buffer_id, size, (unsigned long long)buffer->address, session_id, accl_operation_str(op));
    
    return buffer;  // Return pointer to newly allocated buffer
}

/**
 * @brief Free a specific buffer by its buffer ID
 * 
 * Releases a previously allocated buffer and returns its memory to the pool.
 * This is typically called when a collective operation completes and no
 * longer needs its temporary storage.
 * 
 * The function searches for the buffer with the specified ID, then:
 * 1. Frees the allocated data memory
 * 2. Marks the buffer slot as available for reuse
 * 3. Updates memory usage statistics
 * 
 * @param memory Pointer to SoCMemory management structure
 * @param buffer_id Unique ID of the buffer to free
 */
void accl_free_buffer(SoCMemory *memory, uint32_t buffer_id) {
    if (!memory) return;  // Safety check
    
    // Search for the buffer with the specified ID
    for (int i = 0; i < SOC_MAX_BUFFERS; i++) {
        if (memory->buffers[i].in_use && memory->buffers[i].buffer_id == buffer_id) {
            // Found the buffer - log the deallocation
            printf("  [SoC Memory] Freed buffer %u: %u bytes\n",
                   buffer_id, memory->buffers[i].size);
            
            // Release the memory
            free(memory->buffers[i].data);
            memory->buffers[i].data = NULL;          // Clear pointer (safety)
            memory->buffers[i].in_use = false;       // Mark slot as free
            
            // Update usage statistics
            memory->used_memory -= memory->buffers[i].size;  // Decrease used memory
            memory->num_buffers--;                           // Decrease buffer count
            return;  // Buffer freed successfully
        }
    }
    // If we get here, buffer_id was not found (possibly already freed)
}

/**
 * @brief Find a buffer by its associated session ID
 * 
 * Searches for an active buffer that was allocated for a specific session.
 * This is commonly used during collective operations to locate the buffer
 * where intermediate results are being stored.
 * 
 * Example use case:
 * - During a REDUCE operation, when a new data packet arrives, we need to
 *   find the buffer where we're accumulating results for that session.
 * 
 * @param memory Pointer to SoCMemory management structure
 * @param session_id Session ID to search for
 * @return Pointer to the buffer if found, NULL if not found
 */
SoCMemoryBuffer* accl_find_buffer(SoCMemory *memory, uint32_t session_id) {
    if (!memory) return NULL;  // Safety check
    
    // Linear search through all buffer slots
    for (int i = 0; i < SOC_MAX_BUFFERS; i++) {
        // Check if buffer is active and matches the session ID
        if (memory->buffers[i].in_use && memory->buffers[i].session_id == session_id) {
            return &memory->buffers[i];  // Found matching buffer
        }
    }
    return NULL;  // No buffer found for this session
}

/**
 * @brief Write data to a buffer at a specific offset
 * 
 * Copies data into the buffer at the specified offset. This simulates a
 * memory write operation that would occur in hardware via DMA or direct
 * CPU writes to on-chip memory.
 * 
 * Bounds checking is performed to prevent buffer overflows. The function
 * tracks the highest offset written to update the 'used' field, which
 * indicates how much of the buffer contains valid data.
 * 
 * Use cases:
 * - Writing received packet payload data to a buffer
 * - Storing partial results during multi-segment transfers
 * - Accumulating data during GATHER operations
 * 
 * @param buffer Pointer to the buffer to write to
 * @param data Pointer to source data to copy
 * @param offset Starting offset in buffer (0-based)
 * @param length Number of bytes to write
 * @return 0 on success, -1 on error (overflow or invalid buffer)
 */
int accl_write_to_buffer(SoCMemoryBuffer *buffer, const uint8_t *data, uint32_t offset, uint32_t length) {
    // Validate inputs
    if (!buffer || !buffer->in_use || !buffer->data || !data) return -1;
    
    // Check for buffer overflow
    if (offset + length > buffer->size) {
        fprintf(stderr, "SoC Memory: Write overflow (offset=%u, length=%u, size=%u)\n",
                offset, length, buffer->size);
        return -1;  // Write would exceed buffer bounds
    }
    
    // Perform the memory copy
    memcpy(buffer->data + offset, data, length);
    
    // Update the 'used' field to track how much data is in the buffer
    // This handles the case where writes may be non-sequential
    if (offset + length > buffer->used) {
        buffer->used = offset + length;  // Update high-water mark
    }
    
    return 0;  // Success
}

/**
 * @brief Read data from a buffer at a specific offset
 * 
 * Copies data out of the buffer from the specified offset into a
 * destination buffer. This simulates a memory read operation that would
 * occur in hardware via DMA or direct CPU reads from on-chip memory.
 * 
 * Bounds checking is performed to prevent reading beyond buffer limits.
 * 
 * Use cases:
 * - Reading accumulated results after a REDUCE operation
 * - Extracting data for transmission during BROADCAST/SCATTER
 * - Retrieving data for verification or final output
 * 
 * @param buffer Pointer to the buffer to read from
 * @param data Pointer to destination buffer for copied data
 * @param offset Starting offset in source buffer (0-based)
 * @param length Number of bytes to read
 * @return 0 on success, -1 on error (overflow or invalid buffer)
 */
int accl_read_from_buffer(SoCMemoryBuffer *buffer, uint8_t *data, uint32_t offset, uint32_t length) {
    // Validate inputs
    if (!buffer || !buffer->in_use || !buffer->data || !data) return -1;
    
    // Check for buffer overflow
    if (offset + length > buffer->size) {
        fprintf(stderr, "SoC Memory: Read overflow (offset=%u, length=%u, size=%u)\n",
                offset, length, buffer->size);
        return -1;  // Read would exceed buffer bounds
    }
    
    // Perform the memory copy
    memcpy(data, buffer->data + offset, length);
    return 0;  // Success
}

/**
 * @brief Perform in-place reduction operation on buffer data
 * 
 * This is a critical function for collective operations like REDUCE and ALLREDUCE.
 * It combines incoming data with existing buffer data using a reduction operation
 * (e.g., SUM, MAX, MIN) and stores the result back in the buffer.
 * 
 * Operation flow:
 * 1. Interpret data as arrays of 32-bit unsigned integers
 * 2. Convert from network byte order (big-endian) to host order
 * 3. Apply the reduction function element-by-element
 * 4. Convert result back to network byte order
 * 5. Store result in buffer (in-place)
 * 
 * Supported reduction operations (func_id):
 *   0 = SUM:  result = buffer_val + incoming_val (e.g., gradient aggregation)
 *   1 = MAX:  result = max(buffer_val, incoming_val)
 *   2 = MIN:  result = min(buffer_val, incoming_val)
 *   3 = PROD: result = buffer_val * incoming_val
 *   4 = LAND: result = buffer_val && incoming_val (logical AND)
 *   5 = LOR:  result = buffer_val || incoming_val (logical OR)
 * 
 * Example: ALLREDUCE with SUM for gradient aggregation in ML training
 *   - Node 0 buffer: [10, 20, 30]
 *   - Node 1 sends:  [5, 15, 25]
 *   - Result:        [15, 35, 55]
 * 
 * @param buffer Pointer to buffer containing accumulated results
 * @param incoming_data Pointer to new data to reduce with buffer contents
 * @param length Number of bytes to process (must be multiple of 4)
 * @param func_id Reduction function identifier (0-5)
 */
void accl_reduce_buffer_data(SoCMemoryBuffer *buffer, const uint8_t *incoming_data, uint32_t length, uint32_t func_id) {
    // Validate inputs
    if (!buffer || !buffer->in_use || !buffer->data || !incoming_data) return;
    
    // Clamp length to buffer size to prevent overflow
    if (length > buffer->size) {
        length = buffer->size;
    }
    
    // Process data as 32-bit integers (4 bytes at a time)
    uint32_t num_elements = length / 4;
    uint32_t *buffer_vals = (uint32_t*)buffer->data;           // Buffer as int array
    const uint32_t *incoming_vals = (const uint32_t*)incoming_data;  // Incoming as int array
    
    // Process each element
    for (uint32_t i = 0; i < num_elements; i++) {
        // Convert from network byte order (big-endian) to host order
        uint32_t buffer_val = ntohl(buffer_vals[i]);
        uint32_t incoming_val = ntohl(incoming_vals[i]);
        uint32_t result = 0;
        
        // Apply the specified reduction operation
        switch (func_id) {
            case 0:  // SUM - Add values together (most common for ML gradients)
                result = buffer_val + incoming_val;
                break;
            case 1:  // MAX - Keep the larger value
                result = (buffer_val > incoming_val) ? buffer_val : incoming_val;
                break;
            case 2:  // MIN - Keep the smaller value
                result = (buffer_val < incoming_val) ? buffer_val : incoming_val;
                break;
            case 3:  // PROD - Multiply values together
                result = buffer_val * incoming_val;
                break;
            case 4:  // LAND - Logical AND (true if both non-zero)
                result = (buffer_val && incoming_val) ? 1 : 0;
                break;
            case 5:  // LOR - Logical OR (true if either non-zero)
                result = (buffer_val || incoming_val) ? 1 : 0;
                break;
            default:  // Unknown operation - default to SUM for safety
                result = buffer_val + incoming_val;
                break;
        }
        
        // Convert result back to network byte order and store in buffer
        buffer_vals[i] = htonl(result);
    }
    
    // Update the 'used' field to track valid data range
    if (length > buffer->used) {
        buffer->used = length;
    }
}

/**
 * @brief Print comprehensive memory usage statistics
 * 
 * Displays detailed information about SoC memory utilization, including:
 * - Total capacity and current usage
 * - Peak memory usage (high-water mark)
 * - Number of active buffers
 * - Details of each active buffer (ID, session, size, operation)
 * 
 * This is typically called at the end of a simulation run to analyze
 * memory efficiency and identify potential memory bottlenecks.
 * 
 * Example output:
 *   === SoC Memory Statistics ===
 *   Total Memory: 256 MB
 *   Used Memory: 1024 KB (0.39%)
 *   Peak Memory: 2048 KB (0.78%)
 *   Active Buffers: 3 / 128
 *   
 *   Active Buffers:
 *     Buffer 5: Session=42, Size=512000, Used=512000, Op=REDUCE
 *     Buffer 7: Session=43, Size=256000, Used=128000, Op=GATHER
 *     Buffer 9: Session=44, Size=256000, Used=256000, Op=ALLREDUCE
 *   =============================
 * 
 * @param memory Pointer to SoCMemory structure to print statistics for
 */
void accl_print_memory_stats(const SoCMemory *memory) {
    if (!memory) return;  // Safety check
    
    printf("\n=== SoC Memory Statistics ===\n");
    
    // Display total capacity
    printf("Total Memory: %llu MB\n", (unsigned long long)(memory->total_memory / (1024 * 1024)));
    
    // Display current usage with percentage
    printf("Used Memory: %llu KB (%.2f%%)\n",
           (unsigned long long)(memory->used_memory / 1024),
           100.0 * memory->used_memory / memory->total_memory);
    
    // Display peak usage (highest usage seen during execution)
    printf("Peak Memory: %llu KB (%.2f%%)\n",
           (unsigned long long)(memory->peak_memory / 1024),
           100.0 * memory->peak_memory / memory->total_memory);
    
    // Display buffer utilization
    printf("Active Buffers: %u / %d\n", memory->num_buffers, SOC_MAX_BUFFERS);
    
    // If there are active buffers, list details for each one
    if (memory->num_buffers > 0) {
        printf("\nActive Buffers:\n");
        for (int i = 0; i < SOC_MAX_BUFFERS; i++) {
            if (memory->buffers[i].in_use) {
                // Print buffer details: ID, session, size, actual usage, operation type
                printf("  Buffer %u: Session=%u, Size=%u, Used=%u, Op=%s\n",
                       memory->buffers[i].buffer_id,      // Unique identifier
                       memory->buffers[i].session_id,     // Associated session
                       memory->buffers[i].size,           // Total allocated size
                       memory->buffers[i].used,           // Bytes actually written
                       accl_operation_str(memory->buffers[i].operation));  // Operation type
            }
        }
    }
    printf("=============================\n\n");
}
