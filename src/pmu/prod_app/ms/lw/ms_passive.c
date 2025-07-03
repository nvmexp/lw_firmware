/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   ms_passive.c
 * @brief  Memory System object for providing dummy FSM related methods
 *
 * MS passive is part of MS Group, that serves as a characterization
 * framwork of LPWR MS feature at different Pstates.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "pmu_objhal.h"
#include "pmu_objms.h"
#include "dbgprintf.h"

/* ------------------------ Application includes --------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes --------------------------------------*/
static FLCN_STATUS s_msPassiveEntry(void)
                   GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_msPassiveExit(void)
                   GCC_ATTRIB_NOINLINE();

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */
/*!
 * @brief MS passive initialization
 */
void
msPassiveInit(void)
{
    OBJPGSTATE *pMsPassiveState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_PASSIVE);

    // Initialize PG interfaces for MS-Passive
    pMsPassiveState->pgIf.lpwrEntry = s_msPassiveEntry;
    pMsPassiveState->pgIf.lpwrExit  = s_msPassiveExit;

    // Initialize idle masks for MS-Passive
    msPassiveIdleMaskInit_HAL(&Ms);

    // Configure entry conditions of MS-PASSIVE
    msPassiveEntryConditionConfig_HAL(&Ms);

}

/*!
 * @brief Context save interface for LPWR_ENG(MS_PASSIVE)
 *
 * @return FLCN_OK      Success.
 */
static FLCN_STATUS
s_msPassiveEntry(void)
{
    return FLCN_OK;
}

/*!
 * @brief Context restore interface for LPWR_ENG(MS_PASSIVE)
 *
 * @return FLCN_OK      Success.
 */
static FLCN_STATUS
s_msPassiveExit(void)
{
    return FLCN_OK;
}
