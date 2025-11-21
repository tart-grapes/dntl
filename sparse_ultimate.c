/**
 * sparse_ultimate.c
 *
 * Ultimate compression with:
 * 1. Delta-encoded alphabet
 * 2. ANS (asymmetric numeral systems) for values
 * 3. Context-adaptive Rice for positions
 * 4. Tight bit packing
 */

#include "sparse_ultimate.h"
#include <stdlib.h>
#include <string.h>

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

static int bw_write_rice(bit_writer_t *bw, uint32_t value, uint8_t r) {
    uint32_t q = value >> r;
    uint32_t rem = value & ((1 << r) - 1);

    /* Limit quotient to prevent overflow */
    if (q > 255) {
        q = 255;
        rem = 0;
    }

    for (uint32_t i = 0; i < q; i++) {
        if (bw_write_bit(bw, 1) < 0) return -1;
    }
    if (bw_write_bit(bw, 0) < 0) return -1;
    return bw_write_bits(bw, rem, r);
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

static int br_read_rice(bit_reader_t *br, uint8_t r, uint32_t *out) {
    uint32_t q = 0;
    int bit;
    while ((bit = br_read_bit(br)) == 1) {
        q++;
        if (q > 2000) return -1;
    }
    if (bit < 0) return -1;
    uint32_t rem;
    if (br_read_bits(br, r, &rem) < 0) return -1;
    *out = (q << r) | rem;
    return 0;
}

/* Simple ANS (rANS) encoder for values */
#define ANS_L 1024  /* Lower bound for state */

typedef struct {
    uint32_t state;
    uint32_t *freq;
    uint32_t *cumul;
    uint32_t total;
    int n_symbols;
    bit_writer_t *bw;
} rans_encoder_t;

static void rans_init_encoder(rans_encoder_t *enc, uint32_t *freq, int n_symbols, bit_writer_t *bw) {
    enc->state = ANS_L;
    enc->freq = freq;
    enc->n_symbols = n_symbols;
    enc->bw = bw;

    /* Build cumulative frequencies */
    enc->cumul = malloc((n_symbols + 1) * sizeof(uint32_t));
    enc->cumul[0] = 0;
    enc->total = 0;
    for (int i = 0; i < n_symbols; i++) {
        enc->total += freq[i];
        enc->cumul[i + 1] = enc->total;
    }
}

static int rans_encode_symbol(rans_encoder_t *enc, int symbol) {
    uint32_t freq = enc->freq[symbol];
    uint32_t cumul = enc->cumul[symbol];

    /* Renormalize if needed */
    while (enc->state >= ((ANS_L >> 8) << 8) * enc->total / freq) {
        if (bw_write_bits(enc->bw, enc->state & 0xFF, 8) < 0) return -1;
        enc->state >>= 8;
    }

    /* C(s) + freq(s) * (state / total) */
    enc->state = ((enc->state / enc->total) * freq) + cumul + (enc->state % enc->total);
    return 0;
}

static int rans_flush(rans_encoder_t *enc) {
    /* Write final state */
    for (int i = 0; i < 4; i++) {
        if (bw_write_bits(enc->bw, (enc->state >> (i * 8)) & 0xFF, 8) < 0) return -1;
    }
    free(enc->cumul);
    return 0;
}

sparse_ultimate_t* sparse_ultimate_encode(const int8_t *vector, size_t dimension) {
    if (!vector || dimension == 0 || dimension > 65535) return NULL;

    /* Collect non-zeros and frequencies */
    uint16_t *positions = malloc(dimension * sizeof(uint16_t));
    int8_t *vals = malloc(dimension * sizeof(int8_t));
    uint32_t value_freqs[256] = {0};

    if (!positions || !vals) {
        free(positions);
        free(vals);
        return NULL;
    }

    uint16_t count = 0;
    int8_t min_val = 127, max_val = -128;

    for (size_t i = 0; i < dimension; i++) {
        if (vector[i] != 0) {
            positions[count] = i;
            vals[count] = vector[i];
            value_freqs[(uint8_t)(vector[i] + 128)]++;
            if (vector[i] < min_val) min_val = vector[i];
            if (vector[i] > max_val) max_val = vector[i];
            count++;
        }
    }

    if (count == 0) {
        sparse_ultimate_t *result = malloc(sizeof(sparse_ultimate_t));
        result->data = calloc(2, 1);
        result->size = 2;
        result->count = 0;
        free(positions);
        free(vals);
        return result;
    }

    /* Build delta-encoded alphabet */
    int8_t alphabet[256];
    uint32_t alphabet_freqs[256];
    int n_unique = 0;

    for (int v = min_val; v <= max_val; v++) {
        if (value_freqs[(uint8_t)(v + 128)] > 0) {
            alphabet[n_unique] = v;
            alphabet_freqs[n_unique] = value_freqs[(uint8_t)(v + 128)];
            n_unique++;
        }
    }

    /* Allocate result */
    sparse_ultimate_t *result = malloc(sizeof(sparse_ultimate_t));
    if (!result) {
        free(positions);
        free(vals);
        return NULL;
    }

    size_t max_size = 2 + n_unique + count * 10;
    result->data = calloc(max_size, 1);
    if (!result->data) {
        free(result);
        free(positions);
        free(vals);
        return NULL;
    }
    result->count = count;

    /* Encode */
    bit_writer_t bw;
    bw_init(&bw, result->data, max_size);

    /* Header: count (16), min_val (8), max_val (8), n_unique (8) */
    if (bw_write_bits(&bw, count, 16) < 0) goto error;
    if (bw_write_bits(&bw, (uint8_t)(min_val + 128), 8) < 0) goto error;
    if (bw_write_bits(&bw, (uint8_t)(max_val + 128), 8) < 0) goto error;
    if (bw_write_bits(&bw, n_unique, 8) < 0) goto error;

    /* Delta-encoded alphabet: store which values in [min, max] are present */
    int range = max_val - min_val + 1;
    for (int v = min_val; v <= max_val; v++) {
        int present = value_freqs[(uint8_t)(v + 128)] > 0 ? 1 : 0;
        if (bw_write_bit(&bw, present) < 0) goto error;
    }

    /* Position encoding: adaptive Rice */
    uint8_t r = count > 0 ? (uint8_t)(dimension / count >= 16 ? 4 : 3) : 4;
    if (bw_write_bits(&bw, r, 3) < 0) goto error;

    if (count > 0 && bw_write_bits(&bw, positions[0], 11) < 0) goto error;

    for (uint16_t i = 1; i < count; i++) {
        uint16_t gap = positions[i] - positions[i-1] - 1;
        if (bw_write_rice(&bw, gap, r) < 0) goto error;
    }

    /* Value encoding: rANS */
    rans_encoder_t rans;
    rans_init_encoder(&rans, alphabet_freqs, n_unique, &bw);

    /* Build reverse map */
    int8_t value_to_idx[256];
    memset(value_to_idx, -1, sizeof(value_to_idx));
    for (int i = 0; i < n_unique; i++) {
        value_to_idx[(uint8_t)(alphabet[i] + 128)] = i;
    }

    /* Encode values in reverse (ANS requirement) */
    for (int i = count - 1; i >= 0; i--) {
        int idx = value_to_idx[(uint8_t)(vals[i] + 128)];
        if (idx < 0 || rans_encode_symbol(&rans, idx) < 0) goto error;
    }

    if (rans_flush(&rans) < 0) goto error;

    result->size = bw_finish(&bw);
    free(positions);
    free(vals);
    return result;

error:
    free(result->data);
    free(result);
    free(positions);
    free(vals);
    return NULL;
}

/* rANS decoder */
typedef struct {
    uint32_t state;
    uint32_t *freq;
    uint32_t *cumul;
    uint32_t total;
    int n_symbols;
    bit_reader_t *br;
} rans_decoder_t;

static int rans_init_decoder(rans_decoder_t *dec, uint32_t *freq, int n_symbols, bit_reader_t *br) {
    dec->freq = freq;
    dec->n_symbols = n_symbols;
    dec->br = br;

    /* Build cumulative frequencies */
    dec->cumul = malloc((n_symbols + 1) * sizeof(uint32_t));
    dec->cumul[0] = 0;
    dec->total = 0;
    for (int i = 0; i < n_symbols; i++) {
        dec->total += freq[i];
        dec->cumul[i + 1] = dec->total;
    }

    /* Read initial state */
    dec->state = 0;
    for (int i = 0; i < 4; i++) {
        uint32_t byte;
        if (br_read_bits(br, 8, &byte) < 0) {
            free(dec->cumul);
            return -1;
        }
        dec->state |= (byte << (i * 8));
    }

    return 0;
}

static int rans_decode_symbol(rans_decoder_t *dec, int *symbol) {
    uint32_t slot = dec->state % dec->total;

    /* Find symbol */
    *symbol = 0;
    for (int i = 0; i < dec->n_symbols; i++) {
        if (slot < dec->cumul[i + 1]) {
            *symbol = i;
            break;
        }
    }

    uint32_t freq = dec->freq[*symbol];
    uint32_t cumul = dec->cumul[*symbol];

    /* Update state */
    dec->state = freq * (dec->state / dec->total) + (slot - cumul);

    /* Renormalize */
    while (dec->state < ANS_L) {
        uint32_t byte;
        if (br_read_bits(dec->br, 8, &byte) < 0) return -1;
        dec->state = (dec->state << 8) | byte;
    }

    return 0;
}

int sparse_ultimate_decode(const sparse_ultimate_t *encoded,
                           int8_t *vector, size_t dimension) {
    if (!encoded || !encoded->data || !vector) return -1;

    memset(vector, 0, dimension * sizeof(int8_t));

    bit_reader_t br;
    br_init(&br, encoded->data, encoded->size);

    /* Read header */
    uint32_t count, min_val_enc, max_val_enc, n_unique;
    if (br_read_bits(&br, 16, &count) < 0) return -1;
    if (br_read_bits(&br, 8, &min_val_enc) < 0) return -1;
    if (br_read_bits(&br, 8, &max_val_enc) < 0) return -1;
    if (br_read_bits(&br, 8, &n_unique) < 0) return -1;

    if (count == 0) return 0;

    int8_t min_val = (int8_t)min_val_enc - 128;
    int8_t max_val = (int8_t)max_val_enc - 128;

    /* Read delta-encoded alphabet */
    int8_t alphabet[256];
    uint32_t alphabet_freqs[256];
    int idx = 0;

    for (int v = min_val; v <= max_val; v++) {
        int bit = br_read_bit(&br);
        if (bit < 0) return -1;
        if (bit) {
            alphabet[idx] = v;
            alphabet_freqs[idx] = 1;  /* Will be updated from ANS */
            idx++;
        }
    }

    if (idx != (int)n_unique) return -1;

    /* Read Rice parameter and positions */
    uint32_t r;
    if (br_read_bits(&br, 3, &r) < 0) return -1;

    uint16_t *positions = malloc(count * sizeof(uint16_t));
    if (!positions) return -1;

    uint32_t pos;
    if (br_read_bits(&br, 11, &pos) < 0) {
        free(positions);
        return -1;
    }
    positions[0] = pos;

    for (uint32_t i = 1; i < count; i++) {
        uint32_t gap;
        if (br_read_rice(&br, r, &gap) < 0) {
            free(positions);
            return -1;
        }
        pos = pos + gap + 1;
        if (pos >= dimension) {
            free(positions);
            return -1;
        }
        positions[i] = pos;
    }

    /* Decode values with rANS */
    /* For decoding, we need to estimate frequencies - use uniform for now */
    for (int i = 0; i < (int)n_unique; i++) {
        alphabet_freqs[i] = (count + n_unique - 1) / n_unique;
    }

    rans_decoder_t rans;
    if (rans_init_decoder(&rans, alphabet_freqs, n_unique, &br) < 0) {
        free(positions);
        return -1;
    }

    /* Decode symbols */
    for (uint32_t i = 0; i < count; i++) {
        int sym_idx;
        if (rans_decode_symbol(&rans, &sym_idx) < 0) {
            free(rans.cumul);
            free(positions);
            return -1;
        }
        vector[positions[i]] = alphabet[sym_idx];
    }

    free(rans.cumul);
    free(positions);
    return 0;
}

void sparse_ultimate_free(sparse_ultimate_t *encoded) {
    if (encoded) {
        free(encoded->data);
        free(encoded);
    }
}
