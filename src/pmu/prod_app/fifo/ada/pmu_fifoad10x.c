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
 * @file   pmu_fifogaxxx.c
 * @brief  HAL-interface for the GAXXX Fifo Engine.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application includes --------------------------- */
#include "pmu_objfifo.h"
#include "pmu_bar0.h"
#include "dev_top.h"
#include "dev_runlist.h"

#include "config/g_fifo_private.h"

/* ------------------------- Static Variables ------------------------------- */
static PMU_ENGINE_LOOKUP_TABLE EngineLookupTable_GA10X[] =
{
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_GRAPHICS, PMU_ENGINE_GR,     0},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWENC,    PMU_ENGINE_ENC0,   0},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWENC,    PMU_ENGINE_ENC1,   1},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWENC,    PMU_ENGINE_ENC2,   2},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LCE,      PMU_ENGINE_CE0,    0},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LCE,      PMU_ENGINE_CE1,    1},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LCE,      PMU_ENGINE_CE2,    2},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LCE,      PMU_ENGINE_CE3,    3},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LCE,      PMU_ENGINE_CE4,    4},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LCE,      PMU_ENGINE_CE5,    5},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWDEC,    PMU_ENGINE_DEC0,   0},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWDEC,    PMU_ENGINE_DEC1,   1},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWDEC,    PMU_ENGINE_DEC2,   2},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWDEC,    PMU_ENGINE_DEC3,   3},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_SEC,      PMU_ENGINE_SEC2,   0},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWJPG,    PMU_ENGINE_JPG0,   0},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWJPG,    PMU_ENGINE_JPG1,   1},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWJPG,    PMU_ENGINE_JPG2,   2},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWJPG,    PMU_ENGINE_JPG3,   3},
    {LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_OFA,      PMU_ENGINE_OFA0,   0},
};

/* ------------------------ Macros and Defines ----------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static Prototypes ------------------------------ */
/* ------------------------ Prototypes  ------------------------------------ */
/* ------------------------ Public Functions  ------------------------------ */
/*!
 * Returns the list of engines for this chip
 * @param[out]  pEngineLookupTable
 *    list of engines for this chip
 * @param[out]  engLookupTblSize
 */
void
fifoGetEngineLookupTable_AD10X
(
    PMU_ENGINE_LOOKUP_TABLE **pEngineLookupTable,
    LwU32                    *pEngLookupTblSize
)
{
    *pEngineLookupTable = EngineLookupTable_GA10X;
    *pEngLookupTblSize  = LW_ARRAY_ELEMENTS(EngineLookupTable_GA10X);
}
