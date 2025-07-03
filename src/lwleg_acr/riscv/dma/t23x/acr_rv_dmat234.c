/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_rv_dmat234.c
 */
#include "acr_rv_dmat234.h"

#include <liblwriscv/dma.h>
#include <liblwriscv/io_pri.h>
#include <liblwriscv/shutdown.h>

#include "dev_pwr_csb.h"
#include "dev_pwr_pri.h"
#include "dev_fbif_v4.h"
#include "dev_falcon_v4.h"
#include "dev_graphics_nobundle.h"

/*! LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT Macro defining 
* the WPR and VPR RegionCfg addr alignment. 
*/
#define LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT 0x0000000lw

/*!
 * @brief Setup DMACTL register to write REQUIRE_CTX field.
 * 
 * @param[in] pFlcnCfg       Falcon Configuration
 * @param[in] bIsCtxRequired Boolean to specify if CTX is REQUIRED. 
 *
 */
void
acrlibSetupDmaCtl_T234
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwBool bIsCtxRequired
)
{
    if (bIsCtxRequired)
    {
        acrlibFlcnRegLabelWrite_HAL(pFlcnCfg, 
          REG_LABEL_FLCN_DMACTL, 0x1);
    }
    else
    {
        acrlibFlcnRegLabelWrite_HAL(pFlcnCfg, 
          REG_LABEL_FLCN_DMACTL, 0x0);
    }
}
 
/*!
 * @brief Program DMA base register.
 *        Lower 32 bits of fbBase is obtained using safety compliant funcition lwU64Lo32 and assigned to fbBaseLo32.
 *        This is then used in to program the DMA base register.
 *
 * @param[in] pFlcnCfg   Structure to hold falcon config
 * @param[in] fbBase     Base address of fb region to be copied
 */
void
acrlibProgramDmaBase_T234
(
    PACR_FLCN_CONFIG    pFlcnCfg,
    LwU64               fbBase
)
{
    LwU32 fbBaseLo32 = lwU64Lo32(fbBase);
    if (fbBaseLo32 == 0xDEADBEEFU)
    {
        acrWriteStatusToFalconMailbox(ACR_ERROR_VARIABLE_SIZE_OVERFLOW);
        riscvShutdown();
    }
    acrlibFlcnRegWrite_HAL(pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFBASE, fbBaseLo32);
}
 
/*!
 * @brief Programs the region CFG in FBIF_REGIONCFG register.
 *        Falcon need to select its region ID for a request.  There are 8 region IDs, called as T0region ~T7region, which could be selected at the same time with TRANSCFG 
 *        by a 3-bits CTXDMA in the request packet sent from falcon to FBIF. Region ID field has width of 4 bits, but not all bits are used in the final packet, it depends on the interface definition between FBIF & HUB/MMU.
 *        This is used for Write Protected Region. Both the region ID and falcon's security status will be forwarded to MMU in the request packet and MMU will check the rule, neither falcon nor FBIF needs to mask the region ID.
 *
 * @param[in] pFlcnCfg Falcon configuration.
 * @param[in] bUseCsb  Bool to specify if CSB interface needs to be used otherwise it will use BAR0_FBIF.
 * @param[in] ctxDma   TRANSCFG selection index.
 * @param[in] regionID region ID to be programmed.
 * 
 * @return ACR_OK if the region CFG is programmed successfully.
 */
ACR_STATUS
acrlibProgramRegionCfg_T234
(
    PACR_FLCN_CONFIG  pFlcnCfg,
    LwBool            bUseCsb,
    LwU32             ctxDma,
    LwU32             regionID
)
{
    LwU32 data   = 0;
    FLCN_REG_TGT tgt    = BAR0_FBIF;
    LwU32 addr   = LW_PFALCON_FBIF_REGIONCFG;
    LwU32 mask   = 0;
    LwU32 shift;

    CHECK_WRAP_AND_ERROR(ctxDma > LW_U32_MAX / 4U);
    shift = ctxDma * 4U;

    mask   = ~((LwU32)0xFU << shift); 

    if ((pFlcnCfg != LW_NULL) && (!(pFlcnCfg->bFbifPresent)))
    {
        data = priRead(pFlcnCfg->regCfgAddr);
        data = (data & mask) | ((regionID & 0xFU) << shift);
        priWrite(pFlcnCfg->regCfgAddr, data);
        data = priRead(pFlcnCfg->regCfgAddr);
    }
    else
    {
        if (bUseCsb)
        {
            tgt  = CSB_FLCN;
            addr = LW_CPWR_FBIF_REGIONCFG;
        }
 
        data = acrlibFlcnRegRead_HAL(pFlcnCfg, tgt, addr);
        data = (data & mask) | ((regionID & 0xFU) << shift);
        acrlibFlcnRegWrite_HAL(pFlcnCfg, tgt, addr, data);
    }

    return ACR_OK;
}

