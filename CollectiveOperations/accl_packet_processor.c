/**
 * @file accl_packet_processor.c
 * @brief ACCL Packet Processor - Implementation
 * 
 * Implementation of ACCL packet processing functions that simulate
 * send/recv and collective operations based on packet traces.
 */

#include "accl_packet_processor.h"
#include <stdlib.h>
#include <string.h>

/* Platform-specific includes for byte order conversion */
#ifdef _WIN32
    #include <winsock2.h>
    /* Windows byte order functions */
    #define be64toh(x) _byteswap_uint64(x)
    #ifndef htobe64
        #define htobe64(x) _byteswap_uint64(x)
    #endif
#else
    #include <arpa/inet.h>
    #ifdef __APPLE__
        #include <libkern/OSByteOrder.h>
        #define be64toh(x) OSSwapBigToHostInt64(x)
        #define htobe64(x) OSSwapHostToBigInt64(x)
    #elif defined(__linux__)
        #include <endian.h>
    #endif
#endif

/* Initialize a node state */
ACCLNodeState* accl_init_node(uint32_t rank, uint32_t num_ranks) {
    ACCLNodeState *node = (ACCLNodeState*)malloc(sizeof(ACCLNodeState));
    if (!node) return NULL;
    
    node->rank = rank;
    node->num_ranks = num_ranks;
    
    memset(&node->stats, 0, sizeof(ACCLStatistics));
    
    /* Initialize collective operation tracking */
    memset(node->collective_ops, 0, sizeof(node->collective_ops));
    node->num_collective_ops = 0;
    
    /* Initialize pending message tracking */
    memset(node->pending_msgs, 0, sizeof(node->pending_msgs));
    node->num_pending_msgs = 0;
    
    /* Initialize barrier state */
    node->barrier_entered = 0;
    node->barrier_exited = 0;
    
    return node;
}

/* Free node resources */
void accl_free_node(ACCLNodeState *node) {
    if (!node) return;
    free(node);
}

/* Convert network byte order to host byte order for packet header */
void accl_ntoh_packet_header(ACCLPacketHeader *header) {
    header->protocol_number = ntohs(header->protocol_number);
    header->version = ntohs(header->version);
    header->packet_type = ntohl(header->packet_type);
    header->operation = ntohl(header->operation);
    header->src_rank = ntohl(header->src_rank);
    header->dst_rank = ntohl(header->dst_rank);
    header->tag = ntohl(header->tag);
    header->session_id = ntohl(header->session_id);
    header->sequence_number = ntohl(header->sequence_number);
    header->data_length = ntohl(header->data_length);
    header->timestamp = ntohl(header->timestamp);
    header->flags = ntohl(header->flags);
    header->address = be64toh(header->address);
    header->payload_segment = ntohl(header->payload_segment);
    header->total_segments = ntohl(header->total_segments);
    header->checksum = ntohl(header->checksum);
}

