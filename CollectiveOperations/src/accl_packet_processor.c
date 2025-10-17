/**
 * @file accl_packet_processor.c
 * @brief Main Packet Processing Implementation
 * 
 * This is the top-level coordinator for the ACCL packet processor. It provides:
 * 
 * 1. NODE LIFECYCLE MANAGEMENT:
 *    - Node initialization with rank and communicator size
 *    - Resource allocation (memory, state trackers)
 *    - Cleanup and deallocation
 * 
 * 2. PACKET PROCESSING DISPATCHER:
 *    - Routes incoming packets to appropriate operation handlers
 *    - Updates statistics for performance analysis
 *    - Tracks packet types and operation types
 * 
 * 3. UTILITY FUNCTIONS:
 *    - Packet visualization (debugging)
 *    - Statistics reporting
 *    - String conversion for enums (packet types, operations)
 * 
 * ARCHITECTURE:
 * =============
 * This module acts as the "main controller" that ties together all other
 * modules. It delegates actual operation handling to the Operations module
 * (accl_operations.c) while maintaining global state and statistics.
 * 
 * WORKFLOW:
 * =========
 * 1. Initialize node with accl_init_node(rank, num_ranks)
 * 2. For each packet in trace:
 *    - Call accl_process_packet(node, packet)
 *    - Packet is dispatched to appropriate handler
 *    - Statistics are updated
 * 3. Print statistics with accl_print_statistics()
 * 4. Clean up with accl_free_node()
 */

#include "accl_packet_processor.h"
#include <stdlib.h>

/* ============================================================================
 * Node Initialization and Cleanup
 * ============================================================================ */

/**
 * @brief Initialize a new ACCL node state
 * 
 * Creates and initializes all the data structures needed to simulate a single
 * node (rank) in a distributed system. Each node maintains:
 * 
 * - Rank identifier: This node's position in the communicator (0 to N-1)
 * - Communicator size: Total number of participating ranks
 * - Statistics: Counters for packets, bytes, operations
 * - Operation trackers: State for ongoing collective operations
 * - Message trackers: State for multi-segment point-to-point messages
 * - Barrier state: Synchronization counters
 * - SoC memory: Simulated on-chip memory for data accumulation
 * 
 * MEMORY ALLOCATION:
 * ------------------
 * - Main node structure: malloc
 * - SoC memory buffers: Dynamically allocated on-demand during operations
 * - All state arrays: Static allocation within node structure
 * 
 * TYPICAL USAGE:
 * --------------
 * ```c
 * ACCLNodeState *node = accl_init_node(3, 8);  // Rank 3 of 8 nodes
 * // ... process packets ...
 * accl_free_node(node);
 * ```
 * 
 * @param rank This node's rank identifier (0-based index)
 * @param num_ranks Total number of ranks in the communicator
 * @return Pointer to initialized node state, or NULL if allocation fails
 */
ACCLNodeState* accl_init_node(uint32_t rank, uint32_t num_ranks) {
    // Allocate memory for the node state structure
    ACCLNodeState *node = (ACCLNodeState*)malloc(sizeof(ACCLNodeState));
    if (!node) return NULL;  // Allocation failed
    
    // Set node identity within the communicator
    node->rank = rank;           // This node's identifier
    node->num_ranks = num_ranks; // Total number of nodes
    
    // Zero-initialize all statistics counters
    memset(&node->stats, 0, sizeof(ACCLStatistics));
    
    // Initialize collective operation tracking
    // Supports up to MAX_COLLECTIVE_OPS simultaneous operations
    memset(node->collective_ops, 0, sizeof(node->collective_ops));
    node->num_collective_ops = 0;  // No active operations yet
    
    // Initialize pending message tracking for point-to-point communication
    // Supports up to MAX_PENDING_MSGS simultaneous multi-segment messages
    memset(node->pending_msgs, 0, sizeof(node->pending_msgs));
    node->num_pending_msgs = 0;  // No pending messages yet
    
    // Initialize barrier synchronization state
    node->barrier_entered = 0;  // Count of barrier entries
    node->barrier_exited = 0;   // Count of barrier exits
    
    // Initialize SoC (System-on-Chip) memory simulation
    // This sets up the 256 MB on-chip memory pool for collective operations
    accl_init_memory(&node->memory);
    
    return node;  // Return initialized node
}

