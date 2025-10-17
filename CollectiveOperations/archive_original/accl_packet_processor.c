/**
 * @file accl_packet_processor.c
 * @brief ACCL Packet Processor - Implementation
 * 
 * Implementation of ACCL packet processing functions that simulate
 * send/recv and collective operations based on packet traces.
 * 
 * Baremetal compatible - no external library dependencies.
 */

#include "accl_packet_processor.h"
#include <stdlib.h>  /* Only for malloc/free/calloc */

/* ============================================================================
 * Baremetal-compatible utility functions
 * ============================================================================ */

/**
 * Custom memset implementation for baremetal systems
 */
static inline void* bm_memset(void* dest, int value, size_t count) {
    unsigned char* d = (unsigned char*)dest;
    unsigned char val = (unsigned char)value;
    while (count--) {
        *d++ = val;
    }
    return dest;
}

/**
 * Custom memcpy implementation for baremetal systems
 */
static inline void* bm_memcpy(void* dest, const void* src, size_t count) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (count--) {
        *d++ = *s++;
    }
    return dest;
}

/**
 * Custom strlen implementation for baremetal systems
 */
static inline size_t bm_strlen(const char* str) {
    const char* s = str;
    while (*s) {
        s++;
    }
    return (s - str);
}

/**
 * Custom strncmp implementation for baremetal systems
 */
static inline int bm_strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return (*(unsigned char*)s1 - *(unsigned char*)s2);
}

/**
 * Custom strcspn implementation for baremetal systems
 * Returns length of initial segment not containing any character from reject
 */
static inline size_t bm_strcspn(const char* s, const char* reject) {
    size_t count = 0;
    while (*s) {
        const char* r = reject;
        while (*r) {
            if (*s == *r) {
                return count;
            }
            r++;
        }
        s++;
        count++;
    }
    return count;
}

/* ============================================================================
 * Byte order conversion functions for baremetal (network to host)
 * Assumes big-endian network byte order
 * ============================================================================ */

/**
 * Check if system is little-endian
 */
static inline int is_little_endian(void) {
    volatile uint32_t i = 0x01234567;
    return (*((uint8_t*)(&i))) == 0x67;
}

/**
 * Convert 32-bit value from network (big-endian) to host byte order
 */
static inline uint32_t bm_ntohl(uint32_t netlong) {
    if (is_little_endian()) {
        return ((netlong & 0x000000FF) << 24) |
               ((netlong & 0x0000FF00) << 8)  |
               ((netlong & 0x00FF0000) >> 8)  |
               ((netlong & 0xFF000000) >> 24);
    }
    return netlong;
}

/**
 * Convert 16-bit value from network (big-endian) to host byte order
 */
static inline uint16_t bm_ntohs(uint16_t netshort) {
    if (is_little_endian()) {
        return ((netshort & 0x00FF) << 8) |
               ((netshort & 0xFF00) >> 8);
    }
    return netshort;
}

/**
 * Convert 64-bit value from big-endian to host byte order
 */
static inline uint64_t bm_be64toh(uint64_t big_endian) {
    if (is_little_endian()) {
        return ((big_endian & 0x00000000000000FFULL) << 56) |
               ((big_endian & 0x000000000000FF00ULL) << 40) |
               ((big_endian & 0x0000000000FF0000ULL) << 24) |
               ((big_endian & 0x00000000FF000000ULL) << 8)  |
               ((big_endian & 0x000000FF00000000ULL) >> 8)  |
               ((big_endian & 0x0000FF0000000000ULL) >> 24) |
               ((big_endian & 0x00FF000000000000ULL) >> 40) |
               ((big_endian & 0xFF00000000000000ULL) >> 56);
    }
    return big_endian;
}

/**
 * Convert 32-bit value from host to network (big-endian) byte order
 */
static inline uint32_t bm_htonl(uint32_t hostlong) {
    return bm_ntohl(hostlong);  /* Same operation */
}

/**
 * Convert 64-bit value from host to big-endian byte order
 */
static inline uint64_t bm_htobe64(uint64_t host) {
    return bm_be64toh(host);  /* Same operation */
}

