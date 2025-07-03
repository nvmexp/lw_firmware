/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_TESTDEVICE_H
#define INCLUDED_TESTDEVICE_H

#include "core/include/rc.h"
#include "core/include/device.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_libif.h"
#include "gpu/reghal/reghal.h"
#include "gpu/perf/thermsub.h"
#include <memory>

class Ism;
class Fuse;
class Display;

/* This it the test device class.  All devices that can be tested
 * by MODS should inherit from this class.  Individual tests will
 * be bound to specific devices. */

class LwRm;

class TestDevice : public Device
{
public:
    INTERFACE_HEADER(TestDevice);

    enum ChipFoundry
    {
         CF_TSMC
        ,CF_UMC
        ,CF_IBM
        ,CF_SMIC
        ,CF_CHARTERED
        ,CF_SONY
        ,CF_TOSHIBA
        ,CF_SAMSUNG
        ,CF_UNKNOWN
    };

    enum class FundamentalResetOptions
    {
        Default,
        Enable,
        Disable
    };

    struct DeviceError
    {
        UINT32 typeId;
        UINT32 instance;
        UINT32 subinstance;
        bool   bFatal;
        string name;
        string jsonTag;
        DeviceError(UINT32 typeId, UINT32 instance, UINT32 subinstance,
                    bool fatal, string name, string jsonTag)
            : typeId(typeId), instance(instance), subinstance(subinstance),
              bFatal(fatal), name(name), jsonTag(jsonTag) {}
    };

    TestDevice(Id i);
    TestDevice(Id i, Device::LwDeviceId deviceId);
    virtual ~TestDevice() {}

