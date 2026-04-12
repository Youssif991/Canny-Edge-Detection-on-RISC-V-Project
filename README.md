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
