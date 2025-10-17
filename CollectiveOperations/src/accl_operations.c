/**
 * @file accl_operations.c
 * @brief Collective Operation Handlers Implementation
 * 
 * This file implements handlers for all MPI-style collective communication
 * operations. These operations are fundamental to parallel computing and
 * distributed machine learning workloads.
 * 
 * OPERATION CATEGORIES:
 * =====================
 * 
 * 1. Point-to-Point Communication:
 *    - SEND/RECV: Direct communication between two ranks
 *    - Uses eager protocol (≤32KB) or rendezvous protocol (>32KB)
 * 
 * 2. One-to-All Communication:
 *    - BROADCAST: Root sends same data to all ranks
 *    - SCATTER: Root sends different chunks to each rank
 * 
 * 3. All-to-One Communication:
 *    - GATHER: All ranks send data to root
 *    - REDUCE: All ranks send data to root with reduction (SUM, MAX, etc.)
 * 
 * 4. All-to-All Communication:
 *    - ALLGATHER: Each rank gathers data from all other ranks
 *    - ALLREDUCE: Reduce data across all ranks, result available to all
 *    - ALLTOALL: Each rank sends unique data to every other rank
 *    - REDUCE_SCATTER: Reduce then scatter the result
 * 
 * 5. Synchronization:
 *    - BARRIER: Synchronization point (all ranks must reach before proceeding)
 * 
 * KEY CONCEPTS:
 * =============
 * 
 * - Session ID: Unique identifier for each operation instance
 * - Segmentation: Large messages split into 4KB segments for transport
 * - State Tracking: Operations maintain state across multiple packets
 * - SoC Memory: On-chip buffers for accumulating intermediate results
 * - Root Rank: Special rank that coordinates many collective operations
 * 
 * REAL-WORLD USE CASES:
 * =====================
 * 
 * - ALLREDUCE: Gradient aggregation in distributed ML training
 * - BROADCAST: Parameter distribution from master to workers
 * - SCATTER: Dataset distribution to worker nodes
 * - GATHER: Collecting results from parallel computations
 * - REDUCE: Aggregating metrics or statistics
 * - BARRIER: Synchronizing before checkpointing or validation phases
 */

#include "accl_operations.h"
#include "accl_memory.h"
#include "accl_utils.h"
#include "accl_byteorder.h"
#include <stdio.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Find or create a collective operation state tracker
 * 
 * Collective operations often span multiple packets (due to segmentation or
 * multiple participants). This function maintains state across these packets
 * by finding an existing state tracker or creating a new one.
 * 
 * State trackers store:
 * - Session ID: Unique identifier for this operation instance
 * - Operation type: BROADCAST, REDUCE, etc.
 * - Progress counters: How many segments/participants processed
 * - Completion flag: Whether operation finished
 * 
 * Example: For a REDUCE operation with 4 ranks, the root maintains state
 * tracking how many contributions have been received (expected: 3 from peers).
 * 
 * @param node Pointer to node state containing operation trackers
 * @param session_id Unique session identifier for the operation
 * @param op Type of collective operation (BROADCAST, REDUCE, etc.)
 * @return Pointer to operation state, or NULL if no space available
 */
CollectiveOpState* accl_find_or_create_collective_op(ACCLNodeState *node, uint32_t session_id, OperationType op) {
    // First, search for existing operation state with matching session and type
    for (uint32_t i = 0; i < node->num_collective_ops; i++) {
        if (node->collective_ops[i].session_id == session_id && 
            node->collective_ops[i].operation == op) {
            return &node->collective_ops[i];  // Found existing state
        }
    }
    
    // Not found - create new operation state if space available
    if (node->num_collective_ops < MAX_COLLECTIVE_OPS) {
        CollectiveOpState *op_state = &node->collective_ops[node->num_collective_ops];
        memset(op_state, 0, sizeof(CollectiveOpState));  // Zero-initialize
        op_state->session_id = session_id;
        op_state->operation = op;
        op_state->in_progress = true;   // Mark as active
        op_state->completed = false;    // Not finished yet
        node->num_collective_ops++;     // Increment counter
        return op_state;
    }
    
    return NULL;  // No space for new operation (limit reached)
}

/**
 * @brief Find or create a pending message state for point-to-point communication
 * 
 * Point-to-point SEND/RECV operations may involve multiple segments.
 * This function tracks message state across segments, identifying messages
 * by their (tag, peer_rank, is_send) tuple.
 * 
 * Message state includes:
 * - Tag: User-defined message identifier
 * - Peer rank: The other participant in the communication
 * - Direction: Is this a send or receive?
 * - Segment tracking: How many segments received vs expected
 * - Completion flag: Is the full message assembled?
 * 
 * Example: A 100KB message split into 25 segments needs state tracking
 * to know when all segments have arrived.
 * 
 * @param node Pointer to node state containing message trackers
 * @param tag Message tag for matching
 * @param peer_rank Rank of the communication partner
 * @param is_send True if this is a send operation, false for receive
 * @return Pointer to message state, or NULL if no space available
 */
PendingMessage* accl_find_or_create_pending_msg(ACCLNodeState *node, uint32_t tag, uint32_t peer_rank, bool is_send) {
    // Search for existing message state with matching criteria
    for (uint32_t i = 0; i < node->num_pending_msgs; i++) {
        if (node->pending_msgs[i].tag == tag && 
            node->pending_msgs[i].peer_rank == peer_rank &&
            node->pending_msgs[i].is_send == is_send &&
            !node->pending_msgs[i].completed) {  // Only match incomplete messages
            return &node->pending_msgs[i];  // Found matching message
        }
    }
    
    // Not found - create new message state if space available
    if (node->num_pending_msgs < MAX_PENDING_MSGS) {
        PendingMessage *msg = &node->pending_msgs[node->num_pending_msgs];
        memset(msg, 0, sizeof(PendingMessage));  // Zero-initialize
        msg->tag = tag;
        msg->peer_rank = peer_rank;
        msg->is_send = is_send;
        msg->completed = false;         // Not finished yet
        node->num_pending_msgs++;       // Increment counter
        return msg;
    }
    
    return NULL;  // No space for new message (limit reached)
}

