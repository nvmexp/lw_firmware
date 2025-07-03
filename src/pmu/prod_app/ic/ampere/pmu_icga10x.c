/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_icga10x.c
 * @brief  Contains handler-routines for dealing with Gr Wakeup Interupt based on GPIO
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

#include "dev_fuse.h"
#include "pmu_objfuse.h"

#ifdef UPROC_RISCV
#include "drivers/drivers.h"
#endif // UPROC_RISCV

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objic.h"
#include "pmu_objpg.h"

#include "config/g_ic_private.h"

#include "config/g_perf_cf_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief   Initializes the top-level interrupt sources
 *
 * @details The PMU's interrupt sources are organized into a tree. This function
 *          configures and enables the interrupts at the top-level, as required.
 */
void
icPreInitTopLevel_GA10X(void)
{
    if (PMUCFG_FEATURE_ENABLED(ARCH_FALCON))
    {
        icPreInitTopLevel_GMXXX();
        return;
    }

    LwU32 irqDest = 0U;
    LwU32 irqMset = 0U;
    LwU32 irqMode = REG_RD32(CSB, LW_CPWR_FALCON_IRQMODE);

    //
    // Configure and enable the desired top-level PMU interrupt sources and
    // disable all other interrupt sources.
    //

    // HALT: CPU transitioned from running into halt
    irqDest = FLD_SET_DRF(_CPWR, _RISCV_IRQDEST, _HALT, _HOST, irqDest);
    irqMset = FLD_SET_DRF(_CPWR, _RISCV_IRQMSET, _HALT, _SET, irqMset);

    // MEMERR: external mem or register access error interrupt (level-triggered)
    irqDest = FLD_SET_DRF(_CPWR, _RISCV_IRQDEST, _MEMERR, _RISCV, irqDest);
    irqMset = FLD_SET_DRF(_CPWR, _RISCV_IRQMSET, _MEMERR, _SET, irqMset);
    irqMode = FLD_SET_DRF(_CPWR, _FALCON_IRQMODE, _LVL_MEMERR, _TRUE, irqMode);

    // IOPMP: error on memory access from certain IOPMP primary (level-triggered)
    irqDest = FLD_SET_DRF(_CPWR, _RISCV_IRQDEST, _IOPMP, _RISCV, irqDest);
    irqMset = FLD_SET_DRF(_CPWR, _RISCV_IRQMSET, _IOPMP, _SET, irqMset);
    irqMode = FLD_SET_DRF(_CPWR, _FALCON_IRQMODE, _LVL_IOPMP, _TRUE, irqMode);

    // SWGEN0: for communication with RM via Message Queue
    irqDest = FLD_SET_DRF(_CPWR, _RISCV_IRQDEST, _SWGEN0, _HOST, irqDest);
    irqMset = FLD_SET_DRF(_CPWR, _RISCV_IRQMSET, _SWGEN0, _SET, irqMset);

    // SWGEN1: for the PMU's print buffer
    irqDest = FLD_SET_DRF(_CPWR, _RISCV_IRQDEST, _SWGEN1, _HOST, irqDest);
    irqMset = FLD_SET_DRF(_CPWR, _RISCV_IRQMSET, _SWGEN1, _SET, irqMset);

    // ICD: for debug interrupts
    irqDest = FLD_SET_DRF(_CPWR, _RISCV_IRQDEST, _ICD, _HOST, irqDest);
    irqMset = FLD_SET_DRF(_CPWR, _RISCV_IRQMSET, _ICD, _SET, irqMset);

    // SECONDARY: for ELPG interrupts
    irqDest = FLD_SET_DRF(_CPWR, _RISCV_IRQDEST, _EXT_SECOND, _RISCV, irqDest);
    irqMset = FLD_SET_DRF(_CPWR, _RISCV_IRQMSET, _EXT_SECOND, _SET, irqMset);

    // MISCIO: for GPIO interrupts
    if (PMUCFG_FEATURE_ENABLED(PMU_IC_MISCIO))
    {
        irqDest = FLD_SET_DRF(_CPWR, _RISCV_IRQDEST, _EXT_MISCIO, _RISCV, irqDest);
        irqMset = FLD_SET_DRF(_CPWR, _RISCV_IRQMSET, _EXT_MISCIO, _SET, irqMset);
    }

    // WDTMR: for Watchdog interrupts for Watchdog task
    if (PMUCFG_FEATURE_ENABLED(PMUTASK_WATCHDOG))
    {
        irqDest = FLD_SET_DRF(_CPWR, _RISCV_IRQDEST, _WDTMR, _RISCV, irqDest);
        irqMset = FLD_SET_DRF(_CPWR, _RISCV_IRQMSET, _WDTMR, _SET, irqMset);
    }

    // THERM: for MSGBOX interrupts
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_INTR))
    {
        irqDest = FLD_SET_DRF(_CPWR, _RISCV_IRQDEST, _EXT_THERM, _RISCV, irqDest);
        irqMset = FLD_SET_DRF(_CPWR, _RISCV_IRQMSET, _EXT_THERM, _SET, irqMset);
    }

    REG_WR32(CSB, LW_CPWR_FALCON_IRQMODE, irqMode);
    REG_WR32(CSB, LW_CPWR_RISCV_IRQDEST, irqDest);
    REG_WR32(CSB, LW_CPWR_RISCV_IRQMSET, irqMset);
    REG_WR32(CSB, LW_CPWR_RISCV_IRQMCLR, ~irqMset);

    //
    // Enable ECC interrupts if needed. Must go after above configuration (at
    // least IRQDEST) because the above does direct writes and not RMWs
    //
    icEccIntrEnable_HAL(&Ic);
}

