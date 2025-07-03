/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_LWL_SIM_DEV_H
#define INCLUDED_LWL_SIM_DEV_H

#include "core/include/utility.h"
#include "gpu/include/testdevice.h"
#include "gpu/lwlink/lwlinkimpl.h"

class LwRm;

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief Abstraction for simulated lwlink devies
    class SimDev : public TestDevice
    {
        public:
            SimDev(Id i);
            virtual ~SimDev() { }

        private:
            // Make all public functions from the base class private
            // in order to forbid external access to this class except through
            // the base class pointer

            // Functions sorted alphabetically
            UINT32 BusType() override { return 0; }
            RC GetAteDvddIddq(UINT32 *pIddq) override { return RC::UNSUPPORTED_FUNCTION; }
            RC GetAteIddq
            (
                UINT32 *pIddq,
                UINT32 *pVersion
            ) override { return RC::UNSUPPORTED_FUNCTION; }
            RC GetAteRev(UINT32 *pAteRev) override { return RC::UNSUPPORTED_FUNCTION; }
            RC GetAteSpeedos
            (
                vector<UINT32> *pValues,
                UINT32 * pVersion
            ) override{ return RC::UNSUPPORTED_FUNCTION; }
            RC GetChipPrivateRevision
            (
                UINT32 *pPrivRev
            ) override { return RC::UNSUPPORTED_FUNCTION; }
            RC GetChipRevision(UINT32 *pRev) override { return RC::UNSUPPORTED_FUNCTION; }
            RC GetChipSkuModifier(string *pStr) override { return RC::UNSUPPORTED_FUNCTION; }
            RC GetChipSkuNumber(string *pStr) override { return RC::UNSUPPORTED_FUNCTION; }
            RC GetChipTemps(vector<FLOAT32> *pTempsC) override { return RC::UNSUPPORTED_FUNCTION; }
            RC GetClockMHz(UINT32 *pClkMhz) override { return RC::UNSUPPORTED_FUNCTION; }
            RC GetDeviceErrorList(vector<DeviceError>* pErrorList) override;
            Display* GetDisplay() override { return nullptr; }
            RC GetEcidString(string *pStr) override;
            RC GetFoundry(ChipFoundry *pFoundry) override { return RC::UNSUPPORTED_FUNCTION; }
            Fuse* GetFuse() override { return nullptr; }
            Ism* GetIsm() override { return nullptr; }

            LibIfPtr GetLibIf() const override { return LibIfPtr(); }
            string GetName() const override;
            RC GetRawEcid(vector<UINT32> *pEcids, bool bReverseFields = false) override;
            RC GetRomVersion(string* pVersion) override { return RC::UNSUPPORTED_FUNCTION; }
            RC GetVoltageMv(UINT32 *pMv) override { return RC::UNSUPPORTED_FUNCTION; }
            RC HotReset(FundamentalResetOptions funResetOption) override { return RC::UNSUPPORTED_FUNCTION; }
            RC FLReset() override { return RC::UNSUPPORTED_FUNCTION; }
            RC PexReset() override { return RC::UNSUPPORTED_FUNCTION; }

            RC Initialize() override;

            bool IsEmulation() const override { return false; }
            bool IsEndpoint() const override { return true; }
            bool IsModsInternalPlaceholder() const override { return true; }
            bool IsSysmem() const override { return false; }
            void Print(Tee::Priority pri, UINT32 code, string prefix) const override;

            UINT32 RegRd32(UINT32 offset) const override { return 0; }
            UINT32 RegRd32
            (
                UINT32        domIdx,
                RegHalDomain  domain,
                LwRm        * pLwRm,
                UINT32        offset
            ) override { return 0; }
            void RegWr32(UINT32 offset, UINT32 data) override {}
            void RegWr32
            (
                UINT32        domIdx,
                RegHalDomain  domain,
                LwRm        * pLwRm,
                UINT32        offset,
                UINT32        data
            ) override { }
            void RegWrBroadcast32
            (
                UINT32        domIdx,
                RegHalDomain  domain,
                LwRm        * pLwRm,
                UINT32        offset,
                UINT32        data
            ) override { }
            void RegWrSync32(UINT32 offset, UINT32 data) override {}
            void RegWrSync32
            (
                UINT32        domIdx,
                RegHalDomain  domain,
                LwRm        * pLwRm,
                UINT32        offset,
                UINT32        data
            ) override { }
            UINT64 RegRd64(UINT32 offset) const override { return 0; }
            UINT64 RegRd64
            (
                UINT32        domIdx,
                RegHalDomain  domain,
                LwRm        * pLwRm,
                UINT32        offset
            ) override { return 0; }
            void RegWr64(UINT32 offset, UINT64 data) override {}
            void RegWr64
            (
                UINT32        domIdx,
                RegHalDomain  domain,
                LwRm        * pLwRm,
                UINT32        offset,
                UINT64        data
            ) override {}
            void RegWrBroadcast64
            (
                UINT32        domIdx,
                RegHalDomain  domain,
                LwRm        * pLwRm,
                UINT32        offset,
                UINT64        data
            ) override {}
            void RegWrSync64(UINT32 offset, UINT64 data) override {}
            void RegWrSync64
            (
                UINT32        domIdx,
                RegHalDomain  domain,
                LwRm        * pLwRm,
                UINT32        offset,
                UINT64        data
            ) override {}

            RC Shutdown() override { m_Initialized = false; return OK; }

            RC ReadHost2Jtag
            (
                UINT32 Chiplet,
                UINT32 Instruction,
                UINT32 ChainLength,
                vector<UINT32> *pResult
            ) override { return RC::UNSUPPORTED_FUNCTION; }

            RC WriteHost2Jtag
            (
                UINT32 Chiplet,
                UINT32 Instruction,
                UINT32 ChainLength,
                const vector<UINT32> &InputData
            ) override { return RC::UNSUPPORTED_FUNCTION; }


            virtual string GetTypeString() const = 0;

        private:
            bool m_Initialized;                       //!< True if the device has been initialized
    };
};

#endif

