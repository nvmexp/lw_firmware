/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "ucodefuzzersub.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "lwRmApi.h"
#include "core/include/platform.h"

UcodeFuzzerSub::UcodeFuzzerSub(GpuSubdevice *pSubdevice)
  : m_pSubdevice(pSubdevice)
{
}

RC UcodeFuzzerSub::RtosCommand
(
    LwU32 ucode,
    LwU32 timeoutMs,
    const std::vector<LwU8>& cmd,
    const std::vector<LwU8>& payload
)
{
    RC                                       rc;
    LwRmPtr                                  pLwRm;
    LW2080_CTRL_UCODE_FUZZER_RTOS_CMD_PARAMS ctrlCmd;

    if (cmd.size() > sizeof(ctrlCmd.cmd) ||
        payload.size() > sizeof(ctrlCmd.payload))
    {
        return RC::LWRM_ILWALID_ARGUMENT;
    }

    memset(&ctrlCmd, 0x41, sizeof(ctrlCmd));
    std::copy(cmd.begin(),     cmd.end(),     ctrlCmd.cmd);
    std::copy(payload.begin(), payload.end(), ctrlCmd.payload);

    ctrlCmd.ucode       = ucode;
    ctrlCmd.queueId     = 0; // Always safe, maybe make configurable?
    ctrlCmd.timeoutMs   = timeoutMs;
    ctrlCmd.sizeCmd     = cmd.size();
    ctrlCmd.sizePayload = payload.size();

    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_pSubdevice),
                        LW2080_CTRL_CMD_UCODE_FUZZER_RTOS_CMD,
                        &ctrlCmd,
                        sizeof(ctrlCmd));
    return rc;
}

RC UcodeFuzzerSub::SanitizerCovGetControl
(
    LwU32   ucode,
    LwU32&  usedOut,
    LwU32&  missedOut,
    LwBool& bEnabledOut
)
{
    RC       rc = RC::LWRM_NOT_SUPPORTED;

#ifdef LW2080_CTRL_CMD_UCODE_FUZZER_SANITIZER_COV_GET_CONTROL
    LwRmPtr  pLwRm;
    LW2080_CTRL_UCODE_FUZZER_SANITIZER_COV_CONTROL_PARAMS params;

    params.ucode = ucode;

    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_pSubdevice),
                        LW2080_CTRL_CMD_UCODE_FUZZER_SANITIZER_COV_GET_CONTROL,
                        &params,
                        sizeof(params));

    if (rc == RC::OK)
    {
        usedOut = params.used;
        missedOut = params.missed;
        bEnabledOut = params.bEnabled;
    }
#endif  // (LW2080_CTRL_CMD_UCODE_FUZZER_SANITIZER_COV_GET_CONTROL)

    return rc;
}

RC UcodeFuzzerSub::SanitizerCovSetControl
(
    LwU32  ucode,
    LwU32  used,
    LwU32  missed,
    LwBool bEnabled
)
{
    RC       rc = RC::LWRM_NOT_SUPPORTED;

#ifdef LW2080_CTRL_CMD_UCODE_FUZZER_SANITIZER_COV_SET_CONTROL
    LwRmPtr  pLwRm;
    LW2080_CTRL_UCODE_FUZZER_SANITIZER_COV_CONTROL_PARAMS params;

    params.ucode = ucode;
    params.used = used;
    params.missed = missed;
    params.bEnabled = bEnabled;

    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_pSubdevice),
                        LW2080_CTRL_CMD_UCODE_FUZZER_SANITIZER_COV_SET_CONTROL,
                        &params,
                        sizeof(params));
#endif  // (LW2080_CTRL_CMD_UCODE_FUZZER_SANITIZER_COV_SET_CONTROL)

    return rc;
}

RC UcodeFuzzerSub::SanitizerCovDataGet
(
    LwU32 ucode,
    std::vector<LwU64>& data
)
{
    RC          rc = RC::LWRM_NOT_SUPPORTED;

#ifdef LW2080_CTRL_CMD_UCODE_FUZZER_SANITIZER_COV_DATA_GET
    LwRmPtr     pLwRm;
    const LwU32 maxElementsAtOnce = 512;
    LwU64       buf[maxElementsAtOnce];

    LW2080_CTRL_UCODE_FUZZER_SANITIZER_COV_DATA_GET_PARAMS params;

    params.ucode = ucode;
    params.pData = buf;
    params.numElements = maxElementsAtOnce;  // request as much as we can (may get less)
    params.bDone = LW_FALSE;

    while (!params.bDone)
    {
        rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_pSubdevice),
                            LW2080_CTRL_CMD_UCODE_FUZZER_SANITIZER_COV_DATA_GET,
                            &params,
                            sizeof(params));
        if (rc != RC::OK)
        {
            break;
        }

        data.insert(data.end(), params.pData, params.pData + params.numElements);
    }
#endif  // (LW2080_CTRL_CMD_UCODE_FUZZER_SANITIZER_COV_DATA_GET)

    return rc;
}