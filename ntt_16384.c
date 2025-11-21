/**
 * Full-Size 16384-Point NTT Implementation
 *
 * Implements a complete 16384-point Number Theoretic Transform
 * instead of using block-wise 64-point NTTs.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define N 16384  // Full NTT size: 2^14

// Modulus: q = 998244353 = 119 * 2^23 + 1
// This is a well-known NTT-friendly prime that supports up to 2^23-point NTT
// Commonly used in competitive programming and cryptography
#define Q 998244353u
#define PRIMITIVE_ROOT 3u  // Verified primitive root for this modulus

// ============================================================================
// MODULAR ARITHMETIC
// ============================================================================

/**
 * Modular addition
 */
static inline uint32_t add_mod(uint32_t a, uint32_t b) {
    uint64_t sum = (uint64_t)a + (uint64_t)b;
    return sum >= Q ? sum - Q : sum;
}

/**
 * Modular subtraction
 */
static inline uint32_t sub_mod(uint32_t a, uint32_t b) {
    return a >= b ? a - b : a + Q - b;
}

/**
 * Modular multiplication (simple division-based for correctness)
 */
static inline uint32_t mul_mod(uint32_t a, uint32_t b) {
    return ((uint64_t)a * (uint64_t)b) % Q;
}

/**
 * Modular exponentiation: base^exp mod Q
 */
static uint32_t pow_mod(uint32_t base, uint32_t exp) {
    uint32_t result = 1;
    base %= Q;
    while (exp > 0) {
        if (exp & 1) {
            result = mul_mod(result, base);
        }
        base = mul_mod(base, base);
        exp >>= 1;
    }
    return result;
}

// ============================================================================
// TWIDDLE FACTORS
// ============================================================================

static uint32_t twiddle_fwd[N];  // Forward NTT twiddle factors
static uint32_t twiddle_inv[N];  // Inverse NTT twiddle factors

/**
 * Precompute twiddle factors for 16384-point NTT
 */
static void precompute_twiddles(void) {
    // omega = primitive_root^((Q-1)/N) is the principal N-th root of unity
    uint32_t exponent = (Q - 1) / N;
    printf("Computing twiddle factors...\n");
    printf("  N = %d\n", N);
    printf("  Q = %u\n", Q);
    printf("  Q - 1 = %u\n", Q - 1);
    printf("  (Q - 1) / N = %u\n", exponent);
    printf("  Primitive root = %u\n", PRIMITIVE_ROOT);

    uint32_t omega = pow_mod(PRIMITIVE_ROOT, exponent);
    printf("  Omega (N-th root of unity) = %u\n", omega);

    // Verify omega is correct
    printf("  Verifying omega^N mod Q...\n");
    uint32_t omega_n = pow_mod(omega, N);
    printf("  Verification: omega^N mod Q = %u (should be 1)\n", omega_n);

    // Also verify omega^(N/2) = -1 (mod Q) for negacyclic check
    uint32_t omega_half = pow_mod(omega, N/2);
    printf("  Check: omega^(N/2) mod Q = %u (should be %u for negacyclic)\n",
           omega_half, Q - 1);

    if (omega_n != 1) {
        fprintf(stderr, "ERROR: omega is not an N-th root of unity!\n");
        fprintf(stderr, "Debug: Let me try computing 3^60944 manually...\n");

        // Try simple test: 3^(Q-1) should be 1 by Fermat's little theorem
        uint32_t test = pow_mod(3, Q - 1);
        fprintf(stderr, "3^(Q-1) mod Q = %u (should be 1)\n", test);
        exit(1);
    }

    // Forward twiddles: omega^i for i = 0..N-1
    twiddle_fwd[0] = 1;
    for (int i = 1; i < N; i++) {
        twiddle_fwd[i] = mul_mod(twiddle_fwd[i-1], omega);
    }

    // Inverse twiddles: omega^(-i) for i = 0..N-1
    uint32_t omega_inv = pow_mod(omega, Q - 2);  // Fermat's little theorem
    twiddle_inv[0] = 1;
    for (int i = 1; i < N; i++) {
        twiddle_inv[i] = mul_mod(twiddle_inv[i-1], omega_inv);
    }

    printf("  ✓ Twiddle factors computed\n\n");
}

// ============================================================================
// BIT REVERSAL
// ============================================================================

/**
 * Bit-reverse a 14-bit index (for N=16384=2^14)
 */
static inline uint32_t bit_reverse_14(uint32_t x) {
    uint32_t result = 0;
    for (int i = 0; i < 14; i++) {
        result = (result << 1) | (x & 1);
        x >>= 1;
    }
    return result;
}

