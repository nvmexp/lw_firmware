/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_LWL_LWSWITCH_DEV_H
#define INCLUDED_LWL_LWSWITCH_DEV_H

#include "lwl_libif.h"
#include "core/include/utility.h"
#include "core/include/ism.h"
#include "gpu/utility/thermalthrottling.h"
#include "gpu/perf/thermsub.h"
#include "gpu/fuse/fuse.h"
#include "gpu/include/device_interfaces.h"
#include <map>
#include <vector>
#include <set>
#include <memory>

extern unique_ptr<Fuse> CreateLwSwitchFuse(TestDevice*);

namespace LwLinkDevIf
{
    //------------------------------------------------------------------------------
    //! \brief Abstraction for LwSwitch device
    //!
    class LwSwitchDev : public TestDevice
    {
        public:
            LwSwitchDev
            (
                LibIfPtr pLwSwitchLibif,
                Id i,
                LibInterface::LibDeviceHandle handle,
                Device::LwDeviceId deviceId
            );
            virtual ~LwSwitchDev() { }

            RC ReadHost2Jtag
            (
                UINT32 Chiplet,
                UINT32 Instruction,
                UINT32 ChainLength,
                vector<UINT32> *pResult
            ) override;

            RC WriteHost2Jtag
            (
                UINT32 Chiplet,
                UINT32 Instruction,
                UINT32 ChainLength,
                const vector<UINT32> &InputData
            ) override;

            virtual UINT32 GetDriverId() const
            {
                return m_LibHandle & ~LibInterface::HANDLE_TYPE_MASK;
            }

        protected:
            virtual std::unique_ptr<Fuse> CreateFuse()
            {
                return CreateLwSwitchFuse(this);
            }

            virtual RC SetupFeatures()
            {
                SetHasFeature(GPUSUB_SUPPORTS_ASPM_L1P);
                return RC::OK;
            }

            virtual RC SetupBugs()
            {
                return RC::OK;
            }

            // Make all public virtual functions from the base class private
            // in order to forbid external access to this class except through
            // the base class pointer
            RC GetAteDvddIddq(UINT32 *pIddq) override;
            RC GetAteIddq(UINT32 *pIddq, UINT32 *pVersion) override;
            RC GetAteSpeedos(vector<UINT32> *pValues, UINT32 * pVersion) override;
            RC GetAteRev(UINT32 *pAteRev) override;
            RC GetChipPrivateRevision(UINT32 *pPrivRev) override;
            RC GetChipRevision(UINT32 *pRev) override;
            RC GetChipSkuModifier(string *pStr) override;
            RC GetChipSkuNumber(string *pStr) override;
            RC GetChipTemps(vector<FLOAT32> *pTempsC) override;
            RC GetClockMHz(UINT32 *pClkMhz) override;
            RC GetDeviceErrorList(vector<DeviceError>* pErrorList) override;
            Display* GetDisplay() override { return nullptr; }
            RC GetEcidString(string *pStr) override;
            RC GetExpectedOverTempCounter(UINT32 *pCount) override;
            RC GetFoundry(ChipFoundry *pFoundry) override;
            Fuse* GetFuse() override;
            LwLinkDevIf::LibIfPtr GetLibIf() const override { return m_pLibIf; }
            string GetName() const override;
            RC GetOverTempCounter(UINT32 *pCount) override;
            RC GetPhysicalId(UINT32* pId) override;
            RC GetRawEcid(vector<UINT32> *pEcids, bool bReverseFields = false) override;
            RC GetRomVersion(string* pVersion) override;
            Type GetType(void) const override { return TYPE_LWIDIA_LWSWITCH; }
            RC GetVoltageMv(UINT32 *pMv) override;
            RC HotReset(FundamentalResetOptions funResetOption) override;
            RC FLReset() override { return RC::UNSUPPORTED_FUNCTION; }
            RC PexReset() override { return RC::UNSUPPORTED_FUNCTION; }
            RC Initialize() override;
            bool IsEmulation() const override { return m_IsEmulation; }
            bool IsEndpoint() const override { return false; }
            bool IsModsInternalPlaceholder() const override { return false; }
            bool IsSysmem() const override { return false; }
            void Print(Tee::Priority pri, UINT32 code, string prefix) const override;
            UINT32 RegRd32(UINT32 offset) const override;
            UINT32 RegRd32
            (
                UINT32        domIdx,
                RegHalDomain  domain,
                LwRm        * pLwRm,
                UINT32        offset
            ) override;
            void RegWr32(UINT32 offset, UINT32 data) override;
            void RegWr32
            (
                UINT32        domIdx,
                RegHalDomain  domain,
                LwRm        * pLwRm,
                UINT32        offset,
                UINT32        data
            ) override;
            void RegWrBroadcast32
            (
                UINT32        domIdx,
                RegHalDomain  domain,
                LwRm        * pLwRm,
                UINT32        offset,
                UINT32        data
            ) override;
            void RegWrSync32(UINT32 offset, UINT32 data) override;
            void RegWrSync32
            (
                UINT32        domIdx,
                RegHalDomain  domain,
                LwRm        * pLwRm,
                UINT32        offset,
                UINT32        data
            ) override;
            UINT64 RegRd64(UINT32 offset) const override;
            UINT64 RegRd64
            (
                UINT32        domIdx,
                RegHalDomain  domain,
                LwRm        * pLwRm,
                UINT32        offset
            ) override;
            void RegWr64(UINT32 offset, UINT64 data) override;
            void RegWr64
            (
                UINT32        domIdx,
                RegHalDomain  domain,
                LwRm        * pLwRm,
                UINT32        offset,
                UINT64        data
            ) override;
            void RegWrBroadcast64
            (
                UINT32        domIdx,
                RegHalDomain  domain,
                LwRm        * pLwRm,
                UINT32        offset,
                UINT64        data
            ) override;
            void RegWrSync64(UINT32 offset, UINT64 data) override;
            void RegWrSync64
            (
                UINT32        domIdx,
                RegHalDomain  domain,
                LwRm        * pLwRm,
                UINT32        offset,
                UINT64        data
            ) override;
            RC SetOverTempRange(OverTempCounter::TempType overTempType, INT32 min, INT32 max) override;
            RC Shutdown() override;
            RC GetIsDevidHulkRevoked(bool *pIsRevoked) const override
                { return RC::UNSUPPORTED_FUNCTION; }
            RC GetFpfGfwRev(UINT32 *pGfwRev) const override
                { return RC::UNSUPPORTED_FUNCTION; }
            RC GetFpfRomFlashRev(UINT32 *pGfwRev) const override
                { return RC::UNSUPPORTED_FUNCTION; }

