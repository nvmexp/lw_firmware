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
 * @file: acr_sanity_checks_ga100_and_later.c
 */

//
// Includes
//
#include "acr.h"
#include "acr_objacr.h"
#include "acr_objacrlib.h"

#include "dev_fuse.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

#ifdef ACR_GSP_RM_BUILD
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#endif
/*!
 * @brief Get the HW fuse version
 */
ACR_STATUS
acrGetUcodeFpfFuseVersion_GA100(LwU32* pUcodeFpfFuseVersion)
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
 * @brief Check the HW fpf version
 */
ACR_STATUS
acrCheckFuseRevocationAgainstHWFpfVersion_GA100(LwU32 ucodeBuildVersion)
{
    ACR_STATUS status              = ACR_OK;
    LwU32      ucodeFpfFuseVersion = 0;

    // Read the version number from FPF FUSE
    status = acrGetUcodeFpfFuseVersion_HAL(pAcr, &ucodeFpfFuseVersion);
    if (status != ACR_OK)
    {
        return ACR_ERROR_ILWALID_UCODE_FUSE;
    }

    if (ucodeBuildVersion < ucodeFpfFuseVersion)
    {
        // Halt if this ucode is not supposed to run in this chip
        return ACR_ERROR_UCODE_REVOKED;
    }

    return ACR_OK;
}

#if ACR_GSP_RM_BUILD
/*!
 * @brief Check whether handoff from booter load binary / AHESASC NEW is successfully completed for pre-hopper GSP RM build.
 */
ACR_STATUS
acrAhesascGspRmCheckHandoff_GA100(void)
{
    LwU32 handoffValue = ACR_REG_RD32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14);

    if (!FLD_TEST_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _BOOTER_LOAD_HANDOFF, _VALUE_DONE, handoffValue))
    {
        return ACR_ERROR_AUTH_GSP_RM_HANDOFF_FAILED;
    }

    return ACR_OK;
}

/*!
 * @brief Check whether handoff from booter load , AHESASC & booter reload binary is successfully completed for pre-hopper GSP RM build.
 */
ACR_STATUS
acrUnloadGspRmCheckHandoff_GA100(void)
{
    LwU32 handoffValue = ACR_REG_RD32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14);

    // ACR Unload checks for handoff with BOOTER_LOAD, AHESASC & BOOTER_RELOAD
    if ((!FLD_TEST_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _BOOTER_LOAD_HANDOFF, _VALUE_DONE, handoffValue))           ||
        (!FLD_TEST_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _AHESASC_BOOTER_RELOAD_HANDOFF, _VALUE_DONE, handoffValue)) ||
        (!FLD_TEST_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _BOOTER_RELOAD_ACR_UNLOAD_HANDOFF, _VALUE_DONE, handoffValue)))
    {
        return ACR_ERROR_AUTH_GSP_RM_HANDOFF_FAILED;
    }

    return ACR_OK;
}

/*!
 * @brief Writes handoff from GSP-RM AHESASC to Booter Reload for pre-hopper GSP RM build.
 */
void
acrWriteAhesascGspRmHandoffValueToBsiSelwreScratch_GA100(void)
{

    LwU32 handoffValue = ACR_REG_RD32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14);
    handoffValue       = FLD_SET_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _AHESASC_BOOTER_RELOAD_HANDOFF, _VALUE_DONE, handoffValue);
    
    ACR_REG_WR32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14, handoffValue);

}

/*!
 * @brief Writes handoff from GSP-RM ACR unload to Booter Unload for pre-hopper GSP RM build.
 */
void
acrWriteAcrUnloadGspRmHandoffValueToBsiSelwreScratch_GA100(void)
{

    LwU32 handoffValue = ACR_REG_RD32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14);
    handoffValue       = FLD_SET_DRF(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _ACR_UNLOAD_BOOTER_UNLOAD_HANDOFF, _VALUE_DONE, handoffValue);

    ACR_REG_WR32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14, handoffValue);

}
#endif // ACR_GSP_RM_BUILD
