/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrchannel_2x.c
 * @brief PMGR Power Channel Specific to Power Topology Table 2.0.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "pmgr/pwrchannel_2x.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
// Used for colwerting energy callwlated in pJ to mJ
#define PICO_TO_MILLI_COLWERSION (1000000000)

/* ------------------------- Private Functions ------------------------------ */
static void s_pwrChannelEnergyStatusEstimate(PWR_CHANNEL *pChannel, LW2080_CTRL_PMGR_PWR_TUPLE *pTuple)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChannelEnergyStatusEstimate");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Initialize Policy 3.X / Topology 2.X specific fields.
 *
 * @param[in]   pChannel            Pointer to PWR_CHANNEL object
 * @param[in]   pDescTmp            Init data received from the RM
 * @param[in]   bFirstConstruct     Set if constructed for the first time
 */
FLCN_STATUS
pwrChannelConstructSuper_2X
(
    PWR_CHANNEL                          *pChannel,
    RM_PMU_PMGR_PWR_CHANNEL_BOARDOBJ_SET *pDescTmp,
    LwBool                                bFirstConstruct
)
{
    pChannel->voltFixeduV       = pDescTmp->voltFixeduV;
    pChannel->lwrrCorrSlope     = pDescTmp->lwrrCorrSlope;
    pChannel->lwrrCorrOffsetmA  = pDescTmp->lwrrCorrOffsetmA;
    pChannel->dependentChMask   = pDescTmp->dependentChMask;
    pChannel->bTupleCached      = LW_FALSE;

    // Initialize Energy counter related fields.
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_ENERGY_COUNTER))
    {
        if (bFirstConstruct)
        {
            memset(&pChannel->channelCachedTuple.energymJ, 0,
                   sizeof(LwU64_ALIGN32));
            osPTimerTimeNsLwrrentGet(&(pChannel->lastQueryTime));
        }

        // Set the HW energy support flag. If not set is estimated otherwise.
        pChannel->bIsEnergySupported = pDescTmp->bIsEnergySupported;
    }

    return FLCN_OK;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrChannelIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJ *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SENSOR_CLIENT_ALIGNED:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_CHANNEL_SENSOR_CLIENT_ALIGNED);
            status = pwrChannelIfaceModel10GetStatus_SENSOR_CLIENT_ALIGNED(pModel10, pPayload);
            break;
        }
        default:
        {
            RM_PMU_PMGR_PWR_CHANNEL_STATUS *pGetStatus =
                (RM_PMU_PMGR_PWR_CHANNEL_STATUS *)pPayload;
            LW2080_CTRL_PMGR_PWR_TUPLE     *pTuple =
                (LW2080_CTRL_PMGR_PWR_TUPLE *)&(pGetStatus->tuple);

            //
            // TODO-Tariq: Will need a PWR_CHANNEL_1X implementation of this to
            // enable this path generically for all PWR_CHANNEL classes.
            //
            status = pwrMonitorChannelTupleStatusGet(Pmgr.pPwrMonitor,
                BOARDOBJ_GET_GRP_IDX_8BIT(pBoardObj), pTuple);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pwrChannelIfaceModel10GetStatus_exit;
            }

            // Call into _SUPER implementation to handle the rest.
            status = pwrChannelGetStatus_SUPER(pBoardObj, pPayload, LW_FALSE);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pwrChannelIfaceModel10GetStatus_exit;
            }
            break;
        }
    }

pwrChannelIfaceModel10GetStatus_exit:
    return status;
}

