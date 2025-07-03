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
 * @file   sec2_prmpkandcerttu116.c
 * @brief  Contains all PlayReady MPK and certificate related routines specific
 *         to SEC2 TU116/TU117.
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_hs.h"
#include "pr/pr_common.h"
#include "lwosselwreovly.h"

#include "config/g_pr_private.h"

/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * @brief  Ensure PR is not running with revoked ACR, scrubber, UDE, GSP ucodes
 *
 * @return FLCN_OK if there is no revoked ucode
 *         FLCN_ERR_HS_PR_MPK_DEC_NEEDS_NEWER_ACR_UDE_SCRUBBER if there is any revoked ucode
 *         Or bubble up the error code returned from the callee
 */
FLCN_STATUS
prCheckDependentBinMilwersion_TU116(void)
{
    FLCN_STATUS  flcnStatus      = FLCN_ERR_ILLEGAL_OPERATION;
    LwU32        lwrrAcrVer      = 0;
    LwU32        lwrrUdeVer      = 0;
    LwU32        lwrrScrubberVer = 0;
    LwU32        lwrrGspVer      = 0;
    LwU32        minAcrVer       = 0xFFFFFFFF;
    LwU32        minUdeVer       = 0xFFFFFFFF;
    LwU32        minScrubberVer  = 0xFFFFFFFF;
    LwU32        minGspVer       = 0xFFFFFFFF;
    LwU32        tmp             = 0;

    // Read the current versions of ACR, UDE, scrubber and GSP
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_BSI_SELWRE_SCRATCH_14, &tmp));
    lwrrAcrVer = DRF_VAL(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _ACR_BINARY_VERSION, tmp);

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_BSI_SELWRE_SCRATCH_15, &tmp));
    lwrrUdeVer      = DRF_VAL(_PGC6, _BSI_VPR_SELWRE_SCRATCH_15, _VBIOS_UDE_VERSION, tmp);
    lwrrScrubberVer = DRF_VAL(_PGC6, _BSI_VPR_SELWRE_SCRATCH_15, _SCRUBBER_BINARY_VERSION, tmp);

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_04_00, &tmp));
    lwrrGspVer = DRF_VAL(_PGC6, _AON_SELWRE_SCRATCH_GROUP_04_00, _GSP_UCODE_VERSION, tmp);

    //
    // Compute the min required versions for ACR, UDE, scrubber and GSP
    //
    // The numbers have been obtained as follows
    // ACR: Adding one to the values found at https://rmopengrok.lwpu.com/source/xref/r400_00/uproc/acr/src/acr/turing/acructu116.c#acrGetUcodeBuildVersion_TU116 as of 10/26/2018
    // UDE: Looking up fuse.lwpu.com for fuse opt_fuse_ucode_gfw_rev and picking the least value across all shipping SKUs for each chip.
    //      Even if SW version was bumped after this, unless we can force update vbios in the field, we are stuck to lowest shipping value
    // Scrubber: Adding one to the values found at https://rmopengrok.lwpu.com/source/s?refs=SELWRESCRUB_HS_UCODE_VERSION_TU116&project=r400_00 as of 10/26/2018
    // GSP: Adding one to the values found at https://rmopengrok.lwpu.com/source/s?refs=GSP_TU116_UCODE_VERSION&project=r400_00 as of 10/26/2018
    //

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &tmp));

    tmp = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, tmp);
    switch(tmp)
    {
        case LW_PMC_BOOT_42_CHIP_ID_TU116:
        case LW_PMC_BOOT_42_CHIP_ID_TU117:
        {
            minAcrVer      = 2;
            minUdeVer      = 1;
            minScrubberVer = 1;
            minGspVer      = 1;
            break;
        }
    }

    // Ensure that ACR, UDE, scrubber and gsplite meet the min version criteria
    if ((lwrrAcrVer < minAcrVer) ||
        (lwrrUdeVer < minUdeVer) ||
        (lwrrScrubberVer < minScrubberVer) ||
        (lwrrGspVer < minGspVer))
    {
        flcnStatus = FLCN_ERR_HS_PR_MPK_DEC_NEEDS_NEWER_ACR_UDE_SCRUBBER;
        goto ErrorExit;
    }

ErrorExit:
    return flcnStatus;
}

