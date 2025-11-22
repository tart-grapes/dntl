/**
 * sparse_batch.c
 *
 * Batch encoding: amortizes rANS overhead across multiple vectors
 * - Single shared frequency table
 * - Single rANS stream for all values
 * - Per-vector positions
 */

#include "sparse_batch.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Bit writer/reader (same as Phase 2) */
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
    for (uint32_t i = 0; i < q; i++) {
        if (bw_write_bit(bw, 1) < 0) return -1;
    }
    if (bw_write_bit(bw, 0) < 0) return -1;
    return bw_write_bits(bw, rem, r);
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

/* rANS (same as Phase 2 with 8-bit frequencies) */
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

    uint32_t target = 256;
    uint64_t sum = 0;

    for (int i = 0; i < n_symbols; i++) {
        uint64_t scaled = ((uint64_t)freqs[i] * target) / total;
        if (scaled == 0) scaled = 1;
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

static void rans_init_encoder(rans_encoder_t *enc, uint32_t *freqs, int n_symbols, bit_writer_t *bw, size_t max_symbols) {
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

    uint32_t max_state = ((ANS_L >> 8) << 8) * freq;
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

/* Batch encoding */
sparse_batch_t* sparse_batch_encode(const int8_t **vectors,
                                     uint16_t num_vectors,
                                     size_t dimension) {
    if (!vectors || num_vectors == 0 || dimension == 0 || dimension > 65535) return NULL;

    /* Collect all non-zeros and build global frequency table */
    uint32_t value_freqs[256] = {0};
    int8_t min_val = 127, max_val = -128;
    uint32_t global_count = 0;

    /* First pass: count frequencies and find min/max */
    for (uint16_t v = 0; v < num_vectors; v++) {
        for (size_t i = 0; i < dimension; i++) {
            if (vectors[v][i] != 0) {
                value_freqs[(uint8_t)(vectors[v][i] + 128)]++;
                if (vectors[v][i] < min_val) min_val = vectors[v][i];
                if (vectors[v][i] > max_val) max_val = vectors[v][i];
                global_count++;
            }
        }
    }

    if (global_count == 0) {
        sparse_batch_t *result = malloc(sizeof(sparse_batch_t));
        result->data = calloc(4, 1);
        result->size = 4;
        result->num_vectors = num_vectors;
        result->dimension = dimension;
        return result;
    }

    /* Build alphabet */
    int8_t alphabet[256];
    uint32_t alphabet_freqs[256];
    int n_unique = 0;
    for (int v = -128; v <= 127; v++) {
        if (value_freqs[(uint8_t)(v + 128)] > 0) {
            alphabet[n_unique] = v;
            alphabet_freqs[n_unique] = value_freqs[(uint8_t)(v + 128)];
            n_unique++;
        }
    }

    /* Value-to-index map */
    int8_t value_to_idx[256];
    memset(value_to_idx, -1, sizeof(value_to_idx));
    for (int i = 0; i < n_unique; i++) {
        value_to_idx[(uint8_t)(alphabet[i] + 128)] = i;
    }

    /* Allocate result */
    sparse_batch_t *result = malloc(sizeof(sparse_batch_t));
    if (!result) return NULL;

    result->num_vectors = num_vectors;
    result->dimension = dimension;

    int range = max_val - min_val + 1;
    size_t max_size = 20 + (range + 7) / 8 + n_unique +
                      num_vectors * 10 + global_count * 2;
    result->data = calloc(max_size, 1);
    if (!result->data) {
        free(result);
        return NULL;
    }

    /* Encode */
    bit_writer_t bw;
    bw_init(&bw, result->data, max_size);

    /* Header */
    if (bw_write_bits(&bw, num_vectors, 16) < 0) goto error;
    if (bw_write_bits(&bw, dimension, 16) < 0) goto error;
    if (bw_write_bits(&bw, global_count, 16) < 0) goto error;
    if (bw_write_bits(&bw, (uint8_t)(min_val + 128), 8) < 0) goto error;
    if (bw_write_bits(&bw, (uint8_t)(max_val + 128), 8) < 0) goto error;

    /* Delta-encoded alphabet */
    for (int v = min_val; v <= max_val; v++) {
        int present = (value_freqs[(uint8_t)(v + 128)] > 0) ? 1 : 0;
        if (bw_write_bit(&bw, present) < 0) goto error;
    }

    /* Frequency table (8 bits each) */
    uint32_t norm_freqs[256];
    normalize_freqs(alphabet_freqs, n_unique, norm_freqs);
    for (int i = 0; i < n_unique; i++) {
        if (bw_write_bits(&bw, norm_freqs[i], 8) < 0) goto error;
    }

    /* Rice parameter */
    uint8_t r = global_count > 0 ?
                (uint8_t)(dimension * num_vectors / global_count >= 16 ? 4 : 3) : 4;
    if (bw_write_bits(&bw, r, 3) < 0) goto error;

    /* Encode positions for each vector */
    for (uint16_t v = 0; v < num_vectors; v++) {
        /* Count non-zeros */
        uint16_t count = 0;
        for (size_t i = 0; i < dimension; i++) {
            if (vectors[v][i] != 0) count++;
        }

        if (bw_write_bits(&bw, count, 16) < 0) goto error;

        if (count > 0) {
            /* Find positions */
            uint16_t *positions = malloc(count * sizeof(uint16_t));
            if (!positions) goto error;

            uint16_t pos_idx = 0;
            for (size_t i = 0; i < dimension; i++) {
                if (vectors[v][i] != 0) {
                    positions[pos_idx++] = i;
                }
            }

            /* Encode first position */
            if (bw_write_bits(&bw, positions[0], 11) < 0) {
                free(positions);
                goto error;
            }

            /* Encode gaps */
            for (uint16_t i = 1; i < count; i++) {
                uint16_t gap = positions[i] - positions[i-1] - 1;
                if (bw_write_rice(&bw, gap, r) < 0) {
                    free(positions);
                    goto error;
                }
            }

            free(positions);
        }
    }

    /* Align before rANS */
    bw_align(&bw);

    /* Encode ALL values with rANS (in reverse order) */
    rans_encoder_t rans;
    rans_init_encoder(&rans, alphabet_freqs, n_unique, &bw, global_count);

    /* Encode values in reverse: last vector to first */
    for (int v = num_vectors - 1; v >= 0; v--) {
        /* Encode values in reverse within each vector */
        for (int i = (int)dimension - 1; i >= 0; i--) {
            if (vectors[v][i] != 0) {
                int idx = value_to_idx[(uint8_t)(vectors[v][i] + 128)];
                if (idx < 0 || rans_encode_symbol(&rans, idx) < 0) goto error;
            }
        }
    }

    if (rans_flush(&rans) < 0) goto error;

    result->size = bw_finish(&bw);
    return result;

error:
    free(result->data);
    free(result);
    return NULL;
}

int sparse_batch_decode(const sparse_batch_t *encoded,
                        int8_t **vectors,
                        uint16_t num_vectors,
                        size_t dimension) {
    if (!encoded || !encoded->data || !vectors) return -1;

    bit_reader_t br;
    br_init(&br, encoded->data, encoded->size);

    /* Read header */
    uint32_t num_vecs_enc, dim_enc, global_count, min_val_enc, max_val_enc;
    if (br_read_bits(&br, 16, &num_vecs_enc) < 0) return -1;
    if (br_read_bits(&br, 16, &dim_enc) < 0) return -1;
    if (br_read_bits(&br, 16, &global_count) < 0) return -1;
    if (br_read_bits(&br, 8, &min_val_enc) < 0) return -1;
    if (br_read_bits(&br, 8, &max_val_enc) < 0) return -1;

    if (num_vecs_enc != num_vectors || dim_enc != dimension) return -1;

    /* Zero all vectors */
    for (uint16_t v = 0; v < num_vectors; v++) {
        memset(vectors[v], 0, dimension * sizeof(int8_t));
    }

    if (global_count == 0) return 0;

    int8_t min_val = (int8_t)min_val_enc - 128;
    int8_t max_val = (int8_t)max_val_enc - 128;

    /* Read alphabet */
    int8_t alphabet[256];
    uint32_t alphabet_freqs[256];
    int n_unique = 0;

    for (int v = min_val; v <= max_val; v++) {
        int bit = br_read_bit(&br);
        if (bit < 0) return -1;
        if (bit) {
            alphabet[n_unique] = v;
            n_unique++;
        }
    }

    /* Read frequency table */
    for (int i = 0; i < n_unique; i++) {
        uint32_t freq;
        if (br_read_bits(&br, 8, &freq) < 0) return -1;
        alphabet_freqs[i] = freq;
    }

    /* Read Rice parameter */
    uint32_t r;
    if (br_read_bits(&br, 3, &r) < 0) return -1;

    /* Read positions for each vector */
    uint16_t **all_positions = malloc(num_vectors * sizeof(uint16_t*));
    uint16_t *all_counts = malloc(num_vectors * sizeof(uint16_t));
    if (!all_positions || !all_counts) {
        free(all_positions);
        free(all_counts);
        return -1;
    }

    for (uint16_t v = 0; v < num_vectors; v++) {
        uint32_t count;
        if (br_read_bits(&br, 16, &count) < 0) {
            for (uint16_t j = 0; j < v; j++) free(all_positions[j]);
            free(all_positions);
            free(all_counts);
            return -1;
        }
        all_counts[v] = count;

        if (count > 0) {
            all_positions[v] = malloc(count * sizeof(uint16_t));
            if (!all_positions[v]) {
                for (uint16_t j = 0; j < v; j++) free(all_positions[j]);
                free(all_positions);
                free(all_counts);
                return -1;
            }

            uint32_t pos;
            if (br_read_bits(&br, 11, &pos) < 0) {
                for (uint16_t j = 0; j <= v; j++) free(all_positions[j]);
                free(all_positions);
                free(all_counts);
                return -1;
            }
            all_positions[v][0] = pos;

            for (uint32_t i = 1; i < count; i++) {
                uint32_t gap;
                if (br_read_rice(&br, r, &gap) < 0) {
                    for (uint16_t j = 0; j <= v; j++) free(all_positions[j]);
                    free(all_positions);
                    free(all_counts);
                    return -1;
                }
                pos = pos + gap + 1;
                all_positions[v][i] = pos;
            }
        }
    }

    /* Align before rANS */
    br_align(&br);

    /* Decode ALL values with rANS */
    rans_decoder_t rans;
    if (rans_init_decoder(&rans, alphabet_freqs, n_unique, &br) < 0) {
        for (uint16_t j = 0; j < num_vectors; j++) {
            if (all_counts[j] > 0) free(all_positions[j]);
        }
        free(all_positions);
        free(all_counts);
        return -1;
    }

    /* Decode in forward order (encoded in reverse) */
    uint32_t values_decoded = 0;
    for (uint16_t v = 0; v < num_vectors; v++) {
        for (uint32_t i = 0; i < all_counts[v]; i++) {
            int sym_idx;
            int is_last = (values_decoded == global_count - 1);
            if (rans_decode_symbol(&rans, &sym_idx, is_last) < 0) {
                rans_free_decoder(&rans);
                for (uint16_t j = 0; j < num_vectors; j++) {
                    if (all_counts[j] > 0) free(all_positions[j]);
                }
                free(all_positions);
                free(all_counts);
                return -1;
            }
            vectors[v][all_positions[v][i]] = alphabet[sym_idx];
            values_decoded++;
        }
    }

    rans_free_decoder(&rans);

    for (uint16_t j = 0; j < num_vectors; j++) {
        if (all_counts[j] > 0) free(all_positions[j]);
    }
    free(all_positions);
    free(all_counts);

    return 0;
}

void sparse_batch_free(sparse_batch_t *encoded) {
    if (encoded) {
        free(encoded->data);
        free(encoded);
    }
}
