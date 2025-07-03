/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/* ------------------------ System Includes -------------------------------- */
#include "soesw.h"
#include "soe_bar0.h"
#include "soe_objsaw.h"

/* ------------------------ Application Includes --------------------------- */
#include "config/g_saw_hal.h"
#include "config/g_pmgr_hal.h"
#include "config/g_smbpbi_hal.h"
#include "config/g_therm_private.h"

#include "dev_lwlsaw_ip.h"

/*!
 * Pre-STATE_INIT for SAW
 */
void
sawPreInit_LR10(void)
{
    LwU32  regVal;

    // We will setup the interrupts from scratch, CLR all interrupt masks now.
    REG_WR32(SAW, LW_LWLSAW_LWSPMC_INTR_SOE_EN_CLR_LEGACY,      0xFFFFFFFF);
    REG_WR32(SAW, LW_LWLSAW_LWSPMC_INTR_SOE_EN_CLR_FATAL,       0xFFFFFFFF);
    REG_WR32(SAW, LW_LWLSAW_LWSPMC_INTR_SOE_EN_CLR_NONFATAL,    0xFFFFFFFF);
    REG_WR32(SAW, LW_LWLSAW_LWSPMC_INTR_SOE_EN_CLR_CORRECTABLE, 0xFFFFFFFF);


    // Setup SAW LEGACY interrupt steering and then enable interrupts
    regVal = (DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_LEGACY, _PTIMER_0,     _HOST) |  // TODO: Revisit this setting
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_LEGACY, _PTIMER_1,     _HOST) |  // TODO: Revisit this setting
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_LEGACY, _PMGR_0,       _HOST) |  // HOST services PMGR 0 (RM)
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_LEGACY, _PMGR_1,       _SOE)  |  // SOE services PMGR 1 (PMU)
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_LEGACY, _SMBUS_MSGBOX, _SOE));   // SOE services SMBPBI
    REG_WR32(SAW, LW_LWLSAW_LWSPMC_STEER_INTR_LEGACY, regVal);

    regVal = DRF_NUM(_LWLSAW_LWSPMC, _INTR_SOE_EN_SET_LEGACY, _SMBUS_MSGBOX, 1);
    REG_WR32(SAW, LW_LWLSAW_LWSPMC_INTR_SOE_EN_SET_LEGACY,  regVal);


    // Setup SAW CORRECTABLE steering and then enable interrupts
    regVal = (DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _SAW_WRITE_LOCKED, _HOST) |
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _SOE_SHIM_FLUSH,   _HOST) | // TODO: Revisit this setting
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _SOE_SHIM_ILLEGAL, _HOST) | // TODO: Revisit this setting
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _OVER_TEMP_ALERT,  _SOE)  |
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _OVER_TEMP,        _SOE)  |
              // LWLIPTs
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _LWLIPT_0,         _HOST) |
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _LWLIPT_1,         _HOST) |
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _LWLIPT_2,         _HOST) |
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _LWLIPT_3,         _HOST) |
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _LWLIPT_4,         _HOST) |
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _LWLIPT_5,         _HOST) |
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _LWLIPT_6,         _HOST) |
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _LWLIPT_7,         _HOST) |
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _LWLIPT_8,         _HOST) |
              // NPGs
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _NPG_0,            _HOST) |
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _NPG_1,            _HOST) |
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _NPG_2,            _HOST) |
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _NPG_3,            _HOST) |
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _NPG_4,            _HOST) |
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _NPG_5,            _HOST) |
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _NPG_6,            _HOST) |
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _NPG_7,            _HOST) |
              DRF_DEF(_LWLSAW_LWSPMC, _STEER_INTR_CORRECTABLE, _NPG_8,            _HOST));
    REG_WR32(SAW, LW_LWLSAW_LWSPMC_STEER_INTR_CORRECTABLE, regVal);

    regVal = (DRF_NUM(_LWLSAW_LWSPMC, _INTR_SOE_EN_SET_CORRECTABLE, _SAW_WRITE_LOCKED, 1) |
              DRF_NUM(_LWLSAW_LWSPMC, _INTR_SOE_EN_SET_CORRECTABLE, _SOE_SHIM_FLUSH,   1) |
              DRF_NUM(_LWLSAW_LWSPMC, _INTR_SOE_EN_SET_CORRECTABLE, _SOE_SHIM_ILLEGAL, 1) |
              DRF_NUM(_LWLSAW_LWSPMC, _INTR_SOE_EN_SET_CORRECTABLE, _OVER_TEMP_ALERT,  1) |
              DRF_NUM(_LWLSAW_LWSPMC, _INTR_SOE_EN_SET_CORRECTABLE, _OVER_TEMP,        1));
    REG_WR32(SAW, LW_LWLSAW_LWSPMC_INTR_SOE_EN_SET_CORRECTABLE, regVal);
}

