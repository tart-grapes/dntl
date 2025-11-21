/**
 * sparse_delta.c
 *
 * Delta-alphabet sparse encoder: Phase 1 optimization
 * - Rice gaps + canonical Huffman values
 * - Delta-encoded alphabet (saves ~36 bytes)
 * Target: ~173 bytes for 97 nz vectors
 */

#include "sparse_delta.h"
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

/* Huffman tree */
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
    if (na->freq != nb->freq) return na->freq - nb->freq;
    return na->value - nb->value;  /* Deterministic tiebreaker */
}

static void free_tree(huff_node_t *node) {
    if (!node) return;
    free_tree(node->left);
    free_tree(node->right);
    free(node);
}

/* Build canonical Huffman codes */
static void build_huffman(uint32_t *freqs, int8_t *values, int n_values,
                         huff_code_t *codes, int8_t *code_map) {
    if (n_values == 0) return;

    /* Special case: single symbol */
    if (n_values == 1) {
        codes[0].code = 0;
        codes[0].length = 1;
        code_map[values[0] + 128] = 0;
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

    /* Extract code lengths via DFS */
    uint8_t lengths[256] = {0};
    void extract_lengths(huff_node_t *node, uint8_t depth, int8_t *vals, int n, uint8_t *lens) {
        if (!node) return;
        if (!node->left && !node->right) {
            for (int i = 0; i < n; i++) {
                if (vals[i] == node->value) {
                    lens[i] = depth;
                    break;
                }
            }
        } else {
            extract_lengths(node->left, depth + 1, vals, n, lens);
            extract_lengths(node->right, depth + 1, vals, n, lens);
        }
    }
    extract_lengths(nodes[0], 0, values, n_values, lengths);

    /* Build canonical codes */
    uint32_t code = 0;
    for (uint8_t len = 1; len <= 32; len++) {
        for (int i = 0; i < n_values; i++) {
            if (lengths[i] == len) {
                codes[i].code = code;
                codes[i].length = len;
                code_map[values[i] + 128] = i;
                code++;
            }
        }
        code <<= 1;
    }

    free_tree(nodes[0]);
    free(nodes);
}

sparse_delta_t* sparse_delta_encode(const int8_t *vector, size_t dimension) {
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
        sparse_delta_t *result = malloc(sizeof(sparse_delta_t));
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

    /* Build Huffman codes */
    huff_code_t codes[256];
    int8_t code_map[256];
    memset(code_map, -1, sizeof(code_map));
    build_huffman(alphabet_freqs, alphabet, n_unique, codes, code_map);

    /* Allocate result */
    sparse_delta_t *result = malloc(sizeof(sparse_delta_t));
    if (!result) {
        free(positions);
        free(vals);
        return NULL;
    }

    int range = max_val - min_val + 1;
    size_t max_size = 2 + 3 + range + n_unique + count * 12;
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

    /* Huffman code lengths for present values only */
    for (int i = 0; i < n_unique; i++) {
        if (bw_write_bits(&bw, codes[i].length, 5) < 0) goto error;
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

    /* Values */
    for (uint16_t i = 0; i < count; i++) {
        int idx = code_map[(uint8_t)(vals[i] + 128)];
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

int sparse_delta_decode(const sparse_delta_t *encoded,
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
    huff_code_t codes[256];
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

    /* Read Huffman code lengths for present values */
    for (int i = 0; i < n_unique; i++) {
        uint32_t len;
        if (br_read_bits(&br, 5, &len) < 0) return -1;
        codes[i].length = len;
    }

    /* Reconstruct canonical codes */
    uint32_t code = 0;
    for (uint8_t len = 1; len <= 32; len++) {
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

        for (uint8_t code_len = 1; code_len <= 32; code_len++) {
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

void sparse_delta_free(sparse_delta_t *encoded) {
    if (encoded) {
        free(encoded->data);
        free(encoded);
    }
}
