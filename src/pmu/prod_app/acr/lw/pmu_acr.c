/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_acr.c
 * @brief  PMU ACR functions
 *
 * ACR helper functions.
 * On RISCV CheetAh builds, this file is shared between the main PMU build and
 * the ACR partition.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE))
// Use the tegra_acr submake
# include "acr.h"
# include "g_acrlib_hal.h"
# include "acr_objacrlib.h"
#else // (PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE))
# include "acr/pmu_acr.h"
#endif // (PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE))

#ifdef UPROC_RISCV
# include "rmlsfm_new_wpr_blob.h"
# include "sbilib/sbilib.h"
#endif // UPROC_RISCV

/* ------------------------- Application Includes --------------------------- */
#include "lib_lw64.h"

#include "acr/task_acr.h"
#include "acr/pmu_objacr.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_falcon_v4.h"

#include "config/g_acr_private.h"

/* ------------------------- Global Variables ------------------------------- */
OBJACR              Acr;
OBJACR_ALIGNED_256  Acr256
    GCC_ATTRIB_SECTION("alignedData256", "Acr256");

/* ------------------------- Type Definitions ------------------------------- */
#ifdef UPROC_RISCV
typedef struct
{
    LwU32 ucodeOffset;
    LwU32 ucodeSize;
    LwU32 dataSize;
    LwU32 blCodeSize;
    LwU32 blImemOffset;
    LwU32 blDataOffset;
    LwU32 blDataSize;
    LwU32 appCodeOffset;
    LwU32 appCodeSize;
    LwU32 appDataOffset;
    LwU32 appDataSize;
    LwU32 flags;
} LSF_LSB_HEADER_TMP;
#endif // UPROC_RISCV

/* ------------------------- Macros and Defines ----------------------------- */
#if (!PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE))
// Validation defines
#define IS_FALCONID_ILWALID(id, off)                                           \
    (((id) == LSF_FALCON_ID_ILWALID) || ((id) >= LSF_FALCON_ID_END_PMU) || (!(off)))
#endif // (!PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE))


#ifdef PMU_TEGRA_ACR_PARTITION
// In the ACR partition, we're in atomic context (effectively outside the RTOS), so no need for DMA lock
#define acrDmaLockAcquire()  lwosNOP()
#define acrDmaLockRelease()  lwosNOP()
#else // PMU_TEGRA_ACR_PARTITION
#define acrDmaLockAcquire()  lwosDmaLockAcquire()
#define acrDmaLockRelease()  lwosDmaLockRelease()
#endif // PMU_TEGRA_ACR_PARTITION

/* ------------------------- Prototypes ------------------------------------- */

static FLCN_STATUS s_acrValidateWprVA( RM_FLCN_MEM_DESC *wprRegiolwirtAddr, void *buffer)
        GCC_ATTRIB_SECTION("imem_acr", "_acrvalidatewprva");
static FLCN_STATUS s_acrPrivLoadTargetFalcon(LwU32 dstOff, LwU32 fbOff, LwU32 sizeInBytes, LwU32 wprRegionID, LwU8 bIsDstImem, ACR_FLCN_CONFIG *pFlcnCfg)
        GCC_ATTRIB_SECTION("imem_acr", "s_acrPrivLoadTargetFalcon");
static FLCN_STATUS s_acrSetupLSFalcon(RM_FLCN_U64 *pWprBase, LwU32 wprRegionID, PLSF_WPR_HEADER  pWprHeader, ACR_FLCN_CONFIG *pFlcnCfg, LSF_LSB_HEADER *pLsbHeader)
        GCC_ATTRIB_SECTION("imem_acr", "s_acrSetupLSFalcon");
static void s_acrAccessWprHeader(LwBool bWrite)
        GCC_ATTRIB_SECTION("imem_acr", "s_acrAccessWprHeader");

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief s_acrGetWprHeader
 *        points to the WPR header of falconId
 *
 * @param[in]  falconId Falcon Id
 * @param[out] ppWprHeader WPR header of falcon Id
 *
 * @return FLCN_OK   Success
 *         FLCN_ERROR Failed to located WPR header of falcon Id
 */
