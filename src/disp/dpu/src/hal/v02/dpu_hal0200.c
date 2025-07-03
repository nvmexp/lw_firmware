/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwdpu.h"

#define DPUCFG_ENGINE_SETUP      1   // pull in per-gpu engine interface's
#define DPUCFG_HAL_SETUP_GF11X   1   // All enabled GF11X gpus
#define DPUCFG_HAL_SUPPORT_GF11X 1   // not required, but keeps us honest

#include "config/dpu-config.h"
#include "../lw/dpu_halgen.c"
#include "dpu_objhal.h"

#if (DPUCFG_CHIP_ENABLED(GF11X))
FLCN_STATUS registerHalModule_GF11X(void)
    GCC_ATTRIB_SECTION("imem_init", "registerHalModule_GF11X");

FLCN_STATUS
registerHalModule_GF11X(void)
{
    IfaceSetup = &halIface_GF119;
    return FLCN_OK;
}
#endif

