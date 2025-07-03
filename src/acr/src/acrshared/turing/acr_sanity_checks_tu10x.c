/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_sanity_checks_tu10x.c
 */
//
// Includes
//
#include "acr.h"
#include "acrdrfmacros.h"
#include "acr_objacr.h"

#include "falcon.h"
#include "dev_master.h"
#include "dev_fb.h"
#include "dev_se_seb.h"
#include "dev_se_seb_addendum.h"
#include "dev_pwr_csb.h"
#include "dev_fuse.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"

#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
#include "dev_sec_csb.h"
#endif

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

#ifndef BSI_LOCK
/*
 * This funtion will ilwalidate the bubbles (blocks not of ACR HS, caused if ACR blocks are not loaded contiguously in IMEM)
 */
void
acrValidateBlocks_TU10X(void)
{
    LwU32 start        = (ACR_PC_TRIM((LwU32)_acr_resident_code_start));
    LwU32 end          = (ACR_PC_TRIM((LwU32)_acr_resident_code_end));

    LwU32 tmp          = 0;
    LwU32 imemBlks     = 0;
    LwU32 blockInfo    = 0;
    LwU32 lwrrAddr     = 0;

    //
    // Loop through all IMEM blocks and ilwalidate those that doesnt fall within
    // acr_resident_code_start and acr_resident_code_end
    //

    // Read the total number of IMEM blocks
#if defined(AHESASC) || defined(ASB) || defined(ACR_UNLOAD_ON_SEC2)
    tmp = ACR_REG_RD32(CSB, LW_CSEC_FALCON_HWCFG);
    imemBlks = DRF_VAL(_CSEC_FALCON, _HWCFG, _IMEM_SIZE, tmp);
#elif defined(ACR_UNLOAD)
    tmp = ACR_REG_RD32(CSB, LW_CPWR_FALCON_HWCFG);
    imemBlks = DRF_VAL(_CPWR_FALCON, _HWCFG, _IMEM_SIZE, tmp);
#else
    ct_assert(0);
#endif
    for (tmp = 0; tmp < imemBlks; tmp++)
    {
        falc_imblk(&blockInfo, tmp);

        if (!(IMBLK_IS_ILWALID(blockInfo)))
        {
            lwrrAddr = IMBLK_TAG_ADDR(blockInfo);

            // Ilwalidate the block if it is not a part of ACR binary
            if (lwrrAddr < start || lwrrAddr >= end)
            {
                falc_imilw(tmp);
            }
        }
    }
}
#endif


/*
 * @brief: Check if the secure lock HW state is compatible with ACR's expectations
 * 
 * Row#  Secure  WPR with  Policy allows                                                                                                                                                                                   
 *       Lock    secure      WPR with     WPR Permitted ?                                                                   Assert?                                                                                        
 *       set     lock set   secure lock                                                                                                                                                                                    
 * 
 * 1       0         0           0         Yes                                                                              No                                                                                             
 *                                                                                                                                                                                                                         
 * 2       0         1           0         Theoretically yes but practially no because we need to assert                    Yes. Invalid setup by HULK. Why would they turn on WPR allowed bit and not enable secure lock? 
 *                                                                                                                                                                                                                         
 * 3       1         0           0         No. HW wont allow WPR.                                                          No                                                                                             
 *                                                                                                                                                                                                                         
 * 4       1         1           0         No because as per policy we don't permit WPR with secure lock while it is set    Yes. Invalid setup by HULK.                                                                    
 *                                                                                                                                                                                                                         
 * 5       0         0           1         Yes                                                                              No                                                                                             
 *                                                                                                                                                                                                                         
 * 6       0         1           1         Theoretically yes but practially no because we need to assert                    Yes. Invalid setup by HULK. Why would they turn on WPR allowed bit and not enable secure lock? 
 *                                                                                                                                                                                                                         
 * 7       1         0           1         No because as per policy we want to permit WPR with secure lock while it is set  No                                                                                             
 *                                                                                                                                                                                                                         
 * 8       1         1           1         Yes                                                                              No                                                                                             
 *  
 *
 */
