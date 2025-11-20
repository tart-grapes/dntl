/**
 * sparse_delta.h
 *
 * Delta-alphabet optimized sparse encoder
 * - Rice coding for position gaps
 * - Adaptive Huffman for values
 * - DELTA-ENCODED ALPHABET (saves 36 bytes vs full encoding)
 */

#ifndef SPARSE_DELTA_H
#define SPARSE_DELTA_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data;
    size_t size;
    uint16_t count;
} sparse_delta_t;

/**
 * Encode with delta-alphabet optimization
 * Target: ~173 bytes for 97 nz vectors (vs 209 baseline)
 */
sparse_delta_t* sparse_delta_encode(const int8_t *vector, size_t dimension);

/**
 * Decode delta-alphabet vector
 */
int sparse_delta_decode(const sparse_delta_t *encoded,
                        int8_t *vector, size_t dimension);

/**
 * Free encoded result
 */
void sparse_delta_free(sparse_delta_t *encoded);

#endif /* SPARSE_DELTA_H */
