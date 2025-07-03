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
 * @file   sec2ga10x.c
 * @brief  SEC2 HAL Functions for GA10X
 *
 * SEC2 HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "lwosselwreovly.h"
#include "sec2_cmdmgmt.h"
#include "dev_master.h"
#include "sec2_bar0.h"
#include "sec2_csb.h"
#include "dev_sec_csb.h"
#include "sec2_hs.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "dev_master.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_fbpa.h"
#include "dev_fbfalcon_pri.h"
#include "dev_therm.h"
#include "oemcommonlw.h"

#ifdef BOOT_FROM_HS_BUILD
#include "dev_sec_csb_addendum.h"
#endif // BOOT_FROM_HS_BUILD

#ifdef RUNTIME_HS_OVL_SIG_PATCHING
#include "rmlsfm_new_wpr_blob.h"
#include "mmu/mmucmn.h"
#include "dev_fb.h"
#endif // RUNTIME_HS_OVL_SIG_PATCHING

/* ------------------------- Application Includes --------------------------- */
#include "config/g_sec2_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
#ifdef RUNTIME_HS_OVL_SIG_PATCHING
extern LwU32 _imem_hs_signatures[][SIG_SIZE_IN_BITS / 32];
extern LwU32 _num_imem_hs_overlays[];
#endif // RUNTIME_HS_OVL_SIG_PATCHING
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */


/*
 * @brief Get the SW fuse version
 */
#define SEC2_GA10X_DEFAULT_UCODE_VERSION    (0x0)

#define SEC2_GA102_UCODE_VERSION            (0x1)

FLCN_STATUS
sec2GetSWFuseVersionHS_GA10X
(
    LwU32* pFuseVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    LwU32       chip;

    if (OS_SEC_FALC_IS_DBG_MODE())
    {
        *pFuseVersion = SEC2_GA10X_DEFAULT_UCODE_VERSION;
        return FLCN_OK;
    }
    else
    {
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &chip));

        chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);

        switch(chip)
        {
            case LW_PMC_BOOT_42_CHIP_ID_GA102:
            case LW_PMC_BOOT_42_CHIP_ID_GA103:
            case LW_PMC_BOOT_42_CHIP_ID_GA104:
            case LW_PMC_BOOT_42_CHIP_ID_GA106:
            case LW_PMC_BOOT_42_CHIP_ID_GA107:
            {
                *pFuseVersion = SEC2_GA102_UCODE_VERSION;
                return FLCN_OK;
            }
            default:
            {
                return FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
            }
        }
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Make sure the chip is allowed to do Playready
 *
 * Note that this function should never be prod signed for a chip that doesn't
 * have a silicon.
 *
 * @returns  FLCN_OK if chip is allowedto do Playready
 *           FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED otherwise
 */
FLCN_STATUS
sec2EnforceAllowedChipsForPlayreadyHS_GA10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    LwU32       chip;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &chip));

    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);
    switch(chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_GA102:
        case LW_PMC_BOOT_42_CHIP_ID_GA103:
        case LW_PMC_BOOT_42_CHIP_ID_GA104:
        case LW_PMC_BOOT_42_CHIP_ID_GA106:
        case LW_PMC_BOOT_42_CHIP_ID_GA107:
            return FLCN_OK;
        default:
            return FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    }

ErrorExit:
    return flcnStatus;
}

/*
 * @brief Configure the PLM settings of registers used for
 *        PMU-SEC2 communication by GPC-RG feature
 *
 * This includes the following registers -
 *  - LW_CSEC_QUEUE_HEAD(_PMU)
 *  - LW_PSEC_FALCON_COMMON_SCRATCH_GROUP_0
 *
 * For both the above registers, the access is required by level 2
 * ucode running on PMU and SEC2, and no one else as these registers
 * are used for communication between them for GPC-RG feature.
 *
 * Following confluence page has more details -
 * https://confluence.lwpu.com/display/A10xSRC/GPC-RG%3A+PMU-SEC2+communication+for+GPCCS+bootstrap
 */
