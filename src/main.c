#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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
    jpeg_init(&jpeg, size, data);
    printf("Loaded JPEG of size %dx%d with %d components\n", jpeg.width, jpeg.height, jpeg.n_components);
    jpeg_print_segments(&jpeg);
    jpeg_decode_huffman(&jpeg);
    return 0;
}
