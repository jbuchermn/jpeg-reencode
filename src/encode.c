#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "jpeg.h"
#include "huffman.h"

static int jpeg_obitstream_write(void* data, uint8_t bit){
    struct jpeg_obitstream* stream = (struct jpeg_obitstream*)data;

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

void jpeg_obitstream_init(struct jpeg_obitstream* stream, unsigned char* data, long size){
    obitstream_init(&stream->obitstream, stream, &jpeg_obitstream_write);
    stream->at = data;
    stream->at_bit = 0;
    stream->size_bytes = size;
}

static int write_ssss(struct obitstream* stream, struct huffman_inv* huffman_inv, int value){
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

    int status = huffman_inv_encode(huffman_inv, stream, ssss);
    if(status){
        return status;
    }

    if(ssss > 0){
        status = obitstream_write(stream, value > 0);
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
        int status = obitstream_write(stream, (additional >> (7 - i)) & 1);
        if(status){
            return status;
        }
    }

    return 0;
}

static int encode_block(int16_t* data, struct obitstream* stream, int* dc_offset, struct huffman_inv* dc_inv, struct huffman_inv* ac_inv){
    
    int value = data[0] - (*dc_offset);
    int status = write_ssss(stream, dc_inv, value);
    *dc_offset = data[0];

    if(status){
        return status;
    }

    // Skip all AC values
    status = huffman_inv_encode(ac_inv, stream, 0);
    if(status){
        return status;
    }

    return 0;
}

long jpeg_encode_huffman(struct jpeg* jpeg, unsigned char* buffer, long buffer_size){
    /* Simply copy the header for now */
    struct jpeg_segment* sos = jpeg_find_segment(jpeg, 0xDA, 0);
    unsigned char* scan_data = sos->data + sos->size;

    unsigned char* in = jpeg->data;
    unsigned char* out = buffer;
    while(in<scan_data){ *(out++) = *(in++); }

    /* Write scan */
    struct jpeg_obitstream stream;
    jpeg_obitstream_init(&stream, out, buffer_size - (out - buffer));

    int dc_offset[MAX_COMPONENTS] = { 0 };

    for(int i=0; i<jpeg->n_blocks; i++){
        struct jpeg_block* block = jpeg->blocks + i;
        int dc_id = jpeg->components[block->component_id - 1]->dc_huffman_id;
        int ac_id = jpeg->components[block->component_id - 1]->ac_huffman_id;

        int status = encode_block(jpeg->blocks[i].values, &stream.obitstream,
                dc_offset + block->component_id - 1,
                jpeg->dc_huffman_tables[dc_id]->huffman_inv,
                jpeg->ac_huffman_tables[ac_id]->huffman_inv);

        if(status){
            return status;
        }
    }

    // Pad byte with ones
    if(stream.size_bytes > 0){
        while(stream.at_bit != 0) obitstream_write(&stream.obitstream, 1);
    }

    // Write EOS
    out = stream.at;
    assert(out - buffer <= buffer_size - 2);
    *out = 0xFF;
    *out++;
    *out = 0xD9;
    *out++;

    return out - buffer;
}
