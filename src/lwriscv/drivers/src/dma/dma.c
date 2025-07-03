/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    dma.c
 * @brief   DMA driver
 *
 * Handles dma part of engine.
 * Used to load data from FB to IMEM/DMEM.
 */

/* ------------------------ LW Includes ------------------------------------ */
#include <flcnifcmn.h>
#include <lwtypes.h>
#include <lwuproc.h>
#include <osptimer.h>

// Safertos headers
#include <lwrtos.h>
#include <sections.h>
#include <lwriscv/print.h>

// Chip info
#include <engine.h>
#include "memops.h"

/* ------------------------ RISC-V system library  -------------------------- */
#include "config/g_sections_riscv.h"

/* ------------------------ Module Includes -------------------------------- */
#include "drivers/cache.h"
#include "drivers/drivers.h"
#include "drivers/hardware.h"
#include "drivers/mpu.h"
#include "drivers/odp.h"
#include "sbilib/sbilib.h"

//------------------------------------------------------------------------------
// External symbols
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Variables
//------------------------------------------------------------------------------

// MMINTS-TODO: source these two values from headers!
#define ENGINE_DMA_BLOCK_SIZE_MIN   0x4
#define ENGINE_DMA_BLOCK_SIZE_MAX   0x100

#define ENGINE_ITCM_START           (ItcmRegionStart)
#define ENGINE_DTCM_START           (DtcmRegionStart)
#define ENGINE_ITCM_SIZE            (ItcmRegionSize)
#define ENGINE_DTCM_SIZE            (DtcmRegionSize)

LwU32 dmaIndex = 0;

static sysKERNEL_DATA GCC_ATTRIB_ALIGN(ENGINE_DMA_BLOCK_SIZE_MAX) LwU8 dmaBuffer[ENGINE_DMA_BLOCK_SIZE_MAX];
static LwUPtr dmaBufOffset = 0;

void dmaWaitForIdle(void)
{
    if (!OS_PTIMER_SPIN_WAIT_US_COND(csbPollFunc,
            (void*)&((LW_RISCV_REG_POLL){
                .addr      = ENGINE_REG(_FALCON_DMATRFCMD),
                .mask      = DRF_SHIFTMASK(LW_PRGNLCL_FALCON_DMATRFCMD_IDLE),
                .val       = DRF_DEF(_PRGNLCL, _FALCON_DMATRFCMD, _IDLE, _TRUE),
                .pollForEq = LW_TRUE,
            }),
            LW_DMA_TIMEOUT_US))
    {
        dbgPrintf(LEVEL_CRIT, "Timed out waiting on DMA completion!\n");
        kernelVerboseCrash(0);
    }
}

void dmaWaitForNotFull(void)
{
    if (!OS_PTIMER_SPIN_WAIT_US_COND(csbPollFunc,
            (void*)&((LW_RISCV_REG_POLL){
                .addr      = ENGINE_REG(_FALCON_DMATRFCMD),
                .mask      = DRF_SHIFTMASK(LW_PRGNLCL_FALCON_DMATRFCMD_FULL),
                .val       = DRF_DEF(_PRGNLCL, _FALCON_DMATRFCMD, _FULL, _FALSE),
                .pollForEq = LW_TRUE,
            }),
            LW_DMA_TIMEOUT_US))
    {
        dbgPrintf(LEVEL_CRIT, "Timed out waiting on DMA to be non-full!\n");
        kernelVerboseCrash(0);
    }
}

#ifdef LW_PRGNLCL_TFBIF_TRANSCFG
/* This block of code is copied over from liblwriscv to allow us to configure the TFBIF on CHEETAH
 * It will no longer be needed when we move to liblwriscv */

#define NUM_TFBIF_APERTURES 8

/*!
 * Configuration struct for a TFBIF Aperture. This is the CheetAh equivilant of FBIF_APERTURE_CFG
 */
typedef struct {
    /*
     * The aperture to configure (0-7)
     */
    LwU8 apertureIdx;

    /*
     * Value to program into the LW_PRGNLCL_TFBIF_TRANSCFG_ATTx_SWID field
     */
    LwU8 swid;

    /*
     * Value to program into the REGIONCFG_Tx_VPR field corresponding to this aperture
     */
    LwBool vpr;

    /*
     * Aperture ID AKA GSC_ID
     * Value to program into the REGIONCFGx_Tx_APERT_ID field corresponding to this aperture
     */
    LwU8 apertureId;
} TFBIF_APERTURE_CFG;

