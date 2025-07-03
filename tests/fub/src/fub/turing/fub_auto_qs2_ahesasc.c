/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: fub_auto_qs2_ahesasc.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "fub.h"
#include "fubreg.h"
#include "config/g_fub_private.h"
#include "dev_fuse.h"
#include "dev_fuse_addendum.h"
#include "dev_fpf.h"
#include "lwdevid.h"
#include "fub_inline_static_funcs_tu10x.h"
#if (FUBCFG_FEATURE_ENABLED(FUBCORE_TU10X) || FUBCFG_FEATURE_ENABLED(FUBCORE_TU116))
#include "fub_inline_static_funcs_fpf_tu10x.h"
#include "fub_inline_static_funcs_fpf_tu116.h"
#include "fub_inline_static_funcs_tu116.h"
#endif
#include "dev_gsp.h"
#include "dev_pwr_pri.h"
#include "dev_sec_csb.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
extern RM_FLCN_FUB_DESC  g_desc;
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */


#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_AHESASC_FUSE)         || \
     FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_ASB_FUSE)             || \
     FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_LWFLASH_FUSE)         || \
     FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_IMAGESELECT_FUSE)     || \
     FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_HULK_FUSE)            || \
     FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_FWSEC_FUSE)) 

/*
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 * @brief: Validates that the input addr is from the allowlisted register address.
 *         If found valid reads the register and returns the data.
 *         This function only reads the data via BAR0.
 *         To be used only in case of AUTO QS1 to QS2 binary else never use this.
 *         This function is created since the original function FALC_REG_RD32 is inline.
 *         fub binary size with original functional was going above 64K limit.
 * @param[in] : addr addr whose values needs to be read.
 * @param[out]: pData if register is read, data is returned through this parameter.
 * @return FUB_OK if the register reads was succefull, error otherwise.
 *
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 */
static FUB_STATUS fubNonInlineReadBar0_TU10X(LwU32 addr, LwU32 *pData)
                    GCC_ATTRIB_SECTION("imem_fub", "fubNonInlineReadBar0_TU10X");
static FUB_STATUS
fubNonInlineReadBar0_TU10X(LwU32 addr, LwU32 *pData)
{
    LwU32      val32         = 0;
    LwBool     bReadRegister = LW_FALSE;
    FUB_STATUS fubStatus     = FUB_ERR_CSB_PRIV_READ_ERROR;

    if (pData == NULL)
    {
        return FUB_ERR_ILWALID_ARGUMENT;
    }

    switch (addr)
    {
        case LW_PMC_BOOT_42                 :
        case LW_FUSE_SPARE_BIT_37           :
        case LW_FUSE_OPT_PCIE_DEVIDA        :
        case LW_FUSE_OPT_PCIE_DEVIDB        :
        case LW_FUSE_OPT_PKC_PK_ARRAY_INDEX :
            bReadRegister = LW_TRUE;
            break;
        default                             :
            bReadRegister = LW_FALSE;
            break;
    }
    if (bReadRegister == LW_FALSE)
    {
        return FUB_ERR_ILWALID_ARGUMENT;
    }
#ifdef FUB_ON_LWDEC
    // Control should not come here hence adding ct_assert.
    ct_assert(0);
#elif FUB_ON_SEC2
    CHECK_FUB_STATUS(fubBar0WaitIdle_HAL());
    CHECK_FUB_STATUS(FALC_REG_WR32_STALL(CSB, LW_CSEC_BAR0_ADDR, addr));

    CHECK_FUB_STATUS(FALC_REG_WR32_STALL(CSB, LW_CSEC_BAR0_CSR,
                        DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _READ) |
                        DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF  ) |
                        DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE)));

    CHECK_FUB_STATUS(FALC_REG_RD32_STALL(CSB, LW_CSEC_BAR0_CSR, &val32));

    CHECK_FUB_STATUS(fubBar0WaitIdle_HAL());

    CHECK_FUB_STATUS(FALC_REG_RD32(CSB, LW_CSEC_BAR0_DATA, pData));