/* ============================================================================
 * Operation Handlers
 * ============================================================================
 * 
 * Each handler function processes packets for a specific collective operation.
 * Handlers are responsible for:
 * 
 * 1. Validating the packet is relevant to this node
 * 2. Managing multi-segment message assembly
 * 3. Maintaining operation state across multiple packets
 * 4. Performing operation-specific logic (reduction, data gathering, etc.)
 * 5. Detecting operation completion
 * 6. Updating statistics
 * 
 * Common patterns:
 * - Find/create operation state using session_id
 * - Track segment progress (current_segment / total_segments)
 * - Use SoC memory buffers for intermediate storage
 * - Log detailed progress for debugging
 */

/**
 * @brief Handle SEND/RECV operation (Point-to-Point communication)
 * 
 * PURPOSE:
 * --------
 * Manages direct communication between two specific ranks. This is the
 * foundational operation for all inter-node data transfer.
 * 
 * PROTOCOLS:
 * ----------
 * 1. Eager Protocol (messages ≤ 32KB):
 *    - Data sent directly in DATA_EAGER packets
 *    - Automatic segmentation for messages > 4KB (max packet payload)
 *    - Each segment contains: payload_segment, total_segments
 *    - Receiver reassembles segments
 * 
 * 2. Rendezvous Protocol (messages > 32KB):
 *    - 3-phase handshake for zero-copy transfers:
 *      Phase 1: RNDZV_ADDR - Receiver sends buffer address to sender
 *      Phase 2: RNDZV_DATA - Sender performs RDMA write to receiver's buffer
 *      Phase 3: RNDZV_COMPLETE - Sender notifies completion
 *    - Optimizes large transfers by avoiding intermediate copies
 * 
 * STATE TRACKING:
 * ---------------
 * - Uses PendingMessage structure to track multi-segment transfers
 * - Identifies messages by tuple: (tag, peer_rank, is_send)
 * - Counts segments: received_segments vs expected_segments
 * - Message complete when all segments received
 * 
 * @param node Pointer to node state structure
 * @param packet Pointer to received packet
 */
void accl_handle_send_recv(ACCLNodeState *node, ACCLPacket *packet) {
    // Determine this node's role in the communication
    bool is_sender = (packet->header.src_rank == node->rank);
    bool is_receiver = (packet->header.dst_rank == node->rank);
    
    // Filter out packets not involving this node
    if (!is_sender && !is_receiver) {
        return; /* Not relevant to this node */
    }
    
    /* Handle RNDZV_ADDR packets - address exchange for rendezvous protocol */
    if (packet->header.packet_type == PTYPE_RNDZV_ADDR) {
        if (is_receiver) {
            printf("  [Node %u] RNDZV_ADDR received from rank %u: address 0x%016llX for RDMA\n",
                   node->rank, packet->header.src_rank, (unsigned long long)packet->header.address);
        } else if (is_sender) {
            printf("  [Node %u] RNDZV_ADDR sent to rank %u: address 0x%016llX for RDMA\n",
                   node->rank, packet->header.dst_rank, (unsigned long long)packet->header.address);
        }
        return;
    }
    
    /* Handle RNDZV_DATA packets - RDMA data transfer */
    if (packet->header.packet_type == PTYPE_RNDZV_DATA) {
        if (is_receiver) {
            node->stats.bytes_received += packet->header.data_length;
            printf("  [Node %u] RNDZV_DATA (RDMA): %u bytes from rank %u to addr 0x%016llX [COMPLETE]\n",
                   node->rank, packet->header.data_length, packet->header.src_rank,
                   (unsigned long long)packet->header.address);
        } else if (is_sender) {
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] RNDZV_DATA (RDMA): %u bytes to rank %u at addr 0x%016llX [COMPLETE]\n",
                   node->rank, packet->header.data_length, packet->header.dst_rank,
                   (unsigned long long)packet->header.address);
        }
        return;
    }
    
    /* Handle RNDZV_COMPLETE packets - completion notification */
    if (packet->header.packet_type == PTYPE_RNDZV_COMPLETE) {
        if (is_receiver) {
            printf("  [Node %u] RNDZV_COMPLETE: RDMA transfer from rank %u completed\n",
                   node->rank, packet->header.src_rank);
        }
        return;
    }
    
    /* Identify the peer rank (the other party in the communication) */
    uint32_t peer_rank = is_sender ? packet->header.dst_rank : packet->header.src_rank;
    
    /* Find or create state tracking for this message (identified by tag and peer) */
    PendingMessage *msg = accl_find_or_create_pending_msg(node, packet->header.tag, peer_rank, is_sender);
    
    if (msg) {
        /* Initialize message state on first segment */
        if (msg->expected_segments == 0) {
            msg->expected_segments = packet->header.total_segments;
            msg->session_id = packet->header.session_id;
        }
        
        /* Update message progress counters */
        msg->received_segments++;
        msg->bytes += packet->header.data_length;
        
        /* Track directional statistics and print status */
        if (is_sender) {
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] SEND to rank %u: %u bytes, tag=%u, seq=%u, seg=%u/%u",
                   node->rank, peer_rank, packet->header.data_length,
                   packet->header.tag, packet->header.sequence_number,
                   packet->header.payload_segment, packet->header.total_segments);
        } else {
            node->stats.bytes_received += packet->header.data_length;
            printf("  [Node %u] RECV from rank %u: %u bytes, tag=%u, seq=%u, seg=%u/%u",
                   node->rank, peer_rank, packet->header.data_length,
                   packet->header.tag, packet->header.sequence_number,
                   packet->header.payload_segment, packet->header.total_segments);
        }
        
        /* Check if all segments have been received (message complete) */
        if (msg->received_segments >= msg->expected_segments) {
            msg->completed = true;
            printf(" [COMPLETE: %llu bytes total]\n", (unsigned long long)msg->bytes);
        } else {
            printf("\n");
        }
    } else {
        /* Fallback if state tracking array is full (MAX_PENDING_MSGS reached) */
        if (is_sender) {
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] SEND to rank %u: %u bytes, tag=%u, seq=%u\n",
                   node->rank, peer_rank, packet->header.data_length,
                   packet->header.tag, packet->header.sequence_number);
        } else {
            node->stats.bytes_received += packet->header.data_length;
            printf("  [Node %u] RECV from rank %u: %u bytes, tag=%u, seq=%u\n",
                   node->rank, peer_rank, packet->header.data_length,
                   packet->header.tag, packet->header.sequence_number);
        }
    }
}

