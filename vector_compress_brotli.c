/**
 * vector_compress_brotli.c
 *
 * Vector compression implementation using Huffman + Brotli
 */

#include "vector_compress_brotli.h"
#include "huffman_vector.h"
#include <brotli/encode.h>
#include <brotli/decode.h>
#include <stdlib.h>
#include <string.h>

compressed_vector_brotli_t* vector_compress_brotli(const uint8_t *vector,
                                                    size_t dimension,
                                                    brotli_level_t level) {
    if (!vector || dimension == 0) return NULL;

    /* Step 1: Huffman encode */
    encoded_result_t *huffman_enc = huffman_encode(vector, dimension);
    if (!huffman_enc) return NULL;

    /* Step 2: Brotli compress */
    size_t max_compressed_size = BrotliEncoderMaxCompressedSize(huffman_enc->size);
    uint8_t *brotli_buffer = malloc(max_compressed_size);
    if (!brotli_buffer) {
        huffman_free(huffman_enc);
        return NULL;
    }

    size_t brotli_size = max_compressed_size;
    int result = BrotliEncoderCompress(
        (int)level,                     /* quality (0-11) */
        BROTLI_DEFAULT_WINDOW,          /* lgwin */
        BROTLI_DEFAULT_MODE,            /* mode */
        huffman_enc->size,              /* input size */
        huffman_enc->data,              /* input data */
        &brotli_size,                   /* output size (in/out) */
        brotli_buffer                   /* output buffer */
    );

    if (!result) {
        free(brotli_buffer);
        huffman_free(huffman_enc);
        return NULL;
    }

    /* Create result */
    compressed_vector_brotli_t *comp_result = malloc(sizeof(compressed_vector_brotli_t));
    if (!comp_result) {
        free(brotli_buffer);
        huffman_free(huffman_enc);
        return NULL;
    }

    /* Trim to actual size */
    comp_result->data = realloc(brotli_buffer, brotli_size);
    if (!comp_result->data) {
        comp_result->data = brotli_buffer;  /* Use original if realloc fails */
    }
    comp_result->size = brotli_size;

    huffman_free(huffman_enc);

    return comp_result;
}

int vector_decompress_brotli(const uint8_t *compressed_data,
                              size_t compressed_size,
                              uint8_t *vector,
                              size_t dimension) {
    if (!compressed_data || !vector || compressed_size == 0) return -1;

    /* Step 1: Allocate buffer for decompressed data */
    /* We don't know the exact size, so start with a reasonable estimate */
    size_t max_output_size = 10 * compressed_size;  /* Conservative estimate */
    uint8_t *huffman_buffer = malloc(max_output_size);
    if (!huffman_buffer) return -1;

    /* Step 2: Brotli decompress */
    size_t decompressed_size = max_output_size;

    BrotliDecoderResult result = BrotliDecoderDecompress(
        compressed_size,
        compressed_data,
        &decompressed_size,
        huffman_buffer
    );

    if (result != BROTLI_DECODER_RESULT_SUCCESS) {
        /* Try with larger buffer */
        free(huffman_buffer);
        max_output_size = 100 * compressed_size;
        if (max_output_size < 10000) max_output_size = 10000;
        huffman_buffer = malloc(max_output_size);
        if (!huffman_buffer) return -1;

        decompressed_size = max_output_size;
        result = BrotliDecoderDecompress(
            compressed_size,
            compressed_data,
            &decompressed_size,
            huffman_buffer
        );

        if (result != BROTLI_DECODER_RESULT_SUCCESS) {
            free(huffman_buffer);
            return -1;
        }
    }

    /* Step 3: Huffman decode */
    int decode_result = huffman_decode(huffman_buffer, decompressed_size, vector, dimension);

    free(huffman_buffer);

    return decode_result;
}

void vector_compress_brotli_free(compressed_vector_brotli_t *result) {
    if (!result) return;
    free(result->data);
    free(result);
}
