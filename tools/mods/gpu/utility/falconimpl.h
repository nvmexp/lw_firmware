/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"
#include "core/include/setget.h"
#include "falconucode.h"

class FalconImpl
{
public:
    FalconImpl();
    virtual ~FalconImpl() = default;

    virtual RC Initialize();
    virtual RC WaitForHalt(UINT32 timeoutMs);
    virtual RC Start(bool bWaitForHalt);
    virtual RC Start(bool bWaitForHalt, UINT32 delayUs);
    virtual RC ShutdownUCode();
    virtual RC Reset();
    virtual RC LoadBinary
    (
        const string& filename,
        FalconUCode::UCodeDescType descType
    );
    virtual RC LoadBinary
    (
        unsigned char const* pArray,
        UINT32 pArraySize,
        FalconUCode::UCodeDescType descType
    );
    virtual RC LoadBinary
    (
        const vector<UINT32>& binary,
        FalconUCode::UCodeDescType descType
    );

    virtual RC GetUCodeVersion(UINT32 ucodeId, UINT32 *pVersion);

    virtual RC ReadMailbox (UINT32 num, UINT32 *pVal);
    virtual RC WriteMailbox(UINT32 num, UINT32 val);

    virtual RC WriteIMem
    (
        const vector<UINT32>& binary,
        UINT32 offset,
        UINT32 secStart,
        UINT32 secEnd
    );

    virtual RC WriteDMem
    (
        const vector<UINT32>& binary,
        UINT32 offset
    );

    virtual RC WriteEMem
    (
        const vector<UINT32>& binary,
        UINT32 offset
    );

    virtual RC ReadDMem
    (
        UINT32 offset,
        UINT32 dwordCount,
        vector<UINT32> *pRetArray
    );

    virtual RC ReadIMem
    (
        UINT32 offset,
        UINT32 dwordCount,
        vector<UINT32> *pRetArray
    );

    virtual RC ReadEMem
    (
        UINT32 offset,
        UINT32 dwordCount,
        vector<UINT32> *pRetArray
    );

    virtual RC LoadProgram
    (
        const vector<UINT32>& imemBinary,
        const vector<UINT32>& dmemBinary,
        const FalconUCode::UCodeInfo &ucodeInfo
    );

    SETGET_PROP(TimeoutMs, UINT32);
    SETGET_PROP(DelayUs,   UINT32);

    Tee::Priority GetVerbosePriority();

protected:
    FLOAT64 AdjustTimeout(UINT32 timeout);

private:
    UINT32 m_TimeoutMs = 5000;
    UINT32 m_DelayUs   = 1000;
};
