/*
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_verify_engine_tuxxx.c
 * @brief  Code that verifies the PMU microcode is running on the PMU hardware
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"

#include "config/g_pmu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Verifies that the PMU microcode is running on the expected engine
 *          (the PMU).
 *
 * @details Uses appropriate registers to determine the microcode's engine and
 *          halts the core if it is not the PMU.
 *
 * @note    This function exelwtes a @ref PMU_HALT upon failure. Exelwting a
 *          @ref PMU_HALT is normally strongly discouraged, but it serves an
 *          important purpose here. Namely, once we have determined that we are
 *          not running on the PMU, it not safe to execute *any* more code; we
 *          have no idea what engine we're on, what environment we're in, what
 *          registers are valid, etc. To avoid exposing any other security
 *          issues by continuing exelwtion, this function instead exelwtes a
 *          @ref PMU_HALT. It still returns an error, however, in case we are
 *          in some environment where an agent can incorrectly restart us, so
 *          that, hopefully, the calling code will avoid continuing exelwtion.
 *
 * @return  FLCN_OK                 Microcode is running on the PMU
 * @return  FLCN_ERR_ILWALID_STATE  Microcode is running on some other engine
 */
FLCN_STATUS
pmuPreInitVerifyEngine_TUXXX(void)
{
    FLCN_STATUS status = FLCN_OK;

    if (!FLD_TEST_REF(
            LW_CPWR_FALCON_ENGID_FAMILYID,
            _PWR_PMU,
           REG_RD32(CSB, LW_CPWR_FALCON_ENGID)))
    {
        //
        // To understand the reasoning for the PMU_HALT, see the function
        // comments.
        //
        PMU_HALT();
        status = FLCN_ERR_ILWALID_STATE;
        goto pmuPreInitVerifyEngine_TUXXX_exit;
    }

pmuPreInitVerifyEngine_TUXXX_exit:
    return status;
}
