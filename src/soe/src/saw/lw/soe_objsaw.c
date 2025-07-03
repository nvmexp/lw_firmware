/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/*!
 * @file   soe_objsaw.c
 * @brief  Container-object for SOE SAW routines. Contains generic non-HAL
 *         interrupt-routines plus logic required to hook-up chip-specific
 *         interrupt HAL-routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objhal.h"
#include "soe_objsoe.h"
#include "soe_objsaw.h"
#include "soe_objdiscovery.h"
#include "dev_lws_top.h"

#include "config/g_saw_hal.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
SAW_HAL_IFACES SawHal;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */

void
constructSaw(void)
{
    sawBase = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_SAW, INSTANCE0, ADDRESS_UNICAST, 0);
    IfaceSetup->sawHalIfacesSetupFn(&SawHal);
}