/**
 * @brief Handle BROADCAST operation
 * 
 * PURPOSE:
 * --------
 * Distributes identical data from one root rank to all other ranks in the
 * communicator. This is a fundamental one-to-all collective operation.
 * 
 * COMMUNICATION PATTERN:
 * ----------------------
 * Root rank (usually rank 0) sends the same data to all non-root ranks.
 * 
 * Visual example with 4 ranks:
 *   Rank 0 (root) --> Rank 1
 *                 --> Rank 2
 *                 --> Rank 3
 * 
 * TOPOLOGY:
 * ---------
 * - Small groups: Direct (flat) communication from root to each rank
 * - Large groups: Tree-based (binary tree) for better scalability
 *   - Reduces load on root
 *   - Logarithmic completion time: O(log N)
 * 
 * STATE TRACKING:
 * ---------------
 * - Uses CollectiveOpState identified by session_id
 * - Root tracks: Total bytes sent to all destinations
 * - Non-root tracks: Total bytes received from root
 * - Segment counters: Tracks multi-segment broadcasts
 * 
 * COMPLETION:
 * -----------
 * - Root: Complete when all data sent to all ranks
 * - Non-root: Complete when all data received from root
 * - Verification: received_segments >= expected_segments
 * 
 * @param node Pointer to node state structure
 * @param packet Pointer to received packet
 */
void accl_handle_broadcast(ACCLNodeState *node, ACCLPacket *packet) {
    // Find or create collective operation state (identified by session_id)
    CollectiveOpState *op = accl_find_or_create_collective_op(node, packet->header.session_id, OP_BROADCAST);
    
    if (op) {
        // Initialize operation state on first packet
        if (op->total_segments == 0) {
            op->total_segments = packet->header.total_segments;
            op->root_rank = packet->header.src_rank;  // Identify root from source
        }
        
        // Handle RNDZV_DATA packets differently - they represent RDMA transfers
        if (packet->header.packet_type == PTYPE_RNDZV_DATA) {
            /* RDMA transfer: data goes directly to destination memory via DMA */
            if (packet->header.dst_rank == node->rank) {
                /* Simulate DMA write to SoC memory at the specified address */
                node->stats.bytes_received += packet->header.data_length;
                printf("  [Node %u] BROADCAST (RDMA): %u bytes transferred to addr 0x%016llX from root %u\n",
                       node->rank, packet->header.data_length, 
                       (unsigned long long)packet->header.address, packet->header.src_rank);
                op->bytes_processed += packet->header.data_length;
                op->completed = true;  /* RDMA completes in single transfer */
            }
            return;
        }
        
        /* Regular packet-based broadcast (eager protocol) */
        /* Update operation progress counters */
        op->segments_received++;
        op->bytes_processed += packet->header.data_length;
        
        /* Track directional flow: root sends, non-root receives */
        if (packet->header.src_rank == node->rank) {
            /* This node is the root, sending broadcast data */
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] BROADCAST (root) to rank %u: %u bytes, seg=%u/%u",
                   node->rank, packet->header.dst_rank, packet->header.data_length,
                   packet->header.payload_segment, packet->header.total_segments);
        } else if (packet->header.dst_rank == node->rank) {
            /* This node is a receiver, getting broadcast data from root */
            node->stats.bytes_received += packet->header.data_length;
            printf("  [Node %u] BROADCAST from root %u: %u bytes received, seg=%u/%u",
                   node->rank, packet->header.src_rank, packet->header.data_length,
                   packet->header.payload_segment, packet->header.total_segments);
        }
        
        /* Check if broadcast is complete (all segments sent/received) */
        if (op->segments_received >= op->total_segments) {
            op->completed = true;
            printf(" [OP COMPLETE: %llu bytes total]\n", (unsigned long long)op->bytes_processed);
        } else {
            printf("\n");
        }
    }
}

/**
 * Handle SCATTER operation
 * 
 * Purpose: Root distributes unique chunks of data to each rank
 * 
 * Communication Pattern:
 * - Root → Rank 0: chunk 0
 * - Root → Rank 1: chunk 1
 * - Root → Rank N: chunk N
 * Each rank receives a different portion of the data
 * 
 * State Tracking:
 * - Tracks which ranks have received their chunks
 * - Monitors segmented delivery (large chunks may be multi-segment)
 * 
 * Completion:
 * - All ranks have received their designated chunk
 */
