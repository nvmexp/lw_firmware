/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJECID_H
#define PMU_OBJECID_H

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"

/* ------------------------ Application includes --------------------------- */
#include "config/g_ecid_hal.h"

/* ------------------------ Types definitions ------------------------------ */

/* ------------------------ External definitions --------------------------- */
/*!
 * ECID object Definition
 */
typedef struct
{
    LwU8    dummy;    // unused -- for compilation purposes only
} OBJECID;

extern OBJECID Ecid;

/* ------------------------ Static variables ------------------------------- */

/* ------------------------ Function Prototypes ---------------------------- */
void
constructEcid(void);

#endif // PMU_OBJECID_H

