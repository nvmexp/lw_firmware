/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020  by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    rrlga100.c
 * @brief   PMU HAL functions for GA100, handling SMBPBI queries for
 *          Row remapper.
 */

/* ------------------------ System Includes -------------------------------- */
#include "lwuproc.h"
#include "pmusw.h"

/* ------------------------ Application Includes --------------------------- */
#include "task_therm.h"
#include "dbgprintf.h"
#include "pmu_objsmbpbi.h"

#include "config/g_smbpbi_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Function Prototypes  -------------------- */
/* ------------------------ Static Function Prototypes  -------------------- */

LwU8 s_smbpbiGetRemapRowStateFlags(LwU8 arg2, LwU32 *pData);
LwU8 s_smbpbiGetRemapRowRawCounts(LwU8 arg2, LwU32 *pData);
LwU8 s_smbpbiGetRemapRowHistogram(LwU8 arg2, LwU32 *pCmd, LwU32 *pData, LwU32 *pExtData);

/* ------------------------ Public Functions  ------------------------------ */

/*!
 * Handler for the LW_MSGBOX_CMD_OPCODE_REMAP_ROW_STATS command.
 * Reports the requested row-remapping stats
 *
 * @param[in/out] pCmd
 *     Pointer to dword containing the opcode/args and possibly output
 *
 * @param[out] pData
 *     Pointer to dword populated with requested row-remapping stats
 *
 * @param[out] pExtData
 *     Pointer to dword populated with requested row-remapping stats

 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     Successfully serviced request.
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED
 *     Row remap data not available
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     Internal error
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG1
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *     Error in request arguments
 */
LwU8
smbpbiGetRemapRowStats_GA100
(
    LwU32 *pCmd,
    LwU32 *pData,
    LwU32 *pExtData
)
{
    LwU8 status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
    LwU8 arg1, arg2;

    if (!(PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_RRL_STATS) &&
            LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2, _REMAP_ROW_STATS)))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
        goto smbpbiGetRemapRowStats_GA100_exit;
    }

    if (pData)
    {
        *pData = 0;
    }
    else
    {
        goto smbpbiGetRemapRowStats_GA100_exit;
    }

    arg1 = DRF_VAL(_MSGBOX, _CMD, _ARG1, *pCmd);
    arg2 = DRF_VAL(_MSGBOX, _CMD, _ARG2, *pCmd);

    switch (arg1)
    {
        case LW_MSGBOX_CMD_ARG1_REMAP_ROWS_RAW_COUNTS:
        {
            status = s_smbpbiGetRemapRowRawCounts(arg2, pData);
            break;
        }
        case LW_MSGBOX_CMD_ARG1_REMAP_ROWS_STATE_FLAGS:
        {
            status = s_smbpbiGetRemapRowStateFlags(arg2, pData);
            break;
        }
        case LW_MSGBOX_CMD_ARG1_REMAP_ROWS_HISTOGRAM:
        {
            status = s_smbpbiGetRemapRowHistogram(arg2, pCmd, pData, pExtData);
            break;
        }
        default:
            status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
    }

smbpbiGetRemapRowStats_GA100_exit:
    return status;
}

/*!
 * Get row remapping raw counts
 *
 * @param[in]   arg2      Requested count type
 * @param[out]  pData     Pointer to output Data
 *
 * @return      status    SMBPBI status
 */
LwU8
s_smbpbiGetRemapRowRawCounts
(
    LwU8  arg2,
    LwU32 *pData
)
{
    LwU8 status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    switch (arg2)
    {
        case LW_MSGBOX_CMD_ARG2_REMAP_ROW_RAW_CNT_COMBINED:
        {
            *pData = FLD_SET_DRF_NUM(_MSGBOX_DATA, _REMAP_ROW, _UNCORR_CNT,
                                        Smbpbi.remappedRowsCntUnc, *pData);
            *pData = FLD_SET_DRF_NUM(_MSGBOX_DATA, _REMAP_ROW, _CORR_CNT,
                                        Smbpbi.remappedRowsCntCor, *pData);

            if ((Smbpbi.remappedRowsCntUnc >> DRF_SIZE(LW_MSGBOX_DATA_REMAP_ROW_UNCORR_CNT)) != 0)
            {
                *pData = FLD_SET_DRF(_MSGBOX_DATA, _REMAP_ROW, _UNCORR_CNT_EXCESS,
                                        _TRUE, *pData);
            }

            if ((Smbpbi.remappedRowsCntCor >> DRF_SIZE(LW_MSGBOX_DATA_REMAP_ROW_CORR_CNT)) != 0)
            {
                *pData = FLD_SET_DRF(_MSGBOX_DATA, _REMAP_ROW, _CORR_CNT_EXCESS,
                                        _TRUE, *pData);
            }

            break;
        }
        case LW_MSGBOX_CMD_ARG2_REMAP_ROW_RAW_CNT_UNCORR:
        {
            *pData = Smbpbi.remappedRowsCntUnc;
            break;
        }
        case LW_MSGBOX_CMD_ARG2_REMAP_ROW_RAW_CNT_CORR:
        {
            *pData = Smbpbi.remappedRowsCntCor;
            break;
        }
        default:
            status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
    }

    return status;
}

