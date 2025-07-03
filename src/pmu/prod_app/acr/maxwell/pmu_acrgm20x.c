/* _LWPMU_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWPMU_COPYRIGHT_END_
 */

/*!
 * @file   pmu_acrgm20x.c
 * @brief  GM20X Access Control Region
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application includes --------------------------- */
#if PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE)
#include "acr.h"
#else
#include "acr/pmu_acr.h"
#include "acr/pmu_acrutils.h"
#endif
#include "acr/task_acr.h"
#include "acr/pmu_objacr.h"
#include "pmu_objpg.h"
#include "dev_fb.h"
#include "dev_pwr_pri.h"
#include "dev_pwr_csb.h"
#include "dev_fbif_v4.h"
#include "dev_falcon_v4.h"
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_SEC2_PRESENT))
#include "dev_sec_csb.h"
#endif
#include "dev_graphics_nobundle.h"
#include "dev_graphics_nobundle_addendum.h"
#include "pmu_bar0.h"
#include "dbgprintf.h"
#include "config/g_acr_private.h"
#include "dev_master.h"
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_LWDEC_PRESENT))
#include "dev_lwdec_pri.h"
#endif

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */

static RM_FLCN_ACR_DESC *pAcrDesc;

/* ------------------------ Global variables ------------------------------- */
#define DIV_ROUND_UP(a, b) (((a) + (b) - 1) / (b))
/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/*!
 * Setting default alignment to 128KB
 * TODO: Replace this with manual define when its available
 */
#define LW_ACR_DEFAULT_ALIGNMENT        0x20000

// Timeouts
#define ACR_DEFAULT_TIMEOUT_NS          (100*1000*1000)  // 100 ms

/* ------------------------ Local Functions  ------------------------------- */
#if (!(PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE)))
static void _acrInitRegMapping_GM20X(ACR_REG_MAPPING acrRegMap[REG_LABEL_END])
    GCC_ATTRIB_SECTION("imem_acr", "_acrInitRegMapping_GM20X");
#endif // !(PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE))
/* ------------------------ Public Functions  ------------------------------ */

static LwU32
s_acrReadMmuWprInfoRegister_GM20X(LwU32 index)
    GCC_ATTRIB_SECTION("imem_libAcr", "s_acrReadMmuWprInfoRegister_GM20X");

/*!
 * @brief acrGetRegionDetails_GM20X
 *          returns ACR region details
 *
 * @param [in]  regionID ACR region ID
 * @param [out] pStart   Start address of ACR region
 * @param [out] pEnd     End address of ACR region
 * @param [out] pRmask   Read permission of ACR region
 * @param [out] pWmask   Write permission of ACR region
 *
 * @return      FLCN_OK : If proper register details are found
 *              FLCN_ERR_ILWALID_ARGUMENT : invalid Region ID
 */
FLCN_STATUS
acrGetRegionDetails_GM20X
(
    LwU32      regionID,
    LwU64      *pStart,
    LwU64      *pEnd,
    LwU16      *pRmask,
    LwU16      *pWmask
)
{
    FLCN_STATUS status    = FLCN_OK;
    LwU32      index      = 0;
    LwU32      acrRegions;
    LwU32      regVal;
    LWU64_TYPE uStart;
    LWU64_TYPE uEnd;

    acrRegions = (LW_PFB_PRI_MMU_VPR_WPR_WRITE_ALLOW_READ_WPR__SIZE_1 - 1);

    if ((regionID == 0) || (regionID > acrRegions))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if (pStart != NULL)
    {
        // Read start address
        index = LW_PFB_PRI_MMU_WPR_INFO_INDEX_WPR_ADDR_LO(regionID);
        regVal = s_acrReadMmuWprInfoRegister_GM20X(index);
        regVal = DRF_VAL(_PFB, _PRI_MMU_WPR_INFO, _DATA, regVal);

        LWU64_HI(uStart) = regVal >> (32 - LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT);
        LWU64_LO(uStart) = regVal << LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;
        *pStart = uStart.val;
    }

    if (pEnd != NULL)
    {
        // Read end address
        index = LW_PFB_PRI_MMU_WPR_INFO_INDEX_WPR_ADDR_HI(regionID);
        regVal = s_acrReadMmuWprInfoRegister_GM20X(index);
        regVal = DRF_VAL(_PFB, _PRI_MMU_WPR_INFO, _DATA, regVal);

        LWU64_HI(uEnd) = regVal >> (32 - LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT);
        LWU64_LO(uEnd) = regVal << LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;
        //
        // End address as read from the register always points to start of
        // the last aligned address. Add the alignment to make it point to
        // the first byte after the end.
        //
        lw64Add32_MOD(&(uEnd.val), LW_ACR_DEFAULT_ALIGNMENT);

        *pEnd = uEnd.val;
    }

    // Read ReadMask
    if (pRmask != NULL)
    {
        index = LW_PFB_PRI_MMU_WPR_INFO_INDEX_ALLOW_READ;
        regVal = s_acrReadMmuWprInfoRegister_GM20X(index);
        *pRmask = DRF_IDX_VAL( _PFB, _PRI_MMU_WPR_INFO, _ALLOW_READ_WPR, regionID, regVal);
    }

    if (pWmask != NULL)
    {
        // Read WriteMask
        index = FLD_SET_DRF(_PFB, _PRI_MMU_WPR, _INFO_INDEX, _ALLOW_WRITE, 0);
        regVal = s_acrReadMmuWprInfoRegister_GM20X(index);
        *pWmask = DRF_IDX_VAL( _PFB, _PRI_MMU_WPR_INFO, _ALLOW_READ_WPR, regionID, regVal);

        //
        // TODO: This is needed because of a bug in manual defines. This will be
        //      removed once the the bug is fixed.
        //
        *pWmask &= 0xF;
    }

    return status;
}

/*!
 * @brief: acrVerifyApertureSetup_GM20X
 *          Verify the aperture setup of the context DMAs.
 *
 * @param[in] dmaIdx: Context DMA of memory aperture
 *
 * @return : FLCN_OK: Context DMA is properly programmed
 *           FLCN_ERROR: Context DMA is setup is invalid
 */
FLCN_STATUS
acrApertureSetupVerify_GM20X
(
    LwU8 ctxDma
)
{
    LwU32 setup;
    LwU32 data;
    LwBool bProper = LW_FALSE;

    setup = REG_RD32(BAR0, LW_PPWR_FBIF_TRANSCFG(ctxDma));

    switch (ctxDma)
    {
        case RM_PMU_DMAIDX_UCODE:
            data = REG_RD32(CSB, LW_CPWR_FBIF_REGIONCFG);
            data = (data) & (Acr.wprRegionId << ctxDma*4);
            bProper = bProper & (data == (Acr.wprRegionId << ctxDma*4));
        break;
        case RM_PMU_DMAIDX_VIRT:
            bProper = FLD_TEST_DRF(_PPWR, _FBIF_TRANSCFG, _MEM_TYPE, _VIRTUAL,
                        setup);
        break;

        case RM_PMU_DMAIDX_PHYS_VID:
            bProper = (FLD_TEST_DRF(_PPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL, setup) &&
                        FLD_TEST_DRF(_PPWR, _FBIF_TRANSCFG, _TARGET, _LOCAL_FB, setup));
        break;

        case RM_PMU_DMAIDX_PHYS_SYS_COH:
            bProper = (FLD_TEST_DRF(_PPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL, setup) &&
                        FLD_TEST_DRF(_PPWR, _FBIF_TRANSCFG, _TARGET, _COHERENT_SYSMEM, setup));
        break;

        case RM_PMU_DMAIDX_PHYS_SYS_NCOH:
            bProper = (FLD_TEST_DRF(_PPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL, setup) &&
                        FLD_TEST_DRF(_PPWR, _FBIF_TRANSCFG, _TARGET, _NONCOHERENT_SYSMEM, setup));
        break;

        default:
            return FLCN_ERROR;
    }

    return bProper ? FLCN_OK : FLCN_ERROR;
}

/*!
 * @brief: acrWriteErrorCode_GM20X
 *          Writes error codes to MAILBOX registers.
 *
 * @param[in] dmaIdx: Context DMA of memory aperture
 *
 * @return : FLCN_OK: Context DMA is properly programmed
 *           FLCN_ERROR: Context DMA is setup is invalid
 */