/* Load packets from binary file */
int accl_load_packets_from_binary(ACCLNodeState *node, const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return -1;
    }
    
    /* Read file header: ACCL + version + num_ranks + num_packets */
    char protocol[4];
    uint32_t version, num_ranks, num_packets;
    
    if (fread(protocol, 1, 4, fp) != 4 ||
        fread(&version, 4, 1, fp) != 1 ||
        fread(&num_ranks, 4, 1, fp) != 1 ||
        fread(&num_packets, 4, 1, fp) != 1) {
        fprintf(stderr, "Error: Cannot read file header\n");
        fclose(fp);
        return -1;
    }
    
    version = ntohl(version);
    num_ranks = ntohl(num_ranks);
    num_packets = ntohl(num_packets);
    
    if (strncmp(protocol, "ACCL", 4) != 0) {
        fprintf(stderr, "Error: Invalid protocol number\n");
        fclose(fp);
        return -1;
    }
    
    printf("Processing packets from binary file (network simulation mode)...\n");
    printf("File info: version=%u, ranks=%u\n", version, num_ranks);
    
    /* Process packets one-by-one without storing them - like real network */
    uint32_t packets_processed = 0;
    
    while (!feof(fp)) {
        /* Allocate a single packet buffer (like receiving from network) */
        ACCLPacket *pkt = (ACCLPacket*)malloc(sizeof(ACCLPacket));
        if (!pkt) {
            fprintf(stderr, "Error: Cannot allocate packet buffer\n");
            fclose(fp);
            return -1;
        }
        
        /* Read header (64 bytes) */
        size_t read = fread(&pkt->header, sizeof(ACCLPacketHeader), 1, fp);
        if (read != 1) {
            /* End of file reached */
            free(pkt);
            break;
        }
        
        /* Convert to host byte order */
        accl_ntoh_packet_header(&pkt->header);
        
        /* Verify protocol */
        if (pkt->header.protocol_number != PROTOCOL_NUMBER) {
            fprintf(stderr, "Warning: Invalid protocol number in packet %u\n", packets_processed);
        }
        
        /* Read payload if exists */
        if (pkt->header.data_length > 0) {
            pkt->payload = (uint8_t*)malloc(pkt->header.data_length);
            if (!pkt->payload) {
                fprintf(stderr, "Error: Cannot allocate payload for packet %u\n", packets_processed);
                free(pkt);
                fclose(fp);
                return -1;
            }
            
            if (fread(pkt->payload, 1, pkt->header.data_length, fp) != pkt->header.data_length) {
                fprintf(stderr, "Error: Cannot read packet %u payload\n", packets_processed);
                free(pkt->payload);
                free(pkt);
                fclose(fp);
                return -1;
            }
        } else {
            pkt->payload = NULL;
        }
        
        /* Process packet immediately (like real network handler) */
        accl_process_packet(node, pkt);
        
        /* Free packet after processing */
        if (pkt->payload) {
            free(pkt->payload);
        }
        free(pkt);
        
        packets_processed++;
    }
    
    fclose(fp);
    printf("Processed %u packets successfully\n", packets_processed);
    return 0;
}

/* Load packets from hex file */
int accl_load_packets_from_hex(ACCLNodeState *node, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return -1;
    }
    
    printf("Processing packets from hex file (network simulation mode)...\n");
    
    /* Process packets one-by-one without storing them - like real network */
    char line[16384];
    uint32_t packets_processed = 0;
    
    /* Read packets line by line */
    while (fgets(line, sizeof(line), fp)) {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n') continue;
        
        /* Remove newline */
        line[strcspn(line, "\n")] = 0;
        
        size_t hex_len = strlen(line);
        if (hex_len < 128) continue; /* Header is 64 bytes = 128 hex chars */

        /* Allocate a single packet buffer */
        ACCLPacket *pkt = (ACCLPacket*)malloc(sizeof(ACCLPacket));
        if (!pkt) {
            fprintf(stderr, "Error: Cannot allocate packet buffer\n");
            fclose(fp);
            return -1;
        }
        
        /* Parse header (64 bytes = 128 hex characters) */
        uint8_t header_bytes[sizeof(ACCLPacketHeader)];
        for (size_t i = 0; i < sizeof(ACCLPacketHeader); i++) {
            unsigned int byte;
            if (sscanf(&line[i * 2], "%2x", &byte) != 1) {
                fprintf(stderr, "Error: Invalid hex data at packet %u\n", packets_processed);
                free(pkt);
                fclose(fp);
                return -1;
            }
            header_bytes[i] = (uint8_t)byte;
        }
        
        memcpy(&pkt->header, header_bytes, sizeof(ACCLPacketHeader));
        accl_ntoh_packet_header(&pkt->header);
        
        /* Parse payload */
        size_t payload_hex_chars = hex_len - 128;
        size_t payload_bytes = payload_hex_chars / 2;
        
        if (payload_bytes > 0) {
            pkt->payload = (uint8_t*)malloc(payload_bytes);
            if (!pkt->payload) {
                fprintf(stderr, "Error: Cannot allocate payload\n");
                free(pkt);
                fclose(fp);
                return -1;
            }
            
            for (size_t i = 0; i < payload_bytes; i++) {
                unsigned int byte;
                if (sscanf(&line[128 + i * 2], "%2x", &byte) != 1) {
                    fprintf(stderr, "Error: Invalid payload hex at packet %u\n", packets_processed);
                    free(pkt->payload);
                    free(pkt);
                    fclose(fp);
                    return -1;
                }
                pkt->payload[i] = (uint8_t)byte;
            }
        } else {
            pkt->payload = NULL;
        }
        
        /* Process packet immediately (like real network handler) */
        accl_process_packet(node, pkt);
        
        /* Free packet after processing (like real network) */
        if (pkt->payload) {
            free(pkt->payload);
        }
        free(pkt);
        
        packets_processed++;
    }
    
    fclose(fp);
    printf("Processed %u packets successfully\n", packets_processed);
    return 0;
}

