/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_mstuxxxadxxx.c
 * @brief  HAL-interface for the TU10X thorugh AD10X Memory System Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_bus.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objms.h"

#include "config/g_ms_private.h"

/*!
 * @brief: Host memory operation checking function
 *
 * In this fucntion we will check for pending/outstanding
 * memory  operation in the HOST. Bug 1893989 has detailed
 * information.
 *
 * This regsiter will provide the following Host Mem Ops
 * status:
 *
 * 1. BAR_BIND_STATUS - This status will indicate if there
 * is any BAR1/BAR2 bind requests are pending/outstanding.
 *
 * 2. L2_SYSMEM - This bit will indicate if there is any
 * L2 sysmem ilwalidate operation is pending which is issued
 * by CPU for any of the GFID.
 *
 * 3. L2_PEERMEM - This bit will indidcate if there is any
 * Peer to peer memory L2 data ilwalidate is pending which
 * is issued by CPU for any of the GFID.
 *
 * 4. L2 CLEAN COMPTAGS - This bit will indicate if there
 * is any CPU initiated L2 compgression tag clean operation
 * which is pending/outstanding.
 *
 * 5. L2 FLUSH - This bit will indicate if there is any L2
 * cache dirtly line flush is pending or not.
 *
 * 6. L2 WAIT SYS READ - This bit will indicate if there is any
 * L2 wait operation pending for pending system reads.
 *
 * @return   LW_TRUE  if a host mem op are pending
 * @return   LW_FALSE otherwise
 */
LwBool
msIsHostMemOpPending_TU10X(void)
{
    LwU32 val;

    val = REG_RD32(BAR0, LW_PBUS_POWER_STATUS);
    if (FLD_TEST_DRF(_PBUS, _POWER_STATUS, _BAR_BIND_STATUS,
                     _IN_PROGRESS, val)                                  ||
        FLD_TEST_DRF(_PBUS, _POWER_STATUS, _FLUSH_STATUS,
                     _IN_PROGRESS, val)                                  ||
        FLD_TEST_DRF(_PBUS, _POWER_STATUS, _L2_SYSMEM_ILWALIDATE_STATUS,
                     _IN_PROGRESS, val)                                  ||
        FLD_TEST_DRF(_PBUS, _POWER_STATUS, _L2_PEERMEM_ILWALIDATE_STATUS,
                     _IN_PROGRESS, val)                                  ||
        FLD_TEST_DRF(_PBUS, _POWER_STATUS, _L2_CLEAN_COMPTAGS_STATUS,
                     _IN_PROGRESS, val)                                  ||
        FLD_TEST_DRF(_PBUS, _POWER_STATUS, _L2_FLUSH_DIRTY_STATUS,
                     _IN_PROGRESS, val)                                  ||
        FLD_TEST_DRF(_PBUS, _POWER_STATUS, _L2_WAIT_FOR_SYS_PENDING_READS_STATUS,
                     _IN_PROGRESS, val))
    {
        return LW_TRUE;
    }

    return LW_FALSE;
}