#endif

ErrorExit:
    return fubStatus;
}

/*
 * @brief: Common check to verify that the board is auto sku board.
 * @param[out]: pbIsFuseBurningApplicable True when the board is auto sku board else false.
 * @return FUB_OK if the board is auto sku board, error otherwise.
 */
FUB_STATUS
fubCheckIfAutoQs2IsApplicableCommonChecks_TU104
(
    LwBool* pbIsFuseBurningApplicable

)
{

    FUB_STATUS  fubStatus   = FUB_ERR_GENERIC;
    LwU32 regVal            = 0;
    LwU32 regData           = 0;
    LwU32 devIdA            = 0;
    LwU32 devIdB            = 0;
    LwU32 chip              = 0;
    
    if (pbIsFuseBurningApplicable == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }
    *pbIsFuseBurningApplicable = LW_FALSE;

    //
    // Following code makes use of fubNonInlineReadBar0_TU10X. 
    // Use of fubNonInlineReadBar0_TU10X is restricted to this function only.
    // Never use the same else where in code.
    //

    // Verify that FUB is running on auto sku. 
    CHECK_FUB_STATUS(fubNonInlineReadBar0_TU10X(LW_FUSE_SPARE_BIT_37, &regData));
    regData = DRF_VAL(_FUSE, _SPARE_BIT_37, _DATA, regData);
    if(regData == LW_FUSE_SPARE_BIT_37_DATA_DISABLE)
    {
        *pbIsFuseBurningApplicable = LW_FALSE;
        goto ErrorExit;
    }

    //check to verify that the PKC index is 3.
    CHECK_FUB_STATUS(fubNonInlineReadBar0_TU10X(LW_FUSE_OPT_PKC_PK_ARRAY_INDEX, &regVal));
    if (regVal != LW_FUSE_OPT_PKC_PK_ARRAY_INDEX_DATA_AUTO_TU104)
    {
        *pbIsFuseBurningApplicable = LW_FALSE;
        goto ErrorExit;
    }
    
    // Dev-Id check
    CHECK_FUB_STATUS(fubNonInlineReadBar0_TU10X(LW_FUSE_OPT_PCIE_DEVIDA, &devIdA));
    CHECK_FUB_STATUS(fubNonInlineReadBar0_TU10X(LW_FUSE_OPT_PCIE_DEVIDB, &devIdB));

    CHECK_FUB_STATUS(fubNonInlineReadBar0_TU10X(LW_PMC_BOOT_42, &chip));

    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);
    
    // These defines are coming from lwdevid.h, so make sure they are not tampered with
    ct_assert(LW_PCI_DEVID_DEVICE_PG189_SKU510 == (0x1EBC));
    ct_assert(LW_PCI_DEVID_DEVICE_PG189_SKU510_DEVIDB == (0x1EFC));

    if (chip == LW_PMC_BOOT_42_CHIP_ID_TU104)
    {
       if ((devIdA != LW_PCI_DEVID_DEVICE_PG189_SKU510) || 
           (devIdB != LW_PCI_DEVID_DEVICE_PG189_SKU510_DEVIDB))
       {
           *pbIsFuseBurningApplicable = LW_FALSE;
           goto ErrorExit;
       }
    }
    else
    {
        CHECK_FUB_STATUS(FUB_ERR_UNSUPPORTED_CHIP);
    }

    // If we reached here means all the checks are passed 
    *pbIsFuseBurningApplicable = LW_TRUE;

ErrorExit:
    return fubStatus;
}
#endif

#if (FUBCFG_FEATURE_ENABLED(FUB_BURN_AUTO_QS2_AHESASC_FUSE))
/*
 * @brief: Check to verify that the board is auto sku board.
 * @param[out]: pbIsFuseBurningApplicable True when the board is auto sku board else false.
 * @return FUB_OK if the board is auto sku board, error otherwise.
 */
