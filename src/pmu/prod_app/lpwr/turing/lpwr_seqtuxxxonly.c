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
  * @file   lpwr_seqtuxxxonly.c
  * @brief  Implements LPWR Sequencer for Turing
  */

  /* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_chiplet_pwr.h"
#include "dev_fbpa.h"

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
void s_lpwrSeqRppgEntry_TU10X(LwU8 ctrlId, LwBool bBlocking);
void s_lpwrSeqRppgExit_TU10X (LwU8 ctrlId);
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
lpwrSeqSramStateSetHal_TU10X
(
    LwU8   ctrlId,
    LwU8   targetState,
    LwBool bBlocking
)
{
    if (targetState == LPWR_SEQ_STATE_SRAM_RPPG)
    {
        s_lpwrSeqRppgEntry_TU10X(ctrlId, bBlocking);
    }
    else if (targetState == LPWR_SEQ_STATE_PWR_ON)
    {
        s_lpwrSeqRppgExit_TU10X(ctrlId);
    }

    return FLCN_OK;
}

/*!
 * @brief Function Init LPWR Logic Sequencer settings
 */
void
lpwrSeqSramInit_TU10X(void)
{
    LwU32 regVal = 0;
    LwU32 idx    = 0;

    //
    // Disable the TPC PG on NAFLL LUT which is present in partition GTBG0SC
    // Bug: 2192213 has all the details.
    //

    //
    // Set the index to GTBG0SC partition i.e. 44 (0x2c) and then disable the
    // power gating.
    //
    REG_WR32(FECS, LW_PCHIPLET_PWR_GPCS_POWER_CTRL_GR_RAM_PARTITION_INDEX, 0x2c);
    REG_WR32(FECS, LW_PCHIPLET_PWR_GPCS_POWER_CTRL_GR_RAM_PARTITION, 0x0);

    //
    // Disable the MS RPPG on all FPBS partitions
    // Bug: 2155180 has all the details.
    //
    regVal = FLD_SET_DRF(_PCHIPLET_PWR, _FBPS_POWER_CTRL_MS_RAM_PARTITION_INDEX,
                          _WRITEINCR, _ENABLED, 0);
    REG_WR32(FECS, LW_PCHIPLET_PWR_FBPS_POWER_CTRL_MS_RAM_PARTITION_INDEX, regVal);

    for (idx = LW_PCHIPLET_PWR_FBPS_POWER_CTRL_MS_RAM_PARTITION_INDEX_INDEX_MIN;
         idx <= LW_PCHIPLET_PWR_FBPS_POWER_CTRL_MS_RAM_PARTITION_INDEX_INDEX_MAX;
         idx++)
    {
        REG_WR32(FECS, LW_PCHIPLET_PWR_FBPS_POWER_CTRL_MS_RAM_PARTITION, 0x0);
    }
}

/*!
 * @brief Function to update the PLM of PG Sequencer
 */
