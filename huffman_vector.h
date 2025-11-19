/**
 * huffman_vector.h
 *
 * Huffman encoding for dense vectors with small alphabets
 * Optimized for values in range 0-255
 *
 * Usage:
 *   // Encode
 *   encoded_result_t *enc = huffman_encode(vector, 2048);
 *   // Use enc->data and enc->size
 *   // Then optionally compress with zstd/brotli
 *   huffman_free(enc);
 *
 *   // Decode
 *   uint8_t *decoded = malloc(2048);
 *   huffman_decode(enc->data, enc->size, decoded, 2048);
 */

#ifndef HUFFMAN_VECTOR_H
#define HUFFMAN_VECTOR_H

#include <stdint.h>
#include <stddef.h>

/* Encoded result structure */
typedef struct {
    uint8_t *data;      /* Encoded bytes (malloc'd, caller must free) */
    size_t size;        /* Size in bytes */
} encoded_result_t;

/**
 * Encode a vector using Huffman coding
 *
 * @param vector Input vector (values 0-255)
 * @param dimension Vector dimension
 * @return Encoded result (caller must free with huffman_free)
 *
 * Returns NULL on error
 */
encoded_result_t* huffman_encode(const uint8_t *vector, size_t dimension);

/**
 * Decode Huffman-encoded data back to vector
 *
 * @param encoded_data Encoded bytes
 * @param encoded_size Size of encoded data
 * @param vector Output vector (must be pre-allocated)
 * @param dimension Expected vector dimension
 * @return 0 on success, -1 on error
 */
int huffman_decode(const uint8_t *encoded_data, size_t encoded_size,
                   uint8_t *vector, size_t dimension);

/**
 * Free encoded result
 */
void huffman_free(encoded_result_t *result);

#endif /* HUFFMAN_VECTOR_H */
