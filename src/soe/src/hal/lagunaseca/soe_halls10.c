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
 * @file soe_halls10.c
 */

#include "soesw.h"

#define SOECFG_ENGINE_SETUP      1   // pull in per-gpu engine interface's
#define SOECFG_HAL_SETUP_GH10X   1   // All enabled GH10X gpu's
#define SOECFG_HAL_SUPPORT_GH10X 1   // not required, but keeps us honest

#include "../lw/soe_halgen.c"
#include "soe_objhal.h"

#if (SOECFG_CHIP_ENABLED(GH10X))
FLCN_STATUS registerHalModule_LS10(void)
    GCC_ATTRIB_SECTION("imem_init", "registerHalModule_LS10");

FLCN_STATUS
registerHalModule_GH10X(void)
{
    IfaceSetup = &halIface_GH100;
    return FLCN_OK;
}
#endif