/*!
 * @brief Finds CTX dma with TRANCFG setting mapping to physical FB access. If not present, it will program offset 0 with physical FB access.
 *
 * @param[out] pCtxDma Pointer to hold the resulting ctxDma
 * @return ACR_OK if ctxdma is found
 */
static ACR_STATUS
acrFindCtxDma_T234
(
    LwU32        *pCtxDma
)
{
    /*
     * CTX DMA is programmed for PMU but not for SEC2
     */
    *pCtxDma = LSF_BOOTSTRAP_CTX_DMA_BOOTSTRAP_OWNER;
    return ACR_OK;
}

/*!
 * @brief  Populates the DMA properties.
 *         Do sanity checks on wprRegionID/wprOffset.
 *         Make sure ucodeBlobsize is not greater than wpr size.\n
 *         wprBase is populated using region properties.\n
 *         wprRegionID is populated using g_desc.\n
 *         g_scrubState.scrubTrack ACR scrub tracker is also initialized to 0x0.\n
 *         ctxDma is also populated by calling acrFindCtxDma_HAL.
 *
 * @param[in] wprRegIndex Index into pAcrRegions holding the wpr region.
 *
 * @return ACR_ERROR_VARIABLE_SIZE_OVERFLOW If there is a size overflow. 
 * @return ACR_ERROR_NO_WPR                 If wprRegionID is 0x0 or wprOffset is not as expected.
 * @return ACR_OK                           If DMA parameters are successfully populated.
 */
ACR_STATUS
acrPopulateDMAParameters_T234
(
    LwU32       wprRegIndex
)
{
    ACR_STATUS               status       = ACR_OK;
    RM_FLCN_ACR_REGION_PROP *pRegionProp;
    LwU32                    wprRegionSize = 0;
    /*
     * Do sanity check on region ID and wpr offset.
     * If wprRegionID is not set, return as it shows that RM
     * didnt setup the WPR region. 
     * TODO: Do more sanity checks including read/write masks.
     */
    if (g_desc.wprRegionID == 0U)
    {
        return ACR_ERROR_NO_WPR;
    } 

    if (g_desc.wprOffset != LSF_WPR_EXPECTED_OFFSET)
    {
        return ACR_ERROR_NO_WPR;
    }

    pRegionProp         = &(g_desc.regions.regionProps[wprRegIndex]);

    CHECK_WRAP_AND_ERROR(pRegionProp->startAddress > pRegionProp->endAddress);
    wprRegionSize = (LwU32)(pRegionProp->endAddress - pRegionProp->startAddress)
                        << LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT;

    if (g_desc.ucodeBlobSize > wprRegionSize)
    {
        return ACR_ERROR_NO_WPR;
    }
    /*
     * startAddress is 32 bit integer and wprOffset is 256B aligned.
     * Right shift wprOffset by 8 before adding to wprBase.
     */
    g_dmaProp.wprBase   = (((LwU64)pRegionProp->startAddress << 12)+
                                ((LwU64)g_desc.wprOffset));
    g_dmaProp.regionID  = g_desc.wprRegionID;

    /*
     * Find the right ctx dma for us to use.
     */
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFindCtxDma_T234(&(g_dmaProp.ctxDma)));

    return status;
}

/*!
 * @brief Program FBIF to allow physical transactions. Incase of GR falcons, make appropriate writes.
 *        Programs TRANSCFG_TARGET to specify where memory is located, if address mode is physical or not.\n
 *        Programs TRANSCFG_MEM_TYPE to indicate FBIF access address mode (virtual or physical).\n
 *        For FECS falcon, program FECS_ARB_CMD_OVERRIDE. This is for overriding memory access command.  Can be any fb_op_t type.  When the command is overriden, all access will be of that command type.\n
 *        Physical writes are not allowed, and will be blocked unless ARB_WPR_CMD_OVERRIDE_PHYSICAL_WRITES is set.\n
 *        Program OVERRIDE CMD to PHYS_SYS_MEM_NONCOHERENT for FECS for gv11b.\n
 *
 * @param[in] pFlcnCfg     Falcon configuration.
 * @param[in] ctxDma       CTXDMA configuration that needs to be setup.
 * @param[in] bIsPhysical  Boolean used to enable physical writes to FB/MEMORY for FECS.
 *
 * @return ACR_OK If CTX DMA setup is successful.
 * @return ACR_ERROR_VARIABLE_SIZE_OVERFLOW If there is a size overflow. 
 */
