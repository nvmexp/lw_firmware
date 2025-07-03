/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file acr_halgm20x.c
 */

#define ACRCFG_ENGINE_SETUP      1   // pull in per-gpu engine interface's
#define ACRCFG_HAL_SETUP_GM20X   1   // All enabled GM20X gpus
#define ACRCFG_HAL_SUPPORT_GM20X 1   // not required, but keeps us honest

#include "acr.h"
#include "../lw/acr_halgen.c"
#include "acr_objhal.h"
#include "config/acr-config.h"

#if (ACRCFG_CHIP_ENABLED(GM20X))
ACR_STATUS registerHalModule_GM20X(void)
    GCC_ATTRIB_SECTION("imem_acr", "__FUNC__");

ACR_STATUS
registerHalModule_GM20X(void)
{
#if (ACRCFG_CHIP_ENABLED(GM20B))
    IfaceSetup = &halIface_GM20B;
#else
    IfaceSetup = &halIface_GM200;
#endif
    return ACR_OK;
}
#endif

