/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_fifogpxxx.c
 * @brief  HAL-interface for the GPXXX Fifo Engine.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "dev_fifo.h"
#include "dev_top.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objfifo.h"
#include "pmu_objfuse.h"
#include "pmu_objpg.h"
#include "pmu_objpmu.h"

#include "config/g_fifo_private.h"

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------ Macros and Defines ----------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static Prototypes ------------------------------ */
/* ------------------------ Prototypes  ------------------------------------ */
/* ------------------------ Public Functions  ------------------------------ */

/**
 * @brief Get corresponding pmu engine from lookup table.
 *        For Pascal, both engine type enum and inst id
 *        will be used to map the correct pmu engine
 *
 * @param[in]  engineLookupTable
 * @param[in]  engLookupTblSize
 * @param[in]  fifoEngineData
 * @param[in]  fifoEngineType
 * @param[in]  bFifoDataFound
 */
LwU8
fifoGetPmuEngineFromLookupTable_GPXXX
(
    PMU_ENGINE_LOOKUP_TABLE *engineLookupTable,
    LwU32                    engLookupTblSize,
    LwU32                    fifoEngineData,
    LwU8                     fifoEngineType,
    LwBool                   bFifoDataFound
)
{
    LwU32 tableIdx;

    // Data entry is present for all host supported engines
    PMU_HALT_COND(bFifoDataFound);

    for (tableIdx = 0; tableIdx < engLookupTblSize; ++tableIdx)
    {
        if ( (engineLookupTable[tableIdx].typeEnum == fifoEngineType) &&
             (DRF_VAL(_PTOP, _DEVICE_INFO, _DATA_INST_ID,
              fifoEngineData) == engineLookupTable[tableIdx].instId) )
        {
            return engineLookupTable[tableIdx].pmuEngineId;
        }
    }
    return PMU_ENGINE_ILWALID;
}
