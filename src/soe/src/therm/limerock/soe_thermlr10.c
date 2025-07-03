/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/
/* ------------------------ System Includes -------------------------------- */
#include "soesw.h"
/* ------------------------ Application Includes --------------------------- */
#include "dev_therm.h"
#include "dev_soe_csb.h"
#include "dev_lwlsaw_ip.h"
#include "soe_bar0.h"
#include "soe_objsoe.h"
#include "soe_objtherm.h"
#include "main.h"
#include "soe_objsaw.h"
#include "soe_os.h"
#include "soe_timer.h"
#include "soe_cmdmgmt.h"
#include "soe_bar0.h"
#include "soe_lwlink.h"
#include "config/g_timer_hal.h"
#include "config/g_lwlink_hal.h"
#include "config/g_intr_hal.h"
#include "config/g_therm_hal.h"
#include "config/g_saw_hal.h"

/* ------------------------ Static Variables ------------------------------- */
static LwU32 minSlmTime;
static LwU32 warnThreshold;
static RM_FLCN_CMD_SOE thermCmd;

/* ------------------------- Macros and Defines ----------------------------- */
#define THERM_MIN_SLM_TIME_LR10     1000000 // in usec

/*!
 * @brief Returns MAX_OFFSET temperature of all the internal sensors 
 *        The return value equals max temperature plus the sensor offset.
 *
 */
LwTemp
thermGetTsenseMax_LR10(void)
{
    LwU32 maxTemp;

    maxTemp = REG_RD32(BAR0, LW_THERM_TSENSE_MAXIMUM_TEMPERATURE);
    maxTemp = DRF_VAL(_THERM_TSENSE, _MAXIMUM_TEMPERATURE,
                      _MAXIMUM_TEMPERATURE, maxTemp);

    return LW_TSENSE_FXP_9_5_TO_24_8(maxTemp);
}

/*!
 * @brief Returns TSENSE thermal limits
 */
FLCN_STATUS
thermGetTsenseThresholds_LR10
(
    LwU32 thresholdIdx,
    LwTemp *pThresholdValue
)
{
    LwU32 threshold = REG_RD32(BAR0, LW_THERM_TSENSE_THRESHOLD_TEMPERATURES);

    if (thresholdIdx == TSENSE_OVERT_THRESHOLD)
    {
        threshold = DRF_VAL(_THERM_TSENSE, _THRESHOLD_TEMPERATURES,
                            _OVERTEMP_TEMPERATURE, threshold);
    }
    else if (thresholdIdx == TSENSE_WARN_THRESHOLD)
    {
        threshold = DRF_VAL(_THERM_TSENSE, _THRESHOLD_TEMPERATURES,
                            _WARNING_TEMPERATURE, threshold);
    }
    else
    {
        return FLCN_ERROR;
    }

    *pThresholdValue = LW_TSENSE_FXP_9_5_TO_24_8(threshold);

    return FLCN_OK;
}

/*!
 * @brief Send slowdown message to the driver.
 */
void
thermSendSlowdownMessage_LR10
(
    LwBool slm
)
{
    DISP2UNIT_CMD disp2Therm = { 0 };
    RM_SOE_THERM_CMD_SEND_ASYNC_MSG *pMsg;
    LwU32 maxTemperature;
    LwU32 warningThreshold;
    LwU32 source;

    disp2Therm.eventType = DISP2UNIT_EVT_COMMAND;
    disp2Therm.cmdQueueId = 0;
    disp2Therm.pCmd = &thermCmd;

    memset(&thermCmd, 0, sizeof(thermCmd));

    thermCmd.hdr.unitId = RM_SOE_UNIT_THERM;
    thermCmd.hdr.ctrlFlags = 0;
    thermCmd.hdr.seqNumId = 0;
    thermCmd.hdr.size = sizeof(thermCmd);
    thermCmd.cmd.therm.cmdType = RM_SOE_THERM_SEND_MSG_TO_DRIVER;

    pMsg = &thermCmd.cmd.therm.msg;
    pMsg->status.msgType = RM_SOE_THERM_MSG_ID_SLOWDOWN_STATUS;
    pMsg->status.slowdown.bSlowdown = slm;

    maxTemperature = ISR_REG_RD32(LW_THERM_TSENSE_MAXIMUM_TEMPERATURE);
    maxTemperature = DRF_VAL(_THERM_TSENSE, _MAXIMUM_TEMPERATURE, 
                             _MAXIMUM_TEMPERATURE, maxTemperature);
    pMsg->status.slowdown.maxTemperature = LW_TSENSE_FXP_9_5_TO_24_8(maxTemperature);

    warningThreshold = ISR_REG_RD32(LW_THERM_TSENSE_THRESHOLD_TEMPERATURES);
    warningThreshold = DRF_VAL(_THERM_TSENSE, _THRESHOLD_TEMPERATURES,
                               _WARNING_TEMPERATURE, warningThreshold);
    pMsg->status.slowdown.warnThreshold = LW_TSENSE_FXP_9_5_TO_24_8(warningThreshold);

    if (slm)
    {
        source = SAW_ISR_REG_RD32(LW_LWLSAW_LWSTHERMALEVENT);
        pMsg->status.slowdown.source.bTsense =
            FLD_TEST_DRF(_LWLSAW, _LWSTHERMALEVENT, _TSENSE_THERM_ALERT_STATUS,
                         _TSENSE_THERMAL_ALERT_INPROGRESS, source);
        pMsg->status.slowdown.source.bPmgr =
            FLD_TEST_DRF(_LWLSAW, _LWSTHERMALEVENT, _PMGR_THERM_ALERT_STATUS,
                         _PMGR_THERMAL_ALERT_INPROGRESS, source);
    }
    lwrtosQueueSendFromISR(Disp2QThermThd, &disp2Therm, sizeof(disp2Therm));
}

