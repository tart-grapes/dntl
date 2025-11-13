#include "ntt64.h"
#include <string.h>

// ============================================================================
// MODULI AND PARAMETERS
// ============================================================================

// Prime moduli (all satisfy q ≡ 1 mod 128 for N=64 negacyclic NTT)
static const uint32_t Q[NTT_NUM_LAYERS] = {
    257u,        // 2^8 + 1
    3329u,       // Kyber-like
    12289u,      // 2^13 + 2^12 + 1
    40961u,      // 2^15 + 2^13 + 1
    65537u,      // 2^16 + 1 (Fermat prime)
    786433u,     // 2^19 + 2^18 + 1
    2013265921u  // 2^31 - 2^27 + 1
};

// Primitive 128th roots of unity (omega) for each modulus
// omega^128 ≡ 1 (mod q) and omega^64 ≡ -1 (mod q)
static const uint32_t OMEGA[NTT_NUM_LAYERS] = {
    9u,          // for q=257
    1915u,       // for q=3329
    12149u,      // for q=12289
    19734u,      // for q=40961
    13987u,      // for q=65537
    381732u,     // for q=786433
    397765732u   // for q=2013265921
};

// Inverse of omega for each modulus (omega_inv = omega^(-1) mod q)
static const uint32_t OMEGA_INV[NTT_NUM_LAYERS] = {
    200u,        // for q=257
    1010u,       // for q=3329
    790u,        // for q=12289
    36928u,      // for q=40961
    39504u,      // for q=65537
    559977u,     // for q=786433
    1856545343u  // for q=2013265921
};

// Inverse of N=64 for each modulus (N_inv = 64^(-1) mod q)
static const uint32_t N_INV[NTT_NUM_LAYERS] = {
    253u,        // for q=257
    3277u,       // for q=3329
    12097u,      // for q=12289
    40321u,      // for q=40961
    64513u,      // for q=65537
    774145u,     // for q=786433
    1981808641u  // for q=2013265921
};

// Barrett reduction constants: floor(2^64 / q)
static const uint64_t BARRETT_CONST[NTT_NUM_LAYERS] = {
    71777214294589695ULL,    // for q=257
    5541226816974932ULL,     // for q=3329
    1501077717772768ULL,     // for q=12289
    450348967889200ULL,      // for q=40961
    281470681808895ULL,      // for q=65537
    23456218233097ULL,       // for q=786433
    9162596893ULL            // for q=2013265921
};

// ============================================================================
// CONSTANT-TIME MODULAR ARITHMETIC
// ============================================================================

/**
 * Constant-time Barrett reduction
 * Returns x mod q
 */
static inline uint32_t ct_barrett_reduce(uint64_t x, int layer) {
    // Compute q * floor(x / q) approximately
    uint64_t q = Q[layer];
    uint64_t b = BARRETT_CONST[layer];

    // quotient approximation: floor(x * b / 2^64)
    uint64_t q_approx = ((__uint128_t)x * b) >> 64;

    // remainder: x - q * q_approx
    uint64_t r = x - q_approx * q;

    // Conditional subtraction (constant-time)
    // If r >= q, subtract q; otherwise keep r
    uint64_t mask = -(uint64_t)(r >= q);
    r -= mask & q;

    // One more reduction may be needed for very large inputs
    mask = -(uint64_t)(r >= q);
    r -= mask & q;

    return (uint32_t)r;
}

/**
 * Constant-time modular multiplication: (a * b) mod q
 */
static inline uint32_t ct_mul_mod(uint32_t a, uint32_t b, int layer) {
    uint64_t product = (uint64_t)a * (uint64_t)b;
    return ct_barrett_reduce(product, layer);
}

/**
 * Constant-time modular addition: (a + b) mod q
 */
static inline uint32_t ct_add_mod(uint32_t a, uint32_t b, int layer) {
    uint64_t sum = (uint64_t)a + (uint64_t)b;
    uint32_t q = Q[layer];

    // Conditional subtraction
    uint32_t mask = -(uint32_t)(sum >= q);
    sum -= mask & q;

    return (uint32_t)sum;
}

/**
 * Constant-time modular subtraction: (a - b) mod q
 */
static inline uint32_t ct_sub_mod(uint32_t a, uint32_t b, int layer) {
    uint32_t q = Q[layer];
    int64_t diff = (int64_t)a - (int64_t)b;

    // Conditional addition
    int64_t mask = -(int64_t)(diff < 0);
    diff += mask & q;

    return (uint32_t)diff;
}

/**
 * Constant-time modular exponentiation (for twiddle factor computation)
 * Computes base^exp mod q
 */
static uint32_t ct_pow_mod(uint32_t base, uint32_t exp, int layer) {
    uint32_t result = 1;
    uint32_t b = base;

    for (int i = 0; i < 32; i++) {
        // If bit i of exp is set, multiply result by b
        uint32_t bit = (exp >> i) & 1;
        uint32_t mask = -(uint32_t)bit;

        uint32_t new_result = ct_mul_mod(result, b, layer);
        result = (result & ~mask) | (new_result & mask);

        // Square b for next iteration
        b = ct_mul_mod(b, b, layer);
    }

    return result;
}

// ============================================================================
// BIT-REVERSAL
// ============================================================================

/**
 * Bit-reverse a 6-bit index (for N=64=2^6)
 */
static inline uint32_t bit_reverse_6(uint32_t x) {
    uint32_t result = 0;
    result |= ((x & 0x01) << 5);  // bit 0 -> bit 5
    result |= ((x & 0x02) << 3);  // bit 1 -> bit 4
    result |= ((x & 0x04) << 1);  // bit 2 -> bit 3
    result |= ((x & 0x08) >> 1);  // bit 3 -> bit 2
    result |= ((x & 0x10) >> 3);  // bit 4 -> bit 1
    result |= ((x & 0x20) >> 5);  // bit 5 -> bit 0
    return result;
}

