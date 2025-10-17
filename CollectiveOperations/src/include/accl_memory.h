/**
 * @file accl_memory.h
 * @brief SoC Memory Simulation Module
 * 
 * Simulates on-chip memory management for packet processing
 * and collective operations.
 */

#ifndef ACCL_MEMORY_H
#define ACCL_MEMORY_H

#include "accl_types.h"

/* ============================================================================
 * Memory Management Functions
 * ============================================================================ */

/**
 * Initialize SoC memory system
 * @param memory Pointer to SoCMemory structure to initialize
 */
void accl_init_memory(SoCMemory *memory);

/**
 * Free all allocated SoC memory buffers
 * @param memory Pointer to SoCMemory structure to clean up
 */
void accl_free_memory(SoCMemory *memory);

/**
 * Allocate a memory buffer in simulated SoC memory
 * @param memory Pointer to SoCMemory manager
 * @param size Size of buffer to allocate in bytes
 * @param session_id Session ID associated with this buffer
 * @param op Operation type using this buffer
 * @return Pointer to allocated buffer, or NULL on failure
 */
SoCMemoryBuffer* accl_allocate_buffer(SoCMemory *memory, uint32_t size, 
                                      uint32_t session_id, OperationType op);

/**
 * Free a specific memory buffer
 * @param memory Pointer to SoCMemory manager
 * @param buffer_id ID of buffer to free
 */
void accl_free_buffer(SoCMemory *memory, uint32_t buffer_id);

/**
 * Find a buffer associated with a session ID
 * @param memory Pointer to SoCMemory manager
 * @param session_id Session ID to search for
 * @return Pointer to buffer, or NULL if not found
 */
SoCMemoryBuffer* accl_find_buffer(SoCMemory *memory, uint32_t session_id);

/**
 * Write data to a memory buffer
 * @param buffer Pointer to target buffer
 * @param data Data to write
 * @param offset Offset within buffer
 * @param length Number of bytes to write
 * @return 0 on success, -1 on error
 */
int accl_write_to_buffer(SoCMemoryBuffer *buffer, const uint8_t *data, 
                         uint32_t offset, uint32_t length);

/**
 * Read data from a memory buffer
 * @param buffer Pointer to source buffer
 * @param data Destination for read data
 * @param offset Offset within buffer
 * @param length Number of bytes to read
 * @return 0 on success, -1 on error
 */
int accl_read_from_buffer(SoCMemoryBuffer *buffer, uint8_t *data, 
                          uint32_t offset, uint32_t length);

/**
 * Perform reduction operation on buffer data
 * Combines incoming data with existing buffer data
 * @param buffer Target buffer
 * @param incoming_data New data to reduce
 * @param length Number of bytes to process
 * @param func_id Reduction function (0=SUM, 1=MAX, 2=MIN, 3=PROD, 4=LAND, 5=LOR)
 */
void accl_reduce_buffer_data(SoCMemoryBuffer *buffer, const uint8_t *incoming_data, 
                             uint32_t length, uint32_t func_id);

/**
 * Print SoC memory statistics
 * @param memory Pointer to SoCMemory manager
 */
void accl_print_memory_stats(const SoCMemory *memory);

/* ============================================================================
 * Helper function declaration (defined elsewhere)
 * ============================================================================ */

const char* accl_operation_str(OperationType op);

#endif /* ACCL_MEMORY_H */
