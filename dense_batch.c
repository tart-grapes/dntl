/**
 * dense_batch.c
 *
 * Dense vector batch encoding using rANS
 * - No position encoding (all positions have values)
 * - Exploits value concentration via frequency distribution
 * - Single shared frequency table for all vectors
 */

#include "dense_batch.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Bit writer/reader */
typedef struct {
    uint8_t *buffer;
    size_t byte_pos;
    uint8_t bit_pos;
    size_t capacity;
} bit_writer_t;

typedef struct {
    const uint8_t *buffer;
    size_t byte_pos;
    uint8_t bit_pos;
    size_t size;
} bit_reader_t;

static void bw_init(bit_writer_t *bw, uint8_t *buffer, size_t capacity) {
    bw->buffer = buffer;
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    bw->capacity = capacity;
}

static int bw_write_bit(bit_writer_t *bw, uint8_t bit) {
    if (bw->byte_pos >= bw->capacity) return -1;
    if (bit) bw->buffer[bw->byte_pos] |= (1 << (7 - bw->bit_pos));
    bw->bit_pos++;
    if (bw->bit_pos == 8) {
        bw->bit_pos = 0;
        bw->byte_pos++;
    }
    return 0;
}

static int bw_write_bits(bit_writer_t *bw, uint32_t value, uint8_t num_bits) {
    for (int i = num_bits - 1; i >= 0; i--) {
        if (bw_write_bit(bw, (value >> i) & 1) < 0) return -1;
    }
    return 0;
}

static void bw_align(bit_writer_t *bw) {
    if (bw->bit_pos > 0) {
        bw->byte_pos++;
        bw->bit_pos = 0;
    }
}

static int bw_write_byte(bit_writer_t *bw, uint8_t byte) {
    if (bw->bit_pos != 0) return -1;
    if (bw->byte_pos >= bw->capacity) return -1;
    bw->buffer[bw->byte_pos++] = byte;
    return 0;
}

static size_t bw_finish(bit_writer_t *bw) {
    return bw->byte_pos + (bw->bit_pos > 0 ? 1 : 0);
}

static void br_init(bit_reader_t *br, const uint8_t *buffer, size_t size) {
    br->buffer = buffer;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->size = size;
}

static int br_read_bit(bit_reader_t *br) {
    if (br->byte_pos >= br->size) return -1;
    int bit = (br->buffer[br->byte_pos] >> (7 - br->bit_pos)) & 1;
    br->bit_pos++;
    if (br->bit_pos == 8) {
        br->bit_pos = 0;
        br->byte_pos++;
    }
    return bit;
}

static int br_read_bits(bit_reader_t *br, uint8_t num_bits, uint32_t *out) {
    uint32_t value = 0;
    for (uint8_t i = 0; i < num_bits; i++) {
        int bit = br_read_bit(br);
        if (bit < 0) return -1;
        value = (value << 1) | bit;
    }
    *out = value;
    return 0;
}

static void br_align(bit_reader_t *br) {
    if (br->bit_pos > 0) {
        br->byte_pos++;
        br->bit_pos = 0;
    }
}

static int br_read_byte(bit_reader_t *br, uint8_t *out) {
    if (br->bit_pos != 0) return -1;
    if (br->byte_pos >= br->size) return -1;
    *out = br->buffer[br->byte_pos++];
    return 0;
}

/* rANS with 10-bit frequencies (max sum = 1024) for better precision */
#define ANS_L 65536

typedef struct {
    uint32_t state;
    uint32_t *freq;
    uint32_t *cumul;
    uint32_t total;
    int n_symbols;
    bit_writer_t *bw;
    uint8_t *rans_buffer;
    size_t rans_capacity;
    size_t rans_pos;
} rans_encoder_t;

typedef struct {
    uint32_t state;
    uint32_t *freq;
    uint32_t *cumul;
    uint32_t total;
    int n_symbols;
    bit_reader_t *br;
    uint8_t *rans_buffer;
    int rans_pos;
} rans_decoder_t;

static void normalize_freqs(uint32_t *freqs, int n_symbols, uint32_t *norm_freqs) {
    uint32_t total = 0;
    for (int i = 0; i < n_symbols; i++) {
        total += freqs[i];
    }

    /* Use 10-bit frequencies for dense data (better precision) */
    uint32_t target = 1024;
    uint64_t sum = 0;

    for (int i = 0; i < n_symbols; i++) {
        uint64_t scaled = ((uint64_t)freqs[i] * target) / total;
        if (scaled == 0 && freqs[i] > 0) scaled = 1;
        norm_freqs[i] = (uint32_t)scaled;
        sum += scaled;
    }

    while (sum > target) {
        int max_idx = 0;
        for (int i = 1; i < n_symbols; i++) {
            if (norm_freqs[i] > norm_freqs[max_idx]) max_idx = i;
        }
        norm_freqs[max_idx]--;
        sum--;
    }
    while (sum < target) {
        int max_idx = 0;
        for (int i = 1; i < n_symbols; i++) {
            if (norm_freqs[i] > norm_freqs[max_idx]) max_idx = i;
        }
        norm_freqs[max_idx]++;
        sum++;
    }
}

