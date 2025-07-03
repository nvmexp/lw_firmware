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
 * @file: acr_bsi_lock.c
 */

//
// Includes
//
#include "acr.h"
#include "dev_fb.h"
#include "dev_pwr_pri.h"
#include "dev_pwr_falcon_csb.h"
#include "dev_pwr_csb.h"
#ifdef SEC2_PRESENT
#include "dev_sec_csb.h"
#endif
#include "dev_fuse.h"
#include "dev_master.h"
#include "acr_objacr.h"
#include "acr_objacrlib.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

//
// Global Variables
//

//
// Extern Variables
//
extern LwU32   _acr_code_start[] ATTR_OVLY(".data");
extern LwBool  g_bIsDebug;

#ifdef ACR_BSI_PHASE
/*!
 * @brief ACR BSI lock routine
 */
ACR_STATUS acrBsiPhase(void)
{
    ACR_STATUS   status     = ACR_OK;

    // Make sure that this binary is called in GC6 exit path
    if((status = acrCheckIfGc6ExitIndeed_HAL(pAcr)) != ACR_OK)
    {
        goto Cleanup;
    }

    if((status = acrVerifyBsiPhase_HAL(pAcr)) != ACR_OK)
    {
        goto Cleanup;
    }

    if ((status = acrProgramHubEncryption_HAL(pAcr)) != ACR_OK)
    {
        goto Cleanup;
    }

    if((status = acrVerifyAcrBsiLockToGC6UDEHandoffIsReset_HAL()) != ACR_OK)
    {
        goto Cleanup;
    }

    // No need ACRLIB, so passing it as NULL
    LwU32        wprIndex   = 0;
    if ((status = acrLockAcrRegions_HAL(pAcr, &wprIndex)) != ACR_OK)
    {
        goto Cleanup;
    }

    // Restore subWpr settings during GC6 exit
    if ((status = acrRestoreSubWprsOnResumeFromGC6_HAL(pAcr)) != ACR_OK)
    {
        goto Cleanup;
    }

    if ((status = acrProgramMemoryRanges_HAL(pAcr))  != ACR_OK)
    {
        goto Cleanup;
    }

    // Release global memory lock
    if ((status = acrReleaseGlobalMemoryLock_HAL(pAcr))  != ACR_OK)
    {
        goto Cleanup;
    }

    if((status = acrProgramAcrBsiLockToGC6UDEHandoff_HAL()) != ACR_OK)
    {
        goto Cleanup;
    }

Cleanup:
    return status;
}
#endif // ACR_BSI_PHASE

