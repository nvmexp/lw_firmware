/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "core/include/framebuf.h"
#include "core/include/utility.h"
#include "gpu/include/floorsweepimpl.h"
#include "gpu/include/gpusbdev.h"

class StubGpuSubdevice : public GpuSubdevice
{
public:
    StubGpuSubdevice(const char* n, Gpu::LwDeviceId i, const PciDevInfo* p) : GpuSubdevice(n, i, p) { }

    GpuPerfmon * CreatePerfmon() override { return nullptr; }
    unique_ptr<PciCfgSpace> AllocPciCfgSpace() const override { return make_unique<PciCfgSpace>(); }
    void   ApplyJtagChanges(UINT32 processFlag) override { }
    RC     CalibratePLL(Gpu::ClkSrc clkSrc) override { return RC::SOFTWARE_ERROR; }
    void   ClearEdcDebug(UINT32 virtualFbio) override { return; }
    void   DisablePwrBlock() override { }
    RC     DoEndOfSimCheck(Gpu::ShutDownMode mode) override { return OK; }
    void   EnableBar1PhysMemAccess() override { }
    RC     EnableFBRowDebugMode() override { return RC::UNSUPPORTED_FUNCTION; }
    void   EnablePwrBlock() override { }
    RC     FbFlush(FLOAT64 timeOutMs) override { return RC::SOFTWARE_ERROR; }
    RC     GetAteIddq(UINT32 *pIddq, UINT32* pVersion) override { return RC::UNSUPPORTED_FUNCTION; }
    RC     GetAteRev(UINT32* pRevision) override { return RC::UNSUPPORTED_FUNCTION; }
    RC     GetAteSpeedo(UINT32 SpeedoNum, UINT32 *pSpeedo, UINT32 *pVersion) override { return RC::UNSUPPORTED_FUNCTION; }
    RC     GetAteSramIddq(UINT32 *pIddq) override { return RC::UNSUPPORTED_FUNCTION; }
    UINT32 GetCbcBaseAddress() override { return 0; }
    UINT32 GetCompCacheLineSize() override { return 0; }
    UINT32 GetComptagLinesPerCacheLine() override { return 0; }
    UINT32 GetCropWtThreshold() override { return 0; }
    RC     GetEcid(string* pEcid) override { return RC::UNSUPPORTED_FUNCTION; }
    RC     GetEdcDebugInfo(UINT32 virtualFbio, ExtEdcInfo *result) override { return RC::UNSUPPORTED_FUNCTION; }
    RC     GetEdcEnabled(bool *pbSupported, UINT32 *pEnabledMask) override { *pbSupported = false; return OK; }
    UINT32 GetEmulationRev() override { return 0; }
    UINT32 GetEngineStatus(Engine engineName) override { return 0; }
    UINT32 GetFbBankSwizzle() override { return 0; }
    bool   GetFbBankSwizzleIsSet() override { return 0; }
    bool   GetFbGddr5x16Mode() override { return false; }
    bool   GetFbMs01Mode() override { return false; }
    string GetFermiGpcTileMap() override { return ""; }
    RC     GetKFuseStatus(KFuseStatus *pStatus, FLOAT64 TimeoutMs) override { return RC::UNSUPPORTED_FUNCTION; }
    bool   GetL1EccEnabled() const override { return false; }
    bool   GetL2EccCheckbitsEnabled() const override { return false; }
    UINT32 GetMaxFbpCount() const override { return 0; }
    UINT32 GetMaxGpcCount() const override { return 0; }
    UINT32 GetMaxShaderCount() const override { return 0; }
    UINT32 GetMaxSmPerTpcCount() const override { return 0; }
    UINT32 GetMaxTpcCount() const override { return 0; }
    UINT32 GetMaxZlwllCount() const override { return 0; }
    UINT32 GetMmeInstructionRamSize() override { return 0; }
    UINT32 GetMmeShadowRamSize() override { return 0; }
    UINT32 GetMmeStartAddrRamSize() override { return 0; }
    void   GetMmeToMem2MemRegInfo(bool bEnable, UINT32 *pOffset, UINT32 *pMask, UINT32 *pData)override { }
    UINT32 GetMmeVersion() override { return 0; }
    RC     GetPdi(UINT64* pPdi) override { return RC::UNSUPPORTED_FUNCTION; }
    UINT32 GetPLLCfg(PLLType Pll) const override { return 0; }
    void   GetPLLList(vector<PLLType>* pPllList) const override { pPllList->clear(); }
    RC     GetRawEcid(vector<UINT32>* pEcid, bool bReverseFields = false) override { return RC::UNSUPPORTED_FUNCTION; }
    RC     GetRdEdcErrors(UINT32* count) override { return RC::UNSUPPORTED_FUNCTION; }
    RC     GetSmEccEnable(bool * pEnable, bool * pOverride) const override { return RC::UNSUPPORTED_FUNCTION; }
    UINT32 GetSpeedoCount() override { return GetFB()->GetFbioCount(); }
    RC     GetStraps(vector<pair<UINT32, UINT32> >* pStraps) override { return RC::UNSUPPORTED_FUNCTION; }
    UINT32 GetSysPartitionCount() override { return 0; }
    UINT32 GetTexCountPerTpc() const override { return 0; }
    UINT32 GetTextureAlignment() override { return 1; }
    UINT32 GetTpcCount() const override { return Utility::CountBits(GetFsImpl()->TpcMask()); }
    UINT32 GetUserdAlignment() const override { return 0; }
    RC     GetWrEdcErrors(UINT32* count) override { return RC::UNSUPPORTED_FUNCTION; }
    UINT32 GetZlwllNumZFar() const override { return 0; }
    UINT32 GetZlwllNumZNear() const override { return 0; }
    UINT32 GetZlwllPixelsCovered() const override { return 0; }
    unsigned GetNumIntrTrees() const override { return 1; }
    UINT32 GobHeight() const override { return 8; }
    RC     HwGpcToVirtualGpc(UINT32 hwGpcNum, UINT32 *pVirtualGpcNum) const override { *pVirtualGpcNum = hwGpcNum;  return OK; }
    RC     HwTpcToVirtualTpc(UINT32 hwGpcNum, UINT32 hwTpcNum, UINT32 *pVirtualTpcNum) const  override { *pVirtualTpcNum = hwTpcNum; return OK; }
    RC     IlwalidateCCache() override { return RC::UNSUPPORTED_FUNCTION; }
    RC     IlwalidateCompBitCache(FLOAT64 timeoutMs) override { return OK; }
    RC     IlwalidateL2Cache(UINT32 flags) override { return RC::UNSUPPORTED_FUNCTION; }
    UINT32 Io3GioConfigPadStrap() override { return 0xFFFFFFFF; }
    bool   Is32bitChannel() override { return false; }
    bool   IsAzaliaMandatory() const override { return false; }
    bool   IsDisplayFuseEnabled() override { return false; }
    bool   IsEccFuseEnabled() override { return false; }
    bool   IsFeatureEnabled(Feature feature) override { return false; }
    bool   IsMsdecEnabled() override { return false; }
    bool   IsPLLEnabled(PLLType Pll) const override { return false; }
    bool   IsPLLLocked(PLLType Pll) const override { return false; }
    bool   IsQuadroEnabled() override { return false; }
    bool   IsXbarSecondaryPortSupported() const override { return false; }
    bool   IsXbarSecondaryPortConfigurable() const override { return false; }
    RC     LoadMmeShadowData() override { return RC::UNSUPPORTED_FUNCTION; }
    RC     MmeShadowRamRead(const MmeMethod method, UINT32 *pData) override { return RC::UNSUPPORTED_FUNCTION; }
    RC     MmeShadowRamWrite(const MmeMethod method, UINT32 data) override { return RC::UNSUPPORTED_FUNCTION; }
    void   PostRmShutdown() override { };
    UINT32 PrivateRevisionWithoutRm() override  { return 0xbad; }
    UINT32 MemoryIndex() override { return 0xFFFFFFFF; }
    UINT32 RamConfigStrap() override { return 0xFFFFFFFF; }
    bool   RegPowerGated(UINT32 offset) const override { return false; }
    void   ResetEdcErrors() override { }
    RC     RestoreFBConfig(const FBConfigData& data) override { return RC::UNSUPPORTED_FUNCTION; }
    UINT32 RevisionWithoutRm() override { return 0xbad; };
    RC     SaveFBConfig(FBConfigData* pData) override { return RC::UNSUPPORTED_FUNCTION; }
    RC     SetCompressionMap(Surface2D* pSurf, const vector<UINT32>& comprMap) override { return RC::UNSUPPORTED_FUNCTION; }
    void   SetCropWtThreshold(UINT32 Thresh) override { }
    RC     SetDebugState(string name, UINT32 value) override { return RC::UNSUPPORTED_FUNCTION; }
    RC     SetFbMinimum(UINT32 fbMinMB) override { return OK; }
    RC     SetGpioDirection(UINT32 gpioNum, GpioDirection direction) override { return RC::UNSUPPORTED_FUNCTION; }
    RC     SetIntThermCalibration(UINT32 valueA, UINT32 valueB) override { return RC::UNSUPPORTED_FUNCTION; }
    RC     SetL1EccEnabled(bool enable) override { return RC::UNSUPPORTED_FUNCTION; }
    RC     SetL2EccCheckbitsEnabled(bool enable) override { return RC::UNSUPPORTED_FUNCTION; }
    void   SetPLLCfg(PLLType Pll, UINT32 cfg) override { }
    void   SetPLLLocked(PLLType Pll) override  { }
    RC     SetSMDebuggerMode(bool enable) override { return RC::UNSUPPORTED_FUNCTION; }
    RC     SetSmEccEnable(bool bEnable, bool bOverride) override { return RC::UNSUPPORTED_FUNCTION; }
    RC     SetSysPll(UINT64 targetHz) override { return RC::UNSUPPORTED_FUNCTION; }
    RC     SetZlwllSize(UINT32 zlwllRegion, UINT32 displayWidth, UINT32 displayHeight) override { return RC::CANNOT_ALLOCATE_MEMORY; }
    UINT32 ShaderCount() const override { return 0; }
    RC     SmidToHwGpcHwTpc(UINT32 VirtualGpcNum, UINT32 *pHwGpcNum, UINT32 *pHwTpcNum) const override { return RC::UNSUPPORTED_FUNCTION; }
    void   StopVpmCapture() override { }
    UINT32 TvEncoderStrap() override { return 0xFFFFFFFF; }
    UINT32 UserStrapVal() override { return 0xFFFFFFFF; }
    RC     VirtualGpcToHwGpc(UINT32 virtualGpcNum, UINT32 *pHwGpcNum) const override { *pHwGpcNum = virtualGpcNum;  return OK; }
    RC     VirtualTpcToHwTpc(UINT32 hwGpcNum, UINT32 virtualTpcNum, UINT32 *pHwTpcNum) const  override { *pHwTpcNum = virtualTpcNum; return OK; }
};
