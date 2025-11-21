/**
 * vector_compress.h
 *
 * Complete vector compression: Huffman + zstd
 *
 * Achieves ~200-250 bytes for 2048-dim dense vectors
 *
 * Usage:
 *   // Compress
 *   compressed_vector_t *comp = vector_compress(data, 2048, COMPRESS_LEVEL_MAX);
 *   // comp->data and comp->size ready to store/transmit
 *
 *   // Decompress
 *   uint8_t *decompressed = malloc(2048);
 *   vector_decompress(comp->data, comp->size, decompressed, 2048);
 *
 *   // Cleanup
 *   vector_compress_free(comp);
 */

#ifndef VECTOR_COMPRESS_H
#define VECTOR_COMPRESS_H

#include <stdint.h>
#include <stddef.h>

/* Compression levels */
typedef enum {
    COMPRESS_LEVEL_FAST = 1,      /* zstd level 1 - fastest */
    COMPRESS_LEVEL_DEFAULT = 3,   /* zstd level 3 - balanced */
    COMPRESS_LEVEL_BEST = 19,     /* zstd level 19 - best compression */
    COMPRESS_LEVEL_MAX = 22       /* zstd level 22 - maximum (slow) */
} compress_level_t;

/* Compressed result */
typedef struct {
    uint8_t *data;      /* Compressed bytes (malloc'd) */
    size_t size;        /* Size in bytes */
} compressed_vector_t;

/**
 * Compress a vector using Huffman + zstd
 *
 * @param vector Input vector (values 0-255)
 * @param dimension Vector dimension
 * @param level Compression level (COMPRESS_LEVEL_*)
 * @return Compressed result (caller must free with vector_compress_free)
 *
 * Returns NULL on error
 */
compressed_vector_t* vector_compress(const uint8_t *vector, size_t dimension,
                                     compress_level_t level);

/**
 * Decompress vector back to original
 *
 * @param compressed_data Compressed bytes
 * @param compressed_size Size of compressed data
 * @param vector Output vector (must be pre-allocated)
 * @param dimension Expected vector dimension
 * @return 0 on success, -1 on error
 */
int vector_decompress(const uint8_t *compressed_data, size_t compressed_size,
                      uint8_t *vector, size_t dimension);

/**
 * Free compressed result
 */
void vector_compress_free(compressed_vector_t *result);

#endif /* VECTOR_COMPRESS_H */
