/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "falconimpl.h"

#include "core/include/fileholder.h"
#include "core/include/utility.h"
#include "core/include/tasker.h"

//-----------------------------------------------------------------------------
FalconImpl::FalconImpl()
{
}

RC FalconImpl::Initialize()
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC FalconImpl::LoadBinary
(
    const string& filename,
    FalconUCode::UCodeDescType descType
)
{
    RC rc;
    CHECK_RC(Initialize());

    vector<UINT32> binary;
    CHECK_RC(FalconUCode::ReadBinary(filename, &binary));

    return LoadBinary(binary, descType);
}

RC FalconImpl::LoadBinary
(
    unsigned char const* pArray,
    UINT32 pArraySize,
    FalconUCode::UCodeDescType descType
)
{
    vector<UINT32> binary;
    UINT32 const* pStart = reinterpret_cast<UINT32 const*>(pArray);
    size_t binarySize = pArraySize / sizeof(UINT32);
    binary.assign(pStart, pStart + binarySize);

    return LoadBinary(binary, descType);
}

RC FalconImpl::LoadBinary
(
    const vector<UINT32>& binary,
    FalconUCode::UCodeDescType descType
)
{
    RC rc;
    vector<UINT32> imemBinary, dmemBinary;
    FalconUCode::UCodeInfo ucodeInfo;

    CHECK_RC(Initialize());
    CHECK_RC(FalconUCode::ParseBinary(binary,
                                      &imemBinary,
                                      &dmemBinary,
                                      &ucodeInfo,
                                      descType));
    if (ucodeInfo.sigSizeBytes != 0)
    {
        UINT32 version;
        CHECK_RC(GetUCodeVersion(ucodeInfo.ucodeId, &version));
        CHECK_RC(FalconUCode::AddSignature(binary,
                                           ucodeInfo,
                                           version,
                                           &dmemBinary));
    }

    CHECK_RC(LoadProgram(imemBinary, dmemBinary, ucodeInfo));
    return rc;
}

RC FalconImpl::WaitForHalt(UINT32 timeoutMs)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC FalconImpl::Start(bool bWaitForHalt)
{
    return Start(bWaitForHalt, m_DelayUs);
}

RC FalconImpl::Start(bool bWaitForHalt, UINT32 delayUs)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC FalconImpl::ShutdownUCode()
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC FalconImpl::Reset()
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC FalconImpl::GetUCodeVersion(UINT32 ucodeId, UINT32 *pVersion)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC FalconImpl::ReadMailbox(UINT32 num, UINT32 *pVal)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC FalconImpl::WriteMailbox(UINT32 num, UINT32 val)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC FalconImpl::LoadProgram
(
    const vector<UINT32>& imemBinary,
    const vector<UINT32>& dmemBinary,
    const FalconUCode::UCodeInfo& ucodeInfo
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC FalconImpl::WriteIMem
(
    const vector<UINT32>& binary,
    UINT32 offset,
    UINT32 secStart,
    UINT32 secEnd
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC FalconImpl::WriteDMem
(
    const vector<UINT32>& binary,
    UINT32 offset
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC FalconImpl::WriteEMem
(
    const vector<UINT32>& binary,
    UINT32 offset
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC FalconImpl::ReadDMem
(
    UINT32 offset,
    UINT32 dwordCount,
    vector<UINT32> *pRetArray
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC FalconImpl::ReadIMem
(
    UINT32 offset,
    UINT32 dwordCount,
    vector<UINT32> *pRetArray
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC FalconImpl::ReadEMem
(
    UINT32 offset,
    UINT32 dwordCount,
    vector<UINT32>* pRetArray
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

FLOAT64 FalconImpl::AdjustTimeout(UINT32 timeout)
{
    // Scaling by timeout_ms
    return (timeout * Tasker::GetDefaultTimeoutMs()) / 1000;
}

Tee::Priority FalconImpl::GetVerbosePriority()
{
     if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
     {
        return Tee::PriNone;
     }
     return Tee::PriLow;
}
