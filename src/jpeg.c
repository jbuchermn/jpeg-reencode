#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "jpeg.h"
#include "huffman.h"

static uint16_t uint16_from_uchar(unsigned char* at){
    return at[1] + 256*at[0];
}

int jpeg_huffman_table_init(struct jpeg_huffman_table* table, unsigned char* at){
    unsigned char* at_orig = at;
    uint8_t info = *at;
    at++;

    table->class = (info & 0xF0) / 16;
    table->id = info & 0x0F;
    table->huffman_tree = malloc(sizeof(struct huffman_tree));
    huffman_tree_init(table->huffman_tree);

    int n_elements[16];
    for(int i=0; i<16; i++){
        n_elements[i] = *at;
        at++;
    }

    for(int depth=1; depth<=16; depth++){
        for(int i=0; i<n_elements[depth - 1]; i++){
            uint8_t element = *at;
            at++;

            int success = huffman_tree_insert_goleft(table->huffman_tree, depth, element);
            assert(success);
        }
    }

    return at - at_orig;
}

int jpeg_quantisation_table_init(struct jpeg_quantisation_table* table, unsigned char* at){
    unsigned char* at_orig = at;

    uint8_t info = *at;
    at++;

    int double_precision = (info & 0xF0);
    table->id = info & 0x0F;

    for(int i=0; i<64; i++){
        if(!double_precision){
            table->values[i] = *at;
            at++;
        }else{
            table->values[i] = uint16_from_uchar(at);
            at += 2;
        }
    }

    return at - at_orig;
}

int jpeg_component_init(struct jpeg_component* component, unsigned char* at){
    component->id = at[0];
    uint8_t sampling = at[1];
    component->vertical_sampling = (sampling & 0xF0) / 16;
    component->horizontal_sampling = sampling & 0x0F;
    component->quantisation_id = at[2];
    return 3;
}

int jpeg_component_add_huffman(struct jpeg_component* component, unsigned char* at){
    assert(component->id = at[0]);
    uint8_t huffman = at[1];
    component->dc_huffman_id = (huffman & 0xF0) / 16;
    component->ac_huffman_id = huffman & 0x0F;
    return 2;
}

void jpeg_segment_init(struct jpeg_segment* segment, struct jpeg* jpeg, long size, unsigned char* data){
    segment->size = size;
    segment->data = data;
    segment->jpeg = jpeg;
    segment->next_segment = 0;

    // Need at least a header
    assert(size > 1);
}

void jpeg_block_init(struct jpeg_block* block, int component_id){
    block->component_id = component_id;
}