void accl_handle_scatter(ACCLNodeState *node, ACCLPacket *packet) {
    /* Find or create state for this scatter operation */
    CollectiveOpState *op = accl_find_or_create_collective_op(node, packet->header.session_id, OP_SCATTER);
    
    if (op) {
        /* Initialize on first packet */
        if (op->total_segments == 0) {
            op->total_segments = packet->header.total_segments;
            op->root_rank = packet->header.src_rank;  /* Root distributes chunks */
        }
        
        /* Update progress: count segments across all chunk distributions */
        op->segments_received++;
        op->bytes_processed += packet->header.data_length;
        
        /* Track directional flow: root sends unique chunks to each rank */
        if (packet->header.src_rank == node->rank) {
            /* This node is root, distributing a unique chunk */
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] SCATTER (root) to rank %u: chunk %u bytes, seg=%u/%u",
                   node->rank, packet->header.dst_rank, packet->header.data_length,
                   packet->header.payload_segment, packet->header.total_segments);
        } else if (packet->header.dst_rank == node->rank) {
            /* This node is receiving its designated chunk from root */
            node->stats.bytes_received += packet->header.data_length;
            printf("  [Node %u] SCATTER from root %u: chunk %u bytes received, seg=%u/%u",
                   node->rank, packet->header.src_rank, packet->header.data_length,
                   packet->header.payload_segment, packet->header.total_segments);
        }
        
        /* Check if scatter is complete (all chunks distributed) */
        if (op->segments_received >= op->total_segments) {
            op->completed = true;
            printf(" [OP COMPLETE]\n");
        } else {
            printf("\n");
        }
    }
}

/**
 * Handle GATHER operation
 * 
 * Purpose: Root collects data from all other ranks
 * 
 * Topology: Ring-based for scalability
 * 
 * Communication Pattern (Ring):
 * - Rank 0 → Rank 1 → Rank 2 → ... → Root
 * - Each rank forwards its own data plus accumulated data from previous ranks
 * - Root receives all data at the end of the ring
 * 
 * Node Roles:
 * - Root: Final collector, receives all accumulated data
 * - Relay nodes: Forward their data + received data to next in ring
 * 
 * State Tracking:
 * - Identifies root from destination rank
 * - Tracks data collection progress through the ring
 * 
 * Performance: O(N) communication steps for N ranks (ring-based)
 */
void accl_handle_gather(ACCLNodeState *node, ACCLPacket *packet) {
    /* Find or create state for this gather operation */
    CollectiveOpState *op = accl_find_or_create_collective_op(node, packet->header.session_id, OP_GATHER);
    
    if (op) {
        /* Initialize: ring-based gather expects data from all ranks */
        if (op->total_segments == 0) {
            /* Total segments = segments per rank * (num_ranks - 1) for ring circulation */
            op->total_segments = packet->header.total_segments * (node->num_ranks - 1);
            op->root_rank = packet->header.dst_rank;  /* Final destination is root */
        }
        
        /* Update progress through the ring */
        op->segments_received++;
        op->bytes_processed += packet->header.data_length;
        
        /* Track ring-based forwarding */
        if (packet->header.src_rank == node->rank) {
            /* This node is sending data to next in ring (own data + accumulated) */
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] GATHER: sending to next in ring (rank %u), %u bytes, seg=%u/%u",
                   node->rank, packet->header.dst_rank, packet->header.data_length,
                   packet->header.payload_segment, packet->header.total_segments);
        } else if (packet->header.dst_rank == node->rank) {
            /* This node is receiving data from previous in ring */
            node->stats.bytes_received += packet->header.data_length;
            
            /* Determine node role: root collects all, non-root relays */
            bool is_root = (node->rank == op->root_rank);
            if (is_root) {
                /* Root: final collector of all data */
                printf("  [Node %u] GATHER (root): collecting from rank %u, %u bytes, seg=%u/%u",
                       node->rank, packet->header.src_rank, packet->header.data_length,
                       packet->header.payload_segment, packet->header.total_segments);
            } else {
                /* Non-root: relay node, will forward accumulated data */
                printf("  [Node %u] GATHER: relaying from rank %u, %u bytes, seg=%u/%u",
                       node->rank, packet->header.src_rank, packet->header.data_length,
                       packet->header.payload_segment, packet->header.total_segments);
            }
        }
        printf("\n");
    }
}

/**
 * @brief Handle REDUCE operation
 * 
 * PURPOSE:
 * --------
 * Combines data from all ranks using a reduction operation (e.g., SUM, MAX)
 * and delivers the final result to a designated root rank. Critical for
 * aggregating distributed computations.
 * 
 * COMMUNICATION PATTERN (Ring-based with in-network reduction):
 * --------------------------------------------------------------
 * Example with 4 ranks, root=0, operation=SUM:
 *   Rank 0 (data: 100) ─┐
 *   Rank 1 (data: 200) ─┼─> Rank 1: combines → (300)
 *   Rank 2 (data: 300) ─┼─> Rank 2: combines → (600)
 *   Rank 3 (data: 400) ─┴─> Rank 0: combines → (1000) [FINAL]
 * 
 * REDUCTION OPERATIONS (encoded in tag field bits 16-31):
 * --------------------------------------------------------
 *   0 = SUM:  result = a + b (gradient aggregation in ML)
 *   1 = MAX:  result = max(a, b)
 *   2 = MIN:  result = min(a, b)
 *   3 = PROD: result = a * b
 *   4 = LAND: result = a && b (logical AND)
 *   5 = LOR:  result = a || b (logical OR)
 * 
 * SOC MEMORY: Each node allocates buffer for accumulation, performs
 * in-place reduction with incoming data. Root's final buffer contains
 * the complete reduced result.
 * 
 * @param node Pointer to node state structure
 * @param packet Pointer to received packet
 */
