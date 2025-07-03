/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_fifogmxxx.c
 * @brief  HAL-interface for the GMXXX Fifo Engine.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "dev_fifo.h"
#include "hwproject.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objfifo.h"
#include "pmu_objpmu.h"
#include "pmu_bar0.h"
#include "dbgprintf.h"

#include "config/g_fifo_private.h"

//
// Compile time check to ensure that number of PBDMAs does not exceed 16.
// We cache the pbdma IDs in a 16 bit mask as of now, so need to extend
// the mask to 32 bit if the number of PBDMAs exceeds 16 on any chip.
//
ct_assert(LW_HOST_NUM_PBDMA <= 16);
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros and Defines ----------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static Prototypes ------------------------------ */
/* ------------------------ Prototypes  ------------------------------------ */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * Find PBDMA ID Mask from the PBDMA map for the given runlist
 *
 * @param[in] runlistId Runlist ID to find supporting pbdma.
 *
 * @return  pbdma ID Mask          - if found
 *          PMU_PBMDA_MASK_ILWALID - otherwise
 */
LwU16
fifoFindPbdmaMaskForRunlist_GM10X
(
    LwU32    runlistId
)
{
   LwU32  pbdmaId;
   LwU32  pbdmaCount  = REG_FLD_RD_DRF(BAR0, _PFIFO, _CFG0, _NUM_PBDMA);
   LwU16  pbdmaIdMask = PMU_PBDMA_MASK_ILWALID;

   for (pbdmaId = 0; pbdmaId < pbdmaCount; ++pbdmaId)
   {
       if (REG_RD32(BAR0, LW_PFIFO_PBDMA_MAP(pbdmaId)) & BIT(runlistId))
       {
           pbdmaIdMask |= BIT(pbdmaId);
       }
   }

   PMU_HALT_COND(pbdmaIdMask != PMU_PBDMA_MASK_ILWALID);
   return pbdmaIdMask;
}

/* ------------------------ Static Functions ------------------------------- */
