/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmgrad10x.c
 * @brief  Contains all PCB Management (PGMR) routines specific to AD10X+.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "lwosreg.h"
#include "pmu_objpmgr.h"

#include "config/g_pmgr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Initialize interrupts to be serviced by PMGR
 */
void
pmgrInterruptInit_AD10X(void)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BEACON_INTERRUPT))
    {
        LwU32 reg32;

        // Route the ISENSE_BEACONX interrupts to PMU
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_INTR_ROUTE);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_ROUTE, _ISENSE_ADC_RESET_INTR, _PMU,
                reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_ROUTE, _ISENSE_BEACON1_INTR, _PMU,
                reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_ROUTE, _ISENSE_BEACON2_INTR, _PMU,
                reg32);
        REG_WR32(CSB, LW_CPWR_THERM_INTR_ROUTE, reg32);

        // Enable the ISENSE_BEACONX interrupts
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_INTR_EN_0);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0, _ISENSE_ADC_RESET_INTR, _ENABLED,
                reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0, _ISENSE_BEACON1_INTR, _ENABLED,
                reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0, _ISENSE_BEACON2_INTR, _ENABLED,
                reg32);
        REG_WR32(CSB, LW_CPWR_THERM_INTR_EN_0, reg32);

        //
        // Initialize the device index for beacon interrupt to invalid.
        // The power device which will service beacon interrupts will populate
        // this index to a valid value during construct.
        //
        PMGR_PWR_DEV_IDX_BEACON_INIT(&Pmgr);
    }
}

/*!
 * Specific handler-routine for dealing with beacon interrupts.
 */
void
pmgrPwrDevBeaconInterruptService_AD10X(void)
{
    DISPATCH_PMGR   disp2Pmgr = {{ 0 }};
    LwU32           reg32;

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_INTR_0);

    if ((FLD_TEST_DRF(_CPWR_THERM, _INTR_0, _ISENSE_ADC_RESET_INTR,
                     _PENDING, reg32)) ||
        (FLD_TEST_DRF(_CPWR_THERM, _INTR_0, _ISENSE_BEACON1_INTR,
                     _PENDING, reg32)) ||
        (FLD_TEST_DRF(_CPWR_THERM, _INTR_0, _ISENSE_BEACON2_INTR,
                     _PENDING, reg32)))
    {
        //
        // Disable beacon interrupts while it is being serviced
        // Event handler will ensure to enable these interrupts after servicing
        //
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_INTR_EN_0);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0, _ISENSE_ADC_RESET_INTR, _DISABLED,
                reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0, _ISENSE_BEACON1_INTR, _DISABLED,
                reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0, _ISENSE_BEACON2_INTR, _DISABLED,
                reg32);
        REG_WR32(CSB, LW_CPWR_THERM_INTR_EN_0, reg32);

        disp2Pmgr.hdr.eventType = PMGR_EVENT_ID_PWR_DEV_BEACON_INTERRUPT;
        lwrtosQueueIdSendFromISR(LWOS_QUEUE_ID(PMU, PMGR),
                &disp2Pmgr, sizeof(disp2Pmgr));
    }
}

/*!
 * Specific handler-routine for clearing and re-enabling beacon interrupts.
 */
void
pmgrPwrDevBeaconInterruptClearAndReenable_AD10X(void)
{
    LwU32 reg32;

    // Clear the pending beacon interrupts
    REG_WR32(CSB, LW_CPWR_THERM_INTR_0,
        (DRF_DEF(_CPWR_THERM, _INTR_0, _ISENSE_ADC_RESET_INTR, _CLEAR) |
         DRF_DEF(_CPWR_THERM, _INTR_0, _ISENSE_BEACON1_INTR, _CLEAR)   |
         DRF_DEF(_CPWR_THERM, _INTR_0, _ISENSE_BEACON2_INTR, _CLEAR)));

    appTaskCriticalEnter();
    {
        // Re-enable beacon interrupts
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_INTR_EN_0);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0,
                _ISENSE_ADC_RESET_INTR, _ENABLED, reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0,
                _ISENSE_BEACON1_INTR, _ENABLED, reg32);
        reg32 = FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0,
                _ISENSE_BEACON2_INTR, _ENABLED, reg32);
        REG_WR32(CSB, LW_CPWR_THERM_INTR_EN_0, reg32);
    }
    appTaskCriticalExit();
}
