/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2022 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/* ------------------------ System Includes -------------------------------- */

#include "soesw.h"
#include "soe_bar0.h"
#include "soe_objlwlink.h"
#include "lwlinkip_discovery.h"
#include "soe_objdiscovery.h"
#include "soe_objsoe.h"
#include "dev_lwlipt_lnk_ip.h"
#include "dev_soe_ip_addendum.h"
#include "dev_minion_ip.h"
#include "dev_minion_ip_addendum.h"
#include "dev_lwlipt_ip.h"


/* ------------------------ Application Includes --------------------------- */
#include "config/g_lwlink_hal.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

// Timeout for polling L1 seccmd ready if
#define SOE_L1_SECCMD_READY_POLL_TIMEOUT_NS_LS10 (1000*1000)  //1ms

// ToDo- Update discovery- Tracking Bug - 3520872
#define MINION_BASE_ADDRESS(i)         (0x1004000 + 0x100000 * i)

OBJLWLINK Lwlink;

#if (SOECFG_FEATURE_ENABLED(SOE_MINION_SECCMD))
/*! 
 * @brief: Send SECCMD to minion
 * 
 * @param[in] timeout Timeout for polling on SECCMD ready bit set by minion
 */
static LwBool
_lwlinkPollSeccmdReady
(
    LwU32 minionBaseAddr,
    LwU32 timeout
)
{
    FLCN_TIMESTAMP timeStart;
    LwU32 regVal = 0;

    osPTimerTimeNsLwrrentGet(&timeStart);
    do
    {
        regVal = ISR_REG_RD32(minionBaseAddr + LW_MINION_SECCMD);
        if (FLD_TEST_DRF(_MINION, _SECCMD, _READY, _TRUE, regVal))
        {
            break;
        }
    } while (osPTimerTimeNsElapsedGet(&timeStart) < timeout);

    if (!FLD_TEST_DRF(_MINION, _SECCMD, _READY, _TRUE, regVal))
    {
        return LW_FALSE;
    }

    return LW_TRUE; 
}

/*! 
 * @brief: Send SECCMD to minion
 */
FLCN_STATUS
lwlinkSendMinionSeccmd_LS10
(
    LwU32 seccmdType,
    LwU32 minionInstance,
    SECCMD_ARGS *args
)
{
    LwU32 regVal;
    LwU32 minionBaseAddr;

    // ToDo- Update discovery- Tracking Bug - 3520872
    minionBaseAddr = MINION_BASE_ADDRESS(minionInstance);

    // Poll for SECCMD READY to be set by MINION
    if (!_lwlinkPollSeccmdReady(minionBaseAddr, SOE_L1_SECCMD_READY_POLL_TIMEOUT_NS_LS10))
    {
        return FLCN_ERR_TIMEOUT;
    }

    // Check for SECCMD FAULT
    regVal = ISR_REG_RD32(minionBaseAddr + LW_MINION_SECCMD);
    if (FLD_TEST_DRF(_MINION, _SECCMD, _FAULT, _FAULT_CLEAR, regVal))
    {
        goto SeccmdFault;            
    }   

    OS_ASSERT(seccmdType == LW_MINION_SECCMD_COMMAND_NOP             ||
              seccmdType == LW_MINION_SECCMD_COMMAND_THERMAL_EVENT   ||
              seccmdType == LW_MINION_SECCMD_COMMAND_ALWAYSFAULT);
    regVal = FLD_SET_DRF_NUM(_MINION, _SECCMD, _COMMAND, seccmdType, regVal);

    switch (seccmdType)
    {
        case LW_MINION_SECCMD_COMMAND_THERMAL_EVENT:
        {
            regVal = FLD_SET_DRF_NUM(_MINION, _SECCMD, _ARGS_TYPE, args->seccmdThermalArgs.argsType, regVal);
            regVal = FLD_SET_DRF_NUM(_MINION, _SECCMD, _ARGS_LEVEL, args->seccmdThermalArgs.argsLevel, regVal);
            break;
        }

        case LW_MINION_SECCMD_COMMAND_NOP:
        {
            regVal = FLD_SET_DRF(_MINION, _SECCMD, _ARGS, _INIT, regVal);
            break;  
        }

        case LW_MINION_SECCMD_COMMAND_ALWAYSFAULT:
        {
            regVal = FLD_SET_DRF(_MINION, _SECCMD, _ARGS, _INIT, regVal);
            break;  
        }

        default:
        {
            SOE_HALT();
            break;
        }                     
    }

    // Send SECCMD to MINION
    ISR_REG_WR32((minionBaseAddr + LW_MINION_SECCMD), regVal);

    // Poll for SECCMD READY to be set by MINION
    if (_lwlinkPollSeccmdReady(minionBaseAddr, SOE_L1_SECCMD_READY_POLL_TIMEOUT_NS_LS10))
    {
        return FLCN_OK;
    }
    else
    {
        // Check for SECCMD FAULT
        regVal = ISR_REG_RD32(minionBaseAddr + LW_MINION_SECCMD);
        if (FLD_TEST_DRF(_MINION, _SECCMD, _FAULT, _FAULT_CLEAR, regVal))
        {
            goto SeccmdFault;            
        }

        return FLCN_ERR_TIMEOUT;                       
    }

SeccmdFault:

    // Issue NOP SECCMD to clear the fault, Bug 3316854
    regVal = ISR_REG_RD32(minionBaseAddr + LW_MINION_SECCMD);
    regVal = FLD_SET_DRF(_MINION, _SECCMD, _ARGS, _INIT, regVal);
    ISR_REG_WR32((minionBaseAddr + LW_MINION_SECCMD), regVal);
    // ToDo - send error message to driver - Tracking Bug 3520876

    return FLCN_ERROR;  
}

/*! 
 * @brief: Get SECCMD Thermal Status
 * 
 * @param[in] minionInstance        Minion instance to send SECCMD
 * @param[in] SECCMD_THERMAL_STAT   Current Thermal Mode and Level
 */
FLCN_STATUS
lwlinkGetSeccmdThermalStatus_LS10
(
    LwU32 minionInstance,
    SECCMD_THERMAL_STAT *args
)
{
    FLCN_STATUS status;
    SECCMD_ARGS seccmdArgs;
    LwU32 regVal, minionBaseAddr;

    seccmdArgs.seccmdThermalArgs.argsType = LW_MINION_SECCMD_ARGS_TYPE_STATUS;
    status = lwlinkSendMinionSeccmd_HAL(&Lwlink, LW_MINION_SECCMD_COMMAND_THERMAL_EVENT, minionInstance, &seccmdArgs); 

    if (status != FLCN_OK)
    {
        return FLCN_ERROR;
    }

    // Read SECCMD DATA register and return status in arg
    // ToDo- Update discovery- Tracking Bug - 3520872
    minionBaseAddr = MINION_BASE_ADDRESS(minionInstance); 
    regVal = ISR_REG_RD32(minionBaseAddr + LW_MINION_SECCMD_DATA);
    args->thermalMode = DRF_VAL(_MINION, _SECCMD, _DATA_LWRRENT_THERMAL_MODE, regVal);
    args->thermalLevel = DRF_VAL(_MINION, _SECCMD, _DATA_LWRRENT_THERMAL_MODE_LEVEL, regVal);
    
    return FLCN_OK;
}
#endif

/*!
 * Pre-STATE_INIT for LWLINK
 */
void
lwlinkPreInit_LS10(void)
{
    Lwlink.linksEnableMask = 0xFFFFFFFFFFFFFFFFULL;
}
