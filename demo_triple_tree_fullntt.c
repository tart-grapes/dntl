/**
 * Triple NTT Convolution Tree with FULL-SIZE 16384-Point NTTs
 *
 * Uses actual 16384-point NTTs instead of block-wise 64-point NTTs.
 * Creates three independent trees (X, Y, Z) and tests algebraic properties.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define N 16384  // Full NTT size
#define Q 998244353u  // NTT-friendly modulus
#define PRIMITIVE_ROOT 3u
#define INITIAL_VECTORS 64

// ============================================================================
// MODULAR ARITHMETIC
// ============================================================================

static inline uint32_t add_mod(uint32_t a, uint32_t b) {
    uint64_t sum = (uint64_t)a + (uint64_t)b;
    return sum >= Q ? sum - Q : sum;
}

static inline uint32_t sub_mod(uint32_t a, uint32_t b) {
    return a >= b ? a - b : a + Q - b;
}

static inline uint32_t mul_mod(uint32_t a, uint32_t b) {
    return ((uint64_t)a * (uint64_t)b) % Q;
}

static uint32_t pow_mod(uint32_t base, uint32_t exp) {
    uint32_t result = 1;
    base %= Q;
    while (exp > 0) {
        if (exp & 1) result = mul_mod(result, base);
        base = mul_mod(base, base);
        exp >>= 1;
    }
    return result;
}

// ============================================================================
// NTT TWIDDLE FACTORS
// ============================================================================

static uint32_t *twiddle_fwd = NULL;
static uint32_t *twiddle_inv = NULL;

static inline uint32_t bit_reverse_14(uint32_t x) {
    uint32_t result = 0;
    for (int i = 0; i < 14; i++) {
        result = (result << 1) | (x & 1);
        x >>= 1;
    }
    return result;
}

static void precompute_twiddles(void) {
    twiddle_fwd = malloc(N * sizeof(uint32_t));
    twiddle_inv = malloc(N * sizeof(uint32_t));

    uint32_t omega = pow_mod(PRIMITIVE_ROOT, (Q - 1) / N);
    uint32_t omega_inv = pow_mod(omega, Q - 2);

    twiddle_fwd[0] = 1;
    twiddle_inv[0] = 1;
    for (int i = 1; i < N; i++) {
        twiddle_fwd[i] = mul_mod(twiddle_fwd[i-1], omega);
        twiddle_inv[i] = mul_mod(twiddle_inv[i-1], omega_inv);
    }
}

// ============================================================================
// NTT OPERATIONS
// ============================================================================

static void bit_reverse_copy(uint32_t *dst, const uint32_t *src) {
    for (int i = 0; i < N; i++) {
        dst[bit_reverse_14(i)] = src[i];
    }
}

static void ntt_forward(uint32_t *a) {
    uint32_t *tmp = malloc(N * sizeof(uint32_t));
    bit_reverse_copy(tmp, a);
    memcpy(a, tmp, N * sizeof(uint32_t));
    free(tmp);

    for (int stage = 1; stage <= 14; stage++) {
        int m = 1 << stage;
        int m_half = m >> 1;
        int stride = N / m;

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

static void ntt_inverse(uint32_t *a) {
    uint32_t *tmp = malloc(N * sizeof(uint32_t));
    bit_reverse_copy(tmp, a);
    memcpy(a, tmp, N * sizeof(uint32_t));
    free(tmp);

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

    uint32_t n_inv = pow_mod(N, Q - 2);
    for (int i = 0; i < N; i++) {
        a[i] = mul_mod(a[i], n_inv);
    }
}

static void convolve(uint32_t *result, const uint32_t *a, const uint32_t *b) {
    uint32_t *a_ntt = malloc(N * sizeof(uint32_t));
    uint32_t *b_ntt = malloc(N * sizeof(uint32_t));

    memcpy(a_ntt, a, N * sizeof(uint32_t));
    memcpy(b_ntt, b, N * sizeof(uint32_t));

    ntt_forward(a_ntt);
    ntt_forward(b_ntt);

    for (int i = 0; i < N; i++) {
        result[i] = mul_mod(a_ntt[i], b_ntt[i]);
    }

    ntt_inverse(result);

    free(a_ntt);
    free(b_ntt);
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static double compute_l2_norm(const uint32_t *vec) {
    double sum = 0.0;
    for (int i = 0; i < N; i++) {
        int64_t c = (int64_t)vec[i];
        if (c > Q/2) c -= Q;
        sum += (double)c * c;
    }
    return sqrt(sum);
}

static int vectors_equal(const uint32_t *a, const uint32_t *b) {
    return memcmp(a, b, N * sizeof(uint32_t)) == 0;
}

static void init_vector(uint32_t *vec, uint32_t seed) {
    uint32_t state = seed;
    for (int i = 0; i < N; i++) {
        state = (state * 1103515245u + 12345u) & 0x7FFFFFFFu;
        uint32_t val = state % 20;
        vec[i] = (val < 10) ? (val % Q) : 0;
    }
}

// ============================================================================
// TREE OPERATIONS
// ============================================================================

typedef struct {
    uint32_t **vectors;
    int num_vectors;
} tree_level_t;

static void run_convolution_tree(tree_level_t levels[7], const char *name) {
    printf("  %s tree: ", name);
    fflush(stdout);

    for (int level = 0; level < 6; level++) {
        int dst_count = levels[level + 1].num_vectors;

        for (int pair = 0; pair < dst_count; pair++) {
            convolve(levels[level + 1].vectors[pair],
                    levels[level].vectors[pair * 2],
                    levels[level].vectors[pair * 2 + 1]);
        }
        printf("%d ", levels[level + 1].num_vectors);
        fflush(stdout);
    }
    printf("→ done\n");
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║    TRIPLE TREE with FULL 16384-POINT NTTs                ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    printf("Initializing full-size NTT (N=%d)...\n", N);
    precompute_twiddles();
    printf("✓ Twiddle factors computed\n\n");

    // Allocate trees
    tree_level_t x_tree[7], y_tree[7], z_tree[7];
    int num_vecs[] = {64, 32, 16, 8, 4, 2, 1};

    for (int l = 0; l < 7; l++) {
        x_tree[l].num_vectors = num_vecs[l];
        y_tree[l].num_vectors = num_vecs[l];
        z_tree[l].num_vectors = num_vecs[l];

        x_tree[l].vectors = malloc(num_vecs[l] * sizeof(uint32_t*));
        y_tree[l].vectors = malloc(num_vecs[l] * sizeof(uint32_t*));
        z_tree[l].vectors = malloc(num_vecs[l] * sizeof(uint32_t*));

        for (int i = 0; i < num_vecs[l]; i++) {
            x_tree[l].vectors[i] = malloc(N * sizeof(uint32_t));
            y_tree[l].vectors[i] = malloc(N * sizeof(uint32_t));
            z_tree[l].vectors[i] = malloc(N * sizeof(uint32_t));
        }
    }

    // Initialize level 0
    printf("Initializing 3 × 64 vectors (%d dimensions each)...\n", N);
    for (int i = 0; i < 64; i++) {
        init_vector(x_tree[0].vectors[i], 1000 + i * 123);
        init_vector(y_tree[0].vectors[i], 2000 + i * 456);
        init_vector(z_tree[0].vectors[i], 3000 + i * 789);
    }
    printf("✓ Initialization complete\n\n");

    // Run trees
    printf("Running convolution trees (64→32→16→8→4→2→1):\n");
    run_convolution_tree(x_tree, "X");
    run_convolution_tree(y_tree, "Y");
    run_convolution_tree(z_tree, "Z");
    printf("\n");

    uint32_t *x = x_tree[6].vectors[0];
    uint32_t *y = y_tree[6].vectors[0];
    uint32_t *z = z_tree[6].vectors[0];

    printf("Final results:\n");
    printf("  X: L2 norm = %.2f\n", compute_l2_norm(x));
    printf("  Y: L2 norm = %.2f\n", compute_l2_norm(y));
    printf("  Z: L2 norm = %.2f\n\n", compute_l2_norm(z));

    // Test algebraic properties
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║              ALGEBRAIC PROPERTIES TEST                    ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    uint32_t *xy = malloc(N * sizeof(uint32_t));
    uint32_t *yx = malloc(N * sizeof(uint32_t));
    uint32_t *xy_z = malloc(N * sizeof(uint32_t));
    uint32_t *yz = malloc(N * sizeof(uint32_t));
    uint32_t *x_yz = malloc(N * sizeof(uint32_t));

    printf("Testing commutativity (x⊗y = y⊗x)...\n");
    convolve(xy, x, y);
    convolve(yx, y, x);
    int comm = vectors_equal(xy, yx);
    printf("  Result: %s\n\n", comm ? "✓ PASS" : "✗ FAIL");

    printf("Testing associativity ((x⊗y)⊗z = x⊗(y⊗z))...\n");
    convolve(xy_z, xy, z);
    convolve(yz, y, z);
    convolve(x_yz, x, yz);
    int assoc = vectors_equal(xy_z, x_yz);
    printf("  Result: %s\n\n", assoc ? "✓ PASS" : "✗ FAIL");

    printf("Testing all 6 permutations...\n");
    uint32_t *perm[6];
    for (int i = 0; i < 6; i++) {
        perm[i] = malloc(N * sizeof(uint32_t));
    }

    memcpy(perm[0], xy_z, N * sizeof(uint32_t));  // (x⊗y)⊗z

    uint32_t *xz = malloc(N * sizeof(uint32_t));
    uint32_t *zx = malloc(N * sizeof(uint32_t));
    uint32_t *zy = malloc(N * sizeof(uint32_t));

    convolve(xz, x, z);
    convolve(perm[1], xz, y);  // (x⊗z)⊗y

    convolve(perm[2], yx, z);  // (y⊗x)⊗z
    convolve(perm[3], yz, x);  // (y⊗z)⊗x

    convolve(zx, z, x);
    convolve(perm[4], zx, y);  // (z⊗x)⊗y

    convolve(zy, z, y);
    convolve(perm[5], zy, x);  // (z⊗y)⊗x

    int all_equal = 1;
    for (int i = 1; i < 6; i++) {
        if (!vectors_equal(perm[0], perm[i])) {
            all_equal = 0;
            printf("  Permutation %d differs!\n", i);
        }
    }

    printf("  Result: %s\n\n", all_equal ? "✓ ALL EQUAL" : "✗ DIFFER");

    // Summary
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                      SUMMARY                              ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    if (comm && assoc && all_equal) {
        printf("  ✓ Commutativity:  VERIFIED\n");
        printf("  ✓ Associativity:  VERIFIED\n");
        printf("  ✓ All permutations: IDENTICAL\n\n");
        printf("  ✓ Full 16384-point NTT forms commutative, associative algebra\n");
        printf("  ✓ All algebraic properties hold with full-size NTTs\n");
    } else {
        printf("  ✗ Some properties failed\n");
    }

    printf("\n");

    // Cleanup
    free(twiddle_fwd);
    free(twiddle_inv);
    for (int l = 0; l < 7; l++) {
        for (int i = 0; i < num_vecs[l]; i++) {
            free(x_tree[l].vectors[i]);
            free(y_tree[l].vectors[i]);
            free(z_tree[l].vectors[i]);
        }
        free(x_tree[l].vectors);
        free(y_tree[l].vectors);
        free(z_tree[l].vectors);
    }
    free(xy);
    free(yx);
    free(xy_z);
    free(yz);
    free(x_yz);
    free(xz);
    free(zx);
    free(zy);
    for (int i = 0; i < 6; i++) free(perm[i]);

    return (comm && assoc && all_equal) ? 0 : 1;
}
