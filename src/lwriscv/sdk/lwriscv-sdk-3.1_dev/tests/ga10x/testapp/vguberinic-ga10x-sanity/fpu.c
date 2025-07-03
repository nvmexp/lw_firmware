/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#include "main.h"

#define matrixMultiply(z, a, b) do { \
z##11 = a##11 * b##11 + a##12 * b##21 + a##13 * b##31; \
z##12 = a##11 * b##12 + a##12 * b##22 + a##13 * b##32; \
z##13 = a##11 * b##13 + a##12 * b##23 + a##13 * b##33; \
z##21 = a##21 * b##11 + a##22 * b##21 + a##23 * b##31; \
z##22 = a##21 * b##12 + a##22 * b##22 + a##23 * b##32; \
z##23 = a##21 * b##13 + a##22 * b##23 + a##23 * b##33; \
z##31 = a##31 * b##11 + a##32 * b##21 + a##33 * b##31; \
z##32 = a##31 * b##12 + a##32 * b##22 + a##33 * b##32; \
z##33 = a##31 * b##13 + a##32 * b##23 + a##33 * b##33; \
} while(0)

static LwU64 fpuTestMatrix(LwU32 intParam)
{
    // On 10/2/2019 this monstrosity compiled to use all 32 fp regs.

    LwF32 a11 = 1.0f, a12 = 2.0f, a13 = 3.0f, a21 = 4.0f, a22 = 5.0f, a23 = 6.0f, a31 = 7.0f, a32 = 8.0f, a33 = 9.0f;
    LwF32 b11 = 0.0f, b12 = 0.0f, b13 = 0.0f, b21 = 0.0f, b22 = 0.0f, b23 = 0.0f, b31 = 0.0f, b32 = 0.0f, b33 = 0.0f;
    LwF32 c11 = 0.0f, c12 = 0.0f, c13 = 0.0f, c21 = 0.0f, c22 = 0.0f, c23 = 0.0f, c31 = 0.0f, c32 = 0.0f, c33 = 0.0f;
    LwF32 d11 = 1.0f, d12 = 2.0f, d13 = 3.0f, d21 = 4.0f, d22 = 5.0f, d23 = 6.0f, d31 = 7.0f, d32 = 8.0f, d33 = 9.0f;

    LwF32 param = 1.0f;
    LwF32 baseParam = intParam;
    LwU32 idx;
    LwU64 errors = 0;

    b11 = b12 = b13 = b21 = b22 = b23 = b31 = b32 = b33 = 0.f;

    for (idx = 0; idx < 2; ++idx)
    {
        param = baseParam;

        // Multiply by identity matrix multiplied by constant
        b11 = b22 = b33 = baseParam;
        matrixMultiply(c, a, b);
        b11 = b22 = b33 = b12;

        // Expect every element to be scaled up by constant
        errors += c11 != param * a11;
        errors += c12 != param * a12;
        errors += c13 != param * a13;
        errors += c21 != param * a21;
        errors += c22 != param * a22;
        errors += c23 != param * a23;
        errors += c31 != param * a31;
        errors += c32 != param * a32;
        errors += c33 != param * a33;

        // Multiplying by b will mirror matrix on vertical axis
        b13 = b22 = b31 = 1.0f;
        matrixMultiply(a, c, b);
        b13 = b22 = b31 = b12;

        // Check if matrix is mirrored
        errors += c13 != a11;
        errors += c12 != a12;
        errors += c11 != a13;
        errors += c23 != a21;
        errors += c22 != a22;
        errors += c21 != a23;
        errors += c33 != a31;
        errors += c32 != a32;
        errors += c31 != a33;

        // Multiplying by b will mirror matrix on horizontal axis
        b13 = b22 = b31 = 1.0f;
        matrixMultiply(c, b, a);
        b13 = b22 = b31 = b12;

        // Check if matrix is mirrored
        errors += c31 != a11;
        errors += c32 != a12;
        errors += c33 != a13;
        errors += c21 != a21;
        errors += c22 != a22;
        errors += c23 != a23;
        errors += c11 != a31;
        errors += c12 != a32;
        errors += c13 != a33;

        b11 = b22 = b33 = 1.0f;
        matrixMultiply(a, c, b);
        b11 = b22 = b33 = b12;

        // Now a is rotated twice, rotate d twice in other direction

        // Multiplying by b will mirror matrix on horizontal axis
        b13 = b22 = b31 = 1.0f;
        matrixMultiply(c, b, d);
        b13 = b22 = b31 = b12;

        // Check if matrix is mirrored
        errors += c31 != d11;
        errors += c32 != d12;
        errors += c33 != d13;
        errors += c21 != d21;
        errors += c22 != d22;
        errors += c23 != d23;
        errors += c11 != d31;
        errors += c12 != d32;
        errors += c13 != d33;

        // Multiplying by b will mirror matrix on vertical axis
        b13 = b22 = b31 = 1.0f;
        matrixMultiply(d, c, b);
        b13 = b22 = b31 = b12;

        // Check if matrix is mirrored
        errors += c13 != d11;
        errors += c12 != d12;
        errors += c11 != d13;
        errors += c23 != d21;
        errors += c22 != d22;
        errors += c21 != d23;
        errors += c33 != d31;
        errors += c32 != d32;
        errors += c31 != d33;

        a11 -= (baseParam - 1) * d11;
        a12 -= (baseParam - 1) * d12;
        a13 -= (baseParam - 1) * d13;
        a21 -= (baseParam - 1) * d21;
        a22 -= (baseParam - 1) * d22;
        a23 -= (baseParam - 1) * d23;
        a31 -= (baseParam - 1) * d31;
        a32 -= (baseParam - 1) * d32;
        a33 -= (baseParam - 1) * d33;

        // Check if transform is correct
        errors += d13 != a13;
        errors += d12 != a12;
        errors += d11 != a11;
        errors += d23 != a23;
        errors += d22 != a22;
        errors += d21 != a21;
        errors += d33 != a33;
        errors += d32 != a32;
        errors += d31 != a31;

        // Theoretically pattern repeats every 2 cycles
    }

    return errors;
}

