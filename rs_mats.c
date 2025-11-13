#include "rs_mats.h"
#include "rs_prf.h"
#include <string.h>
#include <stdlib.h>

// ============================================================================
// MATRIX DERIVATION
// ============================================================================

void rs_derive_A(const rs_params_t *p,
                 rs_family_t family,
                 int ell,
                 int slot,
                 rs_matrix_t *A_out) {
    // Select key, seed, and label based on family
    const uint8_t *key;
    const uint8_t *seed;
    const char *label;

    switch (family) {
        case RS_FAMILY_AX:
            key = p->key_ax;
            seed = p->seed_ax;
            label = "AX_A";
            break;
        case RS_FAMILY_AY:
            key = p->key_ay;
            seed = p->seed_ay;
            label = "AY_A";
            break;
        case RS_FAMILY_AOX:
            key = p->key_orb_x;
            seed = p->seed_orb_x;
            label = "AOX_A";
            break;
        case RS_FAMILY_AOY:
            key = p->key_orb_y;
            seed = p->seed_orb_y;
            label = "AOY_A";
            break;
        default:
            // Invalid family
            return;
    }

    // Derive nonce from seed, label, ell, slot
    uint8_t nonce[RS_NONCE_BYTES];
    rs_derive_nonce_16(seed, label, ell, slot, nonce);

    // Generate N×N×4 bytes using AES-256-CTR
    size_t out_len = (size_t)RS_N * RS_N * 4;
    uint8_t *buf = malloc(out_len);
    if (!buf) return;

    rs_prf_aes256_ctr(key, nonce, 0, buf, out_len);

    // Convert bytes to matrix with modulus q
    uint32_t q = RS_Q_LAYERS[ell];
    size_t idx = 0;

    for (int i = 0; i < RS_N; i++) {
        for (int j = 0; j < RS_N; j++) {
            uint32_t v;
            memcpy(&v, &buf[idx], 4);  // Little-endian 32-bit read
            idx += 4;
            A_out->data[i][j] = v % q;
        }
    }

    free(buf);
}

// ============================================================================
// ROW DERIVATION
// ============================================================================

void rs_derive_B_row(const rs_params_t *p,
                     int row_idx,
                     rs_flavor_t flavor,
                     rs_row_t *row_out) {
    // Use B key and seed
    const uint8_t *key = p->key_B;
    const uint8_t *seed = p->seed_B;
    const char *label = "B_ROW";

    // Flavor provides domain separation via second index
    int flavor_id = (int)flavor;

    // Derive nonce
    uint8_t nonce[RS_NONCE_BYTES];
    rs_derive_nonce_16(seed, label, row_idx, flavor_id, nonce);

    // Generate SECRET_DIM × 4 bytes
    size_t out_len = (size_t)RS_SECRET_DIM * 4;
    uint8_t *buf = malloc(out_len);
    if (!buf) return;

    rs_prf_aes256_ctr(key, nonce, 0, buf, out_len);

    // Convert bytes to row (no modulus reduction - full ℤ_{2^32})
    size_t idx = 0;
    for (int j = 0; j < RS_SECRET_DIM; j++) {
        uint32_t v;
        memcpy(&v, &buf[idx], 4);
        idx += 4;
        row_out->data[j] = v;
    }

    free(buf);
}

void rs_derive_C_row(const rs_params_t *p,
                     int row_idx,
                     rs_row_t *row_out) {
    // Use C key and seed
    const uint8_t *key = p->key_C;
    const uint8_t *seed = p->seed_C;
    const char *label = "C_ROW";

    // Derive nonce (second index = 0 for C rows)
    uint8_t nonce[RS_NONCE_BYTES];
    rs_derive_nonce_16(seed, label, row_idx, 0, nonce);

    // Generate SECRET_DIM × 4 bytes
    size_t out_len = (size_t)RS_SECRET_DIM * 4;
    uint8_t *buf = malloc(out_len);
    if (!buf) return;

    rs_prf_aes256_ctr(key, nonce, 0, buf, out_len);

    // Convert bytes to row
    size_t idx = 0;
    for (int j = 0; j < RS_SECRET_DIM; j++) {
        uint32_t v;
        memcpy(&v, &buf[idx], 4);
        idx += 4;
        row_out->data[j] = v;
    }

    free(buf);
}