static FLCN_STATUS
s_acrGetWprHeader
(
    LwU32 falconId,
    PLSF_WPR_HEADER *ppWprHeader
)
{
    LwU8 index;
    PLSF_WPR_HEADER pWprHeader;
    *ppWprHeader = NULL;

    pWprHeader = (PLSF_WPR_HEADER)Acr256.pWpr;
    // Go through falcon header
    for (index=0; index < LSF_FALCON_ID_END_PMU; index++)
    {
        // Check if this is the end of WPR header by checking falconID
        if (IS_FALCONID_ILWALID(pWprHeader[index].falconId, pWprHeader[index].lsbOffset))
        {
            break;
        }

        if (pWprHeader[index].falconId == falconId)
        {
            *ppWprHeader = (PLSF_WPR_HEADER)&pWprHeader[index];
            break;
        }
    }

    if (*ppWprHeader == NULL)
    {
        return FLCN_ERROR;
    }
    return FLCN_OK;
}

/*!
 * @brief s_acrSetupLSFalcon:  Copies IMEM and DMEM into falcon
 *
 * @param[in] pWprBase   :
 * @param[in] pFlcnCfg   : Falcon Config register
 * @param[in] pLsbHeader : LSB Header
 */
FLCN_STATUS
s_acrSetupLSFalcon
(
    RM_FLCN_U64     *pWprBase,
    LwU32            regionId,
    PLSF_WPR_HEADER  pWprHeader,
    ACR_FLCN_CONFIG *pFlcnCfg,
    PLSF_LSB_HEADER  pLsbHeader
)
{
    ACR_STATUS  status = ACR_OK;
    LwU32       addrHi;
    LwU32       addrLo;
    LwU32       wprBase;
    LwU32       blWprBase;
    LwU32       dst;

#ifdef UPROC_FALCON
    // Check if the code validation completed before going forward
    if ((pWprHeader->status != LSF_IMAGE_STATUS_VALIDATION_DONE) &&
        (pWprHeader->status != LSF_IMAGE_STATUS_VALIDATION_SKIPPED) &&
        (pWprHeader->status != LSF_IMAGE_STATUS_BOOTSTRAP_READY))
    {

        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }
#endif // UPROC_FALCON

    addrHi = pWprBase->hi;
    addrLo = pWprBase->lo;

    addrLo += Acr.wprOffset;
    if (addrLo < pWprBase->lo)
    {
        addrHi += 1;
    }

    wprBase = (addrLo >> IMEM_BLOCK_WIDTH) |
              (addrHi << (32 - IMEM_BLOCK_WIDTH));

    pLsbHeader->blCodeSize = LW_ALIGN_UP(pLsbHeader->blCodeSize, IMEM_BLOCK_SIZE);
    PMU_HALT_COND(ACR_OK == acrSetupTargetRegisters_HAL(&Acr, pFlcnCfg));

    //
    // Load code into IMEM
    // BL starts at offset, but since the IMEM VA is not zero, we need to make
    // sure FBOFF is equal to the expected IMEM VA. So adjusting the FB BASE to
    // make sure FBOFF equals to VA as expected.
    //
    blWprBase = ((wprBase) +
                 ((pLsbHeader->ucodeOffset) >> IMEM_BLOCK_WIDTH)) -
        (pLsbHeader->blImemOffset >> IMEM_BLOCK_WIDTH);

    // Check if code needs to be loaded at start of IMEM or at end of IMEM
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _LOAD_CODE_AT_0, _TRUE, pLsbHeader->flags))
    {
        dst = 0;
    }
    else
    {
        dst = (acrFindFarthestImemBl_HAL(&Acr, pFlcnCfg, pLsbHeader->blCodeSize) *
               IMEM_BLOCK_SIZE);
    }

    // Check if this falcon should be loaded via priv copy or DMA
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _FORCE_PRIV_LOAD, _TRUE, pLsbHeader->flags))
    {
        status = s_acrPrivLoadTargetFalcon(0,
              pLsbHeader->ucodeOffset + pLsbHeader->appCodeOffset, pLsbHeader->appCodeSize,
              regionId, LW_TRUE, pFlcnCfg);
        if (status != ACR_OK)
        {
            return status;
        }

        // Load data into DMEM
        status = s_acrPrivLoadTargetFalcon(0,
              pLsbHeader->ucodeOffset + pLsbHeader->appDataOffset, pLsbHeader->appDataSize,
              regionId, LW_FALSE, pFlcnCfg);
        if (status != ACR_OK)
        {
            return status;
        }

        // Set the BOOTVEC
        acrSetupBootvec_HAL(&Acr, pFlcnCfg, 0);
    }
    else
    {
        status = acrIssueTargetFalconDma_HAL(&Acr,
              dst, blWprBase, pLsbHeader->blImemOffset, pLsbHeader->blCodeSize,
              regionId, LW_TRUE, LW_TRUE, pFlcnCfg);
        if (status != ACR_OK)
        {
            return status;
        }

        // Load data into DMEM
        status = acrIssueTargetFalconDma_HAL(&Acr,
              0, wprBase, pLsbHeader->blDataOffset, pLsbHeader->blDataSize,
              regionId, LW_TRUE, LW_FALSE, pFlcnCfg);
        if (status != ACR_OK)
        {
            return status;
        }

        // Set the BOOTVEC
        acrSetupBootvec_HAL(&Acr, pFlcnCfg, pLsbHeader->blImemOffset);
    }

    // Check if Falcon wants virtual ctx
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _SET_VA_CTX, _TRUE, pLsbHeader->flags))
    {
        acrSetupCtxDma_HAL(&Acr, pFlcnCfg, pFlcnCfg->ctxDma, LW_FALSE);
    }
    // Check if Falcon wants REQUIRE_CTX
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _DMACTL_REQ_CTX, _TRUE, pLsbHeader->flags))
    {
        acrSetupDmaCtl_HAL(&Acr, pFlcnCfg, LW_TRUE);
    }
    else
    {
        acrSetupDmaCtl_HAL(&Acr, pFlcnCfg, LW_FALSE);
    }
    //
    // Program the IMEM and DMEM PLMs to the final desired values (which may
    // be level 3 for certain falcons) now that acr is done loading the
    // code onto the  falcon.
    //
    acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg, REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK, pFlcnCfg->imemPLM);
    acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK, pFlcnCfg->dmemPLM);

    // Set the status in WPR header
    pWprHeader->status = LSF_IMAGE_STATUS_BOOTSTRAP_READY;

    return status;
}

