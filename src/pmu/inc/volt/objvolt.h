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
 * @file objvolt.h
 * @brief @copydoc objvolt.c
 */

#ifndef OBJVOLT_H
#define OBJVOLT_H

#include "g_objvolt.h"

#ifndef G_OBJVOLT_H
#define G_OBJVOLT_H

/* ------------------------- System Includes ------------------------------- */
#include "unit_api.h"

/* ------------------------- Application Includes -------------------------- */
#include "volt/voltrail.h"
#include "volt/voltdev.h"
#include "volt/voltpolicy.h"
#include "lib_semaphore.h"
#include "lwostimer.h"

/* ------------------------- Macros ---------------------------------------- */

/*!
 * @brief Macro to check if PMU VOLT infrastructure is loaded.
 */
#define PMU_VOLT_IS_LOADED()    (Volt.bLoaded)

/*!
 * @brief Macros for read-write semaphore implementation to synchronize access to VOLT
 * data - current voltage, voltage offset etc.
 * This implementation uses the RW semaphore library implementation.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_SEMAPHORE))
    #define voltSemaphoreRWInitialize()                                        \
            OS_SEMAPHORE_CREATE_RW(OVL_INDEX_DMEM(perf))

    #define voltReadSemaphoreTake()                                            \
            OS_SEMAPHORE_RW_TAKE_RD(Volt.pVoltSemaphoreRW);

    #define voltReadSemaphoreGive()                                            \
            OS_SEMAPHORE_RW_GIVE_RD(Volt.pVoltSemaphoreRW);

    #define voltWriteSemaphoreTake()                                           \
            OS_SEMAPHORE_RW_TAKE_WR(Volt.pVoltSemaphoreRW);

    #define voltWriteSemaphoreGive()                                           \
            OS_SEMAPHORE_RW_GIVE_WR(Volt.pVoltSemaphoreRW);
#else
    #define voltSemaphoreRWInitialize()     NULL
    #define voltReadSemaphoreTake()         lwosNOP()
    #define voltReadSemaphoreGive()         lwosNOP()
    #define voltWriteSemaphoreTake()        lwosNOP()
    #define voltWriteSemaphoreGive()        lwosNOP()
#endif

/* ------------------------- Datatypes ------------------------------------- */

/*!
 * @brief VOLT Object Definition
 */
typedef struct
{
    /*!
     * @brief Boolean to indicate whether PMU VOLT infrastructure is loaded.
     */
    LwBool                  bLoaded;

    /*!
     * @brief Voltage Rail data containing board object group of all @ref VOLT_RAIL
     * objects and voltage domain HAL.
     */
    VOLT_RAIL_METADATA      railMetadata;

    /*!
     * @brief Board Object Group of all @ref VOLT_DEVICE objects.
     */
    BOARDOBJGRP_E32         devices;

    /*!
     * @brief Board Object Group of all @ref VOLT_POLICY objects.
     */
    BOARDOBJGRP_E32         policies;

   /*!
     * @brief Volt policy data obtained from VBIOS.
     */
    VOLT_POLICY_METADATA    policyMetadata;

#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_SEMAPHORE))
    PSEMAPHORE_RW           pVoltSemaphoreRW;
#endif
} OBJVOLT;

/* ------------------------ External Definitions --------------------------- */
extern  OBJVOLT     Volt;

/* ------------------------ Function Prototypes ---------------------------- */
// Called when the perf task is scheduled for the first time.
mockable FLCN_STATUS voltPostInit(void)
    GCC_ATTRIB_SECTION("imem_libPerfInit", "voltPostInit");

FLCN_STATUS voltProcessDynamilwpdate(void);

mockable FLCN_STATUS voltProcessRmCmdFromPerf(DISP2UNIT_RM_RPC *pDispatch)
    GCC_ATTRIB_SECTION("imem_perf", "voltProcessRmCmdFromPerf");

FLCN_STATUS voltVoltSetVoltage(LwU8 clientID, LW2080_CTRL_VOLT_VOLT_RAIL_LIST *pRailList);

mockable FLCN_STATUS voltVoltSanityCheck(LwU8 clientID, LW2080_CTRL_VOLT_VOLT_RAIL_LIST *pRailList, LwBool bCheckState);

FLCN_STATUS voltVoltOffsetRangeGet(LwU8 clientID, LW2080_CTRL_VOLT_VOLT_RAIL_LIST *pRailList, VOLT_RAIL_OFFSET_RANGE_LIST *pRailOffsetList, LwBool bSkipVoltRangeTrim)
    GCC_ATTRIB_SECTION("imem_libVoltApi", "voltVoltOffsetRangeGet");

/* ------------------------ Include Derived Types -------------------------- */

#endif // G_OBJVOLT_H
#endif // OBJVOLT_H
