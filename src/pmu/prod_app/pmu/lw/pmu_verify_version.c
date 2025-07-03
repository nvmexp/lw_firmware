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
 * @file   pmu_verify_version.c
 * @brief  Code to verify that the version of the running ucode is supported.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc pmuVerifyVersion
 */
FLCN_STATUS
pmuVerifyVersion(void)
{
    FLCN_STATUS status;

    if ((LS_UCODE_VERSION < pmuFuseVersionGet_HAL()) ||
        (LS_UCODE_VERSION < pmuFpfVersionGet_HAL()))
    {
        PMU_HALT();
        status = FLCN_ERR_LS_CHK_UCODE_REVOKED;
        goto pmuVerifyVersion_exit;
    }

    status = FLCN_OK;

pmuVerifyVersion_exit:
    return status;
}
