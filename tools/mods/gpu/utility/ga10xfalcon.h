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

#pragma once

#include "gm20xfalcon.h"

class GA10xFalcon : public GM20xFalcon
{
public:
    GA10xFalcon(GpuSubdevice* pSubdev, FalconType falconType);
    virtual ~GA10xFalcon() = default;

    virtual RC Initialize() override;
    virtual RC Reset() override;

    virtual RC GetUCodeVersion(UINT32 ucodeId, UINT32 *pVersion) override;

    virtual RC LoadProgram
    (
        const vector<UINT32>& imemBinary,
        const vector<UINT32>& dmemBinary,
        const FalconUCode::UCodeInfo &ucodeInfo
    ) override;

protected:
    RC ResetPmu() override;
    RC ResetSec() override;

    RC SetFalconCore();
    RC SetEncyptedBit(bool bIsEncrypted);

    static bool PollBcrCtrlValid(void *pArgs);

private:
    GpuSubdevice* m_pSubdev       = nullptr;
    bool          m_IsInitialized = false;

    RC InitSecSpr(bool bIsEncrypted);
};