FLCN_STATUS
sec2GpcRgRegPlmConfigHs_GA10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
    LwU32       privMask   = 0;
    LwU32       privSource = 0;

    // Configure the priv sources as PMU and SEC2 to be enabled
    privSource = (BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_PMU)  |
                  BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_SEC2) |
                  BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_LOCAL_SOURCE_FALCON));

    // Configure PLM settings for LW_CSEC_QUEUE_HEAD(_PMU)
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(
        LW_CSEC_CMDQ_HEAD_PRIV_LEVEL_MASK(SEC2_CMDMGMT_CMD_QUEUE_PMU), &privMask));

    privMask = FLD_SET_DRF(_CSEC, _CMDQ_HEAD_PRIV_LEVEL_MASK,
                           _READ_PROTECTION_LEVEL0, _ENABLE, privMask);
    privMask = FLD_SET_DRF(_CSEC, _CMDQ_HEAD_PRIV_LEVEL_MASK,
                           _READ_PROTECTION_LEVEL1, _ENABLE, privMask);
    privMask = FLD_SET_DRF(_CSEC, _CMDQ_HEAD_PRIV_LEVEL_MASK,
                           _READ_PROTECTION_LEVEL2, _ENABLE, privMask);

    privMask = FLD_SET_DRF(_CSEC, _CMDQ_HEAD_PRIV_LEVEL_MASK,
                           _WRITE_PROTECTION_LEVEL0, _DISABLE, privMask);
    privMask = FLD_SET_DRF(_CSEC, _CMDQ_HEAD_PRIV_LEVEL_MASK,
                           _WRITE_PROTECTION_LEVEL1, _DISABLE, privMask);
    privMask = FLD_SET_DRF(_CSEC, _CMDQ_HEAD_PRIV_LEVEL_MASK,
                           _WRITE_PROTECTION_LEVEL2, _ENABLE, privMask);

    privMask = FLD_SET_DRF_NUM(_CSEC, _CMDQ_HEAD_PRIV_LEVEL_MASK,
                               _SOURCE_ENABLE, privSource, privMask);

    CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(
        LW_CSEC_CMDQ_HEAD_PRIV_LEVEL_MASK(SEC2_CMDMGMT_CMD_QUEUE_PMU), privMask));

    // Configure PLM settings for LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_0
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(
        LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK, &privMask));

    privMask = FLD_SET_DRF(_CSEC, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK,
                           _READ_PROTECTION_LEVEL0, _ENABLE, privMask);
    privMask = FLD_SET_DRF(_CSEC, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK,
                           _READ_PROTECTION_LEVEL1, _ENABLE, privMask);
    privMask = FLD_SET_DRF(_CSEC, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK,
                           _READ_PROTECTION_LEVEL2, _ENABLE, privMask);

    privMask = FLD_SET_DRF(_CSEC, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK,
                           _WRITE_PROTECTION_LEVEL0, _DISABLE, privMask);
    privMask = FLD_SET_DRF(_CSEC, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK,
                           _WRITE_PROTECTION_LEVEL1, _DISABLE, privMask);
    privMask = FLD_SET_DRF(_CSEC, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK,
                           _WRITE_PROTECTION_LEVEL2, _ENABLE, privMask);

    privMask = FLD_SET_DRF_NUM(_CSEC, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK,
                               _SOURCE_ENABLE, privSource, privMask);

    CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(
        LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK, privMask));

ErrorExit:
    return flcnStatus;
}

/*
 * Check if Chip is Supported - LS
 */
FLCN_STATUS
sec2CheckIfChipIsSupportedLS_GA10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    LwU32       chip;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(LW_PMC_BOOT_42, &chip));

    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);

    if((chip == LW_PMC_BOOT_42_CHIP_ID_GA102) ||
       (chip == LW_PMC_BOOT_42_CHIP_ID_GA103) ||
       (chip == LW_PMC_BOOT_42_CHIP_ID_GA104) ||
       (chip == LW_PMC_BOOT_42_CHIP_ID_GA106) ||
       (chip == LW_PMC_BOOT_42_CHIP_ID_GA107))
    {
        return FLCN_OK;
    }

    return FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;

