#include "ntt64.h"
#include "ntt64_simd.h"
#include <stdio.h>

// Global function pointers (initialized to scalar by default)
ntt64_forward_fn ntt64_forward_ptr = ntt64_forward_scalar;
ntt64_inverse_fn ntt64_inverse_ptr = ntt64_inverse_scalar;
ntt64_pointwise_mul_fn ntt64_pointwise_mul_ptr = ntt64_pointwise_mul_scalar;

// Global implementation name
static const char* implementation_name = "scalar";

// ============================================================================
// CPU FEATURE DETECTION
// ============================================================================

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

// x86/x86-64 CPU feature detection
#include <cpuid.h>

int ntt64_detect_cpu_features(void) {
    int features = NTT_CPU_SCALAR;

    #ifdef __AVX2__
    // Check if AVX2 is supported at runtime
    unsigned int eax, ebx, ecx, edx;
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        if (ebx & (1 << 5)) {  // AVX2 bit
            features |= NTT_CPU_AVX2;
        }
    }
    #endif

    return features;
}

#elif defined(__ARM_NEON) || defined(__aarch64__)

// ARM CPU feature detection
int ntt64_detect_cpu_features(void) {
    int features = NTT_CPU_SCALAR;

    #ifdef __ARM_NEON
    // NEON is available (either always-on for AArch64 or detected)
    features |= NTT_CPU_NEON;
    #endif

    return features;
}

#else

// Fallback for unsupported architectures
int ntt64_detect_cpu_features(void) {
    return NTT_CPU_SCALAR;
}

#endif

// ============================================================================
// INITIALIZATION AND DISPATCH
// ============================================================================

void ntt64_init(void) {
    int features = ntt64_detect_cpu_features();

    // Priority: AVX2 > NEON > Scalar
    #ifdef __AVX2__
    if (features & NTT_CPU_AVX2) {
        ntt64_forward_ptr = ntt64_forward_avx2;
        ntt64_inverse_ptr = ntt64_inverse_avx2;
        ntt64_pointwise_mul_ptr = ntt64_pointwise_mul_avx2;
        implementation_name = "AVX2";
        return;
    }
    #endif

    #ifdef __ARM_NEON
    if (features & NTT_CPU_NEON) {
        ntt64_forward_ptr = ntt64_forward_neon;
        ntt64_inverse_ptr = ntt64_inverse_neon;
        ntt64_pointwise_mul_ptr = ntt64_pointwise_mul_neon;
        implementation_name = "NEON";
        return;
    }
    #endif

    // Fallback to scalar (already initialized)
    ntt64_forward_ptr = ntt64_forward_scalar;
    ntt64_inverse_ptr = ntt64_inverse_scalar;
    ntt64_pointwise_mul_ptr = ntt64_pointwise_mul_scalar;
    implementation_name = "scalar";
}

const char* ntt64_get_implementation_name(void) {
    return implementation_name;
}
