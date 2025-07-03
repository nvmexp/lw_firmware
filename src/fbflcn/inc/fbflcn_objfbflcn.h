/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef FBFLCN_OBJFBFLCN_H
#define FBFLCN_OBJFBFLCN_H

#include "fbflcn_gpuarch.h"

#include "fbflcn_defines.h"
#include "config/g_fbfalcon_hal.h"
#include "config/fbfalcon-config.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/*!
 * FBFLCN object Definition
 */
typedef struct
{
    FBFALCON_HAL_IFACES hal;
    FBFLCN_CHIP_INFO  chipInfo;
    LwU8            flcnPrivLevelExtDefault;
    LwU8            flcnPrivLevelCsbDefault;
    LwBool          bDebugMode;
} OBJFBFLCN;

extern OBJFBFLCN Fbflcn;


#endif   // FBFLCN_OBJFBFLCN_H



