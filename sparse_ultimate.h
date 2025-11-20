/**
 * sparse_ultimate.h
 *
 * Ultimate sparse encoder - full optimization for minimum size
 * Target: ~148 bytes for 97 nz vectors
 *
 * Optimizations:
 * 1. Delta-encoded alphabet (saves 36 bytes)
 * 2. ANS encoding for values (saves 5-10 bytes)
 * 3. Context-adaptive position coding (saves 10-15 bytes)
 * 4. Bit-level packing optimizations (saves 5-10 bytes)
 */

#ifndef SPARSE_ULTIMATE_H
#define SPARSE_ULTIMATE_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data;
    size_t size;
    uint16_t count;
} sparse_ultimate_t;

/**
 * Encode with ultimate compression
 * Target: ~148 bytes for 97 nz, 44 unique values
 */
sparse_ultimate_t* sparse_ultimate_encode(const int8_t *vector, size_t dimension);

/**
 * Decode ultimate compressed vector
 */
int sparse_ultimate_decode(const sparse_ultimate_t *encoded,
                           int8_t *vector, size_t dimension);

/**
 * Free encoded result
 */
void sparse_ultimate_free(sparse_ultimate_t *encoded);

#endif /* SPARSE_ULTIMATE_H */
