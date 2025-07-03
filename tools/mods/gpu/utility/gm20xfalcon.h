/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2016,2019-2021 by LWPU Corporation.  All rights reserved.  All
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
#include "gpu/include/gpusbdev.h"

class GM20xFalcon : public FalconImpl
{
public:
    enum class FalconType : UINT08
    {
        PMU,
        SEC,
        LWDEC,
        GSP
    };

    struct FalconInfo
    {
        ModsGpuRegAddress engineBase;
        ModsGpuRegAddress engineBase2;
        ModsGpuRegAddress scpBase;
    };

    struct PollFalconArgs
    {
        GpuSubdevice *pSubdev;
        UINT32 engineBase;
    };

    struct PollKfuseSfkArgs
    {
        GpuSubdevice *pSubdev;
        UINT32 kfuseRegAddr;
        UINT32 sfkRegAddr;
    };

    GM20xFalcon(GpuSubdevice* pSubdev, FalconType falconType);
    virtual ~GM20xFalcon() = default;

    GET_PROP(EngineBase, UINT32);
    GET_PROP(EngineBase2, UINT32);

    virtual RC Initialize() override;

    virtual RC WaitForHalt(UINT32 timeoutMs) override;
    virtual RC Start(bool bWaitForHalt) override
        { return FalconImpl::Start(bWaitForHalt); }
    virtual RC Start(bool bWaitForHalt, UINT32 delayUs) override;
    virtual RC ShutdownUCode() override
        { return RC::OK; }
    virtual RC Reset() override;

    virtual RC GetUCodeVersion(UINT32 ucodeId, UINT32 *pVersion) override;

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

    virtual RC ReadEMem
    (
        UINT32 offset,
        UINT32 dwordCount,
        vector<UINT32> *pRetArray
    ) override;

    virtual RC WriteEMem
    (
        const vector<UINT32>& binary,
        UINT32 offset
    ) override;

    virtual RC LoadProgram
    (
        const vector<UINT32>& imemBinary,
        const vector<UINT32>& dmemBinary,
        const FalconUCode::UCodeInfo &ucodeInfo
    ) override;

protected:
    virtual RC IsInitialized();
    virtual RC ApplyEngineReset();
    virtual RC ResetPmu();
    virtual RC ResetSec();
    virtual RC TriggerAndPollScrub();
    virtual RC WaitForKfuseSfkLoad(UINT32 timeoutMs);

    static bool PollScrubbingDone(void * pArgs);
    static bool PollDmaDone(void * pArgs);
    static bool PollEngineHalt(void *pArgs);
    static bool PollKfuseSfkLoaded(void *pArgs);

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

    RC Falcon2RegRd
    (
        ModsGpuRegAddress reg,
        UINT32 *pReadBack
    );
    RC Falcon2RegRd
    (
        ModsGpuRegAddress reg,
        UINT32 idx,
        UINT32 *pReadBack
    );
    RC Falcon2RegWr
    (
        ModsGpuRegAddress reg,
        UINT32 val
    );
    RC Falcon2RegWr
    (
        ModsGpuRegAddress reg,
        UINT32 idx,
        UINT32 val
    );

    static RC FalconRegRdImpl
    (
        GpuSubdevice *pSubdev,
        UINT32 engineBase,
        ModsGpuRegAddress reg,
        UINT32 *pReadBack
    );
    static RC FalconRegRdIdxImpl
    (
        GpuSubdevice *pSubdev,
        UINT32 engineBase,
        ModsGpuRegAddress reg,
        UINT32 idx,
        UINT32 *pReadBack
    );
    static RC FalconRegWrImpl
    (
        GpuSubdevice *pSubdev,
        UINT32 engineBase,
        ModsGpuRegAddress reg,
        UINT32 val
    );
    static RC FalconRegWrIdxImpl
    (
        GpuSubdevice *pSubdev,
        UINT32 engineBase,
        ModsGpuRegAddress reg,
        UINT32 idx,
        UINT32 val
    );

private:
    static constexpr UINT32 ILWALID_ENGINE_BASE = 0xFFFFFFFF;

    GpuSubdevice* m_pSubdev       = nullptr;
    FalconType    m_FalconType    = FalconType::PMU;
    UINT32        m_EngineBase    = ILWALID_ENGINE_BASE;
    UINT32        m_EngineBase2   = ILWALID_ENGINE_BASE;
    UINT32        m_ScpBase       = ILWALID_ENGINE_BASE;
    bool          m_IsInitialized = false;
};