ACR_STATUS acrValidateSelwreLockHwStateCompatibilityWithACR_TU10X(void)
{
    ACR_STATUS  status                           = ACR_OK;
    LwBool      bIsMmuSelwreLockEnabledInHW      = LW_TRUE;
    LwBool      bIsWprAllowedWithSelwreLockInHW  = LW_FALSE;
    LwBool      bPolicyAllowsWPRWithMMULocked    = LW_FALSE;

    // Fetch the current state of secure lock from HW
    if (ACR_OK != (status = acrGetMmuSelwreLockStateFromHW_HAL(pAcr, &bIsMmuSelwreLockEnabledInHW, &bIsWprAllowedWithSelwreLockInHW)))
    {
        return status;
    }

    // Fetch the policy for WPR allowed with secure lock for this chip
    if (ACR_OK != (status = acrGetMmuSelwreLockWprAllowPolicy_HAL(pAcr, &bPolicyAllowsWPRWithMMULocked)))
    {
        return status;
    }

    if (bPolicyAllowsWPRWithMMULocked)
    {
        //
        // If policy allows us to run with secure lock, either secure lock must be completely off in HW,
        // or it must be ON with WPR allowed, otherwise ACR can't run. Reject to run on such a setup if we find it.
        //
        if (!bIsMmuSelwreLockEnabledInHW && bIsWprAllowedWithSelwreLockInHW)
        {
            return ACR_ERROR_SELWRE_LOCK_NOT_ENABLED_BUT_WPR_ALLOWED_SET;
        }
        else if (bIsMmuSelwreLockEnabledInHW && !bIsWprAllowedWithSelwreLockInHW)
        {
            return ACR_ERROR_SELWRE_LOCK_SETUP_DOES_NOT_ALLOW_WPR;
        }
    }
    else
    {
        //
        // If policy does not allow us to run with secure lock, secure lock must not be set up, as well as WPR_ALLOW must be off
        // otherwise it may be an incorrect way of configuring secure lock. Reject to run on such a setup if we find.
        //
        if (!bIsMmuSelwreLockEnabledInHW && bIsWprAllowedWithSelwreLockInHW)
        {
            return ACR_ERROR_SELWRE_LOCK_NOT_ENABLED_BUT_WPR_ALLOWED_SET;
        }
        else if (bIsMmuSelwreLockEnabledInHW && !bIsWprAllowedWithSelwreLockInHW)
        {
            return ACR_ERROR_SELWRE_LOCK_SETUP_DOES_NOT_ALLOW_WPR;
        }
        else if (bIsMmuSelwreLockEnabledInHW && bIsWprAllowedWithSelwreLockInHW)
        {
            return ACR_ERROR_ACR_POLICY_DOES_NOT_ALLOW_ACR_WITH_SELWRE_LOCK;
        }
    }

    return status;
}

/*
* @brief: Returns the ACR binary's policy whether it allows ACR to run and WPRs to be set up with secure lock enabled
*/
ACR_STATUS
acrGetMmuSelwreLockWprAllowPolicy_TU10X
(
    LwBool *pbPolicyAllowsWPRWithMMULocked
)
{
    if (NULL == pbPolicyAllowsWPRWithMMULocked)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    // On GV100, we will not allow ACR to work with secure lock, even if the secure lock has been setup in HW to allow WPR
    *pbPolicyAllowsWPRWithMMULocked = LW_FALSE;

    return ACR_OK;
}

/*
 * Verify if binary is running on expected falcon/engine. Volta onwards we have ENGID support
 */
ACR_STATUS
acrCheckIfEngineIsSupported_TU10X(void)
{
    //
    // Check if binary is allowed on engine/falcon, lwrrently ENGID supported only on SEC2 and GSPLite
    //
#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    LwU32 engId = DRF_VAL(_CSEC, _FALCON_ENGID, _FAMILYID, ACR_REG_RD32(CSB, LW_CSEC_FALCON_ENGID));
    if (engId != LW_CSEC_FALCON_ENGID_FAMILYID_SEC)
    {
        return ACR_ERROR_BINARY_NOT_RUNNING_ON_EXPECTED_FALCON;
    }
#endif

#if defined(ASB)
    LwU32 engId = DRF_VAL(_CSEC, _FALCON_ENGID, _FAMILYID, ACR_REG_RD32(CSB, LW_CSEC_FALCON_ENGID));
    if (engId != LW_CSEC_FALCON_ENGID_FAMILYID_GSP)
    {
        return ACR_ERROR_BINARY_NOT_RUNNING_ON_EXPECTED_FALCON;
    }
#endif

    return ACR_OK;
}

/*!
 * Verify is this build should be allowed to run on particular chip
 */
