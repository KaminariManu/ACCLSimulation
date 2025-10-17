/**
 * @file accl_utils.c
 * @brief Baremetal-Compatible Utility Functions Implementation
 * 
 * This file provides custom implementations of standard C library functions
 * that are suitable for bare-metal and embedded systems where the standard
 * library may not be available or desirable.
 * 
 * WHY CUSTOM IMPLEMENTATIONS?
 * ===========================
 * 
 * 1. PORTABILITY:
 *    - Works on systems without a C standard library
 *    - No dependencies on libc (suitable for firmware/bootloaders)
 *    - Cross-platform (Windows, Linux, macOS, embedded)
 * 
 * 2. PREDICTABILITY:
 *    - Known, simple implementations (no hidden optimizations)
 *    - Easier to debug and verify
 *    - Consistent behavior across all platforms
 * 
 * 3. SIZE CONSTRAINTS:
 *    - Minimal code size (important for embedded systems)
 *    - No bloat from unused library features
 *    - Can be optimized for specific use cases
 * 
 * 4. CONTROL:
 *    - Full understanding of implementation
 *    - Can be customized for specific hardware
 *    - No licensing concerns
 * 
 * FUNCTION CATEGORIES:
 * ====================
 * 
 * Memory Functions:
 *   - bm_memset(): Fill memory with a constant byte
 *   - bm_memcpy(): Copy memory block
 * 
 * String Functions:
 *   - bm_strlen(): Calculate string length
 *   - bm_strncmp(): Compare strings with length limit
 *   - bm_strcspn(): Get length of substring not containing reject chars
 * 
 * NAMING CONVENTION:
 * ==================
 * All functions are prefixed with "bm_" (bare-metal) to:
 * - Avoid naming conflicts with standard library
 * - Clearly indicate these are custom implementations
 * - Make it easy to identify usage throughout codebase
 */

#include "accl_utils.h"

/* ============================================================================
 * Memory Functions
 * ============================================================================ */

/**
 * @brief Fill a block of memory with a constant byte value
 * 
 * This is a bare-metal implementation of the standard C memset() function.
 * It sets each byte in a memory region to a specified value.
 * @param dest Pointer to memory block to fill
 * @param value Value to set (only lowest byte is used)
 * @param count Number of bytes to set
 * @return Pointer to the filled memory block (same as dest)
 */
void* bm_memset(void* dest, int value, size_t count) {
    unsigned char* d = (unsigned char*)dest;  // Cast for byte-by-byte access
    unsigned char val = (unsigned char)value; // Only use lowest 8 bits
    
    // Fill each byte with the value
    while (count--) {
        *d++ = val;  // Write value and advance pointer
    }
    
    return dest;  // Return original pointer (standard memset behavior)
}

/**
 * @brief Copy a block of memory from source to destination
 * 
 * This is a bare-metal implementation of the standard C memcpy() function.
 * It copies bytes from one memory location to another.
 * @param dest Pointer to destination memory block
 * @param src Pointer to source memory block
 * @param count Number of bytes to copy
 * @return Pointer to destination memory block (same as dest)
 */
void* bm_memcpy(void* dest, const void* src, size_t count) {
    unsigned char* d = (unsigned char*)dest;        // Destination as byte pointer
    const unsigned char* s = (const unsigned char*)src;  // Source as byte pointer
    
    // Copy each byte from source to destination
    while (count--) {
        *d++ = *s++;  // Copy byte and advance both pointers
    }
    
    return dest;  // Return original destination pointer
}

/* ============================================================================
 * String Functions
 * ============================================================================ */

/**
 * @brief Calculate the length of a null-terminated string
 * 
 * This is a bare-metal implementation of the standard C strlen() function.
 * It counts the number of characters in a string, not including the
 * terminating null character ('\0').
 * @param str Pointer to null-terminated string
 * @return Number of characters in string (excluding null terminator)
 */
size_t bm_strlen(const char* str) {
    const char* s = str;  // Keep original pointer for length calculation
    
    // Advance through string until null terminator found
    while (*s) {
        s++;  // Move to next character
    }
    
    // Calculate length as difference between pointers
    return (s - str);  // Pointer arithmetic gives number of characters
}

/**
 * @brief Compare two strings up to n characters
 * 
 * This is a bare-metal implementation of the standard C strncmp() function.
 * It compares two strings lexicographically, stopping after n characters
 * or when a null terminator or difference is found.
 * @param s1 Pointer to first null-terminated string
 * @param s2 Pointer to second null-terminated string
 * @param n Maximum number of characters to compare
 * @return 0 if equal, <0 if s1<s2, >0 if s1>s2
 */
int bm_strncmp(const char* s1, const char* s2, size_t n) {
    // Compare characters while:
    // - Characters remain to be compared (n > 0)
    // - Current character in s1 is not null
    // - Characters are equal
    while (n && *s1 && (*s1 == *s2)) {
        s1++;  // Advance to next character in s1
        s2++;  // Advance to next character in s2
        n--;   // Decrement remaining characters to compare
    }
    
    // If we compared all n characters, they're equal
    if (n == 0) {
        return 0;
    }
    
    // Return difference between first differing characters
    // Cast to unsigned char to ensure proper comparison
    // (avoids issues with sign extension)
    return (*(unsigned char*)s1 - *(unsigned char*)s2);
}

/**
 * @brief Calculate length of substring not containing reject characters
 * 
 * This is a bare-metal implementation of the standard C strcspn() function.
 * It returns the length of the initial segment of string s that consists
 * entirely of characters NOT in the reject string.
 * 
 * "cspn" stands for "complement span" - span of characters not in the set.
 * RELATIONSHIP TO OTHER FUNCTIONS:
 * --------------------------------
 * - strspn(): Returns length of segment containing ONLY reject chars
 * - strchr(): Finds first occurrence of single character
 * - strcspn(): Finds first occurrence of ANY character from a set
 * @param s Pointer to null-terminated string to search
 * @param reject Pointer to null-terminated string containing reject characters
 * @return Length of initial segment not containing any reject characters
 */
size_t bm_strcspn(const char* s, const char* reject) {
    size_t count = 0;  // Count of characters examined
    
    // Iterate through each character in string s
    while (*s) {
        const char* r = reject;  // Start at beginning of reject string
        
        // Check if current character matches any in reject string
        while (*r) {
            if (*s == *r) {
                // Found a match - return current position
                return count;
            }
            r++;  // Check next reject character
        }
        
        // Current character not in reject set - continue
        s++;      // Move to next character in s
        count++;  // Increment count of non-rejected characters
    }
    
    // Reached end of s without finding any reject characters
    return count;  // Return total length
}
