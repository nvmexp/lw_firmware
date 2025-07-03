/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  dpu_os.c
 * @brief Top-Level interfaces between the DPU application layer and the RTOS
 *
 * This module is intended to serve as the top-level interface layer for dealing
 * with all application-specific aspects of the OS. This would include handling
 * exceptions that the OS could not handle or even a custom idle-hook could be
 * installed. In it's current state, it is simply provides a hook for unhandled
 * exceptions to save some non-PRIV accessible state before halting.
 *
 * Note: All data-types in this module should be consistent with those used
 *       in the OS.
 */

/* ------------------------- System Includes ------------------------------- */
#include "lwrtosHooks.h"

/* ------------------------- Application Includes -------------------------- */
#include "lwdpu.h"
#include "dpusw.h"
#include "dpu_os.h"
#include "dpu_objdpu.h"
#include "dpu_objlsf.h"
#include "dpu_gpuarch.h"
#if GSPLITE_RTOS
#include "gsp_hs.h"
#endif

#include "config/g_ic_private.h"

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Static Variables ------------------------------ */
/* ------------------------- Defines --------------------------------------- */
/*!
 * @brief   Interface providing per-build stack size of idle task.
 *
 * @return  Idle task's stack size.
 */
LwU16
vApplicationIdleTaskStackSize(void)
{
    return OSTASK_ATTRIBUTE_IDLE_STACK_SIZE / sizeof(LwUPtr);
}

/*!
 * @brief Called by the RTOS repeatedly from within the IDLE task.
 *
 * Code added to this hook is guaranteed to run continuously (at low-priorty)
 * during periods of idleness. The following constraints must be meet when
 * adding to this function:
 * 1. Do NOT call any functions that could cause the IDLE task to block.
 * 2. Do NOT code with infinite-loops. Though this is lwrrently permissible,
 *    it could become an issue if task-deletion ever becomes necessary.
 */
void
lwrtosHookIdle(void)
{
#if UPROC_FALCON
    falc_ssetb_i(0); // set CSW F0 flags
    falc_wait(0);    // suspend exelwtion until next interrupt
#else
    __asm__ __volatile__ ("wfi");
#endif
}

void
lwrtosHookError
(
    void *pxTask,
    LwS32 xErrorCode
)
{
    // VR/NJ-TODO: Update RM to read the Mailbox register for the error code.
    DPU_HALT();
}

/*!
 * Callback function called by the RTOS to set falcon privilege level
 *
 * @param[in]  privLevelExt  falcon EXT privilege level
 * @param[in]  privLevelCsb  falcon CSB privilege level
 */
void
vApplicationFlcnPrivLevelSet
(
   LwU8 privLevelExt,
   LwU8 privLevelCsb
)
{
    if (DPUCFG_FEATURE_ENABLED(DYNAMIC_FLCN_PRIV_LEVEL))
    {
        dpuFlcnPrivLevelSet_HAL(&Dpu.hal, privLevelExt, privLevelCsb);
    }
}

/*!
 * Callback function called by the RTOS to reset falcon privilege level
 */
void
vApplicationFlcnPrivLevelReset(void)
{
    if (DPUCFG_FEATURE_ENABLED(DYNAMIC_FLCN_PRIV_LEVEL))
    {
        extern LwU8 dpuFlcnPrivLevelExtDefault;
        extern LwU8 dpuFlcnPrivLevelCsbDefault;

        dpuFlcnPrivLevelSet_HAL(&Dpu.hal,
                                dpuFlcnPrivLevelExtDefault,
                                dpuFlcnPrivLevelCsbDefault);
    }
}

#ifdef FLCNDBG_ENABLED
/*!
 * Callback function called by the RTOS when a breakpoint is hit in the ucode
 * and a software interrupt is generated. Lwrrently used by falcon debugger.
 */
void
vApplicationIntOnBreak(void)
{
    LwU32 csw = 0;

#if (DPU_PROFILE_v0201)
    if (!DISP_IP_VER_GREATER_OR_EQUAL(DISP_VERSION_V02_04))
    {
        return;
    }
#endif

    // Exception handler, clear CSW.E;
    falc_rspr(csw, CSW);
    falc_wspr(CSW, csw & 0xff7ffff);

    icServiceSwIntr_HAL();
}
#endif // FLCNDBG_ENABLED

/*!
 * Callback function called by the RTOS to check whether an access using the
 * specified dmaIdx is permitted.
 *
 * @param[in]  dmaIdx    DMA index to check
 *
 * @return     bubble up the return value from @ref lsfIsDmaIdxPermitted_HAL
 */
LwBool
vApplicationIsDmaIdxPermitted
(
    LwU8 dmaIdx
)
{
    return lsfIsDmaIdxPermitted_HAL(&LsfHal, dmaIdx);
}

#if HS_OVERLAYS_ENABLED
/*!
 * Interface function to be called by library/os to enter HS mode.
 *
 * @return  error code returned by _enterHS
 */
FLCN_STATUS
vApplicationEnterHS(void)
{
    return _enterHS();
}

/*!
 * Interface function to be called by library/os to exit HS mode.
 */
void
vApplicationExitHS(void)
{
    _exitHS();
}
#endif

#if (DPUCFG_FEATURE_ENABLED(DPUJOB_RTTIMER_TEST))
/*!
 * Interface function to be called by library/os to setup rttimer.
 */
FLCN_STATUS
vApplicationRttimerSetup
(
    LwBool bStartTimer,
    LwU32  intervalUs,
    LwBool bIsModeOneShot
)
{
    return icSetupRttimerIntr_HAL(&IcHal, bStartTimer, intervalUs, LW_TRUE, bIsModeOneShot);
}
#endif
