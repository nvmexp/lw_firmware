/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwdpu.h"

#define DPUCFG_ENGINE_SETUP      1   // pull in per-gpu engine interface's
#define DPUCFG_HAL_SETUP_AD10X   1   // All enabled AD10X gpus
#define DPUCFG_HAL_SUPPORT_AD10X 1   // not required, but keeps us honest

#include "config/dpu-config.h"
#include "../lw/dpu_halgen.c"
#include "dpu_objhal.h"

#if (DPUCFG_CHIP_ENABLED(AD10X))
FLCN_STATUS registerHalModule_AD10X(void)
    GCC_ATTRIB_SECTION("imem_init", "registerHalModule_AD10X");

FLCN_STATUS
registerHalModule_AD10X(void)
{
    IfaceSetup = &halIface_AD102;
    return FLCN_OK;
}
#endif