void
lpwrSeqPlmConfig_TU10X(void)
{
    LwU32 regVal = 0;

    // Disable the PMU PRIV Support for PMU PG Sequence registers
    regVal = REG_RD32(CSB, LW_CPWR_PMU_POWER_CONTROL_PRIV_LEVEL_MASK);

    regVal = FLD_SET_DRF(_CPWR, _PMU_POWER_CONTROL_PRIV_LEVEL_MASK,
                         _WRITE_PROTECTION_LEVEL2, _DISABLE, regVal);
    REG_WR32(CSB, LW_CPWR_PMU_POWER_CONTROL_PRIV_LEVEL_MASK, regVal);


    // Disable the PMU PRIV LEVEL 2 Support for GPCS PG Sequence registers
    regVal = REG_RD32(FECS, LW_PCHIPLET_PWR_GPCS_POWER_CONTROL_PRIV_LEVEL_MASK);

    regVal = FLD_SET_DRF(_PCHIPLET, _PWR_GPCS_POWER_CONTROL_PRIV_LEVEL_MASK,
                         _WRITE_PROTECTION_LEVEL2, _DISABLE, regVal);
    REG_WR32(FECS, LW_PCHIPLET_PWR_GPCS_POWER_CONTROL_PRIV_LEVEL_MASK, regVal);


    // Disable the PMU PRIV LEVEL 2 Support for FBPS PG Sequence registers
    regVal = REG_RD32(FECS, LW_PCHIPLET_PWR_FBPS_POWER_CONTROL_PRIV_LEVEL_MASK);

    regVal = FLD_SET_DRF(_PCHIPLET, _PWR_FBPS_POWER_CONTROL_PRIV_LEVEL_MASK,
                         _WRITE_PROTECTION_LEVEL2, _DISABLE, regVal);
    REG_WR32(FECS, LW_PCHIPLET_PWR_FBPS_POWER_CONTROL_PRIV_LEVEL_MASK, regVal);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Enter the RPPG Mode
 *
 * SW sequence to engage RPPG:
 * 1) Trigger entry for given domain as per ctrlId
 * 2) Wait for completion in case we have blocking call.
 *
 * @param[in] ctrlId    SRAM Sequencer CTRL ID - LPWR_SEQ_SRAM_CTRL_ID_*
 * @param[in] bBlocking If its a blocking call
 */
void
s_lpwrSeqRppgEntry_TU10X
(
    LwU8   ctrlId,
    LwBool bBlocking
)
{
    LPWR_SEQ_CTRL *pSramSeqCtrl = LPWR_SEQ_GET_SRAM_CTRL(ctrlId);
    LwU32          regVal       = 0;

    // Trigger RPPG entry sequence for domain represented by ctrlId
    regVal = REG_RD32(CSB, LW_CPWR_PMU_RAM_TARGET);
    regVal = FLD_IDX_SET_DRF(_CPWR_PMU, _RAM_TARGET, _SEQ, ctrlId, _RPPG, regVal);
    REG_WR32_STALL(CSB, LW_CPWR_PMU_RAM_TARGET, regVal);

    // Wait for entry completion - Blocking call
    if (bBlocking)
    {
        if (!PMU_REG32_POLL_US(LW_CPWR_PMU_RAM_STATUS,
                       DRF_SHIFTMASK(LW_CPWR_PMU_RAM_STATUS_SEQ(ctrlId)),
                       DRF_IDX_DEF(_CPWR, _PMU_RAM_STATUS, _SEQ, ctrlId, _RPPG),
                       pSramSeqCtrl->timeoutUs,
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
 * @brief Exit the RPPG Mode
 *
 * SW sequence to disengage RPPG:
 * 1) Trigger exit for given domainId
 * 2) Wait for completion.
 *
 * @param[in] ctrlId    SRAM Sequencer CTRL ID - LPWR_SEQ_SRAM_CTRL_ID_*
 */
void
s_lpwrSeqRppgExit_TU10X
(
    LwU8 ctrlId
)
{
    LPWR_SEQ_CTRL *pSramSeqCtrl = LPWR_SEQ_GET_SRAM_CTRL(ctrlId);
    LwU32          regVal       = 0;

    // Trigger RPPG exit sequence for domain represented by domainId.
    regVal = REG_RD32(CSB, LW_CPWR_PMU_RAM_TARGET);
    regVal = FLD_IDX_SET_DRF(_CPWR_PMU, _RAM_TARGET, _SEQ, ctrlId, _POWER_ON, regVal);
    REG_WR32_STALL(CSB, LW_CPWR_PMU_RAM_TARGET, regVal);

    // Wait for entry completion - Blocking call
    if (!PMU_REG32_POLL_US(LW_CPWR_PMU_RAM_STATUS,
                   DRF_SHIFTMASK(LW_CPWR_PMU_RAM_STATUS_SEQ(ctrlId)),
                   DRF_IDX_DEF(_CPWR, _PMU_RAM_STATUS, _SEQ, ctrlId, _POWER_ON),
                   pSramSeqCtrl->timeoutUs,
                   PMU_REG_POLL_MODE_CSB_EQ))
    {
        PMU_BREAKPOINT();
    }
}