/**
 * In-place bit-reversal permutation
 */
static void bit_reverse_copy(uint32_t poly[NTT_N]) {
    for (uint32_t i = 0; i < NTT_N; i++) {
        uint32_t j = bit_reverse_6(i);

        if (i < j) {
            uint32_t temp = poly[i];
            poly[i] = poly[j];
            poly[j] = temp;
        }
    }
}

// ============================================================================
// NTT BUTTERFLIES (CONSTANT-TIME)
// ============================================================================

/**
 * Cooley-Tukey butterfly (constant-time)
 *
 * (a, b) -> (a + w*b, a - w*b) mod q
 */
static inline void ct_butterfly(uint32_t *a, uint32_t *b, uint32_t w, int layer) {
    uint32_t t = ct_mul_mod(*b, w, layer);
    uint32_t u = *a;

    *a = ct_add_mod(u, t, layer);
    *b = ct_sub_mod(u, t, layer);
}

/**
 * Gentleman-Sande butterfly for inverse NTT (constant-time)
 *
 * (a, b) -> ((a + b), w*(a - b)) mod q
 */
static inline void ct_inv_butterfly(uint32_t *a, uint32_t *b, uint32_t w, int layer) {
    uint32_t u = *a;
    uint32_t v = *b;

    *a = ct_add_mod(u, v, layer);

    uint32_t diff = ct_sub_mod(u, v, layer);
    *b = ct_mul_mod(diff, w, layer);
}

// ============================================================================
// FORWARD NTT
// ============================================================================

void ntt64_forward(uint32_t poly[NTT_N], int layer) {
    uint32_t psi = OMEGA[layer];  // primitive 128th root of unity

    // For negacyclic NTT, use omega = psi^2 (primitive 64th root)
    uint32_t omega = ct_mul_mod(psi, psi, layer);

    // Preprocessing: multiply poly[i] by psi^i
    uint32_t psi_power = 1;
    for (uint32_t i = 0; i < NTT_N; i++) {
        poly[i] = ct_mul_mod(poly[i], psi_power, layer);
        psi_power = ct_mul_mod(psi_power, psi, layer);
    }

    // Bit-reverse input
    bit_reverse_copy(poly);

    // Standard Cooley-Tukey NTT (iterative, constant-time)
    // log2(64) = 6 stages
    for (uint32_t stage = 0; stage < 6; stage++) {
        uint32_t m = 1u << (stage + 1);        // Block size
        uint32_t m_half = 1u << stage;         // Half block size

        // Twiddle factor step size: omega^(64/m)
        uint32_t omega_step_exp = 64u >> (stage + 1);
        uint32_t omega_m = ct_pow_mod(omega, omega_step_exp, layer);

        // Process all blocks at this stage
        for (uint32_t k = 0; k < NTT_N; k += m) {
            uint32_t w = 1;

            for (uint32_t j = 0; j < m_half; j++) {
                uint32_t idx_a = k + j;
                uint32_t idx_b = k + j + m_half;

                ct_butterfly(&poly[idx_a], &poly[idx_b], w, layer);

                w = ct_mul_mod(w, omega_m, layer);
            }
        }
    }
}

// ============================================================================
// INVERSE NTT
// ============================================================================

void ntt64_inverse(uint32_t poly[NTT_N], int layer) {
    uint32_t psi_inv = OMEGA_INV[layer];  // inverse of psi

    // For negacyclic NTT, use omega = psi^2 (primitive 64th root)
    uint32_t omega_inv = ct_mul_mod(psi_inv, psi_inv, layer);

    // Standard Gentleman-Sande inverse NTT (iterative, constant-time)
    // log2(64) = 6 stages (reverse order compared to forward)
    for (int stage = 5; stage >= 0; stage--) {
        uint32_t m = 1u << (stage + 1);
        uint32_t m_half = 1u << stage;

        // Twiddle factor step size
        uint32_t omega_step_exp = 64u >> (stage + 1);
        uint32_t omega_m = ct_pow_mod(omega_inv, omega_step_exp, layer);

        // Process all blocks at this stage
        for (uint32_t k = 0; k < NTT_N; k += m) {
            uint32_t w = 1;

            for (uint32_t j = 0; j < m_half; j++) {
                uint32_t idx_a = k + j;
                uint32_t idx_b = k + j + m_half;

                ct_inv_butterfly(&poly[idx_a], &poly[idx_b], w, layer);

                w = ct_mul_mod(w, omega_m, layer);
            }
        }
    }

    // Bit-reverse output
    bit_reverse_copy(poly);

    // Multiply by N^(-1) to complete inverse transform
    uint32_t n_inv = N_INV[layer];
    for (uint32_t i = 0; i < NTT_N; i++) {
        poly[i] = ct_mul_mod(poly[i], n_inv, layer);
    }

    // Postprocessing: multiply poly[i] by psi^(-i)
    uint32_t psi_inv_power = 1;
    for (uint32_t i = 0; i < NTT_N; i++) {
        poly[i] = ct_mul_mod(poly[i], psi_inv_power, layer);
        psi_inv_power = ct_mul_mod(psi_inv_power, psi_inv, layer);
    }
}

// ============================================================================
// POINT-WISE MULTIPLICATION
// ============================================================================

void ntt64_pointwise_mul(uint32_t result[NTT_N],
                         const uint32_t a[NTT_N],
                         const uint32_t b[NTT_N],
                         int layer) {
    for (uint32_t i = 0; i < NTT_N; i++) {
        result[i] = ct_mul_mod(a[i], b[i], layer);
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

uint32_t ntt64_get_modulus(int layer) {
    return Q[layer];
}