ACR_STATUS
acrlibSetupCtxDma_T234
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwU32            ctxDma,
    LwBool           bIsPhysical
)
{
    ACR_STATUS status = ACR_OK;
    LwU32      data   = 0;

    CHECK_WRAP_AND_ERROR(ctxDma > LW_U32_MAX / 4U);
    LwU32 regOff = ctxDma * 4U;

    if (pFlcnCfg->bFbifPresent)
    {
        data = acrlibFlcnRegRead_HAL(pFlcnCfg, BAR0_FBIF, regOff);
        data = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _TARGET, _LOCAL_FB, data);

        if (bIsPhysical)
        {
            data = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL,
                                                                        data);
        }
        else
        {
            data = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _MEM_TYPE, _VIRTUAL,
                                                                        data);
        }
        acrlibFlcnRegWrite_HAL(pFlcnCfg, BAR0_FBIF,
                                        LW_PFALCON_FBIF_TRANSCFG(ctxDma), data);
    }
    else
    {
        if (pFlcnCfg->falconId == LSF_FALCON_ID_FECS) 
        {
            if (bIsPhysical)
            {
                data = priRead(LW_PGRAPH_PRI_FECS_ARB_WPR);
                data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_WPR,
                                _CMD_OVERRIDE_PHYSICAL_WRITES, _ALLOWED, data);
                priWrite(LW_PGRAPH_PRI_FECS_ARB_WPR, data);
            }
 
            data = priRead(LW_PGRAPH_PRI_FECS_ARB_CMD_OVERRIDE);
            data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _CMD,
                                                _PHYS_SYS_MEM_COHERENT, data);
 
            if (bIsPhysical) {
                data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _ENABLE,
                                                                    _ON, data);
            } else {
                data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _ENABLE,
                                                                    _OFF, data);
            }
            priWrite(LW_PGRAPH_PRI_FECS_ARB_CMD_OVERRIDE, data);
        }
    }

    return status;
}
 
/*!
 * @brief Issue target falcon DMA. Supports only FB/SYMEM -> Flcn and not the other way.
 *        Support DMAing into both IMEM and DMEM of target falcon from FB.\n
 *        Always use physical addressing for memory transfer. Expect size to be 256 multiple.\n
 *        Make sure size to be DMAed is not greater than target falcon's DMEM/IMEM.\n
 *        Program TRANSCFG_TARGET to specify where memory is located, if address mode is physical or not.\n
 *        Program TRANSCFG_MEM_TYPE to indicate FBIF access address mode (virtual or physical).\n
 *        Disable CTX requirement for falcon DMA engine.\n
 *        Program REQUIRE_CTX fields of DMACTL to false.\n
 *        Program REGION configuration by writing to REGIONCFG register.\n
 *        Program DMA base register with DMA base address.\n
 *        Prepare DMA command e.g. DMA transfer size, CTXDMA and program it into LW_PPWR_FALCON_DMATRFCMD register.\n
 *        Program DMATRFMOFFS & DMATRFFBOFFS to specify IMEM/DMEM & FB offset.\n
 *        Execute transfer in loop till bytes transferred is not equal to sizeInBytes.
 *
 * @param[in] dstOff       Destination offset in either target falcon DMEM or IMEM
 * @param[in] fbBase       Base address of fb region to be copied
 * @param[in] fbOff        Offset from fbBase
 * @param[in] sizeInBytes  Number of bytes to be transferred
 * @param[in] regionID     ACR region ID to be used for this transfer
 * @param[in] bIsSync      Is synchronous transfer?
 * @param[in] bIsDstImem   TRUE if destination is IMEM
 * @param[in] pFlcnCfg     Falcon config
 *
 * @return ACR_OK If DMA is successful.
 * @return ACR_ERROR_TGT_DMA_FAILURE if the size to be transferred and src/dest are not aligned with DMA BLK size.
 * @return ACR_ERROR_UNEXPECTED_ARGS if the size to be transferred is more than target falcon's IMEM/DMEM.
 */
