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

#include <vector>
#include <string>

#include "falconimpl.h"
#include "gpu/include/testdevice.h"

class LwSwitchFalcon : public FalconImpl
{
public:
    enum class FalconType : UINT08
    {
        SOE
    };

    struct PollFalconArgs
    {
        TestDevice *pDev;
        UINT32 engineBase;
    };

    LwSwitchFalcon(TestDevice* pDev, FalconType falconType);
    virtual ~LwSwitchFalcon() = default;

    virtual RC Initialize() override;

    virtual RC WaitForHalt(UINT32 timeoutMs) override;
    virtual RC Start(bool bWaitForHalt) override
        { return FalconImpl::Start(bWaitForHalt); }
    virtual RC Start(bool bWaitForHalt, UINT32 delayUs) override;
    virtual RC ShutdownUCode() override;
    virtual RC Reset() override;

    virtual RC ReadMailbox (UINT32 num, UINT32 *pVal) override;
    virtual RC WriteMailbox(UINT32 num, UINT32 val) override;

    virtual RC WriteIMem
    (
        const vector<UINT32>& binary,
        UINT32 offset,
        UINT32 secStart,
        UINT32 secEnd
    ) override;

    virtual RC WriteDMem
    (
        const vector<UINT32>& binary,
        UINT32 offset
    ) override;

    virtual RC ReadDMem
    (
        UINT32 offset,
        UINT32 dwordCount,
        vector<UINT32> *pRetArray
    ) override;

    virtual RC ReadIMem
    (
        UINT32 offset,
        UINT32 dwordCount,
        vector<UINT32> *pRetArray
    ) override;

    virtual RC LoadProgram
    (
        const vector<UINT32>& imemBinary,
        const vector<UINT32>& dmemBinary,
        const FalconUCode::UCodeInfo& ucodeInfo
    ) override;

protected:
    static bool PollScrubbingDone(void * pArgs);
    static bool PollDmaDone(void * pArgs);
    static bool PollEngineHalt(void *pArgs);

    RC FalconRegRd
    (
        ModsGpuRegAddress reg,
        UINT32 *pReadBack
    );
    RC FalconRegRd
    (
        ModsGpuRegAddress reg,
        UINT32 idx,
        UINT32 *pReadBack
    );
    RC FalconRegWr
    (
        ModsGpuRegAddress reg,
        UINT32 val
    );
    RC FalconRegWr
    (
        ModsGpuRegAddress reg,
        UINT32 idx,
        UINT32 val
    );

    static RC FalconRegRdImpl
    (
        TestDevice *pDev,
        UINT32 engineBase,
        ModsGpuRegAddress reg,
        UINT32 *pReadBack
    );
    static RC FalconRegRdIdxImpl
    (
        TestDevice *pDev,
        UINT32 engineBase,
        ModsGpuRegAddress reg,
        UINT32 idx,
        UINT32 *pReadBack
    );
    static RC FalconRegWrImpl
    (
        TestDevice *pDev,
        UINT32 engineBase,
        ModsGpuRegAddress reg,
        UINT32 val
    );
    static RC FalconRegWrIdxImpl
    (
        TestDevice *pDev,
        UINT32 engineBase,
        ModsGpuRegAddress reg,
        UINT32 idx,
        UINT32 val
    );

private:
    TestDevice   *m_pDev          = nullptr;
    FalconType    m_FalconType    = FalconType::SOE;
    UINT32        m_EngineBase    = 0xFFFFFFFF;
    bool          m_IsInitialized = false;
};
