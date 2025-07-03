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
 * @file   pmu_grgfxxx.c
 * @brief  HAL-interface for the GFXXX Graphics Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_master.h"
#include "dev_fb.h"
#include "dev_top.h"
#include "dev_graphics_nobundle.h"

#include "dev_pri_ringmaster.h"
#include "dev_pri_ringstation_fbp.h"
#include "dev_pri_ringstation_gpc.h"
#include "dev_pri_ringstation_sys.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "pmu_objpmu.h"
#include "pmu_objfifo.h"
#include "pmu_bar0.h"
#include "dbgprintf.h"
#include "config/g_gr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @breif Timeout for unbind operation
 */
#define FIFO_UNBIND_TIMEOUT_US_GMXXX        1000

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Unbind GR engine
 *
 * Issue an unbind request by binding to peer aperture(0x1 = peer).
 *
 * The mmu fault will not happen when the bind to peer aperture is issued.
 * The fault happens if an engine attempts a translation after the bind.
 *
 * Engine needs to be rebound to a different aperture before it issues any
 * requests to the MMU to avoid fault.
 *
 * For graphics - this will ilwalidate the GPC MMU, L2 TLBs
 * In GF10X++ - hub MMU will not send ilwalidates to GPC MMU when gr engine
 * is not bound.
 *
 * NOTE: Until Turing, we can only issue an Unbind request. We don't need to
 *       issue any request to disable the Unbind
 *
 * @param[in] bUnbind   boolean to specify enable/disable unbind
 *                      We never send LW_FALSE for this HAL
 */
FLCN_STATUS
grUnbind_GMXXX(LwU8 ctrlId, LwBool bUnbind)
{
    if (bUnbind)
    {
        // wait until there is space in control fifo
        if (!PMU_REG32_POLL_US(
                LW_PFB_PRI_MMU_CTRL,
                DRF_SHIFTMASK(LW_PFB_PRI_MMU_CTRL_PRI_FIFO_SPACE),
                DRF_NUM(_PFB, _PRI_MMU_CTRL, _PRI_FIFO_SPACE, 0),
                FIFO_UNBIND_TIMEOUT_US_GMXXX, PMU_REG_POLL_MODE_BAR0_NEQ))
        {
            PMU_BREAKPOINT();
        }

        REG_WR32(BAR0, LW_PFB_PRI_MMU_BIND_IMB,
                DRF_NUM(_PFB, _PRI_MMU_BIND_IMB, _APERTURE, 0x1));
    
        REG_WR32(BAR0, LW_PFB_PRI_MMU_BIND,
                DRF_NUM(_PFB, _PRI_MMU_BIND, _ENGINE_ID,
                        LW_PFB_PRI_MMU_BIND_ENGINE_ID_GRAPHICS)|
                DRF_DEF(_PFB, _PRI_MMU_BIND, _TRIGGER, _TRUE));

        //
        // Wait until all other pending commands are flushed. This is not strictly
        // needed but we want to make sure RM is not pounding others commands while
        // we're unbinding the GR engine.
        //
        if (!PMU_REG32_POLL_US(
                LW_PFB_PRI_MMU_CTRL,
                DRF_SHIFTMASK(LW_PFB_PRI_MMU_CTRL_PRI_FIFO_EMPTY),
                DRF_DEF(_PFB, _PRI_MMU_CTRL, _PRI_FIFO_EMPTY, _TRUE),
                FIFO_UNBIND_TIMEOUT_US_GMXXX, PMU_REG_POLL_MODE_BAR0_EQ))
        {
            PMU_BREAKPOINT();
        }
    }

    return FLCN_OK;
}

/*!
 * Submits a FECS method
 *
 * @param[in] method       A method to be sent to FECS.
 * @param[in] methodData   Data to be sent along with the method
 * @param[in] pollValue    value to poll on MALBOX(0) for method completion
 * 
 * @return FLCN_STATUS  FLCN_OK    If save/Restore reglist is sucessful in 4ms/10ms.
 *                      FLCN_ERROR If save/Restore reglist is unsucessful in 4ms/10ms.
 */
FLCN_STATUS
grSubmitMethod_GMXXX
(
    LwU32 method,
    LwU32 methodData,
    LwU32 pollValue
)
{
    LW_STATUS status     = LW_OK;
    LwU32     timeoutUs  = GR_FECS_SUBMIT_METHOD_TIMEOUT_US;

    // Clear MAILBOX(0) which is used to poll for completion.
    REG_WR32(FECS, LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(0), 0);

    // Set the method data
    REG_WR32(FECS, LW_PGRAPH_PRI_FECS_METHOD_DATA, methodData);

    //
    // bump up the priority of the current task greater than that of the
    // sequencer task to prevent fb from being disabled.
    //
    lwrtosLwrrentTaskPrioritySet(OSTASK_PRIORITY(SEQ) + 1);

    // Submit Method
    REG_WR32(FECS, LW_PGRAPH_PRI_FECS_METHOD_PUSH,
        DRF_NUM(_PGRAPH, _PRI_FECS_METHOD_PUSH, _ADR, method));

    //
    // Bug : 3162570
    // According to the type of the method timeoutUS value is set 
    // For save the value is 4ms and for restore the value is 10ms.
    //
    if (method == LW_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_SAVE_REGLIST) 
    {
        timeoutUs = GR_FECS_SUBMIT_METHOD_SAVE_REGLIST_TIMEOUT_US;
    }
    else if (method == LW_PGRAPH_PRI_FECS_METHOD_PUSH_ADR_RESTORE_REGLIST)
    {   
        timeoutUs = GR_FECS_SUBMIT_METHOD_RESTORE_REGLIST_TIMEOUT_US;
    }

    // wait until we get a result under timeout
    if (!PMU_REG32_POLL_US(
            USE_FECS(LW_PGRAPH_PRI_FECS_CTXSW_MAILBOX(0)),
            LW_U32_MAX, pollValue, timeoutUs,
            PMU_REG_POLL_MODE_BAR0_EQ))
    {
        status = FLCN_ERROR;
    }

    // lower it back to the original priority.
    lwrtosLwrrentTaskPrioritySet(OSTASK_PRIORITY(LPWR));

    return status;
}