/*!
 * SUPER implementation.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrChannelGetStatus_SUPER
(
    BOARDOBJ        *pBoardObj,
    RM_PMU_BOARDOBJ *pPayload,
    LwBool          bLwrrAdjRequired
)
{
    BOARDOBJ_IFACE_MODEL_10 *pModel10 =
        boardObjIfaceModel10FromBoardObjGet(pBoardObj);
    PWR_CHANNEL *pChannel = (PWR_CHANNEL *)pBoardObj;
    RM_PMU_PMGR_PWR_CHANNEL_STATUS *pGetStatus =
        (RM_PMU_PMGR_PWR_CHANNEL_STATUS *)pPayload;
    LW2080_CTRL_PMGR_PWR_TUPLE     *pTuple =
        (LW2080_CTRL_PMGR_PWR_TUPLE *)&(pGetStatus->tuple);
    FLCN_STATUS status = FLCN_OK;

    // Call into BOARDOBJ super implementation.
    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrChannelGetStatus_SUPER_exit;
    }

    if (bLwrrAdjRequired)
    {
        // Apply current correction and re-callwlate power.
        if ((pChannel->lwrrCorrSlope != LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1)) ||
            (pChannel->lwrrCorrOffsetmA != 0))
        {
            LwU32 lwrrScaled;
            LwS32 lwrrAdjmA;

            // Multiply by M.
            pTuple->lwrrmA = LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12,
                pTuple->lwrrmA * pChannel->lwrrCorrSlope);

            // Add B.
            lwrrAdjmA = pTuple->lwrrmA + pChannel->lwrrCorrOffsetmA;
            // Bound to 0, as current is an unsigned value.
            pTuple->lwrrmA = LW_MAX(0, lwrrAdjmA);

            // Re-callwlate power.
            lwrrScaled = LW_UNSIGNED_ROUNDED_DIV(pTuple->pwrmW * 1000,
                LW_UNSIGNED_ROUNDED_DIV(pTuple->voltuV, 1000));
            // Multiply by M.
            lwrrScaled = LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12,
                lwrrScaled * pChannel->lwrrCorrSlope);

            // Add B.
            lwrrAdjmA = lwrrScaled + pChannel->lwrrCorrOffsetmA;
            // Bound to 0, as current is an unsigned value.
            lwrrScaled = LW_MAX(0, lwrrAdjmA);
            pTuple->pwrmW = LW_UNSIGNED_ROUNDED_DIV((lwrrScaled *
                LW_UNSIGNED_ROUNDED_DIV(pTuple->voltuV, 1000)), 1000);
        }
    }

    // If PWR_CHANNEL_35 enabled, call into it as a "pre"-SUPER.
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_CHANNEL_35))
    {
        status = pwrChannelIfaceModel10GetStatus_35(pModel10, pPayload);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pwrChannelGetStatus_SUPER_exit;
        }
    }
    else
    {
        // For pre-PWR_CHANNEL_35, faking tuplePolled by copying out tuple.
        pGetStatus->tuplePolled = pGetStatus->tuple;
    }

pwrChannelGetStatus_SUPER_exit:
    return status;
}

/*!
 * @copydoc PwrChannelTupleStatusGet
 */
FLCN_STATUS
pwrChannelTupleStatusGet
(
    PWR_CHANNEL                *pChannel,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    FLCN_STATUS      status = FLCN_ERR_NOT_SUPPORTED;
    LwS32            lwrrAdjmA;

    // Return cached tuple, if applicable.
    if (pChannel->bTupleCached)
    {
        *pTuple = pChannel->channelCachedTuple;
        return FLCN_OK;
    }

    switch (BOARDOBJ_GET_TYPE(pChannel))
    {
        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SENSOR:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_TOPOLOGY);
            status = pwrChannelTupleStatusGet_SENSOR(pChannel, pTuple);
            break;
        }
        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_ESTIMATION:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_TOPOLOGY);
            status = pwrChannelTupleStatusGet_ESTIMATION(pChannel, pTuple);
            break;
        }
        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SUMMATION:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_TOPOLOGY);
            status = pwrChannelTupleStatusGet_SUMMATION(pChannel, pTuple);
            break;
        }
        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_PSTATE_ESTIMATION_LUT:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_CHANNEL_PSTATE_ESTIMATION_LUT);
            status = pwrChannelTupleStatusGet_PSTATE_ESTIMATION_LUT(pChannel, pTuple);
            break;
        }
        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SENSOR_CLIENT_ALIGNED:
        {
            //
            // This channel type is supported only when PWR_CHANNEL_35 is
            // enabled and hence the get status call for this type should be
            // routed to pwrChannelIfaceModel10GetStatus() and should never reach this
            // function. If SUMMATION channel relwrsively calls into this
            // function the tuple should have been cached and can be returned
            // early.
            //
            status = FLCN_ERR_NOT_SUPPORTED;
            PMU_BREAKPOINT();
            break;
        }
    }

    if (status == FLCN_OK)
    {
        // Apply current correction and re-callwlate power.
        if ((pChannel->lwrrCorrSlope != LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1)) ||
            (pChannel->lwrrCorrOffsetmA != 0))
        {
            // Multiply by M.
            pTuple->lwrrmA = LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12,
                pTuple->lwrrmA * pChannel->lwrrCorrSlope);

            // Add B.
            lwrrAdjmA = pTuple->lwrrmA + pChannel->lwrrCorrOffsetmA;
            // Bound to 0, as current is an unsigned value.
            pTuple->lwrrmA = LW_MAX(0, lwrrAdjmA);

            // Re-callwlate power.
            pTuple->pwrmW =
                LW_UNSIGNED_ROUNDED_DIV((pTuple->lwrrmA *
                 LW_UNSIGNED_ROUNDED_DIV(pTuple->voltuV, 1000)), 1000);
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_ENERGY_COUNTER) &&
                !PWR_CHANNEL_IS_ENERGY_SUPPORTED(pChannel))
        {
            s_pwrChannelEnergyStatusEstimate(pChannel, pTuple);
        }

        // Cache the tuple.
        pChannel->bTupleCached = LW_TRUE;
        pChannel->channelCachedTuple = *pTuple;

        // Update lastQueryTime.
        osPTimerTimeNsLwrrentGet(&pChannel->lastQueryTime);
    }

    return status;
}