/* Process a single packet */
void accl_process_packet(ACCLNodeState *node, ACCLPacket *packet) {
    if (!node || !packet) return;
    
    /* Update statistics */
    node->stats.total_packets++;
    node->stats.total_data_bytes += packet->header.data_length;
    
    /* Count by packet type */
    switch (packet->header.packet_type) {
        case PTYPE_DATA_EAGER:
            node->stats.eager_packets++;
            break;
        case PTYPE_RNDZV_ADDR:
            node->stats.rendezvous_addr_packets++;
            break;
        case PTYPE_RNDZV_DATA:
            node->stats.rendezvous_data_packets++;
            break;
        case PTYPE_RNDZV_COMPLETE:
            node->stats.rendezvous_complete_packets++;
            break;
        case PTYPE_COLLECTIVE_DATA:
            node->stats.collective_packets++;
            break;
    }
    
    /* Count by operation type */
    switch (packet->header.operation) {
        case OP_SEND:
        case OP_RECV:
            node->stats.send_recv_packets++;
            accl_handle_send_recv(node, packet);
            break;
        case OP_BROADCAST:
            node->stats.broadcast_packets++;
            accl_handle_broadcast(node, packet);
            break;
        case OP_SCATTER:
            node->stats.scatter_packets++;
            accl_handle_scatter(node, packet);
            break;
        case OP_GATHER:
            node->stats.gather_packets++;
            accl_handle_gather(node, packet);
            break;
        case OP_REDUCE:
            node->stats.reduce_packets++;
            accl_handle_reduce(node, packet);
            break;
        case OP_ALLGATHER:
            node->stats.allgather_packets++;
            accl_handle_allgather(node, packet);
            break;
        case OP_ALLREDUCE:
            node->stats.allreduce_packets++;
            accl_handle_allreduce(node, packet);
            break;
        case OP_REDUCE_SCATTER:
            node->stats.reduce_scatter_packets++;
            accl_handle_reduce_scatter(node, packet);
            break;
        case OP_BARRIER:
            node->stats.barrier_packets++;
            accl_handle_barrier(node, packet);
            break;
        case OP_ALLTOALL:
            node->stats.alltoall_packets++;
            accl_handle_alltoall(node, packet);
            break;
    }
}

/* Helper function: Find or create collective operation state */
CollectiveOpState* accl_find_or_create_collective_op(ACCLNodeState *node, uint32_t session_id, OperationType op) {
    /* First, try to find existing operation */
    for (uint32_t i = 0; i < node->num_collective_ops; i++) {
        if (node->collective_ops[i].session_id == session_id && 
            node->collective_ops[i].operation == op) {
            return &node->collective_ops[i];
        }
    }
    
    /* Create new operation if not found and space available */
    if (node->num_collective_ops < MAX_COLLECTIVE_OPS) {
        CollectiveOpState *op_state = &node->collective_ops[node->num_collective_ops];
        memset(op_state, 0, sizeof(CollectiveOpState));
        op_state->session_id = session_id;
        op_state->operation = op;
        op_state->in_progress = true;
        op_state->completed = false;
        node->num_collective_ops++;
        return op_state;
    }
    
    return NULL;
}

/* Helper function: Find or create pending message state */
PendingMessage* accl_find_or_create_pending_msg(ACCLNodeState *node, uint32_t tag, uint32_t peer_rank, bool is_send) {
    /* First, try to find existing message */
    for (uint32_t i = 0; i < node->num_pending_msgs; i++) {
        if (node->pending_msgs[i].tag == tag && 
            node->pending_msgs[i].peer_rank == peer_rank &&
            node->pending_msgs[i].is_send == is_send &&
            !node->pending_msgs[i].completed) {
            return &node->pending_msgs[i];
        }
    }
    
    /* Create new message if not found and space available */
    if (node->num_pending_msgs < MAX_PENDING_MSGS) {
        PendingMessage *msg = &node->pending_msgs[node->num_pending_msgs];
        memset(msg, 0, sizeof(PendingMessage));
        msg->tag = tag;
        msg->peer_rank = peer_rank;
        msg->is_send = is_send;
        msg->completed = false;
        node->num_pending_msgs++;
        return msg;
    }
    
    return NULL;
}

