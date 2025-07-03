/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/* ------------------------ System Includes -------------------------------- */
#include "soesw.h"
#include "soe_bar0.h"
#include "soe_objpmgr.h"
#include "soe_objifr.h"
#include "soe_objsaw.h"

/* ------------------------ Application Includes --------------------------- */
#include "config/g_ifr_hal.h"
#include "dev_pmgr.h"
#include "dev_ext_devices.h"
#include "dev_lwlsaw_ip.h"
#include "dev_lwlsaw_ip_addendum.h"

/* ------------------------- Static Variables ------------------------------- */

/* ------------------------- Prototypes ------------------------------------- */

/* ------------------------- Public Functions ------------------------------- */

/*!
 * Pre-STATE_INIT for IFR
 */
void
ifrPreInit_LR10(void)
{
}

FLCN_STATUS
ifrGetGcThreshold_LR10
(
    LwU32 *pGcSectorThreshold
)
{
    *pGcSectorThreshold = INFOROM_FS_GC_SYNC_THRESHOLD_LR10;

    return FLCN_OK;
}
