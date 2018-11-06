#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "jpeg.h"
#include "huffman.h"


void jpeg_ibitstream_init(struct jpeg_ibitstream* stream, unsigned char* data, long size){
    stream->at = data;
    stream->at_restart = 0;
    stream->at_bit = 0;
    stream->size_bytes = size;
}

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

static inline int decode_block(int16_t* result, struct jpeg_ibitstream* stream, int* dc_offset, struct huffman_tree* dc_tree, struct huffman_tree* ac_tree){
    int value = 0;
    int status = read_dc_value(stream, dc_tree, &value);
    value += *dc_offset;
    *dc_offset = value;

    result[0] = value;

    if(status){
        return status;
    }

    for(int i=1; i<64; i++){
        uint8_t leading_zeros;
        int value;
        int status = read_ac_value(stream, ac_tree, &value, &leading_zeros);
        if(status){
            return status;
        }

        i += leading_zeros;
        if(i >= 64){
            break;
        }

        result[i] = value;
    }

    return 0;
}


static int jpeg_processing_unit_decode_huffman(struct jpeg_processing_unit* pu){
    struct jpeg_ibitstream stream;
    jpeg_ibitstream_init(&stream, pu->data, pu->size);

    int loop_count = 0;
    for(int i=0; i<pu->jpeg->n_components; i++){
        loop_count += pu->jpeg->components[i]->vertical_sampling * pu->jpeg->components[i]->horizontal_sampling;
    }

    struct jpeg_component** loop = malloc(loop_count * sizeof(struct jpeg_component*));
    int k = 0;
    for(int i=0; i<pu->jpeg->n_components; i++){
        int block_count = pu->jpeg->components[i]->vertical_sampling * pu->jpeg->components[i]->horizontal_sampling;
        for(int j=0; j<block_count; j++){
            loop[k++] = pu->jpeg->components[i];
        }
    }

    int dc_offset[MAX_COMPONENTS] = { 0 };

    int component = 0;
    for(int i=0; i<pu->n_blocks; i++){
        pu->blocks[i].component_id = loop[component]->id;
        int dc_id = loop[component]->dc_huffman_id;
        int ac_id = loop[component]->ac_huffman_id;

        int done = 0;
        int status = 0;
        while(!done){
            status = decode_block(pu->blocks[i].values, &stream, 
                    dc_offset + loop[component]->id - 1,
                    pu->jpeg->dc_huffman_tables[dc_id]->huffman_tree,
                    pu->jpeg->ac_huffman_tables[ac_id]->huffman_tree);

            if(status == E_RESTART){
                for(int i=0; i<MAX_COMPONENTS; i++) dc_offset[i] = 0;
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

    return 0;

}

int jpeg_decode_huffman(struct jpeg* jpeg){
    for(int i=0; i<jpeg->n_processing_units; i++){
        int status = jpeg_processing_unit_decode_huffman(jpeg->processing_units + i);
        if(status){
            return status;
        }
    }
    return 0;
}

static struct batch {
    pthread_t thread;
    struct jpeg_processing_unit* pu;
    int pu_stride;
    int n_pu;
    int status;
};

static void* run(void* arg){
    struct batch* batch = (struct batch*)arg;
    for(int i=0; i<batch->n_pu; i+=batch->pu_stride){
        batch->status = jpeg_processing_unit_decode_huffman(batch->pu + i);
        if(batch->status){
            return 0;
        }
    }

    return 0;
}

int jpeg_decode_huffman_parallel(struct jpeg* jpeg, int nproc){
    struct batch* batches = malloc(nproc * sizeof(struct batch));

    for(int i=0; i<nproc; i++){
        batches[i].status = 0;
        batches[i].pu = jpeg->processing_units + i;
        batches[i].pu_stride = nproc;
        batches[i].n_pu = jpeg->n_processing_units - i;
    }

    for(int i=0; i<nproc; i++){
        pthread_create(&batches[i].thread, NULL, &run, (void*)&batches[i]);
    }

    int status = 0;
    for(int i=0; i<nproc; i++){
        pthread_join(batches[i].thread, NULL);
        if(batches[i].status){
            status = batches[i].status;
        }
    }

    free(batches);
    return status;
}
