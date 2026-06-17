#include "sobel.cpp"
#include "magnitude.cpp"
#include "direction.cpp"
#include "sobel.hpp"
#include <ctime>
#include <iostream>

// هذا يضمن أن الدوال يتم بناؤها هنا في هذا الملف
template Status processing::sobel_3x3<uint8_t, int16_t>(const image::io::metadata_t<uint8_t>&, image::io::metadata_t<int16_t>&, image::io::metadata_t<int16_t>&);
template Status processing::MagL2<uint8_t, int16_t, float>(const image::io::metadata_t<uint8_t>&, const int16_t*, const int16_t*);
template Status processing::Direction<uint8_t, int16_t>(const image::io::metadata_t<uint8_t>&, const int16_t*, const int16_t*);

Status sobel_3x3_measured(const image::io::metadata_t<uint8_t>& in, image::io::metadata_t<int16_t>& gx, image::io::metadata_t<int16_t>& gy) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for(int i=0; i<100; i++) { 
        processing::sobel_3x3(in, gx, gy);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    std::cout << "Sobel 100 iterations time: " << (elapsed * 1000) / 100 << " ms" << std::endl;
    return Status::E_OK;
}