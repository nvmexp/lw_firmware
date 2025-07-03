/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu_hbm_interface.h"

#include "core/include/utility.h"
#include "gpu/include/hbmimpl.h"

using namespace Memory;

//!
//! \brief Constructor.
//!
//! \param pSubdev The GPU subdevice abstracted by this interface.
//!
Hbm::GpuHbmInterface::GpuHbmInterface(GpuSubdevice* pSubdev)
    : m_pSubdev(pSubdev)
{
    MASSERT(pSubdev);
}

//!
//! \brief Write the given WIR to the specified channel(s) of an HBM site.
//!
//! \param wir WIR to write.
//! \param hbmSite HBM site to write to.
//! \param chanSel Channel(s) to write to.
//!
RC Hbm::GpuHbmInterface::WirWrite
(
    const Wir& wir,
    const Site& hbmSite,
    Wir::ChannelSelect chanSel
) const
{
    return WirWrite(wir, hbmSite, chanSel, WdrData());
}

//!
//! \brief Write the given WIR to the specified channel of an HBM site and read the
//! result from the WDR.
//!
//! \param wir WIR to write.
//! \param hbmSite HBM site to write to.
//! \param hbmChannel Channel to write to.
//! \param pData[out] WDR read data.
//!
RC Hbm::GpuHbmInterface::WirRead
(
    const Wir& wir,
    const Site& hbmSite,
    const Channel& hbmChannel,
    WdrData* pData
) const
{
    return WirRead(wir, hbmSite,
                   static_cast<Wir::ChannelSelect>(hbmChannel.Number()),
                   pData);
}

//!
//! \brief Write the given WIR to the specified channel of an HBM site.
//!
//! \param wir WIR to write.
//! \param hbmSite HBM site to write to.
//! \param hbmChannel Channel to write to.
//!
RC Hbm::GpuHbmInterface::WirWrite
(
    const Wir& wir,
    const Site& hbmSite,
    const Channel& hbmChannel
) const
{
    return WirWrite(wir, hbmSite,
                    static_cast<Hbm::Wir::ChannelSelect>(hbmChannel.Number()));
}

//!
//! \brief Write the given WIR and its WDR data to the specified channel on an
//! HBM site.
//!
//! \param wir WIR to write.
//! \param hbmSite HBM site to write to.
//! \param hbmChannel Channel to write to.
//! \param data WDR write data.
//!
RC Hbm::GpuHbmInterface::WirWrite
(
    const Wir& wir,
    const Site& hbmSite,
    const Channel& hbmChannel,
    const WdrData& data
) const
{
    return WirWrite(wir, hbmSite,
                    static_cast<Hbm::Wir::ChannelSelect>(hbmChannel.Number()),
                    data);
}

//!
//! \brief Write the given WIR to the specific channel on an HBM site. Only the
//! instruction register is written.
//!
//! \param wir WIR to write.
//! \param hbmSite HBM site to write to.
//! \param hbmChannel Channel to write to.
//!
RC Hbm::GpuHbmInterface::WirWriteRaw
(
    const Wir& wir,
    const Site& hbmSite,
    const Channel& hbmChannel
) const
{
    return WirWriteRaw(wir, hbmSite,
                       static_cast<Hbm::Wir::ChannelSelect>(hbmChannel.Number()));
}

//!
//! \brief Writes the mode register and the WDR. The WIR is not written
//! to the instruction register.
//!
//! \param wir WIR to write.
//! \param hbmSite HBM site to write to.
//! \param hbmChannel Channel to write to.
//! \param data WDR write data.
//!
RC Hbm::GpuHbmInterface::WdrWrite
(
    const Wir& wir,
    const Site& hbmSite,
    const Channel& hbmChannel,
    const WdrData& data
) const
{
    return WdrWrite(wir, hbmSite,
                    static_cast<Hbm::Wir::ChannelSelect>(hbmChannel.Number()),
                    data);
}

//!
//! \brief Read the WDR result from a previous exelwtion of the given WIR on the
//! specified channel of an HBM site.
//!
//! \param wir WIR associated with the WDR data.
//! \param hbmSite HBM site to write to.
//! \param hbmChannel Channel to write to.
//! \param pData[out] WDR read data.
//!
RC Hbm::GpuHbmInterface::WdrReadRaw
(
    const Wir& wir,
    const Site& hbmSite,
    const Channel& hbmChannel,
    WdrData* pData
) const
{
    return WdrReadRaw(wir, hbmSite,
                      static_cast<Wir::ChannelSelect>(hbmChannel.Number()),
                      pData);
}

//!
//! \brief Write WDR data directly.
//!
//! \param hbmSite HBM site to write to.
//! \param hbmChannel Channel to write to.
//! \param data WDR write data.
//!
RC Hbm::GpuHbmInterface::WdrWriteRaw
(
    const Site& hbmSite,
    const Channel& hbmChannel,
    const WdrData& data
) const
{
    return WdrWriteRaw(hbmSite,
                       static_cast<Wir::ChannelSelect>(hbmChannel.Number()),
                       data);
}

//!
//! \brief Sleep the CPU for the given number of milliseconds.
//!
//! If no work is being done on the GPU, it implies sleeping the GPU as well.
//!
//! \param ms Time to sleep in milliseconds.
//!
void Hbm::GpuHbmInterface::SleepMs(FLOAT64 ms) const
{
    static constexpr FLOAT64 usPerMs = 1000;
    SleepUs(static_cast<UINT32>(ms * usPerMs));
}

//!
//! \brief Sleep the CPU for the given number of microseconds.
//!
//! If no work is being done on the GPU, it implies sleeping the GPU as well.
//!
//! \param us Time to sleep in microseconds.
//!
void Hbm::GpuHbmInterface::SleepUs(UINT32 us) const
{
    Utility::SleepUS(us);
}


