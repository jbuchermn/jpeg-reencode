#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "jpeg.h"
#include "huffman.h"

static inline int from_ssss(uint8_t ssss, struct jpeg_ibitstream* stream, int* value){
    if(ssss == 0){
        *value = 0;
        return 0;
    }

    int basevalue = 1 << (ssss - 1);

    uint8_t positive; 
    int status = jpeg_ibitstream_read(stream, &positive);
    if(status){
        return status;
    }

    if(!positive){
        basevalue = 1 - 2*basevalue;
    }

    int additional = 0;
    for(int i=0; i<ssss - 1; i++){
        uint8_t next_bit;
        int status = jpeg_ibitstream_read(stream, &next_bit);
        if(status){
            return status;
        }
        additional = (additional << 1) + next_bit;
    }

    *value = basevalue + additional;
    return 0;
}

static inline int read_dc_value(struct jpeg_ibitstream* stream, struct huffman_tree* tree, int* value){
    uint8_t ssss;
    int status = huffman_tree_decode(tree, stream, &ssss);
    if(status){
        return status;
    }

    return from_ssss(ssss, stream, value);
}

static inline int read_ac_value(struct jpeg_ibitstream* stream, struct huffman_tree* tree, int* value, uint8_t* leading_zeros){
    uint8_t rrrrssss;
    int status = huffman_tree_decode(tree, stream, &rrrrssss);
    if(status){
        return status;
    }

    uint8_t rrrr = (rrrrssss & 0xF0) / 16;
    uint8_t ssss = rrrrssss & 0x0F;

    if(rrrr == 0 && ssss == 0){
        // Terminate
        *leading_zeros = 64;
        *value = 0;

        return 0;
    }else if(rrrr == 15 && ssss == 0){
        // 16 zeros
        *leading_zeros = 15;
        *value = 0;

        return 0;
    }else{
        // rrrr zeros followed by value specified through ssss
        *leading_zeros = rrrr;

        return from_ssss(ssss, stream, value);
    }
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

static inline int reencode_block(
        struct jpeg_ibitstream* istream,
        struct jpeg_obitstream* ostream,
        int* dec_dc_offset,
        int* enc_dc_offset,
        struct huffman_tree* dc_tree,
        struct huffman_tree* ac_tree,
        struct huffman_inv* dc_inv,
        struct huffman_inv* ac_inv,
        struct jpeg_quantisation_table* quantisation){

    int value = 0;
    int status = read_dc_value(istream, dc_tree, &value);
    int value_abs = value + (*dec_dc_offset);
    *dec_dc_offset = value_abs;

    if(status){
        return status;
    }

    value_abs = round(value_abs * quantisation->recompress_factors[0]);
    status = write_rrrrssss(ostream, dc_inv, value_abs - (*enc_dc_offset), 0);
    *enc_dc_offset = value_abs;

    if(status){
        return status;
    }

    int enc_leading_zeros = 0;
    for(int i=1; i<64; i++){
        uint8_t leading_zeros;
        int value;
        status = read_ac_value(istream, ac_tree, &value, &leading_zeros);
        if(status){
            return status;
        }

        i += leading_zeros;
        enc_leading_zeros += leading_zeros;
        if(i >= 64){
            break;
        }

        value = round(value * quantisation->recompress_factors[i]);
        if(value == 0){
            enc_leading_zeros++;
        }else{
            while(enc_leading_zeros > 16){
                status = huffman_inv_encode(ac_inv, ostream, 0xF0);
                if(status){
                    return status;
                }
                enc_leading_zeros -= 16;
            }

            status = write_rrrrssss(ostream, ac_inv, value, enc_leading_zeros);
            if(status){
                return status;
            }
            enc_leading_zeros = 0;
        }
    }

    if(enc_leading_zeros > 0){
        // Terminate
        status = huffman_inv_encode(ac_inv, ostream, 0);
        if(status){
            return status;
        }

    }

    return 0;
}




long jpeg_reencode_huffman(struct jpeg* jpeg, unsigned char* buffer, long buffer_size){
    struct jpeg_segment* sos = jpeg_find_segment(jpeg, 0xDA, 0);
    unsigned char* scan_data = sos->data + sos->size;
    long scan_size = jpeg->size - (scan_data - jpeg->data);

    struct jpeg_ibitstream istream;
    jpeg_ibitstream_init(&istream, scan_data, scan_size);

    struct jpeg_obitstream ostream;
    jpeg_obitstream_init(&ostream, buffer, buffer_size);

    int loop_count = 0;
    for(int i=0; i<jpeg->n_components; i++){
        loop_count += jpeg->components[i]->vertical_sampling * jpeg->components[i]->horizontal_sampling;
    }

    struct jpeg_component** loop = malloc(loop_count * sizeof(struct jpeg_component*));
    int k = 0;
    for(int i=0; i<jpeg->n_components; i++){
        int block_count = jpeg->components[i]->vertical_sampling * jpeg->components[i]->horizontal_sampling;
        for(int j=0; j<block_count; j++){
            loop[k++] = jpeg->components[i];
        }
    }

    int enc_dc_offset[MAX_COMPONENTS] = { 0 };
    int dec_dc_offset[MAX_COMPONENTS] = { 0 };

    int component = 0;
    for(int i=0; i<jpeg->n_blocks; i++){
        int dc_id = loop[component]->dc_huffman_id;
        int ac_id = loop[component]->ac_huffman_id;
        int quant_id = loop[component]->quantisation_id;

        int done = 0;
        int status = 0;

        int ostream_at_bit_stored = ostream.at_bit;
        unsigned char* ostream_at_stored = ostream.at;
        while(!done){
            status = reencode_block(&istream, &ostream, 
                    dec_dc_offset + loop[component]->id - 1,
                    enc_dc_offset + loop[component]->id - 1,
                    jpeg->dc_huffman_tables[dc_id]->huffman_tree,
                    jpeg->ac_huffman_tables[ac_id]->huffman_tree,
                    jpeg->dc_huffman_tables[dc_id]->huffman_inv,
                    jpeg->ac_huffman_tables[ac_id]->huffman_inv,
                    jpeg->quantisation_tables[quant_id]);

            if(status == E_RESTART){
                for(int i=0; i<MAX_COMPONENTS; i++) dec_dc_offset[i] = 0;
                ostream.at_bit = ostream_at_bit_stored;
                ostream.at = ostream_at_stored;
            }else{
                done = 1;
            }
        }

        if(status){
            free(loop);
            return status;
        }

        component = (component + 1) % loop_count;
    }

    free(loop);

    // Move to byte boundary
    if(istream.size_bytes > 0){
        uint8_t dummy;
        while(istream.at_bit != 0) jpeg_ibitstream_read(&istream, &dummy);
    }

    // Assert we hit EOS
    if(istream.size_bytes != 0){
        return E_SIZE_MISMATCH;
    }

    // Pad byte with ones
    if(ostream.size_bytes > 0){
        while(ostream.at_bit != 0) jpeg_obitstream_write(&ostream, 1);
    }

    // Write EOS
    unsigned char* out = ostream.at;
    assert(out - buffer <= buffer_size - 2);
    *(out++) = 0xFF;
    *(out++) = 0xD9;

    return out - buffer;
}