#ifdef UPROC_RISCV
/*!
 * Update transcfg and regioncfg which is needed to access wpr region.
 */
static void
s_updateRegionCfg
(
    LwU8 wprRegionId
)
{
    riscvFbifTransCfgSet(RM_PMU_DMAIDX_UCODE,
            DRF_DEF(_CPWR, _FBIF_TRANSCFG, _TARGET, _COHERENT_SYSMEM) |
            DRF_DEF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL)      |
            DRF_DEF(_CPWR, _FBIF_TRANSCFG, _L2C_WR, _L2_EVICT_NORMAL) |
            DRF_DEF(_CPWR, _FBIF_TRANSCFG, _L2C_RD, _L2_EVICT_NORMAL) |
            DRF_DEF(_CPWR, _FBIF_TRANSCFG, _WACHK0, _DISABLE)         |
            DRF_DEF(_CPWR, _FBIF_TRANSCFG, _WACHK1, _DISABLE)         |
            DRF_DEF(_CPWR, _FBIF_TRANSCFG, _RACHK0, _DISABLE)         |
            DRF_DEF(_CPWR, _FBIF_TRANSCFG, _RACHK1, _DISABLE));
    riscvFbifRegionCfgSet(RM_PMU_DMAIDX_UCODE, wprRegionId);
}
#endif // UPROC_RISCV

