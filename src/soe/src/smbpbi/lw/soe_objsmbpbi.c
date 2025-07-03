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
 * @file   soe_objsmbpbi.c
 * @brief  Container-object for SOE SMBPBI routines. Contains generic non-HAL
 *         interrupt-routines plus logic required to hook-up chip-specific
 *         interrupt HAL-routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objhal.h"
#include "soe_objsmbpbi.h"
#include "soe_objsoe.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

/*!
* Main structure for all SMBPBI data.
*/
OBJSMBPBI Smbpbi;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct the SMBPBI object.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructSmbpbi(void)
{
    memset(&Smbpbi, 0, sizeof(OBJSMBPBI));
    IfaceSetup->smbpbiHalIfacesSetupFn(&Smbpbi.hal);
    return FLCN_OK;
}

