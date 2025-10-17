/**
 * @file accl_utils.h
 * @brief Baremetal-Compatible Utility Functions
 * 
 * Provides custom implementations of standard C library functions
 * for baremetal/embedded systems without standard library support.
 */

#ifndef ACCL_UTILS_H
#define ACCL_UTILS_H

#include <stddef.h>

/* ============================================================================
 * Memory Functions
 * ============================================================================ */

/**
 * Custom memset implementation for baremetal systems
 * Sets count bytes of dest to value
 */
void* bm_memset(void* dest, int value, size_t count);

/**
 * Custom memcpy implementation for baremetal systems
 * Copies count bytes from src to dest
 */
void* bm_memcpy(void* dest, const void* src, size_t count);

/* ============================================================================
 * String Functions
 * ============================================================================ */

/**
 * Custom strlen implementation for baremetal systems
 * Returns the length of str (excluding null terminator)
 */
size_t bm_strlen(const char* str);

/**
 * Custom strncmp implementation for baremetal systems
 * Compares at most n bytes of s1 and s2
 */
int bm_strncmp(const char* s1, const char* s2, size_t n);

/**
 * Custom strcspn implementation for baremetal systems
 * Returns length of initial segment of s not containing any character from reject
 */
size_t bm_strcspn(const char* s, const char* reject);

/* ============================================================================
 * Compatibility Macros
 * ============================================================================ */

/* Define macros to use baremetal functions by default */
#define memset bm_memset
#define memcpy bm_memcpy
#define strlen bm_strlen
#define strncmp bm_strncmp
#define strcspn bm_strcspn

#endif /* ACCL_UTILS_H */
