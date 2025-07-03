/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJLOWLATENCY_H
#define PMU_OBJLOWLATENCY_H

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"

/* ------------------------ Application includes --------------------------- */
#include "unit_api.h"

/* ------------------------ Defines ---------------------------------------- */

/*!
 * Event type for lane margining related interrupts
 */
#define LOWLATENCY_EVENT_ID_LANE_MARGINING_INTERRUPT (DISP2UNIT_EVT__COUNT + 0)

/* ------------------------ Types definitions ------------------------------ */
typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;
} DISPATCH_LOWLATENCY;

/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */

/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS constructLowlatency(void)
    GCC_ATTRIB_SECTION("imem_init", "constructLowlatency");

#endif // PMU_OBJLOWLATENCY_H
