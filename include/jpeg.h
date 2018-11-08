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
#define E_ALREADY_DECODED -5
#define E_NOT_YET_DECODED -6

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
    uint8_t at_restart;
    unsigned char* at;
    uint8_t at_bit;
    long size_bytes;
};

void jpeg_ibitstream_init(struct jpeg_ibitstream* stream, unsigned char* data, long size);
inline int jpeg_ibitstream_read(struct jpeg_ibitstream* stream, uint8_t* result){

    if(stream->size_bytes == 0) return E_EMPTY;
    if(stream->at_restart){
        stream->at_restart = 0;
        return E_RESTART;
    }

    *result = ((*stream->at) >> (7 - stream->at_bit)) & 1;

    if(stream->at_bit == 7){
        if(
            stream->size_bytes >= 2 && 
            stream->at[0] == 0xFF &&
            stream->at[1] == 0x00
        ){
            stream->at++;
            stream->size_bytes--;
        }

        if(
            stream->size_bytes >= 3 &&
            stream->at[1] == 0xFF &&
            stream->at[2] != 0x00
        ){
            stream->at += 2;
            stream->size_bytes -= 2;

            // EOS marker
            if(*stream->at == 0xD9){
                stream->size_bytes = 1;
            }

            // Restart marker
            if(*stream->at >= 0xD0 && *stream->at <= 0xD7){
                stream->at_restart = 1;
            }
        }

        stream->at_bit = 0;
        stream->at++;
        stream->size_bytes--;
    }else{
        stream->at_bit++;
    }

    return 0;
}

struct jpeg_obitstream {
    unsigned char* at;
    uint8_t at_bit;
    long size_bytes;
};

void jpeg_obitstream_init(struct jpeg_obitstream* stream, unsigned char* data, long size);
inline int jpeg_obitstream_write(struct jpeg_obitstream* stream, uint8_t bit){

    if(stream->size_bytes == 0) return E_FULL;

    if(bit){
        (*stream->at) |= 1 << (7 - stream->at_bit);
    }

    if(stream->at_bit == 7){
        if(stream->at[0] == 0xFF){
            if(stream->size_bytes < 2){
                return E_FULL;
            }
            stream->at++;
            stream->size_bytes--;
        }

        stream->at_bit = 0;
        stream->at++;
        stream->size_bytes--;
    }else{
        stream->at_bit++;
    }

    return 0;
}

int jpeg_decode_huffman(struct jpeg* jpeg);

long jpeg_write_recompress_header(struct jpeg* jpeg, unsigned char* buffer, long buffer_size);

/* buffer is required to be 0-initialised */
long jpeg_encode_huffman(struct jpeg* jpeg, unsigned char* buffer, long buffer_size);

long jpeg_reencode_huffman(struct jpeg* jpeg, unsigned char* buffer, long buffer_size);


#endif
