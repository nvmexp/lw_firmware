/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_vprtu10xonly.c
 * @brief  VPR HAL Functions for TU10X
 *
 * VPR HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "flcntypes.h"
#include "sec2sw.h"
#include "dev_disp.h"
#include "dev_fuse.h"
#include "dev_fb.h"
#include "dev_master.h"
/* ------------------------- System Includes -------------------------------- */
#include "csb.h"
#include "flcnretval.h"
#include "sec2_bar0_hs.h"
#include "dev_sec_pri.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objvpr.h"
#include "config/g_vpr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Private Functions ------------------------------ */


/*!
 * @brief: Checks whether VPR app is running on Falcon core or not
 */
FLCN_STATUS
vprCheckIfRunningOnFalconCore_TU10X(void)
{
    FLCN_STATUS flcnStatus  = FLCN_ERR_VPR_APP_UNEXPECTEDLY_RUNNING_ON_RISCV;
    LwU32 flcnActiveStatus  = 0;
    LwU32 riscvActiveStatus = 0;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(
        LW_PSEC_RISCV_CORE_SWITCH_FALCON_STATUS, &flcnActiveStatus));

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(
        LW_PSEC_RISCV_CORE_SWITCH_RISCV_STATUS, &riscvActiveStatus));

    if(FLD_TEST_DRF(_PSEC, _RISCV_CORE_SWITCH_FALCON_STATUS,
        _ACTIVE_STAT, _ACTIVE, flcnActiveStatus) &&
        !FLD_TEST_DRF(_PSEC, _RISCV_CORE_SWITCH_RISCV_STATUS,
        _ACTIVE_STAT, _ACTIVE, riscvActiveStatus))
    {
        flcnStatus = FLCN_OK;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * Check Display IP version
 */
FLCN_STATUS
vprIsDisplayIpVersionSupported_TU10X(void)
{
    LwU32       ipVersion;
    FLCN_STATUS flcnStatus = FLCN_ERR_VPR_APP_DISPLAY_VERSION_NOT_SUPPORTED;
    LwU32       majorVer;
    LwU32       minorVer;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PDISP_FE_IP_VER, &ipVersion));
    majorVer = DRF_VAL(_PDISP, _FE_IP_VER, _MAJOR, ipVersion);
    minorVer = DRF_VAL(_PDISP, _FE_IP_VER, _MINOR, ipVersion);

    if ((majorVer == LW_PDISP_FE_IP_VER_MAJOR_INIT) &&
        (minorVer >= LW_PDISP_FE_IP_VER_MINOR_INIT))
    {
        flcnStatus = FLCN_OK;
    }

ErrorExit:
    return flcnStatus;
}
