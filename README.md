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

## Authors

- [@Youssif991](https://github.com/Youssif991)
- [@youssefteam18-boop](https://github.com/youssefteam18-boop)
- [@naderhany12](https://github.com/naderhany12)
- [@minawaeltanagho](https://github.com/minawaeltanagho)