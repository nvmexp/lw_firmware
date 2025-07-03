/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_fifotu10x.c
 * @brief  HAL-interface for the TU10X Fifo Engine.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "pmu_objfifo.h"
#include "pmu_objfuse.h"
#include "dev_fuse.h"
#include "dev_top.h"
#include "dev_fifo.h"

/* ------------------------ Application includes --------------------------- */

#include "config/g_fifo_private.h"

/* ------------------------- Static Variables ------------------------------- */
static PMU_ENGINE_LOOKUP_TABLE EngineLookupTable_TU10X[] =
{
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_GRAPHICS, PMU_ENGINE_GR,   0},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LCE,      PMU_ENGINE_CE2,  2},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LCE,      PMU_ENGINE_CE3,  3},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LCE,      PMU_ENGINE_CE4,  4},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LCE,      PMU_ENGINE_CE5,  5},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LWENC,    PMU_ENGINE_ENC0, 0},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LWENC,    PMU_ENGINE_ENC1, 1},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LCE,      PMU_ENGINE_CE0,  0},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LCE,      PMU_ENGINE_CE1,  1},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LWDEC,    PMU_ENGINE_BSP,  0},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LWDEC,    PMU_ENGINE_DEC1, 1},
    {LW_PTOP_DEVICE_INFO_TYPE_ENUM_LWDEC,    PMU_ENGINE_DEC2, 2},
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
fifoGetEngineLookupTable_TU10X
(
    PMU_ENGINE_LOOKUP_TABLE **ppEngineLookupTable,
    LwU32                    *pEngLookupTblSize
)
{
    *ppEngineLookupTable = EngineLookupTable_TU10X;
    *pEngLookupTblSize  = LW_ARRAY_ELEMENTS(EngineLookupTable_TU10X);
}

/*!
 * @brief Check if runlist and all engines associated with runlist are idle
 *
 * return   LW_TRUE     Runlist and all engines associated with runlist
 *                      are idle
 *          LW_FALSE    Otherwise
 */
LwBool
fifoIsRunlistAndEngIdle_TU10X
(
    LwU32 runlistId
)
{
    LwU32 val;

    val = REG_RD32(BAR0, LW_PFIFO_RUNLIST_INFO(runlistId));

    if (FLD_TEST_DRF(_PFIFO, _RUNLIST_INFO, _RUNLIST_IDLE, _TRUE, val) &&
        FLD_TEST_DRF(_PFIFO, _RUNLIST_INFO, _ENG_IDLE, _TRUE, val))
    {
        return LW_TRUE;
    }
    else
    {
        return LW_FALSE;
    }
}
