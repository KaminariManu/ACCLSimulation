/**
 * @file accl_packet_loader.c
 * @brief Packet Loading Implementation
 * 
 * Implements loading of packets from binary and hex format files.
 */

#include "accl_packet_loader.h"
#include "accl_utils.h"
#include "accl_byteorder.h"
#include <stdlib.h>
#include <stdio.h>

/* Forward declarations */
void accl_process_packet(ACCLNodeState *node, ACCLPacket *packet);

/* ============================================================================
 * Packet Loading Functions
 * ============================================================================ */

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
