/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file selwrescrub_halgv10x.c
 */

#define SELWRESCRUBCFG_ENGINE_SETUP      1   // pull in per-gpu engine interface's
#define SELWRESCRUBCFG_HAL_SETUP_GV10X   1   // All enabled GV10X gpus
#define SELWRESCRUBCFG_HAL_SUPPORT_GV10X 1   // not required, but keeps us honest

#include "selwrescrub.h"
#include "../lw/selwrescrub_halgen.c"
#include "selwrescrub_objhal.h"
#include "config/selwrescrub-config.h"

/*
#if (SELWRESCRUBCFG_CHIP_ENABLED(GV10X))
SELWRESCRUB_RC registerHalModule_GV10X(void)
    GCC_ATTRIB_SECTION("imem_selwrescrub", "__FUNC__");

SELWRESCRUB_RC
registerHalModule_GV10X(void)
{
    IfaceSetup = &halIface_GV100;
    return SELWRESCRUB_RC_OK;
}
#endif
*/

