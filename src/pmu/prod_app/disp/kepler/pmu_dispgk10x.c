/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_dispgk10x.c
 * @brief  HAL-interface for the GK10x Display Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_disp.h"
#include "dev_pwr_csb.h" // For profiling using mailbox regsiters
#include "pmu/pmumailboxid.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_objdisp.h"
#include "pmu_objtimer.h"
#include "pmu_objseq.h"
#include "pmu_disp.h"
#include "pmu_gc6.h"
#include "dbgprintf.h"
#include "pmu/pmuifseq.h"
#include "config/g_disp_private.h"
#include "class/cl917a.h"
#include "class/cl917d.h"
#include "class/cl917c.h"
#include "class/cl907d.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

#define PMS_DEBUG   0 // enable for debug info

#if PMS_DEBUG
#define PMS_DEBUG_CHECKPOINT(cpoint)     PMS_DEBUG_CHECKPOINT (REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(      \
                                          RM_PMU_MAILBOX_ID_PMS_DEBUG_CHECKPOINT_REG), (cpoint))
#else
#define PMS_DEBUG_CHECKPOINT(cpoint)
#endif

#define MAX_BACK_PORCH_EXTEND_VALUE         DRF_MASK(LW_PDISP_RG_RASTER_V_EXTEND_BACK_PORCH_EXTEND_HEIGHT)
#define INIT_BACK_PORCH_EXTEND_VALUE        0

/*!
 * This has to be adjusted based on the time DISP is really taking to process
 * the method submitted to the debug channel. For now setting an aggressive
 * timeout.
 */
#define DISP_METHOD_COMPLETION_WAIT_DELAY_US 4
#define WAIT_FOR_SVX_INTR_USECS              100     // Wait for SuperVisor interrupt
#define TIMEOUT_FOR_SVX_INTR_USECS           10*1000 // Timeout for SuperVisor interrupt set to 10 msec
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static LwBool s_dispCheckForMethodCompletion_GMXXX (LwU32 dsiDebugCtl)
     GCC_ATTRIB_SECTION("imem_disp", "s_dispCheckForMethodCompletion_GMXXX") GCC_ATTRIB_USED();

/*!
 * Get the number of heads.
 *
 * @return Number of heads.
 *
 * @note
 *     This HAL is always safe to call.
 */
LwU8
dispGetNumberOfHeads_GMXXX(void)
{
    return DISP_IS_PRESENT() ?
        DRF_VAL(_PDISP, _CLK_REM_MISC_CONFIGA, _NUM_HEADS,
            REG_RD32(BAR0, LW_PDISP_CLK_REM_MISC_CONFIGA)) : 0;
}

/*!
 * Gets offset of LW_PDISP_DSI_DEBUG_CTL, LW_PDISP_DSI_DEBUG_DAT
 *
 * @param[in] dispChnlNum  Display channel number.
 * @param[out] pDsiDebugCtl LW_PDISP_DSI_DEBUG_CTL offset.
 * @param[out] dsiDebugDat LW_PDISP_DSI_DEBUG_DAT offset.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT if channel is invalid.
 * @return FLCN_OK otherwise.
 */
FLCN_STATUS
dispGetDebugCtlDatOffsets_GMXXX
(
    LwU32   dispChnlNum,
    LwU32   *pDsiDebugCtl,
    LwU32   *pDsiDebugDat
)
{
    if (dispChnlNum >= LW_PDISP_DSI_DEBUG_CTL__SIZE_1)
    {
        // Unexpected channel number.
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Return Debug control and data registers.
    *pDsiDebugCtl = LW_PDISP_DSI_DEBUG_CTL(dispChnlNum);
    *pDsiDebugDat = LW_PDISP_DSI_DEBUG_DAT(dispChnlNum);

    return FLCN_OK;
}

/*!
 * Enqueues method, data for a channel with given dbgctrl register.
 *
 * @param[in] dispChnlNum  Display channel number.
 * @param[in] methodOffset Display method offset.
 * @param[in] data         Method data.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT if channel is invalid.
 * @return FLCN_ERROR on timeout of method completion.
 * @return FLCN_OK otherwise.
 */
FLCN_STATUS
dispEnqMethodForChannel_GMXXX
(
    LwU32   dispChnlNum,
    LwU32   methodOffset,
    LwU32   data
)
{
    LwU32       dsiDebugCtl;
    LwU32       dsiDebugDat;
    LwU32       debugCtlRegVal;
    FLCN_STATUS status;

    if ((status = dispGetDebugCtlDatOffsets_HAL(&Disp, dispChnlNum, &dsiDebugCtl, &dsiDebugDat)) != FLCN_OK)
    {
        return status;
    }

    // Method offset in cl507*.h is in bytes. Let's colwert it to DWORD offset
    // because that's what we use everywhere
    methodOffset >>= 2;
    // First write the data.
    REG_WR32(BAR0, dsiDebugDat, data);

    // Now write the method and the trigger
    debugCtlRegVal = REG_RD32(BAR0, dsiDebugCtl);
    debugCtlRegVal = FLD_SET_DRF_NUM(_PDISP, _DSI_DEBUG_CTL, _METHOD_OFS,
                                    methodOffset, debugCtlRegVal);
    debugCtlRegVal = FLD_SET_DRF(_PDISP, _DSI_DEBUG_CTL, _CTXDMA, _NORMAL,
                                 debugCtlRegVal);
    debugCtlRegVal = FLD_SET_DRF(_PDISP, _DSI_DEBUG_CTL, _NEW_METHOD,
                                 _TRIGGER, debugCtlRegVal);
    debugCtlRegVal = FLD_SET_DRF(_PDISP, _DSI_DEBUG_CTL, _MODE, _ENABLE,
                                 debugCtlRegVal);

    REG_WR32(BAR0, dsiDebugCtl, debugCtlRegVal);

    if (!OS_PTIMER_SPIN_WAIT_US_COND(s_dispCheckForMethodCompletion_GMXXX,
        dsiDebugCtl, DISP_METHOD_COMPLETION_WAIT_DELAY_US))
    {
        // Method timed out, hence restore debug mode setting and return error.
        return FLCN_ERROR;
    }
    return FLCN_OK;
}


/*!
 * Checks to see if the method submitted is consumed / pending.
 *
 * @param[in] dsiDebutCtl  BAR0 register offset of DEBUG_CTL.
 *
 * @return LW_TRUE if method exelwtion is complete.
 */
static LwBool
s_dispCheckForMethodCompletion_GMXXX
(
    LwU32 dsiDebugCtl
)
{
    LwU32 regVal = REG_RD32(BAR0, dsiDebugCtl);

    if (FLD_TEST_DRF(_PDISP, _DSI_DEBUG_CTL, _NEW_METHOD, _PENDING, regVal))
    {
        return LW_FALSE;
    }
    else
    {
        return LW_TRUE;
    }
}
