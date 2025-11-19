/**
 * sparse_optimal.c
 *
 * Optimized sparse vector encoder using bit-packed format
 *
 * Format: [count:16] [pos:11, val:2]*
 * - count: number of non-zeros (0-2048)
 * - pos: 11-bit position index (0-2047)
 * - val: 2-bit value encoding:
 *   00 = -2
 *   01 = -1
 *   10 = +1
 *   11 = +2
 *
 * Total: 16 + k*13 bits = (16 + k*13 + 7) / 8 bytes
 * For k=145: (16 + 1885 + 7) / 8 = 238 bytes
 */

#include "sparse_optimal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Bit writer helper */
typedef struct {
    uint8_t *buffer;
    size_t byte_pos;
    uint8_t bit_pos;  /* 0-7, next bit to write */
} bit_writer_t;

static void bit_writer_init(bit_writer_t *bw, uint8_t *buffer) {
    bw->buffer = buffer;
    bw->byte_pos = 0;
    bw->bit_pos = 0;
}

static void bit_writer_write(bit_writer_t *bw, uint32_t value, uint8_t num_bits) {
    for (int i = num_bits - 1; i >= 0; i--) {
        uint8_t bit = (value >> i) & 1;
        bw->buffer[bw->byte_pos] |= (bit << (7 - bw->bit_pos));
        bw->bit_pos++;
        if (bw->bit_pos == 8) {
            bw->bit_pos = 0;
            bw->byte_pos++;
        }
    }
}

static size_t bit_writer_finish(bit_writer_t *bw) {
    return bw->byte_pos + (bw->bit_pos > 0 ? 1 : 0);
}

/* Bit reader helper */
typedef struct {
    const uint8_t *buffer;
    size_t byte_pos;
    uint8_t bit_pos;  /* 0-7, next bit to read */
} bit_reader_t;

static void bit_reader_init(bit_reader_t *br, const uint8_t *buffer) {
    br->buffer = buffer;
    br->byte_pos = 0;
    br->bit_pos = 0;
}

static uint32_t bit_reader_read(bit_reader_t *br, uint8_t num_bits) {
    uint32_t value = 0;
    for (uint8_t i = 0; i < num_bits; i++) {
        uint8_t bit = (br->buffer[br->byte_pos] >> (7 - br->bit_pos)) & 1;
        value = (value << 1) | bit;
        br->bit_pos++;
        if (br->bit_pos == 8) {
            br->bit_pos = 0;
            br->byte_pos++;
        }
    }
    return value;
}

/* Encode value {-2,-1,+1,+2} to 2-bit code */
static uint8_t encode_value(int8_t val) {
    switch (val) {
        case -2: return 0;
        case -1: return 1;
        case  1: return 2;
        case  2: return 3;
        default: return 0;  /* Should not happen for valid input */
    }
}

/* Decode 2-bit code to value */
static int8_t decode_value(uint8_t code) {
    const int8_t values[] = {-2, -1, 1, 2};
    return values[code & 3];
}

sparse_encoded_t* sparse_encode(const int8_t *vector, size_t dimension) {
    if (!vector || dimension == 0 || dimension > 2048) {
        return NULL;
    }

    /* Count non-zeros and allocate temp storage */
    uint16_t count = 0;
    for (size_t i = 0; i < dimension; i++) {
        if (vector[i] != 0) count++;
    }

    /* Allocate result */
    sparse_encoded_t *result = malloc(sizeof(sparse_encoded_t));
    if (!result) return NULL;

    /* Calculate maximum size: 16 bits header + count * 13 bits */
    size_t max_size = (16 + count * 13 + 7) / 8;
    result->data = calloc(max_size, 1);
    if (!result->data) {
        free(result);
        return NULL;
    }
    result->count = count;

    /* Write header and entries */
    bit_writer_t bw;
    bit_writer_init(&bw, result->data);

    /* Write count */
    bit_writer_write(&bw, count, 16);

    /* Write (position, value) pairs */
    for (size_t i = 0; i < dimension; i++) {
        if (vector[i] != 0) {
            bit_writer_write(&bw, i, 11);               /* Position (11 bits) */
            bit_writer_write(&bw, encode_value(vector[i]), 2);  /* Value (2 bits) */
        }
    }

    result->size = bit_writer_finish(&bw);
    return result;
}

int sparse_decode(const sparse_encoded_t *encoded, int8_t *vector, size_t dimension) {
    if (!encoded || !encoded->data || !vector || dimension == 0) {
        return -1;
    }

    /* Clear output vector */
    memset(vector, 0, dimension * sizeof(int8_t));

    /* Read encoded data */
    bit_reader_t br;
    bit_reader_init(&br, encoded->data);

    /* Read count */
    uint16_t count = bit_reader_read(&br, 16);
    if (count != encoded->count) {
        return -1;  /* Corrupted data */
    }

    /* Read (position, value) pairs */
    for (uint16_t i = 0; i < count; i++) {
        uint16_t pos = bit_reader_read(&br, 11);
        uint8_t val_code = bit_reader_read(&br, 2);

        if (pos >= dimension) {
            return -1;  /* Invalid position */
        }

        vector[pos] = decode_value(val_code);
    }

    return 0;
}

void sparse_free(sparse_encoded_t *encoded) {
    if (encoded) {
        free(encoded->data);
        free(encoded);
    }
}

void sparse_stats(const int8_t *vector, size_t dimension,
                  uint16_t *k, float *l2_norm, float *entropy) {
    /* Count values */
    uint32_t counts[5] = {0};  /* -2, -1, 0, +1, +2 */
    uint64_t sum_sq = 0;
    uint16_t nonzero = 0;

    for (size_t i = 0; i < dimension; i++) {
        int8_t v = vector[i];
        sum_sq += v * v;
        if (v != 0) nonzero++;

        /* Count for entropy */
        switch (v) {
            case -2: counts[0]++; break;
            case -1: counts[1]++; break;
            case  0: counts[2]++; break;
            case  1: counts[3]++; break;
            case  2: counts[4]++; break;
        }
    }

    if (k) *k = nonzero;
    if (l2_norm) *l2_norm = sqrtf((float)sum_sq);

    /* Calculate entropy over non-zero values only */
    if (entropy && nonzero > 0) {
        float ent = 0.0f;
        for (int i = 0; i < 5; i++) {
            if (i == 2) continue;  /* Skip zero */
            if (counts[i] > 0) {
                float p = (float)counts[i] / nonzero;
                ent -= p * log2f(p);
            }
        }
        *entropy = ent;
    }
}
