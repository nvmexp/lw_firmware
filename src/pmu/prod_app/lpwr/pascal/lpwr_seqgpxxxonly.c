/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

 /*!
  * @file   lpwr_seqgpxxxonly.c
  * @brief  Implements LPWR Sequencer for Pascal GPU
  */

  /* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_chiplet_pwr.h"

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objhal.h"
#include "pmu_objlpwr.h"
#include "dbgprintf.h"

#include "config/g_lpwr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void s_lpwrSeqGfxRppgEntry_GP10X(LwU32 timeoutUs, LwBool bBlocking);
static void s_lpwrSeqGfxRppgExit_GP10X (LwU32 timeoutUs);
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Set the state of SRAM Sequencer Ctrl
 *
 * @params[in] ctrlId      SRAM Sequencer CTRL Id
 * @params[in] targetState Requested SRAM Sequencer State
 * @params[in] bBlocking   Blocking or non Blocking call
 *
 *  @return    FLCN_OK          Success
 *             FLCN_ERR_xxx     Failure
 *
 */
FLCN_STATUS
lpwrSeqSramStateSetHal_GP10X
(
    LwU8   ctrlId,
    LwU8   targetState,
    LwBool bBlocking
)
{
    LPWR_SEQ_CTRL *pSramSeqCtrl = LPWR_SEQ_GET_SRAM_CTRL(ctrlId);

    // We only support GFX Domain RPPG on Pascal
    if ((ctrlId != LPWR_SEQ_SRAM_CTRL_ID_GR))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if (targetState == LPWR_SEQ_STATE_SRAM_RPPG)
    {
        s_lpwrSeqGfxRppgEntry_GP10X(pSramSeqCtrl->timeoutUs, bBlocking);
    }
    else if (targetState == LPWR_SEQ_STATE_PWR_ON)
    {
        s_lpwrSeqGfxRppgExit_GP10X(pSramSeqCtrl->timeoutUs);
    }

    return FLCN_OK;
}

/*!
 * @brief Initialize RPPG Sequencer
 *
 * This function initializes sleep mode for RPPG and enables FAST_MODE for
 * staggering logic.
 */
void
lpwrSeqSramInit_GP10X(void)
{
    LwU32 regVal;

    regVal = REG_RD32(CSB, LW_CPWR_PMU_RPPG_CTRL);
    regVal = FLD_SET_DRF(_CPWR_PMU, _RPPG_CTRL, _FAST_MODE, _EN, regVal);
    REG_WR32(CSB, LW_CPWR_PMU_RPPG_CTRL, regVal);

    //
    // Disable partitions 1(war for bug 200382249),17 ,18 and 20
    // from PMU_RPPG_PARTITION_EN_0
    //
    regVal = REG_RD32(CSB, LW_CPWR_PMU_RPPG_PARTITION_EN_0);
    regVal = FLD_SET_DRF(_CPWR_PMU, _RPPG_PARTITION_EN_0, _P1, _DIS, regVal);
    regVal = FLD_SET_DRF(_CPWR_PMU, _RPPG_PARTITION_EN_0, _P17, _DIS, regVal);
    regVal = FLD_SET_DRF(_CPWR_PMU, _RPPG_PARTITION_EN_0, _P18, _DIS, regVal);
    regVal = FLD_SET_DRF(_CPWR_PMU, _RPPG_PARTITION_EN_0, _P20, _DIS, regVal);
    REG_WR32(CSB, LW_CPWR_PMU_RPPG_PARTITION_EN_0, regVal);

    // Disable partition 4 to 9
    regVal = REG_RD32(FECS, LW_PCHIPLET_PWR_FBPS_RPPG_PARTITION_EN_0);
    regVal = FLD_SET_DRF(_PCHIPLET_PWR, _FBPS_RPPG_PARTITION_EN_0, _P4, _DIS, regVal);
    regVal = FLD_SET_DRF(_PCHIPLET_PWR, _FBPS_RPPG_PARTITION_EN_0, _P5, _DIS, regVal);
    regVal = FLD_SET_DRF(_PCHIPLET_PWR, _FBPS_RPPG_PARTITION_EN_0, _P6, _DIS, regVal);
    regVal = FLD_SET_DRF(_PCHIPLET_PWR, _FBPS_RPPG_PARTITION_EN_0, _P7, _DIS, regVal);
    regVal = FLD_SET_DRF(_PCHIPLET_PWR, _FBPS_RPPG_PARTITION_EN_0, _P8, _DIS, regVal);
    regVal = FLD_SET_DRF(_PCHIPLET_PWR, _FBPS_RPPG_PARTITION_EN_0, _P9, _DIS, regVal);
    REG_WR32(FECS, LW_PCHIPLET_PWR_FBPS_RPPG_PARTITION_EN_0, regVal);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Enter the RPPG mode for Graphics SRAMs
 *
 * SW sequence to engage RPPG:
 * 1) Trigger entry for gfx domain
 * 2) Wait for completion in case we have blocking call.
 * 3) Update state cache to notify the completion of RPPG entry.
 *
 * @param[in] timeoutUs Timeout in uS for blocking Call
 * @param[in] bBlocking If its a blocking call
 */
