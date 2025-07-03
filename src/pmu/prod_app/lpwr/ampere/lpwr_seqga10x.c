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
  * @file   lpwr_seqga10x.c
  * @brief  Implements LPWR Sequencer for Ampere
  */

  /* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_chiplet_pwr.h"
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
#include "ram_profile_init_seq.h"
#endif // (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objhal.h"
#include "pmu_objlpwr.h"
#include "dbgprintf.h"

#include "config/g_lpwr_private.h"

/* ------------------------- Macros and Defines ----------------------------- */

//
// Profile to be used by SW for SRAM Sequencer
// TODO-pankumar: Get this macro defined in HW File.
//
#ifndef SRAM_PG_PROFILE_POWER_ON
#define SRAM_PG_PROFILE_POWER_ON   LW_CPWR_PMU_RAM_TARGET_SEQ_POWER_ON
#endif

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
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
lpwrSeqSramStateSetHal_GA10X
(
    LwU8   ctrlId,
    LwU8   targetState,
    LwBool bBlocking
)
{
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
    LPWR_SEQ_CTRL *pSramSeqCtrl = LPWR_SEQ_GET_SRAM_CTRL(ctrlId);
    LwU32          regVal       = 0;
    LwU8           profile      = 0;

    if (targetState == LPWR_SEQ_STATE_SRAM_RPPG)
    {
        profile = LPWR_SRAM_PG_PROFILE_GET(RPPG);
    }
    else if (targetState == LPWR_SEQ_STATE_SRAM_RPG)
    {
        profile = LPWR_SRAM_PG_PROFILE_GET(RPG);
    }
    else if (targetState == LPWR_SEQ_STATE_PWR_ON)
    {
        profile = LPWR_SRAM_PG_PROFILE_GET(POWER_ON);
    }

    // Set the profile in Sequencer for the given ctrlId.
    regVal = REG_RD32(CSB, LW_CPWR_PMU_RAM_TARGET);
    regVal = FLD_IDX_SET_DRF_NUM(_CPWR_PMU, _RAM_TARGET, _SEQ,
                                 ctrlId, profile, regVal);
    REG_WR32_STALL(CSB, LW_CPWR_PMU_RAM_TARGET, regVal);

    // Wait for entry completion - Blocking call
    if (bBlocking)
    {
        if (!PMU_REG32_POLL_US(LW_CPWR_PMU_RAM_STATUS,
                 DRF_SHIFTMASK(LW_CPWR_PMU_RAM_STATUS_PROFILE_SEQ(ctrlId)),
                 DRF_IDX_NUM(_CPWR, _PMU_RAM_STATUS, _PROFILE_SEQ, ctrlId, profile),
                 pSramSeqCtrl->timeoutUs,
                 PMU_REG_POLL_MODE_CSB_EQ))
        {
            PMU_BREAKPOINT();
        }
    }
#endif // (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
    return FLCN_OK;
}

/*!
 * @brief Function to initialize the SRAM Sequencer Profile Configuration
 *
 * Note: Most of the partition are programmed by HW using the Power on
 * reset values.
 *
 * We only need to program some of the FBP partition for MS-RPPG and
 * MS-L2RPG features only.
 *
 * However, below function is generic and can be used for any SRAM
 * partition. Just update the ram_profile_init_seq.h file with
 * required configuration.
 */
void
lpwrSeqSramInit_GA10X(void)
{
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
    LwU32 idx     = 0;
    LwU32 listIdx = 0;

    //
    // Extract the SRAM PG Profile Configuration from the
    // ram_profile_init_seq.h file provided by HW folks
    //
    LPWR_SRAM_PG_PROFILE_INIT lpwrSramPgProfileInitList[] =
    {
        SRAM_PG_PROFILE_INIT(EXTRACT_LPWR_SRAM_PG_PROFILE_CONFIG)
    };

    // Initialize all the SRAM Partition's PG Profile
    for (listIdx = 0; listIdx <  LW_ARRAY_ELEMENTS(lpwrSramPgProfileInitList);
         listIdx++)
    {
        // Write all the partition from selected list
        for (idx = 0; idx < lpwrSramPgProfileInitList[listIdx].numWrite; idx++)
        {
            REG_WR32(FECS, lpwrSramPgProfileInitList[listIdx].regAddr,
                     lpwrSramPgProfileInitList[listIdx].regVal);
        }
    }

    // Disable the HSHUB Partition RPPG - Partition 18 and 19. Bug 3017315
    REG_WR32_STALL(CSB, LW_CPWR_PMU_POWER_CTRL_MS_RAM_PARTITION1_INDEX, 0x312);
    REG_WR32_STALL(CSB, LW_CPWR_PMU_POWER_CTRL_MS_RAM_PARTITION1, 0);
    REG_WR32_STALL(CSB, LW_CPWR_PMU_POWER_CTRL_MS_RAM_PARTITION1, 0);

#endif //(!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
}

/* ------------------------- Private Functions ------------------------------ */
