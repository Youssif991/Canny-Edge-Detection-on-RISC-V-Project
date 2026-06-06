# Canny-Edge-Detection-on-RISC-V-Project

## Project Overview
This project implements a Canny edge detection pipeline written in C++. It is targeted for RISC-V (`rv64gcv`) and runs on QEMU in user-mode emulation. 

The goal of this project is to measure performance from a scalar C++ baseline and optimize the hot kernels using RISC-V Vector (RVV) intrinsics.

## Prerequisites
To build and run this project from scratch, you will need the following environment setup:
* An appropriate operating system environment: Linux natively, WSL2 with Ubuntu 24.04 (for Windows), or Docker Desktop (for macOS).
* The RISC-V GNU toolchain built from source (with the `--with-arch=rv64gcv` flag to enable Vector extension support).
* QEMU built from source targeting `riscv64-linux-user`.
* GoogleTest for host-side unit testing.

## Getting Started

### 1. Clone the Repository
```bash
git clone https://github.com/Youssif991/Canny-Edge-Detection-on-RISC-V-Project.git
cd Canny-Edge-Detection-on-RISC-V-Project
```
### 2. Environment Setup (Toolchain & QEMU Build)
To compile and emulate RISC-V Vector (RVV) instructions, the project includes an automated setup script. This script installs required system dependencies, builds a custom RISC-V GCC bare-metal toolchain with vector extensions, builds QEMU user-mode with profiling plugins, builds GoogleTest for verification, and configures a Python virtual environment.

Run the full installation (this can take 30-90 minutes depending on your hardware):
```bash
# Make setup scripts executable
chmod +x toolchain-setup/*.sh

# Run the master setup script
./toolchain-setup/main.sh
```