//
// The register manuals lwrrently don't define these as indexed fields.
// Because of this, we need to define our own. We have asked the hardware
// Folks to provide indexed field definitions in the manuals, so we don't
// need to do this here.
//
#define LW_PRGNLCL_TFBIF_TRANSCFG_ATT_SWID(i)     ((i)*4+1):((i)*4)
#define LW_PRGNLCL_TFBIF_REGIONCFG_T_VPR(i)       ((i)*4+3):((i)*4+3)
#define LW_PRGNLCL_TFBIF_REGIONCFG1_T_APERT_ID(i) ((i)*8+4):((i)*8)
#define LW_PRGNLCL_TFBIF_REGIONCFG2_T_APERT_ID(i) (((i)-4)*8+4):(((i)-4)*8)

FLCN_STATUS
tfbifConfigureAperture
(
    const TFBIF_APERTURE_CFG *cfgs,
    LwU8 numCfgs
)
{
    LwU8 idx;
    LwU32 transCfg, regionCfg, regionCfg1, regionCfg2;

    if (cfgs == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    transCfg = intioRead(LW_PRGNLCL_TFBIF_TRANSCFG);
    regionCfg = intioRead(LW_PRGNLCL_TFBIF_REGIONCFG);
    regionCfg1 = intioRead(LW_PRGNLCL_TFBIF_REGIONCFG1);
    regionCfg2 = intioRead(LW_PRGNLCL_TFBIF_REGIONCFG2);

    for (idx = 0; idx < numCfgs; idx++)
    {
        if (cfgs[idx].apertureIdx >= NUM_TFBIF_APERTURES ||
            cfgs[idx].swid >= (1 << DRF_SIZE(LW_PRGNLCL_TFBIF_TRANSCFG_ATT0_SWID)) ||
            cfgs[idx].apertureId >= (1 << DRF_SIZE(LW_PRGNLCL_TFBIF_REGIONCFG1_T0_APERT_ID))
        )
        {
            return FLCN_ERR_ILWALID_ARGUMENT;
        }

        transCfg = FLD_IDX_SET_DRF_NUM(_PRGNLCL, _TFBIF_TRANSCFG, _ATT_SWID, cfgs[idx].apertureIdx, cfgs[idx].swid, transCfg);
        regionCfg = FLD_IDX_SET_DRF_NUM(_PRGNLCL, _TFBIF_REGIONCFG, _T_VPR, cfgs[idx].apertureIdx, cfgs[idx].vpr, regionCfg);
        if (cfgs[idx].apertureIdx <= 3)
        {
            regionCfg1 = FLD_IDX_SET_DRF_NUM(_PRGNLCL, _TFBIF_REGIONCFG1, _T_APERT_ID, cfgs[idx].apertureIdx, cfgs[idx].apertureId, regionCfg1);
        }
        else
        {
            regionCfg2 = FLD_IDX_SET_DRF_NUM(_PRGNLCL, _TFBIF_REGIONCFG2, _T_APERT_ID, cfgs[idx].apertureIdx - 4, cfgs[idx].apertureId, regionCfg2);
        }
    }

    intioWrite(LW_PRGNLCL_TFBIF_TRANSCFG, transCfg);
    intioWrite(LW_PRGNLCL_TFBIF_REGIONCFG, regionCfg);
    intioWrite(LW_PRGNLCL_TFBIF_REGIONCFG1, regionCfg1);
    intioWrite(LW_PRGNLCL_TFBIF_REGIONCFG2, regionCfg2);

    return FLCN_OK;
}

sysKERNEL_CODE FLCN_STATUS
dmaConfigureFbif_NoContext(LwU8 dmaIdx, LwU8 swid, LwBool vpr, LwU8 apertureId)
{

    FLCN_STATUS ret;
    TFBIF_APERTURE_CFG cfgs []=
    {
        {
            .apertureIdx = dmaIdx,
            .swid = swid,
            .vpr = vpr,
            .apertureId = apertureId
        },
    };

    ret = tfbifConfigureAperture(cfgs, 1);
    if (ret != FLCN_OK)
    {
        dbgPrintf(LEVEL_ERROR, "%s: ERROR: tfbifConfigureAperture failed: %d\n", __FUNCTION__, ret);
    }
    return ret;
}
#else //LW_PRGNLCL_TFBIF_TRANSCFG
// Don't bother supporting FBIF for now. We will switch to liblwriscv before this is needed.
sysKERNEL_CODE FLCN_STATUS
dmaConfigureFbif_NoContext(LwU8 dmaIdx, LwU8 swid, LwBool vpr, LwU8 apertureId)
{
    return FLCN_ERROR;
}
#endif //LW_PRGNLCL_TFBIF_TRANSCFG

//------------------------------------------------------------------------------
// Code
//------------------------------------------------------------------------------

sysKERNEL_CODE FLCN_STATUS
dmaInit(LwU32 idx)
{
    LwU32 reg;

    dmaIndex = idx;

    reg = intioRead(LW_PRGNLCL_FALCON_DMACTL);
    reg = FLD_SET_DRF(_PRGNLCL, _FALCON_DMACTL, _REQUIRE_CTX, _FALSE, reg);
    intioWrite(LW_PRGNLCL_FALCON_DMACTL, reg);

#ifdef LW_PRGNLCL_FBIF_TRANSCFG
    // We can access only physical fb offsets (for now)
    reg = intioRead(LW_PRGNLCL_FBIF_TRANSCFG(dmaIndex));
    reg = FLD_SET_DRF(_PRGNLCL, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL, reg);
    riscvFbifTransCfgSet(dmaIndex, reg);
#endif // LW_PRGNLCL_TFBIF_TRANSCFG

    if (!mpuVirtToPhys(dmaBuffer, &dmaBufOffset, NULL))
    {
        dbgPrintf(LEVEL_ERROR, "%s: mpuVirtToPhys failed\n", __FUNCTION__);
        return FLCN_ERROR;
    }

    if ((dmaBufOffset < ENGINE_DTCM_START) || (dmaBufOffset >= ENGINE_DTCM_START + ENGINE_DTCM_SIZE))
    {
        dbgPrintf(LEVEL_ERROR, "%s: ERROR: dmaBufOffset not in DMEM\n",
                  __FUNCTION__);
        return FLCN_ERROR;
    }

    return FLCN_OK;
}

/*!
 * @see dmaMemTransfer
 *
 * A syscall handler entry-point for dmaMemTransfer
 * used in the optimized syscall code.
 *
 * This function should only be called from asm, and thus it has no
 * forward declarations in header files.
 *
 * Note that in this function, and everything called by it,
 * there's no access to the context in gp, since optimized syscalls
 * don't have a dedicated context!
 */
sysKERNEL_CODE FLCN_STATUS
dmaMemTransferSyscallHandler_NoContext
(
    void  *pBuf,
    LwU64  memDescAddress,
    LwU32  memDescParams,
    LwU32  offset,
    LwU32  sizeBytes,
    LwBool bRead,
    LwBool bMarkSelwre
)
{
    //
    // Syscalls shouldn't cause page faults. This wrapper doesn't
    // really do anything, however, because the only pointer arg here,
    // pBuf, has special handling in dmaMemTransfer - ODP API calls are
    // used to page it in by parts on-demand if needed.
    //
    return dmaMemTransfer(pBuf, memDescAddress, memDescParams, offset, sizeBytes, bRead, bMarkSelwre);
}

//
// NOTE: this function gets called from an optimized syscall,
// so you should NEVER try to access context in gp from it!
//
sysKERNEL_CODE FLCN_STATUS
dmaMemTransfer
(
    void  *pBuf,
    LwU64  memDescAddress,
    LwU32  memDescParams,
    LwU32  offset,
    LwU32  sizeBytes,
    LwBool bRead,
    LwBool bMarkSelwre
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8  *pMem = (LwU8*)pBuf;
    LwU64 apertureOffset;
    LwU32 dmaOffset;
    LwU64 memRiscvPa;
    LwU64 memRiscvPaSize;
    LwU64 tcmOffset;
    LwU32 dmaCmd = 0;
    LwU64 dmaBlockSize;
    LwU64 dmaEncBlockSize;
    LwBool bTranslateVAtoPA = LW_TRUE;
    LwU32 dmaMinBlockSize;
    LwU8 dmaIdx;

    if (pBuf == NULL)
    {
        dbgPrintf(LEVEL_ERROR, "%s: ERROR: NULL pBuf parameter\n", __FUNCTION__);
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    dmaIdx = DRF_VAL(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, memDescParams);

    apertureOffset = memDescAddress + offset;

    // Check minimum alignment
    if (((sizeBytes | ((LwU64)pMem) | apertureOffset) & (ENGINE_DMA_BLOCK_SIZE_MIN - 1)) != 0)
    {
        dbgPrintf(LEVEL_ERROR, "%s: Minimum DMA alignment & granularity is 0x%x.\n",
                  __FUNCTION__, ENGINE_DMA_BLOCK_SIZE_MIN);
        return FLCN_ERR_DMA_ALIGN;
    }

    // Nothing to copy, bail after checks
    if (sizeBytes == 0)
    {
        return FLCN_OK;
    }

    if (bMarkSelwre)
    {
        // Mark memory as secure - see falcon spec section 2.2
        dmaCmd = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _SEC, 1, dmaCmd);
    }

    if (bRead) // FB -> TCM
    {
        dmaCmd = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _WRITE, _FALSE, dmaCmd);
    }
    else // TCM -> FB
    {
        dmaCmd = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _WRITE, _TRUE, dmaCmd);
    }
    dmaCmd = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _CTXDMA, dmaIdx, dmaCmd);
    dmaCmd = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _SET_DMTAG, _FALSE, dmaCmd);

    {

#ifdef DMA_SUSPENSION
        if (fbhubGatedGet())
        {
            dbgPrintf(LEVEL_CRIT,
                      "%s: Fatal error: cannot DMA while in MSCG!\n",
                      __FUNCTION__);
            status = FLCN_ERR_DMA_SUSPENDED;
            goto dmaMemTransfer_exit;
        }
#endif // DMA_SUSPENSION

        dmaOffset = LwU64_LO32(apertureOffset) & (ENGINE_DMA_BLOCK_SIZE_MAX - 1);
        apertureOffset -= dmaOffset;

#if LWRISCV_MPU_FBHUB_ALLOWED == 1
        // Need to ilwalidate first, to avoid eviction causing writes (in case of a non-writethrough policy)
        dcacheIlwalidate();
        SYS_FLUSH_ALL();
#else  // LWRISCV_MPU_FBHUB_ALLOWED
        // Do a lightweight fence to avoid poking FBIF, since that could stall core if FBIF is down
        lwFenceAll();
#endif // LWRISCV_MPU_FBHUB_ALLOWED

        while (sizeBytes != 0)
        {
            // Only lookup PA if we are on a new page.
            if (bTranslateVAtoPA)
            {
#ifdef ODP_ENABLED
                // Poke the memory to load it into TCM if it's an ODP area
                odpLoadPage(pMem);
#endif // ODP_ENABLED

                // IMEM/DMEM Offset
                if (!mpuVirtToPhys(pMem, &memRiscvPa, &memRiscvPaSize))
                {
                    dbgPrintf(LEVEL_ERROR, "%s: mpuVirtToPhys failed (%p)\n",
                              __FUNCTION__, pMem);
                    status = FLCN_ERR_ILWALID_ARGUMENT;
                    goto dmaMemTransfer_exit;
                }

                if ((memRiscvPa >= ENGINE_ITCM_START) && (memRiscvPa < ENGINE_ITCM_START + ENGINE_ITCM_SIZE))
                {
                    if (!bRead)
                    {
                        dbgPrintf(LEVEL_ERROR,
                                  "%s: IMEM DMA read not allowed (%p)\n",
                                  __FUNCTION__, (void*)memRiscvPa);
                        status = FLCN_ERR_ILWALID_ARGUMENT;
                        goto dmaMemTransfer_exit;
                    }

                    tcmOffset = memRiscvPa - LW_RISCV_AMAP_IMEM_START;
                    dmaCmd = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _IMEM, _TRUE, dmaCmd);
                    dmaMinBlockSize = 256;
                }
                else if ((memRiscvPa >= ENGINE_DTCM_START) && (memRiscvPa < (ENGINE_DTCM_START + ENGINE_DTCM_SIZE)))
                {
                    tcmOffset = memRiscvPa - LW_RISCV_AMAP_DMEM_START;
                    dmaCmd = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _IMEM, _FALSE, dmaCmd);
                    dmaMinBlockSize = ENGINE_DMA_BLOCK_SIZE_MIN;
                }
                else
                {
                    dbgPrintf(LEVEL_ERROR,
                              "%s: TCM address not in allowed IMEM or DMEM ranges!\n",
                              __FUNCTION__);
                    status = FLCN_ERR_ILWALID_ARGUMENT;
                    goto dmaMemTransfer_exit;
                }

                bTranslateVAtoPA = LW_FALSE;
            }

            // Determine the largest pow2 block transfer size of the remaining data
            dmaBlockSize = memRiscvPa | dmaOffset | ENGINE_DMA_BLOCK_SIZE_MAX;
            dmaBlockSize = LOWESTBIT(dmaBlockSize);
            while (dmaBlockSize > sizeBytes)
            {
                dmaBlockSize >>= 1;
            }
            // Colwert the pow2 block size to block size encoding
            dmaEncBlockSize = dmaBlockSize / ENGINE_DMA_BLOCK_SIZE_MIN;
            dmaEncBlockSize = BIT_IDX_64(dmaEncBlockSize);

            dmaCmd = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _SIZE, dmaEncBlockSize, dmaCmd);

            if (dmaBlockSize < dmaMinBlockSize)
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                dbgPrintf(LEVEL_ERROR,
                          "%s: Illegal DMA transfer size (0x%llx < 0x%x)\n",
                          __FUNCTION__, dmaBlockSize, dmaMinBlockSize);
                goto dmaMemTransfer_exit;
            }

            // Wait if we're full
            dmaWaitForNotFull();

            intioWrite(LW_PRGNLCL_FALCON_DMATRFMOFFS, tcmOffset);

            /* DMATRFFBOFFS is also used to tag the DMEM */
            intioWrite(LW_PRGNLCL_FALCON_DMATRFFBOFFS, dmaOffset);

            intioWrite(LW_PRGNLCL_FALCON_DMATRFBASE, LwU64_LO32(apertureOffset / ENGINE_DMA_BLOCK_SIZE_MAX));
