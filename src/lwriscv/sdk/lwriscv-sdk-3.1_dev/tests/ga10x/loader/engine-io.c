/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <dev_falcon_v4.h>
#include <dev_riscv_pri.h>
#include <dev_sec_pri.h>
#include <dev_pwr_pri.h>
#include <dev_se_pri.h>
#include <dev_gsp.h>
#include <dev_top.h>
#include <dev_bus.h>
#include <lwmisc.h>
#include <dev_master.h>

#include "engine-io.h"
#include "misc.h"

#define LW_FALCON_MEMD_OFFSET           0x4
#define LW_FALCON_MEMT_OFFSET           0x8
#define FALCON_PAGE_SIZE                (1 << DRF_BASE(LW_PFALCON_FALCON_IMEMC_BLK))
#define FALCON_PAGE_MASK                (FALCON_PAGE_SIZE - 1)

EngineInfo riscv_instances[] =
{
    {"pmu", LW_FALCON_PWR_BASE, LW_FALCON2_PWR_BASE, DRF_BASE(LW_PMC_ENABLE_PWR), LW_PPWR_FBIF_CTL, LW_PPWR_FBIF_CTL2 },
    {"gsp", LW_FALCON_GSP_BASE, LW_FALCON2_GSP_BASE, -1, LW_PGSP_FBIF_CTL, LW_PGSP_FBIF_CTL2},
    {"sec", DRF_BASE(LW_PSEC), LW_FALCON2_SEC_BASE, DRF_BASE(LW_PMC_ENABLE_SEC), LW_PSEC_FBIF_CTL, LW_PSEC_FBIF_CTL2},
    {"sec2",DRF_BASE(LW_PSEC), LW_FALCON2_SEC_BASE, DRF_BASE(LW_PMC_ENABLE_SEC), LW_PSEC_FBIF_CTL, LW_PSEC_FBIF_CTL2},
    {NULL, 0} // terminator
};

// Not in falcon manuals :/
#define ENGINE_RESET_OFFSET         0x3c0
#define ENGINE_RESET_PLM_OFFSET     0x3c4

static void engineProbe(Engine *pEngine)
{
    LwU32 reg;

    if (!pEngine)
        return;

    reg = engineRead(pEngine, LW_PFALCON_FALCON_HWCFG3);
    pEngine->imemSize = DRF_VAL(_PFALCON_FALCON, _HWCFG3, _IMEM_TOTAL_SIZE, reg) * FALCON_PAGE_SIZE;
    pEngine->dmemSize = DRF_VAL(_PFALCON_FALCON, _HWCFG3, _DMEM_TOTAL_SIZE, reg) * FALCON_PAGE_SIZE;
    pEngine->bOldNetlist = LW_FALSE;
    reg = gpuRegRead(pEngine->pGpu, LW_PTOP_PLATFORM);
    if (FLD_TEST_DRF(_PTOP, _PLATFORM, _TYPE, _EMU, reg) ||
            FLD_TEST_DRF(_PTOP, _PLATFORM, _TYPE, _FPGA, reg)) // MK TODO -> is it true?
    {
        if  (gpuRegRead(pEngine->pGpu, LW_PBUS_EMULATION_REV0) <= 9)
        {
            pEngine->bOldNetlist = LW_TRUE;
        }
    }

    printf("Probed %s. IMEM: 0x%x DMEM: 0x%x%s\n", pEngine->pInfo->name,
           pEngine->imemSize,
           pEngine->dmemSize,
           pEngine->bOldNetlist ? " OLD NETLIST" : "");
}

Engine *engineGet(Gpu *pGpu, const char *name)
{
    Engine *pEngine;
    EngineInfo *pInfo = &riscv_instances[0];

    DBG_PRINT("Attempting to use the %s RISC-V.\n", name);

    while (pInfo->name)
    {
        if (strcmp(name, pInfo->name) == 0)
            break;
        pInfo++;
    }

    if (!pInfo->name)
        return NULL; // not found

    pEngine = calloc(1, sizeof(Engine));

    if (!pEngine)
        return pEngine;

    pEngine->pGpu = pGpu;
    pEngine->pInfo = pInfo;

    engineProbe(pEngine);

    return pEngine;
}

Engine *enginePut(Engine *pEngine)
{
    free(pEngine);
}

LwU32 engineRead(Engine *pEngine, LwU32 reg)
{
    return gpuRegRead(pEngine->pGpu, pEngine->pInfo->falconBase + reg);
}