/* Handle send/recv operations */
void accl_handle_send_recv(ACCLNodeState *node, ACCLPacket *packet) {
    bool is_sender = (packet->header.src_rank == node->rank);
    bool is_receiver = (packet->header.dst_rank == node->rank);
    
    if (!is_sender && !is_receiver) {
        return; /* Not relevant to this node */
    }
    
    uint32_t peer_rank = is_sender ? packet->header.dst_rank : packet->header.src_rank;
    PendingMessage *msg = accl_find_or_create_pending_msg(node, packet->header.tag, peer_rank, is_sender);
    
    if (msg) {
        if (msg->expected_segments == 0) {
            msg->expected_segments = packet->header.total_segments;
            msg->session_id = packet->header.session_id;
        }
        
        msg->received_segments++;
        msg->bytes += packet->header.data_length;
        
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
        
        /* Check if message is complete */
        if (msg->received_segments >= msg->expected_segments) {
            msg->completed = true;
            printf(" [COMPLETE: %lu bytes total]\n", msg->bytes);
        } else {
            printf("\n");
        }
    } else {
        /* Fallback if state tracking is full */
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

/* Handle broadcast operation */
void accl_handle_broadcast(ACCLNodeState *node, ACCLPacket *packet) {
    CollectiveOpState *op = accl_find_or_create_collective_op(node, packet->header.session_id, OP_BROADCAST);
    
    if (op) {
        if (op->total_segments == 0) {
            op->total_segments = packet->header.total_segments;
            op->root_rank = packet->header.src_rank;
        }
        
        op->segments_received++;
        op->bytes_processed += packet->header.data_length;
        
        if (packet->header.src_rank == node->rank) {
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] BROADCAST (root) to rank %u: %u bytes, seg=%u/%u",
                   node->rank, packet->header.dst_rank, packet->header.data_length,
                   packet->header.payload_segment, packet->header.total_segments);
        } else if (packet->header.dst_rank == node->rank) {
            node->stats.bytes_received += packet->header.data_length;
            printf("  [Node %u] BROADCAST from root %u: %u bytes received, seg=%u/%u",
                   node->rank, packet->header.src_rank, packet->header.data_length,
                   packet->header.payload_segment, packet->header.total_segments);
        }
        
        /* Check if this collective is complete (received all segments) */
        if (op->segments_received >= op->total_segments) {
            op->completed = true;
            printf(" [OP COMPLETE: %lu bytes total]\n", op->bytes_processed);
        } else {
            printf("\n");
        }
    }
}

/* Handle scatter operation */
void accl_handle_scatter(ACCLNodeState *node, ACCLPacket *packet) {
    CollectiveOpState *op = accl_find_or_create_collective_op(node, packet->header.session_id, OP_SCATTER);
    
    if (op) {
        if (op->total_segments == 0) {
            op->total_segments = packet->header.total_segments;
            op->root_rank = packet->header.src_rank;
        }
        
        op->segments_received++;
        op->bytes_processed += packet->header.data_length;
        
        if (packet->header.src_rank == node->rank) {
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] SCATTER (root) to rank %u: chunk %u bytes, seg=%u/%u",
                   node->rank, packet->header.dst_rank, packet->header.data_length,
                   packet->header.payload_segment, packet->header.total_segments);
        } else if (packet->header.dst_rank == node->rank) {
            node->stats.bytes_received += packet->header.data_length;
            printf("  [Node %u] SCATTER from root %u: chunk %u bytes received, seg=%u/%u",
                   node->rank, packet->header.src_rank, packet->header.data_length,
                   packet->header.payload_segment, packet->header.total_segments);
        }
        
        if (op->segments_received >= op->total_segments) {
            op->completed = true;
            printf(" [OP COMPLETE]\n");
        } else {
            printf("\n");
        }
    }
}

