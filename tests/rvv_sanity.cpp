/**
 * @file rvv_sanity.cpp
 * @brief Critical first test — verifies the full toolchain and QEMU chain.
 *        Compile with -march=rv64gcv and run on QEMU at VLEN 128, 256, 512.
 *        If all three print PASS, the entire toolchain is working.
 * @author Youssef
 */

#include <cstdint>
#include <cstdio>
#include <riscv_vector.h>

int main(void) {
    const int N = 16;
    int32_t a[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    int32_t b[16] = {16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1};
    int32_t c[16] = {0};

    size_t n = N;
    int32_t *pa = a, *pb = b, *pc = c;

    while (n > 0) {
        size_t vl = __riscv_vsetvl_e32m1(n);
        vint32m1_t va = __riscv_vle32_v_i32m1(pa, vl);
        vint32m1_t vb = __riscv_vle32_v_i32m1(pb, vl);
        vint32m1_t vc = __riscv_vadd_vv_i32m1(va, vb, vl);
        __riscv_vse32_v_i32m1(pc, vc, vl);
        pa += vl; pb += vl; pc += vl; n -= vl;
    }

    int ok = 1;
    for (int i = 0; i < N; i++) {
        if (c[i] != 17) {
            printf("FAIL at index %d: got %d expected 17\n", i, c[i]);
            ok = 0;
        }
    }
    printf("%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
