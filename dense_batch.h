/**
 * dense_batch.h
 *
 * Batch encoding for dense vectors with value concentration
 * Optimized for vectors where most values are small (like signatures)
 * - No position encoding (all positions have values)
 * - rANS exploits frequency distribution
 * - Shared frequency table across batch
 */

#ifndef DENSE_BATCH_H
#define DENSE_BATCH_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data;
    size_t size;
    uint16_t num_vectors;
    uint16_t dimension;
} dense_batch_t;

/**
 * Encode multiple dense vectors into a single batch
 * Optimized for vectors with concentrated value distributions
 * (e.g., 80% small values, 20% large values)
 *
 * @param vectors Array of vector pointers (uint16_t values)
 * @param num_vectors Number of vectors in the array
 * @param dimension Dimension of each vector
 * @param modulus Modulus for each coefficient (e.g., 1031 for s, 521 for w)
 * @return Encoded batch, or NULL on error
 */
dense_batch_t* dense_batch_encode(const uint16_t **vectors,
                                   uint16_t num_vectors,
                                   uint16_t dimension,
                                   uint16_t modulus);

/**
 * Decode a batch into multiple dense vectors
 *
 * @param encoded The encoded batch
 * @param vectors Output array (must be pre-allocated: vectors[num_vectors][dimension])
 * @param num_vectors Number of vectors expected
 * @param dimension Dimension of each vector
 * @return 0 on success, -1 on error
 */
int dense_batch_decode(const dense_batch_t *encoded,
                       uint16_t **vectors,
                       uint16_t num_vectors,
                       uint16_t dimension);

/**
 * Free an encoded batch
 */
void dense_batch_free(dense_batch_t *encoded);

#endif /* DENSE_BATCH_H */
