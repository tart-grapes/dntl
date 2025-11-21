/**
 * sparse_rice.h
 *
 * Optimal sparse vector encoder using gap + Rice coding
 *
 * Achieves ~110 bytes for k=145 (vs 238 bytes with fixed-width)
 *
 * Format: [count:16] [Rice-coded gaps] [Huffman-coded values]
 */

#ifndef SPARSE_RICE_H
#define SPARSE_RICE_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data;
    size_t size;
    uint16_t count;
} sparse_rice_t;

/**
 * Encode sparse vector using Rice codes for gaps + Huffman for values
 */
sparse_rice_t* sparse_rice_encode(const int8_t *vector, size_t dimension);

/**
 * Decode Rice-encoded sparse vector
 */
int sparse_rice_decode(const sparse_rice_t *encoded, int8_t *vector, size_t dimension);

/**
 * Free encoded result
 */
void sparse_rice_free(sparse_rice_t *encoded);

#endif /* SPARSE_RICE_H */
