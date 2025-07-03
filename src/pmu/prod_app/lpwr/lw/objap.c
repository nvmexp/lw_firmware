/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   objap.c
 * @brief  Adaptive Power object file
 *
 * OBJAP object provides generic infrastructure to make any power feature
 * adaptive.
 *
 * Refer - "Design: Adaptive Power Feature" @pmuifap.h for the details.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "pmu_objhal.h"
#include "pmu_objap.h"
#include "pmu_objgcx.h"
#include "pmu_objpg.h"
#include "dbgprintf.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
#ifdef DMEM_VA_SUPPORTED
OBJAP Ap
    GCC_ATTRIB_SECTION("dmem_lpwr", "Ap");
#else
    OBJAP Ap;
#endif

/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * Construct the Ap object. This sets up the HAL interface used by the
 * Adaptive PG modules.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructAp(void)
{
    // Add actions here to be performed prior to pg task is scheduled
    return FLCN_OK;
}

/*!
 * @brief AP initialization post init
 *
 * Initialization of AP related structures immediately after scheduling 
 * the pg task in scheduler.
 */
FLCN_STATUS
apPostInit(void)
{
    memset(&Ap, 0, sizeof(Ap));

    return FLCN_OK;
}

/*!
 * @brief Adaptive Power initialization command 
 *
 * @param[in]   pPgCmd      Pointer to RM_PMU_RPC_STRUCT_LPWR_LOADIN_AP_INITs
 *
 * @return      FLCN_OK     On success
 */
FLCN_STATUS
apInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_AP_INIT *pParams
)
{
    Ap.pwrClkMHz = PmuInitArgs.cpuFreqHz / (1000000);

    return FLCN_OK;
}
