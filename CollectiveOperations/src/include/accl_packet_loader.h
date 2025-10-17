/**
 * @file accl_packet_loader.h
 * @brief Packet Loading Module
 * 
 * Functions for loading ACCL packets from binary and hex format files.
 */

#ifndef ACCL_PACKET_LOADER_H
#define ACCL_PACKET_LOADER_H

#include "accl_types.h"

/* ============================================================================
 * Packet Loading Functions
 * ============================================================================ */

/**
 * Load packets from binary file format
 * Processes packets in binary format and calls packet processor for each
 * 
 * @param node Pointer to node state
 * @param filename Path to binary packet trace file
 * @return 0 on success, -1 on error
 */
int accl_load_packets_from_binary(ACCLNodeState *node, const char *filename);

/**
 * Load packets from hex text file format
 * Processes packets in hex ASCII format and calls packet processor for each
 * 
 * @param node Pointer to node state
 * @param filename Path to hex packet trace file  
 * @return 0 on success, -1 on error
 */
int accl_load_packets_from_hex(ACCLNodeState *node, const char *filename);

#endif /* ACCL_PACKET_LOADER_H */
