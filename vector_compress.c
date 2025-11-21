/**
 * vector_compress.c
 *
 * Vector compression implementation using Huffman + zstd
 */

#include "vector_compress.h"
#include "huffman_vector.h"
#include <zstd.h>
#include <stdlib.h>
#include <string.h>

compressed_vector_t* vector_compress(const uint8_t *vector, size_t dimension,
                                     compress_level_t level) {
    if (!vector || dimension == 0) return NULL;

    /* Step 1: Huffman encode */
    encoded_result_t *huffman_enc = huffman_encode(vector, dimension);
    if (!huffman_enc) return NULL;

    /* Step 2: zstd compress */
    size_t zstd_bound = ZSTD_compressBound(huffman_enc->size);
    uint8_t *zstd_buffer = malloc(zstd_bound);
    if (!zstd_buffer) {
        huffman_free(huffman_enc);
        return NULL;
    }

    size_t zstd_size = ZSTD_compress(zstd_buffer, zstd_bound,
                                      huffman_enc->data, huffman_enc->size,
                                      (int)level);

    if (ZSTD_isError(zstd_size)) {
        free(zstd_buffer);
        huffman_free(huffman_enc);
        return NULL;
    }

    /* Create result */
    compressed_vector_t *result = malloc(sizeof(compressed_vector_t));
    if (!result) {
        free(zstd_buffer);
        huffman_free(huffman_enc);
        return NULL;
    }

    /* Trim to actual size */
    result->data = realloc(zstd_buffer, zstd_size);
    if (!result->data) {
        result->data = zstd_buffer;  /* Use original if realloc fails */
    }
    result->size = zstd_size;

    huffman_free(huffman_enc);

    return result;
}

int vector_decompress(const uint8_t *compressed_data, size_t compressed_size,
                      uint8_t *vector, size_t dimension) {
    if (!compressed_data || !vector || compressed_size == 0) return -1;

    /* Step 1: Get decompressed size */
    unsigned long long decompressed_size = ZSTD_getFrameContentSize(compressed_data, compressed_size);
    if (decompressed_size == ZSTD_CONTENTSIZE_ERROR ||
        decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        return -1;
    }

    /* Step 2: Allocate buffer for Huffman-encoded data */
    uint8_t *huffman_buffer = malloc(decompressed_size);
    if (!huffman_buffer) return -1;

    /* Step 3: zstd decompress */
    size_t actual_size = ZSTD_decompress(huffman_buffer, decompressed_size,
                                          compressed_data, compressed_size);

    if (ZSTD_isError(actual_size)) {
        free(huffman_buffer);
        return -1;
    }

    /* Step 4: Huffman decode */
    int result = huffman_decode(huffman_buffer, actual_size, vector, dimension);

    free(huffman_buffer);

    return result;
}

void vector_compress_free(compressed_vector_t *result) {
    if (!result) return;
    free(result->data);
    free(result);
}