void fpuTest()
{
    LwU64 mstatus = csr_read(LW_RISCV_CSR_MSTATUS);
    LwU64 fflags;
    LwU64 fcsr;

    printf("Starting fpuTest\n");

    csr_write(LW_RISCV_CSR_MSTATUS, FLD_SET_DRF(_RISCV, _CSR_MSTATUS, _FS, _OFF, mstatus));
    exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_ILL);
    __asm__ volatile ("fadd.s  fs0,fs0,fa5");
    if (exceptionExpectSet != LW_FALSE)
        HALT();

    // TODO: Optionally verify if state is here qNAN as Aote says it should be

    csr_write(LW_RISCV_CSR_MSTATUS, FLD_SET_DRF(_RISCV, _CSR_MSTATUS, _FS, _INITIAL, mstatus));
    if (fpuTestMatrix(3) != 0)
        HALT();

    // TODO: do some fpu math and verify fflags frm fcsr change
    printf("%llx %llx %llx\n", 
        csr_read(LW_RISCV_CSR_FFLAGS),
        csr_read(LW_RISCV_CSR_FRM),
        csr_read(LW_RISCV_CSR_FCSR));

    volatile float a = 1.0f;
    volatile float b = 0.0f;
    volatile float c;

    c = a / b;

    (void)c;

    fflags = csr_read(LW_RISCV_CSR_FFLAGS);
    fcsr = csr_read(LW_RISCV_CSR_FCSR);
    printf("%llx %llx %llx\n", fflags, csr_read(LW_RISCV_CSR_FRM), fcsr);

    // fcsr.fflags should == fflags
    if (DRF_VAL64(_RISCV_CSR, _FCSR, _FFLAGS, fcsr) !=
        DRF_VAL64(_RISCV_CSR, _FFLAGS, _FFLAGS, fflags))
    {
        printf("fflags mismatch.\n");
        HALT();
    }
    // We expect division by 0, no macro for that :(
    if (DRF_VAL64(_RISCV_CSR, _FCSR, _FFLAGS, fcsr) != 8)
    {
        printf("Invalid FPU exception. Expected division by zero.\n");
        HALT();
    }

    csr_write(LW_RISCV_CSR_MSTATUS, mstatus);
    printf("Finished fpuTest\n");
}