void accl_handle_reduce(ACCLNodeState *node, ACCLPacket *packet) {
    /* Find or create state for this reduce operation */
    CollectiveOpState *op = accl_find_or_create_collective_op(node, packet->header.session_id, OP_REDUCE);
    
    if (op) {
        /* Initialize: ring-based reduce with data combination at each node */
        if (op->total_segments == 0) {
            op->total_segments = packet->header.total_segments * (node->num_ranks - 1);
            op->root_rank = packet->header.dst_rank;  /* Final destination for reduced result */
        }
        
        /* Update reduction progress */
        op->segments_received++;
        op->bytes_processed += packet->header.data_length;
        
        /* Track ring-based reduction flow */
        if (packet->header.src_rank == node->rank) {
            /* This node is forwarding reduced data to next in ring */
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] REDUCE: sending to next in ring (rank %u) for reduction, %u bytes",
                   node->rank, packet->header.dst_rank, packet->header.data_length);
        } else if (packet->header.dst_rank == node->rank) {
            /* This node is receiving data to combine with local data */
            node->stats.bytes_received += packet->header.data_length;
            
            /* Extract reduction function ID from upper 16 bits of tag field */
            /* Python generator encodes it as: tag | (func_id << 16) */
            uint32_t func_id = (packet->header.tag >> 16) & 0xFFFF;
            const char *op_names[] = {"SUM", "MAX", "MIN", "PROD", "LAND", "LOR"};
            const char *op_name = (func_id < 6) ? op_names[func_id] : "UNKNOWN";
            
            /* Find or allocate SoC memory buffer for this reduction operation */
            SoCMemoryBuffer *buffer = accl_find_buffer(&node->memory, packet->header.session_id);
            if (!buffer) {
                /* First data arrival - allocate buffer to store accumulation */
                uint32_t buffer_size = packet->header.total_segments * MAX_PACKETSIZE;
                buffer = accl_allocate_buffer(&node->memory, buffer_size, 
                                             packet->header.session_id, OP_REDUCE);
                
                if (buffer) {
                    /* Initialize buffer with local rank's data (simulated as rank * 100) */
                    uint32_t local_val = htonl(node->rank * 100);
                    for (uint32_t i = 0; i < buffer_size / 4; i++) {
                        memcpy(buffer->data + i * 4, &local_val, 4);
                    }
                    buffer->used = buffer_size;
                }
            }
            
            if (buffer && packet->payload) {
                /* Perform in-memory reduction: combine incoming data with buffer contents */
                accl_reduce_buffer_data(buffer, packet->payload, 
                                       packet->header.data_length, func_id);
                
                /* Extract first value for display (convert from big-endian) */
                uint32_t result_val = 0;
                if (buffer->used >= 4) {
                    result_val = ntohl(*(uint32_t*)buffer->data);
                }
                
                uint32_t incoming_val = 0;
                if (packet->header.data_length >= 4) {
                    incoming_val = ntohl(*(uint32_t*)packet->payload);
                }
                
                /* Determine node role in reduction */
                bool is_root = (node->rank == op->root_rank);
                if (is_root) {
                    /* Root: receives final combined result */
                    printf("  [Node %u] REDUCE (root): %s operation from rank %u, %u bytes\n",
                           node->rank, op_name, packet->header.src_rank, packet->header.data_length);
                    printf("            [SoC Mem] Buffer %u: reduced incoming=%u => result=%u (in memory)",
                           buffer->buffer_id, incoming_val, result_val);
                } else {
                    /* Intermediate: combines received data with local, forwards result */
                    printf("  [Node %u] REDUCE: %s+relay from rank %u, %u bytes\n",
                           node->rank, op_name, packet->header.src_rank, packet->header.data_length);
                    printf("            [SoC Mem] Buffer %u: reduced incoming=%u => forward=%u (in memory)",
                           buffer->buffer_id, incoming_val, result_val);
                }
            }
        }
        printf("\n");
    }
}

/**
 * Handle ALLGATHER operation
 * 
 * Purpose: Each rank gathers data from all other ranks (all-to-all data collection)
 * 
 * Topology: Ring-based circulation
 * 
 * Communication Pattern:
 * - Data circulates around the ring N-1 times (N = number of ranks)
 * - Each rank sends its data to the next rank in the ring
 * - Each rank receives data from the previous rank and forwards it
 * - After N-1 steps, all ranks have all data
 * 
 * Example (4 ranks):
 * Step 0: Each rank has own data
 * Step 1: Rank 0→1, 1→2, 2→3, 3→0 (each forwards own data)
 * Step 2: Continue circulation with accumulated data
 * Step 3: All ranks now have data from all other ranks
 * 
 * Result: All ranks end up with the complete dataset
 * 
 * Complexity: O(N) steps, O(N) data per rank
 */
