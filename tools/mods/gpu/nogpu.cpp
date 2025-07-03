/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2011-2020 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#include "nogpu.h"
#include "cheetah/include/tegchip.h"
#include "core/include/platform.h"
#include "cheetah/include/sysutil.h"

NoGpuSubdevice::NoGpuSubdevice
(
    const char* name,
    Gpu::LwDeviceId deviceId,
    const PciDevInfo* pPciDevInfo
)
: StubGpuSubdevice(name, deviceId, pPciDevInfo)
{
}

/* virtual */ NoGpuSubdevice::~NoGpuSubdevice()
{
}

/* virtual */ RC NoGpuSubdevice::Initialize()
{
    return GpuSubdevice::Initialize();
}

/* virtual */ RC NoGpuSubdevice::Shutdown()
{
    return GpuSubdevice::Shutdown();
}

/* virtual */ RC NoGpuSubdevice::VirtualGpcToHwGpc(UINT32 VirtualGpcNum, UINT32* pHwGpcNum) const
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC NoGpuSubdevice::HwGpcToVirtualGpc(UINT32 HwGpcNum, UINT32* pVirtualGpcNum) const
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC NoGpuSubdevice::VirtualTpcToHwTpc
(
    UINT32 HwGpcNum,
    UINT32 VirtualTpcNum,
    UINT32 *pHwTpcNum
) const
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC NoGpuSubdevice::HwTpcToVirtualTpc
(
    UINT32 HwGpcNum,
    UINT32 HwTpcNum,
    UINT32 *pVirtualTpcNum
) const
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ UINT32 NoGpuSubdevice::RopCount() const
{
    return 0;
}

/* virtual */ UINT32 NoGpuSubdevice::ShaderCount() const
{
    return 0;
}

/* virtual */ UINT32 NoGpuSubdevice::VpeCount() const
{
    return 0;
}

/* virtual */ UINT32 NoGpuSubdevice::GetSysPartitionCount()
{
    return 0;
}

/* virtual */ UINT32 NoGpuSubdevice::GetSpeedoCount()
{
    return 0;
}

/* virtual */ UINT32 NoGpuSubdevice::GetMaxGpcCount() const
{
    return 0;
}

/* virtual */ UINT32 NoGpuSubdevice::GetTpcCount() const
{
    return 0;
}

/* virtual */ UINT32 NoGpuSubdevice::GetTpcCountOnGpc(UINT32 VirtualGpcNum) const
{
    return 0;
}

/* virtual */ UINT32 NoGpuSubdevice::GetMaxShaderCount() const
{
    return 0;
}

/* virtual */ UINT32 NoGpuSubdevice::GetMaxSmPerTpcCount() const
{
    return 0;
}

/* virtual */ UINT32 NoGpuSubdevice::GetMaxTpcCount() const
{
    return 0;
}

/* virtual */ bool NoGpuSubdevice::GetFbMs01Mode()
{
    return false;
}

/* virtual */ RC NoGpuSubdevice::GetEccEnabled(bool *pbSupported, UINT32 *pEnabledMask) const
{
    *pbSupported  = false;
    *pEnabledMask = 0U;
    return OK;
}

/* virtual */ RC NoGpuSubdevice::GetEdcEnabled(bool* pbSupported, UINT32* pEnabledMask)
{
    MASSERT(pbSupported);
    *pbSupported = false;
    return OK;
}

/* virtual */ UINT32 NoGpuSubdevice::GetPLLCfg(PLLType Pll) const
{
    return 0;
}

/* virtual */ void NoGpuSubdevice::SetPLLCfg(PLLType Pll, UINT32 cfg)
{
}

/* virtual */ void NoGpuSubdevice::SetPLLLocked(PLLType Pll)
{
}

/* virtual */ bool NoGpuSubdevice::IsPLLLocked(PLLType Pll) const
{
    return false;
}

/* virtual */ bool NoGpuSubdevice::IsPLLEnabled(PLLType Pll) const
{
    return false;
}

/* virtual */ void NoGpuSubdevice::GetPLLList(vector<PLLType>* pPllList) const
{
    MASSERT(pPllList);
    pPllList->clear();
}