static void rans_init_encoder(rans_encoder_t *enc, uint32_t *freqs, int n_symbols,
                               bit_writer_t *bw, size_t max_symbols) {
    enc->state = ANS_L;
    enc->n_symbols = n_symbols;
    enc->bw = bw;

    enc->freq = malloc(n_symbols * sizeof(uint32_t));
    normalize_freqs(freqs, n_symbols, enc->freq);

    enc->cumul = malloc((n_symbols + 1) * sizeof(uint32_t));
    enc->cumul[0] = 0;
    enc->total = 0;
    for (int i = 0; i < n_symbols; i++) {
        enc->total += enc->freq[i];
        enc->cumul[i + 1] = enc->total;
    }

    enc->rans_capacity = max_symbols * 10 + 4;
    enc->rans_buffer = malloc(enc->rans_capacity);
    enc->rans_pos = 0;
}

static int rans_encode_symbol(rans_encoder_t *enc, int symbol) {
    uint32_t freq = enc->freq[symbol];
    uint32_t cumul = enc->cumul[symbol];

    /* Standard rANS formula for 10-bit frequencies */
    uint32_t max_state = ((ANS_L >> 10) << 8) * freq;
    while (enc->state >= max_state) {
        if (enc->rans_pos >= enc->rans_capacity) return -1;
        uint8_t byte = enc->state & 0xFF;
        enc->rans_buffer[enc->rans_pos++] = byte;
        enc->state >>= 8;
    }

    enc->state = enc->total * (enc->state / freq) + cumul + (enc->state % freq);
    return 0;
}

static int rans_flush(rans_encoder_t *enc) {
    while (enc->state >= (ANS_L << 8)) {
        if (enc->rans_pos >= enc->rans_capacity) return -1;
        uint8_t byte = enc->state & 0xFF;
        enc->rans_buffer[enc->rans_pos++] = byte;
        enc->state >>= 8;
    }

    while ((enc->state >> 8) >= ANS_L && enc->state >= (ANS_L << 1)) {
        if (enc->rans_pos >= enc->rans_capacity) return -1;
        uint8_t byte = enc->state & 0xFF;
        enc->rans_buffer[enc->rans_pos++] = byte;
        enc->state >>= 8;
    }

    if (enc->state < ANS_L) return -1;

    /* Write renorm bytes in forward order */
    for (size_t i = 0; i < enc->rans_pos; i++) {
        if (bw_write_byte(enc->bw, enc->rans_buffer[i]) < 0) {
            free(enc->freq);
            free(enc->cumul);
            free(enc->rans_buffer);
            return -1;
        }
    }

    /* Write final state */
    for (int i = 0; i < 4; i++) {
        uint8_t byte = (enc->state >> (i * 8)) & 0xFF;
        if (bw_write_byte(enc->bw, byte) < 0) {
            free(enc->freq);
            free(enc->cumul);
            free(enc->rans_buffer);
            return -1;
        }
    }

    free(enc->freq);
    free(enc->cumul);
    free(enc->rans_buffer);
    return 0;
}

static int rans_init_decoder(rans_decoder_t *dec, uint32_t *freqs, int n_symbols, bit_reader_t *br) {
    dec->n_symbols = n_symbols;
    dec->br = br;

    dec->freq = malloc(n_symbols * sizeof(uint32_t));
    normalize_freqs(freqs, n_symbols, dec->freq);

    dec->cumul = malloc((n_symbols + 1) * sizeof(uint32_t));
    dec->cumul[0] = 0;
    dec->total = 0;
    for (int i = 0; i < n_symbols; i++) {
        dec->total += dec->freq[i];
        dec->cumul[i + 1] = dec->total;
    }

    size_t remaining = br->size - br->byte_pos;
    dec->rans_buffer = malloc(remaining);
    if (!dec->rans_buffer) {
        free(dec->freq);
        free(dec->cumul);
        return -1;
    }

    for (size_t i = 0; i < remaining; i++) {
        uint8_t byte;
        if (br_read_byte(br, &byte) < 0) {
            free(dec->freq);
            free(dec->cumul);
            free(dec->rans_buffer);
            return -1;
        }
        dec->rans_buffer[i] = byte;
    }

    if (remaining < 4) {
        free(dec->freq);
        free(dec->cumul);
        free(dec->rans_buffer);
        return -1;
    }

    dec->state = 0;
    size_t state_pos = remaining - 4;
    for (int i = 0; i < 4; i++) {
        dec->state |= ((uint32_t)dec->rans_buffer[state_pos + i] << (i * 8));
    }

    dec->rans_pos = (int)state_pos - 1;
    return 0;
}

