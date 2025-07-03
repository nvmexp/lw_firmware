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
#include "dev_minion_ip.h"
#include "dev_soe_ip.h"
#include "dev_minion_ip_addendum.h"
#include "dev_soe_ip_addendum.h"
#include "soe_bar0.h"
#include "soe_objsoe.h"
#include "soe_objtherm.h"
#include "soe_objlwlink.h"
#include "main.h"
#include "soe_objsaw.h"
#include "soe_os.h"
#include "soe_timer.h"
#include "soe_cmdmgmt.h"
#include "soe_bar0.h"
#include "soe_objdiscovery.h"
#include "config/g_timer_hal.h"
#include "config/g_lwlink_hal.h"
#include "config/g_intr_hal.h"
#include "config/g_therm_hal.h"
#include "config/g_gin_hal.h"

/* ------------------------ Static Variables ------------------------------- */
static LwU32 minL1Time;
/* ------------------------- Macros and Defines ----------------------------- */
#define THERM_MINIMUM_L1_TIME_LS10 1000000 // in usec

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
FLCN_STATUS
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
                       ISR_REG_RD32(LW_THERM_TSENSE_THRESHOLD_TEMPERATURES));
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
LwBool
thermCheckThermalEventStatus_LS10(void)
{
    LwU32 data32;
    LwTemp warn, max;

    data32 = ISR_REG_RD32(LW_THERM_TSENSE_THRESHOLD_TEMPERATURES);
    data32 = DRF_VAL(_THERM_TSENSE, _THRESHOLD_TEMPERATURES, _WARNING_TEMPERATURE, data32);
    warn = LW_TSENSE_FXP_9_5_TO_24_8(data32);

    data32 = ISR_REG_RD32(LW_THERM_TSENSE_MAXIMUM_TEMPERATURE);
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
void
thermServiceAlert_LS10(void)
{
    LwU32 intervalUs = minL1Time;
    LwU32 minionInstance;
    LwBool l1Assert = LW_TRUE;
    FLCN_STATUS status;
    SECCMD_ARGS thermalSeccmd;
    SECCMD_THERMAL_STAT thermalStat;

    // Set links to L1
#if (SOECFG_FEATURE_ENABLED(SOE_MINION_SECCMD))
    thermalSeccmd.seccmdThermalArgs.argsType  = LW_MINION_SECCMD_ARGS_TYPE_ON;
    thermalSeccmd.seccmdThermalArgs.argsLevel = LW_MINION_SECCMD_ARGS_LEVEL_0;

    // ToDo- Use Minion BCast write and read instead - Tracking Bug - 3521003
    for (minionInstance = 0; minionInstance < NUM_MINION_ENGINE; minionInstance++)
    {
         lwlinkSendMinionSeccmd_HAL(&Lwlink, LW_MINION_SECCMD_COMMAND_THERMAL_EVENT, minionInstance, &thermalSeccmd);

        // ToDo - Update slowdown message with L1 and SECCMD Thermal Mode status - Tracking bug - 3520873
        lwlinkGetSeccmdThermalStatus_HAL(&Therm, minionInstance, &thermalStat);
    }
#endif

    // Send slowdown message to the driver
    thermSendSlowdownMessage_HAL(&Therm, l1Assert);

    // Start GP timer
    status = timerSetup_HAL(&Timer, TIMER_MODE_START, TIMER_EVENT_THERM,
                            intervalUs, LW_FALSE);
    if (status != FLCN_OK)
    {
        SOE_HALT();
    }
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
void
thermServiceAlertTimerCallback_LS10(void)
{
    LwBool thermStatus;
    SECCMD_ARGS thermalSeccmd;
    LwU32 minionInstance;

    thermStatus = thermCheckThermalEventStatus_HAL();
    if (!thermStatus)
    {
        // Stop GP timer
        timerSetup_HAL(&Timer, TIMER_MODE_STOP,
                       TIMER_EVENT_NONE, 0x0, 0x0);

        // Send SECCMD to get links out of L1
#if (SOECFG_FEATURE_ENABLED(SOE_MINION_SECCMD))
        thermalSeccmd.seccmdThermalArgs.argsType  = LW_MINION_SECCMD_ARGS_TYPE_OFF;
        for (minionInstance = 0; minionInstance < NUM_MINION_ENGINE; minionInstance++)
        {
           lwlinkSendMinionSeccmd_HAL(&Therm, LW_MINION_SECCMD_COMMAND_THERMAL_EVENT, minionInstance, &thermalSeccmd);
        }
#endif
        // ToDo - Update slowdown message with L1 and SECCMD Thermal Mode status - Tracking bug - 3520873
        // Send slowdown message to the driver
        thermSendSlowdownMessage_HAL(&Therm, thermStatus);

        // Clear warn interrupt
        intrClearThermalAlert_HAL();
    }
    else
    {
#if (SOECFG_FEATURE_ENABLED(SOE_MINION_SECCMD))
        // ToDo- Use Minion BCast write and read instead - Tracking Bug - 3521003
        for (minionInstance = 0; minionInstance < NUM_MINION_ENGINE; minionInstance++)
        {            
            thermalSeccmd.seccmdThermalArgs.argsType  = LW_MINION_SECCMD_ARGS_TYPE_ON;
            lwlinkSendMinionSeccmd_HAL(&Therm, LW_MINION_SECCMD_COMMAND_THERMAL_EVENT, minionInstance, &thermalSeccmd);
        }
#endif
    }
}

/*!
* Setup required thermal state during SOE Pre-Init.
*/
FLCN_STATUS
thermPreInit_LS10(void)
{
    minL1Time = THERM_MINIMUM_L1_TIME_LS10;
    return FLCN_OK;
}
