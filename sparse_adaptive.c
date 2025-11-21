/**
 * sparse_adaptive.c
 *
 * Adaptive encoder: Rice gaps + optimal Huffman for actual value distribution
 */

#include "sparse_adaptive.h"
#include <stdlib.h>
#include <string.h>

/* Bit writer/reader (same as Rice encoder) */
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
        if (q > 1000) return -1;
    }
    if (bit < 0) return -1;
    uint32_t rem;
    if (br_read_bits(br, r, &rem) < 0) return -1;
    *out = (q << r) | rem;
    return 0;
}

/* Huffman tree building */
typedef struct huff_node {
    int8_t value;
    uint32_t freq;
    struct huff_node *left, *right;
} huff_node_t;

typedef struct {
    uint32_t code;
    uint8_t length;
} huff_code_t;

static int compare_nodes(const void *a, const void *b) {
    huff_node_t *na = *(huff_node_t**)a;
    huff_node_t *nb = *(huff_node_t**)b;
    return na->freq - nb->freq;
}

/* Build canonical Huffman codes from frequencies */
static void build_huffman_codes(uint32_t *freqs, int8_t *values, int n_values,
                                 huff_code_t *codes, int8_t *code_map) {
    if (n_values == 0) return;

    /* Special case: only one symbol */
    if (n_values == 1) {
        codes[0].code = 0;
        codes[0].length = 1;
        code_map[values[0] + 8] = 0;
        return;
    }

    /* Build Huffman tree */
    huff_node_t **nodes = malloc(n_values * 2 * sizeof(huff_node_t*));
    for (int i = 0; i < n_values; i++) {
        nodes[i] = malloc(sizeof(huff_node_t));
        nodes[i]->value = values[i];
        nodes[i]->freq = freqs[i];
        nodes[i]->left = nodes[i]->right = NULL;
    }

    int n_nodes = n_values;
    while (n_nodes > 1) {
        qsort(nodes, n_nodes, sizeof(huff_node_t*), compare_nodes);

        huff_node_t *new_node = malloc(sizeof(huff_node_t));
        new_node->value = 0;
        new_node->freq = nodes[0]->freq + nodes[1]->freq;
        new_node->left = nodes[0];
        new_node->right = nodes[1];

        nodes[0] = new_node;
        for (int i = 1; i < n_nodes - 1; i++) {
            nodes[i] = nodes[i + 1];
        }
        n_nodes--;
    }

    /* Extract code lengths */
    uint8_t lengths[17] = {0};
    void extract_lengths(huff_node_t *node, uint8_t depth) {
        if (!node) return;
        if (!node->left && !node->right) {
            for (int i = 0; i < n_values; i++) {
                if (values[i] == node->value) {
                    lengths[i] = depth;
                    break;
                }
            }
        } else {
            extract_lengths(node->left, depth + 1);
            extract_lengths(node->right, depth + 1);
        }
    }
    extract_lengths(nodes[0], 0);

    /* Build canonical codes */
    uint32_t code = 0;
    for (uint8_t len = 1; len <= 16; len++) {
        for (int i = 0; i < n_values; i++) {
            if (lengths[i] == len) {
                codes[i].code = code;
                codes[i].length = len;
                code_map[values[i] + 8] = i;
                code++;
            }
        }
        code <<= 1;
    }

    /* Free tree */
    void free_tree(huff_node_t *node) {
        if (!node) return;
        free_tree(node->left);
        free_tree(node->right);
        free(node);
    }
    free_tree(nodes[0]);
    free(nodes);
}

