/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_VPR_H
#define SEC2_VPR_H

#include "unit_dispatch.h"
#include "mmu/mmucmn.h"

/* ------------------------ System includes -------------------------------- */
typedef union
{
    LwU8          eventType;
    DISP2UNIT_CMD cmd;
} DISPATCH_VPR;

typedef struct
{
    LwU8   sec2FlcnResetMask;
    LwU8   gc6BsiCtrlMask;
    LwU8   gc6SciMastMask;
    LwBool areMasksRaised;
} VPR_PRIV_MASKS_RESTORE, *PVPR_PRIV_MASKS_RESTORE;

#endif // SEC2_VPR_H
