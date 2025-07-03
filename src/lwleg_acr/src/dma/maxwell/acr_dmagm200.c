/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_dmagm200.c
 */

//
// Includes
//
#include "acr.h"
#include "acr_objacrlib.h"
#include "g_acr_private.h"
#include "dev_pwr_csb.h"
#include "dev_fbif_v4.h"
#include "dev_falcon_v4.h"
#include "dev_graphics_nobundle.h"
#include "lwos_dma.h"

// DMA CTX update macros
#define  DMA_SET_IMREAD_CTX(ctxSpr, dmaIdx)  \
    (((ctxSpr) & ~((LwU32)0x7U)) | (dmaIdx))

#define  DMA_SET_DMREAD_CTX(ctxSpr, dmaIdx)  \
    (((ctxSpr) & ~((LwU32)0x7U << 8)) | ((dmaIdx) << 8))

#define  DMA_SET_DMWRITE_CTX(ctxSpr, dmaIdx) \
    (((ctxSpr) & ~((LwU32)0x7U << 12)) | ((dmaIdx) << 12))


/*!
 * @brief Setup DMACTL register to write REQUIRE_CTX field.
 *
 * @param[in] pFlcnCfg       Falcon Configuration
 * @param[in] bIsCtxRequired Boolean to specify if CTX is REQUIRED.
 *
 */
void
acrlibSetupDmaCtl_GM200
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwBool bIsCtxRequired
)
{
#ifndef ACR_SAFE_BUILD
    if (bIsCtxRequired)
    {
        acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg,
          REG_LABEL_FLCN_DMACTL, 0x1);
    }
    else
#endif
    {
        acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg,
          REG_LABEL_FLCN_DMACTL, 0x0);
    }
}

/*!
 * @brief Finds CTX dma with TRANCFG setting mapping to physical FB access. If not present, it will program offset 0 with physical FB access.
 *
 * @param[out] pCtxDma Pointer to hold the resulting ctxDma
 * @return ACR_OK if ctxdma is found
 */
ACR_STATUS
acrFindCtxDma_GM200
(
    LwU32        *pCtxDma
)
{
    //
    // CTX DMA is programmed for PMU but not for SEC2
    //
    *pCtxDma = LSF_BOOTSTRAP_CTX_DMA_BOOTSTRAP_OWNER;
    return ACR_OK;
}

#ifndef ACR_SAFE_BUILD
/*!
 * @brief Program FBIF to allow physical transactions. Incase of GR falcons, make appropriate writes.
 *        Programs TRANSCFG_TARGET to specify where memory is located, if address mode is physical or not.\n
 *        Programs TRANSCFG_MEM_TYPE to indicate FBIF access address mode (virtual or physical).\n
 *        For FECS falcon, program FECS_ARB_CMD_OVERRIDE. This is for overriding memory access command.  Can be any fb_op_t type.  When the command is overriden, all access will be of that command type.\n
 *        Physical writes are not allowed, and will be blocked unless ARB_WPR_CMD_OVERRIDE_PHYSICAL_WRITES is set.\n
 *        Program OVERRIDE CMD to PHYS_VID_MEM for FECS for PRE-VOLTA CHIPS.\n
 * @param[in] pFlcnCfg     Falcon configuration.
 * @param[in] ctxDma       CTXDMA configuration that needs to be setup.
 * @param[in] bIsPhysical  Boolean used to enable physical writes to FB/MEMORY for FECS.
 *
 * @return ACR_OK If CTX DMA setup is successful.
 * @return ACR_ERROR_VARIABLE_SIZE_OVERFLOW If there is a size overflow.
 */