//!
//! \brief Remap the given GPU lane to the HBM's spare lane.
//!
//! \param lane GPU FBPA lane to be remapped.
//!
RC Hbm::GpuHbmInterface::RemapGpuLanes(const FbpaLane& lane)
{
    return m_pSubdev->ReconfigGpuHbmLanes(lane);
}

//!
//! \brief Acquire Pmgr mutex. Release on unique_ptr destruction.
//!
//! \param ppPmgrMutex Mutex to acquire.
//!
RC Hbm::GpuHbmInterface::AcquirePmgrMutex(std::unique_ptr<PmgrMutexHolder>* ppPmgrMutex) const
{
    MASSERT(ppPmgrMutex);
    *ppPmgrMutex = make_unique<PmgrMutexHolder>(m_pSubdev, PmgrMutexHolder::MI_HBM_IEEE1500);
    return (*ppPmgrMutex)->Acquire(Tasker::NO_TIMEOUT);
}

//!
//! \brief Return the subpartition associated with the given lane.
//!
//! \param lane FBPA lane.
//!
FbpaSubp Hbm::GpuHbmInterface::GetSubpartition(const FbpaLane& lane) const
{
    return FbpaSubp(GetHbmDevice()->GetSubpartition(lane));
}

//!
//! \brief Colwerts a hardware FBPA and subpartiton to an HBM site and channel.
//!
//! \param hwFbpa Hardware FBPA.
//! \param fbpaSubp Subpartition associated with \a hwFbpa.
//! \param[out] pHbmSite HBM site.
//! \param[out] pHbmChannel HBM channel associated with \a pHbmSite.
//!
RC Hbm::GpuHbmInterface::HwFbpaSubpToHbmSiteChannel
(
    HwFbpa hwFbpa,
    FbpaSubp fbpaSubp,
    Site* pHbmSite,
    Channel* pHbmChannel
) const
{
    MASSERT(pHbmSite);
    MASSERT(pHbmChannel);
    RC rc;

    FrameBuffer* pFb = GetFb();
    UINT32 hbmSite = 0;
    UINT32 hbmChannel = 0;
    CHECK_RC(pFb->FbioSubpToHbmSiteChannel(pFb->HwFbpaToHwFbio(hwFbpa.Number()),
                                           FbpaSubpToFbioSubp(fbpaSubp).Number(),
                                           &hbmSite, &hbmChannel));

    *pHbmSite = Site(hbmSite);
    *pHbmChannel = Channel(hbmChannel);

    return rc;
}

RC Hbm::GpuHbmInterface::HbmSiteChannelToHwFbpaSubp
(
    Site hbmSite,
    Channel hbmChannel,
    HwFbpa* pHwFbpa,
    FbpaSubp* pFbpaSubp
) const
{
    MASSERT(pHwFbpa);
    MASSERT(pFbpaSubp);
    RC rc;

    FrameBuffer* pFb = GetFb();
    UINT32 hwFbio = 0;
    UINT32 fbioSubp = 0;
    CHECK_RC(pFb->HbmSiteChannelToFbioSubp(hbmSite.Number(),
                                           hbmChannel.Number(),
                                           &hwFbio, &fbioSubp));
    *pHwFbpa = HwFbpa(pFb->HwFbioToHwFbpa(hwFbio));
    *pFbpaSubp = FbpaSubp(FbioSubpToFbpaSubp(FbioSubp(fbioSubp)));

    return rc;
}

RC Hbm::GpuHbmInterface::GetFbpaDwordAndByte
(
    const HbmLane& lane,
    UINT32* pFbpaDword,
    UINT32* pFbpaByte
) const
{
    MASSERT(pFbpaDword || pFbpaByte);
    RC rc;

    FbpaLane fbpaLane;
    CHECK_RC(GetFbpaLane(lane, &fbpaLane));
    return GetFbpaDwordAndByte(fbpaLane, pFbpaDword, pFbpaByte);
}

//!
//! \brief Return the master FBPA for a given HBM site.
//!
//! The master FBPA is the one that controls the HBM site.
//!
//! \param hbmSite HBM site.
//! \param[out] pHwFbpa Master hardware FBPA for \a hbmSite.
//!
RC Hbm::GpuHbmInterface::GetHbmSiteMasterFbpa(const Site& hbmSite, HwFbpa* pHwFbpa) const
{
    MASSERT(pHwFbpa);
    RC rc;

    UINT32 masterFbpaNum;
    CHECK_RC(m_pSubdev->GetHBMSiteMasterFbpaNumber(hbmSite.Number(), &masterFbpaNum));
    *pHwFbpa = HwFbpa(masterFbpaNum);

    return rc;
}

//!
//! \brief Return the frambuffer associated with the GPU-HBM interface.
//!
FrameBuffer* Hbm::GpuHbmInterface::GetFb() const
{
    return m_pSubdev->GetFB();
}

//!
//! \brief Return the GPU subdevice associated with the GPU-HBM interface.
//!
GpuSubdevice* Hbm::GpuHbmInterface::GetGpuSubdevice() const
{
    return m_pSubdev;
}

//!
//! \brief Return the HBM device associated with the GPU-HBM interface.
//!
HBMDevice* Hbm::GpuHbmInterface::GetHbmDevice() const
{
    return m_pSubdev->GetHBMImpl()->GetHBMDev().get();
}

//!
//! \brief Sets Settings for repair configurations.
//!
void Hbm::GpuHbmInterface::SetSettings(const Settings &settings)
{
    m_Settings = settings;
}

//!
//! \brief Sets Settings for repair configurations.
//!
Repair::Settings* Hbm::GpuHbmInterface::GetSettings() const
{
    return const_cast<Settings*>(&m_Settings);
}
