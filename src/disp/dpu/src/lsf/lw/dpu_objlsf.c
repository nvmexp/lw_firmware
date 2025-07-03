/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_objlsf.c
 * @brief  Container-object for the DPU Light Secure Falcon routines.
 */

/* ------------------------ System Includes -------------------------------- */
#include "dpusw.h"

/* ------------------------ Application Includes --------------------------- */
#include "dpu_objhal.h"
#include "dpu_objdpu.h"
#include "dpu_objlsf.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
LSF_HAL_IFACES LsfHal;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

void
constructLsf(void)
{
    IfaceSetup->lsfHalIfacesSetupFn(&LsfHal);
}
