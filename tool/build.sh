#!/bin/bash

cd ../
mkdir -p output/reverse_result
mkdir -p output/nop_test
cd reverse 
mkdir -p output
touch output/RE.log
make && make run
cd ../tool
touch ../output/reverse_result/generate_matrix.log
python3 generate_matrix.py | tee ../output/reverse_result/generate_matrix.log
cd ../rhohammer && mkdir -p build && cd build 
if [ ! -f "rhoHammer" ]; then
    echo "rhoHammer not found, compiling..."
    cmake .. && make -j $(nproc)
fi