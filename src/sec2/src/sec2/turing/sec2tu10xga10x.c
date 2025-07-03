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
 * @file   sec2tu10xga10x.c
 * @brief  SEC2 HAL Functions for TU10x thru GA10x 
 *
 * SEC2 HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "dev_fuse.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_bar0_hs.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Get the HW fuse version
 * Please refer to Bug 200333620 for detailed info
 */
FLCN_STATUS
sec2GetHWFuseVersionHS_TU10X
(
    LwU32* pFuseVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
    if (!pFuseVersion)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_FUSE_UCODE_SEC2_REV, pFuseVersion));
    *pFuseVersion = DRF_VAL(_FUSE, _OPT_FUSE_UCODE_SEC2_REV, _DATA, *pFuseVersion);

ErrorExit:
    return flcnStatus;
}
