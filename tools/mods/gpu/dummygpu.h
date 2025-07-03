/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "stubgpu.h"
#include "gpu/include/device_interfaces.h"

class DummyGpuSubdevice final : public StubGpuSubdevice
                              , public DeviceInterfaces<Device::SOC>::interfaces
{
public:
    DummyGpuSubdevice(const char*, Gpu::LwDeviceId, const PciDevInfo*);

    static bool SetBoot0Strap(UINT32 boot0Strap) { return false; }

    RC     PreVBIOSSetup() override               { return RC::OK; }
    RC     PostVBIOSSetup() override              { return RC::OK; }

    RC     ValidatePreInitRegisters() override    { return OK; }

    virtual void   SaveFbPageState()              {}
    virtual void   RestoreFbPageState()           {}
    virtual void   *EnsureFbRegionMapped(UINT32 FbOffset, UINT32 Size) { return 0; }

    UINT32 GetMaxTpcCount() const override        { return 0; }

    RC     SetDebugState(string Name, UINT32 Value) override { return RC::UNSUPPORTED_FUNCTION; }

    void   PostRmShutdown() override              {}

    UINT64 FramebufferBarSize() const override    { return 0; }

    UINT32 RopCount() const override              { return 0; }
    UINT32 ShaderCount() const override           { return 0; }
    UINT32 VpeCount() const override              { return 0; }
    UINT32 GetTextureAlignment() override         { return 32; }
    RC     SetCompressionMap(Surface2D* pSurf, const vector<UINT32>& ComprMap) override
                                                 { return RC::UNSUPPORTED_FUNCTION; }

    void     PrintMonitorInfo() override                {}
    void     SetupBugs() override                       {}
    unsigned GetNumIntrTrees() const override           { return 0; }
    string   GetRefManualFile() const override          { return ""; }

    void EnableSMC() override           {};
    void DisableSMC() override          {};
    bool IsSMCEnabled() const override  { return false; };
    bool IsPgraphRegister(UINT32 offset) const override { return false; }

private:
    TestDevice* GetDevice() override { return this; }
    const TestDevice* GetDevice() const override { return this; }
};