static int rans_decode_symbol(rans_decoder_t *dec, int *symbol, int skip_renorm) {
    uint32_t slot = dec->state % dec->total;

    *symbol = 0;
    for (int i = 0; i < dec->n_symbols; i++) {
        if (slot < dec->cumul[i + 1]) {
            *symbol = i;
            break;
        }
    }

    uint32_t freq = dec->freq[*symbol];
    uint32_t cumul = dec->cumul[*symbol];

    dec->state = freq * (dec->state / dec->total) + (slot - cumul);

    if (!skip_renorm) {
        while (dec->state < ANS_L) {
            if (dec->rans_pos < 0) return -1;
            uint8_t byte = dec->rans_buffer[dec->rans_pos--];
            dec->state = (dec->state << 8) | byte;
        }
    }

    return 0;
}

static void rans_free_decoder(rans_decoder_t *dec) {
    if (dec) {
        free(dec->freq);
        free(dec->cumul);
        free(dec->rans_buffer);
    }
}

/* Dense batch encoding */
dense_batch_t* dense_batch_encode(const uint16_t **vectors,
                                   uint16_t num_vectors,
                                   uint16_t dimension,
                                   uint16_t modulus) {
    if (!vectors || num_vectors == 0 || dimension == 0 || modulus == 0) return NULL;

    /* Build global frequency table */
    uint32_t *value_freqs = calloc(modulus, sizeof(uint32_t));
    if (!value_freqs) return NULL;

    uint32_t total_coeffs = (uint32_t)num_vectors * dimension;

    for (uint16_t v = 0; v < num_vectors; v++) {
        for (uint16_t i = 0; i < dimension; i++) {
            uint16_t val = vectors[v][i] % modulus;
            value_freqs[val]++;
        }
    }

    /* Count unique values */
    int n_unique = 0;
    for (uint16_t i = 0; i < modulus; i++) {
        if (value_freqs[i] > 0) n_unique++;
    }

    if (n_unique == 0) {
        free(value_freqs);
        return NULL;
    }

    /* Build compact alphabet */
    uint16_t *alphabet = malloc(n_unique * sizeof(uint16_t));
    uint32_t *alphabet_freqs = malloc(n_unique * sizeof(uint32_t));
    uint16_t *value_to_idx = malloc(modulus * sizeof(uint16_t));

    if (!alphabet || !alphabet_freqs || !value_to_idx) {
        free(value_freqs);
        free(alphabet);
        free(alphabet_freqs);
        free(value_to_idx);
        return NULL;
    }

    int idx = 0;
    for (uint16_t i = 0; i < modulus; i++) {
        if (value_freqs[i] > 0) {
            alphabet[idx] = i;
            alphabet_freqs[idx] = value_freqs[i];
            value_to_idx[i] = idx;
            idx++;
        }
    }

    free(value_freqs);

    /* Allocate result */
    dense_batch_t *result = malloc(sizeof(dense_batch_t));
    if (!result) {
        free(alphabet);
        free(alphabet_freqs);
        free(value_to_idx);
        return NULL;
    }

    result->num_vectors = num_vectors;
    result->dimension = dimension;

    /* Estimate size: header + alphabet + freq table + rANS stream */
    size_t max_size = 20 + n_unique * 4 + total_coeffs * 2;
    result->data = calloc(max_size, 1);
    if (!result->data) {
        free(result);
        free(alphabet);
        free(alphabet_freqs);
        free(value_to_idx);
        return NULL;
    }

    /* Encode */
    bit_writer_t bw;
    bw_init(&bw, result->data, max_size);

    /* Header */
    if (bw_write_bits(&bw, num_vectors, 16) < 0) goto error;
    if (bw_write_bits(&bw, dimension, 16) < 0) goto error;
    if (bw_write_bits(&bw, modulus, 16) < 0) goto error;
    if (bw_write_bits(&bw, n_unique, 16) < 0) goto error;

    /* Alphabet (values that appear) */
    int bits_per_value = 0;
    uint16_t temp = modulus - 1;
    while (temp > 0) {
        bits_per_value++;
        temp >>= 1;
    }

    for (int i = 0; i < n_unique; i++) {
        if (bw_write_bits(&bw, alphabet[i], bits_per_value) < 0) goto error;
    }

    /* Frequency table (10 bits each) */
    uint32_t norm_freqs[4096];
    normalize_freqs(alphabet_freqs, n_unique, norm_freqs);
    for (int i = 0; i < n_unique; i++) {
        if (bw_write_bits(&bw, norm_freqs[i], 10) < 0) goto error;
    }

    /* Align before rANS */
    bw_align(&bw);

    /* Encode all values with rANS (in reverse order) */
    rans_encoder_t rans;
    rans_init_encoder(&rans, alphabet_freqs, n_unique, &bw, total_coeffs);

    for (int v = num_vectors - 1; v >= 0; v--) {
        for (int i = (int)dimension - 1; i >= 0; i--) {
            uint16_t val = vectors[v][i] % modulus;
            int sym_idx = value_to_idx[val];
            if (rans_encode_symbol(&rans, sym_idx) < 0) goto error;
        }
    }

    if (rans_flush(&rans) < 0) goto error;

    result->size = bw_finish(&bw);

    free(alphabet);
    free(alphabet_freqs);
    free(value_to_idx);
    return result;

error:
    free(result->data);
    free(result);
    free(alphabet);
    free(alphabet_freqs);
    free(value_to_idx);
    return NULL;
}

