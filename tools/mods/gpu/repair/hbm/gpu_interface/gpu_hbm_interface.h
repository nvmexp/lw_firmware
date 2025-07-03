/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/hbmlane.h"
#include "core/include/hbmtypes.h"
#include "core/include/memlane.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/pmgrmutex.h"
#include "gpu/repair/hbm/hbm_wir.h"
#include "gpu/repair/mem_repair_config.h"
#include "gpu/utility/hbmdev.h"

#include <memory>

//!
//! \file gpu_hbm_interface.h
//!
//! \brief GPU-to-HBM interface.
//!

namespace Memory
{
namespace Hbm
{
    //!
    //! \brief Methods of waiting for completion.
    //!
    enum class WaitMethod
    {
        //! Polls the value for completion continuously. Thread is not
        //! yielded.
        POLL,

        //! Most colwenient type of waiting will be used. This may include
        //! polling.
        DEFAULT
    };

    //!
    //! \brief The GPU interface to HBM.
    //!
    //! All GPU related interactions should be provided as APIs here. This
    //! allows:
    //!   1) Traceability of all HBM interaction for debugging and correctness
    //!      purposes.
    //!   2) Separation of concerns between the HBM sequences and the GPU
    //!      interface used to interact with the HBM.
    //!
    //! Extend for each GPU that has a new hardware/software interface for
    //! interacting with the HBM.
    //!
    //! NOTE: *Raw functions mean they do direct access to the associate
    //! register without any addition preamble or postamble register
    //! interaction.
    //!
    class GpuHbmInterface
    {
        using Settings        = Repair::Settings;

    public:
        GpuHbmInterface(GpuSubdevice* pSubdev);
        virtual ~GpuHbmInterface() {}

        //!
        //! \brief Write the given WIR to the specified channel of an HBM site
        //! and read the result from the WDR.
        //!
        //! \param wir WIR to write.
        //! \param hbmSite HBM site to write to.
        //! \param chanSel Channel(s) to write to.
        //! \param pData[out] WDR read data.
        //!
        virtual RC WirRead
        (
            const Wir& wir,
            const Site& hbmSite,
            Wir::ChannelSelect chanSel,
            WdrData* data
        ) const = 0;

        RC WirWrite
        (
            const Wir& wir,
            const Site& hbmSite,
            Wir::ChannelSelect chanSel
        ) const;

        //!
        //! \brief Write the given WIR to the specified channel(s) of an HBM site.
        //!
        //! \param wir WIR to write.
        //! \param hbmSite HBM site to write to.
        //! \param chanSel Channel(s) to write to.
        //! \param data Data to write into the WDR register.
        //!
        virtual RC WirWrite
        (
            const Wir& wir,
            const Site& hbmSite,
            Wir::ChannelSelect chanSel,
            const WdrData& data
        ) const = 0;

        virtual RC WirWriteRaw
        (
            const Wir& wir,
            const Site& hbmSite,
            Wir::ChannelSelect chanSel
        ) const = 0;

        virtual RC WirWriteRaw
        (
            const Site& hbmSite,
            const UINT32 data
        ) const = 0;

        virtual RC ModeWriteRaw
        (
            const Wir& wir,
            const Site& hbmSite
        ) const = 0;

        virtual RC ModeWriteRaw
        (
             const Site& hbmSite,
             const UINT32 data
        ) const = 0;

        //!
        //! \brief Writes the mode register and the WDR. The WIR is not written
        //! to the instruction register.
        //!
        //! \param wir WIR to write.
        //! \param hbmSite HBM site to write to.
        //! \param chanSel Channel(s) to write to.
        //! \param data WDR write data.
        //!
        virtual RC WdrWrite
        (
            const Wir& wir,
            const Site& hbmSite,
            Wir::ChannelSelect chanSel,
            const WdrData& data
        ) const = 0;

        //!
        //! \brief Read the WDR result from a previous exelwtion of the given WIR on the
        //! specified channel of an HBM site.
        //!
        //! \param wir WIR associated with the WDR data.
        //! \param hbmSite HBM site to write to.
        //! \param chanSel Channel(s) to write to.
        //! \param pData[out] WDR read data.
        //!
        virtual RC WdrReadRaw
        (
            const Wir& wir,
            const Site& hbmSite,
            Wir::ChannelSelect chanSel,
            WdrData* pData
        ) const = 0;

        virtual RC WdrReadRaw
        (
            const Site& hbmSite,
            UINT32* pData
        ) const = 0;

        virtual RC WdrReadCompareRaw
        (
            const Site& hbmSite,
            UINT32 data
        ) const = 0;


        //!
        //! \brief Write WDR data directly.
        //!
        //! \param hbmSite HBM site to write to.
        //! \param hbmChannel Site channel to write to.
        //! \param chanSel Channel(s) to write to.
        //! \param data WDR write data.
        //!
        virtual RC WdrWriteRaw
        (
            const Site& hbmSite,
            Wir::ChannelSelect chanSel,
            const WdrData& data
        ) const = 0;

        virtual RC WdrWriteRaw
        (
            const Site& hbmSite,
            const UINT32 data
        ) const = 0;

        RC WirRead
        (
            const Wir& wir,
            const Site& hbmSite,
            const Channel& hbmChannel,
            WdrData* pData
        ) const;

        RC WirWrite
        (
            const Wir& wir,
            const Site& hbmSite,
            const Channel& hbmChannel
        ) const;

        RC WirWrite
        (
            const Wir& wir,
            const Site& hbmSite,
            const Channel& hbmChannel,
            const WdrData& data
        ) const;