int jpeg_init(struct jpeg* jpeg, long size, unsigned char* data){
    jpeg->size = size;
    jpeg->data = data;

    for(int i=0; i<MAX_TABLES; i++){
        jpeg->quantisation_tables[i] = 0;
        jpeg->ac_huffman_tables[i] = 0;
        jpeg->dc_huffman_tables[i] = 0;
    }

    struct jpeg_segment* seg = 0;
    for(long i=0; i<size; i++){
        if(data[i] == 0xFF){
            // Start of segment marker
            if(i<(size-1) && 
                    data[i+1] != 0x00 && // FF00 escapes FF
                    data[i+1] != 0xFF && // FFFF is not a marker
                    (data[i+1] < 0xD0 || data[i+1] > 0xD7) // FFD0 - FFD8 are restart markers
            ){
                struct jpeg_segment* next_seg = malloc(sizeof(struct jpeg_segment));
                jpeg_segment_init(next_seg, jpeg, 2, data + i);
                if(seg){
                    seg->size = data + i - seg->data;
                    seg->next_segment = next_seg;
                }else{
                    jpeg->first_segment = next_seg;
                }
                seg = next_seg;

                // DA, DB, C0, C4 store length as uint16_t after header
                if(i < (size - 3) && 
                        (data[i+1] == 0xDA || data[i+1] == 0xDB || data[i+1] == 0xC0 || data[i+1] == 0xC4)
                ){
                    // We always include the marker in the size
                    seg->size = uint16_from_uchar(data + i + 2) + 2;
                    i += seg->size - 2;
                }

                // Start of scan marker
                if(seg->data[1] == 0xDA){
                    break;
                }

            }
            i++;
        }
    }

    // Quantisation
    jpeg->n_quantisation_tables = 0;
    for(struct jpeg_segment* quantisation = jpeg_find_segment(jpeg, 0xDB, 0); quantisation; quantisation = jpeg_find_segment(jpeg, 0xDB, quantisation)){
        unsigned char* at = quantisation->data + 4;
        while(at - quantisation->data < quantisation->size){
            struct jpeg_quantisation_table* quantisation_table = malloc(sizeof(struct jpeg_quantisation_table));
            at += jpeg_quantisation_table_init(quantisation_table, at);
            assert(quantisation_table->id < MAX_TABLES);
            jpeg->quantisation_tables[quantisation_table->id] = quantisation_table;
            if(quantisation_table->id >= jpeg->n_quantisation_tables){
                jpeg->n_quantisation_tables = quantisation_table->id + 1;
            }
        }
        assert(at - quantisation->data == quantisation->size);
    }

    // Huffman
    jpeg->n_ac_huffman_tables = 0;
    jpeg->n_dc_huffman_tables = 0;
    for(struct jpeg_segment* huffman = jpeg_find_segment(jpeg, 0xC4, 0); huffman; huffman = jpeg_find_segment(jpeg, 0xC4, huffman)){
        unsigned char* at = huffman->data + 4;
        while(at - huffman->data < huffman->size){
            struct jpeg_huffman_table* huffman_table = malloc(sizeof(struct jpeg_huffman_table));
            at += jpeg_huffman_table_init(huffman_table, at);
            assert(huffman_table->id < MAX_TABLES);
            if(huffman_table->class){
                jpeg->ac_huffman_tables[huffman_table->id] = huffman_table;
                if(huffman_table->id >= jpeg->n_ac_huffman_tables){
                    jpeg->n_ac_huffman_tables = huffman_table->id + 1;
                }
            }else{
                jpeg->dc_huffman_tables[huffman_table->id] = huffman_table;
                if(huffman_table->id >= jpeg->n_dc_huffman_tables){
                    jpeg->n_dc_huffman_tables = huffman_table->id + 1;
                }
            }
        }
        assert(at - huffman->data == huffman->size);
    }

    // Start of frame
    struct jpeg_segment* sof = jpeg_find_segment(jpeg, 0xC0, 0);
    assert(sof);
    assert(sof->size >= 10);
    jpeg->height = uint16_from_uchar(sof->data + 5);
    jpeg->width = uint16_from_uchar(sof->data + 7);
    jpeg->n_components = *(sof->data + 9);
    unsigned char* at = sof->data + 10;
    for(int i=0; at - sof->data < sof->size && i < jpeg->n_components; i++){
        struct jpeg_component* component = malloc(sizeof(struct jpeg_component));
        at += jpeg_component_init(component, at);
        jpeg->components[component->id - 1] = component;
    }
    assert(at - sof->data == sof->size);

    // Block layout
    int reference_height = 0;
    int reference_width = 0;
    for(int i=0; i<jpeg->n_components; i++){
        reference_height = reference_height < jpeg->components[i]->horizontal_sampling ? 
            jpeg->components[i]->horizontal_sampling : reference_height;
        reference_width = reference_width < jpeg->components[i]->vertical_sampling ? 
            jpeg->components[i]->vertical_sampling : reference_width;
    }

    int block_height = ceil(jpeg->height / reference_width / 8.);
    int block_width = ceil(jpeg->width / reference_width / 8.);

    jpeg->n_blocks = 0;
    for(int i=0; i<jpeg->n_components; i++){
        jpeg->n_blocks += block_width * jpeg->components[i]->vertical_sampling * 
            block_height * jpeg->components[i]->horizontal_sampling;
    }
    jpeg->blocks = malloc(jpeg->n_blocks * sizeof(struct jpeg_block));

    // Start of scan
    struct jpeg_segment* sos = jpeg_find_segment(jpeg, 0xDA, 0);
    assert(sos);
    assert(jpeg->n_components == *(sos->data + 4));
    at = sos->data + 5;
    for(int i=0; at - sos->data < sos->size && i < jpeg->n_components; i++){
        struct jpeg_component* component = jpeg->components[*at - 1];
        at += jpeg_component_add_huffman(component, at);
    }
    // Three empty bytes before scan data
    assert(at + 3 - sos->data == sos->size);

    return 0;
}

void jpeg_print_sizes(struct jpeg* jpeg){
    printf("------ JPEG -----------\n");
    printf("Size: %dx%d, total %d blocks\n", jpeg->width, jpeg->height, jpeg->n_blocks);
}

void jpeg_print_segments(struct jpeg* jpeg){
    printf("------ Segments -------\n");
    for(struct jpeg_segment* cur = jpeg->first_segment; cur; cur = cur->next_segment){
        printf("Segment(%02X) of length %ld\n", cur->data[1], cur->size);
    }
}

