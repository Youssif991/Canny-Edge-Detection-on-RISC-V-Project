# Amdahl's Law Analysis — Canny Edge Detection Pipeline (`-O3`)
**Configuration: `-O3` | `VLEN=512`**

---

## 1. Profiling Baseline

The following profiling breakdown was measured on the **`-O3`** binary running under QEMU with `VLEN=512`:

| Stage        | Time (ms) | Percentage |
|--------------|-----------|------------|
| Gaussian     | 0.163     | 50.42%     |
| Sobel Gx/Gy  | 0.064     | 19.79%     |
| Magnitude    | 0.045     | 14.06%     |
| Direction    | 0.051     | 15.72%     |
| **Total**    | **0.323** | **100%**   |

---

## 2. Comparison: `-O0` vs `-O3`

| Stage        | `-O0` (ms) | `-O0` % | `-O3` (ms) | `-O3` % | Stage Speedup |
|--------------|------------|---------|------------|---------|---------------|
| Gaussian     | 0.539      | 11.96%  | 0.163      | 50.42%  | **3.3×**      |
| Sobel Gx/Gy  | 1.119      | 24.82%  | 0.064      | 19.79%  | **17.5×**     |
| Magnitude    | 2.021      | 44.84%  | 0.045      | 14.06%  | **44.9×**     |
| Direction    | 0.828      | 18.38%  | 0.051      | 15.72%  | **16.2×**     |
| **Total**    | **4.507**  | 100%    | **0.323**  | 100%    | **~13.9×**    |

> ⚠️ These are QEMU wall-clock ratios. See Section 8 for measurement caveats.

---

## 3. Amdahl's Law — Background

Amdahl's Law gives the theoretical maximum speedup when only a fraction `p` of a program is accelerated:

$$
S = \frac{1}{(1 - p) + \frac{p}{s}}
$$

Where:
- `S` = overall speedup of the whole pipeline
- `p` = fraction of total execution time accelerated (measured from **`-O0`** baseline)
- `s` = speedup factor applied to that fraction
- `(1 - p)` = unchanged fraction (the hard ceiling)

---

## 4. Validating the Total Speedup via Amdahl's Law

Using actual per-stage speedups from `-O0` → `-O3`:

$$
S_{total} = \frac{1}{\frac{0.1196}{3.3} + \frac{0.2482}{17.5} + \frac{0.4484}{44.9} + \frac{0.1838}{16.2}}
$$

$$
= \frac{1}{0.0362 + 0.0142 + 0.0100 + 0.0113} = \frac{1}{0.0717} \approx \mathbf{13.9×}
$$

This matches the measured 13.9× total speedup exactly — confirming the profiling data is internally consistent.

---

## 5. The Bottleneck Inversion

The most striking result is how the percentage distribution **completely inverted** from `-O0` to `-O3`:

| Stage        | `-O0` % | `-O3` % | Shift          |
|--------------|---------|---------|----------------|
| Gaussian     | 11.96%  | 50.42%  | ↑ **+38.46%**  |
| Sobel Gx/Gy  | 24.82%  | 19.79%  | ↓ −5.03%       |
| Magnitude    | **44.84%** | 14.06% | ↓ **−30.78%** |
| Direction    | 18.38%  | 15.72%  | ↓ −2.66%       |

This is Amdahl's Law in action:

- **Magnitude** was the `-O0` bottleneck (44.84%) and got the largest compiler speedup (44.9×), shrinking to just 14.06% of `-O3` runtime.
- **Gaussian** was the cheapest `-O0` stage (11.96%) but got the weakest compiler speedup (3.3×), becoming the **new dominant bottleneck** at 50.42% of `-O3` runtime.
- The bottleneck has **fully inverted** — the cheapest stage is now the most expensive.

**Why did Gaussian benefit least from `-O3`?**
The 5×5 convolution with boundary checks has complex control flow that prevents the compiler from auto-vectorizing the inner loop. `-O3` helps with register allocation and instruction scheduling, but cannot vectorize a loop it can't prove is safe. This is exactly why manual RVV intrinsics are needed for Gaussian specifically.

---

## 6. Per-Stage Analysis (Further Optimization Potential from `-O3`)

### 6.1 Gaussian Blur (`p = 0.1196` at `-O0`, now 50.42% of `-O3` runtime)

Gaussian is now the **dominant bottleneck** at `-O3`. Its 3.3× speedup from the compiler was the weakest, indicating the most room for manual RVV optimization.

