/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_objvrr.c
 * @brief  Container-object for the DPU VRR routines.  Contains
 *         generic non-HAL interrupt-routines plus logic required to hook-up
 *         chip-specific HAL-routines.
 *
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "dpu_objhal.h"
#include "dpu_objvrr.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
VRR_HAL_IFACES VrrHal;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

void
constructVrr(void)
{
    IfaceSetup->vrrHalIfacesSetupFn(&VrrHal);
}