#ifdef LW_PRGNLCL_FALCON_DMATRFBASE1
            intioWrite(LW_PRGNLCL_FALCON_DMATRFBASE1, LwU64_HI32(apertureOffset / ENGINE_DMA_BLOCK_SIZE_MAX));
#else //LW_PRGNLCL_FALCON_DMATRFBASE1
            if (LwU64_HI32(apertureOffset / ENGINE_DMA_BLOCK_SIZE_MAX) != 0)
            {
                dbgPrintf(LEVEL_ERROR, "%s: apertureOffset %llx too large\n",
                          __FUNCTION__, apertureOffset);
                return FLCN_ERR_ILWALID_ARGUMENT;
            }
#endif //LW_PRGNLCL_FALCON_DMATRFBASE1

            intioWrite(LW_PRGNLCL_FALCON_DMATRFCMD, dmaCmd);

            // Adjust all TCM references -- only lookup new PA if we've reached
            // the end of this MPU mapping.
            memRiscvPa += dmaBlockSize;
            memRiscvPaSize -= dmaBlockSize;
            tcmOffset += dmaBlockSize;
            pMem += dmaBlockSize;
            bTranslateVAtoPA = (memRiscvPaSize == 0);

            dmaOffset += dmaBlockSize;
            sizeBytes -= dmaBlockSize;
        }

