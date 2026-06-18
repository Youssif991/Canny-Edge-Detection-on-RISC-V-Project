# Compiler Optimization Analysis: -O0 vs -O3 (VLEN = 512)

## 1. Benchmark Results

| Stage        | -O0 (ms) | -O0 %  | -O3 (ms) | -O3 %  | Speedup S |
|--------------|----------|--------|----------|--------|-----------|
| Gaussian     | 0.539    | 11.96% | 0.163    | 50.42% | 3.31×     |
| Sobel Gx/Gy  | 1.119    | 24.82% | 0.064    | 19.79% | 17.48×    |
| Magnitude    | 2.021    | 44.84% | 0.045    | 14.06% | 44.91×    |
| Direction    | 0.828    | 18.38% | 0.051    | 15.72% | 16.24×    |
| **Total**    | **4.507**| 100%   | **0.323**| 100%   | **13.95×**|

---

## 2. Amdahl's Law Analysis

### 2.1 Formula & What p Means Here

Amdahl's Law for a single vectorized code running with VLEN = N:

$$S = \frac{1}{(1 - p) + \frac{p}{N}}$$

Where:
- **S** = measured speedup of that stage = T_O0 / T_O3
- **N** = 512 (vector length, i.e. how many data elements processed in parallel per cycle)
- **p** = the fraction of the stage's code that is **fully vectorizable** (parallelizable by SIMD)
- **(1 - p)** = the serial fraction that the compiler cannot vectorize (loop overhead, branches, memory latency, scalar tail loops, etc.)

Each stage is **part of one single program**. Each has its own measured speedup, and from that we solve for its own **p** — the degree to which the compiler successfully vectorized it using VLEN=512.

---

### 2.2 Solving for p Per Stage

Rearranging Amdahl's formula to isolate p:

$$S \left[(1-p) + \frac{p}{N}\right] = 1$$

$$S - Sp + \frac{Sp}{N} = 1$$

$$p \left(\frac{S}{N} - S\right) = 1 - S$$

$$\boxed{p = \frac{1 - S}{S\left(\frac{1}{N} - 1\right)}}$$

Applying this with **N = 512** for each stage:

---

#### Gaussian Blur — S = 3.3067×

$$p_{Gaussian} = \frac{1 - 3.3067}{3.3067 \times \left(\frac{1}{512} - 1\right)} = \frac{-2.3067}{3.3067 \times (-0.998)} \approx \boxed{0.699}$$

→ Only **69.9%** of Gaussian is vectorizable. The remaining 30.1% is serial (boundary handling, 2D kernel loops with data dependencies).  
→ This explains the modest speedup of **3.31×** despite N=512.

---

#### Sobel Gx/Gy — S = 17.4844×

$$p_{Sobel} = \frac{1 - 17.484}{17.484 \times \left(\frac{1}{512} - 1\right)} \approx \boxed{0.9447}$$

→ **94.47%** of Sobel is vectorizable. Straightforward convolution with no data dependencies across pixels.  
→ Good speedup of **17.48×**.

---

#### Magnitude — S = 44.9111×

$$p_{Magnitude} = \frac{1 - 44.911}{44.911 \times \left(\frac{1}{512} - 1\right)} \approx \boxed{0.9796}$$

→ **97.96%** vectorizable — the highest of all stages. Pure arithmetic: `sqrt(Gx² + Gy²)` with no branching or inter-pixel dependency. The compiler vectorized it almost perfectly.  
→ Highest speedup at **44.91×**, very close to the theoretical limit of 512.

---

#### Direction — S = 16.2353×

$$p_{Direction} = \frac{1 - 16.235}{16.235 \times \left(\frac{1}{512} - 1\right)} \approx \boxed{0.9402}$$

→ **94.02%** vectorizable. `atan2(Gy, Gx)` is mostly parallelizable but the transcendental function introduces some scalar overhead.  
→ Speedup of **16.24×**.

---

### 2.3 Summary Table

| Stage        |   S (measured) |       p (vectorized fraction) | Serial fraction (1-p) | Theoretical max S (p→1) |
|--------------|---------------:|------------------------------:|----------------------:|------------------------:|
| Gaussian     |         3.31×  |                   **0.6990**  |              **30.1%**|                  512×   |
| Sobel Gx/Gy  |        17.48×  |                   **0.9447**  |               **5.5%**|                  512×   |
| Magnitude    |        44.91×  |                   **0.9796**  |               **2.0%**|                  512×   |
| Direction    |        16.24×  |                   **0.9402**  |               **5.9%**|                  512×   |
| **Total**    |    **13.95×**  |               **0.9302**      |           **6.98%**   |               **512×**  |

The theoretical maximum speedup for any stage (if p = 1.0) would be **512×**. The gap between actual and theoretical speedup is entirely explained by the serial fraction **(1 - p)**.

---

### 2.4 Bottleneck Shift (Amdahl Effect)

After -O3 optimization, the **bottleneck changed completely**:

| Stage       | -O0 share (bottleneck before) | -O3 share (bottleneck after) |
|-------------|-------------------------------|------------------------------|
| Gaussian    | 11.96%                        | **50.42%** ← new bottleneck  |
| Sobel Gx/Gy | 24.82%                        | 19.79%                       |
| Magnitude   | **44.84%** ← old bottleneck   | 14.06%                       |
| Direction   | 18.38%                        | 15.72%                       |