ACR_STATUS
acrCheckIfBuildIsSupported_TU10X(void)
{
    LwU32 chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, ACR_REG_RD32(BAR0, LW_PMC_BOOT_42));

    if ((chip == LW_PMC_BOOT_42_CHIP_ID_TU102) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_TU104) ||
        (chip == LW_PMC_BOOT_42_CHIP_ID_TU106))
    {
        return ACR_OK;
    }
    else
    {
        return ACR_ERROR_ILWALID_CHIP_ID;
    }
}

#define ACR_TU10X_DEFAULT_UCODE_BUILD_VERSION       0x0     // Only for chips TU102, TU104, TU106
/*!
 * @brief Get the ucode build version
 */
ACR_STATUS
acrGetUcodeBuildVersion_TU10X(LwU32* pUcodeBuildVersion)
{
    if (!pUcodeBuildVersion)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    //
    // Setup a ct_assert to warn in advance when we are close to running out of #bits for ACR version
    // Keeping a buffer of 2 version increments
    //
    ct_assert((ACR_TU102_UCODE_BUILD_VERSION         + 3) < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_ACR_BINARY_VERSION));
    ct_assert((ACR_TU104_UCODE_BUILD_VERSION         + 3) < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_ACR_BINARY_VERSION));
    ct_assert((ACR_TU106_UCODE_BUILD_VERSION         + 3) < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_ACR_BINARY_VERSION));
    ct_assert((ACR_TU10X_DEFAULT_UCODE_BUILD_VERSION + 3) < DRF_MASK(LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_ACR_BINARY_VERSION));

    LwU32 chip = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID, ACR_REG_RD32(BAR0, LW_PMC_BOOT_42));

    if (chip == LW_PMC_BOOT_42_CHIP_ID_TU102)
    {
        *pUcodeBuildVersion = ACR_TU102_UCODE_BUILD_VERSION;
    }
    else if (chip == LW_PMC_BOOT_42_CHIP_ID_TU104)
    {
        *pUcodeBuildVersion = ACR_TU104_UCODE_BUILD_VERSION;
    }
    else if (chip == LW_PMC_BOOT_42_CHIP_ID_TU106)
    {
        *pUcodeBuildVersion = ACR_TU106_UCODE_BUILD_VERSION;
    }
    else
    {
        *pUcodeBuildVersion = ACR_TU10X_DEFAULT_UCODE_BUILD_VERSION;
    }

    return ACR_OK;
}

/*!
 * @brief Make sure that ACR BSI binaries are triggered in GC6 exit path only
 */
ACR_STATUS
acrCheckIfGc6ExitIndeed_TU10X(void)
{
    // Check GC6 exit level3 secure interrupt to confirm if we are in GC6 exit path

    LwU32 intrPlm = ACR_REG_RD32(BAR0, LW_PGC6_SCI_SEC_STATUS_PRIV_LEVEL_MASK);
    if (FLD_TEST_DRF( _PGC6, _SCI_SEC_STATUS_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _ENABLE, intrPlm) ||
        FLD_TEST_DRF( _PGC6, _SCI_SEC_STATUS_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _ENABLE, intrPlm) ||
        FLD_TEST_DRF( _PGC6, _SCI_SEC_STATUS_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _ENABLE, intrPlm))
    {
        return ACR_ERROR_GC6_SELWRE_INTERRUPT_NOT_LEVEL3_PROTECTED;
    }

    LwU32 intr = ACR_REG_RD32(BAR0, LW_PGC6_SCI_SW_SEC_INTR_STATUS);
    if (!FLD_TEST_DRF( _PGC6, _SCI_SW_SEC_INTR_STATUS, _GC6_EXIT, _PENDING, intr))
    {
        return ACR_ERROR_NOT_IN_GC6_EXIT;
    }
    return ACR_OK;
}

/*!
 * @brief Function that checks if priv sec is enabled on prod board
 */
ACR_STATUS
acrCheckIfPrivSecEnabledOnProd_TU10X(void)
{

    if ( FLD_TEST_DRF( _CSEC, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED, ACR_REG_RD32( CSB, LW_CSEC_SCP_CTL_STAT ) ) )
    {
        if ( FLD_TEST_DRF( _FUSE, _OPT_PRIV_SEC_EN, _DATA, _NO, ACR_REG_RD32( BAR0, LW_FUSE_OPT_PRIV_SEC_EN ) ) )
        {
            // Error: Priv sec is not enabled
            return ACR_ERROR_PRIV_SEC_NOT_ENABLED;
        }
    }
    return ACR_OK;
}
