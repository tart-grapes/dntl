/**
 * sparse_phase3.h
 *
 * Phase 3: Delta Alphabet + Adaptive Rice
 * - ADAPTIVE Rice coding for position gaps (context-aware parameter selection)
 * - Adaptive Huffman for values
 * - Delta-encoded alphabet (from Phase 1)
 * Target: ~158 bytes for 97 nz vectors (vs 167.5 Phase 1)
 */

#ifndef SPARSE_PHASE3_H
#define SPARSE_PHASE3_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data;
    size_t size;
    uint16_t count;
} sparse_phase3_t;

/**
 * Encode with Phase 3 adaptive Rice optimization
 * Target: ~158 bytes for 97 nz vectors
 */
sparse_phase3_t* sparse_phase3_encode(const int8_t *vector, size_t dimension);

/**
 * Decode Phase 3 vector
 */
int sparse_phase3_decode(const sparse_phase3_t *encoded,
                        int8_t *vector, size_t dimension);

/**
 * Free encoded result
 */
void sparse_phase3_free(sparse_phase3_t *encoded);

#endif /* SPARSE_PHASE3_H */