/**
 * @brief Free all resources associated with a node
 * 
 * Releases all dynamically allocated memory including:
 * - SoC memory buffers (any active buffers from collective operations)
 * - The node structure itself
 * 
 * This should be called during cleanup to prevent memory leaks.
 * After calling this function, the node pointer is invalid and should not be used.
 * 
 * @param node Pointer to node state to be freed (can be NULL - safe)
 */
void accl_free_node(ACCLNodeState *node) {
    if (!node) return;  // Nothing to free (safety check)
    
    // Free all SoC memory buffers
    // This releases any buffers allocated during REDUCE, GATHER, etc.
    accl_free_memory(&node->memory);
    
    // Free the main node structure
    free(node);
}

/* ============================================================================
 * Main Packet Processing
 * ============================================================================ */

/**
 * @brief Process a single packet through the ACCL protocol stack
 * 
 * This is the main entry point for packet processing. It performs two key tasks:
 * 
 * 1. STATISTICS TRACKING:
 *    - Updates global packet and byte counters
 *    - Categorizes packets by type (EAGER, RNDZV, COLLECTIVE)
 *    - Counts operations (SEND, BROADCAST, REDUCE, etc.)
 * 
 * 2. PACKET DISPATCHING:
 *    - Routes the packet to the appropriate operation handler
 *    - Each operation type has a specialized handler function
 *    - Handlers are responsible for operation-specific logic
 * 
 * PACKET FLOW:
 * ------------
 * 1. Packet arrives (from trace file)
 * 2. Update statistics (packet count, byte count)
 * 3. Classify by packet type (EAGER vs RNDZV vs COLLECTIVE)
 * 4. Classify by operation type (SEND vs BROADCAST vs REDUCE, etc.)
 * 5. Dispatch to appropriate handler (accl_handle_xxx)
 * 6. Handler processes packet and updates operation state
 * 
 * PACKET TYPES:
 * -------------
 * - DATA_EAGER: Direct data transfer (≤32KB messages)
 * - RNDZV_ADDR: Rendezvous address exchange (>32KB messages, phase 1)
 * - RNDZV_DATA: Rendezvous RDMA transfer (phase 2)
 * - RNDZV_COMPLETE: Rendezvous completion notification (phase 3)
 * - COLLECTIVE_DATA: Collective operation data packet
 * 
 * OPERATION TYPES:
 * ----------------
 * Point-to-Point: SEND, RECV
 * One-to-All: BROADCAST, SCATTER
 * All-to-One: GATHER, REDUCE
 * All-to-All: ALLGATHER, ALLREDUCE, ALLTOALL, REDUCE_SCATTER
 * Sync: BARRIER
 * 
 * @param node Pointer to node state (contains rank, stats, memory)
 * @param packet Pointer to packet to process (header + optional payload)
 */
