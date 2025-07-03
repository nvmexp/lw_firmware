/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifndef SOE_OBJIFR_H
#define SOE_OBJIFR_H

/*!
 * @file    soe_objifr.h
 * @copydoc soe_objifr.c
 */

/* ------------------------ System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------ Application Includes --------------------------- */
#include "unit_dispatch.h"
#include "inforom_fs.h"
#include "inforom/block.h"
#include "config/soe-config.h"
#include "config/g_ifr_hal.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Global Variables ------------------------------- */
extern IFR_HAL_IFACES IfrHal;
LwrtosQueueHandle Disp2QIfrThd;
IFS *g_pFs;
LwBool g_isInforomInitialized;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
//
// Bug 2905068: Since preOS is running out of memory to load all non-clean
// blocks and preOS updates may not roll out to all lwstomers, increase the
// GC sector threshold so that the InfoROM contains more clean blocks at all
// times leading to less memory allocation in preOS.
//
// INFOROM_FS_GC_SYNC_THRESHOLD_GA100 was callwlated based on the available
// heap space observed for this issue. See Bug 2905068 for details.
//
#define INFOROM_FS_GC_SYNC_THRESHOLD_LR10 (14)
/* ------------------------ Public Functions ------------------------------- */
void constructIfr(void) GCC_ATTRIB_SECTION("imem_init", "constructIfr");
void ifrPreInit(void)   GCC_ATTRIB_SECTION("imem_init", "ifrPreInit");
FLCN_STATUS initInforomFs(void) GCC_ATTRIB_SECTION("imem_resident", "initInforomFs");

#endif // SOE_OBJIFR_H
