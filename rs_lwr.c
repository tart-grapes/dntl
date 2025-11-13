#include "rs_lwr.h"
#include "rs_prf.h"
#include <stdint.h>
#include <stdlib.h>

// ============================================================================
// LWR TAG COMPUTATION
// ============================================================================

void rs_lwr_tag(const rs_row_t *B_rows,
                const int32_t *s,
                uint16_t t_out[RS_PUBLIC_DIM]) {
    for (int i = 0; i < RS_PUBLIC_DIM; i++) {
        // Compute inner product: B[i] · s
        uint64_t acc = 0;

        for (int j = 0; j < RS_SECRET_DIM; j++) {
            // B[i][j] is uint32_t, s[j] is int32_t
            // Product fits in int64_t, accumulate in uint64_t
            int64_t product = (int64_t)B_rows[i].data[j] * (int64_t)s[j];
            acc += (uint64_t)product;
        }

        // acc is now (B[i] · s) mod 2^64
        // We want it mod 2^32 first
        uint32_t u = (uint32_t)acc;  // Implicit mod 2^32

        // Truncate by shifting right: ⌊u / 2^shift⌋
        uint32_t v = u >> RS_LWR_SHIFT;

        // Final modulus: v mod p
        t_out[i] = (uint16_t)(v % RS_P_SMALL);
    }
}

// ============================================================================
// SECRET GENERATION FOR TESTING
// ============================================================================

void rs_generate_secret(int32_t s_out[RS_SECRET_DIM],
                        const uint8_t seed[32]) {
    // Generate deterministic bytes from seed
    uint8_t key[32];
    uint8_t nonce[16] = {0};  // Fixed nonce

    // Derive key from seed
    rs_derive_aes_key(seed, "SECRET", key);

    // Generate RS_SECRET_DIM bytes
    uint8_t *buf = malloc(RS_SECRET_DIM);
    if (!buf) return;

    rs_prf_aes256_ctr(key, nonce, 0, buf, RS_SECRET_DIM);

    // Map bytes to {-3, -2, -1, 0, 1, 2, 3}
    // Use modulo 7 to get 0..6, then subtract 3
    for (int i = 0; i < RS_SECRET_DIM; i++) {
        s_out[i] = (int32_t)(buf[i] % 7) - 3;
    }

    free(buf);
}
