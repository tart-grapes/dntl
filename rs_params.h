#ifndef RS_PARAMS_H
#define RS_PARAMS_H

#include "rs_config.h"

// ============================================================================
// PARAMETER STRUCTURE
// ============================================================================

/**
 * Ring-switching system parameters
 *
 * Stores six seed families and their derived AES-256 keys:
 * - AX:    A matrices for X coordinate
 * - AY:    A matrices for Y coordinate
 * - AOX:   A matrices for orbit X
 * - AOY:   A matrices for orbit Y
 * - B:     B rows for LWR mapping
 * - C:     C rows for exact gadget (optional)
 */
typedef struct {
    // Input seeds (32 bytes each)
    uint8_t seed_ax[RS_SEED_BYTES];
    uint8_t seed_ay[RS_SEED_BYTES];
    uint8_t seed_orb_x[RS_SEED_BYTES];
    uint8_t seed_orb_y[RS_SEED_BYTES];
    uint8_t seed_B[RS_SEED_BYTES];
    uint8_t seed_C[RS_SEED_BYTES];

    // Derived AES-256 keys (one per seed family)
    uint8_t key_ax[RS_KEY_BYTES];
    uint8_t key_ay[RS_KEY_BYTES];
    uint8_t key_orb_x[RS_KEY_BYTES];
    uint8_t key_orb_y[RS_KEY_BYTES];
    uint8_t key_B[RS_KEY_BYTES];
    uint8_t key_C[RS_KEY_BYTES];
} rs_params_t;

/**
 * Initialize parameters from six 32-byte seeds
 *
 * Copies seeds and derives AES-256 keys for each family using SHA3-256.
 *
 * @param p          Parameter structure to initialize
 * @param seed_ax    Seed for AX matrices
 * @param seed_ay    Seed for AY matrices
 * @param seed_orb_x Seed for orbit X matrices
 * @param seed_orb_y Seed for orbit Y matrices
 * @param seed_B     Seed for B rows
 * @param seed_C     Seed for C rows
 */
void rs_params_init(rs_params_t *p,
                    const uint8_t seed_ax[RS_SEED_BYTES],
                    const uint8_t seed_ay[RS_SEED_BYTES],
                    const uint8_t seed_orb_x[RS_SEED_BYTES],
                    const uint8_t seed_orb_y[RS_SEED_BYTES],
                    const uint8_t seed_B[RS_SEED_BYTES],
                    const uint8_t seed_C[RS_SEED_BYTES]);

#endif // RS_PARAMS_H
