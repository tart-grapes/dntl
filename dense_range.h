/**
 * dense_range.h
 *
 * Range-based encoding for dense vectors with value concentration
 * Optimized for single vectors where most values are small
 * - Splits values into size classes
 * - Encodes small values with fewer bits
 * - No frequency table overhead
 */

#ifndef DENSE_RANGE_H
#define DENSE_RANGE_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data;
    size_t size;
    uint16_t dimension;
    uint16_t modulus;
} dense_range_t;

/**
 * Encode a single dense vector using range-based encoding
 * Optimized for vectors with concentrated value distributions
 *
 * @param vector Array of uint16_t values
 * @param dimension Number of coefficients
 * @param modulus Modulus for each coefficient (e.g., 1031 for s)
 * @return Encoded data, or NULL on error
 */
dense_range_t* dense_range_encode(const uint16_t *vector,
                                   uint16_t dimension,
                                   uint16_t modulus);

/**
 * Decode a range-encoded vector
 *
 * @param encoded The encoded data
 * @param vector Output array (must be pre-allocated)
 * @param dimension Dimension of the vector
 * @return 0 on success, -1 on error
 */
int dense_range_decode(const dense_range_t *encoded,
                       uint16_t *vector,
                       uint16_t dimension);

/**
 * Free encoded data
 */
void dense_range_free(dense_range_t *encoded);

#endif /* DENSE_RANGE_H */