ACR_STATUS
acrlibSetupCtxDma_GM200
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwU32            ctxDma,
    LwBool           bIsPhysical
)
{
    ACR_STATUS status = ACR_OK;
    LwU32      data   = 0;
    LwU32      regOff = ctxDma * 4U;

    CHECK_WRAP_AND_ERROR(ctxDma != regOff / 4U);

    if (pFlcnCfg->bFbifPresent)
    {
        data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FBIF, regOff);
        data = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _TARGET, _LOCAL_FB, data);

        if (bIsPhysical)
        {
            data = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL, data);
        }
        else
        {
            data = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _MEM_TYPE, _VIRTUAL, data);
        }
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FBIF, LW_PFALCON_FBIF_TRANSCFG(ctxDma), data);
    }
    else
    {
        if (pFlcnCfg->falconId == LSF_FALCON_ID_FECS)
        {
            if (bIsPhysical)
            {
                data = ACR_REG_RD32(BAR0, LW_PGRAPH_PRI_FECS_ARB_WPR);
                data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_WPR, _CMD_OVERRIDE_PHYSICAL_WRITES, _ALLOWED, data);
                ACR_REG_WR32(BAR0, LW_PGRAPH_PRI_FECS_ARB_WPR, data);
            }

            data = ACR_REG_RD32(BAR0, LW_PGRAPH_PRI_FECS_ARB_CMD_OVERRIDE);
            data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _CMD, _PHYS_VID_MEM, data);

            if (bIsPhysical) {
                data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _ENABLE, _ON, data);
            } else {
                data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _ENABLE, _OFF, data);
            }

            ACR_REG_WR32(BAR0, LW_PGRAPH_PRI_FECS_ARB_CMD_OVERRIDE, data);
        }
    }

    return status;
}
#endif

/*!
 * @brief  Populates the DMA properties.
 *         Do sanity checks on wprRegionID/wprOffset.
 *         wprBase is populated using region properties.\n
 *         wprRegionID is populated using g_desc.\n
 *         g_scrubState.scrubTrack ACR scrub tracker is also initialized to 0x0.\n
 *         ctxDma is also populated by calling acrFindCtxDma_HAL.
 *
 * @param[in] wprRegIndex Index into pAcrRegions holding the wpr region.
 *
 * @return ACR_ERROR_VARIABLE_SIZE_OVERFLOW If there is a size overflow.
 * @return ACR_ERROR_NO_WPR If wprRegionID is 0x0 or wprOffset is not as expected.
 * @return ACR_OK           If DMA parameters are successfully populated.
 */
ACR_STATUS
acrPopulateDMAParameters_GM200
(
    LwU32       wprRegIndex
)
{
    ACR_STATUS               status       = ACR_OK;
    RM_FLCN_ACR_REGION_PROP *pRegionProp;
    //
    // Do sanity check on region ID and wpr offset.
    // If wprRegionID is not set, return as it shows that RM
    // didnt setup the WPR region.
    // TODO: Do more sanity checks including read/write masks
    //
    if (g_desc.wprRegionID == 0U)
    {
        return ACR_ERROR_NO_WPR;
    }

    if (g_desc.wprOffset != LSF_WPR_EXPECTED_OFFSET)
    {
        return ACR_ERROR_NO_WPR;
    }

    pRegionProp         = &(g_desc.regions.regionProps[wprRegIndex]);

    // startAddress is 32 bit integer and wprOffset is 256B aligned. Right shift wprOffset by 8 before adding to wprBase
    g_dmaProp.wprBase   = (((LwU64)pRegionProp->startAddress << 4)+ ((LwU64)g_desc.wprOffset >> 8));
    g_dmaProp.regionID  = g_desc.wprRegionID;

    // Also initialize ACR scrub tracker to 0
    g_scrubState.scrubTrack = 0;

    // Find the right ctx dma for us to use
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFindCtxDma_HAL(pAcr, &(g_dmaProp.ctxDma)));
    return ACR_OK;
}

#ifndef UPROC_RISCV
/*!
  * @brief Use falc_dmread/dmwrite, falc_imread/imwrite to read/write from falcon's IMEM/DMEM based on bIsImem and dmaDirection. \n
  *        The functions are used in loop till bytes transferred is not equal to bytes requested for DMAing i.e. sizeInBytes. \n
  *        Use dmwait after submitting the DMA request(s) based on dmaSync flag.
  *
  *
  * @param[in]  memOff       Offset to be used either as source or destination
  * @param[in]  bIsImem      Bool to specify if Source/destination is IMEM.
  * @param[in]  fbOff        FB offset from DMB
  * @param[in]  sizeInBytes  Size of the content to be DMAed
  * @param[in]  dmaDirection Destination of DMA e.g. FB(SYSMEM)
  * @param[in]  dmaSync      Do dmwait after submitting the DMA request(s).
  *
  * @return Number of bytes transferred.
  */