void accl_handle_allgather(ACCLNodeState *node, ACCLPacket *packet) {
    /* Find or create state for this allgather operation */
    CollectiveOpState *op = accl_find_or_create_collective_op(node, packet->header.session_id, OP_ALLGATHER);
    
    if (op) {
        /* Initialize: all ranks circulate data N-1 times around ring */
        if (op->total_segments == 0) {
            /* Total segments = segments per circulation * number of ranks */
            op->total_segments = packet->header.total_segments * node->num_ranks;
        }
        
        /* Update circulation progress */
        op->segments_received++;
        op->bytes_processed += packet->header.data_length;
        
        /* Track bidirectional ring circulation */
        if (packet->header.src_rank == node->rank) {
            /* This node is sending/forwarding data to next in ring */
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] ALLGATHER: sending to rank %u, %u bytes, seg=%u/%u",
                   node->rank, packet->header.dst_rank, packet->header.data_length,
                   packet->header.payload_segment, packet->header.total_segments);
        } else if (packet->header.dst_rank == node->rank) {
            /* This node is receiving data from previous in ring */
            node->stats.bytes_received += packet->header.data_length;
            printf("  [Node %u] ALLGATHER: receiving from rank %u, %u bytes, seg=%u/%u",
                   node->rank, packet->header.src_rank, packet->header.data_length,
                   packet->header.payload_segment, packet->header.total_segments);
        }
        
        /* Check if circulation complete: all ranks have all data */
        if (op->segments_received >= op->total_segments) {
            op->completed = true;
            printf(" [OP COMPLETE: all ranks have all data]\n");
        } else {
            printf("\n");
        }
    }
}

/**
 * Handle ALLREDUCE operation
 * 
 * Purpose: Reduce data from all ranks and distribute result to all ranks
 * 
 * Implementation: Two-phase operation
 * 1. REDUCE-SCATTER phase: Reduce and distribute chunks to all ranks
 * 2. ALLGATHER phase: Gather all reduced chunks at all ranks
 * 
 * Communication Pattern:
 * Phase 1 (Reduce-Scatter):
 * - Each rank gets a portion of the reduced data
 * - Ring-based reduction with scattering
 * 
 * Phase 2 (Allgather):
 * - All ranks gather all reduced portions
 * - Ring circulation until all have complete result
 * 
 * Reduction Operations (specified in tag field bits 16-31):
 * - 0: SUM, 1: MAX, 2: MIN, 3: PROD, 4: LAND, 5: LOR
 * 
 * SoC Memory Usage:
 * - Allocates buffer on first packet of REDUCE-SCATTER phase
 * - Performs in-memory reduction during phase 1
 * - Buffer stores partial results during REDUCE-SCATTER
 * - Buffer accumulates complete result during ALLGATHER phase
 * - All ranks end with complete reduced result in memory
 * 
 * Result: All ranks have the fully reduced result in SoC memory
 * 
 * Phase Identification:
 * - First half of segments: REDUCE-SCATTER
 * - Second half of segments: ALLGATHER
 * 
 * Efficiency: More efficient than separate Reduce + Broadcast
 */
void accl_handle_allreduce(ACCLNodeState *node, ACCLPacket *packet) {
    /* Find or create state for this allreduce operation */
    CollectiveOpState *op = accl_find_or_create_collective_op(node, packet->header.session_id, OP_ALLREDUCE);
    
    if (op) {
        /* Initialize: two-phase operation with double the segments */
        if (op->total_segments == 0) {
            /* Phase 1 (Reduce-Scatter) + Phase 2 (Allgather) = 2x segments */
            op->total_segments = packet->header.total_segments * node->num_ranks * 2;
        }
        
        /* Update progress across both phases */
        op->segments_received++;
        op->bytes_processed += packet->header.data_length;
        
        /* Identify current phase based on progress */
        /* First 50% = Reduce-Scatter phase, Second 50% = Allgather phase */
        bool is_reduce_scatter_phase = (op->segments_received <= op->total_segments / 2);
        const char *phase = is_reduce_scatter_phase ? "REDUCE-SCATTER" : "ALLGATHER";
        
        /* Extract reduction function ID from upper 16 bits of tag field */
        uint32_t func_id = (packet->header.tag >> 16) & 0xFFFF;
        const char *op_names[] = {"SUM", "MAX", "MIN", "PROD", "LAND", "LOR"};
        const char *op_name = (func_id < 6) ? op_names[func_id] : "UNKNOWN";
        
        /* Track bidirectional communication for both phases */
        if (packet->header.src_rank == node->rank) {
            /* This node is sending in current phase */
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] ALLREDUCE [%s/%s]: sending to rank %u, %u bytes, seg=%u/%u",
                   node->rank, phase, op_name, packet->header.dst_rank, packet->header.data_length,
                   packet->header.payload_segment, packet->header.total_segments);
        } else if (packet->header.dst_rank == node->rank) {
            /* This node is receiving in current phase */
            node->stats.bytes_received += packet->header.data_length;
            
            /* Find or allocate SoC memory buffer for this allreduce operation */
            SoCMemoryBuffer *buffer = accl_find_buffer(&node->memory, packet->header.session_id);
            if (!buffer && is_reduce_scatter_phase) {
                /* First data arrival in reduce-scatter phase - allocate buffer */
                uint32_t buffer_size = packet->header.total_segments * MAX_PACKETSIZE * node->num_ranks;
                buffer = accl_allocate_buffer(&node->memory, buffer_size, 
                                             packet->header.session_id, OP_ALLREDUCE);
                
                if (buffer) {
                    /* Initialize buffer with local rank's data (simulated as rank * 100) */
                    uint32_t local_val = htonl(node->rank * 100);
                    for (uint32_t i = 0; i < buffer_size / 4; i++) {
                        memcpy(buffer->data + i * 4, &local_val, 4);
                    }
                    buffer->used = buffer_size;
                }
            }
            
            if (buffer && packet->payload && is_reduce_scatter_phase) {
                /* REDUCE-SCATTER phase: Perform in-memory reduction */
                accl_reduce_buffer_data(buffer, packet->payload, 
                                       packet->header.data_length, func_id);
                
                /* Extract first value for display */
                uint32_t result_val = 0;
                if (buffer->used >= 4) {
                    result_val = ntohl(*(uint32_t*)buffer->data);
                }
                
                uint32_t incoming_val = 0;
                if (packet->header.data_length >= 4) {
                    incoming_val = ntohl(*(uint32_t*)packet->payload);
                }
                
                printf("  [Node %u] ALLREDUCE [%s/%s]: receiving from rank %u, %u bytes\n",
                       node->rank, phase, op_name, packet->header.src_rank, packet->header.data_length);
                printf("            [SoC Mem] Buffer %u: reduced incoming=%u => result=%u (in memory)",
                       buffer->buffer_id, incoming_val, result_val);
            } else if (buffer && packet->payload && !is_reduce_scatter_phase) {
                /* ALLGATHER phase: Store data in buffer (no reduction) */
                uint32_t offset = packet->header.payload_segment * packet->header.data_length;
                accl_write_to_buffer(buffer, packet->payload, offset, packet->header.data_length);
                
                printf("  [Node %u] ALLREDUCE [%s]: gathering from rank %u, %u bytes\n",
                       node->rank, phase, packet->header.src_rank, packet->header.data_length);
                printf("            [SoC Mem] Buffer %u: stored at offset %u",
                       buffer->buffer_id, offset);
            } else {
                printf("  [Node %u] ALLREDUCE [%s]: receiving from rank %u, %u bytes, seg=%u/%u",
                       node->rank, phase, packet->header.src_rank, packet->header.data_length,
                       packet->header.payload_segment, packet->header.total_segments);
            }
        }
        
        /* Check if both phases complete: all ranks have reduced result */
        if (op->segments_received >= op->total_segments) {
            op->completed = true;
            printf(" [OP COMPLETE: all ranks have reduced result]\n");
        } else {
            printf("\n");
        }
    }
}

