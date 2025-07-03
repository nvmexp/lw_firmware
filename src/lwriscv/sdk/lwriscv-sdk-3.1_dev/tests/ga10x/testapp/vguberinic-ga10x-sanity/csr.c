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

void csrTest()
{
    LwS32 i;

    printf("Starting csrTest\n");

    /*
     * Verify PROD values of informational registers
     */
    LwU64 mvendorid = csr_read(LW_RISCV_CSR_MVENDORID);
    LwU64 marchid   = csr_read(LW_RISCV_CSR_MARCHID);
    LwU64 mimpid    = csr_read(LW_RISCV_CSR_MIMPID);
    LwU64 mhartid   = csr_read(LW_RISCV_CSR_MHARTID);
    LwU64 misa      = csr_read(LW_RISCV_CSR_MISA);

    if (!FLD_TEST_DRF64(_RISCV, _CSR_MVENDORID, _VENDORH, _RST, mvendorid))
        HALT();
    if (!FLD_TEST_DRF64(_RISCV, _CSR_MVENDORID, _VENDORL, _RST, mvendorid))
        HALT();

    if (!FLD_TEST_DRF64(_RISCV, _CSR_MARCHID, _MSB, _RST, marchid))
        HALT();
    if (!FLD_TEST_DRF64(_RISCV, _CSR_MARCHID, _RS1, _RST, marchid))
        HALT();
    if (!FLD_TEST_DRF64(_RISCV, _CSR_MARCHID, _CORE_MAJOR, _RST, marchid))
        HALT();
    if (!FLD_TEST_DRF64(_RISCV, _CSR_MARCHID, _CORE_MINOR, _RST, marchid))
        HALT();

    if (!FLD_TEST_DRF64(_RISCV, _CSR_MIMPID, _IMPID, _RST, mimpid))
        HALT();

    if (!FLD_TEST_DRF64(_RISCV, _CSR_MHARTID, _HART, _RST, mhartid))
        HALT();

    /*
     * Verify misa had needed fields and is ignoring writes
     */
    csr_write(LW_RISCV_CSR_MISA, 0x0);
    if (csr_read(LW_RISCV_CSR_MISA) != misa)
        HALT();

    csr_write(LW_RISCV_CSR_MISA, 0xFFFFFFFFFFFFFFFFU);
    if (csr_read(LW_RISCV_CSR_MISA) != misa)
        HALT();

    if (misa != ((1 << ('c' - 'a')) |
                 (1 << ('f' - 'a')) |
                 (1 << ('i' - 'a')) |
                 (1 << ('m' - 'a')) |
                 (1 << ('s' - 'a')) |
                 (1 << ('u' - 'a')) |
                 (1 << ('x' - 'a')) |
                 DRF_DEF64(_RISCV, _CSR_MISA, _MXL, _64)))
        HALT();

    /*
     * Verify mscratch and friends are writable and non-aliased
     */
    csr_write(LW_RISCV_CSR_MSCRATCH, 0x1);
    csr_write(LW_RISCV_CSR_MSCRATCH2, 0x2);
    csr_write(LW_RISCV_CSR_SSCRATCH, 0x3);
    csr_write(LW_RISCV_CSR_SSCRATCH2, 0x4);

    if (csr_read(LW_RISCV_CSR_MSCRATCH) != 0x1)
        HALT();
    if (csr_read(LW_RISCV_CSR_MSCRATCH2) != 0x2)
        HALT();
    if (csr_read(LW_RISCV_CSR_SSCRATCH) != 0x3)
        HALT();
    if (csr_read(LW_RISCV_CSR_SSCRATCH2) != 0x4)
        HALT();

    mToS();
    {
        exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_ILL);
        csr_write(LW_RISCV_CSR_MSCRATCH, 0x5);
        if (exceptionExpectSet != LW_FALSE)
            HALT();

        exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_ILL);
        csr_write(LW_RISCV_CSR_MSCRATCH2, 0x6);
        if (exceptionExpectSet != LW_FALSE)
            HALT();

        csr_write(LW_RISCV_CSR_SSCRATCH, 0x7);
        csr_write(LW_RISCV_CSR_SSCRATCH2, 0x8);

        if (csr_read(LW_RISCV_CSR_SSCRATCH) != 0x7)
            HALT();
        if (csr_read(LW_RISCV_CSR_SSCRATCH2) != 0x8)
            HALT();
    }
    sToM();

    mToU();
    {
        exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_ILL);
        csr_write(LW_RISCV_CSR_MSCRATCH, 0x9);
        if (exceptionExpectSet != LW_FALSE)
            HALT();

        exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_ILL);
        csr_write(LW_RISCV_CSR_MSCRATCH2, 0xa);
        if (exceptionExpectSet != LW_FALSE)
            HALT();

        exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_ILL);
        csr_write(LW_RISCV_CSR_SSCRATCH, 0xb);
        if (exceptionExpectSet != LW_FALSE)
            HALT();

        exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_ILL);
        csr_write(LW_RISCV_CSR_SSCRATCH2, 0xc);
        if (exceptionExpectSet != LW_FALSE)
            HALT();
    }
    uToM();

    /*
     * Try to poke mhpmevent and mhpmcounter and hpmcounter from U mode
     */
    mToU();
    {
        for (i = 3; i < 32; ++i)
        {
            // Can't write
            exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_ILL);
loop:
            csr_write(LW_RISCV_CSR_MHPMEVENT(3), 0xFFFFFFFFFFFFFFFF);
            *((LwU16 *) ((LwU64) (&&loop) + 4)) += 0x10;
            __asm__ volatile ("fence.i");

            if (exceptionExpectSet != LW_FALSE)
                HALT();

            // Can't read
            exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_ILL);
