/**
 * @file accl_byteorder.h
 * @brief Byte Order Conversion Functions
 * 
 * Provides byte order conversion functions for network-to-host
 * and host-to-network conversions (big-endian <-> little-endian).
 */

#ifndef ACCL_BYTEORDER_H
#define ACCL_BYTEORDER_H

#include <stdint.h>
#include "accl_types.h"

/* ============================================================================
 * Endianness Detection
 * ============================================================================ */

/**
 * Check if system is little-endian
 * @return 1 if little-endian, 0 if big-endian
 */
int is_little_endian(void);

/* ============================================================================
 * Network to Host Conversions
 * ============================================================================ */

/**
 * Convert 16-bit value from network (big-endian) to host byte order
 */
uint16_t bm_ntohs(uint16_t netshort);

/**
 * Convert 32-bit value from network (big-endian) to host byte order
 */
uint32_t bm_ntohl(uint32_t netlong);

/**
 * Convert 64-bit value from big-endian to host byte order
 */
uint64_t bm_be64toh(uint64_t big_endian);

/* ============================================================================
 * Host to Network Conversions
 * ============================================================================ */

/**
 * Convert 16-bit value from host to network (big-endian) byte order
 */
uint16_t bm_htons(uint16_t hostshort);

/**
 * Convert 32-bit value from host to network (big-endian) byte order
 */
uint32_t bm_htonl(uint32_t hostlong);

/**
 * Convert 64-bit value from host to big-endian byte order
 */
uint64_t bm_htobe64(uint64_t host);

/* ============================================================================
 * Packet Header Conversion
 * ============================================================================ */

/**
 * Convert ACCL packet header from network byte order to host byte order
 * @param header Pointer to packet header to convert in-place
 */
void accl_ntoh_packet_header(ACCLPacketHeader *header);

/* ============================================================================
 * Compatibility Macros
 * ============================================================================ */

#define ntohl bm_ntohl
#define ntohs bm_ntohs
#define htonl bm_htonl
#define htons bm_htons
#define be64toh bm_be64toh
#define htobe64 bm_htobe64

#endif /* ACCL_BYTEORDER_H */
