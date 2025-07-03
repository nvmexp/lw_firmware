/*
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file pmu_seqgp100.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_hshub.h"
#include "dev_fb.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objseq.h"
#include "pmu_bar0.h"
#include "dbgprintf.h"
#include "config/g_seq_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Update the LTCS/LTCS2 registers
 *
 * @param[in]  opcode
 *      The opcode for the seq instr
 *
 * @param[in]  value
 *      Value of the ltcs/ltcs2 register to be updated
 */
void
seqUpdateLtcsRegs_GP100
(
    LwU8  opcode,
    LwU32 value
)
{
    switch (opcode)
    {
        case LW_PMU_SEQ_LTCS_OPC:
        {
            PMU_BAR0_WR32_SAFE(LW_PFB_FBHUB_NUM_ACTIVE_LTCS, value);
            PMU_BAR0_WR32_SAFE(LW_PFB_HSHUB_NUM_ACTIVE_LTCS, value);
            break;
        }

        case LW_PMU_SEQ_LTCS2_OPC:
        {
            PMU_BAR0_WR32_SAFE(LW_PFB_FBHUB_NUM_ACTIVE_LTCS2, value);
            PMU_BAR0_WR32_SAFE(LW_PFB_HSHUB_NUM_ACTIVE_LTCS2, value);
            break;
        }

        default:
            //
            // We let this fall through as failing/asserting may leave
            // the PMU at an elevated prilevel.
            //
            break;
    }
}
