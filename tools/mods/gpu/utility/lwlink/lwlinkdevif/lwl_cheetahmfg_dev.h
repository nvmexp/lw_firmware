/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_LWL_GPU_DEV_H
#define INCLUDED_LWL_GPU_DEV_H

#include <memory>

#include "gpu/include/testdevice.h"
#include "gpu/include/device_interfaces.h"

class LwRm;

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief Abstraction for LwLink devices that are part of a LWPU GPU
    //!
    class LwTegraMfgDev
      : public TestDevice
      , public DeviceInterfaces<Device::T194MFGLWL>::interfaces
    {
        public:
            LwTegraMfgDev(LibInterface::LibDeviceHandle handle);
            virtual ~LwTegraMfgDev() { }

        private:
            // Make all public virtual functions from the base class private
            // in order to forbid external access to this class except through
            // the base class pointer
            //
            // Functions sorted alphabetically
            UINT32 BusType() override;
            string DeviceIdString() const override;
            RC GetAteDvddIddq(UINT32 *pIddq) override;
            RC GetAteIddq(UINT32 *pIddq, UINT32 *pVersion) override;
            RC GetAteRev(UINT32 *pAteRev) override;
            RC GetAteSpeedos(vector<UINT32> *pValues, UINT32 * pVersion) override;
            RC GetChipPrivateRevision(UINT32 *pPrivRev) override;
            RC GetChipRevision(UINT32 *pRev) override;
            RC GetChipSkuModifier(string *pStr) override;
            RC GetChipSkuNumber(string *pStr) override;
            RC GetChipTemps(vector<FLOAT32> *pTempsC) override;
            RC GetClockMHz(UINT32 *pClkMhz) override;
            RC GetDeviceErrorList(vector<DeviceError>* pErrorList) override;
            Device::LwDeviceId GetDeviceId() const override;
            Display* GetDisplay() override { return nullptr; }
            RC GetEcidString(string *pStr) override;
            RC GetFoundry(ChipFoundry *pFoundry) override;
            Fuse* GetFuse() override { return nullptr; }
            Ism* GetIsm() override { return nullptr; }
            LwLinkDevIf::LibIfPtr GetLibIf() const override;
            string GetName() const override;
            RC GetRawEcid(vector<UINT32> *pEcids, bool bReverseFields = false) override;
            RC GetRomVersion(string* pVersion) override { return RC::UNSUPPORTED_FUNCTION; }
            Type GetType(void) const override { return TYPE_TEGRA_MFG; }
            RC GetVoltageMv(UINT32 *pMv) override;

            RC HotReset(FundamentalResetOptions funResetOption) override;
            RC FLReset() override { return RC::UNSUPPORTED_FUNCTION; }
            RC PexReset() override { return RC::UNSUPPORTED_FUNCTION; }

            RC Initialize() override;

            bool IsEmulation() const override { return false; }
            bool IsEndpoint() const override { return true; }
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

            RC Shutdown() override;

        private: // Private functions for gpu devices
            TestDevice* GetDevice() override { return this; }
            const TestDevice* GetDevice() const override { return this; }
            RC SetupLinkInfo();

            Device::LibDeviceHandle m_LibHandle =
                Device::ILWALID_LIB_HANDLE; //!< Handle to the library
            bool       m_Initialized = false; //!< True if the device has been initialized

            // Return the expected data size in bytes for this domain & offset.
            virtual UINT32 GetDataSize(UINT32 domain, UINT32 offset) { return 4; }

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
    };
};

#endif