/*!
 * Get row remapping state flags
 *
 * @param[in]   arg2      Requested page number
 * @param[out]  pData     Pointer to output Data
 *
 * @return      status    SMBPBI status
 */
LwU8
s_smbpbiGetRemapRowStateFlags
(
    LwU8  arg2,
    LwU32 *pData
)
{
    LwU8 status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    switch (arg2)
    {
        // Page0
        case 0:
        {
            *pData = FLD_SET_DRF_NUM(_MSGBOX_DATA_REMAP_ROW, _STATE_FLAGS,
                                        _PAGE0_FAILED_REMAPPING, Smbpbi.bRemappingFailed,
                                        *pData);
            *pData = FLD_SET_DRF_NUM(_MSGBOX_DATA_REMAP_ROW, _STATE_FLAGS,
                                        _PAGE0_PENDING_REMAPPING, Smbpbi.bRemappingPending,
                                        *pData);
            break;
        }
        default:
            status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
    }

    return status;
}

/*!
 * Get row remapping histogram
 *
 * @param[in]   arg2      Requested page number
 * @param[out]  pCmd      Pointer to output Cmd
 * @param[out]  pData     Pointer to output Data
 * @param[out]  pExtData  Pointer to output ExtData
 *
 * @return      status    SMBPBI status
 */
LwU8
s_smbpbiGetRemapRowHistogram
(
    LwU8  arg2,
    LwU32 *pCmd,
    LwU32 *pData,
    LwU32 *pExtData
)
{
    if (!(LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2, _REMAP_ROW_HISTOGRAM)))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
    }

    if (arg2 != 0)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_ARG2;
    }

    *pExtData = FLD_SET_DRF_NUM(_MSGBOX_EXT_DATA_REMAP_ROW, _HISTOGRAM, _MAX_AVAILABILITY,
                             Smbpbi.rowRemapHistogram.histogram[LW2080_CTRL_FB_HISTOGRAM_IDX_NO_REMAPPED_ROWS],
                             *pExtData);
    *pExtData = FLD_SET_DRF_NUM(_MSGBOX_EXT_DATA_REMAP_ROW, _HISTOGRAM, _HIGH_AVAILABILITY,
                             Smbpbi.rowRemapHistogram.histogram[LW2080_CTRL_FB_HISTOGRAM_IDX_SINGLE_REMAPPED_ROW],
                             *pExtData);
    *pData = FLD_SET_DRF_NUM(_MSGBOX_DATA_REMAP_ROW, _HISTOGRAM, _PARTIAL_AVAILABILITY,
                             Smbpbi.rowRemapHistogram.histogram[LW2080_CTRL_FB_HISTOGRAM_IDX_MIXED_REMAPPED_REMAINING_ROWS],
                             *pData);
    *pData = FLD_SET_DRF_NUM(_MSGBOX_DATA_REMAP_ROW, _HISTOGRAM, _LOW_AVAILABILITY,
                             Smbpbi.rowRemapHistogram.histogram[LW2080_CTRL_FB_HISTOGRAM_IDX_SINGLE_REMAINING_ROW],
                             *pData);
    *pCmd = FLD_SET_DRF_NUM(_MSGBOX_CMD_REMAP_ROW, _HISTOGRAM, _NONE_AVAILABILITY,
                             Smbpbi.rowRemapHistogram.histogram[LW2080_CTRL_FB_HISTOGRAM_IDX_MAX_REMAPPED_ROWS],
                             *pCmd);

    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}
