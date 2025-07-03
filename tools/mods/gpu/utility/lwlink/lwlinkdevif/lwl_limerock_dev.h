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

#include "lwl_lwswitch_dev.h"
#include "gpu/utility/thermalslowdown.h"
#include "gpu/include/device_interfaces.h"

extern unique_ptr<Fuse> CreateLimerockFuse(TestDevice*);

namespace LwLinkDevIf
{
    //------------------------------------------------------------------------------
    //! \brief Abstraction for Limerock device
    //!
    class LimerockDev : public LwSwitchDev
                      , public ThermalSlowdown
    {
    public:
        LimerockDev
        (
            LibIfPtr pLwSwitchLibif,
            Id i,
            LibInterface::LibDeviceHandle handle,
            Device::LwDeviceId deviceId
        ) : LwSwitchDev(pLwSwitchLibif, i, handle, deviceId) {}
        virtual ~LimerockDev() = default;

        RC GetChipTemps(vector<FLOAT32> *pTempsC) override;
        RC GetPdi(UINT64* pPdi) override;

    protected:
        RC SetupFeatures() override
        {
            SetHasFeature(GPUSUB_SUPPORTS_FUSING_WO_DAUGHTER_CARD);
            SetHasFeature(GPUSUB_SUPPORTS_IFF_SW_FUSING);
            SetHasFeature(GPUSUB_HAS_MERGED_FUSE_MACROS);
            SetHasFeature(GPUSUB_SUPPORTS_LWLINK_OFF_TRANSITION);
            SetHasFeature(GPUSUB_LWLINK_LP_SLM_MODE_POR);
            return LwLinkDevIf::LwSwitchDev::SetupFeatures();
        }
        RC Initialize() override;

        RC HotReset(FundamentalResetOptions funResetOption) override;
        RC PexReset() override;

    private: // Interface implementations
        // ThermalSlowdown
        RC DoStartThermalSlowdown(UINT32 periodUs) override;
        RC DoStopThermalSlowdown() override;

    private:
        vector<FLOAT32>  m_bjtHotspotOffsets;
        RC GetBjtHotspotOffsets(vector<FLOAT32> *pHotspotOffsetsC);

        RC LoadHulkLicence(const vector<UINT32> &hulk, bool ignoreErrors) override;

        RC GetIsDevidHulkRevoked(bool *pIsRevoked) const override;

        RC GetFpfGfwRev(UINT32 *pGfwRev) const override;
        RC GetFpfRomFlashRev(UINT32 *pGfwRev) const override;
    };

#if LWCFG(GLOBAL_LWSWITCH_IMPL_LR10)
    class LR10Dev final
        : public LimerockDev
        , public DeviceInterfaces<Device::LR10>::interfaces
    {
    public:
        LR10Dev
        (
            LibIfPtr pLwSwitchLibif,
            Id i,
            LibInterface::LibDeviceHandle handle
        );
        virtual ~LR10Dev() = default;

    protected:
        std::unique_ptr<Fuse> CreateFuse() override
        {
            return CreateLimerockFuse(this);
        }
        RC SetupBugs() override
        {
            SetHasBug(2597550);
            SetHasBug(2720088);
            return RC::OK;
        }

    private:
        string DeviceIdString() const override { return "LR10"; }
        Device::LwDeviceId GetDeviceId() const override { return Device::LR10; }
        Ism* GetIsm() override { return m_pIsm.get(); }
        RC Shutdown() override;

        TestDevice* GetDevice() override { return this; }
        const TestDevice* GetDevice() const override { return this; }

        unique_ptr<Ism>  m_pIsm; //!< ISM object pointer
    };
#endif
};
