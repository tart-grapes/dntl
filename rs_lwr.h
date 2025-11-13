#ifndef RS_LWR_H
#define RS_LWR_H

#include "rs_mats.h"

// ============================================================================
// LWR (LEARNING WITH ROUNDING) PUBLIC MAP
// ============================================================================

/**
 * Compute LWR tag: t = ⌊(B · s) / 2^shift⌋ mod p
 *
 * Given:
 * - B ∈ ℤ_{2^32}^{m×d} as array of m rows
 * - s ∈ ℤ^{d} secret vector
 *
 * Compute:
 * - For each row i: acc_i = Σⱼ B[i][j] · s[j] mod 2^32
 * - t[i] = ⌊acc_i / 2^shift⌋ mod p
 *
 * @param B_rows  Array of RS_PUBLIC_DIM row vectors
 * @param s       Secret vector of length RS_SECRET_DIM
 * @param t_out   Output tag of length RS_PUBLIC_DIM
 */
void rs_lwr_tag(const rs_row_t *B_rows,
                const int32_t *s,
                uint16_t t_out[RS_PUBLIC_DIM]);

/**
 * Generate a random small secret for testing
 *
 * Generates s[i] ∈ {-3, -2, -1, 0, 1, 2, 3} uniformly.
 *
 * @param s_out  Output secret vector of length RS_SECRET_DIM
 * @param seed   32-byte seed for deterministic generation
 */
void rs_generate_secret(int32_t s_out[RS_SECRET_DIM],
                        const uint8_t seed[32]);

#endif // RS_LWR_H