void jpeg_print_components(struct jpeg* jpeg){
    printf("------ Components -----\n");
    for(int i=0; i<jpeg->n_components; i++){
        printf("Component(%d) sampling: %d %d\n",
                jpeg->components[i]->id,
                jpeg->components[i]->vertical_sampling,
                jpeg->components[i]->horizontal_sampling);
    }
}

void jpeg_print_quantisation_tables(struct jpeg* jpeg){
    printf("------ Quantisation ---\n");
    for(int i=0; i<jpeg->n_quantisation_tables; i++){
        printf("Quantisation(%d)\n\t", jpeg->quantisation_tables[i]->id);
        for(int j=0; j<64; j++){
            printf("%d ", jpeg->quantisation_tables[i]->values[j]);
        }
        printf("\n");
    }
}

void jpeg_print_huffman_tables(struct jpeg* jpeg){
    printf("------ Huffman -------\n");
    for(int i=0; i<jpeg->n_dc_huffman_tables; i++){
        printf("Huffman(DC, %d)\n", jpeg->dc_huffman_tables[i]->id);
        huffman_tree_print(jpeg->dc_huffman_tables[i]->huffman_tree, "\t");
    }
    for(int i=0; i<jpeg->n_ac_huffman_tables; i++){
        printf("Huffman(AC, %d)\n", jpeg->ac_huffman_tables[i]->id);
        huffman_tree_print(jpeg->ac_huffman_tables[i]->huffman_tree, "\t");
    }

}

struct jpeg_segment* jpeg_find_segment(struct jpeg* jpeg, unsigned char header, struct jpeg_segment* after){
    for(struct jpeg_segment* cur = after ? after->next_segment : jpeg->first_segment; cur; cur = cur->next_segment){
        if(cur->data[1] == header) return cur;
    }

    return 0;
}

static int from_ssss(uint8_t ssss, struct bitstream* stream, int8_t* value){
    if(ssss == 0){
        *value = 0;
        return 0;
    }

    int8_t basevalue = 1 << (ssss - 1);

    int positive; 
    int success = bitstream_next(stream, &positive);
    assert(success);

    if(!positive){
        basevalue = 1 - 2*basevalue;
    }

    int8_t additional = 0;
    for(int i=0; i<ssss - 1; i++){
        int next_bit;
        int success = bitstream_next(stream, &next_bit);
        assert(success);
        additional = (additional << 1) + next_bit;
    }

    *value = basevalue + additional;
    return 0;
}

static int read_dc_value(struct bitstream* stream, struct huffman_tree* tree, int8_t* value){
    uint8_t ssss = huffman_tree_decode(tree, stream);
    return from_ssss(ssss, stream, value);
}

static int read_ac_value(struct bitstream* stream, struct huffman_tree* tree, int8_t* value, uint8_t* leading_zeros){
    uint8_t rrrrssss = huffman_tree_decode(tree, stream);
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

static int decode_block(int8_t* result, struct bitstream* stream, struct huffman_tree* dc_tree, struct huffman_tree* ac_tree){
    read_dc_value(stream, dc_tree, result);
    for(int i=1; i<64; i++){
        uint8_t leading_zeros;
        int8_t value;
        read_ac_value(stream, ac_tree, &value, &leading_zeros);

        i += leading_zeros;
        if(i >= 64){
            break;
        }

        result[i] = value;
    }

    return 0;
}

int jpeg_decode_huffman(struct jpeg* jpeg){
    struct jpeg_segment* sos = jpeg_find_segment(jpeg, 0xDA, 0);
    unsigned char* scan_data = sos->data + sos->size;
    long scan_size = jpeg->size - (scan_data - jpeg->data);

    struct bitstream stream;
    bitstream_init(&stream, scan_data, scan_size, 1);

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

    int component = 0;
    for(int i=0; i<jpeg->n_blocks; i++){
        jpeg_block_init(jpeg->blocks + i, loop[component]->id);
        int dc_id = loop[component]->dc_huffman_id;
        int ac_id = loop[component]->ac_huffman_id;

        decode_block(jpeg->blocks[i].values, &stream, 
                jpeg->dc_huffman_tables[dc_id]->huffman_tree,
                jpeg->ac_huffman_tables[ac_id]->huffman_tree);

        component = (component + 1) % loop_count;
    }

    // Move to byte boundary
    if(stream.size_bytes > 0){
        int dummy;
        while(stream.at_bit != 0) bitstream_next(&stream, &dummy);
    }

    // Assert we hit EOS
    assert(stream.size_bytes == 0);

    free(loop);

    return 0;
}