void
acrWriteErrorCode_GM20X
(
    LwU32 mailBox0,
    LwU32 mailBox1
)
{
    REG_WR32(CSB, LW_CPWR_FALCON_MAILBOX0, mailBox0);
    REG_WR32(CSB, LW_CPWR_FALCON_MAILBOX1, mailBox1);
}

/*!
 * @brief acrCheckRegionSetup_GM20X
 *          returns if ACR is setup correctly
 *
 * @return      FLCN_OK : If ACR is setup i.e enabled
 *              FLCN_ERROR : If ACR is disabled
 */
FLCN_STATUS
acrCheckRegionSetup_GM20X(void)
{
    LwU32 reg;
    LwU32 startAddr, endAddr;

    // Read start address
    reg = FLD_SET_DRF( _PFB, _PRI_MMU_WPR_INFO, _INDEX, _WPR1_ADDR_LO, 0);
    REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_INFO, reg);
    startAddr = DRF_VAL(_PFB, _PRI_MMU_WPR_INFO, _DATA, REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_INFO));
    startAddr = startAddr << LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;

    // Read end address
    reg = FLD_SET_DRF( _PFB, _PRI_MMU_WPR_INFO, _INDEX, _WPR1_ADDR_HI, 0);
    REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_INFO, reg);
    endAddr = DRF_VAL(_PFB, _PRI_MMU_WPR_INFO, _DATA, REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_INFO));
    endAddr = endAddr << LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;

    if (startAddr >= endAddr)
    {
        return FLCN_ERROR;
    }
    return FLCN_OK;
}

/*!
 * @brief acrPatchRegionInfo_GM20X
 *          Copies ACR Region DMEM data to BSI RAM for particular phase
 *
 * @param [in]  phaseId   phase of particular GC6 phase
 *
 * @return      FLCN_OK   : If ACR info is patched successfully
 *              FLCN_ERROR : If ACR info is not patched
 */
FLCN_STATUS
acrPatchRegionInfo_GM20X
(
    LwU32 phaseId
)
{
    LwU32 regionId;
    LwU32 offset;
    LwU64 startAddr, endAddr;
    LwU16 readMask, writeMask;

    RM_PMU_PG_ISLAND_PHASE_DESCRIPTOR phaseDesc;

    pgIslandBsiRamDescGet_HAL(&Pg, phaseId, &phaseDesc);

    //
    // offset for ACR phase on BSI ram
    //
    offset = phaseDesc.dmemSrcAddr;

    //
    // Allocate the buffer - this buffer is global and allocated on DMEM since size is large,
    // So can be used for other tasks as well
    //
    Acr.pBsiRWBuf = (LwU32 *)lwosMallocResident(ACR_PHASE_SIZE_DWORD);

    if (Acr.pBsiRWBuf == NULL)
    {
       DBG_PRINTF_ACR(("ACR: Failed to allocate buffer\n", 0, 0, 0, 0));
       return FLCN_ERR_NO_FREE_MEM;
    }

    if (pgIslandBsiRamRW_HAL(&Pg, Acr.pBsiRWBuf, ACR_PHASE_SIZE_DWORD, offset, LW_TRUE) != FLCN_OK )
    {
        return FLCN_ERROR;
    }

    pAcrDesc = (RM_FLCN_ACR_DESC *)(Acr.pBsiRWBuf);

    pAcrDesc->regions.noOfRegions = RM_FLCN_ACR_MAX_REGIONS;

    for (regionId=1; regionId <= RM_FLCN_ACR_MAX_REGIONS; regionId++)
    {
        if (acrGetRegionDetails_HAL(&Acr, regionId, &startAddr, &endAddr, &readMask, &writeMask) != FLCN_OK )
        {
            return FLCN_ERROR;
        }

        if (startAddr < endAddr)
        {
            pAcrDesc->regions.regionProps[regionId-1].startAddress = (LwU32)(startAddr >> 8);
            pAcrDesc->regions.regionProps[regionId-1].endAddress   = (LwU32)(endAddr >> 8);

            pAcrDesc->regions.regionProps[regionId-1].regionID     = (LwU32)regionId;
            pAcrDesc->regions.regionProps[regionId-1].readMask     = (LwU32)readMask;
            pAcrDesc->regions.regionProps[regionId-1].writeMask    = (LwU32)writeMask;
        }
    }

    // Update the memory range setting that will be programmed during BSI phase
    pAcrDesc->mmuMemoryRange = REG_RD32(BAR0, LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE);

    if (pgIslandBsiRamRW_HAL(&Pg, Acr.pBsiRWBuf, ACR_PHASE_SIZE_DWORD, offset, LW_FALSE) != FLCN_OK)
    {
        return FLCN_ERROR;
    }

    return FLCN_OK;
}

static LwU32
s_acrReadMmuWprInfoRegister_GM20X
(
    LwU32 index
)
{
    LwU32 val;

    do
    {
        REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_INFO,
                 DRF_NUM(_PFB, _PRI_MMU_WPR_INFO, _INDEX, index));

        val = REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_INFO);

    //
    // If the index field has changed, some other thread wrote into
    // the register, and the register value is not what we expect.
    // Try again if the index is changed.
    //
    } while (DRF_VAL(_PFB, _PRI_MMU_WPR_INFO, _INDEX, val) != index);
    return val;
}

//
// PMU CheetAh Profiles use ACRLIB from //uproc/tegra_acr/,
// So they do not need below functions copied form //uproc/acr/ directory
//
#if (!(PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE)))
/*!
 * @brief Find reg mapping using reg label
 *
 */
ACR_STATUS
acrFindRegMapping_GM20X
(
    ACR_FLCN_CONFIG    *pFlcnCfg,
    ACR_FLCN_REG_LABEL  acrLabel,
    ACR_REG_MAPPING    *pMap,
    FLCN_REG_TGT       *pTgt
)
{
    ACR_REG_MAPPING acrRegMap[REG_LABEL_END];
    if ((acrLabel >= REG_LABEL_END) || (!pMap) || (!pTgt) ||
        (acrLabel == REG_LABEL_FLCN_END) || (acrLabel == REG_LABEL_FBIF_END))
    {
        return ACR_ERROR_REG_MAPPING;
    }

    if (pFlcnCfg->bIsBoOwner)
    {
        *pTgt = CSB_FLCN;
    }
    else if (acrLabel < REG_LABEL_FLCN_END)
    {
        *pTgt = BAR0_FLCN;
    }
    else
    {
        *pTgt = BAR0_FBIF;
    }

    _acrInitRegMapping_GM20X(acrRegMap);

    pMap->bar0Off   = acrRegMap[acrLabel].bar0Off;
    pMap->pwrCsbOff = acrRegMap[acrLabel].pwrCsbOff;

    return ACR_OK;
}

/*!
 * @brief Program Dmem range registers
 */
static void
_acrProgramDmemRange
(
    ACR_FLCN_CONFIG *pFlcnCfg
)
{
    LwU32 data  = 0;
    LwU32 rdata = 0;

    //
    // Setup DMEM carveouts
    // ((endblock<<16)|startblock)
    // Just opening up one last block
    //
    data = acrFindTotalDmemBlocks_HAL(&Acr, pFlcnCfg) - 1;
    data = (data<<16)|(data);
    if (pFlcnCfg->bIsBoOwner)
    {
        ACR_REG_WR32_STALL(CSB, pFlcnCfg->range0Addr, data);
        rdata = ACR_REG_RD32_STALL(CSB, pFlcnCfg->range0Addr);
    }
    else
    {
        ACR_REG_WR32(BAR0, pFlcnCfg->range0Addr, data);
        rdata = ACR_REG_RD32(BAR0, pFlcnCfg->range0Addr);
    }

    //
    // This is to make sure RANGE0 is programmable..
    // If it is not, open up DMEM to make sure we dont break
    // the existing SW.. TODO: Remove once RANGE0 becomes
    // programmable..
    //
    if (rdata != data)
    {
        acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK,
                                   ACR_PLMASK_READ_L0_WRITE_L0);
    }
}

