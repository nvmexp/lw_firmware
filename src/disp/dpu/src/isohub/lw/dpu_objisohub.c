/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_objisohub.c
 * @brief  Container-object for the DPU Isohub routines.  Contains
 *         generic non-HAL interrupt-routines plus logic required to hook-up
 *         chip-specific HAL-routines.
 *
 */

/* ------------------------ System Includes -------------------------------- */
#include "dpusw.h"

/* ------------------------ Application Includes --------------------------- */
#include "dpu_objhal.h"
#include "dpu_objisohub.h"

#include "config/g_isohub_private.h"
#if(ISOHUB_MOCK_FUNCTIONS_GENERATED)
#include "config/g_isohub_mock.c"
#endif
/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
ISOHUB_HAL_IFACES IsohubHal;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

/*!
 * Construct the ISOHUB object.  This includes the HAL interface as well as all
 * software objects used by the ISOHUB module.
 */
void
constructIsohub(void)
{
    IfaceSetup->isohubHalIfacesSetupFn(&IsohubHal);
}