static LwU32
acrInitDmaXfer
(
    LwU32             memOff,
    LwU32             fbOff,
    LwU32             sizeInBytes,
    LwBool            bIsImem,
    ACR_DMA_DIRECTION dmaDirection,
    ACR_DMA_SYNC_TYPE dmaSync

)
{
    LwU32             e;
    LwU32             xferSize;
    LwU32             memSrc;
    LwU32             bytesXfered = 0;

    e = DMA_XFER_ESIZE_MAX;
    xferSize = DMA_XFER_SIZE_BYTES(e);

    while (bytesXfered != sizeInBytes)
    {
        if (((sizeInBytes - bytesXfered) >= xferSize) &&
              VAL_IS_ALIGNED(memOff, xferSize) &&
              VAL_IS_ALIGNED(fbOff, xferSize))
        {
            if ((dmaDirection == ACR_DMA_TO_FB) || (dmaDirection == ACR_DMA_TO_FB_SCRUB))
            {
                if (dmaDirection != ACR_DMA_TO_FB_SCRUB) {
                    memSrc = (memOff + bytesXfered);
                } else {
                    memSrc = memOff;
                }

#ifndef ACR_SAFE_BUILD
                if (bIsImem)
                {
                    // Error, no imwrite
                    return bytesXfered;
                }
                else
#endif
                {
                    falc_dmwrite(
                        fbOff + bytesXfered,
                        memSrc | (e << 16));
                }
            }
            else
            {
#ifndef ACR_SAFE_BUILD
                if (bIsImem)
                {
                    falc_imread(
                        fbOff + bytesXfered,
                        (memOff + bytesXfered) | (e << 16));
                }
                else
#endif
                {
                    falc_dmread(
                        fbOff + bytesXfered,
                        (memOff + bytesXfered) | (e << 16));
                }
            }
            bytesXfered += xferSize;

            // Restore e
            e        = DMA_XFER_ESIZE_MAX;
            xferSize = DMA_XFER_SIZE_BYTES(e);
        }
        // try the next largest transfer size
        else
        {
            //
            // Ensure that we have not dropped below the minimum transfer size
            // supported by the HW. Such and event indicates that the requested
            // number of bytes cannot be written either due to mis-alignment of
            // the source and destination pointers or the requested write-size
            // not being a multiple of the minimum transfer size.
            //
            if (e > DMA_XFER_ESIZE_MIN)
            {
                e--;
                xferSize = DMA_XFER_SIZE_BYTES(e);
            }
            else
            {
                break;
            }
        }
    }

    if (dmaSync == ACR_DMA_SYNC_AT_END)
    {
#ifndef ACR_SAFE_BUILD
        if (bIsImem) {
            falc_imwait();
        } else
#endif
        {
            falc_dmwait();
        }
    }

    return bytesXfered;
}


/*!
 * @brief ACR routine to do falcon FB/SYSMEM DMA to and from IMEM/DMEM.\n
 *        Program region configuration.\n
 *        Program DMA base using pDmaProp and bIsImem.\n
 *        Program CTX depending on MEM type(IMEM/DMEM) and direction of transfer.\n
 *        Call acrInitDmaXfer function to transfer the requested bytes of data.
 *
 * @param[in]  memOff       Offset to be used either as source or destination
 * @param[in]  bIsImem      Bool to specify if Source/destination is IMEM.
 * @param[in]  fbOff        FB offset from DMB
 * @param[in]  sizeInBytes  Size of the content to be DMAed
 * @param[in]  dmaDirection Destination of DMA e.g. FB(SYSMEM)
 * @param[in]  dmaSync      Do dmwait after submitting the DMA request(s)
 * @param[in]  pDmaProp     DMA properties to be used for this DMA operation
 *
 * @return Number of bytes transferred. In case of any error/failure 0x0 is returned.
 */
