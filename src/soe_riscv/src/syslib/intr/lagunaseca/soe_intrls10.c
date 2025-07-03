/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
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
#include "soe_objintr.h"
#include "soe_objtherm.h"
#include "soe_objsaw.h"
/* ------------------------ Application Includes --------------------------- */
#include "dev_lwlsaw_ip.h"
#include "dev_ctrl_ip.h"
#include "dev_therm.h"
#include "dev_ctrl_ip_addendum.h"

/*!
 * Pre-STATE_INIT for GIN
 */
sysSYSLIB_CODE void
intrPreInit_LS10(void)
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
    regVal = (DRF_NUM(_CTRL, _SOE_INTR_UNITS, _SEC0_INTR0_0,        1) |
              DRF_NUM(_CTRL, _SOE_INTR_UNITS, _SEC0_NOTIFY_0,       1) |
              DRF_NUM(_CTRL, _SOE_INTR_UNITS, _THERMAL_OVERTEMP,    1) |
              DRF_NUM(_CTRL, _SOE_INTR_UNITS, _THERMAL_ALERT,       1) |
              DRF_NUM(_CTRL, _SOE_INTR_UNITS, _SMBPBI,              1) |
              DRF_NUM(_CTRL, _SOE_INTR_UNITS, _PMGR_SOE,            1) |
              DRF_NUM(_CTRL, _SOE_INTR_UNITS, _PTIMER,              1));
    REG_WR32(GIN, LW_CTRL_SOE_INTR_UNITS_EN_SET, regVal);
}

/*!
 * @brief   Service GIN interrupts
 */
sysSYSLIB_CODE void
intrService_LS10(void)
{
    LwU32 pending, topEnable, irqs;

    // Check GIN TOP
    topEnable = GIN_REG_RD32(LW_CTRL_SOE_INTR_TOP_EN_SET0);
    pending = GIN_REG_RD32(LW_CTRL_SOE_INTR_TOP(0));
    pending = pending & topEnable;

    if (FLD_TEST_DRF_NUM(_CTRL, _SOE_INTR_TOP_LEAF, _INTR_UNITS, 0x1, pending))
    {
        irqs = GIN_REG_RD32(LW_CTRL_SOE_INTR_UNITS);

        if (FLD_TEST_DRF_NUM(_CTRL, _SOE_INTR_UNITS, _SMBPBI, 0x1, irqs))
        {
//            smbpbiService_HAL();
        }

        if (FLD_TEST_DRF_NUM(_CTRL, _SOE_INTR_UNITS, _PMGR_SOE, 0x1, irqs))
        {
//            pmgrService_HAL();
        }

        if (FLD_TEST_DRF_NUM(_CTRL, _SOE_INTR_UNITS, _THERMAL_OVERTEMP, 0x1, irqs))
        {
            thermServiceOvert_HAL();
        }

        if (FLD_TEST_DRF_NUM(_CTRL, _SOE_INTR_UNITS, _THERMAL_ALERT, 0x1, irqs))
        {
            thermServiceAlert_HAL();
        }

        // Clear GIN LEAF 9
        GIN_REG_WR32(LW_CTRL_SOE_INTR_UNITS, irqs);

    }
}

/*!
 * @brief   Check if lwlipt is in reset.
 */
sysSYSLIB_CODE LwBool
intrIsLwlwInReset_LS10
(
    LwU32 lwliptIdx
)
{
    return LW_FALSE;
}

/*!
 * @brief  Clear thermal alert interrupt
 */
sysSYSLIB_CODE void
intrClearThermalAlert_LS10(void)
{
    LwU32 tsenseStatus, regVal;

    tsenseStatus = REG_RD32(BAR0, LW_THERM_TSENSE_THRESHOLD_TEMPERATURES_INTERRUPT_STATUS);
    tsenseStatus = FLD_SET_DRF_NUM(_THERM, _TSENSE_THRESHOLD_TEMPERATURES_INTERRUPT_STATUS,  _WARNING, 0x1, tsenseStatus);
    REG_WR32(BAR0, LW_THERM_TSENSE_THRESHOLD_TEMPERATURES_INTERRUPT_STATUS, tsenseStatus);

    regVal = REG_RD32(SAW, LW_LWLSAW_LWSTHERMALEVENT_STICKY);
    regVal = FLD_SET_DRF_NUM(_LWLSAW, _LWSTHERMALEVENT, _STICKY_THERM_ALERT_STATUS, 0x1, regVal);
    regVal = FLD_SET_DRF_NUM(_LWLSAW, _LWSTHERMALEVENT, _STICKY_THROTTLE_STATUS, 0x1, regVal);
    regVal = FLD_SET_DRF_NUM(_LWLSAW, _LWSTHERMALEVENT, _STICKY_PMGR_THERM_ALERT_STATUS, 0x1, regVal);
    regVal = FLD_SET_DRF_NUM(_LWLSAW, _LWSTHERMALEVENT, _STICKY_THROTTLE_STATUS, 0x1, regVal);
    regVal = FLD_SET_DRF_NUM(_LWLSAW, _LWSTHERMALEVENT, _STICKY_TSENSE_THERM_ALERT_STATUS, 0x1, regVal);
    REG_WR32(SAW, LW_LWLSAW_LWSTHERMALEVENT_STICKY, regVal);
}

/*!
 * @brief  Clear thermal overt interrupt
 */
sysSYSLIB_CODE void
intrClearThermalOvert_LS10(void)
{
    LwU32 regval;

    // Clear status in tsense block
    regval = REG_RD32(BAR0, LW_THERM_TSENSE_THRESHOLD_TEMPERATURES_INTERRUPT_STATUS);
    REG_WR32(BAR0, LW_THERM_TSENSE_THRESHOLD_TEMPERATURES_INTERRUPT_STATUS, regval);

    // Clear sticky interrupt at SAW
    regval = REG_RD32(SAW, LW_LWLSAW_LWSTHERMALEVENT_STICKY);
    REG_WR32(SAW, LW_LWLSAW_LWSTHERMALEVENT_STICKY, regval);
}
