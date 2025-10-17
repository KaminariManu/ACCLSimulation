/**
 * @file accl_packet_processor.h
 * @brief Main ACCL Packet Processor Interface
 * 
 * Main header that provides the complete ACCL packet processor API.
 * Includes all necessary sub-modules.
 */

#ifndef ACCL_PACKET_PROCESSOR_H
#define ACCL_PACKET_PROCESSOR_H

/* Include all sub-modules */
#include "accl_types.h"
#include "accl_utils.h"
#include "accl_byteorder.h"
#include "accl_memory.h"
#include "accl_packet_loader.h"
#include "accl_operations.h"

#include <stdio.h>

/* ============================================================================
 * Node Management Functions
 * ============================================================================ */

/**
 * Initialize a node state
 * @param rank Node rank (0-based)
 * @param num_ranks Total number of ranks in the system
 * @return Pointer to initialized node state, or NULL on error
 */
ACCLNodeState* accl_init_node(uint32_t rank, uint32_t num_ranks);

/**
 * Free node state and all associated resources
 * @param node Pointer to node state to free
 */
void accl_free_node(ACCLNodeState *node);

/* ============================================================================
 * Packet Processing Functions
 * ============================================================================ */

/**
 * Process a single packet
 * Routes packet to appropriate operation handler based on packet type
 * 
 * @param node Pointer to node state
 * @param packet Pointer to packet to process
 */
void accl_process_packet(ACCLNodeState *node, ACCLPacket *packet);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Print packet information (for debugging)
 * @param packet Pointer to packet to print
 */
void accl_print_packet(const ACCLPacket *packet);

/**
 * Print accumulated statistics
 * @param stats Pointer to statistics structure
 */
void accl_print_statistics(const ACCLStatistics *stats);

/**
 * Get string representation of packet type
 * @param type Packet type enum value
 * @return String name of packet type
 */
const char* accl_packet_type_str(PacketType type);

/**
 * Get string representation of operation type
 * @param op Operation type enum value
 * @return String name of operation type
 */
const char* accl_operation_str(OperationType op);

#endif /* ACCL_PACKET_PROCESSOR_H */