/*!
 * @brief   Service SAW interrupts
 */
void
sawService_LR10(void)
{
    LwU32 irqs;
    LwU32 masked;

    //
    // ===== Start with LEGACY interrupts =====
    //
    irqs = SAW_ISR_REG_RD32(LW_LWLSAW_LWSPMC_INTR_SOE_LEGACY) &
           SAW_ISR_REG_RD32(LW_LWLSAW_LWSPMC_INTR_SOE_EN_LEGACY);

    // SMBUS_MSGBOX
    if (FLD_TEST_DRF_NUM(_LWLSAW_LWSPMC_INTR_SOE, _LEGACY, _SMBUS_MSGBOX, 1, irqs))
    {
        smbpbiService_HAL();
    }

    // PMGR_1 (PMGR_0 is routed to HOST)
    if (FLD_TEST_DRF_NUM(_LWLSAW_LWSPMC_INTR_SOE, _LEGACY, _PMGR_1, 1, irqs))
    {
        pmgrService_HAL();
    }


    //
    // ===== Next are CORRECTABLE interrupts =====
    //
    masked = 0;
    irqs = SAW_ISR_REG_RD32(LW_LWLSAW_LWSPMC_INTR_SOE_CORRECTABLE) &
           SAW_ISR_REG_RD32(LW_LWLSAW_LWSPMC_INTR_SOE_EN_CORRECTABLE);

    // OVER_TEMP
    if (FLD_TEST_DRF_NUM(_LWLSAW_LWSPMC_INTR_SOE, _CORRECTABLE, _OVER_TEMP, 1, irqs))
    {
        thermServiceOvert_HAL();
    }

    // OVER_TEMP_ALERT
    if (FLD_TEST_DRF_NUM(_LWLSAW_LWSPMC_INTR_SOE, _CORRECTABLE, _OVER_TEMP_ALERT, 1, irqs))
    {
        thermServiceAlert_HAL();
    }

    // SAW_WRITE_LOCKED
    if (FLD_TEST_DRF_NUM(_LWLSAW_LWSPMC_INTR_SOE, _CORRECTABLE, _SAW_WRITE_LOCKED, 1, irqs))
    {
        //
        // sawServiceWriteLocked_HAL();
        // The function above could handle this, but mask here for now
        // and remove when implemented.
        //
        masked = FLD_SET_DRF_NUM(_LWLSAW_LWSPMC_INTR_SOE_EN_CLR, _CORRECTABLE, _SAW_WRITE_LOCKED, 1, masked);
    }

    // SOE_SHIM_FLUSH
    if (FLD_TEST_DRF_NUM(_LWLSAW_LWSPMC_INTR_SOE, _CORRECTABLE, _SOE_SHIM_FLUSH, 1, irqs))
    {
        //
        // TODO: soeServiceSoeShimFlush_HAL();
        // The function above could handle this, but mask here for now
        // and remove when implemented.
        //
        masked = FLD_SET_DRF_NUM(_LWLSAW_LWSPMC_INTR_SOE_EN_CLR, _CORRECTABLE, _SOE_SHIM_FLUSH, 1, masked);
    }

    // SOE_SHIM_ILLEGAL
    if (FLD_TEST_DRF_NUM(_LWLSAW_LWSPMC_INTR_SOE, _CORRECTABLE, _SOE_SHIM_ILLEGAL, 1, irqs))
    {
        //
        // TODO: soeServiceSoeShimIllegal_HAL();
        // The function above could handle this, but mask here for now
        // and remove when implemented.
        //
        masked = FLD_SET_DRF_NUM(_LWLSAW_LWSPMC_INTR_SOE_EN_CLR, _CORRECTABLE, _SOE_SHIM_ILLEGAL, 1, masked);
    }

    // Clear the enables (mask) for the requested LWSPMC_INTR_SOE_EN_CORRECTABLE irqs
    SAW_ISR_REG_WR32(LW_LWLSAW_LWSPMC_INTR_SOE_EN_CLR_CORRECTABLE, masked);

    // The LWSPMC_INTR_SOE_* status register is read only, there is nothing to clear.
}

