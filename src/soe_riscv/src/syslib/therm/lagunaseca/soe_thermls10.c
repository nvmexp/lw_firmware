/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
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
#include "tasks.h"
#include "soe_objsaw.h"
#include "cmdmgmt.h"
#include "soe_bar0.h"
#include "config/g_soe_hal.h"

#include "config/g_lwlink_hal.h"
#include "config/g_intr_hal.h"
#include "config/g_therm_hal.h"

/* ------------------------ Static Variables ------------------------------- */
static sysTASK_DATA LwU32 minL1Time;
static sysTASK_DATA LwU32 warnThreshold;
static sysTASK_DATA RM_FLCN_CMD_SOE thermCmd;
static sysTASK_DATA OS_TMR_CALLBACK thermTaskOsTmrCb;
/* ------------------------- Macros and Defines ----------------------------- */
#define THERM_MINIMUM_L1_TIME_LS10     1000000 // in usec
/* ------------------------ Function Prototypes ---------------------------- */
static OsTmrCallbackFunc (_thermServiceAlertTimerCallback_LS10)
    GCC_ATTRIB_USED();
sysSYSLIB_CODE static void _thermSendSlowdownMessage(LwBool slm);

/*!
 * @brief Force/Revert Thermal slowdown.
 *
 * 1. Thermal slowdown is forced by by overriding BJT0 temperature to 0.1C above the Warn threshold temperature.
 * 2. This triggers a TSENSE thermal alert interrupt.
 * 3. The Thermal alert handler will set links to L1 and force a slowdown.
 * 4. The slowdown is reverted by disabling the BJT0 temeperature override.
 * 5. The thermal alert callback function will revert the links from L1.
 *
 */
sysSYSLIB_CODE FLCN_STATUS
thermForceSlowdown_LS10
(
    LwBool bSlm,
    LwU32  periodUs
)
{
    LwU32 warn, overrideTemp;

    if (bSlm)
    {
        minL1Time = periodUs;

        // Override BJT 0 temperature to 0.1C above warn threshold
        warn = DRF_VAL(_THERM_TSENSE, _THRESHOLD_TEMPERATURES, _WARNING_TEMPERATURE, 
                       REG_RD32(BAR0, LW_THERM_TSENSE_THRESHOLD_TEMPERATURES));
        overrideTemp = warn + 0x10;
        REG_WR32(BAR0, LW_THERM_TSENSE_U2_A_0_BJT_0_TEMPERATURE_MODIFICATIONS,
            DRF_NUM(_THERM_TSENSE, _U2_A_0_BJT_0_TEMPERATURE_MODIFICATIONS, _TEMPERATURE_OVERRIDE, overrideTemp) |
            DRF_NUM(_THERM_TSENSE, _U2_A_0_BJT_0_TEMPERATURE_MODIFICATIONS, _TEMPERATURE_OVERRIDE_EN, 0x1));
    }
    else
    {
        minL1Time = THERM_MINIMUM_L1_TIME_LS10;
        // Disable thermal override for BJT 0
        REG_WR32(BAR0, LW_THERM_TSENSE_U2_A_0_BJT_0_TEMPERATURE_MODIFICATIONS,
            DRF_NUM(_THERM_TSENSE, _U2_A_0_BJT_0_TEMPERATURE_MODIFICATIONS, _TEMPERATURE_OVERRIDE, 0x0) |
            DRF_NUM(_THERM_TSENSE, _U2_A_0_BJT_0_TEMPERATURE_MODIFICATIONS, _TEMPERATURE_OVERRIDE_EN, 0x0));
    }


    return FLCN_OK;    
}

/*!
 * @brief Check if Thermal Event is Active
 *
 * Compares maximum temp against threshold temp
 * 
 */
sysSYSLIB_CODE LwBool
thermCheckThermalEventStatus_LS10(void)
{
    LwU32 data32;
    LwTemp warn, max;

    data32 = REG_RD32(BAR0, LW_THERM_TSENSE_THRESHOLD_TEMPERATURES);
    data32 = DRF_VAL(_THERM_TSENSE, _THRESHOLD_TEMPERATURES, _WARNING_TEMPERATURE, data32);
    warn = LW_TSENSE_FXP_9_5_TO_24_8(data32);

    data32 = REG_RD32(BAR0, LW_THERM_TSENSE_MAXIMUM_TEMPERATURE);
    data32 = DRF_VAL(_THERM_TSENSE, _MAXIMUM_TEMPERATURE, _MAXIMUM_TEMPERATURE, data32);
    max = LW_TSENSE_FXP_9_5_TO_24_8(data32); 

    return (max > warn) ? LW_TRUE : LW_FALSE;
}


/*!
 * @brief Handle Thermal ALERT Interrupt.
 *
 * This interrupt will:
 *  1. Assert Thermal Mode L1.
 *  2. Post a message to the driver indicating an ALERT.
 *  3. Kick off a 1 second timer in continous mode using callback function.
 *  4. Exit the handler.
 *
 */