ErrorExit:
    return flcnStatus;
}

#ifdef BOOT_FROM_HS_BUILD
/*
 * Program IMB/IMB1/DMB/DMB1 register by getting the value from common scratch
 * group registers, programmed by acrlib during falcon bootstrap.
 */
FLCN_STATUS
sec2ProgramCodeDataRegistersFromCommonScratch_GA10X(void)
{
    LwU32 val;
    FLCN_STATUS flcnStatus = FLCN_OK;

    CHECK_FLCN_STATUS(CSB_REG_RD32_ERRCHK(LW_CSEC_FALCON_IMB_BASE_COMMON_SCRATCH, &val));
    falc_wspr(IMB, val);

    CHECK_FLCN_STATUS(CSB_REG_RD32_ERRCHK(LW_CSEC_FALCON_IMB1_BASE_COMMON_SCRATCH, &val));
    falc_wspr(IMB1, val);

    CHECK_FLCN_STATUS(CSB_REG_RD32_ERRCHK(LW_CSEC_FALCON_DMB_BASE_COMMON_SCRATCH, &val));
    falc_wspr(DMB, val);

    CHECK_FLCN_STATUS(CSB_REG_RD32_ERRCHK(LW_CSEC_FALCON_DMB1_BASE_COMMON_SCRATCH, &val));
    falc_wspr(DMB1, val);

ErrorExit:
    return flcnStatus;
}
#endif // BOOT_FROM_HS_BUILD

/*
 * Verify that SW fusing is disabled on prod boards
 * through fuse disable_sw_override.
 */
FLCN_STATUS
sec2CheckDisableSwOverrideFuseOnProdHS_GA10X(void)
{
    FLCN_STATUS flcnStatus    = FLCN_OK;
    LwU32       regVal        = 0;
    LwBool      bSec2ProdMode = !OS_SEC_FALC_IS_DBG_MODE();

    if (bSec2ProdMode == LW_TRUE)
    {
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_DISABLE_SW_OVERRIDE_STATUS, &regVal));

        if (!FLD_TEST_DRF_NUM(_FUSE, _DISABLE_SW_OVERRIDE_STATUS, _DATA, 0x1, regVal))
        {
            flcnStatus = FLCN_ERR_HS_CHK_SW_FUSING_ALLOWED_ON_PROD;
        }
    }

ErrorExit:
    return flcnStatus;
}

//
// This function DMA's the HS and HS LIB Ovl signatures from sec2's Data subwpr into sec2's DMEM
// RM patches these signatures to sec2's subWpr at runtime. This Runtime patching of signatures can be done only for
// PKC HS Sig validation. Please refer to bug 200617410 for more info.
//
#ifdef RUNTIME_HS_OVL_SIG_PATCHING
FLCN_STATUS
sec2DmaHsOvlSigBlobFromWpr_GA10X(void)
{
    LwU32            hsOvlSigBlobArraySize          = ((SIG_SIZE_IN_BITS/8) * (LwU32)_num_imem_hs_overlays);
    LwU32            regAddr                        = (LW_PFB_PRI_MMU_FALCON_SEC_CFGB(SEC2_SUB_WPR_ID_4_UCODE_DATA_SECTION_WPR1));
    LwU64            sec2DataSubWprEndAddr4kAligned = DRF_VAL(_PFB, _PRI_MMU_FALCON_SEC_CFGB, _ADDR_HI, REG_RD32(BAR0, regAddr));
    LwU64            hsSigBlobAddr                  = ((sec2DataSubWprEndAddr4kAligned << SHIFT_4KB) - MAX_SUPPORTED_HS_OVL_SIG_BLOB_SIZE);
    FLCN_STATUS      flcnStatus                     = FLCN_ERROR;
    RM_FLCN_MEM_DESC memDesc;

    RM_FLCN_U64_PACK(&(memDesc.address), &hsSigBlobAddr);

    memDesc.params = 0;
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                     RM_SEC2_DMAIDX_UCODE, memDesc.params);
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
                                     hsOvlSigBlobArraySize, memDesc.params);

    CHECK_FLCN_STATUS(dmaReadUnrestricted((void*)_imem_hs_signatures, &memDesc,
                                          0, hsOvlSigBlobArraySize));

