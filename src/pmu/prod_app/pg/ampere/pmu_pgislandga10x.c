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
 * @file   pmu_pgislandga10x.c
 * @brief  HAL-interface for the GA10X PGISLAND
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_gc6_island.h"
#include "dev_fuse.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "lwuproc.h"
#include "regmacros.h"
#include "flcntypes.h"
#include "lwos_dma.h"
#include "pmu_didle.h"
#include "pmu_objpg.h"
#include "pmu_objpmu.h"
#include "acr/pmu_objacr.h"
#include "pmu_objfuse.h"
#include "dev_pwr_falcon_csb.h"
#include "dev_pwr_pri.h"
#include "dev_pmgr.h"
#include "dev_therm.h"
#include "dev_trim.h"
#include "cmdmgmt/cmdmgmt.h"

#include "config/g_pg_private.h"
#include "config/g_acr_private.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * @brief HAL-interface for synchronizing SCI and PMGR GPIO
 *
 * This function parses the GPIO pin mask set by RM and syncs only
 * those GPIOs represented by the "set" bit in the mask
 */
void
pgIslandSciPmgrGpioSync_GA10X
(
    LwU32 gpioPinMask
)
{
    LwU32  oldSciReg = 0;
    LwU32  newSciReg = 0;
    LwU32  pmgrReg   = 0;
    LwU32  gpioPin   = 0;
    LwBool bSciIlwerted;

    //
    // Sychronize the GPIO pins denoted by the "set" bit number
    // in the GPIO pin mask sent from RM
    //
    FOR_EACH_INDEX_IN_MASK(32, gpioPin, gpioPinMask)
    {

        oldSciReg = newSciReg = REG_RD32(FECS, LW_PGC6_SCI_GPIO_OUTPUT_CNTL(gpioPin));
        pmgrReg   = REG_RD32(FECS, LW_GPIO_OUTPUT_CNTL(gpioPin));

        //
        // Make the SCI pin state match the PMGR pin state. Pin state is
        // defined as the actual voltage on the GPIO pin : 0 is 0V, and 1 is 3.3V.
        // pin_state is defined as LW_PMGR_GPIO_*_OUTPUT_CNTL_IO_INPUT. IO_INPUT
        // is an absolute value that reflects the pin state (confirmed
        // experimentally).
        //
        // When setting norm_out_data in the sci, note that Polarity = 1
        // for active high and 0 for active low.
        // pin_state = polarity ? norm_out_data : ~norm_out_data.
        // Thus, in order to set a pin state:
        // norm_out_data = polarity ? pin_state : ~pin_state
        //
        // We need to ensure the "absolute value" of PMGR output should match
        // with SCI. Thus, algorithm takes pin states from _IO_INPUT as it
        // represents absolute value of pin; then considers polarity of SCI
        // (ACTIVE_LOW/ACTIVE_HIGH) and program SCI's NORM_OUT_DATA based on
        // SCI's polarity and absolute value of PMGR Pin
        //

        bSciIlwerted = FLD_TEST_DRF( _PGC6, _SCI_GPIO_OUTPUT_CNTL, _POLARITY,
                                     _ACTIVE_LOW, oldSciReg);

        if (FLD_TEST_DRF(_GPIO, _OUTPUT_CNTL, _IO_INPUT, _0, pmgrReg))
        {
            if (bSciIlwerted)
            {
                newSciReg = FLD_SET_DRF(_PGC6, _SCI_GPIO_OUTPUT_CNTL,
                    _NORM_OUT_DATA, _ASSERT, newSciReg);
            }
            else
            {
                newSciReg = FLD_SET_DRF(_PGC6, _SCI_GPIO_OUTPUT_CNTL,
                    _NORM_OUT_DATA, _DEASSERT, newSciReg);
            }
        }
        else
        {
            if (bSciIlwerted)
            {
                newSciReg = FLD_SET_DRF(_PGC6, _SCI_GPIO_OUTPUT_CNTL,
                    _NORM_OUT_DATA, _DEASSERT, newSciReg);
            }
            else
            {
                newSciReg = FLD_SET_DRF(_PGC6, _SCI_GPIO_OUTPUT_CNTL,
                    _NORM_OUT_DATA, _ASSERT, newSciReg);
            }
        }

        // Write into SCI GPIO, only if the new value is different
        if (oldSciReg != newSciReg)
        {
            REG_WR32(FECS, LW_PGC6_SCI_GPIO_OUTPUT_CNTL(gpioPin), newSciReg);
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;
}