/*!
 * Initiazes WPR region details. WPR is a client of ACR and we can have multiple
 * clients. WPR region is used to store the details of LS falcon and after PMU
 * Boot, we need to tell PMU ACR task about the start address, WPR region ID,
 * and WPR offset.
 */
FLCN_STATUS
acrInitWprRegion
(
    LwU8  wprRegionId,
    LwU8  wprOffset
)
{
    FLCN_STATUS status          = FLCN_OK;
    LwU64       wprStartAddress = 0;
    LwU64       wprEndAddress   = 0;
    LwU64       wprRegionSize   = 0;
    LwU32       index;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libAcr)
    };

    Acr.wprOffset   = wprOffset;
    Acr.wprRegionId = wprRegionId;

    // ACR regions start with 1
    if (Acr.wprRegionId == 0)
    {
        // TODO: This should fail with an error to RM and not HALT PMU.
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto acrInitWprRegion_exit;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = acrGetRegionDetails_HAL(&Acr, Acr.wprRegionId, &wprStartAddress,
                                         &wprEndAddress, NULL, NULL);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    if (status != FLCN_OK)
    {
        goto acrInitWprRegion_exit;
    }

    if (wprStartAddress == 0)
    {
        status = FLCN_ERROR;
        goto acrInitWprRegion_exit;
    }

#ifdef UPROC_RISCV
    s_updateRegionCfg(Acr.wprRegionId);
#endif // UPROC_RISCV

    LW64_FLCN_U64_PACK(&Acr.wprRegionDesc.address, &wprStartAddress);
    lw64Sub(&wprRegionSize, &wprEndAddress, &wprStartAddress);

    Acr.wprRegionDesc.params =
        DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, (LwU32)wprRegionSize) |
        DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, RM_PMU_DMAIDX_UCODE);

    // Read WPR Header
    s_acrAccessWprHeader(LW_FALSE);

    Acr.wprFalconIndex = 0;

    // Loop through each WPR header. Break when we see invalid falcon ID.
    for (index = 0; index < LSF_FALCON_ID_END_PMU; index++)
    {
        LSF_WPR_HEADER *pWprHeader =
            (PLSF_WPR_HEADER)&Acr256.pWpr[(sizeof(LSF_WPR_HEADER)*index)];

        // Check if this is the end of WPR header by checking falconID
        if (IS_FALCONID_ILWALID(pWprHeader->falconId, pWprHeader->lsbOffset))
        {
            break;
        }

        //
        // Check the status of the image
        //   If image is aleady validated or bootstrapped, continue
        //   If the bootstrap owner is not PMU, continue
        //
        if (
#ifdef UPROC_FALCON
            (pWprHeader->bootstrapOwner != LSF_BOOTSTRAP_OWNER_PMU) ||
#else // UPROC_FALCON
            (pWprHeader->bootstrapOwner != LSF_BOOTSTRAP_OWNER_GSPLITE) ||
#endif // UPROC_FALCON
            ((pWprHeader->status != LSF_IMAGE_STATUS_VALIDATION_DONE) &&
             (pWprHeader->status != LSF_IMAGE_STATUS_VALIDATION_SKIPPED)))
        {
            continue;
        }

        Acr.wprFalconIndex |= (LwU8)LWBIT(pWprHeader->falconId);
    }

acrInitWprRegion_exit:
    return status;
}

/*!
 * @brief s_acrValidateWprVA
 *          check whether address points to beginning of WPR by reading WPR header
 *
 * @ param[in] wprRegiolwirtAddr
 *              Virtual address mem descriptor to be verified
 * @ param[in] buffer
 *              buffer to be used for dma
 *
 */