        RC WirWriteRaw
        (
            const Wir& wir,
            const Site& hbmSite,
            const Channel& hbmChannel
        ) const;

        RC WdrWrite
        (
            const Wir& wir,
            const Site& hbmSite,
            const Channel& hbmChannel,
            const WdrData& data
        ) const;

        RC WdrReadRaw
        (
            const Wir& wir,
            const Site& hbmSite,
            const Channel& hbmChannel,
            WdrData* pData
        ) const;

        RC WdrWriteRaw
        (
            const Site& hbmSite,
            const Channel& hbmChannel,
            const WdrData& data
        ) const;

        //!
        //! \brief Reset the given HBM site.
        //!
        virtual RC ResetSite(const Site& hbmSite) const = 0;

        //!
        //! \brief Toggle the HBM WRST_n line off and back on.
        //!
        virtual RC ToggleWrstN(const Site& hbmSite) const { return RC::UNSUPPORTED_FUNCTION; }

        void SleepMs(FLOAT64 ms) const;
        void SleepUs(UINT32 us) const;

        RC RemapGpuLanes(const FbpaLane& lane);
        RC AcquirePmgrMutex(std::unique_ptr<PmgrMutexHolder>* ppPmgrMutex) const;

        FbpaSubp GetSubpartition(const FbpaLane& lane) const;
        virtual FbioSubp FbpaSubpToFbioSubp(const FbpaSubp& fbpaSubp) const = 0;
        virtual FbpaSubp FbioSubpToFbpaSubp(const FbioSubp& fbioSubp) const = 0;

        RC HwFbpaSubpToHbmSiteChannel
        (
            HwFbpa hwFbpa,
            FbpaSubp fbpaSubp,
            Site* pHbmSite,
            Channel* pHbmChannel
        ) const;
        RC HbmSiteChannelToHwFbpaSubp
        (
            Site hbmSite,
            Channel hbmChannel,
            HwFbpa* pHwFbpa,
            FbpaSubp* pFbpaSubp
        ) const;

        virtual RC GetHbmLane(const FbpaLane& fbpaLane, HbmLane* pHbmLane) const = 0;
        virtual RC GetFbpaLane(const HbmLane& hbmLane, FbpaLane* pFbpaLane) const = 0;

        //!
        //! \brief Return the HBM DWORD and byte number corresponding to the given lane.
        //!
        //! \param lane HBM lane.
        //! \param[out] pDword HBM DWORD within HBM channel (0-indexed).
        //! Nullable.
        //! \param[out] pHbmByte HBM byte within DWORD (0-indexed). Nullable.
        //!
        virtual RC GetHbmDwordAndByte
        (
            const HbmLane& lane,
            UINT32* pHbmDword,
            UINT32* pHbmByte
        ) const = 0;

        //!
        //! \brief Return the FBPA DWORD and byte number corresponding to the given lane.
        //!
        //! \param lane FBPA lane.
        //! \param[out] pFbpaDword FBPA DWORD within subpartition (0-indexed).
        //! Nullable.
        //! \param[out] pFbpaByte FBPA byte within DWORD (0-indexed). Nullable.
        //!
        virtual RC GetFbpaDwordAndByte
        (
            const FbpaLane& lane,
            UINT32* pFbpaDword,
            UINT32* pFbpaByte
        ) const = 0;

        //!
        //! \brief Return the FBPA DWORD and byte number corresponding to the given lane.
        //!
        //! \param lane HBM lane.
        //! \param[out] pFbpaDword FBPA DWORD within subpartition (0-indexed).
        //! Nullable.
        //! \param[out] pFbpaByte FBPA byte within DWORD (0-indexed). Nullable.
        //!
        RC GetFbpaDwordAndByte
        (
            const HbmLane& lane,
            UINT32* pFbpaDword,
            UINT32* pFbpaByte
        ) const;

        RC GetHbmSiteMasterFbpa(const Site& hbmSite, HwFbpa* pHwFbpa) const;

        //!
        //! \brief Wait for IEEE1500 bridge to idle.
        //!
        //! \param hbmSite HBM site.
        //! \param waitMethod Type of wait to perform.
        //! \param timeoutMs Wait timeout in milliseconds. \a -1 indicates a
        //! default timeout.
        //!
        virtual RC Ieee1500WaitForIdle
        (
            Site hbmSite,
            WaitMethod waitMethod,
            FLOAT64 timeoutMs = -1
        ) const = 0;

        //!
        //! \brief Retrieve the remap value from a link repair register.
        //!
        //! \param hwFbpa Haredware FBPA.
        //! \param fbpaSubp FBPA subpartition.
        //! \param fbpaDword FBPA DWORD within the subpartition.
        //! \param[out] pRemapValue Remap value.
        //! \param[out] pIsBypassSet true if the bypass bit is set in the link
        //! repair register, false otherwise.
        //!
        virtual RC GetLinkRepairRegRemapValue
        (
            const HwFbpa& hwFbpa,
            const FbpaSubp& fbpaSubp,
            UINT32 fbpaDword,
            UINT16* pRemapValue,
            bool* pIsBypassSet
        ) const = 0;

        FrameBuffer* GetFb() const;
        GpuSubdevice* GetGpuSubdevice() const;
        HBMDevice* GetHbmDevice() const;

        void SetSettings(const Settings &settings);
        Settings* GetSettings() const;

    protected:
        GpuSubdevice* m_pSubdev = nullptr; //!< GPU subdevice
        Settings m_Settings; //!< Repair settings
    };
} // Hbm
} // Memory
