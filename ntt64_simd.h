#ifndef NTT64_SIMD_H
#define NTT64_SIMD_H

#include "ntt64.h"

// CPU feature detection flags
#define NTT_CPU_SCALAR  0
#define NTT_CPU_AVX2    (1 << 0)
#define NTT_CPU_NEON    (1 << 1)

/**
 * Detect available CPU features at runtime
 * Returns a bitmask of NTT_CPU_* flags
 */
int ntt64_detect_cpu_features(void);

/**
 * Get a human-readable name for the current implementation
 */
const char* ntt64_get_implementation_name(void);

/**
 * Initialize the SIMD-optimized NTT implementation
 * This detects CPU features and sets up function pointers
 * Call this once at program startup for best performance
 */
void ntt64_init(void);

// Forward NTT function pointer type
typedef void (*ntt64_forward_fn)(uint32_t poly[NTT_N], int layer);

// Inverse NTT function pointer type
typedef void (*ntt64_inverse_fn)(uint32_t poly[NTT_N], int layer);

// Pointwise multiplication function pointer type
typedef void (*ntt64_pointwise_mul_fn)(uint32_t result[NTT_N],
                                        const uint32_t a[NTT_N],
                                        const uint32_t b[NTT_N],
                                        int layer);

// Global function pointers (set by ntt64_init)
extern ntt64_forward_fn ntt64_forward_ptr;
extern ntt64_inverse_fn ntt64_inverse_ptr;
extern ntt64_pointwise_mul_fn ntt64_pointwise_mul_ptr;

// Implementation-specific functions (don't call directly, use function pointers)
// Scalar (portable C) implementations
void ntt64_forward_scalar(uint32_t poly[NTT_N], int layer);
void ntt64_inverse_scalar(uint32_t poly[NTT_N], int layer);
void ntt64_pointwise_mul_scalar(uint32_t result[NTT_N],
                                 const uint32_t a[NTT_N],
                                 const uint32_t b[NTT_N],
                                 int layer);

#ifdef __AVX2__
// AVX2 implementations (x86-64 with AVX2 support)
void ntt64_forward_avx2(uint32_t poly[NTT_N], int layer);
void ntt64_inverse_avx2(uint32_t poly[NTT_N], int layer);
void ntt64_pointwise_mul_avx2(uint32_t result[NTT_N],
                               const uint32_t a[NTT_N],
                               const uint32_t b[NTT_N],
                               int layer);
#endif

#ifdef __ARM_NEON
// NEON implementations (ARM with NEON support)
void ntt64_forward_neon(uint32_t poly[NTT_N], int layer);
void ntt64_inverse_neon(uint32_t poly[NTT_N], int layer);
void ntt64_pointwise_mul_neon(uint32_t result[NTT_N],
                               const uint32_t a[NTT_N],
                               const uint32_t b[NTT_N],
                               int layer);
#endif

#endif // NTT64_SIMD_H
