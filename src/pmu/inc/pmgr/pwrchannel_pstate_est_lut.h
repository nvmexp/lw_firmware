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
 * @file  pwrchannel_pstate_est_lut.h
 * @brief @copydoc pwrchannel_pstate_est_lut.c
 */

#ifndef PWRCHANNEL_PSTATE_EST_LUT_H
#define PWRCHANNEL_PSTATE_EST_LUT_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrchannel.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_CHANNEL_PSTATE_ESTIMATION_LUT PWR_CHANNEL_PSTATE_ESTIMATION_LUT;

/*!
 * Structure of the Pstate Estimation LUT Entry
 */
typedef struct
{
    /*!
     * Pstate index for LUT entry.
     */
    LwU8  pstateIndex;
    /*!
     * Boolean denoting whether Dynamic LUT is enabled.
     */
    LwBool  bDynamicLut;
    /*!
     * Data based on @bDynamicLut flag.
     */
    LW2080_CTRL_PMGR_PWR_CHANNEL_PSTATE_ESTIMATION_LUT_ENTRY_DATA data;
} PWR_CHANNEL_PSTATE_ESTIMATION_LUT_ENTRY_INFO;

struct PWR_CHANNEL_PSTATE_ESTIMATION_LUT
{
    /*!
     * @copydoc PWR_CHANNEL
     */
    PWR_CHANNEL super;

    /*!
     * Array of LUT entry of size
     * LW2080_CTRL_PMGR_PWR_CHANNEL_PSTATE_ESTIMATION_LUT_MAX_ENTRIES.
     */
    PWR_CHANNEL_PSTATE_ESTIMATION_LUT_ENTRY_INFO
        lutEntry[LW2080_CTRL_PMGR_PWR_CHANNEL_PSTATE_ESTIMATION_LUT_MAX_ENTRIES];
};

/* ------------------------- Defines --------------------------------------- */
/*!
 * @brief   Additional overlays required along OSTASK_OVL_DESC_DEFINE_LIB_PWR_MONITOR
 *          attach paths when PMU_PMGR_PWR_MONITOR_CHANNEL_PSTATE_ESTIMATION_LUT
 *          feature is enabled.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_CHANNEL_PSTATE_ESTIMATION_LUT))
#define OSTASK_OVL_DESC_DEFINE_LIB_PWR_MONITOR_CHANNEL_PSTATE_ESTIMATION_LUT        \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVf)
#else
#define OSTASK_OVL_DESC_DEFINE_LIB_PWR_MONITOR_CHANNEL_PSTATE_ESTIMATION_LUT        \
    OSTASK_OVL_DESC_ILWALID
#endif

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Inline Functions ------------------------------ */
/*!
 * Check if Dynamic Power Lookup for PSTATE_ESTIMATION_LUT channels and the
 * corresponding feature are enabled.
 *
 * @param[in]  pPstateEstLUT  PWR_CHANNEL_PSTATE_ESTIMATION_LUT pointer
 *
 * @return LW_TRUE
 *      Feature @ref PMU_PMGR_PWR_MONITOR_CHANNEL_PSTATE_ESTIMATION_LUT_DYNAMIC
 *      and the Dynamic Lookup Flags are enabled.
 * @return LW_FALSE
 *      Feature @ref PMU_PMGR_PWR_MONITOR_CHANNEL_PSTATE_ESTIMATION_LUT_DYNAMIC
 *      or either of the Dynamic Lookup Flags are disabled.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwBool
pwrChannelPstateEstimationLutDynamicEnabled
(
    PWR_CHANNEL_PSTATE_ESTIMATION_LUT *pPstateEstLUT
)
{
    return PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_CHANNEL_PSTATE_ESTIMATION_LUT_DYNAMIC) &&
           pPstateEstLUT->lutEntry[0].bDynamicLut && 
           pPstateEstLUT->lutEntry[1].bDynamicLut;
}

/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_CHANNEL interfaces
 */
BoardObjGrpIfaceModel10ObjSet           (pwrChannelGrpIfaceModel10ObjSetImpl_PSTATE_ESTIMATION_LUT)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrChannelGrpIfaceModel10ObjSetImpl_PSTATE_ESTIMATION_LUT");
PwrChannelTupleStatusGet    (pwrChannelTupleStatusGet_PSTATE_ESTIMATION_LUT)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChannelTupleStatusGet_PSTATE_ESTIMATION_LUT");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRCHANNEL_PSTATE_EST_LUT_H
