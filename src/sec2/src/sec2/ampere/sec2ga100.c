/*
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2ga100.c
 * @brief  SEC2 HAL Functions for GA100
 *
 * SEC2 HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "dev_ram.h"
#include "lwosselwreovly.h"
#include "dev_master.h"
#include "sec2_bar0.h"
#include "sec2_csb.h"
#include "sec2_hs.h"
#include "dev_ltc.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "hwref/ampere/ga100/bootrom_api_parameters.h"
#include "hwref/ampere/ga100/dev_sec_pri.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_sec2_private.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
SW_PARAM_DECRYPT_FUSEKEYS decFuseStruct;

/* ------------------------- Macros and Defines ----------------------------- */
#define UPDATE_KEY(parm, i, j, k)     ( *(((LwU32*)&((parm)->keyArr[j]))+i) = BAR0_REG_RD32(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_0+((k) * 4)) )
#define UPDATE_PROTECTINFO(parm, i)   ( *(((LwU32*)&((parm)->protInfoArr[i]))) = BAR0_REG_RD32(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_PROTECTINFO_PLAINTEXT_0 + ((i) * 4)))
#define UPDATE_SIGNATURE(parm, i)     ( *(((LwU32*)&((parm)->signature))+i) = BAR0_REG_RD32(LW_FUSE_OPT_ENC_FUSE_KEY_SIGNATURE_0 + ((i)) *4) )
#define BROM_API_DECFUSEKEY(parm)  falc_strap(LW_PSEC_FALCON_BROM_FUNID_ID_DECFUSEKEY, (LwU32*)parm)

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */

/*
 * @brief Get the SW fuse version
 */
#define SEC2_GA100_UCODE_VERSION    (0x1)
FLCN_STATUS
sec2GetSWFuseVersionHS_GA100
(
    LwU32* pFuseVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    LwU32       chip;

    if (OS_SEC_FALC_IS_DBG_MODE())
    {
        *pFuseVersion = SEC2_GA100_UCODE_VERSION;
        return FLCN_OK;
    }
    else
    {
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &chip));

        chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);

        switch(chip)
        {
            case LW_PMC_BOOT_42_CHIP_ID_GA100 :
            {
                *pFuseVersion = SEC2_GA100_UCODE_VERSION;
                return FLCN_OK;
                break;
            }
            default:
            {
                return FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
                break;
            }
        }
    }

ErrorExit:
    return flcnStatus;
}

/*
 * @brief Get the HW fpf version
 */
FLCN_STATUS
sec2GetHWFpfVersionHS_GA100
(
    LwU32* pFpfVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
    LwU32       count      = 0;
    LwU32       fpfFuse    = 0;

    if (!pFpfVersion)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_FPF_UCODE_SEC2_REV, &fpfFuse));
    fpfFuse = DRF_VAL(_FUSE, _OPT_FPF_UCODE_SEC2_REV, _DATA, fpfFuse);

    /*
     * FPF Fuse and SW Ucode version has below binding
     * FPF FUSE      SW Ucode Version
     *   0              0
     *   1              1
     *   3              2
     *   7              3
     * and so on. */
    while (fpfFuse != 0)
    {
        count++;
        fpfFuse >>= 1;
    }

    *pFpfVersion = count;

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Check the SW fuse version revocation against HW fpf version
 */
FLCN_STATUS
sec2CheckFuseRevocationAgainstHWFpfVersionHS_GA100
(
    LwU32 fuseVersionSW
)
{
    LwU32       fpfVersionHW = 0;
    FLCN_STATUS flcnStatus   = FLCN_ERROR;

    // Get HW Fpf Version
    CHECK_FLCN_STATUS(sec2GetHWFpfVersionHS_HAL(&sec2, &fpfVersionHW));

    //
    // Compare SW fuse version against HW fpf version.
    // If SW fuse Version is lower return error.
    //
    if (fuseVersionSW < fpfVersionHW)
    {
        return FLCN_ERR_HS_CHK_UCODE_REVOKED;
    }

ErrorExit:
    return flcnStatus;
}

/*
 * @brief WAR to lower PLMs of TSTG registers and Isolate to PMU and FECS (Bug 2735125)
 */
