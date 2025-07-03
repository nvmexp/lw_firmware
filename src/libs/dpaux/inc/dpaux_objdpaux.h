/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DPAUX_OBJDPAUX_H
#define DPAUX_OBJDPAUX_H

/* ------------------------ Application includes --------------------------- */
#include "dpaux_cmn.h"
#include "config/g_dpaux_hal.h"
#include "config/dpaux-config.h"

/* ------------------------ External definitions --------------------------- */
/*
 * DPAUX Object Definition
*/
typedef struct
{
    DPAUX_HAL_IFACES    hal;
} OBJDPAUX;

extern OBJDPAUX Dpaux;

#endif // DPAUX_OBJDPAUX_H
