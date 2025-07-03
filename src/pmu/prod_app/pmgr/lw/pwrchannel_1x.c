/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrchannel.c
 * @brief Interface specification for a PWR_CHANNEL.
 *
 * A "Power Channel" is a logical construct representing a collection of power
 * sensors whose combined power (and possibly voltage or current, in the future)
 * are monitored together as input to a PWR_MONITOR algorithm.
 *
 * This input will be "sampled" 1 or more times for each "iteration" and
 * statistical data (mean, min, max - possibly will be expanded) will be
 * collected from the samples as possible input for each iteration of the
 * PWR_MONITOR algorithm.
 *
 * This channel of readings might may also be assigned a power limit which a
 * PWR_MONITOR algorithm may try to enforce with some sort of corrective
 * algorithm (clock slowdown, pstate changes, etc.).
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "pmgr/pwrchannel.h"
#include "pmgr/pwrchannel_1x.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * The power value to use/assume when device tampering is detected.  This is an
 * extermely high value so that power capping/policies immediately engage to
 * hinder performance (and protect the board).
 *
 * Current value is 500001 mW.
 *
 * @see _device_tampering for more additional information regarding how device-
 *      tampering is handled
 */
#define PWR_CHANNEL_TAMPER_PENALTY_VALUE_MW                               500001

/*!
 * This is the period over which the channel will report high power-telemetry
 * after a device on the channel has reported a tampering incident. After such
 * an incident, this period becomes active. If devices continue reporting
 * tampering while this period is active, the period is reset (ie. the penalty
 * is restarted).
 *
 * @see _device_tampering for more additional information regarding how device-
 *      tampering is handled
 */
#define  PWR_CHANNEL_TAMPER_PENALTY_PERIOD_NS                        3000000000u

/* ------------------------- Private Functions ------------------------------ */
static void                    s_pwrChannelReset(PWR_CHANNEL *pChannel);
static PwrChannelRefresh       (s_pwrChannelRefresh_DEFAULT)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "s_pwrChannelRefresh_DEFAULT");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc PwrChannelSample
 */
void
pwrChannelSample
(
    PWR_CHANNEL *pChannel
)
{
    LwU32       pwrSamplemW    = 0;
    LwBool      bApplyPenalty  = LW_FALSE;
    LwBool      bUseLastSample = LW_FALSE;
    FLCN_STATUS status         = FLCN_ERR_NOT_SUPPORTED;

    // Call into class-specific interface to retrieve appropriate sensor values.
    switch (BOARDOBJ_GET_TYPE(pChannel))
    {
        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SENSOR:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_TOPOLOGY);
            status = pwrChannelSample_SENSOR(pChannel, &pwrSamplemW);
            break;
        }

        //
        // All other channel types types don't have a concept of active
        // sampling.
        //
    }

    // If active sampling not supported on this channel, bail out early.
    if (status == FLCN_ERR_NOT_SUPPORTED)
    {
        return;
    }

    //
    // Device Tamper Detection:
    //
    // If we're not in an active penalty-period, honor the power-sample
    // just obtained from the device.
    //
    // If a penalty-period is active (meaning that a device on this channel
    // is compromised due to tampering), check to see how long it has been
    // active. If the penalty has expired, clear it and honor the current
    // sample. Otherwise, continue applying the channel's power-limit as
    // the penalty for device tampering.
    //
    // In case of an unexpected I2C error (which may also be the result of
    // device tampering), apply a one time penalty. Do this instead of
    // reporting zero for the device power as it is most conservative
    // action we can take.
    //
    if (status == FLCN_OK)
    {
        if (pChannel->penaltyStart.data != 0)
        {
            if (osPTimerTimeNsElapsedGet(&pChannel->penaltyStart)
                     < PWR_CHANNEL_TAMPER_PENALTY_PERIOD_NS)
            {
                bApplyPenalty = LW_TRUE;
            }
            else
            {
                pChannel->penaltyStart.data = 0;
            }
        }
    }
    //
    // When tampering has been detected, apply the penalty and start
    // the penalty timer so that it continues to get applied for the
    // full duration of the penalty-period.
    //
    else if (status == FLCN_ERR_PWR_DEVICE_TAMPERED)
    {
        bApplyPenalty = LW_TRUE;
        osPTimerTimeNsLwrrentGet(&pChannel->penaltyStart);
    }
    // For transient bus-errors, we should use the last good sample.
    else
    {
        bUseLastSample = LW_TRUE;
    }

    //
    // If applying penatly, override the current samples with the tampering
    // value.
    //
    if (bApplyPenalty)
    {
        pwrSamplemW = PWR_CHANNEL_TAMPER_PENALTY_VALUE_MW;
    }
    //
    // Update statistics for this channel when not applying a penalty.
    // Penalties should not affect the historical statistics.
    //
    else
    {
        // Use the last sample for transient bus errors.
        if (bUseLastSample)
        {
            pwrSamplemW = pChannel->pwrLastSamplemW;
        }
        // Otherwise, save this sample as the last sample
        else
        {
            pChannel->pwrLastSamplemW = pwrSamplemW;
        }

        // Min and max book-keeping.
        if (pwrSamplemW < pChannel->pwrLwrMinmW)
        {
            pChannel->pwrLwrMinmW = pwrSamplemW;
        }
        if (pwrSamplemW > pChannel->pwrLwrMaxmW)
        {
            pChannel->pwrLwrMaxmW = pwrSamplemW;
        }
    }

    pChannel->pwrSummW += pwrSamplemW;
    pChannel->sampleCount++;

    DBG_PRINTF_PMGR(("CS: s: 0x%02x, pA: %d, pS: %d, bP: %d\n",
        status, powermW, pwrSamplemW, bApplyPenalty));
}

