/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021  by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------ System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------ Application Includes --------------------------- */
#include "soe_objsmbpbi.h"
#include "config/g_smbpbi_private.h"

/* ------------------------ Static Functions  ------------------------------ */

static LwU8
s_readEccCount_LR10
(
    LwBool bUncErr,
    LwU64_ALIGN32 *pErrCount
)
{
    LwU8 status = LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;

    if (smbpbiIsSharedSurfaceAvailable())
    {
        LwU32 offset;
        FLCN_STATUS flcnStatus;

        offset = bUncErr ? SOE_SMBPBI_SHARED_OFFSET_INFOROM(ECC, uncorrectedTotal) :
                    SOE_SMBPBI_SHARED_OFFSET_INFOROM(ECC, correctedTotal);

        flcnStatus = smbpbiReadSharedSurface(pErrCount, offset, sizeof(LwU64_ALIGN32));
        if (flcnStatus != FLCN_OK)
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
        }
        else
        {
            status = LW_MSGBOX_CMD_STATUS_SUCCESS;
        }
    }
    else if (pInforomData != NULL)
    {
        *pErrCount = bUncErr ? (pInforomData->ECC.uncorrectedTotal) :
                        (pInforomData->ECC.correctedTotal);

        status = LW_MSGBOX_CMD_STATUS_SUCCESS;
    }

    return status;
}

/* ------------------------ Public Functions  ------------------------------ */

LwU8
smbpbiGetEccV6_LR10
(
    LwU8  arg1,
    LwU8  arg2,
    LwU32 *pData,
    LwU32 *pExtData
)
{
    FLCN_STATUS status;
    LwU64_ALIGN32 errCount;
    LwBool bUncErr;

    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 1, _ECC_V6))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
    }

    switch (arg1)
    {
        case LW_MSGBOX_CMD_ARG1_ECC_V6_ERROR_TYPE_CORRECTABLE_ERROR:
        {
            bUncErr = LW_FALSE;
            break;
        }
        case LW_MSGBOX_CMD_ARG1_ECC_V6_ERROR_TYPE_UNCORRECTABLE_ERROR:
        {
            bUncErr = LW_TRUE;
            break;
        }
        default:
            return LW_MSGBOX_CMD_STATUS_ERR_ARG1;
    }

    if (arg2 != 0)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_ARG2;
    }

    status = s_readEccCount_LR10(bUncErr, &errCount);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        return status;
    }

    *pData      = errCount.lo;
    *pExtData   = errCount.hi;

    return status;
}