/*!
 * @brief Programs target registers to ensure the falcon goes into LS mode
 */
ACR_STATUS
acrSetupTargetRegisters_GM20X
(
    ACR_FLCN_CONFIG    *pFlcnCfg
)
{
    ACR_STATUS status = ACR_OK;
    LwU32      data   = 0;
    LwU32      i      = 0;

    REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(1), 0x11);
    //
    // For few LS falcons ucodes, we use bootloader to load actual ucode,
    // so we need to restrict them to be loaded from WPR only
    //
    if (acrCheckIfFalconIsBootstrappedWithLoader_HAL(pAcr, pFlcnCfg->falconId))
    {
        for (i = 0; i < LW_PFALCON_FBIF_TRANSCFG__SIZE_1; i++)
        {
            acrProgramRegionCfg_HAL(&Acr, pFlcnCfg, LW_FALSE, i, LSF_WPR_EXPECTED_REGION_ID);
        }
    }

    REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(1), 0x12);
    // Step-1: Set SCTL priv level mask to only HS accessible
    acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg, REG_LABEL_FLCN_SCTL_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);

    //
    // Step-2: Set
    //    SCTL_RESET_LVLM_EN to FALSE
    //    SCTL_STALLREQ_CLR_EN to FALSE
    //    SCTL_AUTH_EN to TRUE
    //

    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _RESET_LVLM_EN, _FALSE, 0);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _STALLREQ_CLR_EN, _FALSE, data);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _AUTH_EN, _TRUE, data);
    acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg, REG_LABEL_FLCN_SCTL, data);

    // Step-3: Set CPUCTL_ALIAS_EN to FALSE
    data = FLD_SET_DRF(_PFALCON, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, 0);
    acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg, REG_LABEL_FLCN_CPUCTL, data);

    //
    // Step-4: Setup
    //        IMEM_PRIV_LEVEL_MASK
    //        DMEM_PRIV_LEVEL_MASK
    //        CPUCTL_PRIV_LEVEL_MASK
    //        DEBUG_PRIV_LEVEL_MASK
    //        EXE_PRIV_LEVEL_MASK
    //        IRQTMR_PRIV_LEVEL_MASK
    //        MTHDCTX_PRIV_LEVEL_MASK
    //        FBIF_REGIONCFG_PRIV_LEVEL_MASK
    //
    // Use level 2 protection for writes to IMEM/DMEM. We do not need read protection and leaving IMEM/DMEM
    // open can aid debug if a problem happens before the PLM is overwritten by ACR binary or the PMU task to
    // the final value. Note that level 2 is the max we can use here because this code is run inside PMU at
    // level 2. In future when we move the acr task from PMU to SEC2 and make it HS, we can afford to use
    // the final values from pFlcnCfg->{imem, dmem}PLM directly over here.
    //
    acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg, REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);
    acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);

    acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg, REG_LABEL_FLCN_CPUCTL_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);
    acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg, REG_LABEL_FLCN_EXE_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);
    acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg, REG_LABEL_FLCN_IRQTMR_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L0);
    acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg, REG_LABEL_FLCN_MTHDCTX_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L0);

    if (pFlcnCfg->bFbifPresent)
    {
        acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg, REG_LABEL_FBIF_REGIONCFG_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);
    }
    else
    {
        //
        // FECS and GPCCS case, as they do not support FBIF
        // Changing this to R-M-W, as we have Pri-Source-Isolation from Turing
        // because of which we have more field in REGIONCFG PLM
        //
        LwU32 reg = ACR_REG_RD32(BAR0, pFlcnCfg->regCfgMaskAddr);
        reg = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, _READ_PROTECTION,         _ALL_LEVELS_ENABLED, reg);
        reg = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, _READ_VIOLATION,          _REPORT_ERROR,       reg);
        reg = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE,            reg);
        reg = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE,            reg);
        reg = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _ENABLE,             reg);
        reg = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, _WRITE_VIOLATION,         _REPORT_ERROR,       reg);
        ACR_REG_WR32(BAR0, pFlcnCfg->regCfgMaskAddr, reg);
    }

    // Setup target falcon PLMs (Lwrrently only PRIVSTATE_PLM, more to be added)
    status = acrSetupTargetFalconPlms_HAL(&Acr, pFlcnCfg);
    if (status != ACR_OK)
    {
        return status;
    }

    // ASB already resets SEC2 before setting up registers.
#ifndef ASB
    FLCN_TIMESTAMP   startTimeNs;
    LwS32            timeoutLeftNs;

    // Step-5: Check if the falcon is HALTED
    if (!acrIsFalconHalted_HAL(&Acr, pFlcnCfg))
    {
        // Force reset falcon only..
        acrResetFalcon_HAL(&Acr, pFlcnCfg, LW_TRUE);
    }

    // Wait for it to reach HALT state
    acrGetLwrrentTimeNs_HAL(&Acr, &startTimeNs);
    while (!acrIsFalconHalted_HAL(&Acr, pFlcnCfg))
    {
        status = acrCheckTimeout_HAL(&Acr, ACR_DEFAULT_TIMEOUT_NS, startTimeNs, &timeoutLeftNs);
        if (status != ACR_OK)
        {
            return status;
        }
    }

    // Wait for polling to complete
    status = acrPollForScrubbing_HAL(&Acr, pFlcnCfg);
    if (status != ACR_OK)
    {
        return status;
    }
#endif

    //
    // Step-6: Authorize falcon
    // TODO: Let the individual falcons ucodes take care of setting up TRANSCFG/REGIONCFG
    //


    // Program LSMODE here
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _RESET_LVLM_EN, _TRUE, 0);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _STALLREQ_CLR_EN, _TRUE, data);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _AUTH_EN, _TRUE, data);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _LSMODE, _TRUE, data);
    data = FLD_SET_DRF_NUM(_PFALCON, _FALCON_SCTL, _LSMODE_LEVEL, 0x2, data);
    acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg, REG_LABEL_FLCN_SCTL, data);

    // Check if AUTH_EN is still TRUE
    data = acrFlcnRegLabelRead_HAL(&Acr, pFlcnCfg, REG_LABEL_FLCN_SCTL);
    if (!FLD_TEST_DRF(_PFALCON, _FALCON_SCTL, _AUTH_EN, _TRUE, data))
    {
        return ACR_ERROR_LS_BOOT_FAIL_AUTH;
    }

    if (pFlcnCfg->bOpenCarve)
    {
        _acrProgramDmemRange(pFlcnCfg);
    }

    // Step-7: Set CPUCTL_ALIAS_EN to TRUE
    data = FLD_SET_DRF(_PFALCON, _FALCON_CPUCTL, _ALIAS_EN, _TRUE, 0);
    acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg, REG_LABEL_FLCN_CPUCTL, data);

    // Step-8: Enable ICD
    acrEnableICD_HAL(&Acr, pFlcnCfg);

    // Step-9: Initialize STACKCFG to 0
    //
    // STACKCFG feature is added in GP10X. STACKCFG_BOTTOM acts as water level
    // of SP. Initialise it 0. Boot-primary will set proper value.
    //
    acrInitializeStackCfg_HAL(&Acr, pFlcnCfg, 0);

#ifndef ACR_BSI_BOOT_PATH
    // There are some SEC2-specific registers to be initialized
    if (pFlcnCfg->falconId == LSF_FALCON_ID_SEC2)
    {
        acrSetupSec2Registers_HAL(&Acr, pFlcnCfg);
    }
#endif

    return status;
}

/*!
 * @brief Find the IMEM block from the end to load BL code
 */
LwU32
acrFindFarthestImemBl_GM20X
(
    ACR_FLCN_CONFIG   *pFlcnCfg,
    LwU32              codeSizeInBytes
)
{
    LwU32 hwcfg    = acrFlcnRegLabelRead_HAL(&Acr, pFlcnCfg, REG_LABEL_FLCN_HWCFG);
    LwU32 imemSize = DRF_VAL(_PFALCON_FALCON, _HWCFG, _IMEM_SIZE, hwcfg);
    LwU32 codeBlks = DIV_ROUND_UP(codeSizeInBytes, IMEM_BLOCK_SIZE);

    return (imemSize - codeBlks);
}

/*!
 * @brief Programs the bootvector
 */
