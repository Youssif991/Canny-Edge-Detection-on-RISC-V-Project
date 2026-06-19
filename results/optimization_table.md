# Optimization Table

| Stage        | -O0      | -O2      | -O3      | Auto-vec | RVV 128  | RVV 256  |
|--------------|----------|----------|----------|----------|----------|----------|
| Gaussian 5x5 | 52.696 ms| 17.979 ms| 14.685 ms| 14.617 ms| 32.828 ms| 26.643 ms|
| Sobel Gx/Gy  | 84.337 ms| 2.624 ms | 2.763 ms | 2.679 ms | 5.812 ms | 5.111 ms |
| Magnitude    | 54.682 ms| 1.209 ms | 1.226 ms | 1.218 ms | 5.349 ms | 4.504 ms |
| Direction    | 52.698 ms| 2.160 ms | 2.078 ms | 2.072 ms | scalar   | scalar   |
| Binary size  | 2386 KB  | 2041 KB  | 2042 KB  | 2042 KB  | 836 KB  | 836 KB  |

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

