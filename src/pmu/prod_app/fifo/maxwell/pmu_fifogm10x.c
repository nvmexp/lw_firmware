/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_fifogm10x.c
 * @brief  HAL-interface for the GM10X Fifo Engine.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "dev_fifo.h"
#include "dev_pwr_csb.h"
#include "dev_fuse.h"
#include "dev_top.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objfifo.h"
#include "pmu_objfuse.h"
#include "pmu_objpmu.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "pmu_bar0.h"
#include "dbgprintf.h"

#include "config/g_fifo_private.h"

/* ------------------------- Static Variables ------------------------------- */
static PMU_ENGINE_LOOKUP_TABLE EngineLookupTable_GM10X[] =
{
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_GRAPHICS, PMU_ENGINE_GR,   0},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_COPY2,    PMU_ENGINE_CE2,  0},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LWENC,    PMU_ENGINE_ENC0, 0},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_COPY0,    PMU_ENGINE_CE0,  0},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_COPY1,    PMU_ENGINE_CE1,  0},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LWDEC,    PMU_ENGINE_BSP,  0},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_SEC,      PMU_ENGINE_SEC2, 0},
};

/* ------------------------ Macros and Defines ----------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static Prototypes ------------------------------ */
/* ------------------------ Prototypes  ------------------------------------ */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * Returns the list of engines for this chip
 * @param[out]  pmuEngineTable
 *    list of engines for this chip
  * @param[out]  engLookupTblSize
 */
void
fifoGetEngineLookupTable_GM10X
(
    PMU_ENGINE_LOOKUP_TABLE **pEngineLookupTable,
    LwU32                    *pEngLookupTblSize
)
{
    *pEngineLookupTable = EngineLookupTable_GM10X;
    *pEngLookupTblSize  = LW_ARRAY_ELEMENTS(EngineLookupTable_GM10X);
}

/*!
 * Returns the PMU engine ID for LWDEC.
 *
 * Pmu engine id for LWDEC varies with chip generations.
 *
 * @param[out]  pmuEngIdLwdec
 *    PMU engine ID for LWDEC
 *
 */
void
fifoGetLwdecPmuEngineId_GM10X(LwU8 *pPmuEngIdLwdec)
{
    *pPmuEngIdLwdec = PMU_ENGINE_BSP;
}

/*!
 * Check if engine is disabled (e.g. floorswept).
 *
 * @return  Nonzero    - if disabled or floorswept
 *          Zero       - otherwise
 */
LwBool
fifoIsEngineDisabled_GM10X
(
    LwU8 pmuEngineId
)
{
    LwU32 fuse;

    switch (pmuEngineId)
    {
        case PMU_ENGINE_GR:
        {
            // GR as a whole can never be floorswept.
            return LW_FALSE;
        }
        case PMU_ENGINE_VP:
        case PMU_ENGINE_PPP:
        {
            // VP and PPP have been removed and replaced by LWDEC for Maxwell.
            return LW_TRUE;
        }
        case PMU_ENGINE_BSP:
        {
            //
            // VP, PPP, and BSP are replaced by LWDEC in Maxwell
            // We are reusing the BSP object for LWDEC in Maxwell though
            //
            fuse = fuseRead(LW_FUSE_STATUS_OPT_LWDEC);
            return FLD_TEST_DRF(_FUSE, _STATUS_OPT_LWDEC, _DATA, _DISABLE, fuse);
        }
        case PMU_ENGINE_ENC0:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_LWENC);
            return FLD_TEST_DRF(_FUSE, _STATUS_OPT_LWENC, _DATA, _DISABLE, fuse);
        }
        case PMU_ENGINE_SEC2:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_SEC);
            return FLD_TEST_DRF(_FUSE, _STATUS_OPT_SEC, _DATA, _DISABLE, fuse);
        }
        case PMU_ENGINE_CE0:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_CE);
            return FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_CE, _IDX, 0, _DISABLE, fuse);
        }
        case PMU_ENGINE_CE1:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_CE);
            return FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_CE, _IDX, 1, _DISABLE, fuse);
        }
        case PMU_ENGINE_CE2:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_CE);
            return FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_CE, _IDX, 2, _DISABLE, fuse);
        }
    }
    return LW_FALSE;
}

/* ------------------------ Static Functions ------------------------------- */