void
acrSetupBootvec_GM20X
(
    ACR_FLCN_CONFIG *pFlcnCfg,
    LwU32 bootvec
)
{
    acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg,
          REG_LABEL_FLCN_BOOTVEC, bootvec);
}

/*!
 * @brief Issue target falcon DMA. Supports only FB -> Flcn and not the other way.
 *        Supports DMAing into both IMEM and DMEM of target falcon from FB.
 *        Always uses physical addressing for memory transfer. Expects size to be
 *        256 multiple.
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
 */
ACR_STATUS
acrIssueTargetFalconDma_GM20X
(
    LwU32            dstOff,
    LwU64            fbBase,
    LwU32            fbOff,
    LwU32            sizeInBytes,
    LwU32            regionID,
    LwU8             bIsSync,
    LwU8             bIsDstImem,
    ACR_FLCN_CONFIG *pFlcnCfg
)
{
    ACR_STATUS     status        = ACR_OK;
    LwU32          data          = 0;
    LwU32          dmaCmd        = 0;
    LwU32          bytesXfered   = 0;
    LwS32          timeoutLeftNs = 0;
    FLCN_TIMESTAMP startTimeNs;

    //
    // Sanity checks
    // Only does 256B DMAs
    //
    if ((!LW_IS_ALIGNED(sizeInBytes, IMEM_BLOCK_SIZE)) ||
        (!LW_IS_ALIGNED(dstOff, IMEM_BLOCK_SIZE))      ||
        (!LW_IS_ALIGNED(fbOff, IMEM_BLOCK_SIZE)))
    {
        return ACR_ERROR_TGT_DMA_FAILURE;
    }

    //
    // Program Transcfg to point to physical mode
    //
    acrSetupCtxDma_HAL(&Acr, pFlcnCfg, pFlcnCfg->ctxDma, LW_TRUE);

    if (pFlcnCfg->bFbifPresent)
    {
        //
        // Disable CTX requirement for falcon DMA engine
        //
        data = acrFlcnRegRead_HAL(&Acr, pFlcnCfg, BAR0_FBIF, LW_PFALCON_FBIF_CTL);
        data = FLD_SET_DRF(_PFALCON, _FBIF_CTL, _ALLOW_PHYS_NO_CTX, _ALLOW, data);
        acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, BAR0_FBIF, LW_PFALCON_FBIF_CTL, data);
    }

    // Set REQUIRE_CTX to false
    acrSetupDmaCtl_HAL(&Acr, pFlcnCfg, LW_FALSE);

    //Program REGCONFIG
    acrProgramRegionCfg_HAL(&Acr, pFlcnCfg, LW_FALSE, pFlcnCfg->ctxDma, regionID);

    //
    // Program DMA registers
    // Write DMA base address
    //
    acrProgramDmaBase_HAL(&Acr, pFlcnCfg, fbBase);

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
        acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFMOFFS, data);

        data = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFFBOFFS, _OFFS, (fbOff + bytesXfered), 0);
        acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFFBOFFS, data);

        // Write the command
        acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFCMD, dmaCmd);

        //
        // Poll for completion
        // TODO: Make use of bIsSync
        //
        acrGetLwrrentTimeNs_HAL(&Acr, &startTimeNs);
        data = acrFlcnRegRead_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFCMD);
        while (FLD_TEST_DRF(_PFALCON_FALCON, _DMATRFCMD, _IDLE, _FALSE, data))
        {
            status = acrCheckTimeout_HAL(&Acr, ACR_DEFAULT_TIMEOUT_NS, startTimeNs, &timeoutLeftNs);
            if (status != ACR_OK)
            {
                return status;
            }

            data = acrFlcnRegRead_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFCMD);
        }

        bytesXfered += IMEM_BLOCK_SIZE;
    }

    return status;
}

/*!
 * @brief Program FBIF to allow physical transactions.
 *        Incase of GR falcons, make appropriate writes
 */
ACR_STATUS
acrSetupCtxDma_GM20X
(
    ACR_FLCN_CONFIG *pFlcnCfg,
    LwU32            ctxDma,
    LwBool           bIsPhysical
)
{
    ACR_STATUS status = ACR_OK;
    LwU32      data   = 0;
    LwU32      addr   = LW_CSEC_FBIF_TRANSCFG(ctxDma);;

    if (pFlcnCfg->bFbifPresent)
    {

        if (!(pFlcnCfg->bIsBoOwner))
        {
            data = acrFlcnRegRead_HAL(&Acr, pFlcnCfg, BAR0_FBIF, LW_PFALCON_FBIF_TRANSCFG(ctxDma));
        }
        else
        {
            data =  acrFlcnRegRead_HAL(&Acr, 0, CSB_FLCN, addr);
        }

        data = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _TARGET, _LOCAL_FB, data);

        if (bIsPhysical)
        {
            data = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL, data);
        }
        else
        {
            data = FLD_SET_DRF(_PFALCON, _FBIF_TRANSCFG, _MEM_TYPE, _VIRTUAL, data);
        }

        if (!(pFlcnCfg->bIsBoOwner))
        {
            acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, BAR0_FBIF, LW_PFALCON_FBIF_TRANSCFG(ctxDma), data);
        }
        else
        {
            acrFlcnRegWrite_HAL(&Acr, 0, CSB_FLCN, addr, data);
        }
    }
    else
    {
        if (pFlcnCfg->falconId == LSF_FALCON_ID_FECS)
        {
            acrSetupFecsDmaThroughArbiter_HAL(&Acr, pFlcnCfg, bIsPhysical);
        }
    }

    return status;
}

/*!
 * @brief Setup DMACTL
 */
void
acrSetupDmaCtl_GM20X
(
    ACR_FLCN_CONFIG *pFlcnCfg,
    LwBool bIsCtxRequired
)
{
    if (bIsCtxRequired)
    {
        acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg,
          REG_LABEL_FLCN_DMACTL, 0x1);
    }
    else
    {
        acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg,
          REG_LABEL_FLCN_DMACTL, 0x0);
    }
}

/*!
 * @brief Acr util function to write falcon registers using register label
 *
 * @param[in]  pFlcnCfg  Falcon config
 * @param[in]  pIsFbif   TRUE if register is offset from FBIF base
 * @param[in]  regOff    Register offset
 */
void
acrFlcnRegLabelWrite_GM20X
(
    ACR_FLCN_CONFIG   *pFlcnCfg,
    ACR_FLCN_REG_LABEL reglabel,
    LwU32              data
)
{
    ACR_REG_MAPPING regMapping = {0};
    FLCN_REG_TGT     tgt        = ILWALID_TGT;

    acrFindRegMapping_HAL(&Acr, pFlcnCfg, reglabel, &regMapping, &tgt);

    switch (tgt)
    {
        case CSB_FLCN:
            return acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, tgt, regMapping.pwrCsbOff, data);
        case BAR0_FLCN:
        case BAR0_FBIF:
            return acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, tgt, regMapping.bar0Off, data);
        default:
            // Find better way to propogate error
            PMU_HALT();
    }
}

/*!
 * @brief Acr util function to read falcon registers using register label
 *
 * @param[in]  pFlcnCfg  Falcon config
 * @param[in]  pIsFbif   TRUE if register is offset from FBIF base
 * @param[in]  regOff    Register offset
 */
LwU32
acrFlcnRegLabelRead_GM20X
(
    ACR_FLCN_CONFIG   *pFlcnCfg,
    ACR_FLCN_REG_LABEL reglabel
)
{
    ACR_REG_MAPPING regMapping = {0};
    FLCN_REG_TGT    tgt        = ILWALID_TGT;

    acrFindRegMapping_HAL(&Acr, pFlcnCfg, reglabel, &regMapping, &tgt);
    switch (tgt)
    {
        case CSB_FLCN:
            return acrFlcnRegRead_HAL(&Acr, pFlcnCfg, tgt, regMapping.pwrCsbOff);
        case BAR0_FLCN:
        case BAR0_FBIF:
            return acrFlcnRegRead_HAL(&Acr, pFlcnCfg, tgt, regMapping.bar0Off);
        default:
            // Find better way to propogate error
            PMU_HALT();
    }

    // Control will not reach here since falcon halts in default case.
    return 0xffffffff;
}