/*!
 * @brief   Check if lwlipt is in reset.
 */
LwBool
sawIsLwlwInReset_LR10
(
    LwU32 lwliptIdx
)
{
    LwU32 regVal;

    regVal = REG_RD32(SAW, LW_LWLSAW_LWSPMC_ENABLE_LWLIPT);
    return !(regVal & LWBIT(lwliptIdx));
}

/*!
 * @brief  Enable thermal alert interrupt in SAW block
 */
void
sawEnableThermalAlertIntr_LR10(void)
{
    LwU32 regVal;

    regVal = DRF_DEF(_LWLSAW_LWSPMC, _INTR_SOE_EN_SET_CORRECTABLE,
                     _OVER_TEMP_ALERT, _ENABLE);
    SAW_ISR_REG_WR32(LW_LWLSAW_LWSPMC_INTR_SOE_EN_SET_CORRECTABLE, regVal);
}

/*!
 * @brief   Disable thermal alert interrupt in SAW block
 */
void
sawDisableThermalAlertIntr_LR10(void)
{
    LwU32 regVal;

    regVal = DRF_DEF(_LWLSAW_LWSPMC, _INTR_SOE_EN_CLR_CORRECTABLE,
                     _OVER_TEMP_ALERT, _ENABLE);
    SAW_ISR_REG_WR32(LW_LWLSAW_LWSPMC_INTR_SOE_EN_CLR_CORRECTABLE, regVal);
}

/*!
 * @brief  Disable thermal overt interrupt in SAW block
 */
void
sawDisableThermalOvertIntr_LR10(void)
{
    LwU32 regVal;

    regVal = DRF_DEF(_LWLSAW_LWSPMC, _INTR_SOE_EN_CLR_CORRECTABLE,
                     _OVER_TEMP, _ENABLE);
    SAW_ISR_REG_WR32(LW_LWLSAW_LWSPMC_INTR_SOE_EN_CLR_CORRECTABLE, regVal);
}

/*!
 * @brief  Clear thermal alert interrupt in SAW block
 */
void
sawClearThermalAlertIntr_LR10(void)
{
    LwU32 regVal;

    regVal = SAW_ISR_REG_RD32(LW_LWLSAW_LWSTHERMALEVENT);
    regVal = FLD_SET_DRF(_LWLSAW, _LWSTHERMALEVENT, _THERM_ALERT_STICKY_STATUS, _CLEAR, regVal);
    regVal = FLD_SET_DRF(_LWLSAW, _LWSTHERMALEVENT, _THROTTLE_STICKY_STATUS, _CLEAR, regVal);
    regVal = FLD_SET_DRF(_LWLSAW, _LWSTHERMALEVENT, _PMGR_THERM_ALERT_STICKY_STATUS, _CLEAR, regVal);
    regVal = FLD_SET_DRF(_LWLSAW, _LWSTHERMALEVENT, _THROTTLE_STICKY_STATUS, _CLEAR, regVal);
    regVal = FLD_SET_DRF(_LWLSAW, _LWSTHERMALEVENT, _TSENSE_THERM_ALERT_STICKY_STATUS, _CLEAR, regVal);

    SAW_ISR_REG_WR32(LW_LWLSAW_LWSTHERMALEVENT, regVal);
}
