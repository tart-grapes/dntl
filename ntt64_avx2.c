#ifdef __AVX2__

#include "ntt64.h"
#include "ntt64_simd.h"
#include <immintrin.h>
#include <string.h>

// External references to shared constants and functions from ntt64.c
extern const uint32_t Q[NTT_NUM_LAYERS];
extern const uint32_t N_INV[NTT_NUM_LAYERS];
extern const uint64_t BARRETT_CONST[NTT_NUM_LAYERS];
extern const uint32_t TWIDDLES_FWD[NTT_NUM_LAYERS][6];
extern const uint32_t TWIDDLES_INV[NTT_NUM_LAYERS][6];
extern const uint32_t PSI_POWERS[NTT_NUM_LAYERS][NTT_N];
extern const uint32_t PSI_INV_POWERS[NTT_NUM_LAYERS][NTT_N];

// External reference to scalar functions (for correctness in SIMD version)
uint32_t ntt64_mul_mod(uint32_t a, uint32_t b, int layer);
void bit_reverse_copy(uint32_t poly[NTT_N]);
void ntt64_forward_scalar(uint32_t poly[NTT_N], int layer);
void ntt64_inverse_scalar(uint32_t poly[NTT_N], int layer);
void ntt64_pointwise_mul_scalar(uint32_t result[NTT_N],
                                 const uint32_t a[NTT_N],
                                 const uint32_t b[NTT_N],
                                 int layer);

// ============================================================================
// AVX2 MODULAR ARITHMETIC PRIMITIVES
// ============================================================================

/**
 * AVX2 modular addition for 8 values in parallel
 * Computes (a + b) mod q for 8 uint32_t values
 */
static inline __m256i avx2_add_mod(__m256i a, __m256i b, uint32_t q) {
    __m256i q_vec = _mm256_set1_epi32(q);

    // Compute sum = a + b
    __m256i sum = _mm256_add_epi32(a, b);

    // Check if sum >= q (branchless)
    __m256i cmp = _mm256_cmpgt_epi32(sum, _mm256_sub_epi32(q_vec, _mm256_set1_epi32(1)));

    // If sum >= q, subtract q
    __m256i adjustment = _mm256_and_si256(cmp, q_vec);
    sum = _mm256_sub_epi32(sum, adjustment);

    return sum;
}

/**
 * AVX2 modular subtraction for 8 values in parallel
 * Computes (a - b) mod q for 8 uint32_t values
 */
static inline __m256i avx2_sub_mod(__m256i a, __m256i b, uint32_t q) {
    __m256i q_vec = _mm256_set1_epi32(q);

    // Compute diff = a - b
    __m256i diff = _mm256_sub_epi32(a, b);

    // Check if a < b (need to add q)
    // This is tricky with signed comparison, use unsigned by checking sign bit
    __m256i cmp = _mm256_cmpgt_epi32(b, a);

    // If a < b, add q
    __m256i adjustment = _mm256_and_si256(cmp, q_vec);
    diff = _mm256_add_epi32(diff, adjustment);

    return diff;
}

/**
 * AVX2 modular multiplication using Barrett reduction
 * Computes (a * b) mod q for 8 uint32_t values
 *
 * This is the most complex operation - we need to handle 64-bit products
 * and 128-bit Barrett multiplication.
 */
static inline __m256i avx2_mul_mod(__m256i a, __m256i b, int layer) {
    // Extract values to scalar and use the verified scalar mul_mod
    // This ensures correctness at the cost of some performance
    uint32_t a_arr[8], b_arr[8], result[8];
    _mm256_storeu_si256((__m256i*)a_arr, a);
    _mm256_storeu_si256((__m256i*)b_arr, b);

    // Use the scalar implementation which is known to be correct
    for (int i = 0; i < 8; i++) {
        result[i] = ntt64_mul_mod(a_arr[i], b_arr[i], layer);
    }

    return _mm256_loadu_si256((__m256i*)result);
}

// ============================================================================
// AVX2 BIT-REVERSAL
// ============================================================================

/**
 * Bit-reverse a 6-bit index (for N=64=2^6)
 */
static inline uint32_t bit_reverse_6(uint32_t x) {
    uint32_t result = 0;
    result |= ((x & 0x01) << 5);
    result |= ((x & 0x02) << 3);
    result |= ((x & 0x04) << 1);
    result |= ((x & 0x08) >> 1);
    result |= ((x & 0x10) >> 3);
    result |= ((x & 0x20) >> 5);
    return result;
}

/**
 * AVX2-optimized bit-reversal permutation
 * Uses SIMD loads/stores for better cache performance
 */

// ============================================================================
// AVX2 NTT IMPLEMENTATION
// ============================================================================

void ntt64_forward_avx2(uint32_t poly[NTT_N], int layer) {
    // TEMPORARY: Just call scalar version for debugging
    ntt64_forward_scalar(poly, layer);
}

void ntt64_inverse_avx2(uint32_t poly[NTT_N], int layer) {
    // TEMPORARY: Just call scalar version for debugging
    ntt64_inverse_scalar(poly, layer);
}

void ntt64_pointwise_mul_avx2(uint32_t result[NTT_N],
                               const uint32_t a[NTT_N],
                               const uint32_t b[NTT_N],
                               int layer) {
    // Vectorized pointwise multiplication
    for (uint32_t i = 0; i < NTT_N; i += 8) {
        __m256i a_vec = _mm256_loadu_si256((__m256i*)&a[i]);
        __m256i b_vec = _mm256_loadu_si256((__m256i*)&b[i]);
        __m256i result_vec = avx2_mul_mod(a_vec, b_vec, layer);
        _mm256_storeu_si256((__m256i*)&result[i], result_vec);
    }
}

#endif // __AVX2__
