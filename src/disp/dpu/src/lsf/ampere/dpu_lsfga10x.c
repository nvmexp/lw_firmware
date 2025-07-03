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
 * @file   dpu_lsfga10x.c
 * @brief  Light Secure Falcon (LSF) for Ampere Hal Functions
 *
 * LSF Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "dpusw.h"
#include "gsp_csb.h"
#include "dpu_objdpu.h"

/* ------------------------ Application includes --------------------------- */
#include "config/g_lsf_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */
static FLCN_STATUS _checkIfFalconIsGSP(void);

/*!
 * @brief  Checks if the falcon on which it is exelwting is GSP/GSPLite or not. 
 *
 * @return FLCN_OK  If it is GSP. 
 *         FLCN_ERR_HS_CHK_ENGID_MISMATCH If is not GSP/GSPLite.
 * 
 */
static FLCN_STATUS 
_checkIfFalconIsGSP(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       data32     = 0;

    // Check if current falcon is GSP
    CHECK_FLCN_STATUS(CSB_REG_RD32_ERRCHK(LW_CGSP_FALCON_ENGID, &data32));
    if (LW_CGSP_FALCON_ENGID_FAMILYID_GSP != DRF_VAL(_CGSP, _FALCON_ENGID, _FAMILYID, data32))
    {
        flcnStatus = FLCN_ERR_HS_CHK_ENGID_MISMATCH;
        goto ErrorExit;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief  Check for LS revocation by comparing LS ucode version against GSP rev in HW fuse 
 *
 * @return FLCN_OK  if HW fuse version and LS ucode version matches 
 *         FLCN_ERR_LS_CHK_UCODE_REVOKED  if mismatch
 * 
 */
static FLCN_STATUS 
_checkForLSRevocation(void)
{
    FLCN_STATUS flcnStatus    = FLCN_ERR_LS_CHK_UCODE_REVOKED;
    LwU32       fuseVersionHW = 0;
    LwU32       fpfVersionHW  = 0;

    // Get the Version Number From Fuse  
    CHECK_FLCN_STATUS(dpuGetHWFuseVersion_HAL(&Dpu.hal, &fuseVersionHW));

    // Get the version number from FPF fuse
    CHECK_FLCN_STATUS(dpuGetHWFpfVersion_HAL(&Dpu.hal, &fpfVersionHW));

    if ((GSP_LS_UCODE_VERSION < fpfVersionHW) ||
        (GSP_LS_UCODE_VERSION < fuseVersionHW))
    {
        return FLCN_ERR_LS_CHK_UCODE_REVOKED;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Initializes Light Secure Falcon settings.
 *
 * @returns     FLCN_STATUS   FLCN_OK on successfull exelwtion
 *                            Appropriate error status on failure.
 */
FLCN_STATUS
lsfInit_v04_01(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Ensure that chip on which it is running is allowed chip
    dpuEnforceAllowedChipsForHdcp_HAL(&Dpu.hal);

    // Check if current falcon is GSP
    CHECK_FLCN_STATUS(_checkIfFalconIsGSP());

    // Check For LS Revocation with GSP HW Rev Fuses
    CHECK_FLCN_STATUS(_checkForLSRevocation());

    //
    // After we are done with GA10X specific stuff, call v02_05 version
    // common initialization which is common between GA10x and v02_05.
    //
    CHECK_FLCN_STATUS(lsfInit_v02_05());

ErrorExit:
    return flcnStatus;
}

