/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
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
//#include "dev_fbfalcon_pri.h"

// project headers
#include "priv.h"
#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"

#include "fbflcn_objfbflcn.h"
#include "config/g_fbfalcon_private.h"

#include "lwuproc.h"

void
fbfalconIllegalInterrupt_GA100
(
        void
)
{
    // TODO: Stefans enabled this and clean up the interrupt routine
    //       it seems to run through some times w/o processing the interrupt
    //       maybe the trigger on ok_to_swtich
    // FBFLCN_HALT(FBFLCN_ERROR_CODE_UNRESOLVED_INTERRUPT);
}