> [!NOTE]
> The setup script is modular. You can inspect or run individual steps under the [toolchain-setup](file:///e:/Projects/Embedded/Dr.Omar%20Nasr/Project/Canny-Edge-Detection-on-RISC-V-Project/toolchain-setup) directory if needed.

After setup completes, reload your shell profile to apply the path changes:
```bash
source ~/.bashrc
```

The script automates the following steps:
* **Step 1 (`step1_prerequisites.sh`)**: Installs host system packages needed for compiling GCC, QEMU, CMake, and Doxygen. Supports Ubuntu/Debian/Pop!_OS and Arch/Manjaro.
* **Step 2 (`step2_toolchain.sh`)**: Clones and builds a bare-metal `riscv64-unknown-elf-g++` toolchain with RVV enabled (`--with-arch=rv64gcv`, `--with-abi=lp64d`) under `~/riscv-toolchain`.
* **Step 3 (`step3_qemu.sh`)**: Clones and builds QEMU 9.x+ with user-mode simulation and profiling plugins (`--enable-plugins`) under `~/qemu-install`.
* **Step 4 (`step4_project.sh`)**: Clones GoogleTest and compiles/installs it under `~/googletest-installed`.
* **Step 5 (`step5_python.sh`)**: Sets up a Python virtual environment (`.venv`) inside the project and installs `numpy`, `matplotlib`, and `PyQt5` for analysis and visualization.

---

### 3. Initialize the Project Structure
Initialize the directory structure and generate the scaffold source/header files:
```bash
chmod +x init_project.sh
./init_project.sh
```

---

## Directory Structure

```text
.
├── assets/                 # Test images (raw grayscale format)
├── build/                  # Build artifacts (ignored by Git)
│   ├── host/debug/         # Native host binaries (unit tests)
│   ├── target/debug/       # Target debug RISC-V ELF binaries
│   └── target/release/     # Cross-compiled production RISC-V ELF binaries
├── docs/                   # Documentation output
│   ├── html/               # HTML documentation (open index.html to view)
│   └── latex/              # LaTeX documentation
├── include/                # Header files for the Canny pipeline stages
│   ├── direction.hpp       # Gradient direction quantization interface
│   ├── gaussian.hpp        # 5x5 Gaussian filter interface
│   ├── image_io.hpp        # Grayscale Raw image load/save utility interface
│   ├── magnitude.hpp       # Gradient magnitude calculation interface
│   └── sobel.hpp           # Sobel operator (Gx, Gy) interface
├── src/                    # C++ source implementation files
│   ├── direction.cpp       # Quantization to 0, 45, 90, 135 degrees
│   ├── gaussian.cpp        # Scalar baseline for 5x5 Gaussian blur
│   ├── image_io.cpp        # Image load/save implementation
│   ├── magnitude.cpp       # L1 and L2 norm implementations
│   ├── main.cpp            # Main entry point and orchestration
│   └── sobel.cpp           # Scalar baseline for Sobel Gx/Gy
├── tests/                  # Test suites
│   └── host_tests.cpp      # GoogleTest suite for host-side stage testing
├── toolchain-setup/        # Modular bash files for RISC-V/QEMU setup
├── tools/                  # Script helpers (image convertors, visualizers, etc.)
├── Doxyfile                # Doxygen configuration file
├── Makefile                # GNU Makefile for building and running the project
└── README.md               # Project overview and instructions
```

---

## Canny Edge Detection Pipeline

The pipeline is split into modular stages corresponding to the steps of the Canny algorithm:

| Stage | Files | Description |
| :--- | :--- | :--- |
| **Image I/O** | [image_io.hpp](file:///e:/Projects/Embedded/Dr.Omar%20Nasr/Project/Canny-Edge-Detection-on-RISC-V-Project/include/image_io.hpp) / [image_io.cpp](file:///e:/Projects/Embedded/Dr.Omar%20Nasr/Project/Canny-Edge-Detection-on-RISC-V-Project/src/image_io.cpp) | Loads and saves raw grayscale images consisting of raw `width * height` byte data. |
| **Gaussian Blur** | [gaussian.hpp](file:///e:/Projects/Embedded/Dr.Omar%20Nasr/Project/Canny-Edge-Detection-on-RISC-V-Project/include/gaussian.hpp) / [gaussian.cpp](file:///e:/Projects/Embedded/Dr.Omar%20Nasr/Project/Canny-Edge-Detection-on-RISC-V-Project/src/gaussian.cpp) | Performs a 5x5 Gaussian convolution to remove noise and smooth the input image. |
| **Sobel Gradient** | [sobel.hpp](file:///e:/Projects/Embedded/Dr.Omar%20Nasr/Project/Canny-Edge-Detection-on-RISC-V-Project/include/sobel.hpp) / [sobel.cpp](file:///e:/Projects/Embedded/Dr.Omar%20Nasr/Project/Canny-Edge-Detection-on-RISC-V-Project/src/sobel.cpp) | Computes horizontal ($G_x$) and vertical ($G_y$) spatial derivatives using Sobel kernels. |
| **Gradient Magnitude**| [magnitude.hpp](file:///e:/Projects/Embedded/Dr.Omar%20Nasr/Project/Canny-Edge-Detection-on-RISC-V-Project/include/magnitude.hpp) / [magnitude.cpp](file:///e:/Projects/Embedded/Dr.Omar%20Nasr/Project/Canny-Edge-Detection-on-RISC-V-Project/src/magnitude.cpp) | Calculates the edge strength at each pixel. Supports L1 ($|G_x| + |G_y|$) and L2 ($\sqrt{G_x^2 + G_y^2}$) norms. |
| **Gradient Direction**| [direction.hpp](file:///e:/Projects/Embedded/Dr.Omar%20Nasr/Project/Canny-Edge-Detection-on-RISC-V-Project/include/direction.hpp) / [direction.cpp](file:///e:/Projects/Embedded/Dr.Omar%20Nasr/Project/Canny-Edge-Detection-on-RISC-V-Project/src/direction.cpp) | Quantizes the gradient angle into four discrete sectors ($0^\circ$, $45^\circ$, $90^\circ$, $135^\circ$) for Non-Maximum Suppression (NMS). |

---

## Compilation and Execution

A standard `Makefile` is provided to simplify building, running, and testing.

### Makefile Targets

* **Build RISC-V Binary**:
  ```bash
  make canny_rv
  ```
  Cross-compiles the C++ source files with the RISC-V bare-metal toolchain using vector extension configurations (`-march=rv64gcv -mabi=lp64d -O2`).
  
* **Run on QEMU User-Mode**:
  ```bash
  make run
  ```
  Invokes QEMU with a default Vector Length (`VLEN`) of 256 bits to execute the compiled RISC-V binary.
  
* **Verify Vector-Length-Agnostic (VLA) Correctness**:
  ```bash
  make run_vlen
  ```
  Runs the pipeline sequentially across different vector lengths (`VLEN = 128, 256, 512`) on QEMU to ensure the RVV implementation is agnostic to hardware register sizes.

* **Run Host Unit Tests**:
  ```bash
  make test
  ```
  Compiles the pipeline stages natively with GoogleTest using the host's `g++` and runs unit tests to verify mathematical correctness.

* **Run a Specific Test Binary on QEMU**:
  ```bash
  make run-test NAME=<test_name>
  ```

* **Clean Build Artifacts**:
  ```bash
  make clean
  ```

---

## Documentation

Source code documentation is generated from inline Doxygen-formatted comments. 

To generate the documentation:
```bash
make docs
```
Open `docs/html/index.html` in your web browser to explore the modules, class structure, and file dependencies.

---

## Authors

* [@Youssif991](https://github.com/Youssif991)
* [@youssefteam18-boop](https://github.com/youssefteam18-boop)
* [@naderhany12](https://github.com/naderhany12)
* [@minawaeltanagho](https://github.com/minawaeltanagho)