/* Define macros for compatibility with existing code */
#define memset bm_memset
#define memcpy bm_memcpy
#define strlen bm_strlen
#define strncmp bm_strncmp
#define strcspn bm_strcspn
#define ntohl bm_ntohl
#define ntohs bm_ntohs
#define htonl bm_htonl
#define htons bm_ntohs
#define be64toh bm_be64toh
#define htobe64 bm_htobe64

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
    
    /* Initialize SoC memory simulation */
    accl_init_memory(&node->memory);
    
    return node;
}

/* Free node resources */
void accl_free_node(ACCLNodeState *node) {
    if (!node) return;
    
    /* Free SoC memory */
    accl_free_memory(&node->memory);
    
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

/* ========================================================================
 * SoC Memory Simulation Functions
 * ======================================================================== */

/**
 * Initialize SoC memory system
 * Simulates on-chip memory for storing packet data and operation buffers
 */
void accl_init_memory(SoCMemory *memory) {
    if (!memory) return;
    
    memory->total_memory = SOC_MEMORY_SIZE;
    memory->used_memory = 0;
    memory->peak_memory = 0;
    memory->next_buffer_id = 1;
    memory->next_address = 0x80000000;  /* Simulated base address */
    memory->num_buffers = 0;
    
    /* Initialize all buffers as free */
    for (int i = 0; i < SOC_MAX_BUFFERS; i++) {
        memory->buffers[i].buffer_id = 0;
        memory->buffers[i].in_use = false;
        memory->buffers[i].data = NULL;
    }
}

/**
 * Free all allocated SoC memory buffers
 */
void accl_free_memory(SoCMemory *memory) {
    if (!memory) return;
    
    for (int i = 0; i < SOC_MAX_BUFFERS; i++) {
        if (memory->buffers[i].in_use && memory->buffers[i].data) {
            free(memory->buffers[i].data);
            memory->buffers[i].data = NULL;
            memory->buffers[i].in_use = false;
        }
    }
    memory->num_buffers = 0;
    memory->used_memory = 0;
}

/**
 * Allocate a memory buffer in simulated SoC memory
 * Used for storing data during collective operations
 */
SoCMemoryBuffer* accl_allocate_buffer(SoCMemory *memory, uint32_t size, uint32_t session_id, OperationType op) {
    if (!memory || size == 0) return NULL;
    
    /* Check if we have space */
    if (memory->used_memory + size > memory->total_memory) {
        fprintf(stderr, "SoC Memory: Out of memory (requested=%u, available=%llu)\n",
                size, (unsigned long long)(memory->total_memory - memory->used_memory));
        return NULL;
    }
    
    /* Find a free buffer slot */
    int slot = -1;
    for (int i = 0; i < SOC_MAX_BUFFERS; i++) {
        if (!memory->buffers[i].in_use) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        fprintf(stderr, "SoC Memory: No free buffer slots available\n");
        return NULL;
    }
    
    /* Allocate the buffer */
    SoCMemoryBuffer *buffer = &memory->buffers[slot];
    buffer->data = (uint8_t*)calloc(size, 1);  /* Zero-initialized */
    if (!buffer->data) {
        fprintf(stderr, "SoC Memory: Failed to allocate %u bytes\n", size);
        return NULL;
    }
    
    buffer->buffer_id = memory->next_buffer_id++;
    buffer->session_id = session_id;
    buffer->address = memory->next_address;
    buffer->size = size;
    buffer->used = 0;
    buffer->in_use = true;
    buffer->operation = op;
    
    memory->next_address += size;
    memory->used_memory += size;
    memory->num_buffers++;
    
    /* Track peak memory usage */
    if (memory->used_memory > memory->peak_memory) {
        memory->peak_memory = memory->used_memory;
    }
    
    printf("  [SoC Memory] Allocated buffer %u: %u bytes at 0x%016llX for session %u (%s)\n",
           buffer->buffer_id, size, (unsigned long long)buffer->address, session_id, accl_operation_str(op));
    
    return buffer;
}

/**
 * Free a specific memory buffer
 */
void accl_free_buffer(SoCMemory *memory, uint32_t buffer_id) {
    if (!memory) return;
    
    for (int i = 0; i < SOC_MAX_BUFFERS; i++) {
        if (memory->buffers[i].in_use && memory->buffers[i].buffer_id == buffer_id) {
            printf("  [SoC Memory] Freed buffer %u: %u bytes\n",
                   buffer_id, memory->buffers[i].size);
            
            free(memory->buffers[i].data);
            memory->buffers[i].data = NULL;
            memory->buffers[i].in_use = false;
            memory->used_memory -= memory->buffers[i].size;
            memory->num_buffers--;
            return;
        }
    }
}

/**
 * Find a buffer associated with a session ID
 */
SoCMemoryBuffer* accl_find_buffer(SoCMemory *memory, uint32_t session_id) {
    if (!memory) return NULL;
    
    for (int i = 0; i < SOC_MAX_BUFFERS; i++) {
        if (memory->buffers[i].in_use && memory->buffers[i].session_id == session_id) {
            return &memory->buffers[i];
        }
    }
    return NULL;
}

/**
 * Write data to a memory buffer (simulates DMA or network interface writing to memory)
 */
int accl_write_to_buffer(SoCMemoryBuffer *buffer, const uint8_t *data, uint32_t offset, uint32_t length) {
    if (!buffer || !buffer->in_use || !buffer->data || !data) return -1;
    
    if (offset + length > buffer->size) {
        fprintf(stderr, "SoC Memory: Write overflow (offset=%u, length=%u, size=%u)\n",
                offset, length, buffer->size);
        return -1;
    }
    
    memcpy(buffer->data + offset, data, length);
    
    /* Update used bytes */
    if (offset + length > buffer->used) {
        buffer->used = offset + length;
    }
    
    return 0;
}

/**
 * Read data from a memory buffer
 */
int accl_read_from_buffer(SoCMemoryBuffer *buffer, uint8_t *data, uint32_t offset, uint32_t length) {
    if (!buffer || !buffer->in_use || !buffer->data || !data) return -1;
    
    if (offset + length > buffer->size) {
        fprintf(stderr, "SoC Memory: Read overflow (offset=%u, length=%u, size=%u)\n",
                offset, length, buffer->size);
        return -1;
    }
    
    memcpy(data, buffer->data + offset, length);
    return 0;
}

/**
 * Perform reduction operation on buffer data
 * Combines incoming data with existing buffer data using specified reduction function
 * 
 * @param buffer - Target buffer containing accumulated data
 * @param incoming_data - New data to reduce with buffer contents
 * @param length - Number of bytes to process (must be multiple of 4 for uint32_t operations)
 * @param func_id - Reduction function: 0=SUM, 1=MAX, 2=MIN, 3=PROD, 4=LAND, 5=LOR
 */
void accl_reduce_buffer_data(SoCMemoryBuffer *buffer, const uint8_t *incoming_data, uint32_t length, uint32_t func_id) {
    if (!buffer || !buffer->in_use || !buffer->data || !incoming_data) return;
    
    /* Ensure length doesn't exceed buffer size */
    if (length > buffer->size) {
        length = buffer->size;
    }
    
    /* Perform reduction on uint32_t values (4 bytes at a time) */
    uint32_t num_elements = length / 4;
    uint32_t *buffer_vals = (uint32_t*)buffer->data;
    const uint32_t *incoming_vals = (const uint32_t*)incoming_data;
    
    for (uint32_t i = 0; i < num_elements; i++) {
        /* Convert from big-endian if needed */
        uint32_t buffer_val = ntohl(buffer_vals[i]);
        uint32_t incoming_val = ntohl(incoming_vals[i]);
        uint32_t result = 0;
        
        switch (func_id) {
            case 0: /* SUM */
                result = buffer_val + incoming_val;
                break;
            case 1: /* MAX */
                result = (buffer_val > incoming_val) ? buffer_val : incoming_val;
                break;
            case 2: /* MIN */
                result = (buffer_val < incoming_val) ? buffer_val : incoming_val;
                break;
            case 3: /* PROD */
                result = buffer_val * incoming_val;
                break;
            case 4: /* LAND (Logical AND) */
                result = (buffer_val && incoming_val) ? 1 : 0;
                break;
            case 5: /* LOR (Logical OR) */
                result = (buffer_val || incoming_val) ? 1 : 0;
                break;
            default:
                result = buffer_val + incoming_val; /* Default to SUM */
                break;
        }
        
        /* Store result back in big-endian */
        buffer_vals[i] = htonl(result);
    }
    
    /* Update used bytes */
    if (length > buffer->used) {
        buffer->used = length;
    }
}

/**
 * Print SoC memory statistics
 */
void accl_print_memory_stats(const SoCMemory *memory) {
    if (!memory) return;
    
    printf("\n=== SoC Memory Statistics ===\n");
    printf("Total Memory: %llu MB\n", (unsigned long long)(memory->total_memory / (1024 * 1024)));
    printf("Used Memory: %llu KB (%.2f%%)\n",
           (unsigned long long)(memory->used_memory / 1024),
           100.0 * memory->used_memory / memory->total_memory);
    printf("Peak Memory: %llu KB (%.2f%%)\n",
           (unsigned long long)(memory->peak_memory / 1024),
           100.0 * memory->peak_memory / memory->total_memory);
    printf("Active Buffers: %u / %d\n", memory->num_buffers, SOC_MAX_BUFFERS);
    
    if (memory->num_buffers > 0) {
        printf("\nActive Buffers:\n");
        for (int i = 0; i < SOC_MAX_BUFFERS; i++) {
            if (memory->buffers[i].in_use) {
                printf("  Buffer %u: Session=%u, Size=%u, Used=%u, Op=%s\n",
                       memory->buffers[i].buffer_id,
                       memory->buffers[i].session_id,
                       memory->buffers[i].size,
                       memory->buffers[i].used,
                       accl_operation_str(memory->buffers[i].operation));
            }
        }
    }
    printf("=============================\n\n");
}

/* ========================================================================
 * End of SoC Memory Simulation Functions
 * ======================================================================== */


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
    
    /* Use a static packet buffer instead of malloc (simulates hardware packet buffer) */
    ACCLPacket pkt_buffer;
    uint8_t payload_buffer[MAX_PACKETSIZE];
    
    while (!feof(fp)) {
        /* Reuse packet buffer (like receiving from network into hardware buffer) */
        ACCLPacket *pkt = &pkt_buffer;
        memset(pkt, 0, sizeof(ACCLPacket));
        
        /* Read header (64 bytes) */
        size_t read = fread(&pkt->header, sizeof(ACCLPacketHeader), 1, fp);
        if (read != 1) {
            /* End of file reached */
            break;
        }
        
        /* Convert to host byte order */
        accl_ntoh_packet_header(&pkt->header);
        
        /* Verify protocol */
        if (pkt->header.protocol_number != PROTOCOL_NUMBER) {
            fprintf(stderr, "Warning: Invalid protocol number in packet %u\n", packets_processed);
        }
        
        /* Read payload if exists - use static buffer instead of malloc */
        if (pkt->header.data_length > 0) {
            /* RNDZV_DATA packets use RDMA - they bypass packet buffers and write directly to memory
             * For simulation, we skip reading the payload since it would go via DMA, not packet processing */
            if (pkt->header.packet_type == PTYPE_RNDZV_DATA) {
                /* Skip payload in file - RDMA would transfer directly to destination memory */
                if (fseek(fp, pkt->header.data_length, SEEK_CUR) != 0) {
                    fprintf(stderr, "Error: Cannot skip RDMA payload for packet %u\n", packets_processed);
                    fclose(fp);
                    return -1;
                }
                pkt->payload = NULL;  /* RDMA bypasses packet buffer */
            } else {
                /* Regular packet - read payload into packet buffer */
                if (pkt->header.data_length > MAX_PACKETSIZE) {
                    fprintf(stderr, "Error: Packet %u payload too large (%u > %u)\n",
                            packets_processed, pkt->header.data_length, MAX_PACKETSIZE);
                    fclose(fp);
                    return -1;
                }
                
                if (fread(payload_buffer, 1, pkt->header.data_length, fp) != pkt->header.data_length) {
                    fprintf(stderr, "Error: Cannot read packet %u payload\n", packets_processed);
                    fclose(fp);
                    return -1;
                }
                pkt->payload = payload_buffer;
            }
        } else {
            pkt->payload = NULL;
        }
        
        /* Process packet immediately (like real network handler) */
        accl_process_packet(node, pkt);
        
        /* No need to free - using static buffers */
        
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
    
    /* Use static buffers instead of malloc (simulates hardware packet buffer) */
    ACCLPacket pkt_buffer;
    uint8_t payload_buffer[MAX_PACKETSIZE];
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

        /* Reuse packet buffer (like receiving from network into hardware buffer) */
        ACCLPacket *pkt = &pkt_buffer;
        memset(pkt, 0, sizeof(ACCLPacket));
        
        /* Parse header (64 bytes = 128 hex characters) */
        uint8_t header_bytes[sizeof(ACCLPacketHeader)];
        for (size_t i = 0; i < sizeof(ACCLPacketHeader); i++) {
            unsigned int byte;
            if (sscanf(&line[i * 2], "%2x", &byte) != 1) {
                fprintf(stderr, "Error: Invalid hex data at packet %u\n", packets_processed);
                fclose(fp);
                return -1;
            }
            header_bytes[i] = (uint8_t)byte;
        }
        
        memcpy(&pkt->header, header_bytes, sizeof(ACCLPacketHeader));
        accl_ntoh_packet_header(&pkt->header);
        
        /* Parse payload - use static buffer instead of malloc */
        size_t payload_hex_chars = hex_len - 128;
        size_t payload_bytes = payload_hex_chars / 2;
        
        if (payload_bytes > 0) {
            /* RNDZV_DATA packets use RDMA - they bypass packet buffers and write directly to memory
             * For simulation, we skip the payload since it would go via DMA, not packet processing */
            if (pkt->header.packet_type == PTYPE_RNDZV_DATA) {
                /* Skip payload - RDMA would transfer directly to destination memory */
                pkt->payload = NULL;  /* RDMA bypasses packet buffer */
            } else {
                /* Regular packet - parse payload into packet buffer */
                if (payload_bytes > MAX_PACKETSIZE) {
                    fprintf(stderr, "Error: Packet %u payload too large (%zu > %u)\n",
                            packets_processed, payload_bytes, MAX_PACKETSIZE);
                    fclose(fp);
                    return -1;
                }
                
                for (size_t i = 0; i < payload_bytes; i++) {
                    unsigned int byte;
                    if (sscanf(&line[128 + i * 2], "%2x", &byte) != 1) {
                        fprintf(stderr, "Error: Invalid payload hex at packet %u\n", packets_processed);
                        fclose(fp);
                        return -1;
                    }
                    payload_buffer[i] = (uint8_t)byte;
                }
                pkt->payload = payload_buffer;
            }
        } else {
            pkt->payload = NULL;
        }
        
        /* Process packet immediately (like real network handler) */
        accl_process_packet(node, pkt);
        
        /* No need to free - using static buffers */
        
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
/**
 * Handle SEND/RECV operation (Point-to-Point communication)
 * 
 * Purpose: Manages direct communication between two ranks
 * 
 * Protocol:
 * - Eager Protocol (≤32KB): Data sent directly in segments
 * - Rendezvous Protocol (>32KB): 3-phase handshake (address exchange, RDMA, completion)
 * 
 * State Tracking:
 * - Uses PendingMessage structure to track multi-segment transfers
 * - Identifies messages by (tag, peer_rank, is_send)
 * - Counts segments received until total_segments reached
 * 
 * Completion:
 * - Message is complete when received_segments >= expected_segments
 * - Reports total bytes transferred
 */
void accl_handle_send_recv(ACCLNodeState *node, ACCLPacket *packet) {
    /* Determine if this node is the sender or receiver */
    bool is_sender = (packet->header.src_rank == node->rank);
    bool is_receiver = (packet->header.dst_rank == node->rank);
    
    /* Skip packets not relevant to this node */
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
 * Handle BROADCAST operation
 * 
 * Purpose: Root rank sends identical data to all other ranks
 * 
 * Topology: Tree-based (binary tree) or flat (direct) depending on number of ranks
 * 
 * Communication Pattern:
 * - Root → All non-root ranks (one-to-all)
 * - All ranks receive the same data
 * 
 * State Tracking:
 * - Uses CollectiveOpState identified by session_id
 * - Tracks cumulative bytes sent by root to all destinations
 * - Counts total segments across all receivers
 * 
 * Completion:
 * - Operation complete when all segments sent/received by all participants
 * - Root: when all data sent to all ranks
 * - Non-root: when all data received from root
 */
void accl_handle_broadcast(ACCLNodeState *node, ACCLPacket *packet) {
    /* Find or create collective operation state (identified by session_id) */
    CollectiveOpState *op = accl_find_or_create_collective_op(node, packet->header.session_id, OP_BROADCAST);
    
    if (op) {
        /* Initialize operation state on first packet */
        if (op->total_segments == 0) {
            op->total_segments = packet->header.total_segments;
            op->root_rank = packet->header.src_rank;  /* Identify root from source */
        }
        
        /* Handle RNDZV_DATA packets differently - they represent RDMA transfers */
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
 * Use Case: Distributing work/data across parallel processes
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
 * Handle REDUCE operation
 * 
 * Purpose: Combine data from all ranks using a reduction operation (sum, max, min, etc.)
 * 
 * Topology: Ring-based with in-network reduction
 * 
 * Communication Pattern:
 * - Similar to gather, but data is combined (reduced) at each step
 * - Rank 0 → Rank 1: Rank 1 combines data
 * - Rank 1 → Rank 2: Rank 2 combines with partial result
 * - ... → Root: Root gets final reduced result
 * 
 * Reduction Operations (specified in tag field bits 16-31):
 * - 0: SUM, 1: MAX, 2: MIN, 3: PROD, 4: LAND, 5: LOR
 * 
 * Node Roles:
 * - Root: Receives final reduced result in SoC memory
 * - Intermediate nodes: Combine received data with local data in SoC memory, forward result
 * 
 * SoC Memory Usage:
 * - Allocates buffer on first packet to store accumulated reduction result
 * - Performs in-memory reduction as data arrives
 * - Buffer persists until operation completes
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
 * Use Case: Common in parallel computing (e.g., gradient averaging in distributed ML)
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
 * Use Case: Common in parallel algorithms where each process needs a portion
 *           of the global reduction result
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
 * 
 * Use Case: Synchronizing parallel computation phases
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
 * Use Case: Matrix transpose, FFT, data redistribution in parallel algorithms
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
    printf("  Address=0x%016llX\n", (unsigned long long)packet->header.address);
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
    printf("Total Packets: %llu\n", (unsigned long long)stats->total_packets);
    printf("Total Data: %llu bytes (%.2f MB)\n", 
           (unsigned long long)stats->total_data_bytes, stats->total_data_bytes / (1024.0 * 1024.0));
    printf("  Bytes Sent:     %llu bytes (%.2f MB)\n",
           (unsigned long long)stats->bytes_sent, stats->bytes_sent / (1024.0 * 1024.0));
    printf("  Bytes Received: %llu bytes (%.2f MB)\n",
           (unsigned long long)stats->bytes_received, stats->bytes_received / (1024.0 * 1024.0));
    printf("\n");
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
    printf("By Packet Type:\n");
    printf("  Eager:           %10llu\n", (unsigned long long)stats->eager_packets);
    printf("  Rendezvous Addr: %10llu\n", (unsigned long long)stats->rendezvous_addr_packets);
    printf("  Rendezvous Data: %10llu\n", (unsigned long long)stats->rendezvous_data_packets);
    printf("  Rendezvous Done: %10llu\n", (unsigned long long)stats->rendezvous_complete_packets);
    printf("  Collective:      %10llu\n", (unsigned long long)stats->collective_packets);
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
