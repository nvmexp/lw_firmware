/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "stubgpu.h"
#include "gpu/include/device_interfaces.h"

class ArgReader;
class ArgDatabase;
class GpuPerfmon;

class NoGpuSubdevice final : public StubGpuSubdevice
                           , public DeviceInterfaces<Device::Txxx>::interfaces
{
public:
    NoGpuSubdevice(const char*, Gpu::LwDeviceId, const PciDevInfo*);
    virtual ~NoGpuSubdevice();
    RC Initialize() override;
    RC Shutdown() override;

    RC VirtualGpcToHwGpc(UINT32 VirtualGpcNum, UINT32* pHwGpcNum) const override;
    RC HwGpcToVirtualGpc(UINT32 HwGpcNum, UINT32* pVirtualGpcNum) const override;
    RC VirtualTpcToHwTpc(UINT32 HwGpcNum, UINT32 VirtualTpcNum,
                                 UINT32* pHwTpcNum) const override;
    RC HwTpcToVirtualTpc(UINT32 HwGpcNum, UINT32 HwTpcNum,
                                 UINT32* pVirtualTpcNum) const override;
    UINT32 RopCount() const override;
    UINT32 ShaderCount() const override;
    UINT32 VpeCount() const override;
    UINT32 GetSysPartitionCount() override;
    UINT32 GetSpeedoCount() override;
    UINT32 GetMaxGpcCount() const override;
    UINT32 GetTpcCount() const override;
    UINT32 GetTpcCountOnGpc(UINT32 VirtualGpcNum) const override;
    UINT32 GetMaxShaderCount() const override;
    UINT32 GetMaxSmPerTpcCount() const override;
    UINT32 GetMaxTpcCount() const override;
    bool   GetFbMs01Mode() override;
    RC     GetEccEnabled(bool *pbSupported, UINT32 *pEnabledMask) const override;
    RC     GetEdcEnabled(bool* pbSupported, UINT32* pEnabledMask) override;

    UINT32 GetPLLCfg(PLLType Pll) const override;
    void   SetPLLCfg(PLLType Pll, UINT32 cfg) override;
    void   SetPLLLocked(PLLType Pll) override;
    bool   IsPLLLocked(PLLType Pll) const override;
    bool   IsPLLEnabled(PLLType Pll) const override;
    void   GetPLLList(vector<PLLType>* pPllList) const override;

    string ChipIdCooked() override;

    string DeviceName() override;
    RC     GetChipRevision(UINT32* pRev) override;
    UINT32 BoardId() override;

    RC PreVBIOSSetup() override;
    RC PostVBIOSSetup() override;
    RC ValidatePreInitRegisters() override;

    bool GetPteKindFromName(const char* Name, UINT32* PteKind) override;
    const char* GetPteKindName(UINT32 PteKind) override;

    void PrintMonitorInfo() override;

    UINT32 GetEngineStatus(Engine EngineName) override;

    UINT64 FramebufferBarSize() const override;

    UINT32 MemoryStrap() override;

    RC GetSOCDeviceTypes(vector<UINT32>* pDeviceTypes, SOCDevFilter filter) override;

    bool GpioIsOutput(UINT32 WhichGpio) override;
    void GpioSetOutput(UINT32 WhichGpio, bool Level) override;
    bool GpioGetOutput(UINT32 WhichGpio) override;
    bool GpioGetInput(UINT32 WhichGpio) override;

    RC SetGpioDirection(UINT32 GpioNum, GpioDirection Direction) override;

    UINT32 GetZlwllPixelsCovered() const override;
    UINT32 GetZlwllNumZFar() const override;
    UINT32 GetZlwllNumZNear() const override;
    RC SetZlwllSize(UINT32 zlwllRegion,
                            UINT32 displayWidth,
                            UINT32 displayHeight) override;

    RC SetDebugState(string Name, UINT32 Value) override;
    UINT32 GetTextureAlignment() override;
    RefManual *GetRefManual() const override;

    RC ValidateVbios
    (
        bool         bIgnoreFailure,
        VbiosStatus *pStatus,
        bool        *pbIgnored
    ) override;

    RC IlwalidateL2Cache(UINT32 flags) override;

    int EscapeWrite(const char* Path, UINT32 Index, UINT32 Size, UINT32 Value) override;
    int EscapeRead(const char* Path, UINT32 Index, UINT32 Size, UINT32* Value) override;
    int EscapeWriteBuffer(const char* Path, UINT32 Index, size_t Size, const void* Buf) override;
    int EscapeReadBuffer(const char* Path, UINT32 Index, size_t Size, void* Buf) const override;

    RC FbFlush(FLOAT64 TimeOutMs) override;

    void SetupBugs() override;
    void SetupFeatures() override;

    RC SetCompressionMap(Surface2D* pSurf, const vector<UINT32>& ComprMap) override;
    bool Is32bitChannel() override;

    void GetMmeToMem2MemRegInfo(bool bEnable, UINT32* pOffset,
                                        UINT32* pMask, UINT32* pData) override;
    UINT32 GetMmeInstructionRamSize() override;
    UINT32 GetMmeStartAddrRamSize() override;
    UINT32 GetMmeShadowRamSize() override;
    UINT32 GetMmeVersion() override;
    RC MmeShadowRamWrite(const MmeMethod Method, UINT32 Data) override;
    RC MmeShadowRamRead(const MmeMethod Method, UINT32* pData) override;

    bool IsQuadroEnabled() override;
    bool IsMsdecEnabled() override;
    bool IsEccFuseEnabled() override;

    RC SetFbMinimum(UINT32 fbMinMB) override;

    void PostRmShutdown() override;
    void EnableSMC() override           {};
    void DisableSMC() override          {};
    bool IsSMCEnabled() const override  { return false; };
    bool IsPgraphRegister(UINT32 offset) const override { return false; };

protected:
    bool RegPowerGated(UINT32 Offset) const override;
    bool RegPrivProtected(UINT32 Offset) const override;

    unsigned GetNumIntrTrees() const override;

    string GetRefManualFile() const override;

    GpuPerfmon* CreatePerfmon(void) override;

private:
    TestDevice* GetDevice() override { return this; }
    const TestDevice* GetDevice() const override { return this; }
};
