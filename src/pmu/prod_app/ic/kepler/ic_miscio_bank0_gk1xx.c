/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   ic_miscio_bank0_gk1xx.c
 * @brief  Contains all Interrupt Control routines for MISCIO bank 0 specific to
 *         GK1XX.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objic.h"
#include "config/g_ic_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Enable the MISCIO GPIO Bank0 interrupts
 */
void
icEnableMiscIOBank0_GMXXX(void)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_DISPLAYLESS))
    REG_WR32(CSB, LW_CPWR_PMU_GPIO_INTR_RISING_EN,
        DRF_DEF(_CPWR_PMU, _GPIO_INTR_RISING_EN, _DISP_2PMU_INTR, _ENABLED));
#endif
    REG_WR32(CSB, LW_CPWR_PMU_GPIO_INTR_FALLING_EN,
        DRF_DEF(_CPWR_PMU, _GPIO_INTR_FALLING_EN, _XVE_INTR, _ENABLED));
}
