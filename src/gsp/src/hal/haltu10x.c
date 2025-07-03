/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#define GSPCFG_ENGINE_SETUP      1   // pull in per-gpu engine interface's
#define GSPCFG_HAL_SETUP_TU10X   1   // All enabled TU10X gpus
#define GSPCFG_HAL_SUPPORT_TU10X 1   // not required, but keeps us honest

#include "config/gsp-config.h"
#include "halgen.c"
#include "objhal.h"

#if (GSPCFG_CHIP_ENABLED(TU10X))
FLCN_STATUS registerHalModule_TU10X(void)
    GCC_ATTRIB_SECTION("imem_init", "registerHalModule_TU10X");

FLCN_STATUS
registerHalModule_TU10X(void)
{
    IfaceSetup = &halIface_TU102;
    return FLCN_OK;
}
#endif

