/**
 * sparse_rice.c
 *
 * Optimal sparse encoder using:
 * - Rice coding for position gaps (avg gap ~14)
 * - Huffman coding for values (1.88 bits/value)
 *
 * Target: ~110 bytes for k=145
 */

#include "sparse_rice.h"
#include <stdlib.h>
#include <string.h>

/* Bit writer */
typedef struct {
    uint8_t *buffer;
    size_t byte_pos;
    uint8_t bit_pos;
    size_t capacity;
} bit_writer_t;

static void bw_init(bit_writer_t *bw, uint8_t *buffer, size_t capacity) {
    bw->buffer = buffer;
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    bw->capacity = capacity;
}

static int bw_write_bit(bit_writer_t *bw, uint8_t bit) {
    if (bw->byte_pos >= bw->capacity) return -1;

    if (bit) {
        bw->buffer[bw->byte_pos] |= (1 << (7 - bw->bit_pos));
    }
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

/* Write Rice code: quotient in unary, remainder in r bits */
static int bw_write_rice(bit_writer_t *bw, uint32_t value, uint8_t r) {
    uint32_t q = value >> r;
    uint32_t rem = value & ((1 << r) - 1);

    /* Write q ones */
    for (uint32_t i = 0; i < q; i++) {
        if (bw_write_bit(bw, 1) < 0) return -1;
    }
    /* Write terminating zero */
    if (bw_write_bit(bw, 0) < 0) return -1;

    /* Write remainder */
    return bw_write_bits(bw, rem, r);
}

static size_t bw_finish(bit_writer_t *bw) {
    return bw->byte_pos + (bw->bit_pos > 0 ? 1 : 0);
}

/* Bit reader */
typedef struct {
    const uint8_t *buffer;
    size_t byte_pos;
    uint8_t bit_pos;
    size_t size;
} bit_reader_t;

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

/* Read Rice code */
static int br_read_rice(bit_reader_t *br, uint8_t r, uint32_t *out) {
    /* Count ones until we hit a zero */
    uint32_t q = 0;
    int bit;
    while ((bit = br_read_bit(br)) == 1) {
        q++;
        if (q > 1000) return -1;  /* Sanity check */
    }
    if (bit < 0) return -1;

    /* Read remainder */
    uint32_t rem;
    if (br_read_bits(br, r, &rem) < 0) return -1;

    *out = (q << r) | rem;
    return 0;
}

/* Huffman codes for 70% ±2, 30% ±1 distribution */
/* Optimal codes: -2:0, +2:10, -1:110, +1:111 */
static int encode_value_huffman(bit_writer_t *bw, int8_t val) {
    switch (val) {
        case -2: return bw_write_bit(bw, 0);
        case  2: return bw_write_bits(bw, 0b10, 2);
        case -1: return bw_write_bits(bw, 0b110, 3);
        case  1: return bw_write_bits(bw, 0b111, 3);
        default: return -1;
    }
}

static int decode_value_huffman(bit_reader_t *br, int8_t *val) {
    int bit = br_read_bit(br);
    if (bit < 0) return -1;

    if (bit == 0) {
        *val = -2;
        return 0;
    }

    /* Read second bit */
    bit = br_read_bit(br);
    if (bit < 0) return -1;

    if (bit == 0) {
        *val = 2;
        return 0;
    }

    /* Read third bit */
    bit = br_read_bit(br);
    if (bit < 0) return -1;

    *val = (bit == 0) ? -1 : 1;
    return 0;
}

sparse_rice_t* sparse_rice_encode(const int8_t *vector, size_t dimension) {
    if (!vector || dimension == 0 || dimension > 65535) {
        return NULL;
    }

    /* Find positions and count */
    uint16_t *positions = malloc(dimension * sizeof(uint16_t));
    int8_t *values = malloc(dimension * sizeof(int8_t));
    if (!positions || !values) {
        free(positions);
        free(values);
        return NULL;
    }

    uint16_t count = 0;
    for (size_t i = 0; i < dimension; i++) {
        if (vector[i] != 0) {
            positions[count] = i;
            values[count] = vector[i];
            count++;
        }
    }

    /* Allocate result */
    sparse_rice_t *result = malloc(sizeof(sparse_rice_t));
    if (!result) {
        free(positions);
        free(values);
        return NULL;
    }

    /* Allocate buffer (conservative estimate) */
    size_t max_size = 2 + count * 8;  /* Header + conservative estimate */
    result->data = calloc(max_size, 1);
    if (!result->data) {
        free(result);
        free(positions);
        free(values);
        return NULL;
    }
    result->count = count;

    /* Encode */
    bit_writer_t bw;
    bw_init(&bw, result->data, max_size);

    /* Write count (16 bits) */
    if (bw_write_bits(&bw, count, 16) < 0) goto error;

    /* Rice parameter for gaps (average gap ~14, so r=3 or r=4) */
    uint8_t r = 4;

    /* Write Rice parameter (3 bits: 0-7) */
    if (bw_write_bits(&bw, r, 3) < 0) goto error;

    /* Write first position directly (11 bits) */
    if (count > 0) {
        if (bw_write_bits(&bw, positions[0], 11) < 0) goto error;
    }

    /* Write gaps using Rice codes */
    for (uint16_t i = 1; i < count; i++) {
        uint16_t gap = positions[i] - positions[i-1] - 1;
        if (bw_write_rice(&bw, gap, r) < 0) goto error;
    }

    /* Write values using Huffman */
    for (uint16_t i = 0; i < count; i++) {
        if (encode_value_huffman(&bw, values[i]) < 0) goto error;
    }

    result->size = bw_finish(&bw);
    free(positions);
    free(values);
    return result;

error:
    free(result->data);
    free(result);
    free(positions);
    free(values);
    return NULL;
}

int sparse_rice_decode(const sparse_rice_t *encoded, int8_t *vector, size_t dimension) {
    if (!encoded || !encoded->data || !vector) {
        return -1;
    }

    memset(vector, 0, dimension * sizeof(int8_t));

    bit_reader_t br;
    br_init(&br, encoded->data, encoded->size);

    /* Read count */
    uint32_t count;
    if (br_read_bits(&br, 16, &count) < 0) return -1;
    if (count != encoded->count) return -1;

    /* Read Rice parameter */
    uint32_t r;
    if (br_read_bits(&br, 3, &r) < 0) return -1;

    /* Read first position */
    uint32_t pos = 0;
    if (count > 0) {
        if (br_read_bits(&br, 11, &pos) < 0) return -1;
        if (pos >= dimension) return -1;
    }

    /* Read positions using gaps */
    uint16_t *positions = malloc(count * sizeof(uint16_t));
    if (!positions) return -1;

    if (count > 0) {
        positions[0] = pos;

        for (uint16_t i = 1; i < count; i++) {
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
    }

    /* Read values using Huffman */
    for (uint16_t i = 0; i < count; i++) {
        int8_t val;
        if (decode_value_huffman(&br, &val) < 0) {
            free(positions);
            return -1;
        }
        vector[positions[i]] = val;
    }

    free(positions);
    return 0;
}

void sparse_rice_free(sparse_rice_t *encoded) {
    if (encoded) {
        free(encoded->data);
        free(encoded);
    }
}
