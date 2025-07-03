/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file thrmmon.h
 * @brief @copydoc thrmmon.c
 */

#ifndef PMU_THRMMON_H
#define PMU_THRMMON_H

/* ------------------------ Includes --------------------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------- External definitions --------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
typedef struct THRM_MON THRM_MON, THRM_MON_BASE;

/*!
 * Extends BOARDOBJ providing attributes common to all THRM_MONs.
 */
struct THRM_MON
{
    /*!
     * Super class representing the board object
     */
    BOARDOBJ    super;

    /*!
     * Index into physical instance of Thermal Interrupt Monitor
     */
    LwU8        phyInstIdx;

    /*!
     * SW abstraction of 64 bit counter as we have 32 bit counters lwrrently
     * which can overflow.
     */
    LwU64       counter;

    /*!
     * engaged time in ns
     */
    LwU64       engagedTimens;

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_MONITOR_WRAP_AROUND))
    /*!
     * Previous SW read count of the 32 bit counter
     */
    LwU32       prevCount;
#endif
};

/*!
 * Metadata of Therm Monitor functionality.
 */
typedef struct
{
    /*!
     * Board Object Group of all THRM_MON objects.
     */
    BOARDOBJGRP_E32 monitors;

    /*!
     * Utils clock frequency for use in PMU. This frequency does not change
     * and hence passed from RM to PMU during state initialization.
     */
    LwU32   utilsClkFreqKhz;

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_MONITOR_PATCH))
    /*!
     * Mask of THERM_MONITOR VBIOS DEVINIT patching WARs which PMU must apply for this GPU.
     * Bitfield values specified per @ref RM_PMU_THERM_MONITOR_PATCH_ID.
     */
    LwU32   patchIdMask;
#endif
} THRM_MON_DATA;

/* ------------------------ Macros ----------------------------------------- */
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_THRM_MON  (&(Therm.monData.monitors.super))

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define THRM_MON_GET(_monIdx)   (BOARDOBJGRP_OBJ_GET(THRM_MON, (_monIdx)))

/*!
 * Divisor for thermal monitor's overflow period. This will ensure thermal
 * monitor counters do not overflow
 */
#define THRM_MON_OVERFLOW_PERIOD_DIVISOR  (0x2)

/*!
 * patchIdMask get/set.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_MONITOR_PATCH))
#define THERM_MONITOR_PATCH_ID_MASK_GET(_pThrmMonData)      ((_pThrmMonData)->patchIdMask)
#define THERM_MONITOR_PATCH_ID_MASK_SET(_pThrmMonData, _v)  ((_pThrmMonData)->patchIdMask = (_v))
#else
#define THERM_MONITOR_PATCH_ID_MASK_GET(_pThrmMonData)      (0)
#define THERM_MONITOR_PATCH_ID_MASK_SET(_pThrmMonData, _v)  ((void)0)
#endif

/* ------------------------ Function Prototypes  --------------------------- */
/*!
 * Board Object interfaces.
 */
BoardObjGrpIfaceModel10CmdHandler (thrmMonitorBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thrmMonitorBoardObjGrpIfaceModel10Set");

BoardObjGrpIfaceModel10SetEntry   (thrmThrmMonIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thrmThrmMonIfaceModel10SetEntry");

BoardObjGrpIfaceModel10SetHeader   (thrmThrmMonIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thrmThrmMonIfaceModel10SetHeader");

FLCN_STATUS thrmMonCounter64Get(THRM_MON *pThrmMon, LwU64 *pCount)
    GCC_ATTRIB_SECTION("imem_resident", "thrmMonCounter64Get");

FLCN_STATUS thrmMonTimer64Get(THRM_MON *pThrmMon, PFLCN_TIMESTAMP pTimeTimer, PFLCN_TIMESTAMP pTimeEngagedns)
    GCC_ATTRIB_SECTION("imem_resident", "thrmMonTimer64Get");

BoardObjGrpIfaceModel10CmdHandler        (thrmMonitorBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_thermLibMonitor", "thrmMonitorBoardObjGrpIfaceModel10GetStatus");

BoardObjGrpIfaceModel10GetStatusHeader   (thrmThrmMonIfaceModel10GetStatusHeader)
    GCC_ATTRIB_SECTION("imem_thermLibMonitor", "thrmThrmMonIfaceModel10GetStatusHeader");

BoardObjIfaceModel10GetStatus                (thrmThrmMonIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_thermLibMonitor", "thrmThrmMonIfaceModel10GetStatus");

FLCN_STATUS                  thermMonitorsLoad(void)
    GCC_ATTRIB_SECTION("imem_thermLibMonitor", "thermMonitorsLoad");

void                         thermMonitorsUnload(void)
    GCC_ATTRIB_SECTION("imem_thermLibMonitor", "thermMonitorsUnload");

#endif // PMU_THRMMON_H
