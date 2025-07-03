/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_ecidgf10x.c
 * @brief  Contains access ECID routines specific to GF10x.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_fuse.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_bar0.h"
#include "pmu_objfuse.h"

#include "config/g_ecid_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Implementation --------------------------------- */


/*!
 * This function read the GPU ECID
 *
 * @param[out] pEcidData    This pointor contains the ECID information
 *
 * @return  'FLCN_OK'    If the ECID was retrieved successfully
 */
FLCN_STATUS
ecidGetData_GM10X
(
    LwU64 *pEcidData
)
{
    LwU32 lotCode0;
    LwU32 lotCode1;
    LwU32 fabCode;
    LwU32 waferId;
    LwU32 vendorCode;
    LwU32 xcoord;
    LwU32 ycoord;
    LwU32 fuse;
    LwU32 offset;

    fuse        = fuseRead(LW_FUSE_OPT_LOT_CODE_0);
    lotCode0    = DRF_VAL(_FUSE, _OPT_LOT_CODE_0, _DATA, fuse);

    fuse        = fuseRead(LW_FUSE_OPT_LOT_CODE_1);
    lotCode1    = DRF_VAL(_FUSE, _OPT_LOT_CODE_1, _DATA, fuse);

    fuse        = fuseRead(LW_FUSE_OPT_FAB_CODE);
    fabCode     = DRF_VAL(_FUSE, _OPT_FAB_CODE, _DATA, fuse);

    fuse        = fuseRead(LW_FUSE_OPT_WAFER_ID);
    waferId     = DRF_VAL(_FUSE, _OPT_WAFER_ID, _DATA, fuse);

    fuse        = fuseRead(LW_FUSE_OPT_VENDOR_CODE);
    vendorCode  = DRF_VAL(_FUSE, _OPT_VENDOR_CODE, _DATA, fuse);

    fuse        = fuseRead(LW_FUSE_OPT_X_COORDINATE);
    xcoord      = DRF_VAL(_FUSE, _OPT_X_COORDINATE, _DATA, fuse);

    fuse        = fuseRead(LW_FUSE_OPT_Y_COORDINATE);
    ycoord      = DRF_VAL(_FUSE, _OPT_Y_COORDINATE, _DATA, fuse);

    //
    // Aggregate the components into the ECID
    //
    pEcidData[0] = (LwU64) vendorCode;
    offset = DRF_SIZE(LW_FUSE_OPT_VENDOR_CODE_DATA);

    pEcidData[0] |= (LwU64) lotCode0 << offset;
    offset += DRF_SIZE(LW_FUSE_OPT_LOT_CODE_0_DATA);

    pEcidData[0] |= (LwU64) lotCode1 << offset;

    pEcidData[1] = (LwU64) fabCode;
    offset = DRF_SIZE(LW_FUSE_OPT_FAB_CODE_DATA);

    pEcidData[1] |= (LwU64) waferId << offset;
    offset += DRF_SIZE(LW_FUSE_OPT_WAFER_ID_DATA);

    pEcidData[1] |= (LwU64) xcoord << offset;
    offset += DRF_SIZE(LW_FUSE_OPT_X_COORDINATE_DATA);

    pEcidData[1] |= (LwU64) ycoord << offset;
    offset += DRF_SIZE(LW_FUSE_OPT_Y_COORDINATE_DATA);

    return FLCN_OK;
}

