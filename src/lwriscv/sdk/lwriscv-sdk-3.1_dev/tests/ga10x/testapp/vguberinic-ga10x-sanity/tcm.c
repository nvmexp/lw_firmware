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

void tcmTest()
{
    LwU64 marchid;
    LwU32 falconHwcfg3;
    int i = 0;


#if defined(ENGINE_gsp) | defined(ENGINE_sec)
    LwU64 expectedImemSize = 88 * 1024;
    LwU64 expectedDmemSize = 64 * 1024;
    LwU64 expectedEmemSize = 8 * 1024;
    LwU64 expectedIcacheSize = LW_RISCV_CSR_MARCHID_ICACHE_SIZE_32KB;
    LwU64 expectedDcacheSize = LW_RISCV_CSR_MARCHID_DCACHE_SIZE_32KB;
#elif defined(ENGINE_pmu)
    LwU64 expectedImemSize = 144 * 1024;
    LwU64 expectedDmemSize = 176 * 1024;
    LwU64 expectedIcacheSize = LW_RISCV_CSR_MARCHID_ICACHE_SIZE_16KB;
    LwU64 expectedDcacheSize = LW_RISCV_CSR_MARCHID_DCACHE_SIZE_16KB;
#endif

    printf("Starting tcmTest\n");

    /*
     * Check if cache size advertised values match expected values
     */
    marchid = csr_read(LW_RISCV_CSR_MARCHID);
    if (DRF_VAL(_RISCV, _CSR_MARCHID, _ICACHE_SIZE, marchid) != expectedIcacheSize)
        HALT();
    if (DRF_VAL(_RISCV, _CSR_MARCHID, _DCACHE_SIZE, marchid) != expectedDcacheSize)
        HALT();

    /*
     * Check if tcm size advertised values match expected values
     */
#if defined(ENGINE_gsp) || defined(ENGINE_sec)
    falconHwcfg3 = bar0Read(ENGINE_REG(_FALCON_HWCFG3));
    if (DRF_VAL(_PFALCON, _FALCON_HWCFG3, _IMEM_TOTAL_SIZE, falconHwcfg3) != (expectedImemSize / 256))
        HALT();
    if (DRF_VAL(_PFALCON, _FALCON_HWCFG3, _DMEM_TOTAL_SIZE, falconHwcfg3) != (expectedDmemSize / 256))
        HALT();

    #if defined(LW_PGSP_FALCON_HWCFG)
    LwU32 hwcfg = bar0Read(ENGINE_REG(_HWCFG));
    if (DRF_VAL(_PGSP, _HWCFG, _EMEM_SIZE, hwcfg) != (expectedEmemSize / 256))
        HALT();
    #endif
#elif defined(ENGINE_pmu)
    falconHwcfg3 = bar0Read(ENGINE_REG(_FALCON_HWCFG3));
    if (DRF_VAL(_PPWR, _FALCON_HWCFG3, _IMEM_TOTAL_SIZE, falconHwcfg3) != (expectedImemSize / 256))
        HALT();
    if (DRF_VAL(_PPWR, _FALCON_HWCFG3, _DMEM_TOTAL_SIZE, falconHwcfg3) != (expectedDmemSize / 256))
        HALT();

    #if defined LW_PPWR_HWCFG
    LwU32 hwcfg = bar0Read(ENGINE_REG(_HWCFG));
    if (DRF_VAL(_PPWR, _HWCFG, _EMEM_SIZE, hwcfg) != (expectedEmemSize / 256))
        HALT();
    #endif
#endif

    /*
     * Positive tests - can we access last word of memory
     */

    {
        const LwU64 addresses_ok[] = {
            LW_RISCV_AMAP_IMEM_START + 0x5000,
            LW_RISCV_AMAP_DMEM_START + 0x5000,
    #if defined(ENGINE_gsp) | defined(ENGINE_sec)
            LW_RISCV_AMAP_EMEM_START + 0x1000,
            LW_RISCV_AMAP_EMEM_START,
            LW_RISCV_AMAP_EMEM_START + expectedEmemSize - 4,
    #endif
            //  range checks
            LW_RISCV_AMAP_IMEM_START,
            LW_RISCV_AMAP_IMEM_START + expectedImemSize - 4,
            LW_RISCV_AMAP_DMEM_START,
            LW_RISCV_AMAP_DMEM_START + expectedDmemSize - 4,
            0, };

        const LwU64 addresses_bad[] = {
            0x4,
            LW_RISCV_AMAP_IMEM_START - 4,
            LW_RISCV_AMAP_IMEM_END - 4, // because we have less than 512k IMEM, this also test (lack of) gap imem-dmem
            LW_RISCV_AMAP_DMEM_END - 4, // because we have less than 512k DMEM
    #if defined(ENGINE_pmu)
            LW_RISCV_AMAP_EMEM_START,
            LW_RISCV_AMAP_EMEM_START + 0x1000,
    #endif
            LW_RISCV_AMAP_EMEM_START - 4,
            LW_RISCV_AMAP_EMEM_END - 4, // because we have less than 512k EMEM
            // range checks
            LW_RISCV_AMAP_IMEM_START + expectedImemSize,
            LW_RISCV_AMAP_DMEM_START + expectedDmemSize,
    #if defined(ENGINE_gsp) | defined(ENGINE_sec)
            LW_RISCV_AMAP_EMEM_START + expectedEmemSize,
    #endif
            0, };
        LwBool bSuccess;

    /*
     * Positive tests - we don't fault
     */

        printf("Testing valid addresses...\n");
        while (addresses_ok[i])
        {
            LwU32 val = 0;

            printf("0x%llx: L ", addresses_ok[i]);
            bSuccess = tryLoad(addresses_ok[i], &val);
            if (!bSuccess)
            {
                printf("LOAD FAILED (exception) ");
                HALT();
            }
            printf("S ");
            bSuccess = tryStore(addresses_ok[i], val);
            if (!bSuccess)
            {
                printf("STORE FAILED (exception) ");
                HALT();
            }
            printf("\n");
            i++;
        }

    /*
     * Negative tests - do we fault
     */

        printf("Testing invalid addresses...\n");
        i = 0;
        while (addresses_bad[i])
        {
            LwU32 val = 0;

            printf("0x%llx: L ", addresses_bad[i]);
            bSuccess = tryLoad(addresses_bad[i], &val);
            if (bSuccess)
            {
                printf("LOAD FAILED (missing exception) ");
                HALT();
            }
            printf("S ");
            bSuccess = tryStore(addresses_bad[i], val);
            if (bSuccess)
            {
                printf("STORE FAILED (missing exception) ");
                HALT();
            }
            printf("\n");
            i++;
        }
    }

    printf("Finished tcmTest\n");
}
