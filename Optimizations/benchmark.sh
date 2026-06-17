#!/bin/bash
FLAGS=("-O0" "-O2" "-O3" "-Os" "-Ofast")
VLENS=(128 256 512)
OUTPUT_FILE="Optimizations/optimization_results.txt"
VEC_REPORT="Optimizations/vec_report.txt"

echo "Starting Benchmark..." > $OUTPUT_FILE
echo "Vectorization Reports" > $VEC_REPORT

for flag in "${FLAGS[@]}"; do
    for vlen in "${VLENS[@]}"; do
        echo "--------------------------" | tee -a $OUTPUT_FILE
        echo "Testing Flag: $flag, VLEN: $vlen" | tee -a $OUTPUT_FILE
        
        # Compile مع تقرير الـ Vectorization
        riscv64-linux-gnu-g++ -std=c++20 -fpermissive -fPIC \
            tests/gaussian_test.cpp src/gaussian.cpp src/instantiation.cpp \
            -Iinclude -Isrc -o "gaussian_${flag}_${vlen}" $flag \
            -fopt-info-vec-all 2>> $VEC_REPORT
        
        # قياس الحجم
        ls -lh "gaussian_${flag}_${vlen}" | tee -a $OUTPUT_FILE
        
        # التشغيل والقياس بدقة
        # استخدام time لقياس الوقت الفعلي للتنفيذ
        { time $HOME/qemu/build/qemu-riscv64 -L /usr/riscv64-linux-gnu -cpu rv64,v=true,vlen=$vlen,elen=64 "./gaussian_${flag}_${vlen}"; } 2>&1 | tee -a $OUTPUT_FILE
        
        rm "gaussian_${flag}_${vlen}"
    done
done