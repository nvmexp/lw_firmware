/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   ei_passive.c
 * @brief  Engine idle object for providing dummy FSM related methods
 *
 * EI passive is part of EI Group, that serves as a characterization
 * framwork of LPWR EI feature at different Pstates.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "pmu_objhal.h"
#include "pmu_objei.h"
#include "dbgprintf.h"

/* ------------------------ Application includes --------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes --------------------------------------*/
static FLCN_STATUS s_eiPassiveEntry(void)
                   GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_eiPassiveExit(void)
                   GCC_ATTRIB_NOINLINE();

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */
/*!
 * @brief EI passive initialization
 */
void
eiPassiveInit(void)
{
    OBJPGSTATE *pEiPassiveState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_EI_PASSIVE);

    // Initialize PG interfaces for EI-Passive
    pEiPassiveState->pgIf.lpwrEntry = s_eiPassiveEntry;
    pEiPassiveState->pgIf.lpwrExit  = s_eiPassiveExit;

    // Initialize idle masks for EI-Passive
    eiPassiveIdleMaskInit_HAL(&Ei);
}

/*!
 * @brief Context save interface for LPWR_ENG(EI_PASSIVE)
 *
 * @return FLCN_OK      Success.
 */
static FLCN_STATUS
s_eiPassiveEntry(void)
{
    return FLCN_OK;
}

/*!
 * @brief Context restore interface for LPWR_ENG(EI_PASSIVE)
 *
 * @return FLCN_OK      Success.
 */
static FLCN_STATUS
s_eiPassiveExit(void)
{
    return FLCN_OK;
}
