/**
 * @file accl_types.h
 * @brief ACCL Common Types and Structures
 * 
 * Defines all common types, enumerations, and data structures
 * used throughout the ACCL packet processor.
 */

#ifndef ACCL_TYPES_H
#define ACCL_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PROTOCOL_NUMBER 0xACCE
#define PROTOCOL_VERSION 0x01
#define DATAPATH_WIDTH_BYTES 64
#define MAX_PACKETSIZE 4096
#define MAX_EAGER_SIZE 32768

/* State tracking limits */
#define MAX_COLLECTIVE_OPS 128
#define MAX_PENDING_MSGS 64

/* SoC Memory limits */
#define SOC_MEMORY_SIZE (256 * 1024 * 1024)  /* 256 MB */
#define SOC_MAX_BUFFERS 128

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * Packet Types
 */
typedef enum {
    PTYPE_DATA_EAGER = 0,
    PTYPE_RNDZV_ADDR = 1,
    PTYPE_RNDZV_DATA = 2,
    PTYPE_RNDZV_COMPLETE = 3,
    PTYPE_COLLECTIVE_DATA = 4
} PacketType;

/**
 * Operation Types
 */
typedef enum {
    OP_SEND = 0,
    OP_RECV = 1,
    OP_BROADCAST = 2,
    OP_SCATTER = 3,
    OP_GATHER = 4,
    OP_REDUCE = 5,
    OP_ALLGATHER = 6,
    OP_ALLREDUCE = 7,
    OP_REDUCE_SCATTER = 8,
    OP_BARRIER = 9,
    OP_ALLTOALL = 10
} OperationType;

/* ============================================================================
 * Packet Structures
 * ============================================================================ */

/**
 * Packet Header Structure (64 bytes)
 * Packed to ensure consistent binary layout
 */
typedef struct __attribute__((packed)) {
    uint16_t protocol_number;    /* 0xACCE */
    uint16_t version;            /* 0x0001 */
    uint32_t packet_type;        /* PacketType */
    uint32_t operation;          /* OperationType */
    uint32_t src_rank;           /* Source rank */
    uint32_t dst_rank;           /* Destination rank */
    uint32_t tag;                /* Message tag */
    uint32_t session_id;         /* Session ID */
    uint32_t sequence_number;    /* Sequence number */
    uint32_t data_length;        /* Payload length in bytes */
    uint32_t timestamp;          /* Timestamp */
    uint32_t flags;              /* Flags (compression, host memory, etc.) */
    uint64_t address;            /* Address for rendezvous */
    uint32_t payload_segment;    /* Segment number */
    uint32_t total_segments;     /* Total segments */
    uint32_t checksum;           /* Checksum */
} ACCLPacketHeader;

/**
 * Complete Packet Structure
 */
typedef struct {
    ACCLPacketHeader header;
    uint8_t *payload;            /* Dynamically allocated payload */
} ACCLPacket;

/* ============================================================================
 * Statistics Structure
 * ============================================================================ */

/**
 * Statistics tracking for processed packets
 */
typedef struct {
    uint64_t total_packets;
    uint64_t send_recv_packets;
    uint64_t broadcast_packets;
    uint64_t scatter_packets;
    uint64_t gather_packets;
    uint64_t reduce_packets;
    uint64_t allgather_packets;
    uint64_t allreduce_packets;
    uint64_t reduce_scatter_packets;
    uint64_t barrier_packets;
    uint64_t alltoall_packets;
    
    uint64_t eager_packets;
    uint64_t rendezvous_addr_packets;
    uint64_t rendezvous_data_packets;
    uint64_t rendezvous_complete_packets;
    uint64_t collective_packets;
    
    uint64_t total_data_bytes;
    uint64_t bytes_sent;
    uint64_t bytes_received;
} ACCLStatistics;

/* ============================================================================
 * State Tracking Structures
 * ============================================================================ */

/**
 * Per-collective-operation state tracking
 */
typedef struct {
    uint32_t session_id;
    OperationType operation;
    uint32_t root_rank;         /* For rooted collectives */
    uint32_t total_segments;    /* Total segments expected */
    uint32_t segments_received; /* Segments received so far */
    uint64_t total_bytes;       /* Total bytes in operation */
    uint64_t bytes_processed;   /* Bytes processed so far */
    uint32_t ranks_participated;/* Bitmask or counter of ranks */
    bool in_progress;
    bool completed;
} CollectiveOpState;

/**
 * Pending message tracking for send/recv
 */
typedef struct {
    uint32_t tag;
    uint32_t peer_rank;
    uint32_t session_id;
    uint32_t expected_segments;
    uint32_t received_segments;
    uint64_t bytes;
    bool is_send;               /* true=send, false=recv */
    bool completed;
} PendingMessage;

/* ============================================================================
 * SoC Memory Structures
 * ============================================================================ */

/**
 * Memory buffer descriptor for SoC memory simulation
 */
typedef struct {
    uint32_t buffer_id;         /* Unique buffer identifier */
    uint32_t session_id;        /* Associated session ID */
    uint64_t address;           /* Simulated physical address */
    uint32_t size;              /* Buffer size in bytes */
    uint32_t used;              /* Bytes currently used */
    uint8_t *data;              /* Actual data storage */
    bool in_use;                /* Buffer allocation status */
    OperationType operation;    /* Associated operation type */
} SoCMemoryBuffer;

/**
 * SoC Memory Manager
 */
typedef struct {
    uint64_t total_memory;      /* Total memory available */
    uint64_t used_memory;       /* Currently allocated memory */
    uint64_t peak_memory;       /* Peak memory usage */
    uint32_t next_buffer_id;    /* Next buffer ID to allocate */
    uint64_t next_address;      /* Next physical address to allocate */
    SoCMemoryBuffer buffers[SOC_MAX_BUFFERS];
    uint32_t num_buffers;       /* Number of active buffers */
} SoCMemory;

/* ============================================================================
 * Node State Structure
 * ============================================================================ */

/**
 * Complete node state tracking
 */
typedef struct {
    uint32_t rank;
    uint32_t num_ranks;
    ACCLStatistics stats;
    
    /* State tracking for collective operations */
    CollectiveOpState collective_ops[MAX_COLLECTIVE_OPS];
    uint32_t num_collective_ops;
    
    /* State tracking for point-to-point messages */
    PendingMessage pending_msgs[MAX_PENDING_MSGS];
    uint32_t num_pending_msgs;
    
    /* Barrier synchronization state */
    uint32_t barrier_entered;   /* Count of ranks entered barrier */
    uint32_t barrier_exited;    /* Count of ranks exited barrier */
    
    /* SoC Memory Simulation */
    SoCMemory memory;           /* Simulated SoC memory system */
} ACCLNodeState;

#endif /* ACCL_TYPES_H */
