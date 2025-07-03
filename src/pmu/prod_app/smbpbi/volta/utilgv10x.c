/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2018  by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    utilgv10x.c
 * @brief   PMU HAL functions for GV10X+, handling SMBPBI queries for
 *          util info.
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_objsmbpbi.h"
#include "dev_bus.h"
#include "dev_bus_addendum.h"

/* ------------------------ Application Includes --------------------------- */
#include "pmu_bar0.h"

#include "config/g_smbpbi_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Function Prototypes  -------------------- */
/* ------------------------ Static Function Prototypes  -------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * @brief   Get the utilization scratch register offset
 *
 * @param[in]   counter   Utilization register counter
 * @param[out]  poffset   Utilization register counter offset value
 *
 * @return  FLCN_OK
 */
FLCN_STATUS
smbpbiGetUtilCounterOffset_GV10X
(
    LwU32 counter,
    LwU32 *pOffset
)
{
    FLCN_STATUS status = FLCN_OK;

    switch (counter)
    {
        case PMU_SMBPBI_UTIL_COUNTERS_CONTEXT:
        {
            *pOffset = LW_PBUS_SW_SCRATCH_GPU_UTIL_COUNTERS_CONTEXT;
            break;
        }
        case PMU_SMBPBI_UTIL_COUNTERS_SM:
        {
            *pOffset =  LW_PBUS_SW_SCRATCH_GPU_UTIL_COUNTERS_SM;
            break;
        }
        default:
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
    }

    return status;
}
