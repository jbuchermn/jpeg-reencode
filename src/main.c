#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include "jpeg.h"

int main(int argc, char** argv){
    if (argc < 2){
        printf("Usage jpeg-quality-changer file.jpg");
        exit(1);
    }

    FILE* f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* data = malloc(size);
    fread(data, size, 1, f);
    fclose(f);

    struct jpeg jpeg;
    int status = jpeg_init(&jpeg, size, data);
    assert(status == 0);

    jpeg_print_sizes(&jpeg);
    jpeg_print_segments(&jpeg);
    jpeg_print_components(&jpeg);
    jpeg_print_quantisation_tables(&jpeg);
    jpeg_print_huffman_tables(&jpeg);

    clock_t decode_time = clock();
    status = jpeg_decode_huffman(&jpeg);
    assert(status == 0);
    decode_time = clock() - decode_time;

    printf("Successfully decoded JPEG in %fms\n", 1000.*decode_time/CLOCKS_PER_SEC);

    return 0;
}