/*!
 * @copydoc PwrChannelRefresh
 */
void
pwrChannelRefresh
(
    PWR_CHANNEL *pChannel
)
{
    LwS32 pwrAvgAdjmW;

    // Initialize the latest power average to 0.
    pChannel->pwrAvgmW = 0;

    switch (BOARDOBJ_GET_TYPE(pChannel))
    {
        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SUMMATION:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_TOPOLOGY);
            pwrChannelRefresh_SUMMATION(pChannel);
            break;
        }

        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_ESTIMATION:
        {
            break;
        }

        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_PSTATE_ESTIMATION_LUT:
        {
            break;
        }

        default:
        {
            s_pwrChannelRefresh_DEFAULT(pChannel);
            break;
        }
    }

    //
    // Power Correction via MX+B equation:
    // 1. Multiply by M.
    //
    if (pChannel->pwrCorrSlope != LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1))
    {
        //
        // Note: 32BIT_OVERFLOW - Possible FXP20.12 overflow, not going to fix
        // because this code segment is tied to the @ref PMU_PMGR_PWR_MONITOR_1X
        // feature and is not used on AD10X and later chips.
        //
        pChannel->pwrAvgmW = LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12,
            pChannel->pwrAvgmW * pChannel->pwrCorrSlope);
    }
    // 2. Add B.
    if (pChannel->pwrCorrOffsetmW != 0)
    {
        pwrAvgAdjmW = pChannel->pwrAvgmW + pChannel->pwrCorrOffsetmW;
        // Bound to 0, as power is an unsigned value.
        pChannel->pwrAvgmW = LW_MAX(0, pwrAvgAdjmW);
    }
}

/*!
 * @brief   Constructs PWR_CHANNEL 1X state.
 *
 * @param[in]   pChannel    Pointer to PWR_CHANNEL object
 * @param[in]   pDescTmp            Init data received from the RM
 * @param[in]   bFirstConstruct     Set if constructed for the first time
 */
FLCN_STATUS
pwrChannelConstructSuper_1X
(
    PWR_CHANNEL                          *pChannel,
    RM_PMU_PMGR_PWR_CHANNEL_BOARDOBJ_SET *pDescTmp,
    LwBool                                bFirstConstruct
)
{
    pChannel->pwrCorrSlope    = pDescTmp->pwrCorrSlope;
    pChannel->pwrCorrOffsetmW = pDescTmp->pwrCorrOffsetmW;

    // Zero all the channel's statistical data
    pChannel->pwrAvgmW = 0;
    pChannel->pwrMaxmW = 0;
    pChannel->pwrMinmW = 0;

    pChannel->penaltyStart.data = 0;

    s_pwrChannelReset(pChannel);

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Resets all PWR_CHANNEL state for the next iteration.
 *
 * @param[in] pChannel     Pointer to PWR_CHANNEL object
 */
static void
s_pwrChannelReset
(
    PWR_CHANNEL *pChannel
)
{
    pChannel->sampleCount = 0;
    pChannel->pwrSummW    = 0;
    pChannel->pwrLwrMinmW = LW_U32_MAX;
    pChannel->pwrLwrMaxmW = 0x0;
}

/*!
 * _DEFAULT implementation
 *
 * @copydoc PwrChannelRefresh
 */
static void
s_pwrChannelRefresh_DEFAULT
(
    PWR_CHANNEL *pChannel
)
{
    DBG_PRINTF_PMGR(("CR: pS: %d, c: %d\n", pChannel->pwrSummW, pChannel->sampleCount, 0, 0));
    pChannel->pwrAvgmW = (pChannel->pwrSummW + pChannel->sampleCount / 2) /
                            pChannel->sampleCount;
    s_pwrChannelReset(pChannel);
}
