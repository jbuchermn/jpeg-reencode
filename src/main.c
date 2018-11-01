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
    jpeg_print_segments(&jpeg);
    return 0;
}