    virtual UINT32 BusType();
    virtual string DeviceIdString() const = 0;
    UINT32 DevInst() const { return m_DevInst; }
    virtual RC EndOfLoopCheck(bool captureReference);
    virtual UINT32 GetDriverId() const { return ~0U; }
    virtual RC GetAteDvddIddq(UINT32 *pIddq) = 0;
    virtual RC GetAteIddq(UINT32* pIddq, UINT32* pVersion) = 0;
    virtual RC GetAteRev(UINT32 *pAteRev) = 0;
    virtual RC GetAteSpeedos(vector<UINT32> *pValues, UINT32 * pVersion) = 0;
    virtual RC GetChipPrivateRevision(UINT32* pPrivRev) = 0;
    virtual RC GetChipRevision(UINT32* pRev) = 0;
    virtual RC GetChipSubRevision(UINT32* pRev) { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC GetChipSkuModifier(string *pStr) = 0;
    virtual RC GetChipSkuNumber(string *pStr) = 0;
    virtual RC GetChipSkuNumberModifier(string *pStr);
    virtual RC GetChipTemps(vector<FLOAT32> *pTempsC) = 0;
    virtual RC GetClockMHz(UINT32 *pClkMhz) = 0;
    virtual RC GetDeviceErrorList(vector<DeviceError>* pErrorList) = 0;
    virtual Device::LwDeviceId GetDeviceId() const = 0;
    virtual UINT32 GetDeviceTypeInstance() const { return m_DeviceTypeInstance; }
    virtual Display* GetDisplay() = 0;
    virtual RC GetEcidString(string* pStr) = 0;
    virtual RC GetExpectedOverTempCounter(UINT32 *pCount) { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC GetFoundry(ChipFoundry *pFoundry) = 0;
    virtual Fuse* GetFuse() = 0;
    virtual Id GetId() const { return m_Id; }
    virtual Ism* GetIsm() = 0;
    virtual LwLinkDevIf::LibIfPtr GetLibIf() const { return LwLinkDevIf::LibIfPtr(); }
    virtual RC GetOverTempCounter(UINT32 *pCount) { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC GetPdi(UINT64* pPdi) { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC GetPhysicalId(UINT32* pId) { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC GetRawEcid(vector<UINT32> *pEcids, bool bReverseFields = false) = 0;
    virtual RC GetRomVersion(string* pVersion) = 0;
    virtual RC GetTestPhase(UINT32* pPhase);
    virtual Type GetType() const = 0;
    virtual RC GetVoltageMv(UINT32 *pMv) = 0;
    virtual RC FLReset() = 0;
    virtual RC HotReset(FundamentalResetOptions funResetOption = FundamentalResetOptions::Default) = 0;
    virtual RC PexReset() = 0;
    virtual RC Initialize();
    virtual bool IsEmulation() const = 0;
    virtual bool IsEndpoint() const = 0;
    virtual bool IsSOC() const { return false; }
    virtual bool IsModsInternalPlaceholder() const = 0;
    virtual bool IsSysmem() const = 0;
    virtual void Print(Tee::Priority pri, UINT32 code, string prefix) const = 0;
    virtual RC ReadHost2Jtag
    (
        UINT32 Chiplet,
        UINT32 Instruction,
        UINT32 ChainLength,
        vector<UINT32> *pResult
    ) = 0;
    virtual RegHal& Regs() { return m_Regs; }
    virtual const RegHal& Regs() const { return m_Regs; }
    virtual UINT32 RegRd32(UINT32 offset) const = 0;
    virtual UINT32 RegRd32
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset
    ) = 0;
    virtual void RegWr32(UINT32 offset, UINT32 data) = 0;
    virtual void RegWr32
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset,
        UINT32        data
    ) = 0;
    virtual void RegWrBroadcast32
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset,
        UINT32        data
    ) = 0;
    virtual void RegWrSync32(UINT32 offset, UINT32 data) = 0;
    virtual void RegWrSync32
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset,
        UINT32        data
    ) = 0;
    virtual UINT64 RegRd64(UINT32 offset) const = 0;
    virtual UINT64 RegRd64
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset
    ) = 0;
    virtual void RegWr64(UINT32 offset, UINT64 data) = 0;
    virtual void RegWr64
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset,
        UINT64        data
    ) = 0;
    virtual void RegWrBroadcast64
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset,
        UINT64        data
    ) = 0;
    virtual void RegWrSync64(UINT32 offset, UINT64 data) = 0;
    virtual void RegWrSync64
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset,
        UINT64        data
    ) = 0;
    virtual void SetDeviceTypeInstance(UINT32 inst) { m_DeviceTypeInstance = inst; }
    virtual void SetDevInst(UINT32 inst) { m_DevInst = inst; }
    virtual RC SetOverTempRange(OverTempCounter::TempType overTempType, INT32 min, INT32 max)
        { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC ValidateSoftwareTree() { return RC::OK; }
    virtual RC WriteHost2Jtag
    (
        UINT32 Chiplet,
        UINT32 Instruction,
        UINT32 ChainLength,
        const vector<UINT32> &InputData
    ) = 0;
    virtual RC LoadHulkLicence(const vector<UINT32> &hulk, bool ignoreErrors)
        { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC GetIsDevidHulkEnabled(bool *pIsEnabled) const
        { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC GetIsDevidHulkRevoked(bool *pIsRevoked) const
        { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC GetFpfGfwRev(UINT32 *pGfwRev) const
        { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC GetFpfRomFlashRev(UINT32 *pGfwRev) const
        { return RC::UNSUPPORTED_FUNCTION; }

    string GetTypeName() const;

    static bool IsPciDeviceInAllowedList(const Id& id);
    static string TypeToString(Type t);

    static const UINT32 ILWALID_PHYSICAL_ID = ~0U;

    //! Used to set per-thread device id in a scope, use SCOPED_DEV_INST macro instead
    class ScopedDevInst
    {
        public:
            explicit ScopedDevInst(const TestDevice* pDev);
            ~ScopedDevInst();

            ScopedDevInst(const ScopedDevInst&) = delete;
            ScopedDevInst& operator=(const ScopedDevInst&) = delete;

        private:
            INT32 m_OldId;
    };

    virtual RC IsSltFused(bool *pIsFused) const
        { return RC::UNSUPPORTED_FUNCTION; }
private:
    RegHal m_Regs;
    Id     m_Id;
    UINT32 m_DevInst = ~0U;            //!< Instance of this device
    UINT32 m_DeviceTypeInstance = ~0U; //!< Instance of this particular type of device
    Xp::DriverStats m_ReferenceDriverStats = {};

    // These definitions are so that subclasses can use the override keyword
    // and it will compile regardless of whether they actually implement any
    // interfaces in that configuration
    virtual TestDevice* GetDevice() = 0;
    virtual const TestDevice* GetDevice() const = 0;
};
typedef shared_ptr<TestDevice> TestDevicePtr;

//! Used to set per-thread device id in a scope
#define SCOPED_DEV_INST(pDev) TestDevice::ScopedDevInst __MODS_UTILITY_CATNAME(setDevId, __LINE__)((pDev))

#endif
