/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file soe_hallr10.c
 */

// TODO: This should be LR10
#include "soesw.h"

#define SOECFG_ENGINE_SETUP      1   // pull in per-gpu engine interface's
#define SOECFG_HAL_SETUP_TU10X   1   // All enabled GP10X gpus
#define SOECFG_HAL_SUPPORT_TU10X 1   // not required, but keeps us honest

#include "../lw/soe_halgen.c"
#include "soe_objhal.h"

#if (SOECFG_CHIP_ENABLED(TU10X))
FLCN_STATUS registerHalModule_LR10(void)
    GCC_ATTRIB_SECTION("imem_init", "registerHalModule_LR10");

FLCN_STATUS
registerHalModule_TU10X(void)
{
    IfaceSetup = &halIface_TU102;
    return FLCN_OK;
}
#endif