Magnitude was the worst stage in -O0 but achieved p = 0.98 and nearly fully utilized VLEN=512. Gaussian had the lowest p = 0.699, so it was barely sped up and is now consuming half the runtime. **Gaussian is the critical path.**


---

## 3.1 Gaussian — Increasing p (current p = 0.699)

Gaussian is the **critical bottleneck** (50.42% of -O3 runtime) and has the most serial code (30.1%). It also has the **largest weight room** for improvement — every point of p gained here has the highest leverage on total speedup.

| p_new | $S_{Gaussian}$ | $S_{total}$ | Gain over current -O3 |
|-------|---------------|-------------|----------------------|
| 0.75  | 3.98×         | 15.25×      | **+1.093×**          |
| 0.80  | 4.96×         | 16.78×      | **+1.202×**          |
| 0.85  | 6.59×         | 18.64×      | **+1.336×**          |
| 0.90  | 9.83×         | 20.98×      | **+1.503×**          |
| 0.95  | 19.28×        | 23.98×      | **+1.719×**          |
| 0.99  | 83.80×        | 27.08×      | **+1.941×**          |
| 1.00  | 512.00×       | 27.98×      | **+2.006×**          |

**Key insight:** Even pushing Gaussian to p = 1.00 (perfect vectorization) only yields **2.006× additional gain** over current -O3. This is the **hard ceiling** Amdahl's Law sets for Gaussian alone — because it only contributes 11.96% of total -O0 time, no matter how fast it becomes, the other stages still dominate.

The jump from p = 0.95 → 0.99 is large in stage speedup (19× → 84×) but small in total gain (1.719× → 1.941×) — clear evidence of diminishing returns as Gaussian's share shrinks.

---

## 3.2 Sobel Gx/Gy — Increasing p (current p = 0.9447)

Sobel is already well-vectorized. Its remaining serial fraction is only 5.5%, so the improvement ceiling is modest.

| p_new | $S_{Sobel}$ | $S_{total}$ | Gain over current -O3 |
|-------|------------|-------------|----------------------|
| 0.95  | 19.28×     | 14.22×      | **+1.019×**          |
| 0.99  | 83.80×     | 16.55×      | **+1.186×**          |
| 1.00  | 512.00×    | 17.26×      | **+1.237×**          |

**Key insight:** Perfect vectorization of Sobel (p → 1.0) gives only **1.237× additional gain**. Despite Sobel having the second-largest weight (24.82% of -O0 time), its p is already high, so the marginal return is low. Not a priority for RVV effort.

---

## 3.3 Magnitude — Increasing p (current p = 0.9796)

Magnitude is already near-perfect. Only 2% serial remains — very little room left.

| p_new | $S_{Magnitude}$ | $S_{total}$ | Gain over current -O3 |
|-------|----------------|-------------|----------------------|
| 0.99  | 83.80×         | 14.92×      | **+1.069×**          |
| 1.00  | 512.00×        | 15.99×      | **+1.146×**          |

**Key insight:** Even achieving p = 1.00 on Magnitude — which already had the best compiler speedup (44.91×) — adds only **1.146× total gain**. Despite being the heaviest stage by -O0 weight (44.84%), it is already so fast that further improvement is almost irrelevant to total time.

> This is Amdahl's Law at its most striking: the stage with the **most work** contributes the **least further gain** once it has been well-optimized.

---

## 3.4 Direction — Increasing p (current p = 0.9402)

Direction has a 5.9% serial fraction — slightly higher than Sobel — but also the smallest -O0 weight (18.38%).

| p_new | $S_{Direction}$ | $S_{total}$ | Gain over current -O3 |
|-------|----------------|-------------|----------------------|
| 0.95  | 19.28×         | 14.31×      | **+1.026×**          |
| 0.99  | 83.80×         | 15.99×      | **+1.146×**          |
| 1.00  | 512.00×        | 16.47×      | **+1.180×**          |

**Key insight:** Direction's ceiling is **1.180×** additional gain at p = 1.00. Similar story to Sobel — already reasonably vectorized, low serial fraction, and relatively small weight. Not worth manual RVV effort.

---


## 4. Further Optimization Potential

| Stage       | p       |  -o3%    | RVV OPT.  Needed       |
|-------------|---------|----------|------------------------|
| Gaussian    | 0.699   |  50.42%  | 🔴 Level 1 — Critical |
| Sobel Gx/Gy | 0.945   |  19.79%  | 🟠 Level 2 — Optional |
| Direction   | 0.940   |  15.72%  | 🟡 Level 3 — Skip     |
| Magnitude   | 0.980   |  14.06%  | 🟢 Level 4 — Not needed|

The compiler with -O3 VLEN=512 achieved **13.95× total speedup**, which is strong but far below the theoretical 512×. The serial fractions — especially Gaussian's 30.1% — are the fundamental limiters per Amdahl's Law. Eliminating them via algorithm restructuring (separable filter, stage fusion) is the highest-return next step.