LwU32
acrIssueDma_GM200
(
    LwU32              memOff,
    LwBool             bIsImem,
    LwU32              fbOff,
    LwU32              sizeInBytes,
    ACR_DMA_DIRECTION  dmaDirection,
    ACR_DMA_SYNC_TYPE  dmaSync,
    PACR_DMA_PROP      pDmaProp
)
{
    LwU32           bytesXfered = 0;
    LwU32           ctx;
    ACR_FLCN_CONFIG pFlcnCfg;
    LwBool          bIsSizeOk   = LW_FALSE;

    if (acrlibGetFalconConfig_HAL(pAcrlib, ACR_LSF_LWRRENT_BOOTSTRAP_OWNER, LSF_FALCON_INSTANCE_DEFAULT_0, &pFlcnCfg) != ACR_OK)
    {
        return 0x0;
    }

    if (dmaDirection != ACR_DMA_TO_FB_SCRUB)
    {
        bIsSizeOk = acrlibCheckIfUcodeFitsFalcon_HAL(pAcrlib, &pFlcnCfg, sizeInBytes, bIsImem);

        if (bIsSizeOk == LW_FALSE)
        {
            return 0x0;
        }
    }

    // Set regionCFG
    //always returns ACR_OK, so no need to check
    (void)acrlibProgramRegionCfg_HAL(pAcrlib, LW_NULL, LW_TRUE, pDmaProp->ctxDma, pDmaProp->regionID);

    // Set new DMA and CTX
    acrProgramDmaBase_HAL(pAcrlib, bIsImem, pDmaProp);

    // Update the CTX, this depends on mem type and dir
    falc_rspr(ctx, CTX);
#ifndef ACR_SAFE_BUILD
    if (bIsImem)
    {
        falc_wspr(CTX, DMA_SET_IMREAD_CTX(ctx, pDmaProp->ctxDma));
    }
    else
#endif
    {
        falc_wspr(CTX, (dmaDirection == ACR_DMA_FROM_FB) ?
                  DMA_SET_DMREAD_CTX(ctx, pDmaProp->ctxDma) :
                  DMA_SET_DMWRITE_CTX(ctx, pDmaProp->ctxDma));
    }

    bytesXfered = acrInitDmaXfer(memOff, fbOff, sizeInBytes, bIsImem, dmaDirection, dmaSync);

    return bytesXfered;
}

#endif // UPROC_RISCV

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
acrlibIssueTargetFalconDma_GM200
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

    bIsSizeOk = acrlibCheckIfUcodeFitsFalcon_HAL(pAcrlib, pFlcnCfg, sizeInBytes, bIsDstImem);

    if (bIsSizeOk == LW_FALSE)
    {
        return ACR_ERROR_UNEXPECTED_ARGS;
    }
    //
    // Program Transcfg to point to physical mode
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibSetupCtxDma_HAL(pAcrlib, pFlcnCfg, pFlcnCfg->ctxDma, LW_TRUE));

#ifndef ACR_SAFE_BUILD
    if (pFlcnCfg->bFbifPresent)
    {
        //
        // Disable CTX requirement for falcon DMA engine
        //
        data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FBIF, LW_PFALCON_FBIF_CTL);
        data = FLD_SET_DRF(_PFALCON, _FBIF_CTL, _ALLOW_PHYS_NO_CTX, _ALLOW, data);
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FBIF, LW_PFALCON_FBIF_CTL, data);
    }