/**
 * Handle REDUCE_SCATTER operation
 * 
 * Purpose: Reduce data and scatter the result chunks to different ranks
 * 
 * Communication Pattern:
 * - Combines reduction and scattering in a single operation
 * - Each rank gets a unique portion of the reduced result
 * - More efficient than separate Reduce + Scatter
 * 
 * Example (4 ranks, 4 chunks):
 * - All ranks contribute to reduction
 * - Rank 0 gets reduced chunk 0
 * - Rank 1 gets reduced chunk 1
 * - Rank 2 gets reduced chunk 2
 * - Rank 3 gets reduced chunk 3
 * 
 * Reduction Operations (specified in tag field bits 16-31):
 * - 0: SUM, 1: MAX, 2: MIN, 3: PROD, 4: LAND, 5: LOR
 * 
 * SoC Memory Usage:
 * - Allocates buffer on first packet to store accumulated reduction result
 * - Performs in-memory reduction as data arrives
 * - Each rank maintains buffer for its designated chunk
 * - Buffer persists until operation completes
 * 
 * Result: Each rank has a different reduced chunk in SoC memory
 */
void accl_handle_reduce_scatter(ACCLNodeState *node, ACCLPacket *packet) {
    /* Find or create state for this reduce-scatter operation */
    CollectiveOpState *op = accl_find_or_create_collective_op(node, packet->header.session_id, OP_REDUCE_SCATTER);
    
    if (op) {
        /* Initialize: combined reduction and chunk distribution */
        if (op->total_segments == 0) {
            /* Total segments = segments per chunk * number of ranks */
            op->total_segments = packet->header.total_segments * node->num_ranks;
        }
        
        /* Update progress: each rank combines and distributes chunks */
        op->segments_received++;
        op->bytes_processed += packet->header.data_length;
        
        /* Extract reduction function ID from upper 16 bits of tag field */
        uint32_t func_id = (packet->header.tag >> 16) & 0xFFFF;
        const char *op_names[] = {"SUM", "MAX", "MIN", "PROD", "LAND", "LOR"};
        const char *op_name = (func_id < 6) ? op_names[func_id] : "UNKNOWN";
        
        /* Track chunk distribution: each rank sends/receives different reduced chunks */
        if (packet->header.src_rank == node->rank) {
            /* This node is sending a reduced chunk to a specific rank */
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] REDUCE_SCATTER: sending %s chunk to rank %u, %u bytes",
                   node->rank, op_name, packet->header.dst_rank, packet->header.data_length);
        } else if (packet->header.dst_rank == node->rank) {
            /* This node is receiving its designated reduced chunk */
            node->stats.bytes_received += packet->header.data_length;
            
            /* Find or allocate SoC memory buffer for this reduce-scatter operation */
            SoCMemoryBuffer *buffer = accl_find_buffer(&node->memory, packet->header.session_id);
            if (!buffer) {
                /* First data arrival - allocate buffer to store chunk accumulation */
                uint32_t buffer_size = packet->header.total_segments * MAX_PACKETSIZE;
                buffer = accl_allocate_buffer(&node->memory, buffer_size, 
                                             packet->header.session_id, OP_REDUCE_SCATTER);
                
                if (buffer) {
                    /* Initialize buffer with local rank's data (simulated as rank * 100) */
                    uint32_t local_val = htonl(node->rank * 100);
                    for (uint32_t i = 0; i < buffer_size / 4; i++) {
                        memcpy(buffer->data + i * 4, &local_val, 4);
                    }
                    buffer->used = buffer_size;
                }
            }
            
            if (buffer && packet->payload) {
                /* Perform in-memory reduction: combine incoming chunk with buffer contents */
                accl_reduce_buffer_data(buffer, packet->payload, 
                                       packet->header.data_length, func_id);
                
                /* Extract first value for display (convert from big-endian) */
                uint32_t result_val = 0;
                if (buffer->used >= 4) {
                    result_val = ntohl(*(uint32_t*)buffer->data);
                }
                
                uint32_t incoming_val = 0;
                if (packet->header.data_length >= 4) {
                    incoming_val = ntohl(*(uint32_t*)packet->payload);
                }
                
                printf("  [Node %u] REDUCE_SCATTER: %s chunk from rank %u, %u bytes\n",
                       node->rank, op_name, packet->header.src_rank, packet->header.data_length);
                printf("            [SoC Mem] Buffer %u: reduced incoming=%u => chunk_result=%u (in memory)",
                       buffer->buffer_id, incoming_val, result_val);
            } else {
                printf("  [Node %u] REDUCE_SCATTER: receiving chunk from rank %u, %u bytes",
                       node->rank, packet->header.src_rank, packet->header.data_length);
            }
        }
        
        /* Check if complete: all ranks have received their unique reduced chunk */
        if (op->segments_received >= op->total_segments) {
            op->completed = true;
            printf(" [OP COMPLETE: each rank has its reduced chunk]\n");
        } else {
            printf("\n");
        }
    }
}