void
s_lpwrSeqGfxRppgEntry_GP10X
(
    LwU32  timeoutUs,
    LwBool bBlocking
)
{
    LwU32 regVal = 0;

    // Trigger RPPG entry sequence for domain represented by domainId.
    regVal = REG_RD32(CSB, LW_CPWR_PMU_RPPG_CTRL);
    regVal = FLD_SET_DRF(_CPWR_PMU, _RPPG_CTRL, _GFX_ENTRY, _TRIGGER, regVal);
    REG_WR32(CSB, LW_CPWR_PMU_RPPG_CTRL, regVal);

    // Wait for entry completion - Blocking call
    if (bBlocking)
    {
        if (!PMU_REG32_POLL_US(LW_CPWR_PMU_RPPG_CTRL,
                               DRF_SHIFTMASK(LW_CPWR_PMU_RPPG_CTRL_GFX_ENTRY),
                               LW_CPWR_PMU_RPPG_CTRL_GFX_ENTRY_DONE,
                               timeoutUs,
                               PMU_REG_POLL_MODE_CSB_EQ))
        {
            PMU_BREAKPOINT();
        }
    }
    // Hardcoded wait for non-blocking call
    else
    {
        //
        // Wait for non-blocking call.
        //
        // HW reports incorrect status for immidiate exit triggered after
        // non-blocking entry call. This can be avoided by adding 100ns delay
        // after RPPG entry non-blocking call.
        //
        OS_PTIMER_SPIN_WAIT_NS(LPWR_SEQ_SRAM_CTRL_MIN_WAIT_FOR_COMPLETION_NS);
    }
}

/*!
 * @brief Exit RPPG Mode for Graphics SRAM
 *
 * SW sequence to disengage RPPG:
 * 1) Trigger exit for given domainId
 * 2) Wait for completion in case we have blocking call.
 * 3) Update state cache to notify the completion of RPPG exit.
 *
 * @param[in] timeouUs Timeout for blocking exit
 */
void
s_lpwrSeqGfxRppgExit_GP10X
(
    LwU32 timeoutUs
)
{
    LwU32 regVal = 0;

    // Trigger RPPG exit sequence for domain represented by domainId.
    regVal = REG_RD32(CSB, LW_CPWR_PMU_RPPG_CTRL);
    regVal = FLD_SET_DRF(_CPWR_PMU, _RPPG_CTRL, _GFX_EXIT, _TRIGGER, regVal);
    REG_WR32(CSB, LW_CPWR_PMU_RPPG_CTRL, regVal);

    // Wait for exit completion - Blocking call
    if (!PMU_REG32_POLL_US(LW_CPWR_PMU_RPPG_CTRL,
                           DRF_SHIFTMASK(LW_CPWR_PMU_RPPG_CTRL_GFX_EXIT),
                           LW_CPWR_PMU_RPPG_CTRL_GFX_EXIT_DONE,
                           timeoutUs,
                           PMU_REG_POLL_MODE_CSB_EQ))
    {
        PMU_BREAKPOINT();
    }
}