#endif

    // Set REQUIRE_CTX to false
    acrlibSetupDmaCtl_HAL(pAcrlib, pFlcnCfg, LW_FALSE);

    //Program REGCONFIG
    //always returns ACR_OK, so no need to check
    (void)acrlibProgramRegionCfg_HAL(pAcrlib, pFlcnCfg, LW_FALSE, pFlcnCfg->ctxDma, regionID);

    //
    // Program DMA registers
    // Write DMA base address
    //
    acrlibProgramDmaBase_HAL(pAcrlib, pFlcnCfg, fbBase);

    // prepare DMA command
    {
        dmaCmd = 0;
        if (bIsDstImem)
        {
            dmaCmd = FLD_SET_DRF(_PFALCON, _FALCON_DMATRFCMD, _IMEM, _TRUE, dmaCmd);
        }
        else
        {
            dmaCmd = FLD_SET_DRF(_PFALCON, _FALCON_DMATRFCMD, _IMEM, _FALSE, dmaCmd);
        }

        // Allow only FB->Flcn
        dmaCmd = FLD_SET_DRF(_PFALCON, _FALCON_DMATRFCMD, _WRITE, _FALSE, dmaCmd);

        // Allow only 256B transfers
        dmaCmd = FLD_SET_DRF(_PFALCON, _FALCON_DMATRFCMD, _SIZE, _256B, dmaCmd);

        dmaCmd = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFCMD, _CTXDMA, pFlcnCfg->ctxDma, dmaCmd);
    }

    while (bytesXfered < sizeInBytes)
    {
        data = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFMOFFS, _OFFS, (dstOff + bytesXfered), 0);
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFMOFFS, data);

        data = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFFBOFFS, _OFFS, (fbOff + bytesXfered), 0);
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFFBOFFS, data);

        // Write the command
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFCMD, dmaCmd);

        //
        // Poll for completion
        // TODO: Make use of bIsSync
        //
        acrlibGetLwrrentTimeNs_HAL(pAcrlib, &startTimeNs);
        data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFCMD);
        while(FLD_TEST_DRF(_PFALCON_FALCON, _DMATRFCMD, _IDLE, _FALSE, data))
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCheckTimeout_HAL(pAcrlib, ACR_DEFAULT_TIMEOUT_NS,
                                                            startTimeNs, &timeoutLeftNs));
            data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFCMD);
        }

        bytesXfered += FLCN_IMEM_BLK_SIZE_IN_BYTES;
    }

    return status;
}

#ifndef UPROC_RISCV
/*!
 * @brief ACR routine to program DMA base
 *
 * @param[in]  bIsImem     Is Source/destination IMEM?
 * @param[in]  pDmaProp    DMA properties to be used for this DMA operation
 */
void
acrProgramDmaBase_GM200
(
    LwBool             bIsImem,
    PACR_DMA_PROP      pDmaProp
)
{
    // Set new DMA
#ifndef ACR_SAFE_BUILD
    if (bIsImem)
    {
        falc_wspr(IMB, pDmaProp->wprBase);
    }
    else
#endif
    {
        falc_wspr(DMB, pDmaProp->wprBase);
    }
}
#endif // UPROC_RISCV

/*!
 * @brief Program DMA base register.
 *        Lower 32 bits of fbBase is obtained using safety compliant funcition lwU64Lo32 and assigned to fbBaseLo32.
 *        This is then used in to program the DMA base register.
 *
 * @param[in] pFlcnCfg   Structure to hold falcon config
 * @param[in] fbBase     Base address of fb region to be copied
 */
void
acrlibProgramDmaBase_GM200
(
    PACR_FLCN_CONFIG    pFlcnCfg,
    LwU64               fbBase
)
{
    LwU32 fbBaseLo32 = lwU64Lo32(fbBase);
    if (fbBaseLo32 == 0xDEADBEEFU)
    {
        acrWriteStatusToFalconMailbox(ACR_ERROR_VARIABLE_SIZE_OVERFLOW);
        lwuc_halt();
    }
    acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFBASE, fbBaseLo32);
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
acrlibProgramRegionCfg_GM200
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
        data = ACR_REG_RD32(BAR0, pFlcnCfg->regCfgAddr);
        data = (data & mask) | ((regionID & 0xFU) << shift);
        ACR_REG_WR32(BAR0, pFlcnCfg->regCfgAddr, data);
        data = ACR_REG_RD32(BAR0, pFlcnCfg->regCfgAddr);
    }
    else
    {
        if (bUseCsb)
        {
            tgt  = CSB_FLCN;
#ifdef ACR_FALCON_PMU
            addr = LW_CPWR_FBIF_REGIONCFG;
#else
            ct_assert(0);
#endif
        }

        data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, tgt, addr);
        data = (data & mask) | ((regionID & 0xFU) << shift);
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, tgt, addr, data);
    }

    return ACR_OK;
}