ErrorExit:
    return flcnStatus;
}
#endif // RUNTIME_HS_OVL_SIG_PATCHING

/*!
 * @brief sec2ReconfigurePrivErrorPlmAndPrecedence_GA10X:
 *        Remove all other source but SEC2 from LW_PPRIV_SYS_PRIV_ERROR_PRIV_LEVEL_MASK and
 *        restore back the Priv Error Precedence to SEC2. (We forcibly set it
 *        again to sec2 to be on a safer side)
 *
 * @param  None
 *
 * @return FLCN_STATUS FLCN_OK on success.
 */
FLCN_STATUS
sec2ReconfigurePrivErrorPlmAndPrecedence_GA10X(void)
{
    LwU32 plmVal           = 0;
    LwU32 sourceMask       = 0;
    LwU32 precedenceVal    = 0;
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Fetch the current PLM value.
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPRIV_SYS_PRIV_ERROR_PRIV_LEVEL_MASK, &plmVal));

    // Remove all other source falcons but SEC2 for above PLM
    sourceMask = BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_SEC2);

    // Set the Source Enable bit to SEC2 only.
    plmVal = FLD_SET_DRF_NUM(_PPRIV, _SYS_PRIV_ERROR_PRIV_LEVEL_MASK, _SOURCE_ENABLE, sourceMask, plmVal);
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRIV_ERROR_PRIV_LEVEL_MASK, plmVal));

    // We now set the PRECEDENCE to SEC2 only.
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPRIV_SYS_PRIV_ERROR_PRECEDENCE, &precedenceVal));
    precedenceVal = FLD_SET_DRF_NUM(_PPRIV, _SYS_PRIV_ERROR_PRECEDENCE, _SOURCE_ID, LW_PPRIV_SYS_PRI_SOURCE_ID_SEC2, precedenceVal );
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRIV_ERROR_PRECEDENCE, precedenceVal));

ErrorExit:
    return flcnStatus;
}


/*!
 * @brief sec2UpdatePrivErrorPlm_GA10X:
 *        Add GSP back again to the SOURCE ENABLE of the Priv Error PLM
 *
 * @param  None
 *
 * @return FLCN_STATUS FLCN_OK on success.
 */
FLCN_STATUS
sec2UpdatePrivErrorPlm_GA10X(void)
{
    LwU32       plmVal     = 0;
    LwU32       sourceMask = 0;
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Fetch the current PLM value
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPRIV_SYS_PRIV_ERROR_PRIV_LEVEL_MASK, &plmVal));

    // Add GSP to the Source enable again so that FWSEC can use it again for Posted Writes
    sourceMask = (BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_SEC2) | BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP));

    // Set the Source Enable bit to SEC2 and GSP.
    plmVal = FLD_SET_DRF_NUM(_PPRIV, _SYS_PRIV_ERROR_PRIV_LEVEL_MASK, _SOURCE_ENABLE, sourceMask, plmVal);
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRIV_ERROR_PRIV_LEVEL_MASK, plmVal));

ErrorExit:
    return flcnStatus;
}

#ifdef WAR_SELWRE_THROTTLE_MINING_BUG_3263169

// Originally defined in uproc/common/inc/pmu_fbflcn_if.h
#define FBFLCN_CMD_QUEUE_IDX_PMU                (1U)
#define FBFLCN_CMD_QUEUE_IDX_PMU_HALT_NOTIFY    (2U)
#define PMU_CMD_QUEUE_IDX_FBFLCN                (2U)

