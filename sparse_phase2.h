/**
 * sparse_phase2.h
 *
 * Phase 2 optimization: Delta alphabet + ANS for values
 * - Rice coding for position gaps
 * - rANS (Asymmetric Numeral Systems) for values
 * - Delta-encoded alphabet
 * Target: ~162 bytes for 97 nz vectors (vs 167 baseline)
 */

#ifndef SPARSE_PHASE2_H
#define SPARSE_PHASE2_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data;
    size_t size;
    uint16_t count;
} sparse_phase2_t;

/**
 * Encode with delta-alphabet + rANS optimization
 * Target: ~162 bytes for 97 nz vectors
 */
sparse_phase2_t* sparse_phase2_encode(const int8_t *vector, size_t dimension);

/**
 * Decode phase2 vector
 */
int sparse_phase2_decode(const sparse_phase2_t *encoded,
                         int8_t *vector, size_t dimension);

/**
 * Free encoded result
 */
void sparse_phase2_free(sparse_phase2_t *encoded);

#endif /* SPARSE_PHASE2_H */
