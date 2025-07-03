/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_ACR_H
#define SEC2_ACR_H

/* ------------------------ System includes -------------------------------- */
#include "unit_dispatch.h"

/*!
 * A union of all available commands to ACR.
 */
typedef union
{
    LwU8             eventType;
    // Generic CMD from RM.
    DISP2UNIT_CMD    cmd;
} DISPATCH_ACR;

#endif //SEC2_ACR_H

