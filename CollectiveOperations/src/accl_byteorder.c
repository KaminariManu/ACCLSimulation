/**
 * @file accl_byteorder.c
 * @brief Byte Order Conversion Functions Implementation
 * 
 * Implementation of byte order conversion for network packet processing.
 */

#include "accl_byteorder.h"

/* ============================================================================
 * Endianness Detection
 * ============================================================================ */

/**
 * @brief Detects if the system uses little-endian byte order
 * 
 * This function determines the endianness of the current system by storing
 * a multi-byte value and checking which byte appears first in memory.
 * 
 * Little-endian systems (x86, x86-64, ARM in little-endian mode):
 *   Store the least significant byte first (0x67 at lowest address)
 * 
 * Big-endian systems (PowerPC, SPARC, ARM in big-endian mode):
 *   Store the most significant byte first (0x01 at lowest address)
 * 
 * Example for value 0x01234567:
 *   Little-endian memory layout: [67] [45] [23] [01]
 *   Big-endian memory layout:    [01] [23] [45] [67]
 * 
 * @return 1 if system is little-endian, 0 if big-endian
 */
int is_little_endian(void) {
    volatile uint32_t i = 0x01234567;  // Use volatile to prevent compiler optimization
    // Cast to uint8_t* to read the first byte in memory
    return (*((uint8_t*)(&i))) == 0x67;  // If first byte is 0x67, it's little-endian
}

/* ============================================================================
 * Network to Host Conversions
 * ============================================================================ */

/**
 * @brief Convert 16-bit value from network byte order to host byte order
 * 
 * Network protocols use big-endian byte order (most significant byte first).
 * This function converts from network format to the host system's native format.
 * 
 * Example conversion on little-endian system:
 *   Network (big-endian):  0x1234 = [12] [34]
 *   Host (little-endian):  0x1234 = [34] [12]
 *   
 * On big-endian systems, no conversion is needed (network order = host order).
 * 
 * @param netshort 16-bit value in network byte order
 * @return Value converted to host byte order
 */
uint16_t bm_ntohs(uint16_t netshort) {
    if (is_little_endian()) {
        // Swap the two bytes: [AB] -> [BA]
        return ((netshort & 0x00FF) << 8) |  // Move low byte to high position
               ((netshort & 0xFF00) >> 8);   // Move high byte to low position
    }
    // On big-endian systems, network order matches host order
    return netshort;
}

/**
 * @brief Convert 32-bit value from network byte order to host byte order
 * 
 * Converts a 32-bit integer from big-endian (network) to host byte order.
 * This is commonly used for packet header fields like packet_type, src_rank, etc.
 * 
 * Example conversion on little-endian system:
 *   Network (big-endian):  0x12345678 = [12] [34] [56] [78]
 *   Host (little-endian):  0x12345678 = [78] [56] [34] [12]
 * 
 * @param netlong 32-bit value in network byte order
 * @return Value converted to host byte order
 */
uint32_t bm_ntohl(uint32_t netlong) {
    if (is_little_endian()) {
        // Swap all four bytes: [ABCD] -> [DCBA]
        return ((netlong & 0x000000FF) << 24) |  // Byte 0 -> position 3
               ((netlong & 0x0000FF00) << 8)  |  // Byte 1 -> position 2
               ((netlong & 0x00FF0000) >> 8)  |  // Byte 2 -> position 1
               ((netlong & 0xFF000000) >> 24);   // Byte 3 -> position 0
    }
    return netlong;
}

/**
 * @brief Convert 64-bit value from big-endian to host byte order
 * 
 * Converts a 64-bit integer from big-endian format to the host's native format.
 * This is used for 64-bit address fields in packet headers (e.g., RDMA addresses).
 * 
 * Example conversion on little-endian system:
 *   Big-endian:  0x0123456789ABCDEF = [01][23][45][67][89][AB][CD][EF]
 *   Little-endian: 0x0123456789ABCDEF = [EF][CD][AB][89][67][45][23][01]
 * 
 * @param big_endian 64-bit value in big-endian byte order
 * @return Value converted to host byte order
 */
