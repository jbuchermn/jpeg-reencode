#!/bin/bash
for f in ./reference/*.jpg; do
    if [[ $f != *.compress* ]]; then
        echo "---------- 2 ----------"
        echo ./build/jpeg-reencode 2 $f reference/$(basename "$f" .jpg).compress2.jpg
        ./build/jpeg-reencode 2 $f reference/$(basename "$f" .jpg).compress2.jpg
        echo "---------- 5 ----------"
        echo ./build/jpeg-reencode 5 $f reference/$(basename "$f" .jpg).compress5.jpg
        ./build/jpeg-reencode 5 $f reference/$(basename "$f" .jpg).compress5.jpg
        echo "---------- 10 ---------"
        echo ./build/jpeg-reencode 10 $f reference/$(basename "$f" .jpg).compress10.jpg
        ./build/jpeg-reencode 10 $f reference/$(basename "$f" .jpg).compress10.jpg
    fi
done
