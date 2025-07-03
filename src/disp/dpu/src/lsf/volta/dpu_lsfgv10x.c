/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_lsfgv10x.c
 * @brief  Light Secure Falcon (LSF) for Volta Hal Functions
 *
 * LSF Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "dpusw.h"
#include "rmlsfm.h"
#include "dpu_task.h"
#include "dispflcn_regmacros.h"

/* ------------------------ Application includes --------------------------- */
#include "dpu_objlsf.h"
#include "dpu_objdpu.h"

#include "config/g_lsf_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */

/*!
 * Raises the reset register's PLM.
 */
void
lsfRaiseResetPrivLevelMask_GV10X(void)
{
    if (DpuInitArgs.selwreMode)
    {
        //
        // Only raise priv level masks when we're in booted in LS mode,
        // else we'll take away our own ability to reset the masks when we
        // unload.
        //
        // Coverity detects defect here since it thinks falc_ldx_i can return
        // certain error code which needs to be handled while it's not. So it's
        // marked as a false positive.
        //
        DFLCN_REG_WR_DEF(RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE);
        DFLCN_REG_WR_DEF(ENGCTL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE);
    }
}

/*!
 * Lowers the reset register's PLM.
 */
void
lsfLowerResetPrivLevelMask_GV10X(void)
{
    // 
    // Coverity detects defect here since it thinks falc_ldx_i can return
    // certain error code which needs to be handled while it's not. So it's
    // marked as a false positive.
    //
    DFLCN_REG_WR_DEF(RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _ENABLE);
    DFLCN_REG_WR_DEF(ENGCTL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _ENABLE);
}
