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
 * @file  pwrchannel_status.h
 * @brief strulwture specification for status functionality of PWR_CHANNELS
 */

#ifndef PWRCHANNEL_STATUS_H
#define PWRCHANNEL_STATUS_H

/* ------------------------- System Includes ------------------------------- */
#include "boardobj/boardobjgrp.h"

/* ------------------------- Application Includes -------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------------ Macros ------------------------------------ */

/*!
 * Initialize PWR_CHANNELS status mask
 *
 * @param[in]   _pMask       pointer to PWR_CHANNELS mask
 */
 #define PWR_CHANNELS_STATUS_MASK_INIT(_pMask)                                \
    do {                                                                      \
        boardObjGrpMaskInit_E32(_pMask);                                      \
    } while (LW_FALSE)

// Overlays needed for pwrChannelsStatusGet()
#define OSTASK_OVL_DESC_DEFINE_PWR_CHANNELS_STATUS_GET                       \
        OSTASK_OVL_DESC_DEFINE_LIB_PWR_MONITOR                               \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgr)                          \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgr)                          \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibQuery)                  \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpBasic)                   \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVf)                        \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)                          \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)                   \
        PWR_CHANNEL_35_OVERLAYS_STATUS_GET

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * Contains channels status info. Internal version of RMCTRL PWR_CHANNEL_STATUS
 * Used to query PWR_CHANNELS from internal PMU clients.
 */
typedef struct
{
    /*!
     * Mask of all enabled channels
     */
    BOARDOBJGRPMASK_E32           mask;

    /*!
     * Tuple values used for querying the channel
     */
    RM_PMU_PMGR_PWR_CHANNEL_BOARDOBJ_GET_STATUS_UNION
        channels[LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHANNELS];
} PWR_CHANNELS_STATUS;

/*!
 * @interface PWR_CHANNEL
 *
 * Query PWR_CHANNELS status
 *
 * @param[in] pChannelsStatus     Pointer to PWR_CHANNEL_STATUS object
 */
#define PwrChannelsStatusGet(fname) FLCN_STATUS (fname)(PWR_CHANNELS_STATUS *pChannelsStatus)

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Inline Functions ----------------------------- */

/*!
 * @brief   Accessor for  RM_PMU_PMGR_PWR_CHANNEL_STATUS::tuple 
 *          from PWR_CHANNELS_STATUS
 *
 * @param[in]  pChannesStatus  PWR_CHANNELS_STATUS pointer.
 * @param[in]  chIdx           Power Channel Index
 *
 * @return
 *     @ref M_PMU_PMGR_PWR_CHANNEL_STATUS::tuple pointer
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LW2080_CTRL_PMGR_PWR_TUPLE *
pwrChannelsStatusChannelTupleGet
(
    PWR_CHANNELS_STATUS *pChannelsStatus,
    LwU8                 chIdx
)
{
    return &((pChannelsStatus)->channels[chIdx].channel.tuple);
}

/* ------------------------- Public Functions ------------------------------ */

/*!
 * PWR_CHANNEL_STATUS interfaces
 */
PwrChannelsStatusGet        (pwrChannelsStatusGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChannelsStatusGet");

#endif // PWRCHANNEL_STATUS_H
