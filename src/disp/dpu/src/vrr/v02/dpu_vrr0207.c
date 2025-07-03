/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_vrr0207.c
 * @brief  VRR 02.07 Hal Functions
 *
 * VRR Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "dpusw.h"
#include "dev_disp.h"

/* ------------------------ Application includes --------------------------- */
#include "dpu_objvrr.h"

#include "config/g_vrr_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
#define MAX_LOADV_COUNT_TRY 10
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */

/*!
 * @brief Unblocks the ELV for one frame
 *
 * @param[in]   headIndex  Index of head
 *
 * @return FLCN_OK
 *         Upon successfully writing register
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *         If invalid head index is passed
 */
FLCN_STATUS
vrrAllowOneElv_v02_07
(
    LwU8   headIndex
)
{
    LwU32 val;
    LwU32 i = MAX_LOADV_COUNT_TRY;
    LwU32 oldLoadVCount, newLoadVCount;

    if (headIndex >= LW_PDISP_HEADS)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    oldLoadVCount = REG_RD32(CSB, LW_PDISP_PIPE_IN_LOADV_COUNTER(headIndex));

    // release elv
    val = REG_RD32(CSB, LW_PDISP_DSI_ELV_BLOCK(headIndex));
    val = FLD_SET_DRF(_PDISP, _DSI_ELV_BLOCK, _ALLOW_ONE_ELV, _TRIGGER, val);
    REG_WR32(CSB, LW_PDISP_DSI_ELV_BLOCK(headIndex), val);

    // wait for the loadV to go through
    do
    {
        newLoadVCount = REG_RD32(CSB, LW_PDISP_PIPE_IN_LOADV_COUNTER(headIndex));
    } while ((oldLoadVCount == newLoadVCount) && (--i));

    return FLCN_OK;
}

/*!
 * @brief Force unstall RG
 *
 * @param[in]   headIndex  Index of head
 *
 * @return FLCN_OK
 *         Upon successfully writing a register
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *         If invalid head index is passed
 */
FLCN_STATUS
vrrTriggerRgForceUnstall_v02_07
(
    LwU8   headIndex
)
{
    LwU32 val;

    if (headIndex >= LW_PDISP_HEADS)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }
    
    // unstall RG
    val = REG_RD32(CSB, LW_PDISP_RG_STATUS(headIndex));
    val = FLD_SET_DRF(_PDISP, _RG_STATUS, _FORCE_UNSTALL, _TRIGGER, val);
    REG_WR32(CSB, LW_PDISP_RG_STATUS(headIndex), val);

    return FLCN_OK;
}

/*!
 * @brief Return if RG is stalled or not
 *
 * @param[in]   headIndex  Index of head
 * @param[out]  bStalled   Is RG stalled
 *
 * @return FLCN_OK
 *         Upon successfully reading a register
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *         If invalid head index is passed
 */
FLCN_STATUS
vrrIsRgStalled_v02_07
(
    LwU8   headIndex,
    LwBool *bStalled
)
{
    LwU32 rgStatus;

    if (headIndex >= LW_PDISP_HEADS)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    rgStatus = REG_RD32(CSB, LW_PDISP_RG_STATUS(headIndex));

    if (FLD_TEST_DRF(_PDISP, _RG_STATUS, _STALLED, _NO, rgStatus))
    {
        *bStalled = LW_FALSE;
    }
    else
    {
        *bStalled = LW_TRUE;
    }

    return FLCN_OK;
}