/*!
 * @brief Acr util function to read falcon registers
 *
 * @param[in]  pFlcnCfg  Falcon config
 * @param[in]  pIsFbif   TRUE if register is offset from FBIF base
 * @param[in]  regOff    Register offset
 */
LwU32
acrFlcnRegRead_GM20X
(
    ACR_FLCN_CONFIG *pFlcnCfg,
    FLCN_REG_TGT     tgt,
    LwU32            regOff
)
{
    if (pFlcnCfg && (tgt == BAR0_FLCN))
    {
        return ACR_REG_RD32(BAR0, ((pFlcnCfg->registerBase) + regOff));
    }
    else if (tgt == CSB_FLCN)
    {
        return ACR_REG_RD32_STALL(CSB, regOff);
    }
    else if (pFlcnCfg && (tgt == BAR0_FBIF))
    {
        return ACR_REG_RD32(BAR0, ((pFlcnCfg->fbifBase) + regOff));
    }
    else
    {
        PMU_HALT();    // This halt needs to be revisited to propagate error to caller
    }
    return 0;
}

/*!
 * @brief Acr util function to write falcon registers
 *
 * @param[in]  pFlcnCfg  Falcon config
 * @param[in]  pIsFbif   TRUE if register is offset from FBIF base
 * @param[in]  regOff    Register offset
 */
void
acrFlcnRegWrite_GM20X
(
    ACR_FLCN_CONFIG *pFlcnCfg,
    FLCN_REG_TGT     tgt,
    LwU32            regOff,
    LwU32            data
)
{
    if (pFlcnCfg && (tgt == BAR0_FLCN))
    {
        ACR_REG_WR32(BAR0, ((pFlcnCfg->registerBase) + regOff), data);
    }
    else if (tgt == CSB_FLCN)
    {
        ACR_REG_WR32_STALL(CSB, regOff, data);
    }
    else if (pFlcnCfg && (tgt == BAR0_FBIF))
    {
        ACR_REG_WR32(BAR0, ((pFlcnCfg->fbifBase) + regOff), data);
    }
    else
    {
        PMU_HALT();
    }
}

/*!
 * @brief Programs the region CFG in FBFI register.
 */
ACR_STATUS
acrProgramRegionCfg_GM20X
(
    ACR_FLCN_CONFIG  *pFlcnCfg,
    LwBool            bUseCsb,
    LwU32             ctxDma,
    LwU32             regionID
)
{
    LwU32        data = 0;
    FLCN_REG_TGT tgt  = BAR0_FBIF;
    LwU32        addr = LW_PFALCON_FBIF_REGIONCFG;
    LwU32        mask = 0;

    mask   = ~(0xF << (ctxDma*4));

    if (pFlcnCfg && (!(pFlcnCfg->bFbifPresent)))
    {
        data = ACR_REG_RD32(BAR0, pFlcnCfg->regCfgAddr);
        data = (data & mask) | ((regionID & 0xF) << (ctxDma * 4));
        ACR_REG_WR32(BAR0, pFlcnCfg->regCfgAddr, data);
        data = ACR_REG_RD32(BAR0, pFlcnCfg->regCfgAddr);
    }
    else
    {
        if (bUseCsb)
        {
            tgt  = CSB_FLCN;
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_UNLOAD))
            addr = LW_CPWR_FBIF_REGIONCFG;
#elif defined(AHESASC) || defined(ASB) || defined(BSI_LOCK)
            addr = LW_CSEC_FBIF_REGIONCFG;
#else
            ct_assert(0);
#endif
        }

        data = acrFlcnRegRead_HAL(&Acr, pFlcnCfg, tgt, addr);
        data = (data & mask) | ((regionID & 0xF) << (ctxDma * 4));
        acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, tgt, addr, data);
    }

    return ACR_OK;
}

/*!
 * @brief Resets a given falcon
 */
ACR_STATUS
acrResetFalcon_GM20X
(
    ACR_FLCN_CONFIG    *pFlcnCfg,
    LwBool              bForceFlcnOnlyReset
)
{
    ACR_STATUS status = ACR_OK;

    status = acrPutFalconInReset_HAL(&Acr, pFlcnCfg, bForceFlcnOnlyReset);
    if (status != ACR_OK)
    {
        return status;
    }

    status = acrBringFalconOutOfReset_HAL(&Acr, pFlcnCfg);
    if (status != ACR_OK)
    {
        return status;
    }

    return status;
}

/*!
 * @brief Poll for IMEM and DMEM scrubbing to complete
 *
 * @param[in] pFlcnCfg Falcon Config
 */
ACR_STATUS
acrPollForScrubbing_GM20X
(
    ACR_FLCN_CONFIG  *pFlcnCfg
)
{
    ACR_STATUS     status          = ACR_OK;
    LwU32          data            = 0;
    FLCN_TIMESTAMP startTimeNs;
    LwS32          timeoutLeftNs;


    // Poll for SRESET to complete
    acrGetLwrrentTimeNs_HAL(&Acr, &startTimeNs);
    data = acrFlcnRegRead_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMACTL);
    while ((FLD_TEST_DRF(_PFALCON_FALCON, _DMACTL, _DMEM_SCRUBBING, _PENDING, data)) ||
           (FLD_TEST_DRF(_PFALCON_FALCON, _DMACTL, _IMEM_SCRUBBING, _PENDING, data)))
    {
        status  = acrCheckTimeout_HAL(&Acr, (LwU32)ACR_DEFAULT_TIMEOUT_NS, startTimeNs, &timeoutLeftNs);
        if (status != ACR_OK)
        {
            return status;
        }

        data = acrFlcnRegRead_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMACTL);
    }

    return status;
}

/*!
 * @brief: initializes ACR Register map
 */
static void
_acrInitRegMapping_GM20X(ACR_REG_MAPPING acrRegMap[REG_LABEL_END])
{
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_UNLOAD))
    acrRegMap[REG_LABEL_FLCN_SCTL_PRIV_LEVEL_MASK]       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_SCTL_PRIV_LEVEL_MASK,           LW_CPWR_FALCON_SCTL_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_SCTL]                       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_SCTL,                           LW_CPWR_FALCON_SCTL};
    acrRegMap[REG_LABEL_FLCN_CPUCTL]                     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_CPUCTL,                         LW_CPWR_FALCON_CPUCTL};
    acrRegMap[REG_LABEL_FLCN_DMACTL]                     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMACTL,                         LW_CPWR_FALCON_DMACTL};
    acrRegMap[REG_LABEL_FLCN_DMATRFBASE]                 = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFBASE,                     LW_CPWR_FALCON_DMATRFBASE};
    acrRegMap[REG_LABEL_FLCN_DMATRFCMD]                  = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFCMD,                      LW_CPWR_FALCON_DMATRFCMD};
    acrRegMap[REG_LABEL_FLCN_DMATRFMOFFS]                = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFMOFFS,                    LW_CPWR_FALCON_DMATRFMOFFS};
    acrRegMap[REG_LABEL_FLCN_DMATRFFBOFFS]               = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFFBOFFS,                   LW_CPWR_FALCON_DMATRFFBOFFS};
    acrRegMap[REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK]       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_IMEM_PRIV_LEVEL_MASK,           LW_CPWR_FALCON_IMEM_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK]       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMEM_PRIV_LEVEL_MASK,           LW_CPWR_FALCON_DMEM_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_CPUCTL_PRIV_LEVEL_MASK]     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_CPUCTL_PRIV_LEVEL_MASK,         LW_CPWR_FALCON_CPUCTL_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_EXE_PRIV_LEVEL_MASK]        = (ACR_REG_MAPPING){LW_PFALCON_FALCON_EXE_PRIV_LEVEL_MASK,            LW_CPWR_FALCON_EXE_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_IRQTMR_PRIV_LEVEL_MASK]     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_IRQTMR_PRIV_LEVEL_MASK,         LW_CPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_MTHDCTX_PRIV_LEVEL_MASK]    = (ACR_REG_MAPPING){LW_PFALCON_FALCON_MTHDCTX_PRIV_LEVEL_MASK,        LW_CPWR_FALCON_MTHDCTX_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_HWCFG]                      = (ACR_REG_MAPPING){LW_PFALCON_FALCON_HWCFG,                          LW_CPWR_FALCON_HWCFG};
    acrRegMap[REG_LABEL_FLCN_BOOTVEC]                    = (ACR_REG_MAPPING){LW_PFALCON_FALCON_BOOTVEC,                        LW_CPWR_FALCON_BOOTVEC};
    acrRegMap[REG_LABEL_FLCN_DBGCTL]                     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DBGCTL,                         LW_CPWR_FALCON_DBGCTL};
    acrRegMap[REG_LABEL_FLCN_SCPCTL]                     = (ACR_REG_MAPPING){0xFFFFFFFF,                                       LW_CPWR_PMU_SCP_CTL_STAT};
    acrRegMap[REG_LABEL_FBIF_REGIONCFG_PRIV_LEVEL_MASK]  = (ACR_REG_MAPPING){LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK,        LW_CPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FBIF_REGIONCFG]                  = (ACR_REG_MAPPING){LW_PFALCON_FBIF_REGIONCFG,                        LW_CPWR_FBIF_REGIONCFG};
    acrRegMap[REG_LABEL_FBIF_CTL]                        = (ACR_REG_MAPPING){LW_PFALCON_FBIF_CTL,                              LW_CPWR_FBIF_REGIONCFG};

