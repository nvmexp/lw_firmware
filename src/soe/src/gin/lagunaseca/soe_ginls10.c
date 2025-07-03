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
#include "soe_bar0.h"
#include "soe_objdiscovery.h"
#include "soe_objgin.h"
#include "soe_objlwlink.h"

/* ------------------------ Application Includes --------------------------- */
#include "config/g_lwlink_hal.h"
#include "config/g_gin_hal.h"
#include "config/g_pmgr_hal.h"
#include "config/g_smbpbi_hal.h"
#include "config/g_therm_private.h"
#include "config/g_bif_hal.h"

#include "dev_lwlsaw_ip.h"
#include "dev_ctrl_ip.h"
#include "dev_soe_ip.h"
#include "dev_soe_csb.h"
#include "dev_therm.h"
#include "dev_ctrl_ip_addendum.h"
#include "dev_soe_ip_addendum.h"
#include "dev_minion_ip_addendum.h"
#include "dev_lwlipt_ip.h"
#include "dev_minion_ip.h"

#define ISR_REG_RD32(addr)        _soeBar0RegRd32_LR10(addr) 
#define ISR_REG_WR32(addr, data)  _soeBar0RegWr32NonPosted_LR10(addr, data)

/*!
 * @brief: Pre-STATE_INIT for GIN
 */
void
ginPreInit_LS10(void)
{
    LwU32 regVal;
    ginBase = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_GIN, INSTANCE0, ADDRESS_UNICAST, 0x0);

    // Clear GIN TOP 0 and Leaf 9 registers
    REG_WR32(GIN, LW_CTRL_SOE_INTR_TOP(0), 0xFFFF);
    REG_WR32(GIN, LW_CTRL_SOE_INTR_UNITS, 0xFF);

    // Enable leaf 9 in GIN TOP0 enable register
    regVal = DRF_NUM(_CTRL, _SOE_INTR_TOP_EN_SET0, _LEAF(LW_CTRL_SOE_INTR_UNITS_IDX), 0x1);
    REG_WR32(GIN, LW_CTRL_SOE_INTR_TOP_EN_SET0, regVal);

    // Leaf 9 enables
    regVal = (DRF_NUM(_CTRL, _SOE_INTR_UNITS, _THERMAL_OVERTEMP,    1) |
              DRF_NUM(_CTRL, _SOE_INTR_UNITS, _THERMAL_ALERT,       1) |
              DRF_NUM(_CTRL, _SOE_INTR_UNITS, _SMBPBI,              1) |
              DRF_NUM(_CTRL, _SOE_INTR_UNITS, _PMGR_SOE,            1) |
              DRF_NUM(_CTRL, _SOE_INTR_UNITS, _PTIMER,              1) |
              DRF_NUM(_CTRL, _SOE_INTR_UNITS, _XTL_SOE,             1));
    REG_WR32(GIN, LW_CTRL_SOE_INTR_UNITS_EN_SET, regVal);
}

/*!
 * @brief   Service GIN interrupts
 */
void
ginService_LS10(void)
{
    LwU32 pending,topEnable,irqs;

    // Check GIN TOP
    topEnable = GIN_ISR_REG_RD32(LW_CTRL_SOE_INTR_TOP_EN_SET0);
    pending = GIN_ISR_REG_RD32(LW_CTRL_SOE_INTR_TOP(0));
    pending = pending & topEnable;

    // Service Leaf 9 Interrupts
    if (FLD_TEST_DRF_NUM(_CTRL, _SOE_INTR_TOP_LEAF, _INTR_UNITS, 0x1, pending))
    {
        irqs = GIN_ISR_REG_RD32(LW_CTRL_SOE_INTR_UNITS);

        if (FLD_TEST_DRF_NUM(_CTRL, _SOE_INTR_UNITS, _SMBPBI, 0x1, irqs))
        {
            smbpbiService_HAL();
        }

        if (FLD_TEST_DRF_NUM(_CTRL, _SOE_INTR_UNITS, _PMGR_SOE, 0x1, irqs))
        {
            pmgrService_HAL();
        }

        if (FLD_TEST_DRF_NUM(_CTRL, _SOE_INTR_UNITS, _THERMAL_OVERTEMP, 0x1, irqs))
        {
            thermServiceOvert_HAL();
        }

        if (FLD_TEST_DRF_NUM(_CTRL, _SOE_INTR_UNITS, _THERMAL_ALERT, 0x1, irqs))
        {
            thermServiceAlert_HAL();
        }

        if (FLD_TEST_DRF_NUM(_CTRL, _SOE_INTR_UNITS, _XTL_SOE, 0x1, irqs))
        {
            bifServiceLaneMarginingInterrupts_HAL();
        }

        // Clear GIN LEAF 9
        GIN_ISR_REG_WR32(LW_CTRL_SOE_INTR_UNITS, irqs);
    }
}

/*!
 * @brief   Check if lwlipt is in reset.
 */
LwBool
ginIsLwlwInReset_LS10
(
    LwU32 lwliptIdx
)
{
    return LW_FALSE;
}

/*!
 * @brief  Clear thermal alert interrupt in GIN block
 */
void
ginClearThermalAlertIntr_LS10(void)
{
    LwU32 tsenseStatus, regVal, sawBase;

    sawBase = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_SAW, INSTANCE0, ADDRESS_UNICAST, 0);

    tsenseStatus = ISR_REG_RD32(LW_THERM_TSENSE_THRESHOLD_TEMPERATURES_INTERRUPT_STATUS);
    tsenseStatus = FLD_SET_DRF_NUM(_THERM, _TSENSE_THRESHOLD_TEMPERATURES_INTERRUPT_STATUS, _WARNING, 0x1, tsenseStatus);
    ISR_REG_WR32(LW_THERM_TSENSE_THRESHOLD_TEMPERATURES_INTERRUPT_STATUS, tsenseStatus);

    regVal = ISR_REG_RD32(sawBase + LW_LWLSAW_LWSTHERMALEVENT_STICKY);
    regVal = FLD_SET_DRF_NUM(_LWLSAW, _LWSTHERMALEVENT, _STICKY_THERM_ALERT_STATUS, 0x1, regVal);
    regVal = FLD_SET_DRF_NUM(_LWLSAW, _LWSTHERMALEVENT, _STICKY_THROTTLE_STATUS, 0x1, regVal);
    regVal = FLD_SET_DRF_NUM(_LWLSAW, _LWSTHERMALEVENT, _STICKY_PMGR_THERM_ALERT_STATUS, 0x1, regVal);
    regVal = FLD_SET_DRF_NUM(_LWLSAW, _LWSTHERMALEVENT, _STICKY_THROTTLE_STATUS, 0x1, regVal);
    regVal = FLD_SET_DRF_NUM(_LWLSAW, _LWSTHERMALEVENT, _STICKY_TSENSE_THERM_ALERT_STATUS, 0x1, regVal);
    ISR_REG_WR32(sawBase + LW_LWLSAW_LWSTHERMALEVENT_STICKY, regVal);
}