static FLCN_STATUS
s_acrValidateWprVA
(
    RM_FLCN_MEM_DESC *wprRegiolwirtAddr,
    void *buffer
)
{
    LwU32            i        = 0;
    LwU32            databkp  = 0;
    LwU32            *wprData = (LwU32*)Acr256.pWpr;
    LwU32            ctxDma   = RM_PMU_DMAIDX_VIRT;
    ACR_FLCN_CONFIG  pmuFlcnCfg;

    // take back up of current region config
    PMU_HALT_COND(acrGetFalconConfig_HAL(&Acr, LSF_FALCON_ID_PMU, LSF_FALCON_INSTANCE_DEFAULT_0, &pmuFlcnCfg)
                                         == ACR_OK);
    databkp = acrFlcnRegRead_HAL(&Acr, &pmuFlcnCfg, CSB_FLCN, LW_CPWR_FBIF_REGIONCFG);
    // program region config to point to virtual
#ifdef UPROC_RISCV
    riscvFbifRegionCfgSet(ctxDma, LSF_WPR_EXPECTED_REGION_ID);
#else // UPROC_RISCV
    acrProgramRegionCfg_HAL(&Acr, &pmuFlcnCfg, LW_FALSE, ctxDma, LSF_WPR_EXPECTED_REGION_ID);
#endif // UPROC_RISCV

    if (FLCN_OK != dmaReadUnrestricted(buffer, wprRegiolwirtAddr,
                                       Acr.wprOffset,
                                       LSF_WPR_HEADERS_TOTAL_SIZE_MAX_PMU))
    {
        // DMA fails to read WPR header
        PMU_HALT();
    }
    //restore region config
#ifdef UPROC_RISCV
    riscvFbifRegionCfgSet(ctxDma, DRF_VAL(_CPWR, _FBIF_REGIONCFG, _T(ctxDma), databkp));
#else // UPROC_RISCV
    acrFlcnRegWrite_HAL(&Acr, &pmuFlcnCfg, CSB_FLCN, LW_CPWR_FBIF_REGIONCFG, databkp);
#endif // UPROC_RISCV
    //compare read data with WPR header
    for (i = 0; i < (LSF_WPR_HEADERS_TOTAL_SIZE_MAX_PMU/4); i++)
    {
        if (*(wprData+i) != *((LwU32*)buffer+i))
        {
            //return as no WPR as VA does not point to it
            return ACR_ERROR_NO_WPR;
        }
    }
    return ACR_OK;

}

/*!
 * @brief   Update resident WPR header in OBJACR with FB
 *
 * @param[in]   bWrite  If set WR is requested, RD otherwise
 */
static void
s_acrAccessWprHeader
(
    LwBool bWrite
)
{
    FLCN_STATUS status;

    if (bWrite)
    {
        // Write whole WPR header
        status = dmaWriteUnrestricted((void *)Acr256.pWpr,
                                      &Acr.wprRegionDesc,
                                      Acr.wprOffset,
                                      LSF_WPR_HEADERS_TOTAL_SIZE_MAX_PMU);
    }
    else
    {
        // Read whole WPR header
        status = dmaReadUnrestricted((void *)Acr256.pWpr,
                                     &Acr.wprRegionDesc,
                                     Acr.wprOffset,
                                     LSF_WPR_HEADERS_TOTAL_SIZE_MAX_PMU);
    }

    if (status != FLCN_OK)
    {
        PMU_HALT();
    }
}

/*!
 * @brief s_acrReadLsbHeader:  Reads LSB header of Falcon from FB
 *
 * @param[in] pWprHeader     : WPR header of falcon
 * @param[out] pLsbHeader    : LSB header of Falcon
 */
static FLCN_STATUS
s_acrReadLsbHeader
(
    PLSF_WPR_HEADER pWprHeader,
    PLSF_LSB_HEADER pLsbHeader
)
{
#ifdef UPROC_FALCON
    LwU32   lsbSize = LW_ALIGN_UP(sizeof(LSF_LSB_HEADER),
                                  LSF_LSB_HEADER_ALIGNMENT);
    if (FLCN_OK != dmaReadUnrestricted((void *)pLsbHeader,
                                       &Acr.wprRegionDesc,
                                       pWprHeader->lsbOffset,
                                       lsbSize))
    {
        PMU_HALT();
    }
#else // UPROC_FALCON
    LSF_LSB_HEADER_TMP LsbHeaderTmp;
    LwU32 lsbOffset = pWprHeader->lsbOffset;
    lsbOffset = pWprHeader->lsbOffset + (LwU32)sizeof(LSF_UCODE_DESC_WRAPPER);
    if (FLCN_OK != dmaReadUnrestricted((void *)&LsbHeaderTmp,
                                        &Acr.wprRegionDesc,
                                        lsbOffset,
                                        sizeof(LSF_LSB_HEADER_TMP)))
    {
       PMU_HALT();
    }
    // copy only ucode desc data other than signature
    memcpy(&pLsbHeader->ucodeOffset, &LsbHeaderTmp, sizeof(LSF_LSB_HEADER_TMP));
#endif // UPROC_FALCON

    return FLCN_OK;
}

