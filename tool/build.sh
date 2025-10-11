#!/bin/bash

cd ../
mkdir -p output/reverse_result
cd reverse 
touch output/RE.log
make && make run
cd ../tool
touch ../output/reverse_result/generate_matrix.log
python3 generate_matrix.py | tee ../output/reverse_result/generate_matrix.log
cd ../rhohammer && mkdir build && cd build && cmake .. && make -j $(nproc)
