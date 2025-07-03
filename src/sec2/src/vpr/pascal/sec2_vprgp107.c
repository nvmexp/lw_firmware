 /* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwmisc.h"
#include "flcntypes.h"
#include "sec2sw.h"
#include "sec2_objvpr.h"
#include "sec2_bar0_hs.h"

/*!
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 * 
 *     Explicitly using gp107 manual here, as SPARE_BIT_13 register value is different
 * between gp102/4/6 and gp107/8
 *     This kind of hack (of using full ref manual paths) must not be copied anywhere,
 * without soliciting approval from HS code signers
 *     Any change in this code should also go through HS signers
 *     Reason for this hack - Adding a new gp107 profile for single register would have been
 * a lot of maintenance overhead like maintaining health of two binaries, HS signing, etc.
 * Also driver size would increase
 * 
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 */
#include "pascal/gp107/dev_fuse.h" // DON'T COPY THIS EVER, SEE WARNING ABOVE

#include "config/g_vpr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 *  Get SPARE_BIT_13 value
 */
FLCN_STATUS
vprIsAllowedInSWFuse_GP107(void)
{
    LwU32       spareBit;
    FLCN_STATUS flcnStatus = FLCN_OK;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_SPARE_BIT_13, &spareBit));
    if (FLD_TEST_DRF(_FUSE, _SPARE_BIT_13, _DATA, _ENABLE, spareBit))
    {
        flcnStatus = FLCN_OK;
    }
    else
    {
        flcnStatus = FLCN_ERR_VPR_APP_NOT_SUPPORTED_BY_SW;
    }

ErrorExit:
    return flcnStatus;
}

