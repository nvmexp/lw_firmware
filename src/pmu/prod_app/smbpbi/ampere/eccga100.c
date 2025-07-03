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
 * @file    eccga100.c
 * @brief   PMU HAL functions for GA100, handling SMBPBI queries for
 *          InfoROM ECC structures.
 */

/* ------------------------ System Includes -------------------------------- */
#include "lwuproc.h"
#include "pmusw.h"
#include "oob/smbpbi_shared.h"

/* ------------------------ Application Includes --------------------------- */
#include "task_therm.h"
#include "dbgprintf.h"

#include "config/g_smbpbi_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Function Prototypes  -------------------- */
/* ------------------------ Static Function Prototypes  -------------------- */
static LwU8 s_readEccCount_GA100(LwBool, LwBool, LwU64_ALIGN32 *);

/* ------------------------ Public Functions  ------------------------------ */

/*!
 * Handler for the LW_MSGBOX_CMD_OPCODE_GET_ECC_V6 command.
 * Reads the contents of ECC counters, as specified in ARG.
 *
 * @param[in]     cmd
 *     MSGBOX command, as received from the requestor.
 *
 * @param[in,out] pData
 *     Pointer to DATA dword supplied by requester.  Will populate this dword
 *     with the requested information.
 *
 * @param[in,out] pExtData
 *     Pointer to EXT_DATA dword supplied by requester.  Will populate this dword
 *     with the requested information.
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     Successfully serviced request.
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED
 *     ECC data not available
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG1
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *     Error in request arguments
 */
LwU8
smbpbiEccV6Query_GA100
(
    LwU32   cmd,
    LwU32  *pData,
    LwU32  *pExtData
)
{
    LwU8 status;
    LwBool bDramError, bUncError;
    LwU64_ALIGN32 errCount;

    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 1, _ECC_V6))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
    }

    switch (DRF_VAL(_MSGBOX, _CMD, _ARG2_ECC_V6_COUNTER_TYPE, cmd))
    {
        case LW_MSGBOX_CMD_ARG2_ECC_V6_COUNTER_TYPE_SRAM:
        {
            bDramError = LW_FALSE;
            break;
        }
        case LW_MSGBOX_CMD_ARG2_ECC_V6_COUNTER_TYPE_DRAM:
        {
            bDramError = LW_TRUE;
            break;
        }
        default:
        {
            return LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        }
    }

    switch (DRF_VAL(_MSGBOX, _CMD, _ARG1_ECC_V6_ERROR_TYPE, cmd))
    {
        case LW_MSGBOX_CMD_ARG1_ECC_V6_ERROR_TYPE_CORRECTABLE_ERROR:
        {
            bUncError = LW_FALSE;
            break;
        }
        case LW_MSGBOX_CMD_ARG1_ECC_V6_ERROR_TYPE_UNCORRECTABLE_ERROR:
        {
            bUncError = LW_TRUE;
            break;
        }
        default:
        {
            return LW_MSGBOX_CMD_STATUS_ERR_ARG1;
        }
    }

    status = s_readEccCount_GA100(bDramError, bUncError, &errCount);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        return status;
    }

    *pData    = errCount.lo;
    *pExtData = errCount.hi;

    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

/* ------------------------ Static Functions  ------------------------------ */

static LwU8
s_readEccCount_GA100
(
    LwBool bDramError,
    LwBool bUncError,
    LwU64_ALIGN32 *pCount
)
{
    LwU8 status = LW_MSGBOX_CMD_STATUS_ERR_MISC;

    if (bDramError)
    {
        LwU32 offset = bUncError ? LW_OFFSETOF(INFOROMHALINFO_FERMI,
                                ECC.object.v6.clientInfo.dramUncorrectedTotal) :
                            LW_OFFSETOF(INFOROMHALINFO_FERMI,
                                ECC.object.v6.clientInfo.dramCorrectedTotal);

        status = smbpbiHostMemRead_INFOROM_GF100(offset, sizeof(LwU64_ALIGN32), (LwU8 *)pCount);
    }
    else
    {
        LwU32 offset = bUncError ? LW_OFFSETOF(INFOROMHALINFO_FERMI,
                                ECC.object.v6.clientInfo.sramUncorrectedTotal) :
                            LW_OFFSETOF(INFOROMHALINFO_FERMI,
                                ECC.object.v6.clientInfo.sramCorrectedTotal);

        status = smbpbiHostMemRead_INFOROM_GF100(offset, sizeof(LwU64_ALIGN32), (LwU8 *)pCount);
    }

    return status;
}