#elif defined(AHESASC) || defined(ASB) || defined(BSI_LOCK)
   // Initialize all mappings
    acrRegMap[REG_LABEL_FLCN_SCTL_PRIV_LEVEL_MASK]       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_SCTL_PRIV_LEVEL_MASK,           LW_CSEC_FALCON_SCTL_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_SCTL]                       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_SCTL,                           LW_CSEC_FALCON_SCTL};
    acrRegMap[REG_LABEL_FLCN_CPUCTL]                     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_CPUCTL,                         LW_CSEC_FALCON_CPUCTL};
    acrRegMap[REG_LABEL_FLCN_DMACTL]                     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMACTL,                         LW_CSEC_FALCON_DMACTL};
    acrRegMap[REG_LABEL_FLCN_DMATRFBASE]                 = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFBASE,                     LW_CSEC_FALCON_DMATRFBASE};
    acrRegMap[REG_LABEL_FLCN_DMATRFCMD]                  = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFCMD,                      LW_CSEC_FALCON_DMATRFCMD};
    acrRegMap[REG_LABEL_FLCN_DMATRFMOFFS]                = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFMOFFS,                    LW_CSEC_FALCON_DMATRFMOFFS};
    acrRegMap[REG_LABEL_FLCN_DMATRFFBOFFS]               = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFFBOFFS,                   LW_CSEC_FALCON_DMATRFFBOFFS};
    acrRegMap[REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK]       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_IMEM_PRIV_LEVEL_MASK,           LW_CSEC_FALCON_IMEM_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK]       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMEM_PRIV_LEVEL_MASK,           LW_CSEC_FALCON_DMEM_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_CPUCTL_PRIV_LEVEL_MASK]     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_CPUCTL_PRIV_LEVEL_MASK,         LW_CSEC_FALCON_CPUCTL_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_EXE_PRIV_LEVEL_MASK]        = (ACR_REG_MAPPING){LW_PFALCON_FALCON_EXE_PRIV_LEVEL_MASK,            LW_CSEC_FALCON_EXE_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_IRQTMR_PRIV_LEVEL_MASK]     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_IRQTMR_PRIV_LEVEL_MASK,         LW_CSEC_FALCON_IRQTMR_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_MTHDCTX_PRIV_LEVEL_MASK]    = (ACR_REG_MAPPING){LW_PFALCON_FALCON_MTHDCTX_PRIV_LEVEL_MASK,        LW_CSEC_FALCON_MTHDCTX_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_HWCFG]                      = (ACR_REG_MAPPING){LW_PFALCON_FALCON_HWCFG,                          LW_CSEC_FALCON_HWCFG};
    acrRegMap[REG_LABEL_FLCN_BOOTVEC]                    = (ACR_REG_MAPPING){LW_PFALCON_FALCON_BOOTVEC,                        LW_CSEC_FALCON_BOOTVEC};
    acrRegMap[REG_LABEL_FLCN_DBGCTL]                     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DBGCTL,                         LW_CSEC_FALCON_DBGCTL};
    acrRegMap[REG_LABEL_FLCN_SCPCTL]                     = (ACR_REG_MAPPING){0xFFFFFFFF,                                       LW_CSEC_SCP_CTL_STAT};
    acrRegMap[REG_LABEL_FBIF_REGIONCFG_PRIV_LEVEL_MASK]  = (ACR_REG_MAPPING){LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK,        LW_CSEC_FBIF_REGIONCFG_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FBIF_REGIONCFG]                  = (ACR_REG_MAPPING){LW_PFALCON_FBIF_REGIONCFG,                        LW_CSEC_FBIF_REGIONCFG};
    acrRegMap[REG_LABEL_FBIF_CTL]                        = (ACR_REG_MAPPING){LW_PFALCON_FBIF_CTL,                              LW_CSEC_FBIF_REGIONCFG};
#else
    // Control is not expected to come here. If you hit this assert, please check build profile config
    ct_assert(0);
#endif
}

/*!
 * @brief Check falcons for which we use bootloader to load actual LS ucode
 *        We need to restrict such LS falcons to be loaded only from WPR, Bug 1969345
 */
LwBool
acrCheckIfFalconIsBootstrappedWithLoader_GM20X
(
    LwU32 falconId
)
{
    // On GM20X and GP100, only PMU and DPU are loaded via bootloader
    if ((falconId == LSF_FALCON_ID_PMU) || (falconId == LSF_FALCON_ID_DPU))
    {
        return LW_TRUE;
    }
    return LW_FALSE;
}

/*!
 * @brief Enable ICD
 */
void
acrEnableICD_GM20X
(
    ACR_FLCN_CONFIG    *pFlcnCfg
)
{
    // Enable all of ICD
    acrFlcnRegLabelWrite_HAL(&Acr, pFlcnCfg, REG_LABEL_FLCN_DBGCTL, 0x1ff);
    return;
}

/*!
 * @brief Find the total number of DMEM blocks
 */
LwU32
acrFindTotalDmemBlocks_GM20X
(
    ACR_FLCN_CONFIG    *pFlcnCfg
)
{
    LwU32 hwcfg    = acrFlcnRegLabelRead_HAL(&Acr, pFlcnCfg, REG_LABEL_FLCN_HWCFG);
    return DRF_VAL(_PFALCON_FALCON, _HWCFG, _DMEM_SIZE, hwcfg);
}

/*!
 * Wrapper function for blocking falcon read instruction to halt in case of Priv Error.
 */
LwU32
falc_ldxb_i_with_halt
(
    LwU32 addr
)
{
    LwU32   val           = 0;
    LwU32   csbErrStatVal = 0;

    val = falc_ldxb_i ((LwU32*)(addr), 0);

#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_UNLOAD))
    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CPWR_FALCON_CSBERRSTAT), 0);
#elif defined(AHESASC) || defined(ASB) || defined(BSI_LOCK)
    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CSEC_FALCON_CSBERRSTAT), 0);
#else
    ct_assert(0);
#endif

    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        // If CSBERRSTAT is true, halt no matter what register was read.
        PMU_HALT();
    }
    else if ((val & CSB_INTERFACE_MASK_VALUE) == CSB_INTERFACE_BAD_VALUE)
    {
        //
        // If we get here, we thought we read a bad value. However, there are
        // certain cases where the values may lead us to think we read something
        // bad, but in fact, they were just valid values that the register could
        // report. For example, the host ptimer PTIMER1 and PTIMER0 registers
        // can return 0xbadfxxxx value that we interpret to be "bad".
        //

#ifndef BSI_LOCK // Exclude due to BSI space constraints

#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_UNLOAD))
        if (addr == LW_CPWR_FALCON_PTIMER1)
#elif defined(AHESASC) || defined(ASB) || defined(BSI_LOCK)
        if (addr == LW_CSEC_FALCON_PTIMER1)
#else
        ct_assert(0);
#endif
        {
            //
            // PTIMER1 can report a constant 0xbadfxxxx for a long time, so
            // just make an exception for it.
            //
            return val;
        }
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_UNLOAD))
        else if (addr == LW_CPWR_FALCON_PTIMER0)