/*!
 * @copydoc icService_GMXXX
 */
LwBool
icService_GA10X(LwU32 pendingIrqMask)
{
#ifdef UPROC_RISCV
    // MEMERR Interrupt
    if (DRF_VAL(_CPWR, _FALCON_IRQSTAT, _MEMERR, pendingIrqMask))
    {
        // Use MEMERR handler from lwriscv.
        irqHandleMemerr();
    }

    // IOPMP Interrupt
    if (DRF_VAL(_CPWR, _FALCON_IRQSTAT, _IOPMP, pendingIrqMask))
    {
        // Use IOPMP-err handler from lwriscv.
        irqHandleIopmpErr();
    }
#endif // UPROC_RISCV

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MEM_TUNE))
    {
        perfCfMemTuneWatchdogTimerValidate_HAL();
    }

    // We do not need to clear MEMERR since it's level-triggered
    return icService_GMXXX(pendingIrqMask);
}

/*!
 * @brief Enables the Halt Interrupt
 *
 * It needs to be enabled to HALT early in case of failure in PMU init.
 * Should not be called from task context.
 */
void
icHaltIntrEnable_GA10X(void)
{
    if (PMUCFG_FEATURE_ENABLED(ARCH_FALCON))
    {
        icHaltIntrEnable_GMXXX();
        return;
    }

    // Clear the HALT Interrupt status before unmasking
    REG_WR32_STALL(CSB, LW_CPWR_FALCON_IRQSCLR, DRF_DEF(_CPWR, _FALCON_IRQSCLR, _HALT, _SET));

    // Enable interrupt by writing mask
    REG_WR32_STALL(CSB, LW_CPWR_RISCV_IRQMSET, DRF_DEF(_CPWR, _RISCV_IRQMSET, _HALT, _SET));

    // Ensure HALT interrupt goes to host
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CPWR, _RISCV_IRQDEST, _HALT, _HOST);
}

/*!
 * @brief Disables the Halt interrupt
 *
 * It needs to be disabled before we HALT (shutdown) on detach
 */
void
icHaltIntrDisable_GA10X(void)
{
    if (PMUCFG_FEATURE_ENABLED(ARCH_FALCON))
    {
        icHaltIntrDisable_GMXXX();
        return;
    }

    REG_WR32_STALL(CSB, LW_CPWR_RISCV_IRQMCLR, DRF_DEF(_CPWR, _RISCV_IRQMCLR, _HALT, _SET));
}

/*!
 * @brief Enables the SWGEN1 print buffer interrupt
 *
 * It needs to be enabled to print early in case of failure in PMU init.
 * Should not be called from task context.
 */
void
icRiscvSwgen1IntrEnable_GA10X(void)
{
    if (PMUCFG_FEATURE_ENABLED(ARCH_FALCON))
    {
        // No-op on Falcon if not stubbed
        return;
    }

    //
    // Do not clear the SWGEN1 interrupt status
    // in case there is one in flight already.
    //

    // Enable interrupt by writing mask
    REG_WR32_STALL(CSB, LW_CPWR_RISCV_IRQMSET, DRF_DEF(_CPWR, _RISCV_IRQMSET, _SWGEN1, _SET));

    // Ensure SWGEN1 interrupt goes to host
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CPWR, _RISCV_IRQDEST, _SWGEN1, _HOST);
}

/*!
 * @brief Enables the ICD interrupt
 *
 * It needs to be enabled to have resumable breakpoints early in PMU init.
 * Should not be called from task context.
 */