ACR_STATUS
acrlibIssueTargetFalconDma_T234
(
    LwU32            dstOff, 
    LwU64            fbBase, 
    LwU32            fbOff, 
    LwU32            sizeInBytes,
    LwU32            regionID,
    LwBool           bIsSync,
    LwBool           bIsDstImem,
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    ACR_STATUS     status        = ACR_OK;
    LwU32          data          = 0;
    LwU32          dmaCmd        = 0;
    LwU32          bytesXfered   = 0;
    LwS32          timeoutLeftNs = 0;
    ACR_TIMESTAMP  startTimeNs;
    LwBool         bIsSizeOk     = LW_FALSE; 

    //
    // Sanity checks
    // Only does 256B DMAs
    //
    if ((!VAL_IS_ALIGNED(sizeInBytes, FLCN_IMEM_BLK_SIZE_IN_BYTES)) ||
        (!VAL_IS_ALIGNED(dstOff, FLCN_IMEM_BLK_SIZE_IN_BYTES))      || 
        (!VAL_IS_ALIGNED(fbOff, FLCN_IMEM_BLK_SIZE_IN_BYTES)))
    {
        return ACR_ERROR_TGT_DMA_FAILURE;
    }

    bIsSizeOk = acrlibCheckIfUcodeFitsFalcon_HAL(pFlcnCfg, sizeInBytes, bIsDstImem);

    if (bIsSizeOk == LW_FALSE)
    {
        return ACR_ERROR_UNEXPECTED_ARGS;
    }
    //
    // Program Transcfg to point to physical mode
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibSetupCtxDma_HAL(pFlcnCfg,
                                                pFlcnCfg->ctxDma, LW_TRUE));

    if (pFlcnCfg->bFbifPresent)
    {
        //
        // Disable CTX requirement for falcon DMA engine
        //
        data = acrlibFlcnRegRead_HAL(pFlcnCfg, BAR0_FBIF, LW_PFALCON_FBIF_CTL);
        data = FLD_SET_DRF(_PFALCON, _FBIF_CTL, _ALLOW_PHYS_NO_CTX, _ALLOW, data);
        acrlibFlcnRegWrite_HAL(pFlcnCfg, BAR0_FBIF, LW_PFALCON_FBIF_CTL, data);
    }

    // Set REQUIRE_CTX to false
    acrlibSetupDmaCtl_HAL(pFlcnCfg, LW_FALSE);

    //Program REGCONFIG
    //always returns ACR_OK, so no need to check
    (void)acrlibProgramRegionCfg_HAL(pFlcnCfg, LW_FALSE, pFlcnCfg->ctxDma,
                                                                    regionID);

    //
    // Program DMA registers
    // Write DMA base address
    //
    acrlibProgramDmaBase_HAL(pFlcnCfg, fbBase);

    // prepare DMA command
    {
        dmaCmd = 0;
        if (bIsDstImem)
        {
            dmaCmd = FLD_SET_DRF(_PFALCON, _FALCON_DMATRFCMD, _IMEM, _TRUE,
                                                                    dmaCmd);
        }
        else
        {
            dmaCmd = FLD_SET_DRF(_PFALCON, _FALCON_DMATRFCMD, _IMEM, _FALSE,
                                                                    dmaCmd);
        }

        // Allow only FB->Flcn
        dmaCmd = FLD_SET_DRF(_PFALCON, _FALCON_DMATRFCMD, _WRITE, _FALSE,
                                                                    dmaCmd);

        // Allow only 256B transfers
        dmaCmd = FLD_SET_DRF(_PFALCON, _FALCON_DMATRFCMD, _SIZE, _256B,
                                                                    dmaCmd);

        dmaCmd = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFCMD, _CTXDMA,
                                                    pFlcnCfg->ctxDma, dmaCmd);
    }

    while (bytesXfered < sizeInBytes)
    {
        data = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFMOFFS, _OFFS,
                                                (dstOff + bytesXfered), 0);
        acrlibFlcnRegWrite_HAL(pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFMOFFS,
                                                                    data);

        data = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFFBOFFS, _OFFS,
                                                (fbOff + bytesXfered), 0);
        acrlibFlcnRegWrite_HAL(pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFFBOFFS,
                                                                    data);

        // Write the command
        acrlibFlcnRegWrite_HAL(pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFCMD,
                                                                    dmaCmd);

        //
        // Poll for completion
        // TODO: Make use of bIsSync
        //
        acrlibGetLwrrentTimeNs_HAL(&startTimeNs);
        data = acrlibFlcnRegRead_HAL(pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFCMD);
        while(FLD_TEST_DRF(_PFALCON_FALCON, _DMATRFCMD, _IDLE, _FALSE, data)) 
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCheckTimeout_HAL(ACR_DEFAULT_TIMEOUT_NS, 
                                                            startTimeNs,
                                                            &timeoutLeftNs));
            data = acrlibFlcnRegRead_HAL(pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFCMD);
        }

        bytesXfered += FLCN_IMEM_BLK_SIZE_IN_BYTES;
    }

    return status;
}