#elif defined(AHESASC) || defined(ASB) || defined(BSI_LOCK)
        else if (addr == LW_CSEC_FALCON_PTIMER0)
#else
        ct_assert(0);
#endif
        {
            //
            // PTIMER0 can report a 0xbadfxxxx value for up to 2^16 ns, but it
            // should keep changing its value every nanosecond. We just check
            // to make sure we can detect its value changing by reading it a
            // few times.
            //
            LwU32   val1      = 0;
            LwU32   i         = 0;

            for (i = 0; i < 3; i++)
            {
                val1 = falc_ldx_i ((LwU32 *)(addr), 0);
                if (val1 != val)
                {
                    // Timer0 is progressing, so return latest value, i.e. val1.
                    return val1;
                }
            }
        }
#endif // #ifndef BSI_LOCK

        //
        // While reading SE_TRNG, the random number generator can return a badfxxxx value
        // which is actually a random number provided by TRNG unit. Since the output of this
        // SE_TRNG registers comes in FALCON_DIC_D0, we need to skip PEH for this FALCON_DIC_D0
        // This in turn reduces our PEH coverage for all SE registers since all SE registers are read from
        // FALCON_DIC_D0 register.
        // Skipping 0xBADFxxxx check here on FALCON_DIC_D0 is fine, this check is being done in the
        // upper level parent function acrSelwreBusGetData_HAL and explicitly discounting only the
        // SE TRNG registers from 0xBADFxxxx check.
        // More details in Bug 200326572
        // Also, the ifdef on LW_CPWR_FALCON_DIC_D0 and LW_CSEC_FALCON_DIC_D0 is required since
        // these registers are not defined for GM20X (this file is also built for GM20X).
        // We should make this function a HAL and to fork separate and appropriate implementation for GM20X/GP10X/GV100+
        //
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_UNLOAD))
#ifdef LW_CPWR_FALCON_DIC_D0
        if (addr == LW_CPWR_FALCON_DIC_D0)
        {
            return val;
        }
#endif
#elif defined(AHESASC) || defined(ASB) || defined(BSI_LOCK)
#ifdef LW_CSEC_FALCON_DIC_D0
        if (addr == LW_CSEC_FALCON_DIC_D0)
        {
            return val;
        }
#endif
#else
        ct_assert(0);
#endif

        PMU_HALT();
    }

    return val;
}

/*!
 * Wrapper function for blocking falcon write instruction to halt in case of Priv Error.
 */
void
falc_stxb_i_with_halt
(
    LwU32 addr,
    LwU32 data
)
{
    LwU32 csbErrStatVal = 0;

    falc_stxb_i ((LwU32*)(addr), 0, data);

#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_UNLOAD))
    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CPWR_FALCON_CSBERRSTAT), 0);
#elif defined(AHESASC) || defined(ASB) || defined(BSI_LOCK)
    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CSEC_FALCON_CSBERRSTAT), 0);
#else
    ct_assert(0);
#endif

    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        PMU_HALT();
    }
}

#if PMUCFG_FEATURE_ENABLED(PMU_ACR_DMEM_APERT_ENABLED)
/*!
 * Function to read register value through External Interface when DMEM Apertue is enabled
 * and halt in case of Priv Error.
 */
LwU32
acrBar0RegReadDmemApert_GM20X
(
    LwU32 addr
)
{
    LwU32 value = 0;
    LwU32 extErrStatVal = 0;

    value = (*((const volatile LwU32 *)((addr) | DMEM_APERTURE_OFFSET )));

    // Doing CSB read to get the Status, since BAR0 read has higher chances of failure.
    extErrStatVal = falc_ldxb_i_with_halt(LW_CPWR_FALCON_EXTERRSTAT);

    if (!FLD_TEST_DRF(_CPWR, _FALCON_EXTERRSTAT, _STAT, _ACK_POS, extErrStatVal) ||
        ((value & EXT_INTERFACE_MASK_VALUE) == EXT_INTERFACE_BAD_VALUE))
    {
        PMU_HALT();
    }

    return value;
}

/*!
 * Funtion to write register value through External Interface when DMEM Apertue is enabled
 * and halt in case of Priv Error.
 */
void
acrBar0RegWriteDmemApert_GM20X
(
    LwU32 addr,
    LwU32 data
)
{
    LwU32 extErrStatVal = 0;

    *((volatile LwU32 *) ((LwU32) (addr) | DMEM_APERTURE_OFFSET )) = (data);

    // Doing CSB read to get the Status, since BAR0 read has higher chances of failure.
    extErrStatVal = falc_ldxb_i_with_halt(LW_CPWR_FALCON_EXTERRSTAT);

    if (!FLD_TEST_DRF(_CPWR, _FALCON_EXTERRSTAT, _STAT, _ACK_POS, extErrStatVal))
    {
        PMU_HALT();
    }
}
#else
/*!
 * @brief Wait for BAR0 to become idle
 */
void
acrBar0WaitIdle_GM20X(void)
{
    LwBool bDone = LW_FALSE;

    // As this is an infinite loop, timeout should be considered on using bewlo loop elsewhere.
    do
    {
        switch (DRF_VAL(_CSEC, _BAR0_CSR, _STATUS,
                    ACR_REG_RD32(CSB, LW_CSEC_BAR0_CSR)))
        {
            case LW_CSEC_BAR0_CSR_STATUS_IDLE:
                bDone = LW_TRUE;
                break;

            case LW_CSEC_BAR0_CSR_STATUS_BUSY:
                break;

            // unexpected error conditions
            case LW_CSEC_BAR0_CSR_STATUS_ERR:
            case LW_CSEC_BAR0_CSR_STATUS_TMOUT:
            case LW_CSEC_BAR0_CSR_STATUS_DIS:
            default:
                PMU_HALT();
        }
    }
    while (!bDone);

}

LwU32
acrBar0RegRead_GM20X
(
    LwU32 addr
)
{
    LwU32  val;

    ACR_REG_WR32(CSB, LW_CSEC_BAR0_TMOUT,
        DRF_DEF(_CSEC, _BAR0_TMOUT, _CYCLES, _PROD));
    acrBar0WaitIdle_HAL(&Acr);
    ACR_REG_WR32(CSB, LW_CSEC_BAR0_ADDR, addr);

    ACR_REG_WR32_STALL(CSB, LW_CSEC_BAR0_CSR,
        DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _READ) |
        DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF  ) |
        DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE));

    (void)ACR_REG_RD32(CSB, LW_CSEC_BAR0_CSR);

    acrBar0WaitIdle_HAL(&Acr);
    val = ACR_REG_RD32(CSB, LW_CSEC_BAR0_DATA);
    return val;
}

void
acrBar0RegWrite_GM20X
(
    LwU32 addr,
    LwU32 data
)
{
    ACR_REG_WR32(CSB, LW_CSEC_BAR0_TMOUT,
        DRF_DEF(_CSEC, _BAR0_TMOUT, _CYCLES, _PROD));
    acrBar0WaitIdle_HAL(&Acr);
    ACR_REG_WR32(CSB, LW_CSEC_BAR0_ADDR, addr);
    ACR_REG_WR32(CSB, LW_CSEC_BAR0_DATA, data);

    ACR_REG_WR32(CSB, LW_CSEC_BAR0_CSR,
        DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _WRITE) |
        DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF   ) |
        DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE ));

    (void)ACR_REG_RD32(CSB, LW_CSEC_BAR0_CSR);
    acrBar0WaitIdle_HAL(&Acr);

}
#endif

/*!
 * @brief Check if falcon is halted or not
 */
LwBool
acrIsFalconHalted_GM20X
(
    ACR_FLCN_CONFIG    *pFlcnCfg
)
{
    LwU32 data = 0;
    data = acrFlcnRegRead_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_CPUCTL);
    return FLD_TEST_DRF(_PFALCON_FALCON, _CPUCTL, _HALTED, _TRUE, data);
}

/*!
 * @brief Get current time in NS
 *
 * @param[out] pTime TIMESTAMP struct to hold the current time
 */
