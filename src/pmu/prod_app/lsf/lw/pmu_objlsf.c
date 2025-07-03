/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_objlsf.c
 * @brief  Container-object for the PMU Light Secure Falcon routines.
 */

/* ------------------------ System Includes -------------------------------- */
//#include "pmusw.h"

/* ------------------------ Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmu.h"
#include "pmu_objlsf.h"
#include "main_init_api.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
OBJLSF Lsf;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
FLCN_STATUS
constructLsf_IMPL(void)
{
    // Initializing way ahead of other engines since lsfInit unblocks FB access.
    return lsfInit_HAL(&(Lsf.hal));
}