FLCN_STATUS
sec2ProvideAccessOfTSTGRegistersToPMUAndFecsWARBug2735125HS_GA100(void)
{
    FLCN_STATUS flcnStatus               = FLCN_ERROR;
    LwU32       plmVal                   = 0;
    LwU32       plmPriSourceIsolatiolwal = 0;
    
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PLTCG_LTCS_LTSS_TSTG_CFG_4__PRIV_LEVEL_MASK, &plmVal));

    // Update Read Protection
    plmVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG3_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL2, _ENABLE, plmVal);
    plmVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG3_PRIV_LEVEL_MASK, _READ_VIOLATION, _REPORT_ERROR, plmVal);

    // Update Write Protection
    plmVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG3_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _ENABLE, plmVal);
    plmVal = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG3_PRIV_LEVEL_MASK, _WRITE_VIOLATION, _REPORT_ERROR, plmVal);

    // PRI Source Isolation
    plmPriSourceIsolatiolwal = BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_PMU) | BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_FECS) |
                               BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_SEC2);
    plmVal = FLD_SET_DRF_NUM(_PLTCG, _LTCS_LTSS_TSTG3_PRIV_LEVEL_MASK, _SOURCE_ENABLE, plmPriSourceIsolatiolwal, plmVal);
    
    // Update PLM register
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PLTCG_LTCS_LTSS_TSTG_CFG_4__PRIV_LEVEL_MASK, plmVal));

ErrorExit:
    return flcnStatus;
}

/*
 * Check if Chip is Supported - LS
 */
FLCN_STATUS
sec2CheckIfChipIsSupportedLS_GA100(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    LwU32       chip;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(LW_PMC_BOOT_42, &chip));

    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);

    if (chip == LW_PMC_BOOT_42_CHIP_ID_GA100)
    {
        return FLCN_OK;
    }
    else
    {
        return FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    }

ErrorExit:
    return flcnStatus;
}

/*
 * Decrypt fuse blob using BROM - LS
 */
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
FLCN_STATUS
sec2DecryptFuseKeysLS_GA100(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32 i, j, k;

    SW_PARAM_DECRYPT_FUSEKEYS *pParamBromDecryptFuse = &decFuseStruct;

    // Clear Decrypt Fuse parm structure
    memset(pParamBromDecryptFuse, 0, sizeof(SW_PARAM_DECRYPT_FUSEKEYS));

    // Get Fuse HDR.
    LwU32 fuseHeader = BAR0_REG_RD32(LW_FUSE_OPT_ENC_FUSE_KEY_HEADER_PLAINTEXT);
    *(LwU32 *)(&pParamBromDecryptFuse->header) = fuseHeader;

    //
    // Build paramater block.
    //
    j = 0;  // Indexs keyArr element
    i = 0;  // Indexs dwords in keyArr
    for (k = 0; k < 20; k++) // Indexes dwords in ciphertext
    { 
        i = k % 4;
        UPDATE_KEY(pParamBromDecryptFuse, i, j, k);
        // After 4 dwords, move on to next keyArr element.
        if (i == 3)
        {
            j++;
        }
    }

    for (i = 0; i < 4; i++)
    {
        UPDATE_PROTECTINFO(pParamBromDecryptFuse, i);
    }


    for (i = 0; i < 4; i++)
    {
        UPDATE_SIGNATURE(pParamBromDecryptFuse, i);
    }

    //
    // Call BROM to decrypt fuse blob.
    //
    BROM_API_DECFUSEKEY((LwU32*)pParamBromDecryptFuse);
    LwU32 brRetCode = BAR0_REG_RD32(LW_PSEC_FALCON_BROM_RETCODE);
    if (!FLD_TEST_DRF(_PSEC, _FALCON_BROM_RETCODE, _ERR, _SUCCESS, brRetCode))
    {
        flcnStatus = FLCN_ERROR;
    }

    return flcnStatus;
}
#endif //SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM)

/*
 * Update the Reset PLM's so that NS/LS entity cannot reset Sec2 while in HS
 */
FLCN_STATUS
sec2UpdateResetPrivLevelMasksHS_GA100(LwBool bIsRaise)
{
    FLCN_STATUS flcnStatus   = FLCN_ERROR;
    LwBool      bHSMode      = LW_FALSE;
    LwU32       plmResetVal  = 0;

    _sec2IsLsOrHsModeSet_GP10X(NULL, &bHSMode);

    //
    // Only raise priv level masks when we're in booted in HS mode,
    // else we'll take away our own ability to reset the masks when we
    // unload.
    //
    if (!bHSMode)
    {
        return FLCN_ERR_HS_CHK_NOT_IN_HSMODE;
    }

    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK, &plmResetVal));

    // Lock falcon reset for level 0, 1, 2
    if (bIsRaise)
    {
        plmResetVal = FLD_SET_DRF(_CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmResetVal);
        plmResetVal = FLD_SET_DRF(_CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmResetVal);
        plmResetVal = FLD_SET_DRF(_CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE, plmResetVal);

    } 
    else // Restore LS settings by unlocking LVL2 only
    {
        plmResetVal = FLD_SET_DRF(_CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _ENABLE, plmResetVal);
    }

    CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK, plmResetVal));

ErrorExit:
    return flcnStatus;
}
