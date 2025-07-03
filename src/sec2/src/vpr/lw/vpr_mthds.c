/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "vpr/sec2_vpr.h"
#include "sec2_objsec2.h"
#include "sec2_objvpr.h"
#include "sec2_hs.h"
#include "vpr/vpr_mthds.h"
#include "config/g_vpr_hal.h"
#include "config/g_sec2_hal.h"
#include "sec2_objhal.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Handler for VPR command RM_SEC2_VPR_CMD_SETUP_VPR
 *
 * @return
 *      FLCN_OK if the VPR command is exelwted successfully; otherwise, a
 *      detailed error code is returned.
 */
FLCN_STATUS
vprCommandProgramRegion
(
    LwU64 startAddr,
    LwU64 size
)
{
    FLCN_STATUS flcnStatus       = FLCN_OK;

    // Program region as per the request
    CHECK_FLCN_STATUS(vprProgramRegion_HAL(&VprHal, startAddr, size));

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief: Check if VPR is supported. This function is part of vprHs overlay
 */
FLCN_STATUS
vprIsSupportedHS()
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    CHECK_FLCN_STATUS(sec2HsPreCheckCommon_HAL(&Sec2, LW_FALSE));

    // Check for VPR HW fuse
    CHECK_FLCN_STATUS(vprIsAllowedInHWFuse_HAL(&VprHal));
#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
    CHECK_FLCN_STATUS(vprCheckIfApmIsSupported_HAL(void));
#endif

#if (!SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))
    //
    // Ensure the chip is allowed to do Playready
    // This step must always be step1. No BAR0 read must be done before this step
    // CSB reads are also discouraged and must be carefully reviewed before addition
    //
    CHECK_FLCN_STATUS(sec2EnforceAllowedChipsForPlayreadyHS_HAL(&Sec2));

    // Check if VPR app is running on falcon core (TU10X_and_later: Till we productize RISCV)
    CHECK_FLCN_STATUS(vprCheckIfRunningOnFalconCore_HAL(&VprHal));

    // Check for VPR SW fuse
    CHECK_FLCN_STATUS(vprIsAllowedInSWFuse_HAL(&VprHal));

    // Check if THERM and MMU PLM are at level3
    CHECK_FLCN_STATUS(vprCheckConcernedPLM_HAL(&VprHal));

    // Check for handoff from scrubber binary
    CHECK_FLCN_STATUS(vprCheckScrubberHandoff_HAL(&VprHal));

    // Check if display is present
    CHECK_FLCN_STATUS(vprIsDisplayPresent_HAL(&VprHal));

    // Check if display IP version is supported or not
    CHECK_FLCN_STATUS(vprIsDisplayIpVersionSupported_HAL(&VprHal));

    // Check if DISP falcon is in LS mode
    CHECK_FLCN_STATUS(vprCheckIfDispFalconIsInLSMode_HAL(&VprHal));
#endif // !SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM)

ErrorExit:
    return flcnStatus;
}