uint64_t bm_be64toh(uint64_t big_endian) {
    if (is_little_endian()) {
        // Swap all eight bytes: [ABCDEFGH] -> [HGFEDCBA]
        return ((big_endian & 0x00000000000000FFULL) << 56) |  // Byte 0 -> position 7
               ((big_endian & 0x000000000000FF00ULL) << 40) |  // Byte 1 -> position 6
               ((big_endian & 0x0000000000FF0000ULL) << 24) |  // Byte 2 -> position 5
               ((big_endian & 0x00000000FF000000ULL) << 8)  |  // Byte 3 -> position 4
               ((big_endian & 0x000000FF00000000ULL) >> 8)  |  // Byte 4 -> position 3
               ((big_endian & 0x0000FF0000000000ULL) >> 24) |  // Byte 5 -> position 2
               ((big_endian & 0x00FF000000000000ULL) >> 40) |  // Byte 6 -> position 1
               ((big_endian & 0xFF00000000000000ULL) >> 56);   // Byte 7 -> position 0
    }
    return big_endian;
}

/* ============================================================================
 * Host to Network Conversions
 * ============================================================================ */

/**
 * @brief Convert 16-bit value from host byte order to network byte order
 * 
 * Byte swapping is symmetric: swapping twice returns the original value.
 * Therefore, host-to-network conversion uses the same logic as network-to-host.
 * 
 * @param hostshort 16-bit value in host byte order
 * @return Value converted to network byte order (big-endian)
 */
uint16_t bm_htons(uint16_t hostshort) {
    return bm_ntohs(hostshort);  // Same operation: swap is reversible
}

/**
 * @brief Convert 32-bit value from host byte order to network byte order
 * 
 * @param hostlong 32-bit value in host byte order
 * @return Value converted to network byte order (big-endian)
 */
uint32_t bm_htonl(uint32_t hostlong) {
    return bm_ntohl(hostlong);  // Same operation: swap is reversible
}

/**
 * @brief Convert 64-bit value from host byte order to big-endian
 * 
 * @param host 64-bit value in host byte order
 * @return Value converted to big-endian byte order
 */
uint64_t bm_htobe64(uint64_t host) {
    return bm_be64toh(host);  // Same operation: swap is reversible
}

/* ============================================================================
 * Packet Header Conversion
 * ============================================================================ */

/**
 * @brief Convert entire packet header from network to host byte order
 * 
 * When a packet is received from the network, all multi-byte fields are in
 * big-endian (network) byte order. This function converts all fields in-place
 * to the host system's native byte order for proper interpretation.
 * 
 * This function must be called immediately after reading a packet header from
 * a binary file or network stream, before accessing any of its fields.
 * 
 * Fields converted:
 *   - 16-bit: protocol_number, version
 *   - 32-bit: packet_type, operation, src_rank, dst_rank, tag, session_id,
 *             sequence_number, data_length, timestamp, flags, payload_segment,
 *             total_segments, checksum
 *   - 64-bit: address (for RDMA operations)
 * 
 * @param header Pointer to packet header structure to convert (modified in-place)
 */
void accl_ntoh_packet_header(ACCLPacketHeader *header) {
    if (!header) return;  // Safety check for null pointer
    
    // Convert 16-bit fields (protocol identifier and version)
    header->protocol_number = ntohs(header->protocol_number);  // 0xACCE expected
    header->version = ntohs(header->version);                  // Protocol version
    
    // Convert 32-bit packet classification fields
    header->packet_type = ntohl(header->packet_type);          // EAGER, RNDZV, etc.
    header->operation = ntohl(header->operation);              // SEND, BROADCAST, etc.
    
    // Convert 32-bit routing fields
    header->src_rank = ntohl(header->src_rank);                // Source node rank
    header->dst_rank = ntohl(header->dst_rank);                // Destination rank
    header->tag = ntohl(header->tag);                          // Message matching tag
    
    // Convert 32-bit connection tracking fields
    header->session_id = ntohl(header->session_id);            // Unique session ID
    header->sequence_number = ntohl(header->sequence_number);  // Packet sequence
    
    // Convert 32-bit data description fields
    header->data_length = ntohl(header->data_length);          // Payload size
    header->timestamp = ntohl(header->timestamp);              // Simulation timestamp
    header->flags = ntohl(header->flags);                      // Various flags
    
    // Convert 64-bit RDMA address field
    header->address = be64toh(header->address);                // Memory address for RDMA
    
    // Convert 32-bit segmentation fields
    header->payload_segment = ntohl(header->payload_segment);  // Current segment number
    header->total_segments = ntohl(header->total_segments);    // Total segments in message
    
    // Convert 32-bit integrity field
    header->checksum = ntohl(header->checksum);                // Data integrity check
}
