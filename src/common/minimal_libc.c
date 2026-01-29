/**
 * @file minimal_libc.c
 * @brief Minimal libc implementations for CH32V003 to reduce code size
 *
 * These are space-optimized implementations of basic string functions
 * to avoid pulling in larger library implementations from picolibc.
 */

#include <stddef.h>
#include <stdint.h>

/**
 * Minimal memset implementation optimized for size
 */
void *memset(void *dest, int c, size_t n)
{
	uint8_t *d = (uint8_t *)dest;
	while (n--) {
		*d++ = (uint8_t)c;
	}
	return dest;
}

/**
 * Minimal memcpy implementation optimized for size
 */
void *memcpy(void *dest, const void *src, size_t n)
{
	uint8_t *d       = (uint8_t *)dest;
	const uint8_t *s = (const uint8_t *)src;
	while (n--) {
		*d++ = *s++;
	}
	return dest;
}

/**
 * Minimal strlen implementation optimized for size
 */
size_t strlen(const char *s)
{
	size_t len = 0;
	while (*s++) {
		len++;
	}
	return len;
}
