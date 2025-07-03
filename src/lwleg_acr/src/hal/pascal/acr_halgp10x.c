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
 * @file acr_halgp10x.c
 */

#define ACRCFG_ENGINE_SETUP      1   // pull in per-gpu engine interface's
#define ACRCFG_HAL_SETUP_GP10X   1   // All enabled GP10X gpus
#define ACRCFG_HAL_SUPPORT_GP10X 1   // not required, but keeps us honest

#include "acr.h"
#include "../lw/acr_halgen.c"
#include "acr_objhal.h"
#include "config/acr-config.h"

#if (ACRCFG_CHIP_ENABLED(GP10X))
ACR_STATUS registerHalModule_GP10X(void)
    GCC_ATTRIB_SECTION("imem_acr", "__FUNC__");

ACR_STATUS
registerHalModule_GP10X(void)
{
#if (ACRCFG_CHIP_ENABLED(GP100))
    IfaceSetup = &halIface_GP100;
#else
    IfaceSetup = &halIface_GP102;
#endif
    return ACR_OK;
}
#endif

