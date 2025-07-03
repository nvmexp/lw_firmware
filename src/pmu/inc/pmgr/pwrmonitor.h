/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrmonitor.h
 * @brief @copydoc pwrmonitor.c
 */

#ifndef PWRMONITOR_H
#define PWRMONITOR_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrchannel.h"
#include "pmgr/pwrchrel.h"
#include "task_pmgr.h"
#include "boardobj/boardobjgrp_iface_model_10.h"

/* ------------------------- External definitions -------------------------- */
extern LwrtosSemaphoreHandle PwrMonitorChannelQueryMutex;

/* ------------------------- Macros ---------------------------------------- */
/*!
 * Macro wrapper for constructing PWR_MONITOR SUPER state. There is no state
 * which can be constructed at this time, so just expands to FLCN_OK.  This may
 * be replaced with a full implementation when ready.
 */
#define pwrMonitorConstruct_SUPER(ppMonitor)                                  \
    FLCN_OK

/*!
 * Macros for acquiring and releasing semaphores using RTOS APIs.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_SEMAPHORE))
    #define PWR_MONITOR_SEMAPHORE_TAKE                                        \
        lwrtosSemaphoreTakeWaitForever(PwrMonitorChannelQueryMutex)
    #define PWR_MONITOR_SEMAPHORE_GIVE                                        \
        lwrtosSemaphoreGive(PwrMonitorChannelQueryMutex)
#else
    #define PWR_MONITOR_SEMAPHORE_TAKE lwosNOP()
    #define PWR_MONITOR_SEMAPHORE_GIVE lwosNOP()
#endif

/*!
 * Helper macros to wrap channel status reads from pwrmonitor
 */
#define PWR_MONITOR_QUERY_PROLOGUE(pMon, bFromRM)                             \
    do {                                                                      \
        OSTASK_OVL_DESC _ovlDesc[] = {                                        \
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibQuery)               \
            OSTASK_OVL_DESC_DEFINE_LIB_PWR_MONITOR                            \
        };                                                                    \
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(_ovlDesc);                     \
        PWR_MONITOR_SEMAPHORE_TAKE;                                           \
        pwrMonitorChannelsTupleReset((pMon), (bFromRM));

#define PWR_MONITOR_QUERY_EPILOGUE(pMon, bFromRM)                             \
        pwrMonitorChannelsTupleReset((pMon), (bFromRM));                      \
        PWR_MONITOR_SEMAPHORE_GIVE;                                           \
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(_ovlDesc);                      \
    } while (LW_FALSE)

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * Main structure for power monitoring/capping
 */
typedef struct
{
    LwU8                type; //<! RM_PMU_PMGR_PWR_MONITOR_TYPE_<xyz>

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_1X)
    /*!
     * Sampling period for power monitoring (ms)
     */
    LwU16               samplingPeriodms;
    /*!
     * Sampling period for "low power" (ms).  "Low power" is defined as when the
     * GPU is in the lowest pstate, but the PMU PWR code is not holding the GPU
     * there.
     */
    LwU16               samplingPeriodLowPowerms;
    /*!
     * Sampling period for power monitoring (PMU OS ticks)
     */
    LwLength            samplingPeriodTicks;
    /*!
     * Sampling period for "low power" (PMU OS ticks).
     */
    LwLength            samplingPeriodLowPowerTicks;

    /*!
     * Number of samples to collect for each iteration.  Thus the total period
     * of an iteration (ms) = samplingPeriodms * sampleCount.
     */
    LwU8                sampleCount;
    /*!
     * Current sample number of PWR_MONITORing algorithm, such that when
     * sampleCount == lwrrentSample, the algorithm will complete and iteration
     * and possibly call any power capping.
     */
    LwU8                lwrrentSample;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_1X)

    /*!
     * Set of channels, that, when added up, yield total GPU power
     */
    LwU32               totalGpuPowerChannelMask;
    /*!
     * Set of all physical channels
     */
    LwU32               physicalChannelMask;
    /*!
     * Mask of all channels which can be queried to provide valid tuple values
     */
    BOARDOBJGRPMASK_E32 readableChannelMask;
} PWR_MONITOR;

/*!
 * Constructs the PWR_MONITOR object, including the implementation-specific
 * manner.
 */
#define PwrMonitorConstruct(fname) FLCN_STATUS (fname)(PWR_MONITOR **ppMonitor)

/*!
 * @interface PWR_MONITOR
 *
 * Responsible for monitor-specific HW-initialization and configuration.
 *
 * @note
 *     This PWR_MONITOR interface is optional. The need for this function is
 *     purely a function of the monitor-type and the types of channels attached
 *     to the monitor.  If no HW-actions are required for this monitor-type do
 *     not provide an implementation and simply allow the stub in @ref pwrdev.c
 *     return FLCN_OK when called for instances of this type.
 *
 * @param[in] pMon   Pointer to a PWR_MONITOR object
 *
 * @return FLCN_OK
 *     Upon successful hardware initialization/configuration.
 *
 * @return pmuI2cError<xyz>
 *     Internal I2C failure (Address NACK, SCL timeout, etc ...) for I2C power
 *     devices.
 */
