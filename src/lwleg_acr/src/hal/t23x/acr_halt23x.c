/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file acr_halt23x.c
 */

#define ACRCFG_ENGINE_SETUP     1   // pull in per-gpu engine interface's
#define ACRCFG_HAL_SETUP_T23X   1   // All enabled T19X gpus
#define ACRCFG_HAL_SUPPORT_T23X 1   // not required, but keeps us honest

#include "acr.h"
#include "config/g_hal_private.h"
#include "config/acr-config.h"
#include "config/g_hal_register.h"

#if (ACRCFG_CHIP_ENABLED(T23X))

ACR_STATUS
registerHalModule_T23X(void)
{
    return ACR_OK;
}
#endif
