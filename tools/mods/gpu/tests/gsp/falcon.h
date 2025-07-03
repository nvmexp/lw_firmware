/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "gpu/include/gpusbdev.h"

class GpuSubdevice;

enum ReadWrite
{
    DMEMRD,
    DMEMWR
};

class Falcon
{
public:
    Falcon(GpuSubdevice *dev);
    virtual ~Falcon() = default;

    UINT32 GetMailbox0();
    UINT32 GetMailbox1();
    void SetMailbox0(UINT32 val);
    void SetMailbox1(UINT32 val);
    UINT32 UCodeRead32();
    void UCodeWrite32(UINT32 data);
    void UCodeBarrier();
    void HandleRegRWRequest();

    UINT32 ImemAccess
    (
        UINT32 addr,
        UINT32 *codeArr,
        UINT32 codeSize,
        ReadWrite rw,
        UINT16 tag,
        bool isSelwre
    );
    UINT32 DmemAccess
    (
        UINT32 addr,
        UINT32 *dataArr,
        UINT32 dataSize,
        ReadWrite rw,
        UINT16 tag
    );

    RC GetTestRC();
    RC VerifyDmlvlExceptionAccess();

    void UCodePatchSignature();
    void ClearInterruptOnHalt();
    void Bootstrap(UINT32 bootvector);
    void Resume();
    RC Reset();
    RC WaitForHalt();
    RC WaitForStop();
    RC WaitForScrub();

    virtual UINT32 GetEngineBase() = 0;
    virtual UINT32 *GetUCodeData() const = 0;
    virtual UINT32 *GetUCodeHeader() const = 0;
    virtual UINT32 GetPatchLocation() const = 0;
    virtual UINT32 GetPatchSignature() const = 0;
    virtual UINT32 *GetSignature(bool debug = true) const = 0;
    virtual UINT32 SetPMC(UINT32 pmc, UINT32 state) = 0;
    virtual void EngineReset() = 0;

    UINT32 *GetCode() const;
    UINT32 *GetData() const;
    UINT32 *GetAppCode(UINT32 appId) const;
    UINT32 GetAppCodeSize(UINT32 appId) const;
    UINT32 *GetAppData(UINT32 appId) const;
    UINT32 GetAppDataSize(UINT32 appId) const;

    UINT32 GetCodeOffset() const
    {
        return GetUCodeHeader()[0];
    }

    UINT32 GetCodeSize() const
    {
        return GetUCodeHeader()[1];
    }

    UINT32 GetDataOffset(void) const
    {
        return GetUCodeHeader()[2];
    }

    UINT32 GetDataSize(void) const
    {
        return GetUCodeHeader()[3];
    }

    UINT32 GetNumApps(void) const
    {
        return GetUCodeHeader()[4];
    }

protected:
    GpuSubdevice *m_pSubdev;

private:
    const UINT32 FALC_RESET_TIMEOUT_US = 30 * 1000 * 1000;  // 30s

    RC Poll(bool (Falcon::*PollFunction)(), UINT32 timeoutUs);
    bool IsScrubbingDone();
    bool IsNotRunning();
    bool IsHalted();
    bool IsStopped();
};
