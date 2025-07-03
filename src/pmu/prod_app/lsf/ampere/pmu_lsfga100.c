/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_lsfga100.c
 * @brief  Light Secure Falcon (LSF) GA100 Hal Functions
 *
 * LSF Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "pmu_bar0.h"
#include "dev_pwr_csb.h"
#include "dev_fuse.h"

/* ------------------------ Application includes --------------------------- */
#include "config/g_lsf_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */

/*!
 * @brief Initializes Light Secure Falcon settings.
 *
 * As part of GA100 security hardening, LS falcons are required to check that
 * PRIV_SEC is enabled for production boards. This check will be done here for
 * GA100_and_later profiles.
 *
 * _GM20X hal of LSF_INIT is called following the check for PRIV_SEC.
 */
FLCN_STATUS
lsfVerifyPrivSecEnabled_GA100(void)
{
    FLCN_STATUS status = FLCN_OK;

    // Halt PMU if PRIV_SEC is disabled on production boards.
    if (PMUCFG_FEATURE_ENABLED(PMU_VERIFY_PRIV_SEC_ENABLED))
    {
        if (FLD_TEST_DRF(_CPWR_PMU, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED,
                         REG_RD32(CSB, LW_CPWR_PMU_SCP_CTL_STAT)))
        {
            if (FLD_TEST_DRF(_FUSE, _OPT_PRIV_SEC_EN, _DATA, _NO,
                             REG_RD32(BAR0, LW_FUSE_OPT_PRIV_SEC_EN)))
            {
                PMU_HALT();
                status = FLCN_ERR_PRIV_SEC_VIOLATION;
            }
        }
    }

    return status;
}
