/**
 * dense_range.c
 *
 * Range-based encoding: split values into size classes
 * - Class 0 [0-3]:    2 bits class + 2 bits value = 4 bits total
 * - Class 1 [4-15]:   2 bits class + 4 bits value = 6 bits total
 * - Class 2 [16-255]: 2 bits class + 8 bits value = 10 bits total
 * - Class 3 [256+]:   2 bits class + full bits    = 2+ceil(log2(modulus)) bits
 *
 * For 82.8% small values (< 10), this gives ~5-6 bits/coeff average
 */

#include "dense_range.h"
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

dense_range_t* dense_range_encode(const uint16_t *vector,
                                   uint16_t dimension,
                                   uint16_t modulus) {
    if (!vector || dimension == 0 || modulus == 0) return NULL;

    /* Calculate bits needed for largest class */
    int max_bits = 0;
    uint16_t temp = modulus - 1;
    while (temp > 0) {
        max_bits++;
        temp >>= 1;
    }

    dense_range_t *result = malloc(sizeof(dense_range_t));
    if (!result) return NULL;

    result->dimension = dimension;
    result->modulus = modulus;

    /* Conservative estimate: worst case all values in class 3 */
    size_t max_size = 6 + dimension * (2 + max_bits + 7) / 8;
    result->data = calloc(max_size, 1);
    if (!result->data) {
        free(result);
        return NULL;
    }

    bit_writer_t bw;
    bw_init(&bw, result->data, max_size);

    /* Header */
    if (bw_write_bits(&bw, dimension, 16) < 0) goto error;
    if (bw_write_bits(&bw, modulus, 16) < 0) goto error;
    if (bw_write_bits(&bw, max_bits, 8) < 0) goto error;

    /* Encode each coefficient */
    for (uint16_t i = 0; i < dimension; i++) {
        uint16_t val = vector[i] % modulus;

        /* Determine size class and encode */
        if (val <= 3) {
            /* Class 0: [0-3] → 2 bits class + 2 bits value */
            if (bw_write_bits(&bw, 0, 2) < 0) goto error;  /* Class 0 */
            if (bw_write_bits(&bw, val, 2) < 0) goto error;
        } else if (val <= 15) {
            /* Class 1: [4-15] → 2 bits class + 4 bits value */
            if (bw_write_bits(&bw, 1, 2) < 0) goto error;  /* Class 1 */
            if (bw_write_bits(&bw, val - 4, 4) < 0) goto error;
        } else if (val <= 255) {
            /* Class 2: [16-255] → 2 bits class + 8 bits value */
            if (bw_write_bits(&bw, 2, 2) < 0) goto error;  /* Class 2 */
            if (bw_write_bits(&bw, val - 16, 8) < 0) goto error;
        } else {
            /* Class 3: [256+] → 2 bits class + max_bits value */
            if (bw_write_bits(&bw, 3, 2) < 0) goto error;  /* Class 3 */
            if (bw_write_bits(&bw, val - 256, max_bits) < 0) goto error;
        }
    }

    result->size = bw_finish(&bw);
    return result;

error:
    free(result->data);
    free(result);
    return NULL;
}

int dense_range_decode(const dense_range_t *encoded,
                       uint16_t *vector,
                       uint16_t dimension) {
    if (!encoded || !encoded->data || !vector) return -1;

    bit_reader_t br;
    br_init(&br, encoded->data, encoded->size);

    /* Read header */
    uint32_t dim_enc, mod_enc, max_bits;
    if (br_read_bits(&br, 16, &dim_enc) < 0) return -1;
    if (br_read_bits(&br, 16, &mod_enc) < 0) return -1;
    if (br_read_bits(&br, 8, &max_bits) < 0) return -1;

    if (dim_enc != dimension || mod_enc != encoded->modulus) return -1;

    /* Decode each coefficient */
    for (uint16_t i = 0; i < dimension; i++) {
        uint32_t class_id, val;

        /* Read class ID (2 bits) */
        if (br_read_bits(&br, 2, &class_id) < 0) return -1;

        /* Decode based on class */
        switch (class_id) {
            case 0:  /* [0-3] */
                if (br_read_bits(&br, 2, &val) < 0) return -1;
                vector[i] = val;
                break;
            case 1:  /* [4-15] */
                if (br_read_bits(&br, 4, &val) < 0) return -1;
                vector[i] = val + 4;
                break;
            case 2:  /* [16-255] */
                if (br_read_bits(&br, 8, &val) < 0) return -1;
                vector[i] = val + 16;
                break;
            case 3:  /* [256+] */
                if (br_read_bits(&br, max_bits, &val) < 0) return -1;
                vector[i] = val + 256;
                break;
        }
    }

    return 0;
}

void dense_range_free(dense_range_t *encoded) {
    if (encoded) {
        free(encoded->data);
        free(encoded);
    }
}
