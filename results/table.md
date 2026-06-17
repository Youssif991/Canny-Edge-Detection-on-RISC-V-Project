# Phase 4: Compiler Optimization Sweep

## Benchmark Results (100x75 Image, 100 Iterations, VLEN=128)

| Pipeline Stage | -O0 (Unoptimized) | -O2 | -O3 (Speed) | -O3 + Auto-vec |
| --- | --- | --- | --- | --- |
| **Gaussian Spatial** | 1.467 ms | 0.502 ms | 0.427 ms | 0.431 ms |
| **Gaussian Separable** | 0.786 ms | 0.294 ms | 0.240 ms | 0.242 ms |
| **Sobel (Standard)** | 2.159 ms | 0.085 ms | 0.076 ms | 0.081 ms |
| **Sobel Unbounded** | 0.175 ms | 0.047 ms | 0.040 ms | 0.044 ms |
| **Magnitude L1** | 1.517 ms | 0.038 ms | 0.036 ms | 0.036 ms |
| **Magnitude L2** | 2.793 ms | 0.811 ms | 0.789 ms | 0.804 ms |
| **Direction** | 1.462 ms | 0.054 ms | 0.056 ms | 0.060 ms |

### Binary Size (.text)

| Optimization Level | Size (Bytes) | Size (KB) |
| --- | --- | --- |
| **-O0** | 2,324,471 | ~2,324 KB |
| **-O2** | 1,981,179 | ~1,981 KB |
| **-O3** | 1,982,027 | ~1,982 KB |
| **-O3 + Auto-vec** | 1,982,027 | ~1,982 KB |

---

## Analysis & Discussion

**Measurement Methodology**  
QEMU is not cycle-accurate and does not simulate a real microarchitecture. The wall-clock timings above are meaningless in absolute terms. However, relative comparisons (e.g., `-O0` vs `-O3`) remain highly valid because the improvements are driven by a reduction in total executed instructions.


