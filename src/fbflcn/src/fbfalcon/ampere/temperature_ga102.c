/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>
#include <lwmisc.h>

// include manuals
#include "dev_fbfalcon_csb.h"

// project headers
#include "priv.h"
#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "fbflcn_table.h"
#include "fbflcn_access.h"

#include "fbflcn_objfbflcn.h"
#include "config/g_fbfalcon_private.h"

#include "lwuproc.h"

#include "dev_pmgr.h"
#include "dev_pmgr_addendum.h"


void fbfalconPostDramTemperature_GA102
(
        LwU32 dramTemperature
)
{
    REG_WR32(BAR0, LW_PMGR_SELWRE_WR_SCRATCH_2_FB_TEMPERATURE_TRACKING, dramTemperature);
}