/* Handle gather operation */
void accl_handle_gather(ACCLNodeState *node, ACCLPacket *packet) {
    CollectiveOpState *op = accl_find_or_create_collective_op(node, packet->header.session_id, OP_GATHER);
    
    if (op) {
        if (op->total_segments == 0) {
            op->total_segments = packet->header.total_segments * (node->num_ranks - 1); /* Ring-based */
            op->root_rank = packet->header.dst_rank; /* Final destination */
        }
        
        op->segments_received++;
        op->bytes_processed += packet->header.data_length;
        
        if (packet->header.src_rank == node->rank) {
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] GATHER: sending to next in ring (rank %u), %u bytes, seg=%u/%u",
                   node->rank, packet->header.dst_rank, packet->header.data_length,
                   packet->header.payload_segment, packet->header.total_segments);
        } else if (packet->header.dst_rank == node->rank) {
            node->stats.bytes_received += packet->header.data_length;
            /* Check if this node is root */
            bool is_root = (node->rank == op->root_rank);
            if (is_root) {
                printf("  [Node %u] GATHER (root): collecting from rank %u, %u bytes, seg=%u/%u",
                       node->rank, packet->header.src_rank, packet->header.data_length,
                       packet->header.payload_segment, packet->header.total_segments);
            } else {
                printf("  [Node %u] GATHER: relaying from rank %u, %u bytes, seg=%u/%u",
                       node->rank, packet->header.src_rank, packet->header.data_length,
                       packet->header.payload_segment, packet->header.total_segments);
            }
        }
        printf("\n");
    }
}

/* Handle reduce operation */
void accl_handle_reduce(ACCLNodeState *node, ACCLPacket *packet) {
    CollectiveOpState *op = accl_find_or_create_collective_op(node, packet->header.session_id, OP_REDUCE);
    
    if (op) {
        if (op->total_segments == 0) {
            op->total_segments = packet->header.total_segments * (node->num_ranks - 1);
            op->root_rank = packet->header.dst_rank;
        }
        
        op->segments_received++;
        op->bytes_processed += packet->header.data_length;
        
        if (packet->header.src_rank == node->rank) {
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] REDUCE: sending to next in ring (rank %u) for reduction, %u bytes",
                   node->rank, packet->header.dst_rank, packet->header.data_length);
        } else if (packet->header.dst_rank == node->rank) {
            node->stats.bytes_received += packet->header.data_length;
            bool is_root = (node->rank == op->root_rank);
            if (is_root) {
                printf("  [Node %u] REDUCE (root): combining data from rank %u, %u bytes",
                       node->rank, packet->header.src_rank, packet->header.data_length);
            } else {
                printf("  [Node %u] REDUCE: relaying+combining from rank %u, %u bytes",
                       node->rank, packet->header.src_rank, packet->header.data_length);
            }
        }
        printf("\n");
    }
}

/* Handle allgather operation */
void accl_handle_allgather(ACCLNodeState *node, ACCLPacket *packet) {
    CollectiveOpState *op = accl_find_or_create_collective_op(node, packet->header.session_id, OP_ALLGATHER);
    
    if (op) {
        if (op->total_segments == 0) {
            /* All ranks participate in ring */
            op->total_segments = packet->header.total_segments * node->num_ranks;
        }
        
        op->segments_received++;
        op->bytes_processed += packet->header.data_length;
        
        if (packet->header.src_rank == node->rank) {
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] ALLGATHER: sending to rank %u, %u bytes, seg=%u/%u",
                   node->rank, packet->header.dst_rank, packet->header.data_length,
                   packet->header.payload_segment, packet->header.total_segments);
        } else if (packet->header.dst_rank == node->rank) {
            node->stats.bytes_received += packet->header.data_length;
            printf("  [Node %u] ALLGATHER: receiving from rank %u, %u bytes, seg=%u/%u",
                   node->rank, packet->header.src_rank, packet->header.data_length,
                   packet->header.payload_segment, packet->header.total_segments);
        }
        
        if (op->segments_received >= op->total_segments) {
            op->completed = true;
            printf(" [OP COMPLETE: all ranks have all data]\n");
        } else {
            printf("\n");
        }
    }
}

