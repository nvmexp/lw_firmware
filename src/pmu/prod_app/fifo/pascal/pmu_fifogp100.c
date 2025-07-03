/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_fifogp100.c
 * @brief  HAL-interface for the GP100 Fifo Engine.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "dev_fifo.h"
#include "dev_fuse.h"
#include "dev_top.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objfifo.h"
#include "pmu_objfuse.h"
#include "pmu_objpmu.h"
#include "pmu_objpg.h"

#include "config/g_fifo_private.h"

/* ------------------------- Static Variables ------------------------------- */
static PMU_ENGINE_LOOKUP_TABLE EngineLookupTable_GP100[] =
{
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_GRAPHICS, PMU_ENGINE_GR,   0},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LCE,      PMU_ENGINE_CE2,  2},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LCE,      PMU_ENGINE_CE3,  3},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LCE,      PMU_ENGINE_CE4,  4},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LCE,      PMU_ENGINE_CE5,  5},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LWENC,    PMU_ENGINE_ENC0, 0},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LWENC,    PMU_ENGINE_ENC1, 1},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LWENC,    PMU_ENGINE_ENC2, 2},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LCE,      PMU_ENGINE_CE0,  0},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LCE,      PMU_ENGINE_CE1,  1},
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
fifoGetEngineLookupTable_GP100
(
    PMU_ENGINE_LOOKUP_TABLE **pEngineLookupTable,
    LwU32                    *pEngLookupTblSize
)
{
    *pEngineLookupTable = EngineLookupTable_GP100;
    *pEngLookupTblSize  = LW_ARRAY_ELEMENTS(EngineLookupTable_GP100);
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
fifoGetLwdecPmuEngineId_GP100(LwU8 *pPmuEngIdLwdec)
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
fifoIsEngineDisabled_GP100
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
            return FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_LWENC, _IDX, 0, _DISABLE, fuse);
        }
        case PMU_ENGINE_ENC1:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_LWENC);
            return FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_LWENC, _IDX, 1, _DISABLE, fuse);
        }
        case PMU_ENGINE_ENC2:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_LWENC);
            return FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_LWENC, _IDX, 2, _DISABLE, fuse);
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
        case PMU_ENGINE_CE3:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_CE);
            return FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_CE, _IDX, 3, _DISABLE, fuse);
        }
        case PMU_ENGINE_CE4:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_CE);
            return FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_CE, _IDX, 4, _DISABLE, fuse);
        }
        case PMU_ENGINE_CE5:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_CE);
            return FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_CE, _IDX, 5, _DISABLE, fuse);
        }
    }
    return LW_FALSE;
}