/*!
 * @brief Send shutdown message to the driver.
 */
void
thermSendShutdownMessage_LR10(void)
{
    DISP2UNIT_CMD disp2Therm = { 0 };
    LwU32 maxTemperature;
    LwU32 overtThreshold;
    LwU32 source;
    RM_SOE_THERM_CMD_SEND_ASYNC_MSG *pMsg;

    disp2Therm.eventType = DISP2UNIT_EVT_COMMAND;
    disp2Therm.cmdQueueId = 0;
    disp2Therm.pCmd = &thermCmd;

    memset(&thermCmd, 0, sizeof(thermCmd));

    thermCmd.hdr.unitId = RM_SOE_UNIT_THERM;
    thermCmd.hdr.ctrlFlags = 0;
    thermCmd.hdr.seqNumId = 0;
    thermCmd.hdr.size = sizeof(thermCmd);
    thermCmd.cmd.therm.cmdType = RM_SOE_THERM_SEND_MSG_TO_DRIVER;

    pMsg = &thermCmd.cmd.therm.msg;
    pMsg->status.msgType = RM_SOE_THERM_MSG_ID_SHUTDOWN_STATUS;

    maxTemperature = ISR_REG_RD32(LW_THERM_TSENSE_MAXIMUM_TEMPERATURE);
    maxTemperature = DRF_VAL(_THERM_TSENSE, _MAXIMUM_TEMPERATURE,
                             _MAXIMUM_TEMPERATURE, maxTemperature);
    pMsg->status.shutdown.maxTemperature = LW_TSENSE_FXP_9_5_TO_24_8(maxTemperature);

    overtThreshold = ISR_REG_RD32(LW_THERM_TSENSE_THRESHOLD_TEMPERATURES);
    overtThreshold = DRF_VAL(_THERM_TSENSE, _THRESHOLD_TEMPERATURES,
                             _OVERTEMP_TEMPERATURE, overtThreshold);
    pMsg->status.shutdown.overtThreshold = LW_TSENSE_FXP_9_5_TO_24_8(overtThreshold);

    source = SAW_ISR_REG_RD32(LW_LWLSAW_LWSTHERMALEVENT);
    pMsg->status.shutdown.source.bTsense =
        FLD_TEST_DRF(_LWLSAW, _LWSTHERMALEVENT, _TSENSE_THERM_SHUTDOWN_STATUS,
                     _TSENSE_THERMAL_SHUTDOWN_INPROGRESS, source);
    pMsg->status.shutdown.source.bPmgr =
        FLD_TEST_DRF(_LWLSAW, _LWSTHERMALEVENT, _PMGR_THERM_SHUTDOWN_STATUS,
                     _PMGR_THERMAL_SHUTDOWN_INPROGRESS, source);

    lwrtosQueueSendFromISR(Disp2QThermThd, &disp2Therm, sizeof(disp2Therm));
}

/*!
 * @brief Handle Thermal OVERT Interrupt.
 *
 * The chip goes to shutdown during an OVERT.
 *
 * We are lucky if we can log this interrupt before shutdown.
 *
 * This interrupt will:
 *  1. Post a message to the driver indicating an OVERT.
 *  2. Disable OVER_TEMP interrupt in SAW.
 *  3. Exit the handler.
 *
 */
void
thermServiceOvert_LR10(void)
{
    // Send shutdown message to the driver
    thermSendShutdownMessage_HAL();

    // Disable the Overt interrupt to prevent sticky error
    intrDisableThermalOvertIntr_HAL();
}

/*!
 * @brief Check if Thermal Event is Active
 *
 * TODO : Checks SAW thermal event register
 * 
 */
LwBool
thermCheckThermalEventStatus_LR10(void)
{
    LwU32 data32;

    data32 = SAW_ISR_REG_RD32(LW_LWLSAW_LWSTHERMALEVENT);
    return FLD_TEST_DRF(_LWLSAW, _LWSTHERMALEVENT, _THERM_ALERT_STATUS,
                       _THERMAL_ALERT_INPROGRESS, data32);
}

/*!
 * @brief Handle Thermal ALERT Interrupt.
 *
 * TODO : This callback function will have to:
 *   1. If the Alert is still in progress
 *      a. Re-assert slm on all the links
 *   2. If the Alert is cleared
 *      a. Stop the timer
 *      b. Unassert SLM
 *      c. Post a message to driver indicating ALERT is gone
 *      d. Clear the WARN interrupt and enable it back in SAW
 *      e. Exit
 */
