/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "lwl_libif.h"
#include "lwl_devif.h"
#include "core/include/utility.h"
#include "gpu/include/device_interfaces.h"
#include <map>

class LwRm;

namespace LwLinkDevIf
{
    class IbmNpuLibIf;

    //------------------------------------------------------------------------------
    //! \brief Abstraction for IBM NPU device
    //!
    class IbmNpuDev : public TestDevice
    {
        public:
            IbmNpuDev
            (
                LibIfPtr pIbmNpuLibif,
                Id i,
                const vector<LibInterface::LibDeviceHandle> &handles
            );
            virtual ~IbmNpuDev() { }
        protected:
            string GetName() const override;
            RC Initialize() override;

            void GetHandles(vector<LwLinkDevIf::LibInterface::LibDeviceHandle> * pHandles)
                { for (auto info : m_LibLinkInfo) pHandles->push_back(info.first); }
        private:
            // Make all public virtual functions from the base class private
            // in order to forbid external access to this class except through
            // the base class pointer
            RC GetAteDvddIddq(UINT32 *pIddq) override
                { return RC::UNSUPPORTED_FUNCTION; }
            RC GetAteIddq(UINT32 *pIddq, UINT32 *pVersion) override
                { return RC::UNSUPPORTED_FUNCTION; }
            RC GetAteRev(UINT32 *pAteRev) override
                { return RC::UNSUPPORTED_FUNCTION; }
            RC GetAteSpeedos(vector<UINT32> *pValues, UINT32 * pVersion) override
                { return RC::UNSUPPORTED_FUNCTION; }
            RC GetChipPrivateRevision(UINT32 *pPrivRev) override
                { return RC::UNSUPPORTED_FUNCTION; }
            RC GetChipRevision(UINT32 *pRev) override
                { return RC::UNSUPPORTED_FUNCTION; };
            RC GetChipSkuModifier(string *pStr) override
                { return RC::UNSUPPORTED_FUNCTION; }
            RC GetChipSkuNumber(string *pStr) override
                { return RC::UNSUPPORTED_FUNCTION; }
            RC GetChipTemps(vector<FLOAT32> *pTempsC) override
                { return RC::UNSUPPORTED_FUNCTION; }
            RC GetClockMHz(UINT32 *pClkMhz) override
                { return RC::UNSUPPORTED_FUNCTION; }
            RC GetDeviceErrorList(vector<DeviceError>* pErrorList) override;
            Display* GetDisplay() override { return nullptr; }
            RC GetEcidString(string *pStr) override
                { MASSERT(pStr); *pStr = GetName(); return OK; }
            RC GetFoundry(ChipFoundry *pFoundry) override
                { return RC::UNSUPPORTED_FUNCTION; }
            Fuse* GetFuse() override { return nullptr; }
            Ism* GetIsm() override { return nullptr; }
            LwLinkDevIf::LibIfPtr GetLibIf() const override { return m_pLibIf; }
            RC GetRawEcid(vector<UINT32> *pEcids, bool bReverseFields = false) override
                { return RC::UNSUPPORTED_FUNCTION; }
            RC GetRomVersion(string* pVersion) override
                { return RC::UNSUPPORTED_FUNCTION; }
            Type GetType(void) const override { return TYPE_IBM_NPU; }
            RC GetVoltageMv(UINT32 *pMv) override
                { return RC::UNSUPPORTED_FUNCTION; }
            RC HotReset(FundamentalResetOptions funResetOption) override;
            RC FLReset() override { return RC::UNSUPPORTED_FUNCTION; }
            RC PexReset() override { return RC::UNSUPPORTED_FUNCTION; }
            bool IsEmulation() const override { return false; }
            bool IsEndpoint() const override { return true; }
            bool IsModsInternalPlaceholder() const override { return false; }
            bool IsSysmem() const override { return true; }
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
            RC ReadHost2Jtag
            (
                UINT32 Chiplet,
                UINT32 Instruction,
                UINT32 ChainLength,
                vector<UINT32> *pResult
            ) override
                { return RC::UNSUPPORTED_FUNCTION; }
            RC WriteHost2Jtag
            (
                UINT32 Chiplet,
                UINT32 Instruction,
                UINT32 ChainLength,
                const vector<UINT32> &InputData
            ) override
                { return RC::UNSUPPORTED_FUNCTION; }
        private:
            virtual UINT32 GetMaxLinks() const = 0;

            LibInterface::LibDeviceHandle GetLibHandle(UINT32 linkId) const;
            UINT32 GetLibLinkId(UINT32 linkId) const;

            enum PciProcedure
            {
                PP_Reset
            };
            RC RunPciProcedure(UINT32 linkId, PciProcedure procedure);

            LibIfPtr m_pLibIf = nullptr;         //!< Library Interface
            //! Vector of (handle, linknumber) pairs used to communicate with the
            //! library interface
            vector<pair<LibInterface::LibDeviceHandle, UINT32>> m_LibLinkInfo;
            UINT32       m_SavedReplayThresh = ~0U; //!< Saved value of the replay threshold
                                                    //!< (MODS needs to change the threshold
                                                    //!< for testing)
            UINT32       m_SavedSL1ErrCountCtrl = ~0U;
    };
};