int dense_batch_decode(const dense_batch_t *encoded,
                       uint16_t **vectors,
                       uint16_t num_vectors,
                       uint16_t dimension) {
    if (!encoded || !encoded->data || !vectors) return -1;

    bit_reader_t br;
    br_init(&br, encoded->data, encoded->size);

    /* Read header */
    uint32_t num_vecs_enc, dim_enc, modulus, n_unique;
    if (br_read_bits(&br, 16, &num_vecs_enc) < 0) return -1;
    if (br_read_bits(&br, 16, &dim_enc) < 0) return -1;
    if (br_read_bits(&br, 16, &modulus) < 0) return -1;
    if (br_read_bits(&br, 16, &n_unique) < 0) return -1;

    if (num_vecs_enc != num_vectors || dim_enc != dimension) return -1;

    /* Read alphabet */
    int bits_per_value = 0;
    uint16_t temp = modulus - 1;
    while (temp > 0) {
        bits_per_value++;
        temp >>= 1;
    }

    uint16_t *alphabet = malloc(n_unique * sizeof(uint16_t));
    uint32_t *alphabet_freqs = malloc(n_unique * sizeof(uint32_t));
    if (!alphabet || !alphabet_freqs) {
        free(alphabet);
        free(alphabet_freqs);
        return -1;
    }

    for (uint32_t i = 0; i < n_unique; i++) {
        uint32_t val;
        if (br_read_bits(&br, bits_per_value, &val) < 0) {
            free(alphabet);
            free(alphabet_freqs);
            return -1;
        }
        alphabet[i] = val;
    }

    /* Read frequency table */
    for (uint32_t i = 0; i < n_unique; i++) {
        uint32_t freq;
        if (br_read_bits(&br, 10, &freq) < 0) {
            free(alphabet);
            free(alphabet_freqs);
            return -1;
        }
        alphabet_freqs[i] = freq;
    }

    /* Align before rANS */
    br_align(&br);

    /* Decode all values with rANS */
    rans_decoder_t rans;
    if (rans_init_decoder(&rans, alphabet_freqs, n_unique, &br) < 0) {
        free(alphabet);
        free(alphabet_freqs);
        return -1;
    }

    uint32_t total_coeffs = (uint32_t)num_vectors * dimension;
    uint32_t coeff_idx = 0;

    for (uint16_t v = 0; v < num_vectors; v++) {
        for (uint16_t i = 0; i < dimension; i++) {
            int sym_idx;
            int is_last = (coeff_idx == total_coeffs - 1);
            if (rans_decode_symbol(&rans, &sym_idx, is_last) < 0) {
                rans_free_decoder(&rans);
                free(alphabet);
                free(alphabet_freqs);
                return -1;
            }
            vectors[v][i] = alphabet[sym_idx];
            coeff_idx++;
        }
    }

    rans_free_decoder(&rans);
    free(alphabet);
    free(alphabet_freqs);

    return 0;
}

void dense_batch_free(dense_batch_t *encoded) {
    if (encoded) {
        free(encoded->data);
        free(encoded);
    }
}