/**
 * Bit-reversal permutation
 */
static void bit_reverse_copy(uint32_t *dst, const uint32_t *src) {
    for (int i = 0; i < N; i++) {
        dst[bit_reverse_14(i)] = src[i];
    }
}

// ============================================================================
// NTT IMPLEMENTATION
// ============================================================================

/**
 * Forward 16384-point NTT (in-place, Cooley-Tukey)
 */
void ntt_16384_forward(uint32_t *a) {
    // Bit-reversal permutation
    uint32_t *tmp = malloc(N * sizeof(uint32_t));
    bit_reverse_copy(tmp, a);
    memcpy(a, tmp, N * sizeof(uint32_t));
    free(tmp);

    // Cooley-Tukey butterfly iterations
    for (int stage = 1; stage <= 14; stage++) {  // log2(16384) = 14
        int m = 1 << stage;      // Size of each DFT
        int m_half = m >> 1;
        int stride = N / m;      // Twiddle stride

        for (int k = 0; k < N; k += m) {
            for (int j = 0; j < m_half; j++) {
                uint32_t t = mul_mod(twiddle_fwd[j * stride], a[k + j + m_half]);
                uint32_t u = a[k + j];
                a[k + j] = add_mod(u, t);
                a[k + j + m_half] = sub_mod(u, t);
            }
        }
    }
}

/**
 * Inverse 16384-point NTT (in-place)
 */
void ntt_16384_inverse(uint32_t *a) {
    // Bit-reversal permutation
    uint32_t *tmp = malloc(N * sizeof(uint32_t));
    bit_reverse_copy(tmp, a);
    memcpy(a, tmp, N * sizeof(uint32_t));
    free(tmp);

    // Cooley-Tukey butterfly iterations with inverse twiddles
    for (int stage = 1; stage <= 14; stage++) {
        int m = 1 << stage;
        int m_half = m >> 1;
        int stride = N / m;

        for (int k = 0; k < N; k += m) {
            for (int j = 0; j < m_half; j++) {
                uint32_t t = mul_mod(twiddle_inv[j * stride], a[k + j + m_half]);
                uint32_t u = a[k + j];
                a[k + j] = add_mod(u, t);
                a[k + j + m_half] = sub_mod(u, t);
            }
        }
    }

    // Divide by N
    uint32_t n_inv = pow_mod(N, Q - 2);  // N^(-1) mod Q
    for (int i = 0; i < N; i++) {
        a[i] = mul_mod(a[i], n_inv);
    }
}

/**
 * Pointwise multiplication in NTT domain
 */
void ntt_16384_pointwise_mul(uint32_t *result, const uint32_t *a, const uint32_t *b) {
    for (int i = 0; i < N; i++) {
        result[i] = mul_mod(a[i], b[i]);
    }
}

/**
 * Full convolution: result = a ⊗ b
 */
void convolve_16384(uint32_t *result, const uint32_t *a, const uint32_t *b) {
    uint32_t *a_ntt = malloc(N * sizeof(uint32_t));
    uint32_t *b_ntt = malloc(N * sizeof(uint32_t));

    memcpy(a_ntt, a, N * sizeof(uint32_t));
    memcpy(b_ntt, b, N * sizeof(uint32_t));

    ntt_16384_forward(a_ntt);
    ntt_16384_forward(b_ntt);

    ntt_16384_pointwise_mul(result, a_ntt, b_ntt);

    ntt_16384_inverse(result);

    free(a_ntt);
    free(b_ntt);
}