sparse_adaptive_t* sparse_adaptive_encode(const int8_t *vector, size_t dimension) {
    if (!vector || dimension == 0 || dimension > 65535) return NULL;

    /* Find positions and values */
    uint16_t *positions = malloc(dimension * sizeof(uint16_t));
    int8_t *vals = malloc(dimension * sizeof(int8_t));
    if (!positions || !vals) {
        free(positions);
        free(vals);
        return NULL;
    }

    uint16_t count = 0;
    uint32_t value_freqs[17] = {0};  /* -8 to +8 */
    for (size_t i = 0; i < dimension; i++) {
        if (vector[i] != 0) {
            positions[count] = i;
            vals[count] = vector[i];
            value_freqs[vector[i] + 8]++;
            count++;
        }
    }

    if (count == 0) {
        sparse_adaptive_t *result = malloc(sizeof(sparse_adaptive_t));
        result->data = malloc(2);
        result->data[0] = result->data[1] = 0;
        result->size = 2;
        result->count = 0;
        free(positions);
        free(vals);
        return result;
    }

    /* Build value alphabet and Huffman codes */
    int8_t unique_values[17];
    uint32_t unique_freqs[17];
    int n_unique = 0;
    for (int v = -8; v <= 8; v++) {
        if (value_freqs[v + 8] > 0) {
            unique_values[n_unique] = v;
            unique_freqs[n_unique] = value_freqs[v + 8];
            n_unique++;
        }
    }

    huff_code_t codes[17];
    int8_t code_map[17];
    memset(code_map, -1, sizeof(code_map));
    build_huffman_codes(unique_freqs, unique_values, n_unique, codes, code_map);

    /* Allocate buffer */
    sparse_adaptive_t *result = malloc(sizeof(sparse_adaptive_t));
    if (!result) {
        free(positions);
        free(vals);
        return NULL;
    }

    size_t max_size = 2 + 1 + n_unique * 2 + count * 10;
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

    /* Header: count, n_unique, alphabet */
    if (bw_write_bits(&bw, count, 16) < 0) goto error;
    if (bw_write_bits(&bw, n_unique, 4) < 0) goto error;  /* 1-16 values */
    for (int i = 0; i < n_unique; i++) {
        if (bw_write_bits(&bw, (uint8_t)(unique_values[i] + 8), 4) < 0) goto error;
        if (bw_write_bits(&bw, codes[i].length, 4) < 0) goto error;
    }

    /* Positions: Rice parameter */
    uint8_t r = 4;
    if (bw_write_bits(&bw, r, 3) < 0) goto error;

    /* First position */
    if (bw_write_bits(&bw, positions[0], 11) < 0) goto error;

    /* Gaps */
    for (uint16_t i = 1; i < count; i++) {
        uint16_t gap = positions[i] - positions[i-1] - 1;
        if (bw_write_rice(&bw, gap, r) < 0) goto error;
    }

    /* Values using adaptive Huffman */
    for (uint16_t i = 0; i < count; i++) {
        int idx = code_map[vals[i] + 8];
        if (idx < 0 || bw_write_bits(&bw, codes[idx].code, codes[idx].length) < 0) {
            goto error;
        }
    }

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

int sparse_adaptive_decode(const sparse_adaptive_t *encoded, int8_t *vector, size_t dimension) {
    if (!encoded || !encoded->data || !vector) return -1;

    memset(vector, 0, dimension * sizeof(int8_t));

    bit_reader_t br;
    br_init(&br, encoded->data, encoded->size);

    /* Read header */
    uint32_t count, n_unique;
    if (br_read_bits(&br, 16, &count) < 0) return -1;
    if (br_read_bits(&br, 4, &n_unique) < 0) return -1;

    if (count == 0) return 0;

    /* Read alphabet and code lengths */
    int8_t alphabet[16];
    huff_code_t codes[16];
    for (uint32_t i = 0; i < n_unique; i++) {
        uint32_t val_enc, len;
        if (br_read_bits(&br, 4, &val_enc) < 0) return -1;
        if (br_read_bits(&br, 4, &len) < 0) return -1;
        alphabet[i] = (int8_t)val_enc - 8;
        codes[i].length = len;
    }

    /* Reconstruct canonical codes */
    uint32_t code = 0;
    for (uint8_t len = 1; len <= 16; len++) {
        for (uint32_t i = 0; i < n_unique; i++) {
            if (codes[i].length == len) {
                codes[i].code = code;
                code++;
            }
        }
        code <<= 1;
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

    /* Decode values */
    for (uint32_t i = 0; i < count; i++) {
        uint32_t code_val = 0;
        uint8_t found = 0;

        for (uint8_t code_len = 1; code_len <= 16; code_len++) {
            int bit = br_read_bit(&br);
            if (bit < 0) {
                free(positions);
                return -1;
            }
            code_val = (code_val << 1) | bit;

            for (uint32_t j = 0; j < n_unique; j++) {
                if (codes[j].length == code_len && codes[j].code == code_val) {
                    vector[positions[i]] = alphabet[j];
                    found = 1;
                    break;
                }
            }
            if (found) break;
        }

        if (!found) {
            free(positions);
            return -1;
        }
    }

    free(positions);
    return 0;
}

void sparse_adaptive_free(sparse_adaptive_t *encoded) {
    if (encoded) {
        free(encoded->data);
        free(encoded);
    }
}
