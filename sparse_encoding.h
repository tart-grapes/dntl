/**
 * sparse_encoding.h
 *
 * Sparse vector encoding for lattice-based cryptography
 * Optimized for vectors with restricted value sets and L2 norm constraints
 *
 * For value range [-2, -1, 0, 1, 2] in 2048 dimensions:
 * - COO format: ~2-4 bytes per non-zero
 * - Packed format: ~1.75 bytes per non-zero (optimal bit packing)
 */

#ifndef SPARSE_ENCODING_H
#define SPARSE_ENCODING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Configuration */
#define MAX_DIMENSION 2048
#define MAX_NONZEROS 2048

/* Encoding formats */
typedef enum {
    SPARSE_FORMAT_COO,      /* Coordinate list (index, value) pairs */
    SPARSE_FORMAT_PACKED,   /* Bit-packed indices and values */
} sparse_format_t;

/* Sparse vector representation (COO format) */
typedef struct {
    uint16_t dimension;      /* Vector dimension */
    uint16_t count;          /* Number of non-zeros */
    uint16_t *indices;       /* Non-zero indices */
    int8_t *values;          /* Non-zero values */
} sparse_vector_t;

/* Encoded sparse vector */
typedef struct {
    uint8_t *data;          /* Encoded bytes */
    size_t size;            /* Size in bytes */
    sparse_format_t format; /* Encoding format used */
} encoded_vector_t;

/* Statistics and analysis */
typedef struct {
    uint16_t dimension;
    uint16_t nonzero_count;
    float sparsity;
    float l2_norm;
    int8_t min_value;
    int8_t max_value;
    uint16_t unique_values;
    float avg_index_delta;
} sparsity_stats_t;


/* ========== Core API ========== */

/**
 * Allocate a new sparse vector
 */
sparse_vector_t* sparse_vector_new(uint16_t dimension, uint16_t max_nonzeros);

/**
 * Free a sparse vector
 */
void sparse_vector_free(sparse_vector_t *vec);

/**
 * Create sparse vector from dense array
 */
sparse_vector_t* sparse_from_dense(const int8_t *dense, uint16_t dimension);

/**
 * Convert sparse vector to dense array
 */
void sparse_to_dense(const sparse_vector_t *sparse, int8_t *dense);

/**
 * Encode sparse vector using COO format
 * Format: [count:2] [index:2, value:1]*
 * Size: 2 + 3*count bytes
 */
encoded_vector_t* encode_coo(const sparse_vector_t *vec);

/**
 * Decode COO format
 */
sparse_vector_t* decode_coo(const encoded_vector_t *encoded);

/**
 * Encode using bit-packed format for value range [-2, -1, 0, 1, 2]
 * Format: [count:2] [11-bit index, 3-bit value]*
 * Size: 2 + ceil(14*count/8) bytes ≈ 2 + 1.75*count bytes
 */
encoded_vector_t* encode_packed_small_range(const sparse_vector_t *vec);

/**
 * Decode bit-packed format
 */
sparse_vector_t* decode_packed_small_range(const encoded_vector_t *encoded, uint16_t dimension);

/**
 * Free encoded vector
 */
void encoded_vector_free(encoded_vector_t *encoded);


/* ========== Utility Functions ========== */

/**
 * Calculate L2 norm of sparse vector
 */
float sparse_l2_norm(const sparse_vector_t *vec);

/**
 * Analyze sparsity characteristics
 */
sparsity_stats_t sparse_analyze(const sparse_vector_t *vec);

/**
 * Recommend best encoding format
 */
sparse_format_t sparse_recommend_format(const sparse_vector_t *vec);

/**
 * Estimate encoded size without actually encoding
 */
size_t sparse_estimate_size(const sparse_vector_t *vec, sparse_format_t format);

/**
 * Calculate compression ratio
 */
float compression_ratio(uint16_t dimension, size_t encoded_size);


/* ========== Advanced Encoding ========== */

/**
 * Encode with run-length + delta encoding (for clustered sparsity)
 * Format: [count:2] [first_idx:2] [delta:1-2, value:1]*
 */
encoded_vector_t* encode_rle(const sparse_vector_t *vec);

/**
 * Decode RLE format
 */
sparse_vector_t* decode_rle(const encoded_vector_t *encoded, uint16_t dimension);


/* ========== Validation ========== */

/**
 * Verify decoded vector matches original (for testing)
 */
bool sparse_verify(const sparse_vector_t *original, const sparse_vector_t *decoded);

/**
 * Calculate maximum reconstruction error
 */
float sparse_reconstruction_error(const int8_t *original, const int8_t *decoded, uint16_t dimension);


#endif /* SPARSE_ENCODING_H */
