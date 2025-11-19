/**
 * sparse_adaptive.h
 *
 * Adaptive sparse encoder optimized for Gaussian distributions
 * - Rice coding for position gaps
 * - Adaptive Huffman for values (learns from actual distribution)
 */

#ifndef SPARSE_ADAPTIVE_H
#define SPARSE_ADAPTIVE_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data;
    size_t size;
    uint16_t count;
} sparse_adaptive_t;

/**
 * Encode sparse vector with adaptive Huffman for values
 * Works for any value distribution (optimized for Gaussian)
 */
sparse_adaptive_t* sparse_adaptive_encode(const int8_t *vector, size_t dimension);

/**
 * Decode adaptive sparse vector
 */
int sparse_adaptive_decode(const sparse_adaptive_t *encoded, int8_t *vector, size_t dimension);

/**
 * Free encoded result
 */
void sparse_adaptive_free(sparse_adaptive_t *encoded);

#endif /* SPARSE_ADAPTIVE_H */