| RVV speedup `s` on Gaussian | Additional Overall Speedup (on top of `-O3`) |
|-----------------------------|----------------------------------------------|
| 2×                          | 1.34×                                        |
| 4×                          | 1.50×                                        |
| 8×                          | 1.58×                                        |
| ∞                           | **2.02×**                                    |

**Implication:** Gaussian is the **top priority for RVV intrinsics**. A separable 1×5 + 5×1 decomposition and strip-mined multiply-accumulate could realistically achieve 4–8× on top of `-O3`.

---

### 6.2 Sobel Gx/Gy (`p = 0.2482` at `-O0`, now 19.79% of `-O3` runtime)

Sobel received a strong 17.5× compiler speedup. Its further ceiling is modest.

| RVV speedup `s` on Sobel | Additional Overall Speedup |
|--------------------------|---------------------------|
| 2×                       | 1.11×                     |
| 4×                       | 1.17×                     |
| ∞                        | **1.25×**                 |

**Implication:** Second priority for RVV after Gaussian, but returns are limited.

---

### 6.3 Direction (`p = 0.1838` at `-O0`, now 15.72% of `-O3` runtime)

Direction received a 16.2× compiler speedup and is branch-heavy. Further gains are limited.

| RVV speedup `s` on Direction | Additional Overall Speedup |
|------------------------------|---------------------------|
| 2×                           | 1.09×                     |
| ∞                            | **1.19×**                 |

**Implication:** Not worth manual RVV effort. Leave scalar.

---

### 6.4 Magnitude (`p = 0.4484` at `-O0`, now 14.06% of `-O3` runtime)

Magnitude was already the big winner at `-O3` (44.9× speedup). Its further ceiling is small.

| RVV speedup `s` on Magnitude | Additional Overall Speedup |
|------------------------------|---------------------------|
| 2×                           | 1.08×                     |
| ∞                            | **1.16×**                 |

**Implication:** The compiler already handled this well. No further action needed.

---

## 7. Optimization Priority Ranking (from `-O3` toward RVV)

| Priority | Stage       | `-O3` % | Compiler Speedup | RVV Potential | Recommended Action              |
|----------|-------------|---------|------------------|---------------|---------------------------------|
| 1 ✅     | Gaussian    | 50.42%  | 3.3× (weakest)   | High          | Separable filter + RVV intrinsics |
| 2 ⚠️    | Sobel       | 19.79%  | 17.5×            | Low (1.25×)   | RVV if time allows              |
| 3 ❌     | Direction   | 15.72%  | 16.2×            | Very low      | Leave scalar                    |
| 4 ❌     | Magnitude   | 14.06%  | 44.9× (best)     | Minimal       | Already optimal                 |

---

## 8. QEMU Measurement Disclaimer

> ⚠️ QEMU is **not cycle-accurate**. All measurements are wall-clock time via `clock_gettime(CLOCK_MONOTONIC)`, averaged over 100+ iterations.
>
> At sub-millisecond `-O3` runtimes (total: 0.323 ms), **timer resolution and QEMU scheduling noise** are significant. The percentage breakdown is more reliable than absolute values at this scale. Increase the benchmark loop to 1000+ iterations and report mean ± standard deviation for stable results.
>
> The 13.9× total speedup from `-O0` → `-O3` reflects a real reduction in emulated instruction count, but does not directly predict speedup on physical RISC-V silicon.

---

## 9. Summary

| Key Metric                                      | Value                        |
|-------------------------------------------------|------------------------------|
| Total `-O0` runtime                             | 4.507 ms                     |
| Total `-O3` runtime                             | 0.323 ms                     |
| Total compiler speedup (`-O0` → `-O3`)          | **~13.9×**                   |
| Strongest compiler speedup (per stage)          | Magnitude **44.9×**          |
| Weakest compiler speedup (per stage)            | Gaussian **3.3×**            |
| New bottleneck at `-O3`                         | Gaussian (50.42% of runtime) |
| Max further gain (Gaussian RVV, ∞×)             | **~2.02× additional**        |
| Next step                                       | RVV intrinsics on Gaussian   |

The compiler alone delivered **13.9× speedup** from `-O0` to `-O3` — primarily by optimizing Magnitude (44.9×) and Sobel (17.5×). The bottleneck has fully inverted: Gaussian is now the weak link. All RVV optimization effort should focus there first.