void engineWrite(Engine *pEngine, LwU32 reg, LwU32 val)
{
    gpuRegWrite(pEngine->pGpu, pEngine->pInfo->falconBase + reg, val);
}

LwU32 riscvRead(Engine *pEngine, LwU32 reg)
{
    return gpuRegRead(pEngine->pGpu, pEngine->pInfo->riscvBase + reg);
}

void riscvWrite(Engine *pEngine, LwU32 reg, LwU32 val)
{
    gpuRegWrite(pEngine->pGpu, pEngine->pInfo->riscvBase + reg, val);
}


LwBool engineReset(Engine *pEngine)
{
    // Check if you can reset engine
    if ((engineRead(pEngine, ENGINE_RESET_PLM_OFFSET) & 0x11) != 0x11)
    {
        printf("Can't reset %s - PLM prohibits.\n", pEngine->pInfo->name);
        return LW_FALSE;
    }
    printf("Issuing reset to %s... ", pEngine->pInfo->name);
    engineWrite(pEngine, ENGINE_RESET_OFFSET, 1);
    usleep(10000);
    engineWrite(pEngine, ENGINE_RESET_OFFSET, 0);
    printf("Done.\n");
    printf("Waiting for scrubber: [");
    while(!FLD_TEST_DRF(_PFALCON_FALCON, _HWCFG2, _MEM_SCRUBBING, _DONE,
                       engineRead(pEngine, LW_PFALCON_FALCON_HWCFG2)))
    {
        printf(".");
        usleep(10000);
    }
    printf("] Done.\n");
    // MK: We can't test SE as we usually don't have access, so make it not fatal
    if ((gpuRegRead(pEngine->pGpu, LW_PSE_SE_CTRL_RESET__PRIV_LEVEL_MASK) & 0x11) != 0x11)
    {
        printf("Can't reset SE - PLM prohibits, but will try to continue.\n");
    } else
    {
        printf("Issuing reset to SE: [");
        gpuRegWrite(pEngine->pGpu, LW_PSE_SE_CTRL_RESET,
                    DRF_DEF(_PSE_SE, _CTRL_RESET, _SE_SOFT_RESET, _TRIGGER));
        while(!FLD_TEST_DRF(_PSE_SE, _CTRL_RESET, _SE_SOFT_RESET, _DONE,
                           gpuRegRead(pEngine->pGpu, LW_PSE_SE_CTRL_RESET)))
        {
            printf(".");
            usleep(100000);
        }
        printf("] Done.\n");
    }

    return LW_TRUE;
}

#define LW_PRISCV_RISCV_CORE_SWITCH_RISCV_STATUS                                                       0x00000240     /* R-I4R */
#define LW_PRISCV_RISCV_CORE_SWITCH_RISCV_STATUS_ACTIVE_STAT                                           0:0            /* R-IVF */
#define LW_PRISCV_RISCV_CORE_SWITCH_RISCV_STATUS_ACTIVE_STAT_INACTIVE                                  0x00000000     /* R-I-V */
#define LW_PRISCV_RISCV_CORE_SWITCH_RISCV_STATUS_ACTIVE_STAT_ACTIVE                                    0x00000001     /* R---V */
#define LW_PRISCV_RISCV_CORE_SWITCH_RISCV_STATUS_RUN_STAT                                              2:1            /* R-IVF */
#define LW_PRISCV_RISCV_CORE_SWITCH_RISCV_STATUS_RUN_STAT_HALT                                         0x00000000     /* R-I-V */
#define LW_PRISCV_RISCV_CORE_SWITCH_RISCV_STATUS_RUN_STAT_WAIT                                         0x00000001     /* R---V */
#define LW_PRISCV_RISCV_CORE_SWITCH_RISCV_STATUS_RUN_STAT_RUN                                          0x00000002     /* R---V */

LwBool engineIsActive(Engine *pEngine)
{
    LwU32 reg;
    /*
     * Older emulation netlists have Turing core switch registers
     */
    reg = gpuRegRead(pEngine->pGpu, LW_PTOP_PLATFORM);
    if (FLD_TEST_DRF(_PTOP, _PLATFORM, _TYPE, _EMU, reg) &&
        (gpuRegRead(pEngine->pGpu, LW_PBUS_EMULATION_REV0) <= 9))
    {

        reg = riscvRead(pEngine, LW_PRISCV_RISCV_CORE_SWITCH_RISCV_STATUS);
        return FLD_TEST_DRF(_PRISCV_RISCV, _CORE_SWITCH_RISCV_STATUS, _ACTIVE_STAT, _ACTIVE, reg);
    }
    else {
        reg = riscvRead(pEngine, LW_PRISCV_RISCV_CPUCTL);
        return FLD_TEST_DRF(_PRISCV_RISCV, _CPUCTL, _ACTIVE_STAT, _ACTIVE, reg);
    }
}