void
icRiscvIcdIntrEnable_GA10X(void)
{
    if (PMUCFG_FEATURE_ENABLED(ARCH_FALCON))
    {
        // No-op on Falcon if not stubbed
        return;
    }

    //
    // Do not clear the ICD interrupt status
    // in case there is one in flight already.
    //

    // Enable interrupt by writing mask
    REG_WR32_STALL(CSB, LW_CPWR_RISCV_IRQMSET, DRF_DEF(_CPWR, _RISCV_IRQMSET, _ICD, _SET));

    // Ensure the ICD interrupt goes to host
    REG_FLD_WR_DRF_DEF_STALL(CSB, _CPWR, _RISCV_IRQDEST, _ICD, _HOST);
}

/*!
 * @brief Disables the ICD interrupt for RISCV
 *
 * It needs to be disabled before we HALT, because we go into ICD then,
 * but we only want to send the HALT interrupt.
 */
void
icRiscvIcdIntrDisable_GA10X(void)
{
    if (PMUCFG_FEATURE_ENABLED(ARCH_FALCON))
    {
        // No-op on Falcon if not stubbed
        return;
    }

    REG_WR32_STALL(CSB, LW_CPWR_RISCV_IRQMCLR, DRF_DEF(_CPWR, _RISCV_IRQMCLR, _ICD, _SET));
}

/*!
 * @brief   Disables all of the PMU's top-level interrupt sources
 */
void
icDisableAllInterrupts_GA10X(void)
{
    if (PMUCFG_FEATURE_ENABLED(ARCH_FALCON))
    {
        icDisableAllInterrupts_GMXXX();
        return;
    }

    REG_WR32_STALL(CSB, LW_CPWR_RISCV_IRQMCLR, LW_U32_MAX);
}

/*!
 * @brief   Disables interrupts for the detached state
 */
void
icDisableDetachInterrupts_GA10X(void)
{
    if (PMUCFG_FEATURE_ENABLED(ARCH_FALCON))
    {
        icDisableDetachInterrupts_GMXXX();
        return;
    }

    // Disable the top-level Falcon interrupt sources except GPTMR and SWGEN0.
    REG_WR32(CSB, LW_CPWR_RISCV_IRQMCLR,
        FLD_SET_DRF(_CPWR, _RISCV_IRQMCLR, _CTXSW, _SET,
        FLD_SET_DRF(_CPWR, _RISCV_IRQMCLR, _HALT, _SET,
        DRF_SHIFTMASK(LW_CPWR_RISCV_IRQMCLR_EXT))));
}

/*!
 * @brief   Get the mask of interrupts pending to the PMU itself
 *
 * @return  The mask of pending interrupts
 */
LwU32
icGetPendingUprocInterrupts_GA10X(void)
{
    if (PMUCFG_FEATURE_ENABLED(ARCH_FALCON))
    {
        return icGetPendingUprocInterrupts_GMXXX();
    }

    LwU32 dest = REG_RD32(CSB, LW_CPWR_RISCV_IRQDEST);

    // Only handle interrupts that are routed to the PMU itself
    return REG_RD32(CSB, LW_CPWR_FALCON_IRQSTAT) & ~(dest);
}

/*!
 * Enable ECC interrupts for the PMU.
 */
void
icEccIntrEnable_GA10X(void)
{
    if (PMUCFG_FEATURE_ENABLED(ARCH_FALCON))
    {
        icEccIntrEnable_TUXXX();
        return;
    }

    // Only actually enable the interrupt if ECC is enabled
    if (!icEccIsEnabled_HAL(&Ic))
    {
        return;
    }

#if defined(LW_CPWR_RISCV_IRQDEST_EXT_ECC) && defined(LW_CPWR_RISCV_IRQTYPE_EXT_ECC)

    // Read the existing IRQDEST/IRQTYPE values to avoid changing previous configuration.
    LwU32 irqDest = REG_RD32(CSB, LW_CPWR_RISCV_IRQDEST);
    LwU32 irqType = REG_RD32(CSB, LW_CPWR_RISCV_IRQTYPE);

    // Route PMU ECC interrupt to RM as a stalling interrupt.
    irqDest = FLD_SET_DRF(_CPWR, _RISCV_IRQDEST, _EXT_ECC, _HOST, irqDest);
    REG_WR32(CSB, LW_CPWR_RISCV_IRQDEST, irqDest);

    irqType = FLD_SET_DRF(_CPWR, _RISCV_IRQTYPE, _EXT_ECC, _HOST_NORMAL, irqType);
    REG_WR32(CSB, LW_CPWR_RISCV_IRQTYPE, irqType);

    // Writing '0' to IRQMSET has no effect so, only setting the ECC bit is enough.
    REG_WR32(CSB, LW_CPWR_RISCV_IRQMSET, DRF_DEF(_CPWR_RISCV, _IRQMSET, _EXT_ECC, _SET));
#endif
}
