/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_fifotu10x.c
 * @brief  HAL-interface for the TU10X Fifo Engine.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "pmu_objfifo.h"
#include "pmu_objfuse.h"
#include "dev_fuse.h"
#include "dev_ltc.h"

/* ------------------------ Application includes --------------------------- */

#include "config/g_fifo_private.h"

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------ Macros and Defines ----------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static Prototypes ------------------------------ */
/* ------------------------ Prototypes  ------------------------------------ */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * Check if engine is disabled (e.g. floorswept).
 *
 * @return  Nonzero    - if disabled or floorswept
 *          Zero       - otherwise
 */
LwBool
fifoIsEngineDisabled_TU10X
(
    LwU8 pmuEngId
)
{
    LwU32 fuse;

    switch (pmuEngId)
    {
        case PMU_ENGINE_GR:
        {
            // GR as a whole can never be floorswept.
            return LW_FALSE;
        }
        case PMU_ENGINE_VP:
        case PMU_ENGINE_PPP:
        {
            // VP and PPP have been removed and replaced by LWDEC for Maxwell.
            return LW_TRUE;
        }
        case PMU_ENGINE_BSP:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_LWDEC);
            return FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_LWDEC, _IDX, 0, _DISABLE, fuse);
        }
        case PMU_ENGINE_DEC1:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_LWDEC);
            return FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_LWDEC, _IDX, 1, _DISABLE, fuse);
        }
        case PMU_ENGINE_DEC2:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_LWDEC);
            return FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_LWDEC, _IDX, 2, _DISABLE, fuse);
        }
        case PMU_ENGINE_ENC0:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_LWENC);
            return FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_LWENC, _IDX, 0, _DISABLE, fuse);
        }
        case PMU_ENGINE_ENC1:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_LWENC);
            return FLD_IDX_TEST_DRF(_FUSE, _STATUS_OPT_LWENC, _IDX, 1, _DISABLE, fuse);
        }
        case PMU_ENGINE_SEC2:
        {
            fuse = fuseRead(LW_FUSE_STATUS_OPT_SEC);
            return FLD_TEST_DRF(_FUSE, _STATUS_OPT_SEC, _DATA, _DISABLE, fuse);
        }
        //
        // This fuse is removed from GP10X+, CE will no longer be disabled in HW
        // See bug 200025697
        //
        case PMU_ENGINE_CE0:
        case PMU_ENGINE_CE1:
        case PMU_ENGINE_CE2:
        case PMU_ENGINE_CE3:
        case PMU_ENGINE_CE4:
        case PMU_ENGINE_CE5:
        {
            return LW_FALSE;
        }
    }
    return LW_FALSE;
}

/*!
 * Disables PLC_RECOMPRESS_{PLC,RMW}, see bug 2265227
 */
void
fifoDisablePlcRecompressWar_TU10X(void)
{
    LwU32 val;

    val = REG_RD32(BAR0, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_1);

    val = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_SET_MGMT_1,
            _PLC_RECOMPRESS_PLC, _ENABLED, val);
    val = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_SET_MGMT_1,
            _PLC_RECOMPRESS_RMW, _ENABLED, val);
    val = FLD_SET_DRF_NUM(_PLTCG, _LTCS_LTSS_TSTG_SET_MGMT_1,
            _PLC_THRESHOLD, 0x8, val);

    REG_WR32(BAR0, LW_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_1, val);
}