LwBool engineIsInIcd(Engine *pEngine)
{
    LwU32 reg;

    if (!engineIsActive(pEngine))
        return LW_FALSE;

    riscvWrite(pEngine, LW_PRISCV_RISCV_ICD_CMD,
               DRF_DEF(_PRISCV_RISCV, _ICD_CMD, _OPC, _RSTAT) |
               DRF_DEF(_PRISCV_RISCV, _ICD_CMD, _SZ, _W) |
               DRF_DEF(_PRISCV_RISCV, _ICD_CMD, _IDX, _RSTAT4)
               );
    while (1)
    {
        reg = riscvRead(pEngine, LW_PRISCV_RISCV_ICD_CMD);

        if (FLD_TEST_DRF(_PRISCV_RISCV, _ICD_CMD, _BUSY, _FALSE, reg))
        {
            break;
        }
    }
    if (FLD_TEST_DRF(_PRISCV_RISCV, _ICD_CMD, _ERROR, _TRUE, reg))
    {
        printf("Failed reading rstat.\n");
        return LW_FALSE;
    }

    return FLD_TEST_DRF(_PRISCV_RISCV,_ICD_RDATA0_RSTAT4,_ICD_STATE, _ICD,
    riscvRead(pEngine, LW_PRISCV_RISCV_ICD_RDATA0));
}

static LwBool _memReadWord(Engine *pEngine, LwU32 offset, LwU32 *pWord, const LwU32 memcBase)
{
    offset &= ~0x3;

    engineWrite(pEngine, memcBase, offset);
    *pWord = engineRead(pEngine, memcBase + LW_FALCON_MEMD_OFFSET);

    return LW_TRUE;
}

static LwBool _memWriteWord(Engine *pEngine, LwU32 offset, LwU32 word, const LwU32 memcBase)
{
    offset &= ~0x3;

    engineWrite(pEngine, memcBase, offset);
    engineWrite(pEngine, memcBase + LW_FALCON_MEMD_OFFSET, word);

    return LW_TRUE;
}

static void _memReadByte(Engine *pEngine, LwU32 offset, LwU8 *pBuffer, const LwU32 memcBase)
{
    LwU32 val;
    int addressShift = 8 * (offset & 0x3);

    _memReadWord(pEngine, offset & ~0x3, &val, memcBase);
    *pBuffer = (val >> addressShift) & 0xFF;
}

static void _memWriteByte(Engine *pEngine, LwU32 offset, LwU8 byte, const LwU32 memcBase)
{
    LwU32 val;
    int addressShift = 8 * (offset & 0x3);

    _memReadWord(pEngine, offset & ~0x3, &val, memcBase);

    val &= (~(0xFF << addressShift)); // mask out other data
    val |= (((LwU32)byte) << addressShift);

    _memWriteWord(pEngine, offset & ~0x3, val, memcBase);
}

static LwBool _readTcm(Engine *pEngine, LwU32 offset, unsigned len, void *pBuffer, const LwU32 memcBase)
{
    // noop
    if (len == 0)
        return LW_TRUE;

    //
    // Write *MEMT, that is required at least once after reboot, as
    // IMEMT is XXX (has no reset value) on all GPUs
    // DMEMT is 0 on reset, but that may change (default value is not in refman)
    //
    engineWrite(pEngine, memcBase + LW_FALCON_MEMT_OFFSET, 0);

    // Do expensive reads until we hit 4-byte alignment
    if (offset & 3)
        DBG_PRINT("Doing unaligned head reads (%u bytes).\n", offset & 3);

    while (offset & 0x3)
    {
        _memReadByte(pEngine, offset, (LwU8*)pBuffer, memcBase);
        offset++;
        pBuffer = (LwU8*)pBuffer + 1;

        if (--len == 0)
            break;
    }

    // Do fast read for 4-byte aligned reads
    if (len >= 4)
    {
        LwU32 data;

        engineWrite(pEngine, memcBase, DRF_DEF(_PFALCON_FALCON, _IMEMC, _AINCR, _TRUE) | offset);
        while (len >= 4)
        {
            //
            // We must copy with memcpy, as at this point pBuffer
            // may be unaligned.
            //
            data = engineRead(pEngine, memcBase + LW_FALCON_MEMD_OFFSET);
            memcpy(pBuffer, &data, sizeof(data));
            pBuffer = ((LwU8*)pBuffer) + 4;
            len -= 4;
            offset += 4;
        }
    }

    if (len)
        DBG_PRINT("Doing unaligned tail reads (%u bytes).\n", len);

    // Do expensive reads for the "tail"
    while (len)
    {
        _memReadByte(pEngine, offset, (LwU8*)pBuffer, memcBase);
        offset++;
        pBuffer = (LwU8*)pBuffer + 1;
        len --;
    }

    usleep(100000);

    return LW_TRUE;
}