sysSYSLIB_CODE void
thermServiceAlert_LS10(void)
{
    LwU32 intervalUs = minL1Time;
    LwBool l1Assert = LW_TRUE;

    // Set links to L1
    // ToDo- send SECCMD to set link state to L1

    // Send slowdown message to the driver
    _thermSendSlowdownMessage(l1Assert);

    // Initialize thermal task callback
    memset(&thermTaskOsTmrCb, 0, sizeof(OS_TMR_CALLBACK));

    osTmrCallbackCreate(&thermTaskOsTmrCb,                              // pCallback
                OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED, // type
                0,                                                      // ovlImem
                _thermServiceAlertTimerCallback_LS10,                   // pTmrCallbackFunc
                Disp2QThermThd,                                         // queueHandle
                intervalUs,                                             // periodNormalus
                intervalUs,                                             // periodSleepus
                OS_TIMER_RELAXED_MODE_USE_NORMAL,                       // bRelaxedUseSleep
                RM_SOE_UNIT_THERM);                                     // taskId   
 
    // Schedule thermal callback
    osTmrCallbackSchedule(&thermTaskOsTmrCb);
}

/*!
 * @brief Handle Thermal ALERT Interrupt Timer.
 *
 * Check thermal alert status
 *   1. If the Alert is still in progress
 *      a. If links are not in L1
 *         i. Assert L1
 *   2. If the Alert is cleared
 *      a. Stop the timer
 *      b. Unassert L1
 *      c. Post a message to driver indicating ALERT is gone
 *      d. Clear the WARN interrupt
 *      e. Exit
 */
static FLCN_STATUS
_thermServiceAlertTimerCallback_LS10
(
    OS_TMR_CALLBACK *pCallback
)
{
    LwBool thermStatus;

    thermStatus = thermCheckThermalEventStatus_HAL();
    if (!thermStatus)
    {
        // DeSchedule callback for SOE reset polling
        osTmrCallbackCancel(&thermTaskOsTmrCb);

        // Get links out of L1
        // ToDo- Send SECCMD to get links out of L1

        // Send slowdown message to the driver
        _thermSendSlowdownMessage(thermStatus);

        // Clear warn interrupt
        intrClearThermalAlert_HAL();
    }
    else
    {
        // Todo- send L1 SECCMD again
    }

    return FLCN_OK;
}

/*!
 * @brief Returns MAX_OFFSET temperature of all the internal sensors 
 *        The return value equals max temperature plus the sensor offset.
 *
 */
sysSYSLIB_CODE LwTemp
thermGetTsenseMax_LS10(void)
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
sysSYSLIB_CODE FLCN_STATUS
thermGetTsenseThresholds_LS10
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
sysSYSLIB_CODE static void
_thermSendSlowdownMessage
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

    maxTemperature = REG_RD32(BAR0, LW_THERM_TSENSE_MAXIMUM_TEMPERATURE);
    maxTemperature = DRF_VAL(_THERM_TSENSE, _MAXIMUM_TEMPERATURE, 
                             _MAXIMUM_TEMPERATURE, maxTemperature);
    pMsg->status.slowdown.maxTemperature = LW_TSENSE_FXP_9_5_TO_24_8(maxTemperature);

    warningThreshold = REG_RD32(BAR0, LW_THERM_TSENSE_THRESHOLD_TEMPERATURES);
    warningThreshold = DRF_VAL(_THERM_TSENSE, _THRESHOLD_TEMPERATURES,
                               _WARNING_TEMPERATURE, warningThreshold);
    pMsg->status.slowdown.warnThreshold = LW_TSENSE_FXP_9_5_TO_24_8(warningThreshold);

    if (slm)
    {
        source = REG_RD32(SAW, LW_LWLSAW_LWSTHERMALEVENT);
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
sysSYSLIB_CODE static void
_thermSendShutdownMessage(void)
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

    maxTemperature = REG_RD32(BAR0, LW_THERM_TSENSE_MAXIMUM_TEMPERATURE);
    maxTemperature = DRF_VAL(_THERM_TSENSE, _MAXIMUM_TEMPERATURE,
                             _MAXIMUM_TEMPERATURE, maxTemperature);
    pMsg->status.shutdown.maxTemperature = LW_TSENSE_FXP_9_5_TO_24_8(maxTemperature);

    overtThreshold = REG_RD32(BAR0, LW_THERM_TSENSE_THRESHOLD_TEMPERATURES);
    overtThreshold = DRF_VAL(_THERM_TSENSE, _THRESHOLD_TEMPERATURES,
                             _OVERTEMP_TEMPERATURE, overtThreshold);
    pMsg->status.shutdown.overtThreshold = LW_TSENSE_FXP_9_5_TO_24_8(overtThreshold);

    source = REG_RD32(SAW, LW_LWLSAW_LWSTHERMALEVENT);
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
sysSYSLIB_CODE void
thermServiceOvert_LS10(void)
{
    // Send shutdown message to the driver
    _thermSendShutdownMessage();

    // Clear overt interrupt
    intrClearThermalOvert_HAL();
}

/*!
 * Setup required thermal HW state during SOE Pre-Init.
 */
sysSYSLIB_CODE FLCN_STATUS
thermPreInit_LS10(void)
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

    minL1Time = THERM_MINIMUM_L1_TIME_LS10;

    return FLCN_OK;
}