/* Handle allreduce operation */
void accl_handle_allreduce(ACCLNodeState *node, ACCLPacket *packet) {
    CollectiveOpState *op = accl_find_or_create_collective_op(node, packet->header.session_id, OP_ALLREDUCE);
    
    if (op) {
        if (op->total_segments == 0) {
            /* Allreduce = reduce-scatter + allgather phases */
            op->total_segments = packet->header.total_segments * node->num_ranks * 2;
        }
        
        op->segments_received++;
        op->bytes_processed += packet->header.data_length;
        
        /* Determine phase: reduce-scatter (first half) or allgather (second half) */
        const char *phase = (op->segments_received <= op->total_segments / 2) ? "REDUCE-SCATTER" : "ALLGATHER";
        
        if (packet->header.src_rank == node->rank) {
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] ALLREDUCE [%s]: sending to rank %u, %u bytes, seg=%u/%u",
                   node->rank, phase, packet->header.dst_rank, packet->header.data_length,
                   packet->header.payload_segment, packet->header.total_segments);
        } else if (packet->header.dst_rank == node->rank) {
            node->stats.bytes_received += packet->header.data_length;
            printf("  [Node %u] ALLREDUCE [%s]: receiving from rank %u, %u bytes, seg=%u/%u",
                   node->rank, phase, packet->header.src_rank, packet->header.data_length,
                   packet->header.payload_segment, packet->header.total_segments);
        }
        
        if (op->segments_received >= op->total_segments) {
            op->completed = true;
            printf(" [OP COMPLETE: all ranks have reduced result]\n");
        } else {
            printf("\n");
        }
    }
}

/* Handle reduce_scatter operation */
void accl_handle_reduce_scatter(ACCLNodeState *node, ACCLPacket *packet) {
    CollectiveOpState *op = accl_find_or_create_collective_op(node, packet->header.session_id, OP_REDUCE_SCATTER);
    
    if (op) {
        if (op->total_segments == 0) {
            op->total_segments = packet->header.total_segments * node->num_ranks;
        }
        
        op->segments_received++;
        op->bytes_processed += packet->header.data_length;
        
        if (packet->header.src_rank == node->rank) {
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] REDUCE_SCATTER: sending chunk to rank %u, %u bytes",
                   node->rank, packet->header.dst_rank, packet->header.data_length);
        } else if (packet->header.dst_rank == node->rank) {
            node->stats.bytes_received += packet->header.data_length;
            printf("  [Node %u] REDUCE_SCATTER: receiving chunk from rank %u, %u bytes",
                   node->rank, packet->header.src_rank, packet->header.data_length);
        }
        
        if (op->segments_received >= op->total_segments) {
            op->completed = true;
            printf(" [OP COMPLETE: each rank has its reduced chunk]\n");
        } else {
            printf("\n");
        }
    }
}

/* Handle barrier operation */
void accl_handle_barrier(ACCLNodeState *node, ACCLPacket *packet) {
    /* Barrier has two phases: gather (entry) and scatter (exit) */
    if (packet->header.src_rank == node->rank || packet->header.dst_rank == node->rank) {
        /* Determine if this is entry or exit phase based on data flow pattern */
        if (packet->header.src_rank == node->rank) {
            node->barrier_entered++;
            printf("  [Node %u] BARRIER: entering (notification sent)\n", node->rank);
        }
        if (packet->header.dst_rank == node->rank) {
            node->barrier_exited++;
            printf("  [Node %u] BARRIER: received notification (%u/%u)\n", 
                   node->rank, node->barrier_exited, node->num_ranks);
        }
        
        /* Barrier complete when all ranks have entered and exited */
        if (node->barrier_entered >= 1 && node->barrier_exited >= (node->num_ranks - 1)) {
            printf("  [Node %u] BARRIER: COMPLETE (synchronized)\n", node->rank);
            node->barrier_entered = 0;
            node->barrier_exited = 0;
        }
    }
}

/* Handle alltoall operation */
void accl_handle_alltoall(ACCLNodeState *node, ACCLPacket *packet) {
    CollectiveOpState *op = accl_find_or_create_collective_op(node, packet->header.session_id, OP_ALLTOALL);
    
    if (op) {
        if (op->total_segments == 0) {
            /* Each rank sends to every other rank */
            op->total_segments = packet->header.total_segments * node->num_ranks * (node->num_ranks - 1);
        }
        
        op->segments_received++;
        op->bytes_processed += packet->header.data_length;
        
        if (packet->header.src_rank == node->rank) {
            node->stats.bytes_sent += packet->header.data_length;
            printf("  [Node %u] ALLTOALL: sending unique data to rank %u, %u bytes",
                   node->rank, packet->header.dst_rank, packet->header.data_length);
        } else if (packet->header.dst_rank == node->rank) {
            node->stats.bytes_received += packet->header.data_length;
            printf("  [Node %u] ALLTOALL: receiving unique data from rank %u, %u bytes",
                   node->rank, packet->header.src_rank, packet->header.data_length);
        }
        printf("\n");
    }
}