FUB_STATUS
fubCheckIfAutoQs2AhesascIsApplicable_TU104
(
    LwBool *pbIsFuseBurningApplicable
)
{
    FUB_STATUS  fubStatus   = FUB_OK;
    if (pbIsFuseBurningApplicable == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }
    *pbIsFuseBurningApplicable = LW_FALSE;
    //
    // Check if use cases passed by RegKey includes burning fuse for AHESASC QS1 to QS2.
    // In Case of failure set *pbIsFuseBurningApplicable to LW_FALSE but do not return error status
    // as fuse burn decision is based on *pbIsFuseBurningApplicable and not on returned Error status.
    //
    if (!(g_desc.useCaseMask & LW_REG_STR_RM_FUB_USE_CASE_MASK_AUTO_QS2_AHESASC_FUSE))
    {
        *pbIsFuseBurningApplicable = LW_FALSE;
        fubStatus                  = FUB_OK;
        goto ErrorExit;
    }

    // Check first if AUTO_QS2_AHESASC  fuse  is allowed to be burnt on current GPU
    CHECK_FUB_STATUS(fubCheckIfAutoQs2IsApplicableCommonChecks_HAL(pFub, pbIsFuseBurningApplicable));

    if (!*pbIsFuseBurningApplicable)
    {
       fubReportApplicabilityFailure_HAL(pFub, LW_FUB_USE_CASE_MASK_AUTO_QS2_AHESASC_FUSE);
    }
ErrorExit:
    return fubStatus;
}

/*
 * @brief: Check if Fuse for enabling AHESASC QS2 version is to be burnt and burn if check passes.
 * @param[out]: pfubUseCasMask Update this mask if fuse burn was successfull.
 * @return FUB_OK fuse burn is successfull, error otherwise.
 */
FUB_STATUS
fubCheckAndBurnAutoQs2AhesascFuse_TU104
(
    LwU32 *pfubUseCasMask
)
{
    LwBool        bIsFuseBurningApplicable = LW_FALSE;
    FUB_STATUS    fubStatus                = FUB_ERR_GENERIC;
    FPF_FUSE_INFO fuseInfo;

    // Return early if pointer passed as argument is NULL.
    if (pfubUseCasMask == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    // Return early if current use case specific bit is set in use case mask
    if (*pfubUseCasMask & LW_FUB_USE_CASE_MASK_AUTO_QS2_AHESASC_FUSE)
    {
        CHECK_FUB_STATUS(FUB_ERR_AUTO_QS2_AHESASC_ALREADY_SET_IN_MASK);
    }
    
    // Check first if AUTO_QS2_AHESASC  fuse  is allowed to be burnt on current GPU
    CHECK_FUB_STATUS(fubCheckIfAutoQs2AhesascIsApplicable_HAL(pFub, &bIsFuseBurningApplicable));
    if(bIsFuseBurningApplicable)
    {
        // Populate Fuse Information Structure
        CHECK_FUB_STATUS(fubFillFuseInformationStructure_HAL(pFub, &fuseInfo, LW_FUB_USE_CASE_MASK_AUTO_QS2_AHESASC_FUSE));
        // Proceed to Burning only if New fuseValue bits are not already burnt
        if (fuseInfo.fuseAdjustedBurlwalue)
        {
            // Burn Primary Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fusePriAddress, fuseInfo.fuseAdjustedBurlwalue));

            // Burn Redundant Fuse
            CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fuseRedAddress, fuseInfo.fuseAdjustedBurlwalue));
            
            //
            // If we reach this point then, fuse burn is successful,
            // Set use case specific bit in mask variable, for further reference.
            //
            *pfubUseCasMask = (*pfubUseCasMask | LW_FUB_USE_CASE_MASK_AUTO_QS2_AHESASC_FUSE);
        }
        else
        {
            CHECK_FUB_STATUS(FUB_ERR_FUSE_BURN_NOT_REQUIRED);
        }
    }
    else
    {
        // Do not return error as FUB may be running for another use case.
        fubStatus = FUB_OK;
    }
ErrorExit:
    return fubStatus;
}
#endif
