/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwdpu.h"

#define DPUCFG_ENGINE_SETUP      1   // pull in per-gpu engine interface's
#define DPUCFG_HAL_SETUP_GP10X   1   // All enabled GP10X gpus
#define DPUCFG_HAL_SUPPORT_GP10X 1   // not required, but keeps us honest

#include "config/dpu-config.h"
#include "dpu_objhal.h"
#include "../lw/dpu_halgen.c"

#if (DPUCFG_CHIP_ENABLED(GP10X))
FLCN_STATUS registerHalModule_GP10X(void)
    GCC_ATTRIB_SECTION("imem_init", "registerHalModule_GP10X");

FLCN_STATUS
registerHalModule_GP10X(void)
{
    IfaceSetup = &halIface_GP100;
    return FLCN_OK;
}
#endif

