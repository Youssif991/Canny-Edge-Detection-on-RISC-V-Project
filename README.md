# Canny-Edge-Detection-on-RISC-V-Project

[![Doxygen](https://img.shields.io/badge/docs-Doxygen-blue)](docs/html/index.html)

Canny edge detection pipeline in C++ targeting RISC-V Vector (RVV) acceleration, validated on `qemu-riscv64`. Full architecture and API documentation lives in the [Doxygen docs](docs/html/index.html).

## Prerequisites

- RISC-V GNU toolchain: `riscv64-unknown-elf-g++` (`--with-arch=rv64gcv --with-abi=lp64d`)
- `qemu-riscv64` user-mode emulator
- GoogleTest at `$(HOME)/googletest-installed`
- Doxygen 1.9.8

## Setup

```bash
git clone https://github.com/Youssif991/Canny-Edge-Detection-on-RISC-V-Project.git
cd Canny-Edge-Detection-on-RISC-V-Project
chmod +x toolchain-setup/*.sh
./toolchain-setup/main.sh
source ~/.bashrc
```

The setup script builds the RISC-V toolchain, QEMU, GoogleTest, and a Python venv in sequence.

## Build & Run

| Command | Description |
| :--- | :--- |
| `make test` | Host-side GoogleTest suite |
| `make image-io-test` | Image I/O regression test |
| `make canny_rv` | Cross-compile for RISC-V → `build/target/release/canny_rv.elf` |
| `make run` | Run RISC-V binary under QEMU (`vlen=256`) |
| `make run_vlen` | Validate across VLEN 128 / 256 / 512 |
| `make docs` | Generate Doxygen HTML |
| `make clean` | Remove build artifacts |

## Documentation

```bash
make docs
xdg-open docs/html/index.html   # Linux
```
## Phase 4: Compiler Optimization Sweep

The scalar Gaussian blur kernel (both spatial 5×5 and separable implementations) was cross-compiled with five optimization levels and benchmarked on `qemu-riscv64` at VLEN 128/256/512 to establish a baseline before RVV intrinsic work begins.

| Flag | VLEN | Spatial Gaussian (ms) | Separable Gaussian (ms) | Binary Size |
| :--- | :--- | :---: | :---: | :---: |
| `-O0` | 128 | 39 | 22 | 3.6M |
| `-O0` | 256 | 34 | 18 | 3.6M |
| `-O0` | 512 | 36 | 21 | 3.6M |
| `-O2` | 128 | 14 | 8 | 2.4M |
| `-O2` | 256 | 13 | 6 | 2.4M |
| `-O2` | 512 | 15 | 7 | 2.4M |
| `-O3` | 128 | 11 | 6 | 2.4M |
| `-O3` | 256 | 12 | 6 | 2.4M |
| `-O3` | 512 | 11 | 6 | 2.4M |
| `-Os` | 128 | 14 | 8 | 3.0M |
| `-Os` | 256 | 16 | 9 | 3.0M |
| `-Os` | 512 | 15 | 9 | 3.0M |
| `-Ofast` | 128 | 10 | 6 | 2.4M |
| `-Ofast` | 256 | 11 | 6 | 2.4M |
| `-Ofast` | 512 | 11 | 6 | 2.4M |

**Observations:**
- `-O0` is roughly 3× slower than any optimized flag and produces the largest binary — expected, since it disables nearly all compiler optimization passes.
- `-O3` and `-Ofast` give the best runtime, edging out `-O2` slightly, with identical binary size.
- `-Os` trades binary size for speed less favorably than expected here: it's smaller than `-O0` but not smaller than `-O2`/`-O3`/`-Ofast`, while still being the slowest among the optimized flags.
- The separable Gaussian implementation consistently outperforms the spatial 5×5 convolution across every flag, confirming the expected algorithmic advantage (O(n) vs O(n²) per pixel for a 5×5 kernel).
## Authors

- [@Youssif991](https://github.com/Youssif991)
- [@youssefteam18-boop](https://github.com/youssefteam18-boop)
- [@naderhany12](https://github.com/naderhany12)
- [@minawaeltanagho](https://github.com/minawaeltanagho)
