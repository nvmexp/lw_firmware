/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_lsfgm20x_only.c
 * @brief  Light Secure Falcon (LSF) GM20X HAL Functions
 *
 * LSF HAL Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"

/* ------------------------ Application includes --------------------------- */
#include "rmbsiscratch.h"
#include "config/g_lsf_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/*!
 * Address defined in the linker script to mark the memory shared between PMU
 * and RM.  Specifically, this marks the starting offset for the chunk of DMEM
 * that is writeable by the RM.
 */
extern LwU32 _rm_share_start[];

/*!
 * Address defined in the linker script to mark the end of the PMU DMEM.
 * This offset combined with @ref _rm_share_start define the size of the
 * RM-writeable portion of the DMEM.
 */
extern LwU32 _end_physical_dmem[];

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */
/*!
 * @brief Initializes (opens) DMEM carveout regions.
 */
void
lsfInitDmemCarveoutRegions_GM20X(void)
{
    //
    // When DMEM queues are in use we need to open up DMEM to RM access:
    //  1. Command queues
    //  2. Message queue
    //  3. DMEM heap
    //
    // Lwrrently, the 2 sections are grouped together like so:
    //  _rm_share_start
    //      QueuesExtOrig
    //      RM DMEM heap
    //  _end_physical_dmem
    //
    // We use a single range control (RANGE0) to expose all 3.
    //

    //
    // Our carveout range controls have DMEM BLOCK (256 byte) resolution, so
    // to correctly expose _only_ these sections, _rm_share_start and
    // _end_physical_dmem both need to be block aligned.  HALT if it is not.
    //
    PMU_HALT_COND(LW_IS_ALIGNED((LwU32)_rm_share_start,    (LwU32)256));
    PMU_HALT_COND(LW_IS_ALIGNED((LwU32)_end_physical_dmem, (LwU32)256));

    // Everything appears to be aligned, proceed to open up the range.
    REG_WR32(CSB, LW_CPWR_FALCON_DMEM_PRIV_RANGE0,
             (DRF_NUM(_CPWR, _FALCON_DMEM_PRIV_RANGE0, _START_BLOCK,
                      (LwU16)(((LwU32)_rm_share_start)    >> 8)) |
              DRF_NUM(_CPWR, _FALCON_DMEM_PRIV_RANGE0, _END_BLOCK,
                      (LwU16)(((LwU32)_end_physical_dmem) >> 8))));
}