            LibInterface::LibDeviceHandle GetLibHandle() const { return m_LibHandle; }

            OverTempCounter& GetOverTempCounter() { return m_OverTempCounter; }

        private:
            bool     m_Initialized = false;      //!< True if the device was initialized
            bool     m_IsEmulation = false;      //!< True if the device is emulated
            LibIfPtr m_pLibIf = nullptr;      //!< Library Interface
            Device::LibDeviceHandle m_LibHandle
                        = Device::ILWALID_LIB_HANDLE; //!< Handle to the library
            UINT32 m_PhysicalId = ILWALID_PHYSICAL_ID;
            unique_ptr<Fuse>  m_pFuse;               //!< Fuse interface
            OverTempCounter m_OverTempCounter;
            UINT64 m_FatalErrorIndex = 0;

            static const UINT32 ILWALID_DOMAIN = ~0U;
    };

    class WillowDev final
        : public LwSwitchDev
        , public DeviceInterfaces<Device::SVNP01>::interfaces
        , public ThermalThrottling
    {
    public:
        WillowDev
        (
            LibIfPtr pLwSwitchLibif,
            Id i,
            LibInterface::LibDeviceHandle handle
        );
        virtual ~WillowDev() = default;
    private:
        string DeviceIdString() const override { return "SVNP01"; }
        Device::LwDeviceId GetDeviceId() const override { return Device::SVNP01; }
        Ism* GetIsm() override { return m_pIsm.get(); }
        RC Shutdown() override;

        RC SetupBugs() override
        {
            SetHasBug(2700543);
            return RC::OK;
        }

    private: // Interface Implementations
        // ThermalThrottling
        RC DoStartThermalThrottling(UINT32 onCount, UINT32 offCount) override;
        RC DoStopThermalThrottling() override;

    private:
        TestDevice* GetDevice() override { return this; }
        const TestDevice* GetDevice() const override { return this; }

        shared_ptr<Ism>  m_pIsm;                //!< ISM object pointer

    };
};

#endif
