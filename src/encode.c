#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "jpeg.h"
#include "huffman.h"


void jpeg_obitstream_init(struct jpeg_obitstream* stream, unsigned char* data, long size){
    stream->at = data;
    stream->at_bit = 0;
    stream->size_bytes = size;
}

int jpeg_obitstream_write(struct jpeg_obitstream* stream, uint8_t bit){

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

static inline int write_rrrrssss(struct jpeg_obitstream* stream, struct huffman_inv* huffman_inv, int value, uint8_t rrrr){
    int ssss;

    if(value == 0){
        ssss = 0;
    }else if(value > 0){
        ssss = 12;
        while((value & ~(1 << ssss)) == value) ssss--;
        ssss++;
    }else{
        value *= -1;
        ssss = 12;
        while((value & ~(1 << ssss)) == value) ssss--;
        ssss++;
        value *= -1;
    }

    int status = huffman_inv_encode(huffman_inv, stream, (rrrr << 4) + ssss);
    if(status){
        return status;
    }

    if(ssss > 0){
        status = jpeg_obitstream_write(stream, value > 0);
        if(status){
            return status;
        }
    }

    int basevalue = 1 << (ssss - 1);
    if(value < 0){
        basevalue = 1 - 2*basevalue;
    }
    int additional = value - basevalue;

    for(int i=(8 - ssss + 1); i<8; i++){
        int status = jpeg_obitstream_write(stream, (additional >> (7 - i)) & 1);
        if(status){
            return status;
        }
    }

    return 0;
}

static inline int encode_block(int16_t* data, struct jpeg_obitstream* stream, int* dc_offset, struct huffman_inv* dc_inv, struct huffman_inv* ac_inv, struct jpeg_quantisation_table* quantisation){
    
    for(int i=0; i<64; i++){
        if(data[i] == 0) continue;
        data[i] = round(data[i] * quantisation->recompress_factors[i]);
    }

    int value = data[0] - (*dc_offset);
    int status = write_rrrrssss(stream, dc_inv, value, 0);
    *dc_offset = data[0];

    if(status){
        return status;
    }

    int zeros = 0;
    for(int i=1; i<64; i++){
        if(data[i] == 0){
            zeros++;
        }else{
            if(zeros > 16){
                status = huffman_inv_encode(ac_inv, stream, 0xF0);
                if(status){
                    return status;
                }
                zeros -= 16;
            }

            status = write_rrrrssss(stream, ac_inv, data[i], zeros);
            if(status){
                return status;
            }
            zeros = 0;
        }
    }
    if(zeros > 0){
        // Terminate
        status = huffman_inv_encode(ac_inv, stream, 0);
        if(status){
            return status;
        }

    }

    return 0;
}

long jpeg_encode_huffman(struct jpeg* jpeg, unsigned char* buffer, long buffer_size){
    if(!jpeg->blocks){
        return E_NOT_YET_DECODED;
    }

    struct jpeg_obitstream stream;
    jpeg_obitstream_init(&stream, buffer, buffer_size);

    int dc_offset[MAX_COMPONENTS] = { 0 };

    for(int i=0; i<jpeg->n_blocks; i++){
        struct jpeg_block* block = jpeg->blocks + i;
        int dc_id = jpeg->components[block->component_id - 1]->dc_huffman_id;
        int ac_id = jpeg->components[block->component_id - 1]->ac_huffman_id;
        int quantisation_id = jpeg->components[block->component_id - 1]->quantisation_id;

        int status = encode_block(jpeg->blocks[i].values, &stream,
                dc_offset + block->component_id - 1,
                jpeg->dc_huffman_tables[dc_id]->huffman_inv,
                jpeg->ac_huffman_tables[ac_id]->huffman_inv,
                jpeg->quantisation_tables[quantisation_id]);

        if(status){
            return status;
        }
    }

    // Pad byte with ones
    if(stream.size_bytes > 0){
        while(stream.at_bit != 0) jpeg_obitstream_write(&stream, 1);
    }

    // Write EOS
    unsigned char* out = stream.at;
    assert(out - buffer <= buffer_size - 2);
    *(out++) = 0xFF;
    *(out++) = 0xD9;

    return out - buffer;
}