/**
 * Handle BARRIER operation
 * 
 * Purpose: Synchronization point - all ranks wait until all others arrive
 * 
 * Implementation: Two-phase synchronization
 * 1. ENTRY phase (Gather): Each rank notifies it has entered the barrier
 * 2. EXIT phase (Scatter): Each rank is notified that all have arrived
 * 
 * Communication Pattern:
 * Entry: All ranks → Coordinator (gather notifications)
 * Exit: Coordinator → All ranks (scatter release signals)
 * 
 * Semantics:
 * - No rank proceeds past the barrier until all ranks have reached it
 * - Ensures global synchronization across all processes
 * 
 * State Tracking:
 * - barrier_entered: Counts entry notifications sent
 * - barrier_exited: Counts exit notifications received
 * 
 * Completion:
 * - When a rank has entered (sent notification) AND
 * - Received exit notifications from all other ranks (num_ranks - 1)
 */
void accl_handle_barrier(ACCLNodeState *node, ACCLPacket *packet) {
    /* Barrier uses two-phase synchronization: entry notification + exit confirmation */
    if (packet->header.src_rank == node->rank || packet->header.dst_rank == node->rank) {
        /* Determine current phase based on data flow direction */
        if (packet->header.src_rank == node->rank) {
            /* Phase 1: This node is sending entry notification */
            node->barrier_entered++;
            printf("  [Node %u] BARRIER: entering (notification sent)\n", node->rank);
        }
        if (packet->header.dst_rank == node->rank) {
            /* Phase 2: This node is receiving exit confirmation */
            node->barrier_exited++;
            printf("  [Node %u] BARRIER: received notification (%u/%u)\n", 
                   node->rank, node->barrier_exited, node->num_ranks);
        }
        
        /* Synchronization complete when all ranks notified */
        /* Condition: entered at least once AND received exit from all other ranks */
        if (node->barrier_entered >= 1 && node->barrier_exited >= (node->num_ranks - 1)) {
            printf("  [Node %u] BARRIER: COMPLETE (synchronized)\n", node->rank);
            /* Reset counters for next barrier operation */
            node->barrier_entered = 0;
            node->barrier_exited = 0;
        }
    }
}

/**
 * Handle ALLTOALL operation
 * 
 * Purpose: Complete personalized exchange - each rank sends unique data to every other rank
 * 
 * Communication Pattern:
 * - Rank i sends unique data[j] to Rank j for all j ≠ i
 * - All-to-all personalized communication
 * - Each rank sends N-1 messages (one to each other rank)
 * - Each rank receives N-1 messages (one from each other rank)
 * 
 * Example (3 ranks):
 * Rank 0 → Rank 1: data_0_for_1
 * Rank 0 → Rank 2: data_0_for_2
 * Rank 1 → Rank 0: data_1_for_0
 * Rank 1 → Rank 2: data_1_for_2
 * Rank 2 → Rank 0: data_2_for_0
 * Rank 2 → Rank 1: data_2_for_1
 * 
 * Complexity: O(N²) messages for N ranks
 * 
 * Result: Each rank receives personalized data from every other rank
 */
void accl_handle_alltoall(ACCLNodeState *node, ACCLPacket *packet) {
    /* Find or create state for this alltoall operation */
    CollectiveOpState *op = accl_find_or_create_collective_op(node, packet->header.session_id, OP_ALLTOALL);
    
    if (op) {
        /* Initialize: O(N²) communication pattern for personalized exchange */
        if (op->total_segments == 0) {
            /* Each rank sends unique data to every other rank */
            /* Total = segments per message * N ranks * (N-1) exchanges per rank */
            op->total_segments = packet->header.total_segments * node->num_ranks * (node->num_ranks - 1);
        }
        
        /* Update progress: track all personalized exchanges */
        op->segments_received++;
        op->bytes_processed += packet->header.data_length;
        
        /* Track personalized data exchange */
        if (packet->header.src_rank == node->rank) {
            /* This node is sending unique data to a specific rank */
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] ALLTOALL: sending unique data to rank %u, %u bytes",
                   node->rank, packet->header.dst_rank, packet->header.data_length);
        } else if (packet->header.dst_rank == node->rank) {
            /* This node is receiving unique data from a specific rank */
            node->stats.bytes_received += packet->header.data_length;
            printf("  [Node %u] ALLTOALL: receiving unique data from rank %u, %u bytes",
                   node->rank, packet->header.src_rank, packet->header.data_length);
        }
        printf("\n");
    }
}
