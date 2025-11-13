#ifndef RS_CONFIG_H
#define RS_CONFIG_H

#include <stdint.h>

// ============================================================================
// RING-SWITCHING SYSTEM CONFIGURATION
// ============================================================================

// Matrix and vector dimensions
#define RS_N           64        // Matrix dimension (N×N matrices)
#define RS_NUM_LAYERS  7         // Number of modulus layers
#define RS_SLOT_COUNT  4         // Number of slots per layer
#define RS_PUBLIC_DIM  64        // Length of public tag (m in LWR)
#define RS_SECRET_DIM  (RS_N * RS_SLOT_COUNT)  // 256 = dimension of secret

// Moduli for each layer (q₀, q₁, ..., q₆)
// All satisfy qᵢ ≡ 1 (mod 128) for NTT-friendliness
static const uint32_t RS_Q_LAYERS[RS_NUM_LAYERS] = {
    257u,        // ~8 bits
    3329u,       // ~12 bits (Kyber-ish)
    12289u,      // ~14 bits
    40961u,      // ~16 bits
    65537u,      // ~16 bits (Fermat prime)
    786433u,     // ~20 bits
    2013265921u  // ~31 bits
};

// LWR (Learning With Rounding) parameters
#define RS_P_SMALL     12289u    // LWR modulus for tag
#define RS_LWR_SHIFT   16        // Shift bits for LWR truncation (⌊·/2^16⌋)

// AES key and seed sizes
#define RS_SEED_BYTES  32        // 256-bit seeds
#define RS_KEY_BYTES   32        // AES-256 keys
#define RS_NONCE_BYTES 16        // 128-bit nonces for AES-CTR

#endif // RS_CONFIG_H