static LwBool _writeTcm(Engine *pEngine, LwU32 offset, unsigned len,
                           const void *pBuffer, const LwU32 memcBase)
{
    // noop
    if (len == 0)
        return LW_TRUE;

    //
    // Write *MEMT, that is required at least once after reboot, as
    // IMEMT is XXX (has no reset value) on all GPUs
    // DMEMT is 0 on reset, but that may change (default value is not in refman)
    //
    engineWrite(pEngine, memcBase + LW_FALCON_MEMT_OFFSET, 0);

    // Do expensive writes until we hit 4-byte alignment
    if (offset & 3)
        DBG_PRINT("Doing unaligned head writes (%u bytes).\n", offset & 3);

    while (offset & 0x3)
    {
        _memWriteByte(pEngine, offset, *(const LwU8*)pBuffer, memcBase);
        offset++;
        pBuffer = (const LwU8*)pBuffer + 1;

        if (--len == 0)
            break;
    }

    // Do fast write for 4-byte aligned reads
    if (len >= 4)
    {
        LwU32 data;

        engineWrite(pEngine, memcBase, DRF_DEF(_PFALCON_FALCON, _IMEMC, _AINCW, _TRUE) | offset);
        while (len >= 4)
        {
            //
            // We must copy with memcpy, as at this point pBuffer
            // may be unaligned.
            //
            memcpy(&data, pBuffer, sizeof(data));
            engineWrite(pEngine, memcBase + LW_FALCON_MEMD_OFFSET, data);
            pBuffer = ((const LwU8*)pBuffer) + 4;
            len -= 4;
            offset += 4;
        }
    }

    if (len)
        DBG_PRINT("Doing unaligned tail writes (%u bytes).\n", len);

    // Do expensive writes for the "tail"
    while (len)
    {
        _memWriteByte(pEngine, offset, *(const LwU8*)pBuffer, memcBase);
        offset++;
        pBuffer = (const LwU8*)pBuffer + 1;
        len --;
    }

    /*
     * HW Trick:
     * IMEM/DMEM page is marked as pending until _last_ word is written.
     * It is enough to read/write last word, even if not full page was written.
     * Otherwise core will stall when trying to access that page.
     */
    if (offset & FALCON_PAGE_MASK)
    {
        LwU32 writeAddr = (offset & (~FALCON_PAGE_MASK)) | (FALCON_PAGE_SIZE - 0x4);
        LwU32 data;

        DBG_PRINT("Write didn't complete at end of page. Triggering noop write at 0x%x.\n", writeAddr);
        engineWrite(pEngine, memcBase, writeAddr);
        data = engineRead(pEngine, memcBase + LW_FALCON_MEMD_OFFSET);
        engineWrite(pEngine, memcBase + LW_FALCON_MEMD_OFFSET, data);
    }

    usleep(100000);

    return LW_TRUE;
}

LwBool engineReadImem(Engine *pEngine, LwU32 offset, unsigned len, void *pBuffer)
{
    if (!pEngine)
        return LW_FALSE;

    // Sanity check on offset and size
    if (len > pEngine->imemSize)
        return LW_FALSE;
    if (offset > pEngine->imemSize - len)
        return LW_FALSE;

    DBG_PRINT("Reading ucode from IMEM address %08x.\n", offset);
    return _readTcm(pEngine, offset, len, pBuffer, LW_PFALCON_FALCON_IMEMC(0));
}

