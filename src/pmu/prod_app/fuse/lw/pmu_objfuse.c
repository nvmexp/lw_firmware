/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_objfuse.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "pmu_objhal.h"
#include "pmu_objfuse.h"

OBJFUSE Fuse;

FLCN_STATUS
constructFuse(void)
{
    return FLCN_OK;
}

#if (PMUCFG_FEATURE_ENABLED(PMU_ALTER_JTAG_CLK_AROUND_FUSE_ACCESS))
/*!
 * Wrappers for @ref fuseAccess_HAL.
 */
LwU32
fuseRead
(
    LwU32 fuseReg
)
{
    LwU32 fuseVal;

    fuseAccess_HAL(&Fuse, fuseReg, &fuseVal, LW_TRUE);

    return fuseVal;
}

/*!
 * Wrappers for @ref fuseAccess_HAL.
 */
void
fuseWrite
(
    LwU32 fuseReg,
    LwU32 fuseVal
)
{
    fuseAccess_HAL(&Fuse, fuseReg, &fuseVal, LW_FALSE);
}
#endif