loop2:
            csr_read(LW_RISCV_CSR_MHPMEVENT(3));
            *((LwU16 *) ((LwU64) (&&loop2) + 2)) += 0x10;
            __asm__ volatile ("fence.i");

            if (exceptionExpectSet != LW_FALSE)
                HALT();
        }

        for (i = 0; i < 32; ++i)
        {

            // Can't read mhpmcounter
            exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_ILL);
loop3:
            csr_read(LW_RISCV_CSR_MHPMCOUNTER(0));
            *((LwU16 *) ((LwU64) (&&loop3) + 2)) += 0x10;
            __asm__ volatile ("fence.i");

            if (exceptionExpectSet != LW_FALSE)
                HALT();

            // Can't write mhpmcounter
            exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_ILL);
loop4:
            csr_write(LW_RISCV_CSR_MHPMCOUNTER(0), 0xFFFFFFFFFFFFFFFF);
            *((LwU16 *) ((LwU64) (&&loop4) + 4)) += 0x10;
            __asm__ volatile ("fence.i");

            if (exceptionExpectSet != LW_FALSE)
                HALT();

            // Can't write hpcounter
            exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_ILL);
loop5:
            csr_write(LW_RISCV_CSR_HPMCOUNTER(0), 0xFFFFFFFFFFFFFFFF);
            *((LwU16 *) ((LwU64) (&&loop5) + 4)) += 0x10;
            __asm__ volatile ("fence.i");

            if (exceptionExpectSet != LW_FALSE)
                HALT();
        }
    }
    uToM();

    /*
    * Try to poke mhpmevent and mhpmcounter and hpmcounter from S mode
    */
   mToS();
   {
       for (i = 3; i < 32; ++i)
       {
           // Can't write
           exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_ILL);
loop6:
           csr_write(LW_RISCV_CSR_MHPMEVENT(3), 0xFFFFFFFFFFFFFFFF);
           *((LwU16 *) ((LwU64) (&&loop6) + 4)) += 0x10;
           __asm__ volatile ("fence.i");

           if (exceptionExpectSet != LW_FALSE)
               HALT();

           // Can't read
           exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_ILL);
loop7:
           csr_read(LW_RISCV_CSR_MHPMEVENT(3));
           *((LwU16 *) ((LwU64) (&&loop7) + 2)) += 0x10;
           __asm__ volatile ("fence.i");

           if (exceptionExpectSet != LW_FALSE)
               HALT();
       }

       for (i = 0; i < 32; ++i)
       {

           // Can't read mhpmcounter
           exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_ILL);
loop8:
           csr_read(LW_RISCV_CSR_MHPMCOUNTER(0));
           *((LwU16 *) ((LwU64) (&&loop8) + 2)) += 0x10;
           __asm__ volatile ("fence.i");

           if (exceptionExpectSet != LW_FALSE)
               HALT();

           // Can't write mhpmcounter
           exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_ILL);
loop9:
           csr_write(LW_RISCV_CSR_MHPMCOUNTER(0), 0xFFFFFFFFFFFFFFFF);
           *((LwU16 *) ((LwU64) (&&loop9) + 4)) += 0x10;
           __asm__ volatile ("fence.i");

           if (exceptionExpectSet != LW_FALSE)
               HALT();

           // Can't write hpcounter
           exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_ILL);
loop10:
           csr_write(LW_RISCV_CSR_HPMCOUNTER(0), 0xFFFFFFFFFFFFFFFF);
           *((LwU16 *) ((LwU64) (&&loop10) + 4)) += 0x10;
           __asm__ volatile ("fence.i");

           if (exceptionExpectSet != LW_FALSE)
               HALT();
       }
   }
   sToM();

   printf("Finished csrTest\n");
}


