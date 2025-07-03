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
 * @file   soe_objbif.c
 * @brief  Container-object for SOE Secure Bus InterFace routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objhal.h"
#include "soe_objbif.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

/*!
* Main structure for all BIF data.
*/
OBJBIF Bif;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct the BIF object.  This includes the HAL interface as well as all
 * software objects used by the Secure bif module.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructBif(void)
{
    memset(&Bif, 0, sizeof(OBJBIF));
    IfaceSetup->bifHalIfacesSetupFn(&Bif.hal);
    return FLCN_OK;
}