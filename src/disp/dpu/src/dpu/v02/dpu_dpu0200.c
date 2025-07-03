/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_dpu0200.c
 * @brief  DPU 02.00 Hal Functions
 *
 * DPU HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "dpusw.h"
#include "dev_disp.h"
#include "dispflcn_regmacros.h"

#include "lwostimer.h"
/* ------------------------- Application Includes --------------------------- */
#include "dpu_objdpu.h"
#include "dpu_gpuarch.h"
#include "config/g_dpu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */

/*!
 * @brief Early initialization for DPU 02.00
 *
 * Any general initialization code not specific to particular engines should be
 * done here. This function must be called prior to starting the scheduler.
 */
FLCN_STATUS
dpuInit_v02_00(void)
{
    //
    // The CSB base address may vary in different falcons, so each falcon ucode
    // needs to set this address based on its real condition so that the RTOS can
    // do the CSB access properly.
    //
    csbBaseAddress = DFLCN_DRF_BASE();

    return FLCN_OK;
}

/*!
 * @brief Get the IRQSTAT mask for the OS timer
 */
LwU32
dpuGetOsTickIntrMask_v02_00(void)
{
    return DFLCN_DRF_SHIFTMASK(IRQSTAT_GPTMR);
}

/*!
 * @brief Set up the General-purpose timer (used by the scheduler)
 * and optionally start it
 *
 * @param[in] freqHz  DPU frequency in Hz
 * @param[in] bEnable Enable the GPTMR after setting it
 */
void
dpuGptmrSet_v02_00
(
    LwU32  freqHz,
    LwBool bEnable
)
{
    static LwU32 prevFreqHz = 0;

    if (freqHz == 0)
    {
        return;
    }

    // Disable interrupts while we change the GPTMR
    lwrtosENTER_CRITICAL();
    {
        // Disable the timer before changing it
        dpuGptmrDisable_HAL();

        if (freqHz != prevFreqHz)
        {
            DFLCN_REG_WR_NUM(GPTMRINT, _VAL,
                       (freqHz / LWRTOS_TICK_RATE_HZ - 1));

            //
            // The cycle at frequency transition is not correct and meaningless
            // even transform to new value based on new frequency.
            // Let GPTMRVAL decreases to zero then reload the GPTMRINT on the next
            // cycle.
            //
            if (prevFreqHz == 0)
            {
                // First setup, program correct value.
                DFLCN_REG_WR_NUM(GPTMRVAL, _VAL,
                           (freqHz / LWRTOS_TICK_RATE_HZ - 1));
            }
        }

        prevFreqHz = freqHz;
        DpuInitArgs.cpuFreqHz = freqHz;

        if (bEnable)
        {
            dpuGptmrEnable_HAL();
        }
    }
    lwrtosEXIT_CRITICAL();
}

/*!
 * @brief Enable the General-purpose timer
 */
void
dpuGptmrEnable_v02_00(void)
{
    DFLCN_REG_WR32(GPTMRCTL,
        DFLCN_DRF_SHIFTMASK(GPTMRCTL_GPTMREN));
}

/*!
 * @brief Disable the General-purpose timer
 */
void
dpuGptmrDisable_v02_00(void)
{
    LwU32 ctrl = DFLCN_REG_RD32(GPTMRCTL);

    DFLCN_REG_WR32(GPTMRCTL,
        ctrl & ~DFLCN_DRF_SHIFTMASK(GPTMRCTL_GPTMREN));
}