/* virtual */ string NoGpuSubdevice::ChipIdCooked()
{
    return "";
}

/* virtual */ string NoGpuSubdevice::DeviceName()
{
    string name;
    CheetAh::GetChipName(&name);
    return name;
}

/* virtual */ RC NoGpuSubdevice::GetChipRevision(UINT32* pRev)
{
    RC rc;
    UINT32 ver;
    UINT32 major=0, minor=0;
    CHECK_RC(CheetAh::GetChipVersion(&ver, &major, &minor));
    *pRev = (major << 16) | minor;
    return OK;
}

/* virtual */ UINT32 NoGpuSubdevice::BoardId()
{
    return 0;
}

/* virtual */ RC NoGpuSubdevice::PreVBIOSSetup()
{
    return RC::OK;
}

/* virtual */ RC NoGpuSubdevice::PostVBIOSSetup()
{
    return RC::OK;
}

/* virtual */ RC NoGpuSubdevice::ValidatePreInitRegisters()
{
    return OK;
}

/* virtual */ bool NoGpuSubdevice::GetPteKindFromName(const char* Name, UINT32* PteKind)
{
    return false;
}

/* virtual */ const char* NoGpuSubdevice::GetPteKindName(UINT32 PteKind)
{
    return "";
}

/* virtual */ void NoGpuSubdevice::PrintMonitorInfo()
{
}

/* virtual */ UINT32 NoGpuSubdevice::GetEngineStatus(Engine EngineName)
{
    return 0;
}

/* virtual */ UINT64 NoGpuSubdevice::FramebufferBarSize() const
{
    return 0;
}

/* virtual */ UINT32 NoGpuSubdevice::MemoryStrap()
{
    return ~0U;
}

/* virtual */ RC NoGpuSubdevice::GetSOCDeviceTypes(vector<UINT32>* pDeviceTypes, SOCDevFilter filter)
{
    MASSERT(pDeviceTypes);
    pDeviceTypes->clear();
    return OK;
}

/* virtual */ bool NoGpuSubdevice::GpioIsOutput(UINT32 WhichGpio)
{
    return false;
}

/* virtual */ void NoGpuSubdevice::GpioSetOutput(UINT32 WhichGpio, bool Level)
{
    SysUtil::Gpio::Write(WhichGpio, Level);
}

/* virtual */ bool NoGpuSubdevice::GpioGetOutput(UINT32 WhichGpio)
{
    return false;
}

/* virtual */ bool NoGpuSubdevice::GpioGetInput(UINT32 WhichGpio)
{
    UINT32 data = 0;
    SysUtil::Gpio::Read(WhichGpio, &data);
    return data ? true : false;
}

/* virtual */ RC NoGpuSubdevice::SetGpioDirection(UINT32 GpioNum, GpioDirection Direction)
{
    if (Direction == INPUT)
    {
        return SysUtil::Gpio::DirectionInput(GpioNum);
    }
    else
    {
        return SysUtil::Gpio::DirectionOutput(GpioNum, 0);
    }
}

/* virtual */ UINT32 NoGpuSubdevice::GetZlwllPixelsCovered() const
{
    return 0;
}

/* virtual */ UINT32 NoGpuSubdevice::GetZlwllNumZFar() const
{
    return 0;
}

/* virtual */ UINT32 NoGpuSubdevice::GetZlwllNumZNear() const
{
    return 0;
}

/* virtual */ RC NoGpuSubdevice::SetZlwllSize(UINT32 zlwllRegion, UINT32 displayWidth, UINT32 displayHeight)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC NoGpuSubdevice::SetDebugState(string Name, UINT32 Value)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ UINT32 NoGpuSubdevice::GetTextureAlignment()
{
    return 0;
}

/* virtual */ RefManual* NoGpuSubdevice::GetRefManual() const
{
    return 0;
}

/* virtual */ RC NoGpuSubdevice::ValidateVbios
(
    bool         bIgnoreFailure,
    VbiosStatus *pStatus,
    bool        *pbIgnored
)
{
    MASSERT(pStatus);
    *pStatus = VBIOS_OK;
    return OK;
}

