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
 * @file   pmu_didlegk10x.c
 * @brief  HAL-interface for the GFxxx Deepidle Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_didle.h"
#include "pmu_objms.h"

#include "config/g_gcx_private.h"

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief This function attaches the Didle core sequence to GCX task
 */
void
gcxDidleAttachCoreSequence_GMXXX(void)
{
    OSTASK_ATTACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libSwAsr));
}

/*!
 * @brief This function detaches the Didle core sequence from GCX task
 */
void
gcxDidleDetachCoreSequence_GMXXX(void)
{
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libSwAsr));
}


/* ------------------------ Private Functions ------------------------------- */

