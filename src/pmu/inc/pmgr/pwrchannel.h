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
 * @file  pwrchannel.h
 * @brief @copydoc pwrchannel.c
 */

#ifndef PWRCHANNEL_H
#define PWRCHANNEL_H

/* ------------------------- System Includes ------------------------------- */
#include "boardobj/boardobjgrp.h"

/* ------------------------- Application Includes -------------------------- */
#include "pmu_objtimer.h"
#include "pwrchannel_status.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PWR_CHANNEL PWR_CHANNEL, PWR_CHANNEL_BASE;

/* ------------------------------ Macros ------------------------------------ */
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_PWR_CHANNEL \
    (&(Pmgr.pwr.channels.super))

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define PWR_CHANNEL_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(PWR_CHANNEL, (_objIdx)))

/*!
 * @copydoc BOARDOBJGRP_IS_VALID
 */
#define PWR_CHANNEL_IS_VALID(_objIdx) \
    (BOARDOBJGRP_IS_VALID(PWR_CHANNEL, (_objIdx)))

/*!
 * Initialize PWR_CHANNELS status mask
 *
 * @param[in]   _pMask       pointer to PWR_CHANNELS mask
 */
 #define PWR_CHANNELS_STATUS_MASK_INIT(_pMask)                                \
    do {                                                                      \
        boardObjGrpMaskInit_E32(_pMask);                                      \
    } while (LW_FALSE)

/* ------------------------- Types Definitions ----------------------------- */

/*!
 * Main structure representing a power capping channel
 */
struct PWR_CHANNEL
{
    /*!
     * BOARDOBJ super-class.
     */
    BOARDOBJ        super;

    LwU8            pwrRail;        //<! LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_<xyz>

    /*!
     * Power Monitor 1.X Only:
     */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_1X)
    LwUFXP20_12     pwrCorrSlope;   //<! Fixed-point multiplier for power used as a correction slope - FXP20.12
    LwS32           pwrCorrOffsetmW;//<! Power offset value (mW) to add to this channel
    LwU32           sampleCount;    //<! Number of samples lwrrently taken
    LwU32           pwrSummW;       //<! Sum of power measurements used for average (mW)
    LwU32           pwrAvgmW;       //<! Average Power of last iteration (mW)
    LwU32           pwrMaxmW;       //<! Max Power of last iteration (mW)
    LwU32           pwrLwrMaxmW;    //<! Max Power tracking of current iteration (mW)
    LwU32           pwrMinmW;       //<! Min Power of last iteration (mW)
    LwU32           pwrLwrMinmW;    //<! Min Power tracking of current iteration (mW)
    /*!
     * Bug 802747: This information is a hack for I2C errors (i.e. not tampered
     * PWR_DEVICEs).  When a read from a PWR_DEVICE propogates up an I2C_ERROR,
     * we'll just assume the last sample's values.  We'll keep this hack until
     * we can fix the I2C errors.
     */
    LwU32           pwrLastSamplemW;
    /*!
     * This is a timer used to denote the start of a tampering penalty-period.
     * The channel will use this to know when it can terminate a penalty-
     * period (when one is active) and return to it's normal state of reporting
     * good power-telementry.
     */
    FLCN_TIMESTAMP  penaltyStart;
#endif

    /*!
     * Power Policy 3.X / Power Topology 2.X Only:
     */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_2X)
    /*!
     * Is this channel support energy reading.
     */
    LwBool          bIsEnergySupported;
    /*!
     * Boolean indicating if this channel's tuple status is cached. Used to
     * eliminate duplicated query requests on a channel.
     */
    LwBool          bTupleCached;
    /*!
     * The cached tuple value used for query.
     */
    LW2080_CTRL_PMGR_PWR_TUPLE channelCachedTuple;
    /*!
     * Last query time. Indicating when last actual query on this channel is
     * requested.
     */
    FLCN_TIMESTAMP  lastQueryTime;
    /*!
     * Mask of all channel indexes on which this channel depends on.
     * This does not include the channel's own index
     */
    LwU32           dependentChMask;

    LwU32           voltFixeduV;        //<! Fixed voltage to assume for a PWR_CHANNEl.  Facilitates power <-> current colwersion

    LwUFXP20_12     lwrrCorrSlope;      //<! Fixed-point multiplier for current used as a correction slope - FXP20.12
    LwS32           lwrrCorrOffsetmA;   //<! Current offset value (mA) to add to this channel
#endif
};

/*!
 * @interface PWR_CHANNEL
 *
 * Samples the power sensors associated with this channel, by reading the power
 * (and possibly extended to the current and/or voltage) and updating the
 * statistical values associated with the iteration.
 *
 * @param[in] pChannel  Pointer to PWR_CHANNEL object
 */
#define PwrChannelSample(fname) void (fname)(PWR_CHANNEL *pChannel)

/*!
 * @interface PWR_CHANNEL
 *
 * Class-specific implementation to read the power in manner specific to the
 * given PWR_CHANNEL class.  Called by @ref pwrChannelSample() to handle
 * class-specific differences.
 *
 * @param[in]  pChannel  Pointer to PWR_CHANNEL object
 * @param[out] pPowermW  Pointer to LwU32 in which to return the sampled power.
 *
 * @return FLCN_OK
 *     Channel successfully sampled.
 * @return FLCN_ERR_PWR_DEVICE_TAMPERED
 *     Device tampering detected - should enforce penalty.
 * @return Other unexpected errors
 *     Transient, unexpected errors.  Most likely errors from buses which
 *     connect to the sensor.
 */
