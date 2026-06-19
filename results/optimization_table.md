# Optimization Table

| Stage        | -O0      | -O2      | -O3      | Auto-vec | RVV 128  | RVV 256  |
|--------------|----------|----------|----------|----------|----------|----------|
| Gaussian 5x5 | 58.612 ms| 19.864 ms| 16.226 ms| 17.470 ms| 44.737 ms| 37.093 ms|
| Sobel Gx/Gy  | 72.005 ms| 3.157 ms | 3.133 ms | 3.227 ms | 6.732 ms | 6.322 ms |
| Magnitude    | 194.658ms| 4.524 ms | 5.534 ms | 4.222 ms | 5.789 ms | 4.226 ms |
| Direction    | 61.867 ms| 2.015 ms | 2.112 ms | 2.316 ms | scalar   | scalar   |
| Binary size  | 710738 B  | 637622 B  | 640270 B  | 640270  B  |  640334  B  |  640334 B  |

**Notes:**
- `-O0` / `-O2` / `-O3` / `Auto-vec` columns are the **scalar** kernel
  benchmarks (`tests/scalar_test.cpp`, 512x512 image, 100 iterations,
  Magnitude = L1 norm, Gaussian = spatial 5x5, Sobel = bounded/clamped).
- `RVV 128` / `RVV 256` columns are the **RVV-vectorized pipeline**
  timings (Gaussian=LMUL2, Sobel=LMUL2, Magnitude=LMUL4, `-O2` build,
  512x512 image) run under QEMU at `vlen=128` and `vlen=256` respectively.
- Direction has no RVV implementation — it runs scalar in every
  configuration, including the RVV builds.
- Binary size is the same `.elf` for the `-O3` and `Auto-vec` columns since
  `-ftree-vectorize` produced byte-identical code for these kernels. RVV
  128/256 share the same binary (only the QEMU `vlen` parameter at runtime
  differs, not the compiled binary).

