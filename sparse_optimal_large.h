/**
 * sparse_optimal_large.h
 *
 * Optimal sparse encoder for vectors with large value ranges
 * - Rice coding for position gaps
 * - Adaptive Huffman for values (handles any alphabet size)
 * - Optimized for sparse vectors with 40+ unique values
 */

#ifndef SPARSE_OPTIMAL_LARGE_H
#define SPARSE_OPTIMAL_LARGE_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data;
    size_t size;
    uint16_t count;
} sparse_optimal_large_t;

/**
 * Encode sparse vector with optimal compression
 * Handles any value range and alphabet size
 */
sparse_optimal_large_t* sparse_optimal_large_encode(const int8_t *vector, size_t dimension);

/**
 * Decode sparse vector
 */
int sparse_optimal_large_decode(const sparse_optimal_large_t *encoded,
                                int8_t *vector, size_t dimension);

/**
 * Free encoded result
 */
void sparse_optimal_large_free(sparse_optimal_large_t *encoded);

#endif /* SPARSE_OPTIMAL_LARGE_H */