#define PwrChannelSample_IMPL(fname) FLCN_STATUS (fname)(PWR_CHANNEL *pChannel, LwU32 *pPowermW)

/*!
 * @interface PWR_CHANNEL
 *
 * Collects all the samples for this iteration and callwlates the final
 * statistical data for this iteration.
 *
 * @param[in] pChannel     Pointer to PWR_CHANNEL object
 */
#define PwrChannelRefresh(fname) void (fname)(PWR_CHANNEL *pChannel)

/*!
 * @interface PWR_CHANNEL
 *
 * Set power/current limit for this channel.
 *
 * @param[in]   pChannel    Pointer to PWR_CHANNEL object
 *
 * @param[in]   limitIdx    Limit index for this value
 *
 * @param[in]   limitUnit
 *      Units of requested limit as LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_<xyz>
 *
 * @param[in]   limitValue  Limit value for this channel
 *
 * @return  FLCN_OK
 *      This operation was completed successfully.
 *
 * @return  FLCN_ERR_NOT_SUPPORTED
 *      This operation is not supported on given channel.
 *
 * @return  other value
 *      Error code propagated from functions inside (like I2C operation
 *      failure).
 */
#define PwrChannelLimitSet(fname) FLCN_STATUS (fname)(PWR_CHANNEL *pChannel, LwU8 limitIdx, LwU8 limitUnit, LwU32 limitValue)

/*!
 * @interface PWR_CHANNEL
 *
 * Helper function which determines whether this PWR_CHANNEL "contains" a given
 * PWR_CHANNEL, as specified by a channel index (@ref chIdx).  A PWR_CHANNEL
 * "contains" a PWR_CHANNEL if it is the requested PWR_CHANNEL (i.e. same
 * channel index) or callwlates its value via PWR_CHRELATIONSHIP which
 * references the PWR_CHANNEL (directly or indirectly).
 *
 * @note This interface is expected to be called relwrsively for certain
 * PWR_CHANNEL classes.
 *
 * @param[in] pChannel  PWR_CHANNEL object pointer.
 * @param[in] chIdx
 *     Channel index of the PWR_CHANNEL to determine if "contained".
 * @param[in] chIdxEval
 *     Channel index of the PWR_CHANNEL which originally evaluated this
 *     function.  This is used to avoids cirlwlar references.
 *
 * @return LW_TRUE
 *     This PWR_CHANNEL contains the specified PWR_CHANNEL.
 * @return LW_FALSE
 *     This PWR_CHANNEL does not contain the specified PWR_CHANNEL.
 */
#define PwrChannelContains(fname) LwBool (fname)(PWR_CHANNEL *pChannel, LwU8 chIdx, LwU8 chIdxEval)

/*!
 * @interface PWR_CHANNEL
 *
 * Allocate memory for PWR_CHANNELS_STATUS structure
 *
 * @param[in] pChannelsStatus     Pointer to PWR_CHANNELS_STATUS object
 *
 * @return  FLCN_OK
 *      This operation was completed successfully.
 *
 * @return  FLCN_ERR_NO_FREE_MEM
 *      Memory could not be allocated
 */
#define PwrChannelsStatusAllocate(fname) FLCN_STATUS (fname)(PWR_CHANNELS_STATUS *pChannelsStatus)

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
FLCN_STATUS pwrChannelsLoad(void)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrChannelsLoad");
void        pwrChannelsUnload(void)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrChannelsUnload");

/*!
 * PWR_CHANNEL interfaces
 */
PwrChannelSample            (pwrChannelSample)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChannelSample");
PwrChannelRefresh           (pwrChannelRefresh)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChannelRefresh");
PwrChannelLimitSet          (pwrChannelLimitSet)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrChannelLimitSet");
PwrChannelContains          (pwrChannelContains)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChannelContains");

/*!
 * Constructor for the PWR_CHANNEL super-class.
 */
BoardObjGrpIfaceModel10ObjSet           (pwrChannelGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrChannelGrpIfaceModel10ObjSetImpl_SUPER");

/*!
 * Board Object interfaces.
 */
BoardObjGrpIfaceModel10CmdHandler       (pwrChannelBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrChannelBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10SetEntry   (pmgrPwrChannelIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pmgrPwrChannelIfaceModel10SetEntry");
BoardObjGrpIfaceModel10CmdHandler       (pwrChannelBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrChannelBoardObjGrpIfaceModel10GetStatus");
BoardObjGrpIfaceModel10GetStatusHeader  (pwrChannelIfaceModel10GetStatusHeaderPhysical)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrChannelIfaceModel10GetStatusHeaderPhysical");
BoardObjGrpIfaceModel10GetStatusHeader  (pwrChannelIfaceModel10GetStatusHeaderLogical)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrChannelIfaceModel10GetStatusHeaderLogical");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */
#include "pmgr/pwrchannel_2x.h"
#include "pmgr/pwrchannel_35.h"
#include "pmgr/pwrchannel_sensor.h"
#include "pmgr/pwrchannel_est.h"
#include "pmgr/pwrchannel_sum.h"
#include "pmgr/pwrchannel_pstate_est_lut.h"

#endif // PWRCHANNEL_H
