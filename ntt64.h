#ifndef NTT64_H
#define NTT64_H

#include <stdint.h>

// N = 64 for all NTT operations
#define NTT_N 64

// Number of supported modulus layers
#define NTT_NUM_LAYERS 7

// Layer indices for easy access
#define NTT_LAYER_257        0
#define NTT_LAYER_3329       1
#define NTT_LAYER_10753      2
#define NTT_LAYER_43777      3
#define NTT_LAYER_64513      4
#define NTT_LAYER_686593     5
#define NTT_LAYER_2818573313 6

/**
 * Forward NTT (negacyclic, constant-time)
 *
 * Computes the Number Theoretic Transform in-place.
 * Input/output in standard order (not bit-reversed).
 *
 * @param poly      Array of N=64 coefficients (modified in-place)
 * @param layer     Layer index (0-7) selecting the modulus
 *
 * Time complexity: O(N log N) = O(384) operations
 * All operations are constant-time to prevent timing attacks.
 */
void ntt64_forward(uint32_t poly[NTT_N], int layer);

/**
 * Inverse NTT (negacyclic, constant-time)
 *
 * Computes the inverse Number Theoretic Transform in-place.
 * Input/output in standard order (not bit-reversed).
 *
 * @param poly      Array of N=64 coefficients (modified in-place)
 * @param layer     Layer index (0-7) selecting the modulus
 *
 * Time complexity: O(N log N) = O(384) operations
 * All operations are constant-time to prevent timing attacks.
 */
void ntt64_inverse(uint32_t poly[NTT_N], int layer);

/**
 * Point-wise multiplication in NTT domain (constant-time)
 *
 * Computes result[i] = (a[i] * b[i]) mod q for all i.
 * All inputs must be in NTT domain (i.e., already transformed).
 *
 * @param result    Output array (can be same as a or b)
 * @param a         First operand in NTT domain
 * @param b         Second operand in NTT domain
 * @param layer     Layer index (0-7) selecting the modulus
 */
void ntt64_pointwise_mul(uint32_t result[NTT_N],
                         const uint32_t a[NTT_N],
                         const uint32_t b[NTT_N],
                         int layer);

/**
 * Get the modulus for a given layer
 *
 * @param layer     Layer index (0-7)
 * @return          The modulus q for that layer
 */
uint32_t ntt64_get_modulus(int layer);

// ============================================================================
// FIELD ARITHMETIC OPERATIONS (Constant-time)
// ============================================================================

/**
 * Constant-time modular addition: (a + b) mod q
 *
 * @param a         First operand
 * @param b         Second operand
 * @param layer     Layer index (0-7) selecting the modulus
 * @return          (a + b) mod q
 */
uint32_t ntt64_add_mod(uint32_t a, uint32_t b, int layer);

/**
 * Constant-time modular subtraction: (a - b) mod q
 *
 * @param a         First operand
 * @param b         Second operand
 * @param layer     Layer index (0-7) selecting the modulus
 * @return          (a - b) mod q
 */
uint32_t ntt64_sub_mod(uint32_t a, uint32_t b, int layer);

/**
 * Constant-time modular multiplication: (a * b) mod q
 *
 * Uses Barrett reduction for efficiency.
 *
 * @param a         First operand
 * @param b         Second operand
 * @param layer     Layer index (0-7) selecting the modulus
 * @return          (a * b) mod q
 */
uint32_t ntt64_mul_mod(uint32_t a, uint32_t b, int layer);

/**
 * Constant-time modular inverse: a^(-1) mod q
 *
 * Computes the modular multiplicative inverse using binary GCD.
 * Returns 0 if inverse does not exist (i.e., gcd(a, q) != 1).
 *
 * Time complexity: O(log^2 q) in constant time
 *
 * @param a         Value to invert (must be in range [1, q-1])
 * @param layer     Layer index (0-7) selecting the modulus
 * @return          a^(-1) mod q, or 0 if inverse doesn't exist
 */
uint32_t ntt64_inv_mod(uint32_t a, int layer);

#endif // NTT64_H