dmaMemTransfer_exit:
        // Wait for completion of any remaining transfers
        dmaWaitForIdle();

#if LWRISCV_MPU_FBHUB_ALLOWED == 1
        // Need to ilwalidate first, to avoid eviction causing writes (in case of a non-writethrough policy)
        dcacheIlwalidate();
        SYS_FLUSH_ALL();
#else  // LWRISCV_MPU_FBHUB_ALLOWED
        // Do a lightweight fence to avoid poking FBIF, since that could stall core if FBIF is down
        lwFenceAll();
#endif // LWRISCV_MPU_FBHUB_ALLOWED
    }

#ifdef DMA_NACK_SUPPORTED
    status = dmaNackCheckAndClear();
#endif // DMA_NACK_SUPPORTED

    return status;
}

#ifdef DMA_NACK_SUPPORTED
FLCN_STATUS
dmaNackCheckAndClear(void)
{
    LwU32 val = intioRead(LW_PRGNLCL_FALCON_DMAINFO_CTL);

    if (FLD_TEST_DRF(_PRGNLCL, _FALCON_DMAINFO_CTL, _INTR_ERR_COMPLETION,
                     _TRUE, val))
    {
        dbgPrintf(LEVEL_ALWAYS, "Received DMA NACK!\n");
        // Received DMA NACK, clear it and return error
        val = FLD_SET_DRF(_PRGNLCL, _FALCON_DMAINFO_CTL, _INTR_ERR_COMPLETION,
                          _CLR, val);
        intioWrite(LW_PRGNLCL_FALCON_DMAINFO_CTL, val);
        return FLCN_ERR_DMA_NACK;
    }
    else
    {
        // No DMA NACK received
        return FLCN_OK;
    }
}
#endif // DMA_NACK_SUPPORTED
