/**
 * vector_compress_brotli.h
 *
 * Complete vector compression: Huffman + Brotli
 *
 * Achieves ~200-250 bytes for 2048-dim dense vectors
 *
 * Usage:
 *   // Compress
 *   compressed_vector_t *comp = vector_compress_brotli(data, 2048, BROTLI_LEVEL_MAX);
 *   // comp->data and comp->size ready to store/transmit
 *
 *   // Decompress
 *   uint8_t *decompressed = malloc(2048);
 *   vector_decompress_brotli(comp->data, comp->size, decompressed, 2048);
 *
 *   // Cleanup
 *   vector_compress_brotli_free(comp);
 */

#ifndef VECTOR_COMPRESS_BROTLI_H
#define VECTOR_COMPRESS_BROTLI_H

#include <stdint.h>
#include <stddef.h>

/* Brotli compression levels (0-11) */
typedef enum {
    BROTLI_LEVEL_FAST = 1,      /* Level 1 - fastest */
    BROTLI_LEVEL_DEFAULT = 6,   /* Level 6 - balanced */
    BROTLI_LEVEL_BEST = 9,      /* Level 9 - very good compression */
    BROTLI_LEVEL_MAX = 11       /* Level 11 - maximum compression (slow) */
} brotli_level_t;

/* Compressed result */
typedef struct {
    uint8_t *data;      /* Compressed bytes (malloc'd) */
    size_t size;        /* Size in bytes */
} compressed_vector_brotli_t;

/**
 * Compress a vector using Huffman + Brotli
 *
 * @param vector Input vector (values 0-255)
 * @param dimension Vector dimension
 * @param level Compression level (BROTLI_LEVEL_*)
 * @return Compressed result (caller must free with vector_compress_brotli_free)
 *
 * Returns NULL on error
 */
compressed_vector_brotli_t* vector_compress_brotli(const uint8_t *vector,
                                                    size_t dimension,
                                                    brotli_level_t level);

/**
 * Decompress vector back to original
 *
 * @param compressed_data Compressed bytes
 * @param compressed_size Size of compressed data
 * @param vector Output vector (must be pre-allocated)
 * @param dimension Expected vector dimension
 * @return 0 on success, -1 on error
 */
int vector_decompress_brotli(const uint8_t *compressed_data,
                              size_t compressed_size,
                              uint8_t *vector,
                              size_t dimension);

/**
 * Free compressed result
 */
void vector_compress_brotli_free(compressed_vector_brotli_t *result);

#endif /* VECTOR_COMPRESS_BROTLI_H */