/* virtual */ RC NoGpuSubdevice::IlwalidateL2Cache(UINT32 flags)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ int NoGpuSubdevice::EscapeWrite(const char* Path, UINT32 Index, UINT32 Size, UINT32 Value)
{
    return 1;
}

/* virtual */ int NoGpuSubdevice::EscapeRead(const char* Path, UINT32 Index, UINT32 Size, UINT32* Value)
{
    return 1;
}

/* virtual */ int NoGpuSubdevice::EscapeWriteBuffer(const char* Path, UINT32 Index, size_t Size, const void* Buf)
{
    return 1;
}

/* virtual */ int NoGpuSubdevice::EscapeReadBuffer(const char* Path, UINT32 Index, size_t Size, void* Buf) const
{
    return 1;
}

/* virtual */ RC NoGpuSubdevice::FbFlush(FLOAT64 TimeOutMs)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ void NoGpuSubdevice::SetupBugs()
{
    // We should have chip-specific bugs set up here
    // if we ever have non-GPU-specific bugs.
}

/* virtual */ void NoGpuSubdevice::SetupFeatures()
{
    SetHasFeature(GPUSUB_HAS_FRAMEBUFFER_IN_SYSMEM);
    SetHasFeature(GPUSUB_HAS_SYSMEM);
    SetHasFeature(GPUSUB_CHIP_INTEGRATED);
    SetHasFeature(GPUSUB_ALLOW_UNDETECTED_DISPLAYS);
    SetHasFeature(GPUSUB_SUPPORTS_PAGING);
    SetHasFeature(GPUSUB_SUPPORTS_TILING);
    SetHasFeature(GPUSUB_HAS_NONCOHERENT_SYSMEM);
    SetHasFeature(GPUSUB_SUPPORTS_SYSMEM_REFLECTED_MAPPING);
    SetHasFeature(GPUSUB_HAS_BLOCK_LINEAR);
}

/* virtual */ RC NoGpuSubdevice::SetCompressionMap(Surface2D* pSurf, const vector<UINT32>& ComprMap)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ bool NoGpuSubdevice::Is32bitChannel()
{
    return false;
}

/* virtual */ void NoGpuSubdevice::GetMmeToMem2MemRegInfo(bool bEnable, UINT32* pOffset, UINT32* pMask, UINT32* pData)
{
}

/* virtual */ UINT32 NoGpuSubdevice::GetMmeInstructionRamSize()
{
    return 0;
}

/* virtual */ UINT32 NoGpuSubdevice::GetMmeVersion()
{
    return 0;
}

/* virtual */ UINT32 NoGpuSubdevice::GetMmeStartAddrRamSize()
{
    return 0;
}

/* virtual */ UINT32 NoGpuSubdevice::GetMmeShadowRamSize()
{
    return 0;
}

/* virtual */ RC NoGpuSubdevice::MmeShadowRamWrite(const MmeMethod Method, UINT32 Data)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC NoGpuSubdevice::MmeShadowRamRead(const MmeMethod Method, UINT32* pData)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ bool NoGpuSubdevice::IsQuadroEnabled()
{
    return false;
}

/* virtual */ bool NoGpuSubdevice::IsMsdecEnabled()
{
    return false;
}

/* virtual */ bool NoGpuSubdevice::IsEccFuseEnabled()
{
    return false;
}

/* virtual */ RC NoGpuSubdevice::SetFbMinimum(UINT32 fbMinMB)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ bool NoGpuSubdevice::RegPowerGated(UINT32 Offset) const
{
    return false;
}

/* virtual */ bool NoGpuSubdevice::RegPrivProtected(UINT32 Offset) const
{
    return false;
}

/* virtual */ unsigned NoGpuSubdevice::GetNumIntrTrees() const
{
    return 0;
}

/* virtual */ string NoGpuSubdevice::GetRefManualFile() const
{
    return "";
}

/* virtual */ GpuPerfmon* NoGpuSubdevice::CreatePerfmon(void)
{
    return 0;
}

void NoGpuSubdevice::PostRmShutdown()
{
}

CREATE_GPU_SUBDEVICE_FUNCTION(NoGpuSubdevice);
