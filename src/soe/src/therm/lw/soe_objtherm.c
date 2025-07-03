/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/*!
 * @file   soe_objtherm.c
 * @brief  Container-object for SOE Thermal routines. Contains generic non-HAL
 *         interrupt-routines plus logic required to hook-up chip-specific
 *         interrupt HAL-routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objhal.h"
#include "soe_objtherm.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

/*!
* Main structure for all THERM data.
*/
OBJTHERM Therm;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct the THERM object.  This includes the HAL interface as well as all
 * software objects used by the THERM module.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructTherm(void)
{
    memset(&Therm, 0, sizeof(OBJTHERM));
    IfaceSetup->thermHalIfacesSetupFn(&Therm.hal);
    return FLCN_OK;
}

