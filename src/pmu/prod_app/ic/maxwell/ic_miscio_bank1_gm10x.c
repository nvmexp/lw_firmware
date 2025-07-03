/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   ic_miscio_bank1_gm10x.c
 * @brief  Contains all Interrupt Control routines for MISCIO bank 1 specific to
 *         GM10X.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_falcon_csb.h"
#include "dev_pwr_falcon_csb_addendum.h"


/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "dbgprintf.h"
#include "pmu_objic.h"
#include "pmu_objdi.h"
#include "config/g_ic_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * This is the interrupt handler for MISCIO (GPIO) bank 1 interrupts.  Both
 * rising- and falling- edge interrupts are detected and handled here.
 */
void
icServiceMiscIOBank1_GM10X(void)
{
    LwU32  intrRising;
    LwU32  intrFalling;
    LwU32  riseEn;
    LwU32  fallEn;
    LwU32  tempRaising = 0;
    LwU32  tempFalling = 0;

    intrRising   = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING );
    intrFalling  = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_FALLING);
    riseEn       = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN);
    fallEn       = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_FALLING_EN);

    // be sure to mask off the disabled interrupts
    intrRising   = intrRising  & riseEn;
    intrFalling  = intrFalling & fallEn;

    DBG_PRINTF_ISR(("IO(1) int r=0x%08x,f=0x%08x,in=0x%08x\n",
                    intrRising,
                    intrFalling,
                    REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INPUT),
                    0));

    if (PENDING_IO_BANK1(_XP2PMU_STATE_IS_L1_1 , _RISING, intrRising))
    {
        tempRaising  = FLD_SET_DRF(_CPWR_PMU, _GPIO_1_INTR_RISING, _XP2PMU_STATE_IS_L1_1, _PENDING, tempRaising);
        icServiceMiscIODeepL1_HAL(&Ic, &riseEn, LW_TRUE, DI_PEX_SLEEP_STATE_L1_1);
    }
    if (PENDING_IO_BANK1(_XP2PMU_STATE_IS_L1_1, _FALLING, intrFalling))
    { 
        tempFalling  = FLD_SET_DRF(_CPWR_PMU, _GPIO_1_INTR_FALLING, _XP2PMU_STATE_IS_L1_1, _PENDING, tempFalling);
        icServiceMiscIODeepL1_HAL(&Ic, &fallEn, LW_FALSE, DI_PEX_SLEEP_STATE_L1_1);
    }
    if (PENDING_IO_BANK1(_XP2PMU_STATE_IS_L1_2 , _RISING, intrRising))
    {
        tempRaising  = FLD_SET_DRF(_CPWR_PMU, _GPIO_1_INTR_RISING, _XP2PMU_STATE_IS_L1_2, _PENDING, tempRaising);
        icServiceMiscIODeepL1_HAL(&Ic, &riseEn, LW_TRUE, DI_PEX_SLEEP_STATE_L1_2);
    }
    if (PENDING_IO_BANK1(_XP2PMU_STATE_IS_L1_2, _FALLING, intrFalling))
    {
        tempFalling  = FLD_SET_DRF(_CPWR_PMU, _GPIO_1_INTR_FALLING, _XP2PMU_STATE_IS_L1_2, _PENDING, tempFalling);
        icServiceMiscIODeepL1_HAL(&Ic, &fallEn, LW_FALSE, DI_PEX_SLEEP_STATE_L1_2);
    }

    REG_WR32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING, tempRaising);
    REG_WR32(CSB, LW_CPWR_PMU_GPIO_1_INTR_FALLING, tempFalling);

    REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, riseEn);
    REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_1_INTR_FALLING_EN, fallEn);

    icServiceMiscIOBank1_GPXXX();
 }