/* Print packet information */
void accl_print_packet(const ACCLPacket *packet) {
    if (!packet) return;
    
    printf("Packet Info:\n");
    printf("  Protocol: 0x%04X, Version: 0x%04X\n", 
           packet->header.protocol_number, packet->header.version);
    printf("  Type: %s, Operation: %s\n",
           accl_packet_type_str(packet->header.packet_type),
           accl_operation_str(packet->header.operation));
    printf("  SRC=%u, DST=%u, TAG=%u\n",
           packet->header.src_rank, packet->header.dst_rank, packet->header.tag);
    printf("  Session=%u, Seq=%u, Len=%u\n",
           packet->header.session_id, packet->header.sequence_number, packet->header.data_length);
    printf("  Timestamp=%u, Flags=0x%08X\n",
           packet->header.timestamp, packet->header.flags);
    printf("  Address=0x%016lX\n", packet->header.address);
    printf("  Segments: %u/%u\n",
           packet->header.payload_segment + 1, packet->header.total_segments);
}

/* Print statistics */
void accl_print_statistics(const ACCLStatistics *stats) {
    if (!stats) return;
    
    printf("\n");
    printf("========================================\n");
    printf("ACCL Packet Processing Statistics\n");
    printf("========================================\n");
    printf("Total Packets: %lu\n", stats->total_packets);
    printf("Total Data: %lu bytes (%.2f MB)\n", 
           stats->total_data_bytes, stats->total_data_bytes / (1024.0 * 1024.0));
    printf("  Bytes Sent:     %lu bytes (%.2f MB)\n",
           stats->bytes_sent, stats->bytes_sent / (1024.0 * 1024.0));
    printf("  Bytes Received: %lu bytes (%.2f MB)\n",
           stats->bytes_received, stats->bytes_received / (1024.0 * 1024.0));
    printf("\n");
    printf("By Operation:\n");
    printf("  Send/Recv:       %10lu\n", stats->send_recv_packets);
    printf("  Broadcast:       %10lu\n", stats->broadcast_packets);
    printf("  Scatter:         %10lu\n", stats->scatter_packets);
    printf("  Gather:          %10lu\n", stats->gather_packets);
    printf("  Reduce:          %10lu\n", stats->reduce_packets);
    printf("  Allgather:       %10lu\n", stats->allgather_packets);
    printf("  Allreduce:       %10lu\n", stats->allreduce_packets);
    printf("  Reduce_Scatter:  %10lu\n", stats->reduce_scatter_packets);
    printf("  Barrier:         %10lu\n", stats->barrier_packets);
    printf("  Alltoall:        %10lu\n", stats->alltoall_packets);
    printf("\n");
    printf("By Packet Type:\n");
    printf("  Eager:           %10lu\n", stats->eager_packets);
    printf("  Rendezvous Addr: %10lu\n", stats->rendezvous_addr_packets);
    printf("  Rendezvous Data: %10lu\n", stats->rendezvous_data_packets);
    printf("  Rendezvous Done: %10lu\n", stats->rendezvous_complete_packets);
    printf("  Collective:      %10lu\n", stats->collective_packets);
    printf("========================================\n");
}

/* Utility: Get packet type string */
const char* accl_packet_type_str(PacketType type) {
    switch (type) {
        case PTYPE_DATA_EAGER: return "DATA_EAGER";
        case PTYPE_RNDZV_ADDR: return "RNDZV_ADDR";
        case PTYPE_RNDZV_DATA: return "RNDZV_DATA";
        case PTYPE_RNDZV_COMPLETE: return "RNDZV_COMPLETE";
        case PTYPE_COLLECTIVE_DATA: return "COLLECTIVE_DATA";
        default: return "UNKNOWN";
    }
}

/* Utility: Get operation string */
const char* accl_operation_str(OperationType op) {
    switch (op) {
        case OP_SEND: return "SEND";
        case OP_RECV: return "RECV";
        case OP_BROADCAST: return "BROADCAST";
        case OP_SCATTER: return "SCATTER";
        case OP_GATHER: return "GATHER";
        case OP_REDUCE: return "REDUCE";
        case OP_ALLGATHER: return "ALLGATHER";
        case OP_ALLREDUCE: return "ALLREDUCE";
        case OP_REDUCE_SCATTER: return "REDUCE_SCATTER";
        case OP_BARRIER: return "BARRIER";
        case OP_ALLTOALL: return "ALLTOALL";
        default: return "UNKNOWN";
    }
}
