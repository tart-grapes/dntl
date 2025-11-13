#include "rs_prf.h"
#include <string.h>
#include <openssl/evp.h>
#include <openssl/err.h>

// ============================================================================
// AES-256-CTR IMPLEMENTATION
// ============================================================================

void rs_prf_aes256_ctr(const uint8_t key[RS_KEY_BYTES],
                       const uint8_t nonce[RS_NONCE_BYTES],
                       uint64_t counter_start,
                       uint8_t *out,
                       size_t out_len) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        // Fatal error
        return;
    }

    // Initialize AES-256-CTR
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, key, nonce) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return;
    }

    // AES-CTR mode: counter is the IV
    // OpenSSL handles counter increment automatically
    // We set the initial counter by providing it as the IV

    // Build initial counter block:
    // First 8 bytes: fixed nonce prefix
    // Last 8 bytes: counter_start (little-endian)
    uint8_t iv[16];
    memcpy(iv, nonce, 8);  // First 8 bytes from nonce

    // Encode counter_start as little-endian in last 8 bytes
    for (int i = 0; i < 8; i++) {
        iv[8 + i] = (counter_start >> (i * 8)) & 0xFF;
    }

    // Reinitialize with our counter
    if (EVP_EncryptInit_ex(ctx, NULL, NULL, NULL, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return;
    }

    // Generate keystream
    // In CTR mode, "encrypting" zeros gives us the keystream directly
    // But OpenSSL CTR will XOR with plaintext, so we can just encrypt zeros
    int len;
    if (EVP_EncryptUpdate(ctx, out, &len, (const uint8_t *)"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", out_len > 16 ? 16 : out_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return;
    }

    // Actually, let's do this properly by generating the keystream
    // For CTR mode, we can use a simpler approach:
    size_t offset = 0;
    uint8_t plaintext[16];
    memset(plaintext, 0, 16);

    while (offset < out_len) {
        size_t chunk = (out_len - offset) < 16 ? (out_len - offset) : 16;
        int outlen;

        if (EVP_EncryptUpdate(ctx, out + offset, &outlen, plaintext, chunk) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return;
        }

        offset += outlen;
    }

    EVP_CIPHER_CTX_free(ctx);
}

// ============================================================================
// KEY DERIVATION
// ============================================================================

void rs_derive_aes_key(const uint8_t seed[RS_SEED_BYTES],
                       const char *label,
                       uint8_t key_out[RS_KEY_BYTES]) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return;

    // Use SHA3-256 for key derivation
    if (EVP_DigestInit_ex(ctx, EVP_sha3_256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return;
    }

    // Hash: label || seed
    size_t label_len = strlen(label);
    EVP_DigestUpdate(ctx, label, label_len);
    EVP_DigestUpdate(ctx, seed, RS_SEED_BYTES);

    // Finalize to get 32-byte output
    unsigned int out_len;
    EVP_DigestFinal_ex(ctx, key_out, &out_len);

    EVP_MD_CTX_free(ctx);
}

// ============================================================================
// NONCE DERIVATION
// ============================================================================

void rs_derive_nonce_16(const uint8_t seed[RS_SEED_BYTES],
                        const char *label,
                        int index1,
                        int index2,
                        uint8_t nonce[RS_NONCE_BYTES]) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) return;

    // Use SHA3-256 for nonce derivation
    if (EVP_DigestInit_ex(ctx, EVP_sha3_256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return;
    }

    // Hash: label || seed || index1 || index2
    size_t label_len = strlen(label);
    EVP_DigestUpdate(ctx, label, label_len);
    EVP_DigestUpdate(ctx, seed, RS_SEED_BYTES);

    // Encode indices as little-endian 4-byte integers
    uint8_t idx1_bytes[4], idx2_bytes[4];
    for (int i = 0; i < 4; i++) {
        idx1_bytes[i] = (index1 >> (i * 8)) & 0xFF;
        idx2_bytes[i] = (index2 >> (i * 8)) & 0xFF;
    }

    EVP_DigestUpdate(ctx, idx1_bytes, 4);
    EVP_DigestUpdate(ctx, idx2_bytes, 4);

    // Finalize to get 32-byte output
    uint8_t hash[32];
    unsigned int out_len;
    EVP_DigestFinal_ex(ctx, hash, &out_len);

    // Take first 16 bytes as nonce
    memcpy(nonce, hash, RS_NONCE_BYTES);

    EVP_MD_CTX_free(ctx);
}