// ============================================================================
// TESTING
// ============================================================================

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║         FULL-SIZE 16384-POINT NTT DEMONSTRATION          ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    printf("Configuration:\n");
    printf("  NTT size (N):         %d (2^14)\n", N);
    printf("  Modulus (Q):          %u\n", Q);
    printf("  Q in hex:             0x%08X\n", Q);
    printf("  Q ≡ 1 (mod 2N):       %s\n", (Q - 1) % (2 * N) == 0 ? "✓ Yes" : "✗ No");
    printf("\n");

    // Precompute twiddle factors
    precompute_twiddles();

    // ========================================================================
    // TEST 1: NTT/INTT Reversibility
    // ========================================================================
    printf("┌───────────────────────────────────────────────────────────┐\n");
    printf("│ TEST 1: NTT/INTT Reversibility                           │\n");
    printf("└───────────────────────────────────────────────────────────┘\n");

    uint32_t *test_vec = malloc(N * sizeof(uint32_t));
    uint32_t *original = malloc(N * sizeof(uint32_t));

    // Initialize with simple pattern
    for (int i = 0; i < N; i++) {
        test_vec[i] = (i % 100) + 1;
    }
    memcpy(original, test_vec, N * sizeof(uint32_t));

    printf("  Forward NTT...\n");
    ntt_16384_forward(test_vec);

    printf("  Inverse NTT...\n");
    ntt_16384_inverse(test_vec);

    // Check if we got back the original
    int errors = 0;
    for (int i = 0; i < N; i++) {
        if (test_vec[i] != original[i]) {
            errors++;
        }
    }

    printf("  Result: %s (%d errors)\n\n",
           errors == 0 ? "✓ PASS" : "✗ FAIL", errors);

    // ========================================================================
    // TEST 2: Convolution Test
    // ========================================================================
    printf("┌───────────────────────────────────────────────────────────┐\n");
    printf("│ TEST 2: Convolution                                       │\n");
    printf("└───────────────────────────────────────────────────────────┘\n");

    uint32_t *a = malloc(N * sizeof(uint32_t));
    uint32_t *b = malloc(N * sizeof(uint32_t));
    uint32_t *result = malloc(N * sizeof(uint32_t));

    // Initialize test vectors
    for (int i = 0; i < N; i++) {
        a[i] = (i < 100) ? (i + 1) : 0;  // Sparse vector
        b[i] = (i < 50) ? (2 * i + 1) : 0;  // Another sparse vector
    }

    printf("  Vector A: first 100 elements non-zero\n");
    printf("  Vector B: first 50 elements non-zero\n");
    printf("  Computing A ⊗ B...\n");

    convolve_16384(result, a, b);

    // Count non-zeros in result
    int nonzero = 0;
    for (int i = 0; i < N; i++) {
        if (result[i] != 0) nonzero++;
    }

    printf("  Result non-zeros: %d / %d\n", nonzero, N);
    printf("  Result[0] = %u\n", result[0]);
    printf("  Result[10] = %u\n", result[10]);
    printf("  Result[100] = %u\n", result[100]);
    printf("  ✓ Convolution completed\n\n");

    // ========================================================================
    // TEST 3: Commutativity
    // ========================================================================
    printf("┌───────────────────────────────────────────────────────────┐\n");
    printf("│ TEST 3: Commutativity (A⊗B = B⊗A)                        │\n");
    printf("└───────────────────────────────────────────────────────────┘\n");

    uint32_t *ba = malloc(N * sizeof(uint32_t));
    convolve_16384(ba, b, a);

    int comm_errors = 0;
    for (int i = 0; i < N; i++) {
        if (result[i] != ba[i]) {
            comm_errors++;
        }
    }

    printf("  A⊗B vs B⊗A: %s (%d differences)\n\n",
           comm_errors == 0 ? "✓ PASS" : "✗ FAIL", comm_errors);

    // ========================================================================
    // TEST 4: Identity Element
    // ========================================================================
    printf("┌───────────────────────────────────────────────────────────┐\n");
    printf("│ TEST 4: Identity Element                                  │\n");
    printf("└───────────────────────────────────────────────────────────┘\n");

    uint32_t *identity = malloc(N * sizeof(uint32_t));
    memset(identity, 0, N * sizeof(uint32_t));
    identity[0] = 1;  // [1, 0, 0, 0, ...]

    uint32_t *a_id = malloc(N * sizeof(uint32_t));
    convolve_16384(a_id, a, identity);

    int id_errors = 0;
    for (int i = 0; i < N; i++) {
        if (a_id[i] != a[i]) {
            id_errors++;
        }
    }

    printf("  A⊗[1,0,0,...] = A: %s (%d errors)\n\n",
           id_errors == 0 ? "✓ PASS" : "✗ FAIL", id_errors);

    // ========================================================================
    // SUMMARY
    // ========================================================================
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                      SUMMARY                              ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    int all_pass = (errors == 0) && (comm_errors == 0) && (id_errors == 0);

    if (all_pass) {
        printf("  ✓ All tests PASSED\n");
        printf("  ✓ Full 16384-point NTT working correctly\n");
        printf("  ✓ Ready for hierarchical convolution trees\n");
    } else {
        printf("  ✗ Some tests FAILED\n");
    }

    printf("\n");

    // Cleanup
    free(test_vec);
    free(original);
    free(a);
    free(b);
    free(result);
    free(ba);
    free(identity);
    free(a_id);

    return all_pass ? 0 : 1;
}
