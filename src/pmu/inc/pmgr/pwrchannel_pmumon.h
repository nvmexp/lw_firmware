/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pwrchannel_pmumon.h
 * @copydoc pwrchannel_pmumon.c
 */

#ifndef PWRCHANNEL_PMUMON_H
#define PWRCHANNEL_PMUMON_H

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
/* ------------------------- External definitions -------------------------- */
extern LwrtosSemaphoreHandle PwrChannelPmumonQueryMutex;
/* ------------------------- Macros ----------------------------------------- */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_CHANNELS_PMUMON))
    #define PWR_CHANNEL_PMUMON_SEMAPHORE_TAKE                                  \
        lwrtosSemaphoreTakeWaitForever(PwrChannelPmumonQueryMutex)
    #define PWR_CHANNEL_PMUMON_SEMAPHORE_GIVE                                  \
        lwrtosSemaphoreGive(PwrChannelPmumonQueryMutex)
#else
    #define PWR_CHANNEL_PMUMON_SEMAPHORE_TAKE lwosNOP()
    #define PWR_CHANNEL_PMUMON_SEMAPHORE_GIVE lwosNOP()
#endif
/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- Defines ---------------------------------------- */
/*!
 * @brief   Period between samples while in active power callback mode.
 *
 * @todo    Replace hardcoding of 100ms with vbios specified sampling rate.
 */
#define PWR_CHANNEL_PMUMON_ACTIVE_POWER_CALLBACK_PERIOD_US()                   \
    (100000U)

/*!
 * @brief   Period between samples while in low power callback mode.
 *
 * @todo    Replace hardcoding of 1s with vbios specified sampling rate.
 */
#define PWR_CHANNEL_PMUMON_LOW_POWER_CALLBACK_PERIOD_US()                      \
    (1000000U)
/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief      Constructor for PWR_CHANNELS PMUMON interface.
 *
 * @return     FLCN_OK if successful.
 */
FLCN_STATUS pmgrPwrChannelsPmumonConstruct(void)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pmgrPwrChannelsPmumonConstruct");

/*!
 * @brief      Creates and schedules the tmr callback responsible for publishing
 *             PMUMON updates.
 *
 * @return     FLCNOK on success.
 */
FLCN_STATUS pmgrPwrChannelsPmumonLoad(void);

/*!
 * @brief      Cancels the tmr callback that was scheduled during load.
 */
void pmgrPwrChannelsPmumonUnload(void);

/*!
 * @brief       Gets the average value of TGP samples in the last second.
 *
 * @param[out]  Return average TGP value if exists.
 *              If there are no samples in the queue, this value will be 0.
 *
 * @return      average power reading in mW.
 */
LwU32 pmgrPwrChannelsPmumonGetAverageTotalGpuPowerUsage(void);

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // PWRCHANNEL_PMUMON_H