/*!
 * Resets the PWR_CHANNEL tuple status.
 *
 * @param[in]   pChannel    PWR_CHANNEL object pointer.
 * @param[in]   bFromRM     Boolean indicating if this is ilwoked from RM.
 */
void
pwrChannelTupleStatusReset
(
    PWR_CHANNEL    *pChannel,
    LwBool          bFromRM
)
{
    // If this request is from RM and not expired, don't reset this channel.
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_2X_MIN_CLIENT_SAMPLE_PERIOD) &&
        bFromRM)
    {
        FLCN_TIMESTAMP  lwrTime;
        FLCN_TIMESTAMP  elapsedTime;
        LwU32           elapsedMs = 0;

        osPTimerTimeNsLwrrentGet(&lwrTime);
        lw64Sub(&elapsedTime.data, &lwrTime.data, &pChannel->lastQueryTime.data);

        //
        // The following callwlation will always round down numbers. This way we
        // won't trigger false expiration due to round-up.
        //
        elapsedMs = elapsedTime.parts.hi * 4295 + elapsedTime.parts.lo / 1000000;

        if (elapsedMs < PWR_POLICIES_3X_GET_MIN_CLIENT_SAMPLE_PERIOD_MS())
        {
            return;
        }
    }

    // Clear tuple cache.
    pChannel->bTupleCached = LW_FALSE;
}

/*!
 * Estimates total energy aclwmmulated on a channel using cached energy counter,
 * queried power and time difference between the two measurements.
 *
 * @param[in]   pChannel    PWR_CHANNEL object pointer.
 * @param[out]  pStatus     @ref LW2080_CTRL_PMGR_PWR_TUPLE.
 *
 * @pre Requires libLw64 and libLw64Umul to be loaded before invocation
 */
void
s_pwrChannelEnergyStatusEstimate
(
    PWR_CHANNEL    *pChannel,
    LW2080_CTRL_PMGR_PWR_TUPLE
                   *pTuple
)
{
    FLCN_TIMESTAMP  lwrTime;
    FLCN_TIMESTAMP  elapsedTime;
    LwU64           diffEnergypJ;
    LwU64           diffEnergymJ64;
    LwU64           powermW     = pTuple->pwrmW;
    LwU64           picoToMilli = PICO_TO_MILLI_COLWERSION;
    LwU64_ALIGN32   diffEnergymJ;

    //
    // Find the time difference between now and the time when the last cached
    // energy counter was recorded
    //
    osPTimerTimeNsLwrrentGet(&lwrTime);
    lw64Sub(&elapsedTime.data, &lwrTime.data, &((pChannel->lastQueryTime).data));

    // Callwlate the energy in pJ
    lwU64Mul(&diffEnergypJ, &powermW, &elapsedTime.data);

    // Colwert to mJ
    lwU64Div(&diffEnergymJ64, &diffEnergypJ, &picoToMilli);

    // Colwert to LwU64_ALIGN32
    LwU64_ALIGN32_PACK(&diffEnergymJ, &diffEnergymJ64);

    // Accumulate energy value in status
    LwU64_ALIGN32_ADD(&pTuple->energymJ,
        &((pChannel->channelCachedTuple).energymJ),
        &diffEnergymJ);
}

/* ------------------------- Private Functions ------------------------------ */