/*!
 * @brief sec2SelwreThrottleMiningWAR_GA10X:
 *        Secure PLMs for PMU and FBFALCON for mining throttling
 *
 * @param[in] falconId  Falcon ID for which we can selectively
 *                      apply PLM changes.
 *
 * @return FLCN_STATUS FLCN_OK on success.
 */
FLCN_STATUS
sec2SelwreThrottleMiningWAR_GA10X
(
    LwU32 falconId
)
{
    LwU32       plmVal     = 0;
    LwU32       sourceMask = 0;
    LwU32       regVal     = 0;
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       trap14Action = 0;
    LwU32       i;

    // See https://confluence.lwpu.com/display/RMPER/Pri-source+Isolation+of+Registers

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_SPARE_BIT_1, &regVal));

    // Validate the memory tuning controller fuse
    if (!FLD_TEST_DRF(_FUSE, _SPARE_BIT_1, _DATA, _ENABLE, regVal))
    {
        return flcnStatus;
    }

    // Restrict PMU-specific resources and controls: RESET and ENGCTL
    if (falconId == LSF_FALCON_ID_PMU_RISCV)
    {
        // PLMs for PMU-specific registers on the PMU engine reset boundary


        // Enable-mask for PMU and FBFALCON (and also SEC2 to keep access to the PLMs)
        sourceMask = (BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_LOCAL_SOURCE_FALCON) |
                      BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_FBFALCON)            |
                      BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_SEC2));


        // Fetch the current PLM value
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPWR_PMU_QUEUE_HEAD_PRIV_LEVEL_MASK(PMU_CMD_QUEUE_IDX_FBFLCN), &plmVal));
        // Set the enable-mask for PMU and FBFALCON (and SEC2)
        plmVal = FLD_SET_DRF_NUM(_PPWR, _PMU_QUEUE_HEAD_PRIV_LEVEL_MASK, _SOURCE_ENABLE, sourceMask, plmVal);
        // Completely block anything that's not in enable-mask
        plmVal = FLD_SET_DRF(_PPWR, _PMU_QUEUE_HEAD_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL, _BLOCKED, plmVal);
        plmVal = FLD_SET_DRF(_PPWR, _PMU_QUEUE_HEAD_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCKED, plmVal);
        // As an additional measure, disable read and write at L0 and L1, enable at L2
        plmVal = FLD_SET_DRF(_PPWR, _PMU_QUEUE_HEAD_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL0, _DISABLE, plmVal);
        plmVal = FLD_SET_DRF(_PPWR, _PMU_QUEUE_HEAD_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL1, _DISABLE, plmVal);
        plmVal = FLD_SET_DRF(_PPWR, _PMU_QUEUE_HEAD_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
        plmVal = FLD_SET_DRF(_PPWR, _PMU_QUEUE_HEAD_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
        // Write back the PLM
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPWR_PMU_QUEUE_HEAD_PRIV_LEVEL_MASK(PMU_CMD_QUEUE_IDX_FBFLCN), plmVal));


        // Fetch the current PLM value
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPWR_PMU_QUEUE_TAIL_PRIV_LEVEL_MASK(PMU_CMD_QUEUE_IDX_FBFLCN), &plmVal));
        // Set the enable-mask for PMU and FBFALCON (and also SEC2 to keep access to the PLM)
        plmVal = FLD_SET_DRF_NUM(_PPWR, _PMU_QUEUE_TAIL_PRIV_LEVEL_MASK, _SOURCE_ENABLE, sourceMask, plmVal);
        // Completely block anything that's not in enable-mask
        plmVal = FLD_SET_DRF(_PPWR, _PMU_QUEUE_TAIL_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL, _BLOCKED, plmVal);
        plmVal = FLD_SET_DRF(_PPWR, _PMU_QUEUE_TAIL_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCKED, plmVal);
        // As an additional measure, disable read and write at L0 and L1, enable at L2
        plmVal = FLD_SET_DRF(_PPWR, _PMU_QUEUE_TAIL_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL0, _DISABLE, plmVal);
        plmVal = FLD_SET_DRF(_PPWR, _PMU_QUEUE_TAIL_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL1, _DISABLE, plmVal);
        plmVal = FLD_SET_DRF(_PPWR, _PMU_QUEUE_TAIL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
        plmVal = FLD_SET_DRF(_PPWR, _PMU_QUEUE_TAIL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
        // Write back the PLM
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPWR_PMU_QUEUE_TAIL_PRIV_LEVEL_MASK(PMU_CMD_QUEUE_IDX_FBFLCN), plmVal));


        // Fetch the current PLM value
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPWR_FALCON_RESET_PRIV_LEVEL_MASK, &plmVal));
        // Set the enable-mask for PMU (and also SEC2 to keep access to the PLM)
        sourceMask = (BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_LOCAL_SOURCE_FALCON) |
                      BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_SEC2));
        plmVal = FLD_SET_DRF_NUM(_PPWR, _FALCON_RESET_PRIV_LEVEL_MASK, _SOURCE_ENABLE, sourceMask, plmVal);
        // Drop anything that's not in enable-mask to L0. This way when PMU raises this PLM, another L2 core can't issue reset to it.
        plmVal = FLD_SET_DRF(_PPWR, _FALCON_RESET_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL, _LOWERED, plmVal);
        plmVal = FLD_SET_DRF(_PPWR, _FALCON_RESET_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _LOWERED, plmVal);
        // Leave rest of the values at default. PMU will raise its reset PLM as needed at pre-init.
        // Write back the PLM
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPWR_FALCON_RESET_PRIV_LEVEL_MASK, plmVal));


        //
        // Program decode trap 14 to block PRI access to LW_PPWR_FALCON_ENGCTL when PMU is started up
        // See https://confluence.lwpu.com/display/AMPCTXSWPRIHUB/Ampere+DECODE_TRAP+usage+by+chip#AmpereDECODE_TRAPusagebychip-GA10x
        //


        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION, &trap14Action));
        if (!FLD_TEST_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP14_ACTION, _ALL_ACTIONS, _DISABLED, trap14Action))
        {
            // We clear this decode trap in ACR-unload, so we don't expect to see it already set on startup.
            flcnStatus = FLCN_ERR_HS_DECODE_TRAP_ALREADY_IN_USE;
            goto ErrorExit;
        }
        // Set trap on all writes. PMU ENGCTL must be completely locked down to writes from the outside world.
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG,
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP14_MATCH_CFG, _TRAP_APPLICATION, _ALL_LEVELS_ENABLED) |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP14_MATCH_CFG, _IGNORE_READ, _IGNORED)                 |
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP14_MATCH_CFG, _SOURCE_ID_CTL, _SINGLE)));
        // Match all SOURCE_IDs
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP14_MASK,
                            DRF_NUM(_PPRIV, _SYS_PRI_DECODE_TRAP14_MASK, _SOURCE_ID, 0x3F)));
        // Match LW_PPWR_FALCON_ENGCTL
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH,
                            DRF_NUM(_PPRIV, _SYS_PRI_DECODE_TRAP14_MATCH, _ADDR, LW_PPWR_FALCON_ENGCTL)));
        // Set decode trap ACTION to FORCE_ERROR_RETURN
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION,
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP14_ACTION, _FORCE_ERROR_RETURN, _ENABLE)));
        // Zero out DATA1 and DATA2. These are not used since we set PRI_DECODE_TRAP_ACTION._FORCE_ERROR_RETURN
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1,
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP14_DATA1, _V, _I)));
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA2,
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP14_DATA2, _V, _I)));


        // General registers not on PMU engine reset boundary which we only want to bother protecting if PMU is being started


        // Fetch the current PLM value
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_THERM_PP_PRIV_LEVEL_MASK, &plmVal));
        // Set the enable-mask for PMU only (and also SEC2 to keep access to the PLM)
        sourceMask = (BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_LOCAL_SOURCE_FALCON) |
                      BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_SEC2));
        plmVal = FLD_SET_DRF_NUM(_THERM, _PP_PRIV_LEVEL_MASK, _SOURCE_ENABLE, sourceMask, plmVal);
        // Completely block anything that's not in enable-mask
        plmVal = FLD_SET_DRF(_THERM, _PP_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL, _BLOCKED, plmVal);
        plmVal = FLD_SET_DRF(_THERM, _PP_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCKED, plmVal);
        // As an additional measure, disable read and write at L0 and L1, enable at L2
        plmVal = FLD_SET_DRF(_THERM, _PP_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL0, _DISABLE, plmVal);
        plmVal = FLD_SET_DRF(_THERM, _PP_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL1, _DISABLE, plmVal);
        plmVal = FLD_SET_DRF(_THERM, _PP_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
        plmVal = FLD_SET_DRF(_THERM, _PP_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
        // Write back the PLM
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_THERM_PP_PRIV_LEVEL_MASK, plmVal));


        // Fetch the current PLM value
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_FBPA_MEM_PRIV_LEVEL_MASK, &plmVal));
        // Set the enable-mask for PMU, FBFALCON and PAFALCON (and also SEC2 to keep access to the PLM)
        sourceMask = (BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_PMU)                 |
                      BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_FBFALCON)            |
                      BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_LOCAL_SOURCE_FALCON) |
                      BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_SEC2));
        plmVal = FLD_SET_DRF_NUM(_PFB, _FBPA_MEM_PRIV_LEVEL_MASK, _SOURCE_ENABLE, sourceMask, plmVal);
        // Lower read priv to L0 for anything that's not in enable-mask
        plmVal = FLD_SET_DRF(_PFB, _FBPA_MEM_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL, _LOWERED, plmVal);
        // Disable write at L0 and L1, enable at L2
        plmVal = FLD_SET_DRF(_PFB, _FBPA_MEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
        plmVal = FLD_SET_DRF(_PFB, _FBPA_MEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
        // Write back the PLM
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_FBPA_MEM_PRIV_LEVEL_MASK, plmVal));
    }
    else if (falconId == LSF_FALCON_ID_FBFALCON)
    {
        //
        // FBFalcon queue registers accessed by PMU to request FBFalcon to trigger MCLK switch.
        // In case PMU is not started up but FBFalcon is, PRI-source isolation on these registers
        // ensures that an MCLK switch will never be done.
        //

        for (i = FBFLCN_CMD_QUEUE_IDX_PMU; i <= FBFLCN_CMD_QUEUE_IDX_PMU_HALT_NOTIFY; i++)
        {
            // Fetch the current PLM value
            CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFBFALCON_CMDQ_PRIV_LEVEL_MASK(i), &plmVal));
            // Set the enable-mask for PMU and FBFALCON (and also SEC2 to keep access to the PLM)
            sourceMask = (BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_PMU)                 |
                          BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_LOCAL_SOURCE_FALCON) |
                          BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_SEC2));
            plmVal = FLD_SET_DRF_NUM(_PFBFALCON, _CMDQ_PRIV_LEVEL_MASK, _SOURCE_ENABLE, sourceMask, plmVal);
            // Completely block anything that's not in enable-mask
            plmVal = FLD_SET_DRF(_PFBFALCON, _CMDQ_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL, _BLOCKED, plmVal);
            plmVal = FLD_SET_DRF(_PFBFALCON, _CMDQ_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCKED, plmVal);
            // As an additional measure, disable read and write at L0 and L1, enable at L2
            plmVal = FLD_SET_DRF(_PFBFALCON, _CMDQ_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PFBFALCON, _CMDQ_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL1, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PFBFALCON, _CMDQ_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PFBFALCON, _CMDQ_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            // Write back the PLM
            CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFBFALCON_CMDQ_PRIV_LEVEL_MASK(i), plmVal));
        }
    }



ErrorExit:
    return flcnStatus;
}
#endif // WAR_SELWRE_THROTTLE_MINING_BUG_3263169
