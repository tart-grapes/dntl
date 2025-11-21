/**
 * sparse_phase2.c
 *
 * Phase 2 optimization: Delta alphabet + rANS
 * - Rice gaps for positions
 * - rANS (Asymmetric Numeral Systems) for values
 * - Delta-encoded alphabet
 * - Frequency table stored in bitstream
 * Target: ~162 bytes for 97 nz vectors
 */

#include "sparse_phase2.h"
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
    /* Pad to next byte boundary */
    if (bw->bit_pos > 0) {
        bw->byte_pos++;
        bw->bit_pos = 0;
    }
}

static int bw_write_byte(bit_writer_t *bw, uint8_t byte) {
    /* Write a full byte (must be byte-aligned) */
    if (bw->bit_pos != 0) return -1;  /* Must be byte-aligned */
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
    /* Skip to next byte boundary */
    if (br->bit_pos > 0) {
        br->byte_pos++;
        br->bit_pos = 0;
    }
}

static int br_read_byte(bit_reader_t *br, uint8_t *out) {
    /* Read a full byte (must be byte-aligned) */
    if (br->bit_pos != 0) return -1;  /* Must be byte-aligned */
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

/* rANS (range Asymmetric Numeral Systems) for values */
#define ANS_L 65536  /* Lower bound for state (must be >> freq total of 4096) */

typedef struct {
    uint32_t state;
    uint32_t *freq;
    uint32_t *cumul;
    uint32_t total;
    int n_symbols;
    bit_writer_t *bw;
    /* Buffer to collect all rANS bytes for reversing */
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
    /* Buffer for backwards reading */
    uint8_t *rans_buffer;
    int rans_pos;  /* Current read position (decrements) */
} rans_decoder_t;

/* Normalize frequencies to fit in 12 bits (max sum = 4096) */
static void normalize_freqs(uint32_t *freqs, int n_symbols, uint32_t *norm_freqs) {
    uint32_t total = 0;
    for (int i = 0; i < n_symbols; i++) {
        total += freqs[i];
    }

    /* Target total: 4096 (12 bits) */
    uint32_t target = 4096;
    uint64_t sum = 0;

    for (int i = 0; i < n_symbols; i++) {
        uint64_t scaled = ((uint64_t)freqs[i] * target) / total;
        if (scaled == 0) scaled = 1;  /* Ensure non-zero */
        norm_freqs[i] = (uint32_t)scaled;
        sum += scaled;
    }

    /* Adjust for rounding errors */
    while (sum > target) {
        /* Find largest frequency and decrease */
        int max_idx = 0;
        for (int i = 1; i < n_symbols; i++) {
            if (norm_freqs[i] > norm_freqs[max_idx]) max_idx = i;
        }
        norm_freqs[max_idx]--;
        sum--;
    }
    while (sum < target) {
        /* Find largest frequency and increase */
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

    /* Normalize frequencies */
    enc->freq = malloc(n_symbols * sizeof(uint32_t));
    normalize_freqs(freqs, n_symbols, enc->freq);

    /* Build cumulative frequencies */
    enc->cumul = malloc((n_symbols + 1) * sizeof(uint32_t));
    enc->cumul[0] = 0;
    enc->total = 0;
    for (int i = 0; i < n_symbols; i++) {
        enc->total += enc->freq[i];
        enc->cumul[i + 1] = enc->total;
    }

    /* Allocate buffer for collecting rANS bytes (conservative: 20 bytes per symbol + state) */
    enc->rans_capacity = max_symbols * 20 + 4;
    enc->rans_buffer = malloc(enc->rans_capacity);
    enc->rans_pos = 0;
}

static int rans_encode_symbol(rans_encoder_t *enc, int symbol) {
    uint32_t freq = enc->freq[symbol];
    uint32_t cumul = enc->cumul[symbol];

    fprintf(stderr, "DEBUG encode: sym=%d, freq=%u, cumul=%u, state_in=0x%08x\n",
            symbol, freq, cumul, enc->state);

    /* Renormalize: keep state below threshold - write to buffer */
    /* Formula: max_state = ((ANS_L << 8) / freq) * total */
    /* This ensures state stays >= ANS_L after outputting a byte */
    uint32_t max_state = ((ANS_L << 8) / freq) * enc->total;
    while (enc->state >= max_state) {
        if (enc->rans_pos >= enc->rans_capacity) return -1;
        uint8_t byte = enc->state & 0xFF;
        enc->rans_buffer[enc->rans_pos++] = byte;
        fprintf(stderr, "DEBUG encode: buffer renorm byte 0x%02x\n", byte);
        enc->state >>= 8;
    }

    /* Encode: state = total * (state / freq) + cumul + (state % freq) */
    enc->state = enc->total * (enc->state / freq) + cumul + (enc->state % freq);
    fprintf(stderr, "DEBUG encode: state_out=0x%08x\n", enc->state);
    return 0;
}

static int rans_flush(rans_encoder_t *enc) {
    fprintf(stderr, "DEBUG flush: Final state = 0x%08x, %zu rans bytes in buffer\n",
            enc->state, enc->rans_pos);

    /* Write renorm bytes in REVERSE order to main stream */
    fprintf(stderr, "DEBUG flush: Writing %zu renorm bytes in reverse\n", enc->rans_pos);
    for (int i = (int)enc->rans_pos - 1; i >= 0; i--) {
        if (bw_write_byte(enc->bw, enc->rans_buffer[i]) < 0) {
            free(enc->freq);
            free(enc->cumul);
            free(enc->rans_buffer);
            return -1;
        }
    }

    /* Write final state in NORMAL byte order (little-endian, 4 bytes) */
    fprintf(stderr, "DEBUG flush: Writing final state 0x%08x in normal order\n", enc->state);
    for (int i = 0; i < 4; i++) {
        uint8_t byte = (enc->state >> (i * 8)) & 0xFF;
        fprintf(stderr, "DEBUG flush: state byte[%d] = 0x%02x\n", i, byte);
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

    /* Normalize frequencies (same as encoder) */
    dec->freq = malloc(n_symbols * sizeof(uint32_t));
    normalize_freqs(freqs, n_symbols, dec->freq);

    /* Build cumulative frequencies */
    dec->cumul = malloc((n_symbols + 1) * sizeof(uint32_t));
    dec->cumul[0] = 0;
    dec->total = 0;
    for (int i = 0; i < n_symbols; i++) {
        dec->total += dec->freq[i];
        dec->cumul[i + 1] = dec->total;
    }

    /* Read ALL remaining bytes into buffer for backwards reading */
    size_t remaining = br->size - br->byte_pos;
    fprintf(stderr, "DEBUG init_decoder: Reading %zu bytes into buffer\n", remaining);
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

    /* Read initial state from end (4 bytes, little-endian) */
    if (remaining < 4) {
        free(dec->freq);
        free(dec->cumul);
        free(dec->rans_buffer);
        return -1;
    }

    dec->state = 0;
    size_t state_pos = remaining - 4;  /* State is last 4 bytes */
    for (int i = 0; i < 4; i++) {
        dec->state |= ((uint32_t)dec->rans_buffer[state_pos + i] << (i * 8));
    }
    fprintf(stderr, "DEBUG init_decoder: Initial state = 0x%08x\n", dec->state);

    /* Start reading renorm bytes backwards from position before state */
    dec->rans_pos = (int)state_pos - 1;

    return 0;
}

static int rans_decode_symbol(rans_decoder_t *dec, int *symbol, int skip_renorm) {
    /* Find symbol from state % total */
    uint32_t slot = dec->state % dec->total;

    fprintf(stderr, "DEBUG decode_symbol: state_in=0x%08x, slot=%u, total=%u\n",
            dec->state, slot, dec->total);

    *symbol = 0;
    for (int i = 0; i < dec->n_symbols; i++) {
        if (slot < dec->cumul[i + 1]) {
            *symbol = i;
            break;
        }
    }

    uint32_t freq = dec->freq[*symbol];
    uint32_t cumul = dec->cumul[*symbol];

    fprintf(stderr, "DEBUG decode_symbol: found sym=%d, freq=%u, cumul=%u\n",
            *symbol, freq, cumul);

    /* Decode: state = freq * (state / total) + (slot - cumul) */
    uint32_t old_state = dec->state;
    dec->state = freq * (dec->state / dec->total) + (slot - cumul);

    fprintf(stderr, "DEBUG decode_symbol: state 0x%08x -> 0x%08x\n",
            old_state, dec->state);

    /* Renormalize: bring state back to [ANS_L, ...) - read backwards from buffer */
    if (!skip_renorm) {
        while (dec->state < ANS_L) {
            fprintf(stderr, "DEBUG decode_symbol: renorm needed, state=0x%08x < 0x%08x\n",
                    dec->state, ANS_L);
            if (dec->rans_pos < 0) {
                fprintf(stderr, "DEBUG decode_symbol: renorm read FAILED (no more bytes)\n");
                return -1;
            }
            uint8_t byte = dec->rans_buffer[dec->rans_pos--];
            fprintf(stderr, "DEBUG decode_symbol: renorm read byte 0x%02x from pos %d\n",
                    byte, dec->rans_pos + 1);
            dec->state = (dec->state << 8) | byte;
            fprintf(stderr, "DEBUG decode_symbol: renorm state now 0x%08x\n", dec->state);
        }
    } else {
        fprintf(stderr, "DEBUG decode_symbol: skipping renorm (last symbol)\n");
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

sparse_phase2_t* sparse_phase2_encode(const int8_t *vector, size_t dimension) {
    if (!vector || dimension == 0 || dimension > 65535) return NULL;

    /* Find non-zeros and build frequency table */
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

            /* Track min/max for delta encoding */
            if (vector[i] < min_val) min_val = vector[i];
            if (vector[i] > max_val) max_val = vector[i];

            count++;
        }
    }

    if (count == 0) {
        sparse_phase2_t *result = malloc(sizeof(sparse_phase2_t));
        result->data = calloc(2, 1);
        result->size = 2;
        result->count = 0;
        free(positions);
        free(vals);
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

    /* Build value-to-index map for rANS */
    int8_t value_to_idx[256];
    memset(value_to_idx, -1, sizeof(value_to_idx));
    for (int i = 0; i < n_unique; i++) {
        value_to_idx[(uint8_t)(alphabet[i] + 128)] = i;
    }

    /* Allocate result */
    sparse_phase2_t *result = malloc(sizeof(sparse_phase2_t));
    if (!result) {
        free(positions);
        free(vals);
        return NULL;
    }

    int range = max_val - min_val + 1;
    /* Conservative buffer size estimate:
     * Header: 40 bits
     * Bitfield: range bits
     * Freq table: n_unique * 12 bits
     * Positions: count * 15 bits (conservative)
     * rANS values: count * 13 bits (conservative) + 32 bits state
     */
    size_t max_size = 10 + (range + 7) / 8 + (n_unique * 12 + 7) / 8 + count * 4;
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

    /* Header: count(16), min_val(8), max_val(8) */
    if (bw_write_bits(&bw, count, 16) < 0) goto error;
    if (bw_write_bits(&bw, (uint8_t)(min_val + 128), 8) < 0) goto error;
    if (bw_write_bits(&bw, (uint8_t)(max_val + 128), 8) < 0) goto error;

    /* Delta-encoded alphabet: bitfield for [min_val, max_val] */
    for (int v = min_val; v <= max_val; v++) {
        int present = (value_freqs[(uint8_t)(v + 128)] > 0) ? 1 : 0;
        if (bw_write_bit(&bw, present) < 0) goto error;
    }

    /* Store frequency table (normalized to 12 bits each) */
    uint32_t norm_freqs[256];
    normalize_freqs(alphabet_freqs, n_unique, norm_freqs);
    for (int i = 0; i < n_unique; i++) {
        if (bw_write_bits(&bw, norm_freqs[i], 12) < 0) goto error;
    }

    /* Rice parameter */
    uint8_t r = count > 0 ? (uint8_t)(dimension / count >= 16 ? 4 : 3) : 4;
    if (bw_write_bits(&bw, r, 3) < 0) goto error;

    /* First position */
    if (count > 0 && bw_write_bits(&bw, positions[0], 11) < 0) goto error;

    /* Position gaps */
    for (uint16_t i = 1; i < count; i++) {
        uint16_t gap = positions[i] - positions[i-1] - 1;
        if (bw_write_rice(&bw, gap, r) < 0) goto error;
    }

    /* Align to byte boundary before rANS stream */
    bw_align(&bw);
    fprintf(stderr, "DEBUG: Byte-aligned before rANS at pos=%zu\n", bw.byte_pos);

    /* Encode values with rANS (in REVERSE order) */
    rans_encoder_t rans;
    rans_init_encoder(&rans, alphabet_freqs, n_unique, &bw, count);

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

int sparse_phase2_decode(const sparse_phase2_t *encoded,
                         int8_t *vector, size_t dimension) {
    if (!encoded || !encoded->data || !vector) return -1;

    memset(vector, 0, dimension * sizeof(int8_t));

    bit_reader_t br;
    br_init(&br, encoded->data, encoded->size);

    /* Read header: count(16), min_val(8), max_val(8) */
    uint32_t count, min_val_enc, max_val_enc;
    if (br_read_bits(&br, 16, &count) < 0) return -1;
    if (br_read_bits(&br, 8, &min_val_enc) < 0) return -1;
    if (br_read_bits(&br, 8, &max_val_enc) < 0) return -1;

    if (count == 0) return 0;

    int8_t min_val = (int8_t)min_val_enc - 128;
    int8_t max_val = (int8_t)max_val_enc - 128;

    /* Read delta-encoded alphabet bitfield */
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

    if (n_unique == 0 || n_unique > 256) return -1;

    /* Read frequency table */
    for (int i = 0; i < n_unique; i++) {
        uint32_t freq;
        if (br_read_bits(&br, 12, &freq) < 0) return -1;
        alphabet_freqs[i] = freq;
    }

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

    /* Align to byte boundary before rANS stream */
    br_align(&br);
    fprintf(stderr, "DEBUG: Byte-aligned before rANS decode at pos=%zu\n", br.byte_pos);

    /* Decode values with rANS */
    fprintf(stderr, "DEBUG decode: count=%u, n_unique=%d\n", count, n_unique);
    fprintf(stderr, "DEBUG decode: alphabet_freqs: ");
    for (int i = 0; i < n_unique && i < 5; i++) {
        fprintf(stderr, "%u ", alphabet_freqs[i]);
    }
    fprintf(stderr, "\n");

    rans_decoder_t rans;
    if (rans_init_decoder(&rans, alphabet_freqs, n_unique, &br) < 0) {
        fprintf(stderr, "DEBUG decode: rans_init_decoder failed\n");
        free(positions);
        return -1;
    }
    fprintf(stderr, "DEBUG decode: Initial state = 0x%08x, total=%u\n", rans.state, rans.total);

    for (uint32_t i = 0; i < count; i++) {
        int sym_idx;
        int is_last = (i == count - 1);  /* Skip renorm on last symbol */
        fprintf(stderr, "DEBUG decode [%u]: state=0x%08x ", i, rans.state);
        if (rans_decode_symbol(&rans, &sym_idx, is_last) < 0) {
            fprintf(stderr, "FAILED\n");
            rans_free_decoder(&rans);
            free(positions);
            return -1;
        }
        fprintf(stderr, "-> sym=%d (val=%d), new_state=0x%08x\n",
                sym_idx, alphabet[sym_idx], rans.state);
        vector[positions[i]] = alphabet[sym_idx];
    }

    rans_free_decoder(&rans);

    free(positions);
    return 0;
}

void sparse_phase2_free(sparse_phase2_t *encoded) {
    if (encoded) {
        free(encoded->data);
        free(encoded);
    }
}
