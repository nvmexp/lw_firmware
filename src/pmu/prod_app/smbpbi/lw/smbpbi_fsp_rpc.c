/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file    smbpbi_fsp_rpc.c
 * @brief   This file implements all the RPCs which the SMBPBI server uses in
 *          order to communicate with FSP. For more information look at:
 *          https://confluence.lwpu.com/display/CSSRM/FSP+SMBPBI+RPC%3A+Ilwestigations
 */
/* ------------------------ System Includes -------------------------------- */
#include "lwtypes.h"
#include "fsp_rpc.h"
#include "dev_fsp_addendum.h"

/* ------------------------ Application Includes --------------------------- */
#include "pmu_objsmbpbi.h"
#include "fsp/fsp_smbpbi_rpc.h"

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- External Definitions -------------------------- */
/* ------------------------- Static Variables ------------------------------ */
/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Private Functions ----------------------------- */
/* ------------------------- Public Functions ------------------------------ */

FLCN_STATUS
smbpbiFspRpcSendUcodeLoad(void)
{
    int i;
    LW_STATUS status;
    FSP_SMBPBI_RPC_UCODE_LOAD_PARAMS ucodeLoadParams = {{ 0 }};

    ucodeLoadParams.header.data = DRF_DEF(_FSP, _SMBPBI_RPC, _TYPE, _UCODE_LOAD);
    for (i = 0; i < LW_MSGBOX_DATA_CAP_COUNT; i++)
    {
        ucodeLoadParams.capabilityDwords[i] = Smbpbi.config.cap[i];
    }

    // timeout is 0 because this is not a synchronous request
    status = fspRpcMessageSend((LwU32 *)&ucodeLoadParams,
                               sizeof(ucodeLoadParams) / FSP_RPC_BYTES_PER_DWORD,
                               LWDM_TYPE_SMBPBI, 0 /* timeoutUs */,
                               LW_FALSE);

    return ((status == LW_OK) ? FLCN_OK : FLCN_ERROR);
}

FLCN_STATUS
smbpbiFspRpcSendUcodeUnload(void)
{
    LW_STATUS status;
    FSP_SMBPBI_RPC_UCODE_UNLOAD_PARAMS ucodeUnloadParams = {{ 0 }};

    ucodeUnloadParams.header.data = DRF_DEF(_FSP, _SMBPBI_RPC, _TYPE, _UCODE_UNLOAD);

    // timeout is 0 because this is not a synchronous request
    status = fspRpcMessageSend((LwU32 *)&ucodeUnloadParams,
                               sizeof(ucodeUnloadParams) / FSP_RPC_BYTES_PER_DWORD,
                               LWDM_TYPE_SMBPBI, 0 /* timeoutUs */,
                               LW_FALSE);

    return ((status == LW_OK) ? FLCN_OK : FLCN_ERROR);
}
