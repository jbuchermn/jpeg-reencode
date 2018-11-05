#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
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

    table->huffman_inv = malloc(sizeof(struct huffman_inv));
    huffman_inv_init(table->huffman_inv, table->huffman_tree);

    return at - at_orig;
}

void jpeg_huffman_table_destroy(struct jpeg_huffman_table* table){
    huffman_tree_destroy(table->huffman_tree);
    free(table->huffman_tree);
    table->huffman_tree = 0;

    huffman_inv_destroy(table->huffman_inv);
    free(table->huffman_inv);
    table->huffman_inv = 0;
}


int jpeg_quantisation_table_init(struct jpeg_quantisation_table* table, unsigned char* at){
    unsigned char* at_orig = at;

    uint8_t info = *at;
    at++;

    table->double_precision = (info & 0xF0);
    table->id = info & 0x0F;

    for(int i=0; i<64; i++){
        if(!table->double_precision){
            table->values[i] = *at;
            at++;
        }else{
            table->values[i] = uint16_from_uchar(at);
            at += 2;
        }
    }

    for(int i=0; i<64; i++){
        table->recompress_values[i] = table->values[i];
        table->recompress_factors[i] = 1.;
    }

    return at - at_orig;
}

void jpeg_quantisation_table_init_recompress(struct jpeg_quantisation_table* table, float compress){
    for(int i=0; i<64; i++){
        table->recompress_values[i] = floor(table->values[i] * compress + .5);
        table->recompress_factors[i] = ((float)table->values[i]) / table->recompress_values[i];
    }
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

int jpeg_init(struct jpeg* jpeg, long size, unsigned char* data){
    jpeg->size = size;
    jpeg->data = data;

    jpeg->n_processing_units = 0;
    jpeg->processing_units = 0;

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

    // Restart interval
    struct jpeg_segment* dri = jpeg_find_segment(jpeg, 0xDD, 0);
    if(!dri){
        jpeg->restart_interval = -1;
    }else{
        jpeg->restart_interval = uint16_from_uchar(dri->data + 4);
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

    /*
     * Handle stupid way of specifying subsampling
     *
     * Problem: subsampling 2x2, 2x2, 2x2 means there are no 16x16 blocks! It's actually 1x1, 1x1, 1x1
     * However, 2x2, 1x1, 1x1 means there are such blocks
     *
     * Solution: Handle this special case of all factors == 2
     */
    int special_case = 1;
    for(int i=0; i<jpeg->n_components; i++){
        if(jpeg->components[i]->horizontal_sampling != 2 || jpeg->components[i]->vertical_sampling != 2){
            special_case = 0;
            break;
        }
    }
    if(special_case){
        for(int i=0; i<jpeg->n_components; i++){
            jpeg->components[i]->horizontal_sampling = 1;
            jpeg->components[i]->vertical_sampling = 1;
        }
    }

    // Block layout
    int reference_height = 0;
    int reference_width = 0;
    for(int i=0; i<jpeg->n_components; i++){
        reference_height = reference_height < jpeg->components[i]->horizontal_sampling ? 
            jpeg->components[i]->horizontal_sampling : reference_height;
        reference_width = reference_width < jpeg->components[i]->vertical_sampling ? 
            jpeg->components[i]->vertical_sampling : reference_width;
    }

    int block_height = ceil(jpeg->height / reference_height / 8.);
    int block_width = ceil(jpeg->width / reference_width / 8.);

    jpeg->n_blocks = 0;
    for(int i=0; i<jpeg->n_components; i++){
        jpeg->n_blocks += block_width * jpeg->components[i]->vertical_sampling * 
            block_height * jpeg->components[i]->horizontal_sampling;
    }

    jpeg->blocks = malloc(jpeg->n_blocks * sizeof(struct jpeg_block));
    memset(jpeg->blocks, 0, jpeg->n_blocks * sizeof(struct jpeg_block));

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

    jpeg->scan_data = sos->data + sos->size;
    jpeg->scan_size = jpeg->size - (jpeg->scan_data - jpeg->data);

    return 0;
}

void jpeg_init_processing_units(struct jpeg* jpeg){
    if(jpeg->restart_interval == -1){
        jpeg->n_processing_units = 1;
    }else{
        // TODO! Proper block layout
        jpeg->n_processing_units = jpeg->n_blocks/jpeg->restart_interval/4;
        assert(jpeg->restart_interval * jpeg->n_processing_units * 4 == jpeg->n_blocks);
    }

    jpeg->processing_units = malloc(jpeg->n_processing_units * sizeof(struct jpeg_processing_unit));

    struct jpeg_block* blocks = jpeg->blocks;
    for(int i=0; i<jpeg->n_processing_units; i++){
        jpeg->processing_units[i].jpeg = jpeg;
        jpeg->processing_units[i].n_blocks = jpeg->restart_interval * 4;
        jpeg->processing_units[i].blocks = blocks;
        blocks += 4 * jpeg->restart_interval;
    }

    unsigned char* start = jpeg->scan_data;
    int i=0;
    for(unsigned char* at=jpeg->scan_data; at<=jpeg->scan_data + jpeg->scan_size - 1; at++){
        if(at[0] == 0xFF){
            if(at[1] >= 0xD0 && at[1] <= 0xD9){ // Accept restart markers and EOS marker
                jpeg->processing_units[i].data = start;
                jpeg->processing_units[i].size = at - start;
                start = at + 2;
                i++;

                if(i>= jpeg->n_processing_units){
                    assert(at[1] == 0xD9);
                    break;
                }
            }
        }
    }
}

void jpeg_destroy(struct jpeg* jpeg){
    for(int i=0; i<jpeg->n_components; i++){
        free(jpeg->components[i]);
        jpeg->components[i] = 0;
    }

    for(int i=0; i<jpeg->n_quantisation_tables; i++){
        free(jpeg->quantisation_tables[i]);
        jpeg->quantisation_tables[i] = 0;
    }

    for(int i=0; i<jpeg->n_ac_huffman_tables; i++){
        jpeg_huffman_table_destroy(jpeg->ac_huffman_tables[i]);
        free(jpeg->ac_huffman_tables[i]);
        jpeg->ac_huffman_tables[i] = 0;
    }

    for(int i=0; i<jpeg->n_dc_huffman_tables; i++){
        jpeg_huffman_table_destroy(jpeg->dc_huffman_tables[i]);
        free(jpeg->dc_huffman_tables[i]);
        jpeg->dc_huffman_tables[i] = 0;
    }

    free(jpeg->blocks);
    jpeg->blocks = 0;

    struct jpeg_segment* segment = jpeg->first_segment;
    while(segment){
        struct jpeg_segment* next = segment->next_segment;
        free(segment);
        segment = next;
    }
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

long jpeg_write_recompress_header(struct jpeg* jpeg, unsigned char* buffer, long buffer_size){
    unsigned char* at = buffer;

    for(struct jpeg_segment* cur = jpeg->first_segment; cur; cur = cur->next_segment){
        if(cur->data[1] == 0xDD){
            // Skip restart header
            continue;
        }else if(cur->data[1] == 0xDB){
            // Modify quantisation header
            *(at++) = 0xFF;
            *(at++) = 0xDB;
            
            // Leave room for size
            unsigned char* size = at;
            at += 2;

            for(int i=0; i<jpeg->n_quantisation_tables; i++){
                uint8_t info = jpeg->quantisation_tables[i]->id;
                info |= (jpeg->quantisation_tables[i]->double_precision << 4);
                *(at++) = info;
                for(int j=0; j<64; j++){
                    if(!jpeg->quantisation_tables[i]->double_precision){
                        *(at++) = jpeg->quantisation_tables[i]->recompress_values[j];
                    }else{
                        int value = jpeg->quantisation_tables[i]->recompress_values[j];
                        *(at++) = (value & 0xFF00) / 256;
                        *(at++) = value & 0xFF;
                    }
                }
            }

            // Set size
            int s = (at - size);
            *(size++) = (s & 0xFF00) / 256;
            *(size++) = (s & 0xFF);

        }else{
            // Copy other headers
            memcpy(at, cur->data, cur->size);
            at += cur->size;
        }

        /*
         * Actually we are too late at this point, but let's just assume
         * anyone provides enough space for the header
         */
        if(buffer + buffer_size < at){
            return E_FULL;
        }

    }

    return at - buffer;
}
