/**
 * sparse_batch.h
 *
 * Batch encoding for multiple sparse vectors
 * Amortizes rANS overhead across multiple vectors
 */

#ifndef SPARSE_BATCH_H
#define SPARSE_BATCH_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data;
    size_t size;
    uint16_t num_vectors;  /* Number of vectors in batch */
    uint16_t dimension;    /* Dimension of each vector */
} sparse_batch_t;

/**
 * Encode multiple vectors into a single batch
 * All vectors must have the same dimension
 *
 * @param vectors Array of vector pointers
 * @param num_vectors Number of vectors in the array
 * @param dimension Dimension of each vector
 * @return Encoded batch, or NULL on error
 */
sparse_batch_t* sparse_batch_encode(const int8_t **vectors,
                                     uint16_t num_vectors,
                                     size_t dimension);

/**
 * Decode a batch into multiple vectors
 *
 * @param encoded The encoded batch
 * @param vectors Output array (must be pre-allocated: vectors[num_vectors][dimension])
 * @param num_vectors Number of vectors expected
 * @param dimension Dimension of each vector
 * @return 0 on success, -1 on error
 */
int sparse_batch_decode(const sparse_batch_t *encoded,
                        int8_t **vectors,
                        uint16_t num_vectors,
                        size_t dimension);

/**
 * Free an encoded batch
 */
void sparse_batch_free(sparse_batch_t *encoded);

#endif /* SPARSE_BATCH_H */
