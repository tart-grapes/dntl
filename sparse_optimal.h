/**
 * sparse_optimal.h
 *
 * Optimized sparse vector encoder for 256-byte target
 *
 * Target configuration:
 * - k = 145 non-zeros
 * - Distribution: 70% ±2, 30% ±1
 * - L2 norm: ~21.2
 * - Entropy: 1.88 bits/value
 * - Security score: 39.89
 * - Target size: ~236 bytes
 *
 * Encoding: Bit-packed positions + Huffman values
 */

#ifndef SPARSE_OPTIMAL_H
#define SPARSE_OPTIMAL_H

#include <stdint.h>
#include <stddef.h>

/**
 * Encoded sparse vector
 */
typedef struct {
    uint8_t *data;        /* Encoded data */
    size_t size;          /* Size in bytes */
    uint16_t count;       /* Number of non-zeros */
} sparse_encoded_t;

/**
 * Encode a sparse vector (values in {-2, -1, 0, +1, +2})
 *
 * @param vector Input vector (dimension elements)
 * @param dimension Vector dimension (must be <= 2048)
 * @return Encoded result, or NULL on error
 */
sparse_encoded_t* sparse_encode(const int8_t *vector, size_t dimension);

/**
 * Decode a sparse vector
 *
 * @param encoded Encoded data
 * @param vector Output vector (must have space for dimension elements)
 * @param dimension Expected dimension
 * @return 0 on success, -1 on error
 */
int sparse_decode(const sparse_encoded_t *encoded, int8_t *vector, size_t dimension);

/**
 * Free encoded result
 */
void sparse_free(sparse_encoded_t *encoded);

/**
 * Get statistics about encoded vector
 */
void sparse_stats(const int8_t *vector, size_t dimension,
                  uint16_t *k, float *l2_norm, float *entropy);

#endif /* SPARSE_OPTIMAL_H */