/*!
 * @brief acrBootstrapFalcon
 *        Falcon's UC is selwred in ACR region and only LS clients like PMU can
 *        access it. During resetting an engine, RM needs to contact PMU to rest
 *        particular falcon.
 *
 * @param[in] falconId Falcon ID which needs to be reset
 *
 * @return FLCN_STATUS: OK if successful reset of Falcon
 */
FLCN_STATUS
acrBootstrapFalcon
(
    LwU32           falconId,
    LwBool          bReset,
    LwBool          bUseVA,
    RM_FLCN_U64    *pWprBaseVirtual
)
{
    ACR_STATUS      status       = ACR_OK;
    PLSF_WPR_HEADER pWprHeader;
    ACR_FLCN_CONFIG flcnCfg;
    PLSF_LSB_HEADER pLsbHeader;
    RM_FLCN_U64    *pWprBase;

    if (acrIsFalconBootstrapBlocked_HAL(&Acr, falconId))
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    PMU_HALT_COND(acrGetFalconConfig_HAL(&Acr, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg)
                                       == ACR_OK);

    pLsbHeader = (PLSF_LSB_HEADER)&Acr256.lsbHeader;

    s_acrGetWprHeader(falconId, &pWprHeader);
    s_acrReadLsbHeader(pWprHeader, pLsbHeader);

    if (bReset)
    {
        PMU_HALT_COND(acrResetFalcon_HAL(&Acr, &flcnCfg, LW_FALSE) == ACR_OK);
        PMU_HALT_COND(acrPollForScrubbing_HAL(&Acr, &flcnCfg) == ACR_OK);
    }

    //
    // Setup the LS falcon
    //
    if (!bUseVA)
    {
        pWprBase = &Acr.wprRegionDesc.address;
    }
    else
    {
        pWprBase = pWprBaseVirtual;
    }

    status = s_acrSetupLSFalcon(pWprBase, Acr.wprRegionId, pWprHeader, &flcnCfg, pLsbHeader);
    if (status == ACR_OK)
    {
        // Write whole WPR header
        s_acrAccessWprHeader(LW_TRUE);
    }
    return (status == ACR_OK) ? FLCN_OK : FLCN_ERROR;
}

/*!
 * Falcon's UC is selwred in ACR region and only LS clients like the PMU can
 * access it. During resetting an engine, RM needs to contact PMU to reset and
 * boot strap GR falcons.
 */
FLCN_STATUS
acrBootstrapGrFalcons
(
    LwU32 falconIdMask,
    LwBool bReset,
    LwU32 falcolwAMask,
    RM_FLCN_U64 wprBaseVirtual
)
{
    FLCN_STATUS      status = FLCN_OK;
    LwU32            falconId;
    RM_FLCN_MEM_DESC wprRegiolwirtAddress;

    // Make sure we valid falcon ID mask
    PMU_HALT_COND(falconIdMask != 0);

    // Supports only GR falcons
    PMU_HALT_COND((falconIdMask &
                (~(BIT(LSF_FALCON_ID_FECS) | BIT(LSF_FALCON_ID_GPCCS)))) == 0);

    if ((falcolwAMask & falconIdMask) != 0)
    {
        wprRegiolwirtAddress.address = wprBaseVirtual;
        wprRegiolwirtAddress.params =
            FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                            RM_PMU_DMAIDX_VIRT, Acr.wprRegionDesc.params);

        PMU_HALT_COND(s_acrValidateWprVA(&wprRegiolwirtAddress, (void *)Acr256.xmemStore) == ACR_OK);
    }

    // boot one by one and start with zero
    for (falconId = 0; falconId < LSF_FALCON_ID_END_PMU; falconId++)
    {
        if ((falconIdMask & BIT(falconId)) != 0)
        {
            LwBool bUseVA = ((falcolwAMask & BIT(falconId)) != 0);

            status = acrBootstrapFalcon(falconId, bReset, bUseVA, &wprBaseVirtual);
            if (status != FLCN_OK)
            {
                break;
            }
        }
    }

    return status;
}

