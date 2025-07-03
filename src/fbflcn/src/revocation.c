/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>


// include manuals
//#include "dev_pri_ringmaster.h"
//#include "dev_fbpa.h"
#include "dev_fuse.h"
#include "dev_fbfalcon_csb.h"
#include "dev_top.h"


#include "priv.h"
#include "revocation.h"
#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"

#include <rmlsfm.h>

#include "osptimer.h"
#include "fbflcn_objfbflcn.h"
#include "config/g_fbfalcon_private.h"

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
#include "dev_master.h"     // PMC
#endif

extern LwU8 gPlatformType;







