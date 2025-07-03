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
 * @file: acr_sanity_checks_gh100.c
 */
//
// Includes
//
#include "acr.h"
#include "dev_master.h"
#include "dev_fb.h"
#include "dev_se_seb.h"
#include "dev_se_seb_addendum.h"
#include "dev_fuse.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "dev_pri_ringstation_sys_addendum.h"

#include "acr_objacr.h"

#include "config/g_acr_private.h"

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
ACR_STATUS acrValidateSelwreLockHwStateCompatibilityWithACR_GH100(void)
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
acrGetMmuSelwreLockWprAllowPolicy_GH100
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

#ifndef ACR_FMODEL_BUILD
/*!
 * Verify is this build should be allowed to run on particular chip
 */
ACR_STATUS
acrCheckIfBuildIsSupported_GH100(void)
{
    LwU32 chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, ACR_REG_RD32(BAR0, LW_PMC_BOOT_42));

    if (chip == LW_PMC_BOOT_42_CHIP_ID_GH100)
    {
        return ACR_OK;
    }

    return  ACR_ERROR_ILWALID_CHIP_ID;
}
#endif // ACR_FMODEL_BUILD

/*!
 * @brief Get the ucode build version
 */
ACR_STATUS
acrGetUcodeBuildVersion_GH100(LwU32* pUcodeBuildVersion)
{
    if (!pUcodeBuildVersion)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    *pUcodeBuildVersion = ACR_GH100_UCODE_BUILD_VERSION;

    return ACR_OK;
}


/*
 * Verify if binary is running on expected falcon/engine. Volta onwards we have ENGID support
 */
ACR_STATUS
acrCheckIfEngineIsSupported_GH100(void)
{
    //
    // Check if binary is allowed on engine/falcon
    // GH100 ACR runs on GSP
    //
    LwU32 engId = DRF_VAL(_PRGNLCL, _FALCON_ENGID, _FAMILYID, ACR_REG_RD32(PRGNLCL, LW_PRGNLCL_FALCON_ENGID));
    if (engId != LW_PRGNLCL_FALCON_ENGID_FAMILYID_GSP)
    {
        return ACR_ERROR_BINARY_NOT_RUNNING_ON_EXPECTED_FALCON;
    }

    return ACR_OK;
}



/*!
 * @brief Check fuse revocation, make sure
 * ucode version is >= fpf fuse version
 */
ACR_STATUS
acrCheckFuseRevocation_GH100(void)
{
    // TODO: Define a compile time variable to specify this via make arguments
    LwU32 ucodeBuildVersion = 0, ucodeFpfFuseVersion = 0;
    ACR_STATUS  status = ACR_OK;

    // Read the version number from FPF FUSE
    status = acrGetUcodeFpfFuseVersion_HAL(pAcr, &ucodeFpfFuseVersion);
    if (status != ACR_OK)
    {
        return ACR_ERROR_ILWALID_UCODE_FUSE;
    }

    // Get SW defined ucode build version
    status = acrGetUcodeBuildVersion_HAL(pAcr, &ucodeBuildVersion);
    if (status != ACR_OK)
    {
        return ACR_ERROR_UCODE_DOESNT_HAVE_VERSION;
    }

    if (ucodeBuildVersion < ucodeFpfFuseVersion)
    {
        // Halt if this ucode is not supposed to run in this chip
        return ACR_ERROR_UCODE_REVOKED;
    }

    return status;
}



/*!
 * @brief Get the HW fuse version
 */
ACR_STATUS
acrGetUcodeFpfFuseVersion_GH100(LwU32* pUcodeFpfFuseVersion)
{
    LwU32 count = 0;
    LwU32 fpfFuse = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_UCODE_ACR_HS_REV);

    fpfFuse = DRF_VAL( _FUSE, _OPT_FPF_UCODE_ACR_HS_REV, _DATA, fpfFuse);

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

    *pUcodeFpfFuseVersion = count;

    return ACR_OK;
}

/*!
 * @brief Function that checks if priv sec is enabled on prod board
 */
ACR_STATUS
acrCheckIfPrivSecEnabledOnProd_GH100(void)
{

    if ( FLD_TEST_DRF( _PRGNLCL, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED, ACR_REG_RD32( PRGNLCL, LW_PRGNLCL_SCP_CTL_STAT ) ) )
    {
        if ( FLD_TEST_DRF( _FUSE, _OPT_PRIV_SEC_EN, _DATA, _NO, ACR_REG_RD32( BAR0, LW_FUSE_OPT_PRIV_SEC_EN ) ) )
        {
            // Error: Priv sec is not enabled
            return ACR_ERROR_PRIV_SEC_NOT_ENABLED;
        }
    }
    return ACR_OK;
}

/*!
 * @brief Function Check for exceptions before running task
 */
ACR_STATUS
acrCheckRiscvExceptions_GH100(void)
{
    LwU32  regval;

    // TODO: Move the exception error code register to SELWRE_SCRATCH Register
    regval = ACR_REG_RD32(PRGNLCL, LW_PRGNLCL_FALCON_COMMON_SCRATCH_GROUP_0(0));

    if (regval == ACR_ERROR_RISCV_EXCEPTION_FAILURE ||
        regval == ACR_ERROR_PREVIOUS_COMMAND_FAILED)
    {
        return (ACR_STATUS)regval;
    }

    return ACR_OK;
}

/*!
 * @brief: Function Checks if GSP engine reset PLM and source isolation bit are correctly setup
 */
ACR_STATUS
acrCheckEngineResetPlms_GH100(void)
{
    LwU32  regval;

    // check if FALCON_RESET PLM set to L3 and source enable bit set to GSP.
    regval = ACR_REG_RD32(PRGNLCL, LW_PRGNLCL_FALCON_RESET_PRIV_LEVEL_MASK);
    if ((!FLD_TEST_DRF( _PRGNLCL, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ONLY_LEVEL3_ENABLED, regval)) || 
       (DRF_VAL(_PRGNLCL, _FALCON_RESET_PRIV_LEVEL_MASK, _SOURCE_ENABLE, regval) != BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP)))
    {
        return ACR_ERROR_WRONG_RESET_PLM;
    }

    // check if RISCV_CPUCTL PLM set to L3 and source enable bit set to GSP.
    regval = ACR_REG_RD32(PRGNLCL, LW_PRGNLCL_RISCV_CPUCTL_PRIV_LEVEL_MASK);
    if ((!FLD_TEST_DRF( _PRGNLCL, _RISCV_CPUCTL_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ONLY_LEVEL3_ENABLED, regval)) || 
       (DRF_VAL(_PRGNLCL, _RISCV_CPUCTL_PRIV_LEVEL_MASK, _SOURCE_ENABLE, regval) != BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP)))
    {
        return ACR_ERROR_WRONG_RESET_PLM;
    }

    // check if FALCON_EXE PLM set to L3 and source enable bit set to GSP.
    regval = ACR_REG_RD32(PRGNLCL, LW_PRGNLCL_FALCON_EXE_PRIV_LEVEL_MASK);
    if ((!FLD_TEST_DRF( _PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ONLY_LEVEL3_ENABLED, regval)) || 
       (DRF_VAL(_PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _SOURCE_ENABLE, regval) != BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP)))
    {
        return ACR_ERROR_WRONG_RESET_PLM;
    }

    return ACR_OK;
}