LwBool engineReadDmem(Engine *pEngine, LwU32 offset, unsigned len, void *pBuffer)
{
    if (!pEngine)
        return LW_FALSE;

    // Sanity check on offset and size
    if (len > pEngine->dmemSize)
        return LW_FALSE;
    if (offset > pEngine->dmemSize - len)
        return LW_FALSE;

    DBG_PRINT("Reading data from DMEM address %08x.\n", offset);
    return _readTcm(pEngine, offset, len, pBuffer, LW_PFALCON_FALCON_DMEMC(0));
}

LwBool engineWriteImem(Engine *pEngine, LwU32 offset, unsigned len, void *pBuffer)
{
    LwBool ret;

    if (!pEngine)
        return LW_FALSE;

    // Sanity check on offset and size
    if (len > pEngine->imemSize)
        return LW_FALSE;
    if (offset > pEngine->imemSize - len)
        return LW_FALSE;

    DBG_PRINT("Writing ucode to IMEM address %08x.\n", offset);
    ret = _writeTcm(pEngine, offset, len, pBuffer, LW_PFALCON_FALCON_IMEMC(0));

    {
        LwU8 bufRead[len];
        LwU8 *bufOrg = pBuffer;
        int i;

        DBG_PRINT("Veryfying ...\n");
        ret = _readTcm(pEngine, offset, len, bufRead, LW_PFALCON_FALCON_IMEMC(0));

        for (i=0; i<len; i++)
        {
            if (bufRead[i] != bufOrg[i])
            {
                DBG_PRINT("Mismatch at %d/0x%x: %02x vs %02x\n", i, i,
                          bufRead[i], bufOrg[i]);
                return LW_FALSE;
            }
        }
    }
    return ret;
}

LwBool engineWriteDmem(Engine *pEngine, LwU32 offset, unsigned len, void *pBuffer)
{
    LwBool ret;

    if (!pEngine)
        return LW_FALSE;

    // Sanity check on offset and size
    if (len > pEngine->dmemSize)
        return LW_FALSE;
    if (offset > pEngine->dmemSize - len)
        return LW_FALSE;

    DBG_PRINT("Writing data to DMEM address %08x.\n", offset);    
    ret = _writeTcm(pEngine, offset, len, pBuffer, LW_PFALCON_FALCON_DMEMC(0));

    {
        LwU8 bufRead[len];
        LwU8 *bufOrg = pBuffer;
        int i;

        DBG_PRINT("Veryfying ...\n");
        ret = _readTcm(pEngine, offset, len, bufRead, LW_PFALCON_FALCON_DMEMC(0));

        for (i=0; i<len; i++)
        {
            if (bufRead[i] != bufOrg[i])
            {
                DBG_PRINT("Mismatch at %d/0x%x: %02x vs %02x\n", i, i,
                          bufRead[i], bufOrg[i]);
                return LW_FALSE;
            }
        }
    }
    return ret;
}

LwU32 engineReadImemWord(Engine *pEngine, LwU32 offset)
{
    LwU32 word;
    LwBool ret;

    if (offset  > (pEngine->imemSize - 4))
    {
        printf("Can't read beyond IMEM size.");
        return 0xBADCAFFE;
    }

    ret = _memReadWord(pEngine, offset, &word, LW_PFALCON_FALCON_IMEMC(0));

    if (!ret)
    {
        printf("Failed to read PMB.\n");
        return 0xBADCAFFE;
    }

    return word;
}

LwU32 engineReadDmemWord(Engine *pEngine, LwU32 offset)
{
    LwU32 word;
    LwBool ret;

    if (offset  > (pEngine->dmemSize - 4))
    {
        printf("Can't read beyond DMEM size.");
        return 0xBADCAFFE;
    }

    ret = _memReadWord(pEngine, offset, &word, LW_PFALCON_FALCON_DMEMC(0));

    if (!ret)
    {
        printf("Failed to read PMB.\n");
        return 0xBADCAFFE;
    }

    return word;
}

LwBool engineWriteImemWord(Engine *pEngine, LwU32 offset, LwU32 value)
{
    if (offset  > (pEngine->imemSize - 4))
    {
        printf("Can't write beyond TCM size.");
        return LW_FALSE;
    }

    return _memWriteWord(pEngine, offset, value, LW_PFALCON_FALCON_IMEMC(0));
}

LwBool engineWriteDmemWord(Engine *pEngine, LwU32 offset, LwU32 value)
{
    if (offset  > (pEngine->dmemSize - 4))
    {
        printf("Can't write beyond DMEM size.");
        return LW_FALSE;
    }

    return _memWriteWord(pEngine, offset, value, LW_PFALCON_FALCON_DMEMC(0));
}