/*!
 * @brief Use priv access to load target falcon memory.
 *        Supports loading into both IMEM and DMEM of target falcon from FB.
 *        Expects size to be 256 multiple.
 *
 * @param[in] dstOff       Destination offset in either target falcon DMEM or IMEM
 * @param[in] fbOff        Offset from fbBase
 * @param[in] sizeInBytes  Number of bytes to be transferred
 * @param[in] regionID     ACR region ID to be used for this transfer
 * @param[in] bIsDstImem   TRUE if destination is IMEM
 * @param[in] pFlcnCfg     Falcon config
 *
 */
static FLCN_STATUS
s_acrPrivLoadTargetFalcon
(
    LwU32               dstOff,
    LwU32               fbOff,
    LwU32               sizeInBytes,
    LwU32               regionID,
    LwU8                bIsDstImem,
    ACR_FLCN_CONFIG    *pFlcnCfg
)
{
    ACR_STATUS  status        = ACR_OK;
    LwU32       bytesXfered   = 0;
    LwU32       tag;
    LwU32       i;

    //
    // Sanity checks
    // Only does 256B transfers
    //
    if ((!LW_IS_ALIGNED(sizeInBytes, IMEM_BLOCK_SIZE)) ||
        (!LW_IS_ALIGNED(dstOff, IMEM_BLOCK_SIZE))      ||
        (!LW_IS_ALIGNED(fbOff, IMEM_BLOCK_SIZE)))
    {
        PMU_HALT();
    }

    // Setup the MEMC register
    if (bIsDstImem)
    {
        acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMC(0),
                               REF_NUM(LW_PFALCON_FALCON_IMEMC_OFFS, 0) |
                               REF_NUM(LW_PFALCON_FALCON_IMEMC_BLK, dstOff >> 8) |
                               REF_NUM(LW_PFALCON_FALCON_IMEMC_AINCW, 1));
    }
    else
    {
        acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMEMC(0),
                               REF_NUM(LW_PFALCON_FALCON_DMEMC_OFFS, 0) |
                               REF_NUM(LW_PFALCON_FALCON_DMEMC_BLK, dstOff >> 8) |
                               REF_NUM(LW_PFALCON_FALCON_DMEMC_AINCW, 1));
    }

    // Its recommended to use DMA section when task is doing back to back DMA.
    acrDmaLockAcquire();
    {
        tag = 0;
        while (bytesXfered < sizeInBytes)
        {
            // Read in the next block
            if (FLCN_OK != dmaReadUnrestricted((void*)Acr256.xmemStore,
                                               &Acr.wprRegionDesc,
                                               Acr.wprOffset + fbOff + bytesXfered,
                                               IMEM_BLOCK_SIZE))
            {
                PMU_HALT();
            }

            if (bIsDstImem)
            {
                // Setup the tags for the instruction memory.
                acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMT(0), REF_NUM(LW_PFALCON_FALCON_IMEMT_TAG, tag));
                for (i = 0; i < IMEM_BLOCK_SIZE / sizeof(LwU32); i++)
                {
                    acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMD(0), Acr256.xmemStore[i]);
                }
                tag++;
            }
            else
            {
                for (i = 0; i < IMEM_BLOCK_SIZE / sizeof(LwU32); i++)
                {
                    acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMEMD(0), Acr256.xmemStore[i]);
                }
            }
            bytesXfered += IMEM_BLOCK_SIZE;
        }
    }
    acrDmaLockRelease();

    return status;
}
