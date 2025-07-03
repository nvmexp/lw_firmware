/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_objfifo.c
 * @brief  Fifo object providing fifo related methods
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "pmu_objhal.h"
#include "pmu_objfifo.h"

#include "config/g_fifo_private.h"
#if(FIFO_MOCK_FUNCTIONS_GENERATED)
#include "config/g_fifo_mock.c"
#endif

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OBJFIFO Fifo;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct the FIFO object.  This sets up the HAL interface used by the
 * FIFO module.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructFifo(void)
{
    LwU32 i;

    for (i = 0; i < PMU_ENGINE_ILWALID; i++)
    {
        Fifo.pmuEngTbl[i].fifoEngId   = PMU_ENGINE_MISSING;
#if !PMUCFG_FEATURE_ENABLED(PMU_HOST_ENGINE_EXPANSION)
        Fifo.pmuEngTbl[i].pbdmaIdMask = PMU_PBDMA_MASK_ILWALID;
#endif
        Fifo.pmuEngTbl[i].runlistId   = PMU_RUNLIST_MISSING;
    }

    return FLCN_OK;
}

/*!
 * @brief Check the requested CE engine is mapped to GR0.
 * 
 * The API check whether CE engine is valid and mapped to GR0.
 *
 * @param[in] ceEngId   CE Enigne ID.
 *
 * @return LW_TRUE    when CE mapped to GR.
 * @return LW_FALSE   Otherwise.
 */
LwBool
fifoIsGrCeEngine(LwU8 ceEngId)
{
    return (FIFO_IS_ENGINE_PRESENT(GR)                              &&
            (GET_ENG_RUNLIST(PMU_ENGINE_GR) != PMU_RUNLIST_MISSING) &&
            (GET_ENG_RUNLIST(ceEngId) != PMU_RUNLIST_MISSING)       &&
            (GET_ENG_RUNLIST(PMU_ENGINE_GR) == GET_ENG_RUNLIST(ceEngId))) ? LW_TRUE : LW_FALSE;
}
