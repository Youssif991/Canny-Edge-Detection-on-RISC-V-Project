#!/bin/bash
# Script to automate Compiler Flags AND VLEN sweep
FLAGS=("-Os" "-Ofast")
VLENS=(128 256 512)
OUTPUT_FILE="Optimizations/optimization_results.txt"

echo "Starting Benchmark..." | tee $OUTPUT_FILE

for flag in "${FLAGS[@]}"; do
    for vlen in "${VLENS[@]}"; do
        echo "--------------------------" | tee -a $OUTPUT_FILE
        echo "Testing Flag: $flag, VLEN: $vlen" | tee -a $OUTPUT_FILE
        
        # Compile
        riscv64-linux-gnu-g++ -std=c++23 -march=rv64gcv -mabi=lp64d $flag -static -Iinclude tests/gaussian_test.cpp src/gaussian.cpp src/magnitude.cpp src/direction.cpp src/sobel.cpp -o gaussian_${flag}_${vlen}
        
        # Run
        { $HOME/qemu/build/qemu-riscv64 -cpu rv64,v=true,vlen=$vlen,elen=64 ./gaussian_${flag}_${vlen}; } 2>&1 | tee -a $OUTPUT_FILE
        
        # File Size
        ls -lh gaussian_${flag}_${vlen} | tee -a $OUTPUT_FILE
        
        # Cleanup binary to keep folder clean
        rm gaussian_${flag}_${vlen}
    done
done
