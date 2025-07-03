/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_ictuxxx.c
 * @brief  Contains all Interrupt Control routines specific to TUxxx.
 *
 * Implementation Notes:
 *    # Within ISRs only use macro DBG_PRINTF_ISR (never DBG_PRINTF).
 *
 *    # Avoid access to any BAR0/PRIV registers while in the ISR. This is to
 *      avoid inlwrring the penalty of accessing the PRIV bus due to its
 *      relatively high-latency.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dbgprintf.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_falcon_csb.h"
#include "dev_pwr_falcon_csb_addendum.h"
#include "dev_pwr_pri.h"
#include "dev_graphics_nobundle.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objic.h"
#include "pmu_objpg.h"

#include "config/g_ic_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*!
 * Check if ECC/parity for PMU IMEM/DMEM is enabled in HW.
 */
LwBool
icEccIsEnabled_TUXXX(void)
{
#if defined(LW_PGRAPH_PRI_FECS_FEATURE_READOUT_ECC_PMU_FALCON)
    LwU32 val = REG_RD32(FECS, LW_PGRAPH_PRI_FECS_FEATURE_READOUT);

    return FLD_TEST_DRF(_PGRAPH_PRI, _FECS_FEATURE_READOUT, _ECC_PMU_FALCON, _ENABLED, val);
#else
    return LW_FALSE;
#endif
}

/*!
 * Enable ECC interrupts for the PMU.
 */
void
icEccIntrEnable_TUXXX(void)
{
    // Only actually enable the interrupt if ECC is enabled
    if (!icEccIsEnabled_HAL(&Ic))
    {
        return;
    }

#if defined(LW_CPWR_FALCON_IRQDEST_HOST_EXT_ECC) && defined(LW_CPWR_FALCON_IRQDEST_TARGET_EXT_ECC)

    // Read the existing IRQDEST value to avoid changing previous configuration.
    LwU32 val = REG_RD32(CSB, LW_CPWR_FALCON_IRQDEST);

    // Route PMU ECC interrupt to RM as a stalling interrupt.
    val = FLD_SET_DRF(_CPWR_FALCON, _IRQDEST_HOST, _EXT_ECC, _HOST, val);
    val = FLD_SET_DRF(_CPWR_FALCON, _IRQDEST_TARGET, _EXT_ECC, _HOST_NORMAL, val);

    REG_WR32(CSB, LW_CPWR_FALCON_IRQDEST, val);

    // Writing '0' to IRQMSET has no effect so, only setting the ECC bit is enough.
    REG_WR32(CSB, LW_CPWR_FALCON_IRQMSET, DRF_DEF(_CPWR_FALCON, _IRQMSET, _EXT_ECC, _SET));
#endif
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Responsible for all initialization or setup pertaining to PMU interrupts.
 * This includes interrupt routing as well as setting the default enable-state
 * for all PMU interrupt-sources (command queues, gptmr, gpios, etc ...).
 *
 * IRQ0/IRQ1 are the two possible interrupt inputs of the falcon. While setting
 * up an interupt it should be mentioned in IRQDEST_TARGET whether it should be
 * targeted to interrupt the falcon on IRQ0 or IRQ1. The general convention
 * followed on most of the falcons is that only the interrupt used for scheduling
 * goes to IRQ1, everything else goes to IRQ0. We have set up our interrupt handler
 * in such a way that IV1 (vector for IRQ1) can only handle one interrupt which would
 * require to run the scheduler and IV0 calls the global interrupt handler function
 * (icService_xxx). The RTOS allows the clients (PMU, DPU, SEC2, etc.) to install
 * their own IV0 handlers but it never exposes the installation of the IV1 handler
 * since it is implemented by the OS itself.
 *
 * Notes:
 *     - The interrupt-enables for both falcon interrupt vectors should be OFF/
 *       disabled prior to calling this function.
 *     - This function does not modify the interrupt-enables for either vector.
 *     - This function MUST be called PRIOR to starting the scheduler.
 */
void
icPreInit_TUXXX(void)
{
    // Legacy PMU interrupts init/setup
    icPreInit_GMXXX();
}

/*!
 * High-level handler routine for dealing with all first-level PMU interrupts
 * (includes command management, ELPG, interrupt retargetting, etc...). For
 * This function will call the appropriate handler-function for each first-level
 * interrupt-source that is lwrrently pending.
 */
void
icServiceIntr1MainIrq_TUXXX(void)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_BIF_XUSB))
    {
        // Service all XUSB interrupts
        icServiceXusbIntr_HAL(&Ic);
    }

    // Handle legacy interrupts
    icServiceIntr1MainIrq_GMXXX();
}

