#ifndef RS_PRF_H
#define RS_PRF_H

#include "rs_config.h"
#include <stddef.h>

// ============================================================================
// AES-256-CTR PSEUDORANDOM FUNCTION / EXTENDABLE OUTPUT FUNCTION
// ============================================================================

/**
 * AES-256-CTR XOF: Generate arbitrary-length keystream
 *
 * Implements a pseudorandom function that can generate any amount of
 * pseudorandom bytes from a key and nonce.
 *
 * @param key           32-byte AES-256 key
 * @param nonce         16-byte nonce (first 8 bytes fixed, last 8 = counter start)
 * @param counter_start Starting value for 64-bit counter
 * @param out           Output buffer
 * @param out_len       Number of bytes to generate
 */
void rs_prf_aes256_ctr(const uint8_t key[RS_KEY_BYTES],
                       const uint8_t nonce[RS_NONCE_BYTES],
                       uint64_t counter_start,
                       uint8_t *out,
                       size_t out_len);

/**
 * Derive a 32-byte AES-256 key from seed + label
 *
 * Uses SHA3-256(label || seed) to derive a deterministic key.
 *
 * @param seed      32-byte input seed
 * @param label     ASCII label string (e.g., "AX_KEY")
 * @param key_out   32-byte output key
 */
void rs_derive_aes_key(const uint8_t seed[RS_SEED_BYTES],
                       const char *label,
                       uint8_t key_out[RS_KEY_BYTES]);

/**
 * Derive a 16-byte nonce from seed, label, and two indices
 *
 * Uses SHA3-256(label || seed || index1 || index2) and takes first 16 bytes.
 * This ensures unique AES streams for each (seed, label, index1, index2) tuple.
 *
 * @param seed      32-byte input seed
 * @param label     ASCII label string (e.g., "AX_A", "B_ROW")
 * @param index1    First index (e.g., layer ell)
 * @param index2    Second index (e.g., slot or row)
 * @param nonce     16-byte output nonce
 */
void rs_derive_nonce_16(const uint8_t seed[RS_SEED_BYTES],
                        const char *label,
                        int index1,
                        int index2,
                        uint8_t nonce[RS_NONCE_BYTES]);

#endif // RS_PRF_H