void accl_process_packet(ACCLNodeState *node, ACCLPacket *packet) {
    if (!node || !packet) return;  // Safety check for null pointers
    
    // ========== UPDATE GLOBAL STATISTICS ==========
    
    // Increment total packet counter
    node->stats.total_packets++;
    
    // Add to total data bytes (cumulative across all packets)
    node->stats.total_data_bytes += packet->header.data_length;
    
    // ========== COUNT BY PACKET TYPE ==========
    // This helps analyze protocol efficiency (eager vs rendezvous usage)
    
    switch (packet->header.packet_type) {
        case PTYPE_DATA_EAGER:
            // Small message sent directly with data in packet
            node->stats.eager_packets++;
            break;
        case PTYPE_RNDZV_ADDR:
            // Large message: receiver sends buffer address to sender
            node->stats.rendezvous_addr_packets++;
            break;
        case PTYPE_RNDZV_DATA:
            // Large message: sender performs RDMA write
            node->stats.rendezvous_data_packets++;
            break;
        case PTYPE_RNDZV_COMPLETE:
            // Large message: sender confirms completion
            node->stats.rendezvous_complete_packets++;
            break;
        case PTYPE_COLLECTIVE_DATA:
            // Collective operation data (broadcast, reduce, etc.)
            node->stats.collective_packets++;
            break;
    }
    
    // ========== DISPATCH TO OPERATION HANDLER ==========
    // Count by operation type and call the specialized handler
    
    switch (packet->header.operation) {
        case OP_SEND:
        case OP_RECV:
            // Point-to-point communication between two specific ranks
            node->stats.send_recv_packets++;
            accl_handle_send_recv(node, packet);
            break;
        case OP_BROADCAST:
            // Root sends same data to all other ranks
            node->stats.broadcast_packets++;
            accl_handle_broadcast(node, packet);
            break;
        case OP_SCATTER:
            // Root sends different chunks to each rank
            node->stats.scatter_packets++;
            accl_handle_scatter(node, packet);
            break;
        case OP_GATHER:
            // All ranks send data to root
            node->stats.gather_packets++;
            accl_handle_gather(node, packet);
            break;
        case OP_REDUCE:
            // All ranks send data to root with reduction operation (SUM, MAX, etc.)
            node->stats.reduce_packets++;
            accl_handle_reduce(node, packet);
            break;
        case OP_ALLGATHER:
            // Each rank gathers data from all other ranks
            node->stats.allgather_packets++;
            accl_handle_allgather(node, packet);
            break;
        case OP_ALLREDUCE:
            // Reduce data across all ranks, result available to all
            // Common in ML: gradient aggregation
            node->stats.allreduce_packets++;
            accl_handle_allreduce(node, packet);
            break;
        case OP_REDUCE_SCATTER:
            // Combined operation: reduce then scatter the result
            node->stats.reduce_scatter_packets++;
            accl_handle_reduce_scatter(node, packet);
            break;
        case OP_BARRIER:
            // Synchronization: all ranks wait until all reach this point
            node->stats.barrier_packets++;
            accl_handle_barrier(node, packet);
            break;
        case OP_ALLTOALL:
            // Each rank sends unique data to every other rank
            node->stats.alltoall_packets++;
            accl_handle_alltoall(node, packet);
            break;
    }
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Print detailed information about a packet
 * 
 * Displays all fields of a packet header in human-readable format.
 * Useful for debugging packet processing and understanding protocol flow.
 * 
 * OUTPUT FORMAT:
 * --------------
 * Packet Info:
 *   Protocol: 0xACCE, Version: 0x0001
 *   Type: DATA_EAGER, Operation: BROADCAST
 *   SRC=0, DST=3, TAG=42
 *   Session=1234, Seq=5, Len=4096
 *   Timestamp=1000, Flags=0x00000001
 *   Address=0x0000000080000000
 *   Segments: 1/5
 * 
 * FIELD DESCRIPTIONS:
 * -------------------
 * - Protocol: Should be 0xACCE (ACCL protocol identifier)
 * - Version: Protocol version (0x0001)
 * - Type: Packet type (EAGER, RNDZV, COLLECTIVE)
 * - Operation: Communication operation (SEND, BROADCAST, REDUCE, etc.)
 * - SRC/DST: Source and destination ranks
 * - TAG: User-defined message identifier
 * - Session: Unique session ID for this operation instance
 * - Seq: Sequence number for ordering
 * - Len: Payload length in bytes
 * - Timestamp: Simulation timestamp
 * - Flags: Various flags (compression, host_memory, etc.)
 * - Address: 64-bit address for RDMA operations
 * - Segments: Current/Total segments (for multi-segment messages)
 * 
 * @param packet Pointer to packet to print
 */
void accl_print_packet(const ACCLPacket *packet) {
    if (!packet) return;  // Safety check
    
    printf("Packet Info:\n");
    
    // Protocol identification
    printf("  Protocol: 0x%04X, Version: 0x%04X\n", 
           packet->header.protocol_number, packet->header.version);
    
    // Packet classification
    printf("  Type: %s, Operation: %s\n",
           accl_packet_type_str(packet->header.packet_type),
           accl_operation_str(packet->header.operation));
    
    // Routing information
    printf("  SRC=%u, DST=%u, TAG=%u\n",
           packet->header.src_rank, packet->header.dst_rank, packet->header.tag);
    
    // Session and sequencing
    printf("  Session=%u, Seq=%u, Len=%u\n",
           packet->header.session_id, packet->header.sequence_number, packet->header.data_length);
    
    // Additional metadata
    printf("  Timestamp=%u, Flags=0x%08X\n",
           packet->header.timestamp, packet->header.flags);
    
    // RDMA address (for rendezvous protocol)
    printf("  Address=0x%016llX\n", (unsigned long long)packet->header.address);
    
    // Segmentation information (1-based for display)
    printf("  Segments: %u/%u\n",
           packet->header.payload_segment + 1, packet->header.total_segments);
}

/**
 * @brief Print comprehensive statistics about processed packets
 * 
 * Displays a detailed summary of all packet processing activity, including:
 * - Total packet and byte counts
 * - Breakdown by operation type (SEND, BROADCAST, REDUCE, etc.)
 * - Breakdown by packet type (EAGER, RENDEZVOUS, COLLECTIVE)
 * - Directional traffic (bytes sent vs received)
 * 
 * This is typically called at the end of a trace processing run to analyze:
 * - Communication patterns (which operations are most common)
 * - Protocol efficiency (eager vs rendezvous usage)
 * - Data volume (total bandwidth consumed)
 * - Load distribution (sent vs received for this rank)
 * 
 * EXAMPLE OUTPUT:
 * ---------------
 * ========================================
 * ACCL Packet Processing Statistics
 * ========================================
 * Total Packets: 1234
 * Total Data: 52428800 bytes (50.00 MB)
 *   Bytes Sent:     26214400 bytes (25.00 MB)
 *   Bytes Received: 26214400 bytes (25.00 MB)
 * 
 * By Operation:
 *   Send/Recv:            500
 *   Broadcast:            200
 *   Allreduce:            100
 *   ...
 * 
 * By Packet Type:
 *   Eager:               1000
 *   Rendezvous Data:      100
 *   Collective:           134
 * ========================================
 * 
 * @param stats Pointer to statistics structure to print
 */
void accl_print_statistics(const ACCLStatistics *stats) {
    if (!stats) return;  // Safety check
    
    printf("\n");
    printf("========================================\n");
    printf("ACCL Packet Processing Statistics\n");
    printf("========================================\n");
    
    // ========== OVERALL TOTALS ==========
    printf("Total Packets: %llu\n", (unsigned long long)stats->total_packets);
    printf("Total Data: %llu bytes (%.2f MB)\n", 
           (unsigned long long)stats->total_data_bytes, 
           stats->total_data_bytes / (1024.0 * 1024.0));
    
    // Directional traffic (useful for load balancing analysis)
    printf("  Bytes Sent:     %llu bytes (%.2f MB)\n",
           (unsigned long long)stats->bytes_sent, 
           stats->bytes_sent / (1024.0 * 1024.0));
    printf("  Bytes Received: %llu bytes (%.2f MB)\n",
           (unsigned long long)stats->bytes_received, 
           stats->bytes_received / (1024.0 * 1024.0));
    
    printf("\n");
    
    // ========== BREAKDOWN BY OPERATION ==========
    // Shows which collective operations are most frequently used
    printf("By Operation:\n");
    printf("  Send/Recv:       %10llu\n", (unsigned long long)stats->send_recv_packets);
    printf("  Broadcast:       %10llu\n", (unsigned long long)stats->broadcast_packets);
    printf("  Scatter:         %10llu\n", (unsigned long long)stats->scatter_packets);
    printf("  Gather:          %10llu\n", (unsigned long long)stats->gather_packets);
    printf("  Reduce:          %10llu\n", (unsigned long long)stats->reduce_packets);
    printf("  Allgather:       %10llu\n", (unsigned long long)stats->allgather_packets);
    printf("  Allreduce:       %10llu\n", (unsigned long long)stats->allreduce_packets);
    printf("  Reduce_Scatter:  %10llu\n", (unsigned long long)stats->reduce_scatter_packets);
    printf("  Barrier:         %10llu\n", (unsigned long long)stats->barrier_packets);
    printf("  Alltoall:        %10llu\n", (unsigned long long)stats->alltoall_packets);
    
    printf("\n");
    
    // ========== BREAKDOWN BY PACKET TYPE ==========
    // Shows protocol efficiency (eager vs rendezvous split)
    printf("By Packet Type:\n");
    printf("  Eager:           %10llu\n", (unsigned long long)stats->eager_packets);
    printf("  Rendezvous Addr: %10llu\n", (unsigned long long)stats->rendezvous_addr_packets);
    printf("  Rendezvous Data: %10llu\n", (unsigned long long)stats->rendezvous_data_packets);
    printf("  Rendezvous Done: %10llu\n", (unsigned long long)stats->rendezvous_complete_packets);
    printf("  Collective:      %10llu\n", (unsigned long long)stats->collective_packets);
    
    printf("========================================\n");
}

/**
 * @brief Convert packet type enum to human-readable string
 * 
 * Helper function for logging and debugging. Converts the numeric
 * packet type identifier to a descriptive string.
 * 
 * @param type Packet type enum value
 * @return Constant string describing the packet type
 */
const char* accl_packet_type_str(PacketType type) {
    switch (type) {
        case PTYPE_DATA_EAGER: return "DATA_EAGER";           // Small message, data included
        case PTYPE_RNDZV_ADDR: return "RNDZV_ADDR";           // Large message, address exchange
        case PTYPE_RNDZV_DATA: return "RNDZV_DATA";           // Large message, RDMA transfer
        case PTYPE_RNDZV_COMPLETE: return "RNDZV_COMPLETE";   // Large message, completion
        case PTYPE_COLLECTIVE_DATA: return "COLLECTIVE_DATA"; // Collective operation
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert operation type enum to human-readable string
 * 
 * Helper function for logging and debugging. Converts the numeric
 * operation type identifier to a descriptive string.
 * 
 * @param op Operation type enum value
 * @return Constant string describing the operation
 */
const char* accl_operation_str(OperationType op) {
    switch (op) {
        case OP_SEND: return "SEND";                      // Point-to-point send
        case OP_RECV: return "RECV";                      // Point-to-point receive
        case OP_BROADCAST: return "BROADCAST";            // One-to-all
        case OP_SCATTER: return "SCATTER";                // One-to-all (unique chunks)
        case OP_GATHER: return "GATHER";                  // All-to-one
        case OP_REDUCE: return "REDUCE";                  // All-to-one (with reduction)
        case OP_ALLGATHER: return "ALLGATHER";            // All-to-all (gather)
        case OP_ALLREDUCE: return "ALLREDUCE";            // All-to-all (reduce+broadcast)
        case OP_REDUCE_SCATTER: return "REDUCE_SCATTER";  // All-to-all (reduce+scatter)
        case OP_BARRIER: return "BARRIER";                // Synchronization
        case OP_ALLTOALL: return "ALLTOALL";              // All-to-all (personalized)
        default: return "UNKNOWN";
    }
}