typedef FLCN_STATUS PwrMonitorLoad (PWR_MONITOR *pMon);

/*!
 * @interface PWR_MONITOR
 *
 * Returns the sample delay/polling period (in ticks) for the PWR_MONITOR object.
 * This interface is expected to be called by the PMUTASK_PMGR control loop for
 * determining the delay time between calling the @ref PwrMonitor interface.
 *
 * @param[in] pMon   Pointer to a PWR_MONITOR object
 *
 * @return @ref PWR_MONITOR::samplingPeriodTicks
 */
#define PwrMonitorDelay(fname) LwU32 (fname) (PWR_MONITOR *pMon)

/*!
 * @interface PWR_MONITOR
 *
 * Performs a sample, and possibly completes iteration, of the the power
 * monitoring algorithm.
 *
 * @param[in] pMon   Pointer to a PWR_MONITOR object
 *
 * @return LW_TRUE
 *     PWR_MONITOR completed an interation
 *
 * @return LW_FALSE
 *     PWR_MONITOR is still in the middle of an iteration
 */
#define PwrMonitor(fname) LwBool (fname) (PWR_MONITOR *pMon)

/*!
 * @interface PWR_MONITOR
 *
 * Retrieves the latest PWR_CHANNEL status.
 *
 * @param[in]  pMon           Pointer to a PWR_MONITOR object
 * @param[in]  channelIdx     Index for channel to retrieve
 * @param[out] pPowermW       Pointer to the power value
 *
 * @return FLCN_OK
 *     Samples successfully retrieved
 *
 * @return FLCN_ERR_NOT_SUPPORTED
 *     An unsupported channel is specified in the mask
 *
 * @return FLCN_ERROR
 *     Unexpected internal error
 */
#define PwrMonitorChannelStatusGet(fname) FLCN_STATUS (fname) (PWR_MONITOR *pMon, LwU8 channelIdx, LwU32 *pPowermW)

/*!
 * @interface PWR_MONITOR
 *
 * Handles the RM/PMU RM_PMU_PMGR_CMD_ID_PWR_CHANNELS_QUERY.  Iterates
 * over the requested channels (@ref
 * RM_PMU_PMGR_CMD_PWR_CHANNELS_QUERY:channelMask) and calls @ref
 * PwrMonitorQueryChannel for each channel.
 *
 * @param[in]  pMon           Pointer to a PWR_MONITOR object
 * @param[in]  pwrChannelMask Mask of PWR_CHANNELs to retrieve
 * @param[out] pPayload       @ref RM_PMU_PMGR_PWR_MONITOR_SAMPLES_PAYLOAD
 *
 * @return FLCN_OK
 *     Samples successfully retrieved
 *
 * @return FLCN_ERR_NOT_SUPPORTED
 *     An unsupported channel is specified in the mask
 *
 * @return FLCN_ERROR
 *     Unexpected internal error
 */
#define PwrMonitorChannelsQuery(fname) FLCN_STATUS (fname) (PWR_MONITOR *pMon, LwU32 channelMask, RM_PMU_PMGR_PWR_CHANNELS_QUERY_PAYLOAD *pPayload)


/* ------------------------- Defines --------------------------------------- */
#define PMU_PMGR_PWR_MONITOR_SUPPORTED()                                    \
        (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR) &&                    \
        (Pmgr.pPwrMonitor != NULL) &&                                       \
        (Pmgr.pPwrMonitor->type != RM_PMU_PMGR_PWR_MONITOR_TYPE_NO_POLLING))
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
FLCN_STATUS pmgrPwrMonitorChannelQuery(DISPATCH_PMGR_PWR_CHANNEL_QUERY *pPwrChannelQuery)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pmgrPwrMonitorChannelQuery");

/*!
 * PWR_MONITOR interfaces
 */
PwrMonitorConstruct            (pwrMonitorConstruct)
    GCC_ATTRIB_SECTION("imem_libPmgrInit", "pwrMonitorConstruct");
PwrMonitorDelay                (pwrMonitorDelay);
PwrMonitor                     (pwrMonitor);
PwrMonitorChannelStatusGet     (pwrMonitorChannelStatusGet);
PwrMonitorChannelsQuery        (pwrMonitorChannelsQuery);

/*!
 * Board Object interfaces.
 */
BoardObjGrpIfaceModel10SetHeader  (pmgrPwrMonitorIfaceModel10SetHeader_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pmgrPwrMonitorIfaceModel10SetHeader_SUPER");
BoardObjGrpIfaceModel10SetHeader  (pmgrPwrMonitorIfaceModel10SetHeader_DEFAULT)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pmgrPwrMonitorIfaceModel10SetHeader_DEFAULT");
BoardObjGrpIfaceModel10SetHeader  (pmgrPwrMonitorIfaceModel10SetHeader_PSTATE)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pmgrPwrMonitorIfaceModel10SetHeader_PSTATE");
BoardObjGrpIfaceModel10SetHeader  (pmgrPwrMonitorIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pmgrPwrMonitorIfaceModel10SetHeader");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */
#include "pmgr/pwrmonitor_2x.h"
#include "pmgr/pwrmonitor_35.h"

#endif // PWRMONITOR_H