void
thermServiceAlertTimerCallback_LR10(void)
{
    LwBool slm;

    slm = thermCheckThermalEventStatus_HAL();
    if (!slm)
    {
        // Stop GP timer
        timerSetup_HAL(&Timer, TIMER_MODE_STOP,
                       TIMER_EVENT_NONE, 0x0, 0x0);

        // Get links out of Single Lane Mode(SLM)
#if (SOECFG_FEATURE_ENABLED(SOE_LWLINK_SLM))
        lwlinkForceSlmAll_HAL(&Lwlink, slm);
#endif

        // Send slowdown message to the driver
        thermSendSlowdownMessage_HAL(&Therm, slm);

        // Clear warn interrupt
        sawClearThermalAlertIntr_HAL();

        // Enable back WARN interrupt
        intrEnableThermalAlertIntr_HAL();
    }
    else
    {
#if (SOECFG_FEATURE_ENABLED(SOE_LWLINK_SLM))
        lwlinkForceSlmAll_HAL(&Lwlink, slm);
#endif
    }
}

/*!
 * @brief Handle Thermal ALERT Interrupt.
 *
 * This interrupt will:
 *  1. Disable WARN interrupt in SAW.
 *  2. Assert Thermal Slowdown.
 *  3. Post a message to the driver indicating an ALERT.
 *  4. Kick off a 1 second timer in continous mode using callback function.
 *  5. Exit the handler.
 *
 */
void
thermServiceAlert_LR10(void)
{
    LwU32 intervalUs = minSlmTime;
    LwBool slm = LW_TRUE;
    FLCN_STATUS status;

    // Disable WARN interrupt to prevent sticky error
    intrDisableThermalAlertIntr_HAL();

    // Set links to Single Lane Mode(SLM)
#if (SOECFG_FEATURE_ENABLED(SOE_LWLINK_SLM))
    lwlinkForceSlmAll_HAL(&Lwlink, slm);
#endif

    // Send slowdown message to the driver
    thermSendSlowdownMessage_HAL(&Therm, slm);

    // Start GP timer
    status = timerSetup_HAL(&Timer, TIMER_MODE_START, TIMER_EVENT_THERM,
                            intervalUs, LW_FALSE);
    if (status != FLCN_OK)
    {
        SOE_HALT();
    }
}

/*!
 * @brief Force/Revert Thermal slowdown.
 *
 * 1. Thermal slowdown is forced by by lowering the WARN threshold to the lowest possible value.
 * 2. This triggers a TSENSE thermal alert interrupt.
 * 3. The Thermal alert handler will set links to SLM and force a slowdown.
 * 4. The slowdown is reverted by setting back the WARN threshold to the default value.
 * 5. The thermal alert callback function will revert the links from SLM.
 *
 */
FLCN_STATUS
thermForceSlowdown_LR10
(
    LwBool bSlm,
    LwU32  periodUs
)
{
    if (bSlm)
    {
        minSlmTime = periodUs;

        // Set WARN threshold to lowest possible value
        REG_FLD_WR_DRF_NUM(BAR0, _THERM_TSENSE, _THRESHOLD_TEMPERATURES, _WARNING_TEMPERATURE,
                   LW_TSENSE_COLWERT_TO_FXP_9_5(-256));
    }
    else
    {
        REG_FLD_WR_DRF_NUM(BAR0, _THERM_TSENSE, _THRESHOLD_TEMPERATURES,
                   _WARNING_TEMPERATURE, warnThreshold);

        // Set minimum slm time back to its default value
        minSlmTime = THERM_MIN_SLM_TIME_LR10;
    }

    return FLCN_OK;
}

/*!
 * Setup required thermal HW state during SOE Pre-Init.
 */
FLCN_STATUS
thermPreInit_LR10(void)
{
    //
    // Initialization of TSENSE-BJT's and trip limits is moved to the firmware.
    // It is not needed to be setup in the SOE.
    //

    // Enable thermal interrupts from internal TSENSE and external TDIODE sensors.
    REG_WR32(SAW, LW_LWLSAW_THERMAL_CTRL,
        DRF_DEF(_LWLSAW, _THERMAL_CTRL, _TSENSE_OVERTEMP_EN, _ENABLED) |
        DRF_DEF(_LWLSAW, _THERMAL_CTRL, _TSENSE_OVERTEMP_ALERT_EN, _ENABLED) |
        DRF_DEF(_LWLSAW, _THERMAL_CTRL, _PMGR_OVERTEMP_EN, _ENABLED) |
        DRF_DEF(_LWLSAW, _THERMAL_CTRL, _PMGR_OVERTEMP_ALERT_EN, _ENABLED));

    warnThreshold = REG_FLD_RD_DRF(BAR0, _THERM_TSENSE, _THRESHOLD_TEMPERATURES,
                               _WARNING_TEMPERATURE);

    minSlmTime = THERM_MIN_SLM_TIME_LR10;

    return FLCN_OK;
}
