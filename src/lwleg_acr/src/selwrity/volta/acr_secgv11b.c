/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_secgv11b.c
 */
//
// Includes
//
#include "acr.h"
#include "dev_master.h"

#include "acr_objacr.h"
#include "dev_fuse.h"
#include "dev_pwr_pri.h"
#include "dev_pwr_csb.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

/*! Global variable for storing the original falcon reset mask */
static LwU8    g_resetPLM      ATTR_OVLY(".data");

/*! GV11B ACR UCODE SW VERSION */
#define ACR_GV11B_UCODE_VERSION    (0x3)

/*!
 *
 * @brief Get the SW fuse version of ACR ucode from the SW VERSION MACRO defined. This MACRO is updated(revision increased) with important security fixes.
 * @param[out] fuseVersion ptr to return SW version value to callee.
 *
 */
void
acrGetFuseVersionSW_GV11B(LwU32* fuseVersion)
{
    *fuseVersion = ACR_GV11B_UCODE_VERSION;
}

/*!
 * @brief Get the HW version from FUSE register
 *
 * @param[out] fuseVersion ptr to HwfuseVersion variable to return HW version value to callee.
 *
 */
void
acrGetFuseVersionHW_GV11B(LwU32* fuseVersion)
{
    *fuseVersion = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FUSE_UCODE_ACR_HS_REV);
}

/*!
 * @brief Write to the hardware lock bit to lock/unlock falcon reset. This helps to prevent reset happening while HS ucode is running.\n
 *        This is a security feature to avoid any interruption to falcon while HS ucode is exelwting.
 *
 * @param[in] bLock      Bool to specify if callee wants to lock or unlock the falcon reset.
 * @return    ACR_OK     If falcon reset lock/unlock operation is successful.
 */
ACR_STATUS
acrLockFalconReset_GV11B(LwBool bLock)
{
    LwU32 reg = 0;

    //
    // We are typecasting WRITE_PROTECTION field to LwU8 below but since the size of that field is controlled by hw,
    // the ct_assert below will help warn us if our assumption is proven incorrect in future
    //
    ct_assert((sizeof(g_resetPLM) * 8) >= DRF_SIZE(LW_CPWR_FALCON_RESET_PRIV_LEVEL_MASK_WRITE_PROTECTION));

    reg = ACR_REG_RD32_STALL(CSB, LW_CPWR_FALCON_RESET_PRIV_LEVEL_MASK);
    if (bLock)
    {
        // Lock falcon reset for level 0, 1, 2
        g_resetPLM = (LwU8)(DRF_VAL( _CPWR, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION, reg));
        reg = FLD_SET_DRF( _CPWR, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ALL_LEVELS_DISABLED, reg);
    }
    else
    {
        // Restore original mask 
        reg = FLD_SET_DRF_NUM( _CPWR, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION, g_resetPLM, reg);
    }

    ACR_REG_WR32_STALL(CSB, LW_CPWR_FALCON_RESET_PRIV_LEVEL_MASK, reg);    

    return ACR_OK;
}

/*!
 * @brief Verify CHIP ID is same as intended.\n
 *        This is a security feature so as to constrain a binary for a particular chip.
 *
 * @return ACR_OK If chip-id is as same as intended for your build.
 * @return ACR_ERROR_ILWALID_CHIP_ID If chip-id is not same as intended.
 */
ACR_STATUS
acrCheckIfBuildIsSupported_GV11B(void)
{
    LwU32 chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, ACR_REG_RD32(BAR0, LW_PMC_BOOT_42));

    if (chip == LW_PMC_BOOT_42_CHIP_ID_GV11B)
    {
        return ACR_OK;
    }
    return  ACR_ERROR_ILWALID_CHIP_ID;
}

/*!
 * @brief Check fuse for rollback protection
 *        Gets HW version from Fuse.\n
 *        Gets SW vesion of ACR ucode.(defined as macro).\n
 *        Check if SW version is less than HW version. If yes, it means SW binary is missing critical security fixes so should not be allowed to execute.\n
 * @return   ACR_OK  If SW version of the binary is greater than HW version.
 * @return   ACR_ERROR_UCODE_REVOKED  If SW version of the binary is less than HW version.

 */
ACR_STATUS
acrCheckFuseRevocation_GV11B(void)
{
    // TODO: Define a compile time variable to specify this via make arguments
    LwU32       fuseVersionHW = 0, fuseVersionSW = 0;
    ACR_STATUS  status = ACR_OK;

    // Read the version number from FUSE
    acrGetFuseVersionHW_HAL(pAcr, &fuseVersionHW);

    acrGetFuseVersionSW_HAL(pAcr, &fuseVersionSW);

    if (fuseVersionSW < fuseVersionHW)
    {
        // Halt if this ucode is not supposed to run in this chip
        return ACR_ERROR_UCODE_REVOKED;
    }

    // Write ACR version to the version bits reserved for ACR version.
    status = acrWriteAcrVersionToBsiSelwreScratch_HAL(pAcr, fuseVersionSW);

    return status;
}

//