void
acrGetLwrrentTimeNs_GM20X
(
    FLCN_TIMESTAMP *pTime
)
{
    LwU32 hi;
    LwU32 lo;

    // Read loop specified by 'dev_timer.ref'.
    do
    {
        hi = ACR_REG_RD32(CSB, LW_CMSDEC_FALCON_PTIMER1);
        lo = ACR_REG_RD32(CSB, LW_CMSDEC_FALCON_PTIMER0);
    }
    while (hi != ACR_REG_RD32(CSB, LW_CMSDEC_FALCON_PTIMER1));

    // Stick the values into the timestamp.
    pTime->parts.lo = lo;
    pTime->parts.hi = hi;
}

/*!
 * @brief Check if given timeout has elapsed or not.
 *
 * @param[in] timeoutNs      Timeout in nano seconds
 * @param[in] startTimeNs    Start time for this timeout cycle
 * @param[in] pTimeoutLeftNs Remaining time in nanoseconds
 *
 * @return ACR_ERROR_TIMEOUT if timed out
 */
ACR_STATUS
acrCheckTimeout_GM20X
(
    LwU32          timeoutNs,
    FLCN_TIMESTAMP startTimeNs,
    LwS32         *pTimeoutLeftNs
)
{
    *pTimeoutLeftNs = timeoutNs - acrGetElapsedTimeNs_HAL(&Acr, &startTimeNs);
    return (*pTimeoutLeftNs <= 0)?ACR_ERROR_TIMEOUT:ACR_OK;
}

/*!
 * @brief Program DMA base register
 *
 * @param[in] pFlcnCfg   Structure to hold falcon config
 * @param[in] fbBase     Base address of fb region to be copied
 */
void
acrProgramDmaBase_GM20X
(
    ACR_FLCN_CONFIG    *pFlcnCfg,
    LwU64               fbBase
)
{
    acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFBASE, (LwU32)LwU64_LO32(fbBase));
}

/*!
 * @brief Program ARB to allow physical transactions for FECS as it doesn't have FBIF.
 * Not required for GPCCS as it is priv loaded.
 */
ACR_STATUS
acrSetupFecsDmaThroughArbiter_GM20X
(
    ACR_FLCN_CONFIG *pFlcnCfg,
    LwBool           bIsPhysical
)
{
    LwU32 data;

    if (bIsPhysical)
    {
        data = ACR_REG_RD32(BAR0, LW_PGRAPH_PRI_FECS_ARB_WPR);
        data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_WPR, _CMD_OVERRIDE_PHYSICAL_WRITES, _ALLOWED, data);
        ACR_REG_WR32(BAR0, LW_PGRAPH_PRI_FECS_ARB_WPR, data);
    }

    data = ACR_REG_RD32(BAR0, LW_PGRAPH_PRI_FECS_ARB_CMD_OVERRIDE);
    data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _CMD, _PHYS_VID_MEM, data);

    if (bIsPhysical)
    {
        data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _ENABLE, _ON, data);
    }
    else
    {
        data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _ENABLE, _OFF, data);
    }

    ACR_REG_WR32(BAR0, LW_PGRAPH_PRI_FECS_ARB_CMD_OVERRIDE, data);

    return ACR_OK;
}

/*!
 * @brief Reset only the falcon part
 */
ACR_STATUS
acrFalconOnlyReset_GM20X
(
    ACR_FLCN_CONFIG  *pFlcnCfg
)
{
    ACR_STATUS     status = ACR_OK;
    LwU32          sftR   = 0;
    LwU32          data   = 0;
    FLCN_TIMESTAMP startTimeNs;
    LwS32          timeoutLeftNs;

    if (pFlcnCfg->bFbifPresent)
    {
        // TODO: Revisit this once FECS halt bug has been fixed

        // Wait for STALLREQ
        sftR = FLD_SET_DRF(_PFALCON, _FALCON_ENGCTL, _SET_STALLREQ, _TRUE, 0);
        acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_ENGCTL, sftR);

        // Poll for FHSTATE.EXT_HALTED to complete
        acrGetLwrrentTimeNs_HAL(&Acr, &startTimeNs);
        data = acrFlcnRegRead_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_FHSTATE);
        while (FLD_TEST_DRF_NUM(_PFALCON, _FALCON_FHSTATE, _EXT_HALTED, 0, data))
        {
            status = acrCheckTimeout_HAL(&Acr, ACR_DEFAULT_TIMEOUT_NS, startTimeNs, &timeoutLeftNs);
            if (status != ACR_OK)
            {
                return status;
            }

            data = acrFlcnRegRead_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_FHSTATE);
        }

        // Do SFTRESET
        sftR = FLD_SET_DRF(_PFALCON, _FALCON_SFTRESET, _EXT, _TRUE, 0);
        acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_SFTRESET, sftR);

        // Poll for SFTRESET to complete
        acrGetLwrrentTimeNs_HAL(&Acr, &startTimeNs);
        data = acrFlcnRegRead_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_SFTRESET);
        while (FLD_TEST_DRF(_PFALCON, _FALCON_SFTRESET, _EXT, _TRUE, data))
        {
            status = acrCheckTimeout_HAL(&Acr, ACR_DEFAULT_TIMEOUT_NS, startTimeNs, &timeoutLeftNs);
            if (status != ACR_OK)
            {
                return status;
            }

            data = acrFlcnRegRead_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_SFTRESET);
        }
    }

    // Do falcon reset
    sftR = FLD_SET_DRF(_PFALCON, _FALCON_CPUCTL, _SRESET, _TRUE, 0);
    acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_CPUCTL, sftR);

    // Poll for SRESET to complete
    acrGetLwrrentTimeNs_HAL(&Acr, &startTimeNs);
    data = acrFlcnRegRead_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_CPUCTL);
    while (FLD_TEST_DRF(_PFALCON_FALCON, _CPUCTL, _SRESET, _TRUE, data))
    {
        status = acrCheckTimeout_HAL(&Acr, ACR_DEFAULT_TIMEOUT_NS, startTimeNs, &timeoutLeftNs);
        if (status != ACR_OK)
        {
            return status;
        }

        data = acrFlcnRegRead_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_CPUCTL);
    }

    return ACR_OK;
}

/*!
 * @brief Measure the time in nanoseconds elapsed since 'pTime'.
 *
 * Since the time is measured in nanoseconds and returned as an LwU32, this
 * function will only measure time-spans less than 4.29 seconds (2^32-1
 * nanoseconds).
 *
 * @param[in]   pTime   Timestamp to measure from.
 *
 * @return Number of nanoseconds elapsed since 'pTime'.
 *
 */
LwU32
acrGetElapsedTimeNs_GM20X
(
    const FLCN_TIMESTAMP *pTime
)
{
    //
    // Since time-spans are less than 4.29 secs, only the lower 32-bits of the
    // timestamps need to be compared. Subtracting these values will yield
    // the correct result. We also only need to read the PTIMER0 and avoid the
    // read loop ilwolved with reading both PTIMER0 and PTIMER1.
    //
    return ACR_REG_RD32(CSB, LW_CMSDEC_FALCON_PTIMER0) - pTime->parts.lo;
}

#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_LWDEC_PRESENT))
/*!
 * @brief Get the Lwenc2 falcon specifications such as registerBase, pmcEnableMask etc
 *
 * @param[out] pFlcnCfg   Structure to hold falcon config
 */
void
acrGetLwdecConfig_GM20X
(
    ACR_FLCN_CONFIG     *pFlcnCfg
)
{
    pFlcnCfg->pmcEnableMask = DRF_DEF(_PMC, _ENABLE, _LWDEC, _ENABLED);
    pFlcnCfg->registerBase  = LW_FALCON_LWDEC_BASE;
    pFlcnCfg->fbifBase      = LW_PLWDEC_FBIF_TRANSCFG(0);
    pFlcnCfg->range0Addr    = 0;
    pFlcnCfg->range1Addr    = 0;
    pFlcnCfg->bOpenCarve    = LW_FALSE;
    pFlcnCfg->bFbifPresent  = LW_TRUE;
    pFlcnCfg->ctxDma        = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
    pFlcnCfg->bStackCfgSupported = LW_TRUE;
}
#endif
#endif // !(PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE))

