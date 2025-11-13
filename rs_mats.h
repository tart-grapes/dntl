#ifndef RS_MATS_H
#define RS_MATS_H

#include "rs_params.h"

// ============================================================================
// MATRIX AND ROW TYPES
// ============================================================================

/**
 * N×N matrix over ℤ_q
 */
typedef struct {
    uint32_t data[RS_N][RS_N];
} rs_matrix_t;

/**
 * Row vector of length SECRET_DIM over ℤ_{2^32}
 */
typedef struct {
    uint32_t data[RS_SECRET_DIM];
} rs_row_t;

// ============================================================================
// FAMILIES AND FLAVORS
// ============================================================================

/**
 * Matrix families for A matrices
 */
typedef enum {
    RS_FAMILY_AX = 0,   // A matrices for X coordinate
    RS_FAMILY_AY = 1,   // A matrices for Y coordinate
    RS_FAMILY_AOX = 2,  // A matrices for orbit X
    RS_FAMILY_AOY = 3   // A matrices for orbit Y
} rs_family_t;

/**
 * Flavors for B rows (for domain separation)
 */
typedef enum {
    RS_FLAVOR_LWR     = 0,  // LWR public mapping
    RS_FLAVOR_TAGGED  = 1,  // Tagged variant
    RS_FLAVOR_PARTIAL = 2   // Partial variant
} rs_flavor_t;

// ============================================================================
// MATRIX DERIVATION
// ============================================================================

/**
 * Derive A matrix from seed
 *
 * Generates A[family][ell][slot] ∈ ℤ_q^{N×N} where q = RS_Q_LAYERS[ell].
 * Uses AES-256-CTR with domain-separated nonce.
 *
 * @param p      Parameter structure (contains seeds and keys)
 * @param family Matrix family (AX, AY, AOX, AOY)
 * @param ell    Layer index (0..RS_NUM_LAYERS-1)
 * @param slot   Slot index (0..RS_SLOT_COUNT-1)
 * @param A_out  Output matrix
 */
void rs_derive_A(const rs_params_t *p,
                 rs_family_t family,
                 int ell,
                 int slot,
                 rs_matrix_t *A_out);

// ============================================================================
// ROW DERIVATION
// ============================================================================

/**
 * Derive B row from seed
 *
 * Generates B[row_idx] ∈ ℤ_{2^32}^{SECRET_DIM}.
 * Flavor provides domain separation for different uses.
 *
 * @param p        Parameter structure
 * @param row_idx  Row index
 * @param flavor   Flavor for domain separation
 * @param row_out  Output row
 */
void rs_derive_B_row(const rs_params_t *p,
                     int row_idx,
                     rs_flavor_t flavor,
                     rs_row_t *row_out);

/**
 * Derive C row from seed
 *
 * Generates C[row_idx] ∈ ℤ_{2^32}^{SECRET_DIM}.
 * Used for exact gadget matrices.
 *
 * @param p        Parameter structure
 * @param row_idx  Row index
 * @param row_out  Output row
 */
void rs_derive_C_row(const rs_params_t *p,
                     int row_idx,
                     rs_row_t *row_out);

#endif // RS_MATS_H
