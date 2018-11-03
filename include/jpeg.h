#ifndef JPEG_H
#define JPEG_H

struct huffman_tree;
struct jpeg_quantisation_table;
struct jpeg_segment;
struct jpeg;

#include "huffman.h"

#define MAX_TABLES 4
#define MAX_COMPONENTS 4

#define E_EMPTY -1
#define E_FULL -2
#define E_RESTART -3
#define E_SIZE_MISMATCH -4

struct jpeg_segment {
    long size;
    unsigned char* data;

    struct jpeg* jpeg;
    struct jpeg_segment* next_segment;
};

void jpeg_segment_init(struct jpeg_segment* segment, struct jpeg* jpeg, long size, unsigned char* data);

struct jpeg_huffman_table{
    int id;
    int class;
    struct huffman_tree* huffman_tree;
    struct huffman_inv* huffman_inv;
};

int jpeg_huffman_table_init(struct jpeg_huffman_table* table, unsigned char* at);
void jpeg_huffman_table_destroy(struct jpeg_huffman_table* table);

struct jpeg_quantisation_table {
    int id;
    int double_precision;

    /* values in the source */
    uint16_t values[64];

    /* values to be used in recompressing */
    uint16_t recompress_values[64];
    float recompress_factors[64];
};

int jpeq_quantisation_table_init(struct jpeg_quantisation_table* table, unsigned char* at);
void jpeg_quantisation_table_init_recompress(struct jpeg_quantisation_table* table, float compress);

struct jpeg_component {
    int id;
    int vertical_sampling;
    int horizontal_sampling;
    int quantisation_id;
    int dc_huffman_id;
    int ac_huffman_id;
};

int jpeg_component_init(struct jpeg_component* component, unsigned char* at);
int jpeg_component_add_huffman(struct jpeg_component* component, unsigned char* at);

struct jpeg_block {
    int component_id;
    int16_t values[64];
};

void jpeg_block_init(struct jpeg_block* block, int component_id);

struct jpeg {
    long size;
    unsigned char* data;

    int width;
    int height;

    struct jpeg_segment* first_segment;

    int n_components;
    struct jpeg_component* components[MAX_COMPONENTS];

    int n_quantisation_tables;
    struct jpeg_quantisation_table* quantisation_tables[MAX_TABLES];

    int n_ac_huffman_tables;
    struct jpeg_huffman_table* ac_huffman_tables[MAX_TABLES];

    int n_dc_huffman_tables;
    struct jpeg_huffman_table* dc_huffman_tables[MAX_TABLES];

    int n_blocks;
    struct jpeg_block* blocks;
};

int jpeg_init(struct jpeg* jpeg, long size, unsigned char* data);
void jpeg_destroy(struct jpeg* jpeg);

void jpeg_print_sizes(struct jpeg* jpeg);
void jpeg_print_segments(struct jpeg* jpeg);
void jpeg_print_components(struct jpeg* jpeg);
void jpeg_print_quantisation_tables(struct jpeg* jpeg);
void jpeg_print_huffman_tables(struct jpeg* jpeg);

struct jpeg_segment* jpeg_find_segment(struct jpeg* jpeg, unsigned char header, struct jpeg_segment* after);

struct jpeg_ibitstream {
    struct ibitstream ibitstream;

    uint8_t at_restart;
    unsigned char* at;
    uint8_t at_bit;
    long size_bytes;
};

void jpeg_ibitstream_init(struct jpeg_ibitstream* stream, unsigned char* data, long size);

struct jpeg_obitstream {
    struct obitstream obitstream;

    unsigned char* at;
    uint8_t at_bit;
    long size_bytes;
};

void jpeg_obitstream_init(struct jpeg_obitstream* stream, unsigned char* data, long size);

int jpeg_decode_huffman(struct jpeg* jpeg);

long jpeg_write_recompress_header(struct jpeg* jpeg, unsigned char* buffer, long buffer_size);

/* buffer is required to be 0-initialised */
long jpeg_encode_huffman(struct jpeg* jpeg, unsigned char* buffer, long buffer_size);


#endif
