/**
 * @file accl_operations.h
 * @brief Collective and Point-to-Point Operation Handlers
 * 
 * Declares handlers for all ACCL operations including send/recv
 * and collective operations.
 */

#ifndef ACCL_OPERATIONS_H
#define ACCL_OPERATIONS_H

#include "accl_types.h"

/* ============================================================================
 * State Management Functions
 * ============================================================================ */

/**
 * Find or create a collective operation state
 * @param node Pointer to node state
 * @param session_id Session ID to find/create
 * @param op Operation type
 * @return Pointer to collective operation state, or NULL on error
 */
CollectiveOpState* accl_find_or_create_collective_op(ACCLNodeState *node, uint32_t session_id, OperationType op);

/**
 * Find or create a pending message state
 * @param node Pointer to node state
 * @param tag Message tag
 * @param peer_rank Peer rank
 * @param is_send true for send, false for recv
 * @return Pointer to pending message state, or NULL on error
 */
PendingMessage* accl_find_or_create_pending_msg(ACCLNodeState *node, uint32_t tag, uint32_t peer_rank, bool is_send);

/* ============================================================================
 * Operation Handlers
 * ============================================================================ */

/**
 * Handle send/recv operations (eager and rendezvous protocols)
 */
void accl_handle_send_recv(ACCLNodeState *node, ACCLPacket *packet);

/**
 * Handle broadcast operation
 */
void accl_handle_broadcast(ACCLNodeState *node, ACCLPacket *packet);

/**
 * Handle scatter operation
 */
void accl_handle_scatter(ACCLNodeState *node, ACCLPacket *packet);

/**
 * Handle gather operation
 */
void accl_handle_gather(ACCLNodeState *node, ACCLPacket *packet);

/**
 * Handle reduce operation
 */
void accl_handle_reduce(ACCLNodeState *node, ACCLPacket *packet);

/**
 * Handle allgather operation
 */
void accl_handle_allgather(ACCLNodeState *node, ACCLPacket *packet);

/**
 * Handle allreduce operation
 */
void accl_handle_allreduce(ACCLNodeState *node, ACCLPacket *packet);

/**
 * Handle reduce-scatter operation
 */
void accl_handle_reduce_scatter(ACCLNodeState *node, ACCLPacket *packet);

/**
 * Handle barrier operation
 */
void accl_handle_barrier(ACCLNodeState *node, ACCLPacket *packet);

/**
 * Handle alltoall operation
 */
void accl_handle_alltoall(ACCLNodeState *node, ACCLPacket *packet);

#endif /* ACCL_OPERATIONS_H */
