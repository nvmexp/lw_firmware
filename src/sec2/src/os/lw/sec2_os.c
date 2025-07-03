/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  sec2_os.c
 * @brief Top-Level interfaces between the SEC2 application layer and the RTOS
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
#include "sec2sw.h"
#include "sec2_os.h"
#include "sec2_hs.h"
#include "sec2_objsec2.h"
#include "sec2_objlsf.h"
#include "lwostask.h"
#include "lwosselwreovly.h"
#include "lwrtosHooks.h"
/* ------------------------- Application Includes -------------------------- */
#include "scp_rand.h"
/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Global Variables ------------------------------ */
#ifdef DMEM_VA_SUPPORTED
extern LwU32    _dmem_va_bound[];
extern LwU32    _dmem_ovl_start_address[];
extern LwU16    _dmem_ovl_size_lwrrent[];
extern LwU16    _dmem_ovl_size_max[];
extern LwU32    lwr_dmem_va_location[];
#endif
extern LwU32    _num_dmem_blocks[];
/* ------------------------- Static Variables ------------------------------ */
/* ------------------------- Defines --------------------------------------- */
/* ------------------------ Static Function Prototypes  --------------------- */
/*!
 * @brief   Interface providing per-build stack size of idle task.
 *
 * @return  Idle task's stack size.
 */
LwU16
vApplicationIdleTaskStackSize(void)
{
    return OSTASK_ATTRIBUTE_IDLE_STACK_SIZE / 4;
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
    falc_ssetb_i(0); // set CSW F0 flags
    falc_wait(0);    // suspend exelwtion until next interrupt
}

void
lwrtosHookError
(
    void *pxTask,
    LwS32 xErrorCode
)
{
    // VR/NJ-TODO: Update RM to read the Mailbox register for the error code.
    SEC2_HALT();
}

/*!
 * Callback function called by the RTOS to check for DMA NACK
 * Clears the NACK bit if set.
 *
 * @returns    LwBool    LW_TRUE if a DMA NACK was received
 */
LwBool
vApplicationDmaNackCheckAndClear(void)
{
    return sec2DmaNackCheckAndClear_HAL();
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
    if (SEC2CFG_FEATURE_ENABLED(DYNAMIC_FLCN_PRIV_LEVEL))
    {
        sec2FlcnPrivLevelSet_HAL(&Sec2, privLevelExt, privLevelCsb);
    }
}

/*!
 * Callback function called by the RTOS to reset falcon privilege level
 */
void
vApplicationFlcnPrivLevelReset(void)
{
    if (SEC2CFG_FEATURE_ENABLED(DYNAMIC_FLCN_PRIV_LEVEL))
    {
        sec2FlcnPrivLevelSet_HAL(&Sec2,
                                 Sec2.flcnPrivLevelExtDefault,
                                 Sec2.flcnPrivLevelCsbDefault);
    }
}

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

/*!
 * @brief  Check if SCTL_LSMODE or SCTL_HSMODE is set
 *
 * @param[in]  pBLsModeSet   Pointer saving if SCTL_LSMODE is set
 * @param[in]  pBHsModeSet   Pointer saving if SCTL_HSMODE is set
 */
void
vApplicationIsLsOrHsModeSet_hs
(
    LwBool *pBLsModeSet,
    LwBool *pBHsModeSet
)
{
    _sec2IsLsOrHsModeSet_GP10X(pBLsModeSet, pBHsModeSet);
}
