/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_sec2gp10xga10x.c
 * @brief  SEC2 HAL Functions for GP10X thru GA10x 
 *
 * SEC2 HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "dev_fuse.h"
#include "sec2_bar0_hs.h"
#include "dev_lw_xve.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Check the SW fuse version revocation against HW fuse version
 */
FLCN_STATUS
sec2CheckFuseRevocationAgainstHWFuseVersionHS_GP10X(LwU32 fuseVersionSW)
{
    FLCN_STATUS flcnStatus    = FLCN_ERROR;
    LwU32       fuseVersionHW = 0;
    
    // Get HW Fuse Version
    CHECK_FLCN_STATUS(sec2GetHWFuseVersionHS_HAL(&Sec2, &fuseVersionHW));

    //
    // Compare SW Fuse Version against HW Fuse Version
    // return error if SW Fuse version is lower
    //
    if (fuseVersionSW < fuseVersionHW)
    {
        return FLCN_ERR_HS_CHK_UCODE_REVOKED;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * Reads DEVICE ID in HS mode
 */
FLCN_STATUS
sec2ReadDeviceIdHs_GP10X(LwU32 *pDevId)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       devid      = 0;

    if (pDevId == NULL)
    {
        flcnStatus = FLCN_ERR_ILLEGAL_OPERATION;
        goto ErrorExit;
    }

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(DEVICE_BASE(LW_PCFG) + LW_XVE_ID, &devid));
    *pDevId = DRF_VAL(_XVE, _ID, _DEVICE_CHIP, devid);
    devid = 0;

ErrorExit:
    return flcnStatus;
}

