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
 * @file    pmu_ssurface.c
 * @copydoc pmu_ssurface.h
  */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu/ssurface.h"

/* ------------------------- External Definitions --------------------------- */
extern RM_PMU_CMD_LINE_ARGS PmuInitArgs;

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc ssurfaceRd
 */
FLCN_STATUS
ssurfaceRd(void *pBuf, LwU32 offset, LwU32 numBytes)
{
    FLCN_STATUS status;

    status = dmaRead(pBuf, &(PmuInitArgs.superSurface), offset, numBytes);

    return status;
}

/*!
 * @copydoc ssurfaceWr
 */
FLCN_STATUS
ssurfaceWr(void *pBuf, LwU32 offset, LwU32 numBytes)
{
    FLCN_STATUS status;

    status = dmaWrite(pBuf, &(PmuInitArgs.superSurface), offset, numBytes);

    return status;
}

/* ------------------------- Private Functions ------------------------------ */

