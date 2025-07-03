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

#include "hbmdev.h"
#include "class/cl90e7.h"
#include "core/include/framebuf.h"
#include "core/include/hbmtypes.h"
#include "core/include/jscript.h"
#include "core/include/memerror.h"
#include "core/include/mgrmgr.h"
#include "core/include/mle_protobuf.h"
#include "core/include/platform.h"
#include "gpu/hbm/hbm_mle_colw.h"
#include "gpu/hbm/hbm_spec_defines.h"
#include "gpu/hbm/mbist/mbistsamsung.h"
#include "gpu/hbm/mbist/mbistskhynix.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/pmgrmutex.h"
#include "gpu/interface/vbios_preos_pbi.h"

#include "amap_v1.h"
#include "hal_v1.h"

#include <algorithm>
#include <map>
#include <memory>

namespace
{
    //! Bypass bit for the FBIO_HMB_LINK_REPAIR registers
    constexpr UINT32 FBIO_HBM_LINK_REPAIR_BYPASS_BIT = 1 << 16;
}

//-----------------------------------------------------------------------------
HBMSite::HBMSite()
: m_Initialized(false)
, m_UseHostToJtag(false)
, m_IsActive(false)
, m_MBistStarted(false)
, m_SiteID(-1)
, m_pSubdev(nullptr)
{
}

HBMSite::~HBMSite()
{
    DeInitSite();
}

//-----------------------------------------------------------------------------
void HBMSite::SetMBistImpl(MBistImpl * mbist)
{
    m_MbistImpl.reset(mbist);
}

//-----------------------------------------------------------------------------
RC HBMSite::StartMBistAtSite(const UINT32 siteID, const UINT32 mbistType)
{
    MASSERT(m_IsActive);

    RC rc;
    if (!m_MbistImpl)
    {
        Printf(Tee::PriError, "No MBIST implementation available.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    CHECK_RC(m_MbistImpl->StartMBist(siteID, mbistType));
    m_MBistStarted = true;
    return rc;
}

//-----------------------------------------------------------------------------
RC HBMSite::CheckMBistCompletionAtSite(const UINT32 siteID, const UINT32 mbistType)
{
    MASSERT(m_IsActive);

    RC rc;
    if (!m_MBistStarted)
    {
        return OK;
    }
    if (!m_MbistImpl)
    {
        Printf(Tee::PriError, "No MBIST implementation available.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    rc = m_MbistImpl->CheckCompletionStatus(siteID, mbistType);
    if (rc == OK)
    {
        // finished MBIST
        m_MBistStarted = false;
    }
    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Initialize the HBM site information by querying the site.
//!
//! \param pSubdev GPU subdevice associated with the HBM site.
//! \param siteId HBM site number.
//! \param useHostToJtag True if the Host-To-JTag interface should be used
//! to query the HBM. Otherwise the IEEE1500 interface is used.
//!
RC HBMSite::InitSiteInfo(GpuSubdevice *pSubdev, UINT32 siteId, bool useHostToJtag)
{
    MASSERT(pSubdev);
    MASSERT(m_IsActive);
    RC rc;

    const UINT32 numHbmSites = pSubdev->GetNumHbmSites();
    if (siteId >= numHbmSites)
    {
        Printf(Tee::PriError, "Site number out of range: %u, num sites: %u\n",
               siteId, numHbmSites);
        MASSERT(!"Site number out of range");
        return RC::SOFTWARE_ERROR;
    }

    if (m_Initialized &&
        (pSubdev != m_pSubdev || siteId != m_SiteID || useHostToJtag != m_UseHostToJtag))
    {
        Printf(Tee::PriError, "HBM site record is being initialized with new "
               "data without having been de-initialized first!\n");
        MASSERT(!"HBM site record is being initialized with new data without "
                "having been de-initialized first!");
        return RC::SOFTWARE_ERROR;
    }

    m_SiteID = siteId;
    m_pSubdev = pSubdev;
    m_UseHostToJtag = useHostToJtag;

    using HbmSite = Memory::Hbm::Site;
    CHECK_RC(m_pSubdev->CheckHbmIeee1500RegAccess(HbmSite(m_SiteID)));

    UINT32 masterFbpaNum = 0;
    CHECK_RC(m_pSubdev->GetHBMSiteMasterFbpaNumber(m_SiteID, &masterFbpaNum));

    {
        unique_ptr<PmgrMutexHolder> pPmgrMutex;

        // If we're using fbpriv path, we need to pull IEEE1500 out of reset (set to value of 0x11)
        if (!m_UseHostToJtag)
        {
            pPmgrMutex = make_unique<PmgrMutexHolder>(m_pSubdev, PmgrMutexHolder::MI_HBM_IEEE1500);
            CHECK_RC(pPmgrMutex->Acquire(Tasker::NO_TIMEOUT));
            m_pSubdev->Regs().Write32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO, masterFbpaNum, 0x11);

            // DFT team stated we need to sleep for 200us; but waiting longer to be sure
            Utility::Delay(1000);
        }

        CHECK_RC(m_pSubdev->GetHBMImpl()->InitSiteInfo(
                    m_SiteID,
                    m_UseHostToJtag,
                    &m_DevIdData));
    }

    // If using IEEE direct access sleep after releasing the pmgr mutex to prevent
    // starving the PMU on the mutex
    if (!m_UseHostToJtag)
        Tasker::Sleep(1);

    m_Initialized = true;

    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Initialize the HBM site information from the given HBM device ID
//! data.
//!
//! \param pSubdev GPU subdevice associated with the HBM site.
//! \param siteId HBM site number.
//! \param rawDevId The result of the DEVICE_ID WIR of the given HBM site.
//! Note, the value must be adjusted so that the final byte has the value in
//! the least significant bits (LSB), not the MSB.
//!
RC HBMSite::InitSiteInfo
(
    GpuSubdevice *pSubdev,
    UINT32 siteId,
    const vector<UINT32>& rawDevId
)
{
    MASSERT(pSubdev);
    MASSERT(m_IsActive);
    RC rc;

    const UINT32 numHbmSites = pSubdev->GetNumHbmSites();
    (void)numHbmSites;
    MASSERT(siteId < numHbmSites);

    if (m_Initialized && (pSubdev != m_pSubdev || siteId != m_SiteID))
    {
        Printf(Tee::PriError, "HBM site record is being initialized with new "
               "data without having been de-initialized first!\n");
        MASSERT(!"HBM site record is being initialized with new data without "
                "having been de-initialized first!");
        return RC::SOFTWARE_ERROR;
    }

    m_SiteID = siteId;
    m_pSubdev = pSubdev;
    m_UseHostToJtag = false;
    m_DevIdData.Initialize(rawDevId);

    // Validation of HBM model after initialization
    Memory::Hbm::Model HbmModel;
    rc = m_DevIdData.GetHbmModel(&HbmModel);
    if (rc != RC::OK)
    {
        DumpDevIdData();
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC HBMSite::DeInitSite()
{
    RC rc;
    if (!m_IsActive || !m_Initialized)
    {
        return rc;
    }

    m_SiteID = -1;
    m_pSubdev = nullptr;
    m_Initialized = false;
    m_IsActive = false;

    m_MbistImpl.reset();

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Init Site Specific information such as height, ECC support etc.
//!
const HbmDeviceIdData HBMSite::GetSiteInfo() const
{
    MASSERT(m_IsActive);
    return m_DevIdData;
}

//-----------------------------------------------------------------------------
//! \brief Start soft repair at HBM site
//!
RC HBMSite::DidJtagResetOclwrAfterSWRepair(bool *pReset) const
{
    MASSERT(m_IsActive);
    RC rc;
    if (!m_MbistImpl)
    {
        Printf(Tee::PriError, "No MBIST implementation available.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    CHECK_RC(m_MbistImpl->DidJtagResetOclwrAfterSWRepair(pReset));
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Check if JTAG reset happened
//!
RC HBMSite::RepairAtSite
(
    const bool isSoftRepair
    ,const bool isPseudoRepair
    ,const bool skipVerif
    ,const UINT32 stackID
    ,const UINT32 channel
    ,const UINT32 row
    ,const UINT32 bank
    ,const bool firstRow
    ,const bool lastRow
) const
{
    MASSERT(m_IsActive);
    RC rc;

    if (!m_MbistImpl)
    {
        Printf(Tee::PriError, "No MBIST implementation available.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    if ((stackID > 0 && m_DevIdData.GetStackHeight() <= 4) || stackID > 1)
    {
        Printf(Tee::PriError, "Invalid StackId (%u) for chip's StackHeight (%u)\n",
            stackID, m_DevIdData.GetStackHeight());
        return RC::HBM_REPAIR_IMPOSSIBLE;
    }

    Printf(Tee::PriNormal, "\nRepairing site %u, stackID %u, channel %u, bank %u, row %u:\n",
        m_SiteID, stackID, channel, bank, row);

    if (isSoftRepair)
    {
        return m_MbistImpl->SoftRowRepair(m_SiteID, stackID, channel, row, bank,
                                          firstRow, lastRow);
    }
    else
    {
        UINT32 numRowsAvailable = 0;

        // Check if spare repair rows are available in the bank
        CHECK_RC(m_MbistImpl->GetNumSpareRowsAvailable(m_SiteID, stackID, channel, bank,
                                                       &numRowsAvailable));

        if (skipVerif && (numRowsAvailable == 0))
        {
            Printf(Tee::PriWarn,
                  "No spare rows available in [site %u, stackID %u, channel %u, bank %u]\n",
                   m_SiteID, stackID, channel, bank);
            return RC::HBM_SPARE_ROWS_NOT_AVAILABLE;
        }

        // Do actual hard repair:
        CHECK_RC(m_MbistImpl->HardRowRepair(isPseudoRepair, m_SiteID, stackID, channel, row, bank));

        if (!isPseudoRepair)
        {
            // If the number of available rows didn't decrease, the repair wasn't successful
            const UINT32 prevNumRowsAvailable = numRowsAvailable;
            CHECK_RC(m_MbistImpl->GetNumSpareRowsAvailable(m_SiteID, stackID, channel, bank,
                                                        &numRowsAvailable));

            if (numRowsAvailable != prevNumRowsAvailable - 1)
            {
                Printf(Tee::PriWarn,
                    "Row repair failure in [site %u, stackID %u, channel %u, bank %u]\n",
                        m_SiteID, stackID, channel, bank);
                return RC::HBM_SPARE_ROWS_NOT_AVAILABLE;
            }
        }

        return rc;
    }
}

//-----------------------------------------------------------------------------
//! \brief Repair a lane by programming the HBM site fuses
//!
RC HBMSite::RepairLaneAtSite
(
    const bool isSoftRepair
    ,const bool skipVerif
    ,const bool isPseudoRepair
    ,const UINT32 hbmChannel
    ,const LaneError& laneError
) const
{
    MASSERT(m_IsActive);
    if (!m_MbistImpl)
    {
        Printf(Tee::PriError, "No MBIST implementation available.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    return m_MbistImpl->LaneRepair(isSoftRepair, skipVerif, isPseudoRepair,
                                   m_SiteID, hbmChannel, laneError);
}

//-----------------------------------------------------------------------------
//! \brief Print out Device ID Data for this site
//!
RC HBMSite::DumpDevIdData() const
{
    Printf(Tee::PriNormal, "Device Id Data for Site %u ->\n", m_SiteID);
    if (m_IsActive)
    {
        return m_DevIdData.DumpDevIdData();
    }
    else
    {
        Printf(Tee::PriNormal, "    [inactive]\n");
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
//! \brief Print out Row Repairs Data for this site
//!
RC HBMSite::DumpRepairedRows() const
{
    MASSERT(m_IsActive);
    RC rc;

    if (!m_MbistImpl)
    {
        MASSERT(!"No MBIST implementation available.\n");
        return RC::SOFTWARE_ERROR;
    }

    string channelsAvailStr = m_DevIdData.GetChannelsAvail();

    UINT32 numStacks = (m_DevIdData.GetStackHeight() == 8) ? 2 : 1;

    vector< vector<BankRepair> > siteRepairs(numStacks * HBM_MAX_NUM_CHANNELS);

    vector<UINT32> channelRepairs(numStacks * HBM_MAX_NUM_CHANNELS, 0);

    bool hasRepairs = false;

    Printf(Tee::PriNormal, "  Site %u Row Repairs->\n", m_SiteID);
    // Loop over all channels and see which channels can not be repaired any further
    // Loop over all banks and see which have had row repairs and print the no. of repaired rows
    for (UINT32 stackID = 0; stackID < numStacks; stackID++)
    {
        for (UINT32 channel = 0; channel < HBM_MAX_NUM_CHANNELS; channel++)
        {
            siteRepairs[channel + stackID * HBM_MAX_NUM_CHANNELS].resize(0);
            // Check if the channel is available
            if (channelsAvailStr.find('a' + channel) != string::npos)
            {
                UINT32 numRepairs = 0;
                for (UINT32 bank = 0; bank < HBM_BANKS_PER_CHANNEL; bank++)
                {
                    UINT32 numSpareRows = 0;
                    CHECK_RC(m_MbistImpl->GetNumSpareRowsAvailable(m_SiteID,
                                                                   stackID,
                                                                   channel,
                                                                   bank,
                                                                   &numSpareRows,
                                                                   false));
                    if (numSpareRows < 2)
                    {
                        hasRepairs = true;
                        BankRepair b;
                        b.m_BankID = bank;
                        b.m_NumRepairs = 2 - numSpareRows;
                        siteRepairs[channel + stackID * HBM_MAX_NUM_CHANNELS].push_back(b);
                        numRepairs += (2 - numSpareRows);
                    }
                }
                channelRepairs[channel + stackID * HBM_MAX_NUM_CHANNELS] = numRepairs;
            }
            else
            {
                if (numStacks == 2)
                {
                    Printf(Tee::PriLow,
                           "StackID %u Channel %u was unavailable\n",
                           stackID,
                           channel);
                }
                else
                {
                    Printf(Tee::PriLow, "Channel %u was unavailable\n", channel);
                }
            }
        }
    }

    if (!hasRepairs)
    {
        Printf(Tee::PriNormal, "    NO REPAIRS\n");
        return rc;
    }

    std::string ecid;
    CHECK_RC(m_pSubdev->GetEcid(&ecid));

    for (UINT32 i = 0; i < siteRepairs.size(); i++)
    {
        if (channelRepairs[i] > 0)
        {
            UINT32 stack = i / 8;
            UINT32 channel = i % 8;
            if (numStacks == 2)
            {
                Printf(Tee::PriNormal,
                       "    StackID %u Channel %u has %u/16 repaired rows\n",
                       stack,
                       channel,
                       channelRepairs[i]);
            }
            else
            {
                Printf(Tee::PriNormal,
                       "    Channel %u has %u/16 repaired rows\n",
                       channel,
                       channelRepairs[i]);
            }
            for (UINT32 j = 0; j < siteRepairs[i].size(); j++)
            {
                Printf(Tee::PriNormal,
                       "      Bank %u has %u repaired rows\n",
                       siteRepairs[i][j].m_BankID,
                       siteRepairs[i][j].m_NumRepairs);

                Mle::Print(Mle::Entry::hbm_repaired_row)
                    .ecid(ecid)
                    .site(m_SiteID)
                    .stack(stack)
                    .channel(channel)
                    .bank(siteRepairs[i][j].m_BankID)
                    .row_repairs(siteRepairs[i][j].m_NumRepairs);
            }
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Collect or print out lane repair data for this site.
//!
//! \param pOutLaneErrors: Populate given vector with the HBM fuse repair data.
//!
RC HBMSite::GetRepairedLanes(vector<LaneError>* pOutLaneRepairs) const
{
    MASSERT(m_IsActive);
    RC rc;

    if (!m_MbistImpl)
    {
        MASSERT(!"No MBIST implementation available.\n");
        return RC::SOFTWARE_ERROR;
    }

    string channelsAvailStr = m_DevIdData.GetChannelsAvail();

    for (UINT32 channel = 0; channel < HBM_MAX_NUM_CHANNELS; channel++)
    {
        // Check if the channel is available
        if (channelsAvailStr.find('a' + channel) != string::npos)
        {
            CHECK_RC(m_MbistImpl->GetRepairedLanes(m_SiteID, channel,
                                                   pOutLaneRepairs));
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
HBMDevice::HBMDevice(GpuSubdevice *pSubDev) :
    m_pSubdev(pSubDev)
    ,m_ECCSupported(false)
    ,m_Initialized(false)
    ,m_StackHeight(0)
    ,m_VendorID(HbmManufacturerId::Unknown)
    ,m_NumActiveSites(pSubDev->GetNumHbmSites())
    ,m_LastSiteSWRepaired(-1)
{
    // Host to jtag is implemented for GP100, but starting GV100, we should be using IEEE1500
    m_UseHostToJtag = m_pSubdev->DeviceId() == Gpu::GP100;
}

//-----------------------------------------------------------------------------
//!
//! \brief Collect HBM site information from the HBM directly.
//!
//! Uses the hardware interface to communicate directly with the HBM.
//!
RC HBMDevice::CollectSiteInfoFromHbm()
{
    RC rc;
    StickyRC firstRc;

    bool siteInfoCollected = false;
    const UINT32 numHbmSites = m_pSubdev->GetNumHbmSites();
    for (UINT32 sid = 0; sid < numHbmSites; sid++)
    {
        HBMSite& site = m_Sites[sid];

        // Check HBM site floorsweeping
        // Consider a site active if at least one FBP is active.
        UINT32 fbpNum1 = 0;
        UINT32 fbpNum2 = 0;
        CHECK_RC(m_pSubdev->GetHBMSiteFbps(sid, &fbpNum1, &fbpNum2));
        const UINT32 fbpFsMask = m_pSubdev->GetFsImpl()->FbpMask();
        const bool fbpNum1Active = (fbpFsMask & (1U << fbpNum1)) != 0;
        const bool fbpNum2Active = (fbpFsMask & (1U << fbpNum2)) != 0;
        const bool isHbmSiteActive = fbpNum1Active || fbpNum2Active;

        site.SetSiteActive(isHbmSiteActive);
        if (!isHbmSiteActive)
        {
            // Site is floorswept, skip
            continue;
        }

        rc = site.InitSiteInfo(m_pSubdev, sid, m_UseHostToJtag);
        if (rc == RC::OK)
        {
            CHECK_RC(PostProcessSiteInfo(&site, sid, &siteInfoCollected));
        }
        else if (rc == RC::PRIV_LEVEL_VIOLATION)
        {
            Printf(Tee::PriError, "HBM site %u: Insufficient permission, "
                   "unable to communicate.\n", sid);
        }

        firstRc = rc;
        rc.Clear();
    }

    if (firstRc == RC::PRIV_LEVEL_VIOLATION)
    {
        // WAR(2717197): Ampere HBM IEEE1500 registers are write
        // protected. Report that they should be using -fb_hbm_skip_init until
        // we have a better solution in place.
        Printf(Tee::PriWarn, "Unable to initialize HBM devices due to "
               "insufficient permissions. Use -fb_hbm_skip_init to skip HBM "
               "initialization.\n");

        return firstRc;
    }

    if (!siteInfoCollected)
    {
        // If no RC was reported, set to generic error
        firstRc = RC::SOFTWARE_ERROR;

        Printf(Tee::PriError, "Could not get HBM site specific info. "
               "HBM related functionality will be un-available.\n");
        return firstRc;
    }

    MASSERT(rc == RC::OK);
    return firstRc;
}

//!
//! \brief Determine the validity of the device ID information, and cache it in
//! the GPU subdevice.
//!
//! \param pSite HBM site object being initialized.
//! \param siteId HBM site number.
//! \param[out] pSuccessfullyCollected True if the HBM device ID was
//! succesfully collect, false o/w.
//!
RC HBMDevice::PostProcessSiteInfo(HBMSite* pSite, UINT32 siteId, bool* pSuccessfullyCollected)
{
    MASSERT(pSite);
    MASSERT(pSuccessfullyCollected);
    RC rc;

    const HbmDeviceIdData devIdData = pSite->GetSiteInfo();
    if (devIdData.IsInitialized())
    {
        // This site is active
        pSite->SetSiteActive(true);
        m_NumActiveSites++;
        *pSuccessfullyCollected = true;

        if (devIdData.IsDataValid())
        {
            m_StackHeight = devIdData.GetStackHeight();
            m_VendorID = devIdData.GetVendorId();

            // Cache HBM site info in GpuSubdevice
            GpuSubdevice::HbmSiteInfo hbmSiteInfo =
                {
                    devIdData.GetManufacturingYear(),
                    devIdData.GetManufacturingWeek(),
                    devIdData.GetManufacturingLoc(),
                    devIdData.GetModelPartNumber(),
                    devIdData.GetSerialNumber(),
                    devIdData.GetVendorName(),
                    devIdData.GetRevisionStr()
                };
            GetGpuSubdevice()->SetHBMSiteInfo(siteId, hbmSiteInfo);

            // Cache HBM dev id info in GpuSubdevice
            GpuSubdevice::HbmDeviceIdInfo hbmDeviceIdInfo =
                {
                    devIdData.GetEccSupported(),
                    devIdData.GetGen2Supported(),
                    devIdData.GetPseudoChannelSupport(),
                    devIdData.GetStackHeight()
                };
            GetGpuSubdevice()->SetHBMDeviceIdInfo(hbmDeviceIdInfo);

            if (Platform::GetSimulationMode() != Platform::Fmodel)
            {
                // Fail if the device ID data does not match a known HBM model.
                Memory::Hbm::Model hbmModel;
                CHECK_RC(GetHbmModel(&hbmModel));
            }
        }
        else
        {
            Printf(Tee::PriError, "Invalid HBM device information\n");
            MASSERT(!"Invalid HBM device information");
            return RC::SOFTWARE_ERROR;
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC HBMDevice::Initialize()
{
    // NOTE(stewarts): Can't use FB here. It is not available during this init
    // process.

    RC rc;
    StickyRC firstRc;

    if (m_Initialized)
        return RC::OK;

    if (Platform::IsVirtFunMode())
    {
        // In a VF. PF handles all the initialization. No need to do it here.
        m_Initialized = false;
        return RC::OK;
    }

    const UINT32 numHbmSites = m_pSubdev->GetNumHbmSites();
    MASSERT(numHbmSites > 0); // Must have some HBM to be initializing an HBM device.
    m_Sites.resize(numHbmSites);

    CHECK_RC(CollectSiteInfoFromHbm());

    // Sanity check. HBM of different types on the same chip is not supported
    if (!IsAllSameHbmModel(m_Sites))
    {
        Printf(Tee::PriError, "Mismatch in HBM models. This configuration is "
               "not supported.\n");
        MASSERT(!"Mismatch in HBM models. This configuration is not supported");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    // Init MBIST implementation for all sites
    for (UINT32 sid = 0; sid < numHbmSites; sid++)
    {
        HBMSite& site = m_Sites[sid];

        if (site.IsSiteActive())
        {
            switch (m_VendorID)
            {
            case HbmManufacturerId::Samsung:
                site.SetMBistImpl(new SamsungMBist(this, m_UseHostToJtag,
                                                           m_StackHeight));
                break;

            case HbmManufacturerId::SkHynix:
                site.SetMBistImpl(new SKHynixMBist(this, m_UseHostToJtag,
                                                           m_StackHeight));
                break;

            default:
            case HbmManufacturerId::Unknown:
                Printf(Tee::PriLow,
                       "Invalid HBM Vendor, MBIST functionality will be unavailable\n");
                break;
            }
        }
    }

    m_Initialized = true;
    return RC::OK;
}

//-----------------------------------------------------------------------------
//!
//! \brief Initialize the HBM device from pre-read DEVICE_ID data.
//!
//! \param hbmSiteDevIds Pre-read HBM site device IDs.
//!
RC HBMDevice::Initialize(const vector<Memory::DramDevId>& hbmSiteDevIds)
{
    // NOTE(stewarts): Can't use FB here. It is not available during this init
    // process.

    RC rc;
    StickyRC firstRc;

    if (m_Initialized)
        return RC::OK;

    if (Platform::IsVirtFunMode())
    {
        // In a VF. PF handles all the initialization. No need to do it here.
        m_Initialized = false;
        return RC::OK;
    }

    const UINT32 numHbmSites = m_pSubdev->GetNumHbmSites();
    MASSERT(numHbmSites > 0); // Must have some HBM to be initializing an HBM device.
    m_Sites.resize(numHbmSites);

    using HbmSiteId = Memory::Hbm::Site;
    std::set<HbmSiteId> sitesWithInfo;

    for (const Memory::DramDevId& siteDevId : hbmSiteDevIds)
    {
        const HbmSiteId siteId = Memory::Hbm::Site(siteDevId.index);

        sitesWithInfo.insert(siteId);

        HBMSite& site = m_Sites[siteId.Number()];
        site.SetSiteActive(true);
        CHECK_RC(site.InitSiteInfo(m_pSubdev, siteDevId.index,
                                   siteDevId.rawDevId));
        bool successfullyCollected = false;
        CHECK_RC(PostProcessSiteInfo(&site, siteDevId.index,
                                     &successfullyCollected));
        MASSERT(successfullyCollected);
    }

    // Confirm that the sites missing info are inactive and sites that have info
    // are active.
    for (HbmSiteId siteId(0); siteId.Number() < numHbmSites; ++siteId)
    {
        HBMSite* pSite = &m_Sites[siteId.Number()];
        (void)pSite;

        // Use site floorweeping to determine if active. Consider a site active
        // if at least one FBP is active.
        UINT32 fbpNum1 = 0;
        UINT32 fbpNum2 = 0;
        CHECK_RC(m_pSubdev->GetHBMSiteFbps(siteId.Number(), &fbpNum1, &fbpNum2));
        const UINT32 fbpFsMask = m_pSubdev->GetFsImpl()->FbpMask();
        const bool fbpNum1Active = (fbpFsMask & (1U << fbpNum1)) != 0;
        const bool fbpNum2Active = (fbpFsMask & (1U << fbpNum2)) != 0;
        const bool isHbmSiteActive = fbpNum1Active || fbpNum2Active;

        if (sitesWithInfo.count(siteId) > 0)
        {
            // Had data from PBI routine
            if (!isHbmSiteActive)
            {
                Printf(Tee::PriError, "VBIOS preOS PBI reported on inactive "
                       "HBM site %u\n", siteId.Number());
                return RC::SOFTWARE_ERROR;
            }

            MASSERT(pSite->IsSiteActive());
        }
        else
        {
            if (isHbmSiteActive)
            {
                Printf(Tee::PriError, "VBIOS preOS PBI did not report on "
                       "active HBM site %u\n", siteId.Number());
                return RC::SOFTWARE_ERROR;
            }

            MASSERT(!pSite->IsSiteActive());
        }
    }

    // Sanity check. HBM of different types on the same chip is not supported
    if (!IsAllSameHbmModel(m_Sites))
    {
        Printf(Tee::PriError, "Mismatch in HBM models. This configuration is "
               "not supported.\n");
        MASSERT(!"Mismatch in HBM models. This configuration is not supported");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    // Init MBIST implementation for all sites
    for (UINT32 sid = 0; sid < numHbmSites; sid++)
    {
        HBMSite& site = m_Sites[sid];

        if (site.IsSiteActive())
        {
            switch (m_VendorID)
            {
                case HbmManufacturerId::Samsung:
                    site.SetMBistImpl(new SamsungMBist(this, m_UseHostToJtag,
                                                       m_StackHeight));
                    break;

                case HbmManufacturerId::SkHynix:
                    site.SetMBistImpl(new SKHynixMBist(this, m_UseHostToJtag,
                                                       m_StackHeight));
                    break;

                default:
                case HbmManufacturerId::Unknown:
                    Printf(Tee::PriLow,
                           "Invalid HBM Vendor, MBIST functionality will be unavailable\n");
                    break;
            }
        }
    }

    m_Initialized = true;
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC HBMDevice::Shutdown()
{
    RC rc;

    if (m_Initialized)
    {
        for (UINT32 sid = 0; sid < m_pSubdev->GetNumHbmSites(); sid++)
        {
            CHECK_RC(m_Sites[sid].DeInitSite());
        }
    }

    m_Initialized = false;
    return rc;
}

//-----------------------------------------------------------------------------
RC HBMDevice::StartMBist(const UINT32 mbistType)
{
    RC rc;
    if (!m_Initialized)
    {
        Printf(Tee::PriError, "HBM device not initialized, can not start MBIST.\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    for (UINT32 sid = 0; sid < m_pSubdev->GetNumHbmSites(); sid++)
    {
        if (m_Sites[sid].IsSiteActive())
        {
            CHECK_RC(m_Sites[sid].StartMBistAtSite(sid, mbistType));
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC HBMDevice::CheckMBistCompletion(const  UINT32 mbistType)
{
    RC rc;
    for (UINT32 sid = 0; sid < m_pSubdev->GetNumHbmSites(); sid++)
    {
        if (m_Sites[sid].IsSiteActive())
        {
            CHECK_RC(m_Sites[sid].CheckMBistCompletionAtSite(sid, mbistType));
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC HBMDevice::AssertFBStop() const
{
    return m_pSubdev->GetHBMImpl()->AssertFBStop();
}

//-----------------------------------------------------------------------------
RC HBMDevice::DeAssertFBStop() const
{
    return m_pSubdev->GetHBMImpl()->DeAssertFBStop();
}

//-----------------------------------------------------------------------------
RC HBMDevice::RowRepair
(
    const bool isSoftRepair
    ,const vector<Memory::Row>& repairRows
    ,const RowErrorOriginTestIdMap& originTestMap
    ,const bool skipVerif
    ,const bool isPseudoRepair
)
{
    // TODO(stewarts): Include origin test in the MLE metadata.
    StickyRC firstRc;
    const FrameBuffer *pFb = m_pSubdev->GetFB();

    // Repair all rows. If a row cannot be repaired, continue repairing remaining rows.
    for (auto rowIt = repairRows.begin(); rowIt != repairRows.end(); ++rowIt)
    {
        RC rc;

        // Set the active test number to the test that found the memory error
        //
        const auto testIdIter = originTestMap.find(rowIt->rowName);
        if (testIdIter == originTestMap.end())
        {
            // All rows should have an entry in the origin test ID map, even if
            // that entry points to "unknown test ID".
            Printf(Tee::PriError, "Row not present in the origin test ID map: %s\n",
                   rowIt->rowName.c_str());
            MASSERT(!"Row not present in the origin test ID map");
            return RC::SOFTWARE_ERROR;
        }

        ErrorMap::SetTest(testIdIter->second);

        // Repair the row
        //
        UINT32 hbmChannel = 0;
        UINT32 hbmSite = 0;
        CHECK_RC(pFb->FbioSubpToHbmSiteChannel(rowIt->hwFbio.Number(),
                                               rowIt->subpartition.Number(),
                                               &hbmSite, &hbmChannel));

        if (m_Sites[hbmSite].IsSiteActive())
        {
            rc = m_Sites[hbmSite].RepairAtSite(isSoftRepair, isPseudoRepair, skipVerif,
                                               rowIt->rank.Number(),  // stackID = rank
                                               hbmChannel, rowIt->row, rowIt->bank.Number(),
                                               rowIt == repairRows.begin(), rowIt == repairRows.end());
            if (rc == RC::HBM_SPARE_ROWS_NOT_AVAILABLE)
            {
                firstRc = rc;
                continue;
            }
            CHECK_RC(rc);

            if (isSoftRepair)
            {
                m_LastSiteSWRepaired = hbmSite;
            }

            // Report successfully repaired rows
            JavaScriptPtr pJs;
            JsArray Args(1);
            CHECK_RC(pJs->ToJsval(rowIt->rowName, &Args[0]));
            CHECK_RC(pJs->CallMethod(pJs->GetGlobalObject(), "AddRepairedRow", Args, 0));
        }
        else
        {
            Printf(Tee::PriError, "Cannot repair row %s because Site %u is inactive\n",
                                  rowIt->rowName.c_str(), hbmSite);
            firstRc = RC::HBM_REPAIR_IMPOSSIBLE;
        }

        std::string ecid;
        CHECK_RC(m_pSubdev->GetEcid(&ecid));
        Mle::Print(Mle::Entry::hbm_row_repair_attempt)
            .status(firstRc.Get())
            .ecid(ecid)
            .name(rowIt->rowName)
            .stack(rowIt->rank.Number())
            .bank(rowIt->bank.Number())
            .row(rowIt->row)
            .hw_fbpa(rowIt->hwFbio.Number())
            .subp(rowIt->subpartition.Number())
            .site(hbmSite)
            .channel(hbmChannel)
            .interface(m_UseHostToJtag ? Mle::HbmRepair::host2jtag : Mle::HbmRepair::ieee1500)
            .hard_repair(!isSoftRepair)
            .skip_verif(skipVerif)
            .pseudo_repair(isPseudoRepair);
    }

    // Reset current test ID
    ErrorMap::SetTest(s_UnknownTestId);

    return firstRc;
}

//-----------------------------------------------------------------------------
RC HBMDevice::DumpDevIdData() const
{
    RC rc;

    for (UINT32 i = 0; i < m_Sites.size(); i++)
    {
        if (m_Sites[i].IsSiteActive())
        {
            CHECK_RC(m_Sites[i].DumpDevIdData());
            std::string ecid;
            CHECK_RC(m_pSubdev->GetEcid(&ecid));
            Mle::Print(Mle::Entry::hbm_site)
                .ecid(ecid)
                .site_id(i)
                .gen2(m_Sites[i].GetSiteInfo().GetGen2Supported())
                .ecc(m_Sites[i].GetSiteInfo().GetEccSupported())
                .density_per_chan(m_Sites[i].GetSiteInfo().GetDensityPerChanGb())
                .mfg_id(static_cast<UINT08>(m_Sites[i].GetSiteInfo().GetVendorId()))
                .mfg_location(m_Sites[i].GetSiteInfo().GetManufacturingLoc())
                .mfg_year(m_Sites[i].GetSiteInfo().GetManufacturingYear())
                .mfg_week(m_Sites[i].GetSiteInfo().GetManufacturingWeek())
                .serial_num(m_Sites[i].GetSiteInfo().GetSerialNumber())
                .addr_mode(static_cast<Mle::HbmSite::AddressMode>(m_Sites[i].GetSiteInfo().GetAddressModeVal()))
                .channels_avail(m_Sites[i].GetSiteInfo().GetChannelsAvailVal())
                .stack_height(m_Sites[i].GetSiteInfo().GetStackHeight())
                .model_part_num(m_Sites[i].GetSiteInfo().GetModelPartNumber());
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC HBMDevice::DumpRepairedRows() const
{
    RC rc;
    Printf(Tee::PriNormal, "HBM Repaired Rows Summary:\n");
    for (UINT32 i = 0; i < m_Sites.size(); i++)
    {
        if (m_Sites[i].IsSiteActive())
        {
            CHECK_RC(m_Sites[i].DumpRepairedRows());
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Collect the lane repair data from the HBM fuses.
//!
//! \param pOutLaneRepairs: Populate given vector with the HBM fuse repair data.
//!
RC HBMDevice::GetRepairedHbmLanes
(
    vector<LaneError>* const pOutLaneRepairs
) const
{
    RC rc;
    MASSERT(pOutLaneRepairs);

    for (UINT32 i = 0; i < m_Sites.size(); i++)
    {
        if (m_Sites[i].IsSiteActive())
        {
            CHECK_RC(m_Sites[i].GetRepairedLanes(pOutLaneRepairs));
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Collect the lane repair data from the GPU.
//!
//! \param pOutLaneRepairs: Populate given vector with the GPU repair data.
//!
RC HBMDevice::GetRepairedGpuLanes
(
    vector<LaneError>* const pOutLaneRepairs
) const
{
    RC rc;
    MASSERT(pOutLaneRepairs);
    MASSERT(!Platform::IsVirtFunMode());

    const FloorsweepImpl *pFsImpl = m_pSubdev->GetFsImpl();
    const FrameBuffer* const pFb = m_pSubdev->GetFB();
    const UINT32 numBitsPerDword = 32;

    const UINT32 fbioMask = pFsImpl->FbioMask();
    const UINT32 maxFbioCount = pFb->GetMaxFbioCount();
    const UINT32 numSubp = pFb->GetSubpartitions();
    const UINT32 dwordsPerSubp = pFb->GetSubpartitionBusWidth() / numBitsPerDword;

    for (UINT32 hwFbio = 0; hwFbio < maxFbioCount; hwFbio++)
    {
        if (fbioMask & (1 << hwFbio))
        {
            for (UINT32 subp = 0; subp < numSubp; subp++)
            {
                for (UINT32 fbpaDword = 0; fbpaDword < dwordsPerSubp; fbpaDword++)
                {
                    const UINT32 hwFbpa = pFb->HwFbioToHwFbpa(hwFbio);
                    CHECK_RC(GetHBMImpl()->GetLaneRepairsFromLinkRepairReg(hwFbpa, subp, fbpaDword,
                                                                           pOutLaneRepairs));
                }
            }
        }
    }

    return rc;
}

//!
//! \brief Return the index of the byte pair that contains a repair in the given
//! remap value.
//!
//! Index starts at the least signifiant byte pair.
//!
UINT32 HBMDevice::RemapValueBytePairIndex(UINT32 remapValue)
{
    // Must be some repair
    MASSERT((remapValue & HBM_REMAP_DWORD_NO_REPAIR) != HBM_REMAP_DWORD_NO_REPAIR);
    UINT32 mask;

    mask = HBM_REMAP_DWORD_BYTE_PAIR_NO_REPAIR;
    const bool isPair0 = (remapValue & mask) != mask;
    mask = HBM_REMAP_DWORD_BYTE_PAIR_NO_REPAIR << HBM_REMAP_DWORD_BYTE_PAIR_ENCODING_SIZE;
    const bool isPair1 = (remapValue & mask) != mask;

    // Should only have one repair
    MASSERT(!(isPair0 && isPair1));
    (void)isPair1; // Workaround for unused var warning on release builds

    return (isPair0 ? 0 : 1);
}

//-----------------------------------------------------------------------------
//!
//! \brief Callwlate mask and remapping value for programming HBM.
//!
//! See the table title "LANE_REPAIR Wrapper Data Register" in the Micron HBM
//! Specification document for the data format.
//!
//! \param laneType Lane type to be remapped.
//! \param laneBit Lane to be remapped.
//! \param[out] pOutMask Mask representing the portion of the given remap value
//! that is relevent to this repair.
//! \param[out] pOutRemapValue Remap value.
//!
RC HBMDevice::CalcLaneMaskAndRemapValue
(
    LaneError::RepairType laneType
    ,UINT32 laneBit
    ,UINT16* const pOutMask
    ,UINT16* const pOutRemapValue
)
{
    RC rc;
    MASSERT(pOutMask);
    MASSERT(pOutRemapValue);

    UINT32 laneRemapVal = 0;
    switch (laneType)
    {
        case LaneError::RepairType::DM:
            laneRemapVal = 0;
            break;

        case LaneError::RepairType::DATA:
            laneRemapVal = (laneBit % 8) + 1;
            break;

        case LaneError::RepairType::DBI:
            laneRemapVal = 9;
            break;

        default:
            Printf(Tee::PriError, "LaneRepair is not implemented for type: %s\n",
                LaneError::GetLaneTypeString(laneType).c_str());
            CHECK_RC(RC::UNSUPPORTED_FUNCTION);
    }

    const UINT32 dwordSize = 32;
    const UINT32 byteSize = 8;
    const UINT32 byte = (laneBit % dwordSize) / byteSize;
    const UINT16 shift = 4 * (byte % 2);

    UINT16 mask = 0xFF;
    UINT16 remapVal = (0xE0 >> shift) | (laneRemapVal << shift);
    if (byte >= 2)
    {
        mask <<= 8;
        remapVal <<= 8;
    }

    *pOutMask = mask;
    *pOutRemapValue = remapVal;

    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Populates the InfoROM RPR Soft Repair Object with the HBM fuse data.
//!
//! NOTE: Soft lane repair data does not appear in the HBM fuses, and hence will
//! not be reported here.
//!
RC HBMDevice::PopulateInfoRomSoftRepairObj() const
{
    RC rc;

    LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS ifrParams = { 0 };

    Printf(Tee::PriNormal,
           "Populating InfoROM RPR Soft Repair Object with HBM fuse data...\n");

    // Collect lane repair data
    //
    CHECK_RC(CollectInfoRomLaneRepairData(&ifrParams));

    // Add the bypass bit
    //
    // Without the bypass bit, setting the HBM_LINK_REPAIR registers will not
    // override the default GPU remapping or the values set by the fuseless
    // fuses.
    //
    for (UINT32 i = 0; i < ifrParams.entryCount; ++i)
    {
        LW90E7_RPR_INFO* entry = &ifrParams.repairData[i];
        entry->data |= FBIO_HBM_LINK_REPAIR_BYPASS_BIT;
    }

    // Store repair data in the InfoROM
    //
    CHECK_RC(WriteInfoRom(&ifrParams));

    Printf(Tee::PriNormal,
           "InfoROM RPR Soft Repair Object populated. GPU will be remapped "
           "accordingly during the next boot.\n");

    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Clear the InfoROM RPR Soft Repair Object.
//!
//! Clears the object to zero.
//!
RC HBMDevice::ClearInfoRomSoftSoftRepairObj() const
{
    RC rc;

    Printf(Tee::PriNormal, "Clearing InfoROM...\n");

    LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS ifrParams = { 0 };
    ifrParams.entryCount = 0; // Sanity

    // Set the InfoROM to have zero entries
    // This clears the InfoROM to zeros.
    CHECK_RC(WriteInfoRom(&ifrParams));

    Printf(Tee::PriNormal,
           "InfoROM RPR Soft Repair Object cleared. GPU will be remapped "
           "accordingly during the next boot.\n");

    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Validate the contents of the InfoROM RPR Soft Repair Object against
//! the HBM fuse data.
//!
//! Prints mismatches between the repair data and the InfoROM.
//!
RC HBMDevice::ValidateInfoRomSoftRepairObj() const
{
    RC rc;

    Printf(Tee::PriNormal, "Validating InfoROM...\n");

    LwRmPtr pLwRm;
    LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS hbmFuses = { 0 };
    CHECK_RC(CollectInfoRomLaneRepairData(&hbmFuses));

    LW90E7_CTRL_RPR_GET_INFO_PARAMS infoRom = { 0 };
    CHECK_RC(ReadInfoRom(&infoRom));

    // Check the difference between the objects
    //
    if (hbmFuses.entryCount != infoRom.entryCount)
    {
        Printf(Tee::PriError, "Entry count mismatch:\n"
               "HBM Repair data               : %u\n"
               "InfoROM RPR Soft Repair Object: %u\n",
               hbmFuses.entryCount, infoRom.entryCount);
        rc = RC::ILWALID_INFO_ROM;
    }

    typedef LwU32 Address;
    typedef LwU32 Data;
    std::map<Address, Data> repairDataMap;

    // Insert HBM repair info
    for (LwU32 i = 0; i < hbmFuses.entryCount; ++i)
    {
        LW90E7_RPR_INFO* entry = &hbmFuses.repairData[i];
        repairDataMap[entry->address] = entry->data;
    }

    string mismatchedEntries;
    string falseInfoRomEntries;
    string missingInfoRomEntries;
    bool isBypassBitMissing = false;

    // Compare the InfoROM repair into against the HBM
    for (LwU32 i = 0; i < infoRom.entryCount; ++i)
    {
        LW90E7_RPR_INFO* entry = &infoRom.repairData[i];
        auto iter = repairDataMap.find(entry->address);

        if (!(entry->data & FBIO_HBM_LINK_REPAIR_BYPASS_BIT))
        {
            isBypassBitMissing = true;
            rc = RC::ILWALID_INFO_ROM;
            continue;
        }

        if (iter == repairDataMap.end())
        {
            // Address in InfoROM not present in HBM repair data
            falseInfoRomEntries += Utility::StrPrintf("0x%08x 0x%08x\n",
                                                      entry->address, entry->data);

            rc = RC::ILWALID_INFO_ROM;
            continue;
        }

        // Ignore the bypass bit
        // The InfoROM should have it and the HBM data shouldn't.
        if ((iter->second | FBIO_HBM_LINK_REPAIR_BYPASS_BIT) != entry->data)
        {
            // Address in InfoROM does not have the same value as in the HBM
            mismatchedEntries += Utility::StrPrintf("0x%08x 0x%08x 0x%08x\n",
                                                    entry->address,
                                                    iter->second,
                                                    entry->data);

            rc = RC::ILWALID_INFO_ROM;
        }

        // Remove entry from the map
        repairDataMap.erase(iter);
    }

    // Display HBM entries that were not in the InfoROM data
    if (!repairDataMap.empty())
    {
        rc = RC::ILWALID_INFO_ROM;

        for (auto iter = repairDataMap.begin(); iter != repairDataMap.end(); ++iter)
        {
            missingInfoRomEntries += Utility::StrPrintf("0x%08x 0x%08x\n",
                                                        iter->first, iter->second);
        }
    }

    // Print results
    //
    if (isBypassBitMissing)
    {
        Printf(Tee::PriError,
               "Bypass bit was not set on all the InfoROM entries\n");
    }
    if (!falseInfoRomEntries.empty())
    {
        Printf(Tee::PriError,
               "InfoROM entry not present in HBM repair data:\n"
               "%10s %10s\n%s\n",
               "Address", "Data",
               falseInfoRomEntries.c_str());
    }
    if (!missingInfoRomEntries.empty())
    {
        Printf(Tee::PriError,
               "HBM repair data missing from InfoROM:\n"
               "%10s %10s\n%s\n",
               "Address", "Data",
               missingInfoRomEntries.c_str());
    }
    if (!mismatchedEntries.empty())
    {
        Printf(Tee::PriError,
               "Remap value mismatch for address:\n"
               "%9s: %10s %10s\n%s\n",
               "Address", "HBM", "InfoROM",
               mismatchedEntries.c_str());
    }

    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Print the contents of the InfoROM RPR Soft Repair Object.
//!
RC HBMDevice::PrintInfoRomSoftRepairObj() const
{
    RC rc;

    Printf(Tee::PriNormal, "Dumping InfoROM RPR Soft Repair Object...\n");

    LW90E7_CTRL_RPR_GET_INFO_PARAMS ifrData;
    CHECK_RC(ReadInfoRom(&ifrData));

    const UINT32 entryCount = ifrData.entryCount;

    Printf(Tee::PriNormal, "Entry count: %u\n", entryCount);

    UINT32 rprMaxCount = 0;
    CHECK_RC(m_pSubdev->GetInfoRomRprMaxDataCount(&rprMaxCount));

    if (entryCount > rprMaxCount)
    {
        Printf(Tee::PriWarn,
               "InfoROM corruption: Entry count is greater than the maximum possible entries\n");
        return RC::ILWALID_INFO_ROM;
    }

    if (entryCount == 0)
    {
        Printf(Tee::PriNormal, "\tNO REPAIRS\n");
    }
    else
    {
        // Print the address-data pairs
        //
        Printf(Tee::PriNormal, "%10s %10s\n", "Address", "Data");
        for (UINT32 i = 0; i < entryCount; ++i)
        {
            LW90E7_RPR_INFO* entry = &ifrData.repairData[i];
            Printf(Tee::PriNormal, "0x%08x 0x%08x\n", entry->address, entry->data);
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Get site and channel associated with the given lane error.
//!
RC HBMDevice::GetHbmSiteChannel
(
    const LaneError& laneError,
    UINT32* pOutSite
    ,UINT32* pOutChannel
) const
{
    RC rc;
    MASSERT(pOutSite);
    MASSERT(pOutChannel);

    const FrameBuffer* const pFb = m_pSubdev->GetFB();
    const UINT32 subp = GetSubpartition(laneError);
    const UINT32 hwFbio = pFb->HwFbpaToHwFbio(laneError.hwFbpa.Number());
    CHECK_RC(pFb->FbioSubpToHbmSiteChannel(hwFbio, subp, pOutSite, pOutChannel));

    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Return the HBM remap value that would be used to repair the given
//! lane error.
//!
UINT32 HBMDevice::GetRemapValue(const LaneError& laneError) const
{
    UINT32 remapVal;

    const UINT32 dwordSize = 32;
    const UINT32 byteSize = 8;
    const UINT32 byteNum = (laneError.laneBit % dwordSize) / byteSize;

    // Find the remap encoding for the repair
    //
    UINT08 remapEncoding = 0xff;
    switch (laneError.laneType)
    {
    case LaneError::Type::DM:
        remapEncoding = 0;
        break;
    case LaneError::Type::DATA:
        // Encoding is the failing bit in the byte plus one
        remapEncoding = (laneError.laneBit % byteSize) + 1;
        break;
    case LaneError::Type::DBI:
        remapEncoding = 9;
        break;
    default:
        MASSERT(!"Unsupported HBM fuse DWORD remap value type\n");
        break;
    }

    // Create byte pair remap value
    //
    UINT08 bytePair;
    const bool isLowerByteInPair = (byteNum % 2 == 0);
    if (isLowerByteInPair)
    {
        bytePair =
            (HBM_REMAP_DWORD_BYTE_REPAIR_IN_OTHER_BYTE << HBM_REMAP_DWORD_BYTE_ENCODING_SIZE)
            | remapEncoding;
    }
    else
    {
        bytePair =
            HBM_REMAP_DWORD_BYTE_REPAIR_IN_OTHER_BYTE
            | (remapEncoding << HBM_REMAP_DWORD_BYTE_ENCODING_SIZE);
    }

    // Create full remap value
    //
    const bool isLowerBytePair = byteNum < 2;
    if (isLowerBytePair)
    {
        remapVal =
            (HBM_REMAP_DWORD_BYTE_PAIR_NO_REPAIR << HBM_REMAP_DWORD_BYTE_PAIR_ENCODING_SIZE)
            | bytePair;
    }
    else
    {
        remapVal =
            HBM_REMAP_DWORD_BYTE_PAIR_NO_REPAIR
            | bytePair << HBM_REMAP_DWORD_BYTE_PAIR_ENCODING_SIZE;
    }

    return remapVal;
}

//-----------------------------------------------------------------------------
//!
//! \brief Return the zero-indexed subpartition that contains the given error.
//!
UINT32 HBMDevice::GetSubpartition(const LaneError& laneError) const
{
    const FrameBuffer* const pFb = GetGpuSubdevice()->GetFB();
    return laneError.laneBit / pFb->GetSubpartitionBusWidth();
}

//-----------------------------------------------------------------------------
//!
//! \brief Get the model of HBM the HBM device represents.
//!
//! \param[out] pHbmModel HBM model.
//!
RC HBMDevice::GetHbmModel(Memory::Hbm::Model* pHbmModel) const
{
    MASSERT(pHbmModel);
    MASSERT(IsAllSameHbmModel(m_Sites));

    for (const HBMSite& site : m_Sites)
    {
        if (site.IsSiteActive())
        {
            return site.GetSiteInfo().GetHbmModel(pHbmModel);
        }
    }

    // Could not find any active sites
    Printf(Tee::PriLow, "No HBM sites active, can't get HBM model information.\n");
    return RC::UNSUPPORTED_HARDWARE_FEATURE;
}

//-----------------------------------------------------------------------------
//!
//! \brief Print the repaired lanes.
//!
//! \param readHBMData If true, read the repaired lanes from the HBM fuse data.
//! Otherwise read the repair data from the GPU.
//!
RC HBMDevice::DumpRepairedLanes(bool readHBMData) const
{
    RC rc;

    vector<LaneError> laneRepairs;
    string s;
    bool isSrcFromGpu = true;
    // Collect lane repair data
    //
    if (!readHBMData)
    {
        CHECK_RC(GetRepairedGpuLanes(&laneRepairs));
        s += "== HBM Repaired Lanes Summary - sourced from GPU:\n";
    }
    else
    {
        isSrcFromGpu = false;
        CHECK_RC(GetRepairedHbmLanes(&laneRepairs));
        s += "== HBM Repaired Lanes Summary - sourced from HBM fuses:\n";
    }

    std::string ecid;
    CHECK_RC(m_pSubdev->GetEcid(&ecid));

    // Print the repair information
    //
    std::sort(laneRepairs.begin(), laneRepairs.end());
    for (const LaneError& lane : laneRepairs)
    {
        s += lane.ToString();
        s += '\n';

        Mle::Print(Mle::Entry::hbm_repaired_lane)
            .ecid(ecid)
            .name(Utility::StrPrintf("%c", 'A' + lane.hwFbpa.Number()))
            .hw_fbpa(lane.hwFbpa.Number())
            .fbpa_lane(lane.laneBit)
            .type(Mle::ToLane(lane.laneType))
            .repair_source(isSrcFromGpu ? Mle::HbmRepair::gpu : Mle::HbmRepair::hbm_fuses);
    }

    const UINT32 numLaneRepairs = static_cast<UINT32>(laneRepairs.size());
    MASSERT(numLaneRepairs == laneRepairs.size());
    Printf(Tee::PriNormal, "%sTotal lane repairs: %u\n\n", s.c_str(),
           numLaneRepairs);

    return rc;
}

//-----------------------------------------------------------------------------
RC HBMDevice::LaneRepair
(
    const bool isSoftRepair
    ,const vector<LaneError>& repairLanes
    ,bool skipVerif
    ,bool isPseudoRepair
)
{
    StickyRC firstRc;
    const FrameBuffer *pFb = m_pSubdev->GetFB();

    // Repair all lanes. If a lane cannot be repaired, continue
    // repairing remaining lanes.
    for (auto laneIt = repairLanes.begin(); laneIt != repairLanes.end(); ++laneIt)
    {
        RC rc;
        const UINT32 subp = GetSubpartition(*laneIt);

        UINT32 hbmChannel = 0;
        UINT32 hbmSite = 0;
        CHECK_RC(pFb->FbioSubpToHbmSiteChannel(pFb->HwFbpaToHwFbio(laneIt->hwFbpa.Number()),
                                               subp,
                                               &hbmSite, &hbmChannel));

        if (m_Sites[hbmSite].IsSiteActive())
        {
            // NOTE: Works under the assumption that there is a 1:1
            // ratio FBPA:FBIO.
            firstRc = m_Sites[hbmSite].RepairLaneAtSite(isSoftRepair, skipVerif,
                                                        isPseudoRepair,
                                                        hbmChannel, *laneIt);
        }
        else
        {
            Printf(Tee::PriError, "Cannot repair lane %s because Site %u is inactive\n",
                   laneIt->ToString().c_str(), hbmSite);
            firstRc = RC::HBM_REPAIR_IMPOSSIBLE;
        }
    }

    return firstRc;
}

//-----------------------------------------------------------------------------
RC HBMDevice::DidJtagResetOclwrAfterSWRepair(bool *pReset) const
{
    RC rc;
    if (m_LastSiteSWRepaired != -1)
    {
        CHECK_RC(m_Sites[m_LastSiteSWRepaired].DidJtagResetOclwrAfterSWRepair(pReset));
    }
    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Fill the given struct with the contents of the Inforom RPR Soft
//! Repair Object.
//!
RC HBMDevice::ReadInfoRom
(
    LW90E7_CTRL_RPR_GET_INFO_PARAMS* pInfoRomParams
) const
{
    RC rc;

    LwRmPtr pLwRm;

    rc = pLwRm->ControlSubdeviceChild(m_pSubdev,
                                      GF100_SUBDEVICE_INFOROM,
                                      LW90E7_CTRL_CMD_RPR_GET_INFO,
                                      pInfoRomParams,
                                      sizeof(LW90E7_CTRL_RPR_GET_INFO_PARAMS));

    switch (rc)
    {
    case RC::LWRM_NOT_SUPPORTED:
        Printf(Tee::PriWarn,
               "InfoROM RPR Soft Repair Object is not present\n");
        break;
    case RC::LWRM_ILWALID_STATE:
        Printf(Tee::PriWarn,
               "InfoROM RPR Soft Repair Object is corrupted\n");
        break;
    }

    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Write the given data to the InfoROM RPR Soft Repair Object.
//!
RC HBMDevice::WriteInfoRom
(
    LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS* pInfoRomParams
) const
{
    RC rc;

    LwRmPtr pLwRm;

    rc = pLwRm->ControlSubdeviceChild(m_pSubdev,
                                      GF100_SUBDEVICE_INFOROM,
                                      LW90E7_CTRL_CMD_RPR_WRITE_OBJECT,
                                      pInfoRomParams,
                                      sizeof(LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS));

    switch (rc)
    {
    case RC::LWRM_NOT_SUPPORTED:
        Printf(Tee::PriWarn,
               "InfoROM RPR Soft Repair Object is not valid/present\n");
        break;
    case RC::LWRM_STATUS_ERROR_ILWALID_ADDRESS:
        Printf(Tee::PriWarn,
               "InfoROM RPR Soft Repair Object input address is invalid\n");
        break;
    }

    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Fill the given InfoROM RPR Soft Repair Object parameters with the HBM
//! fuse data.
//!
RC HBMDevice::CollectInfoRomLaneRepairData
(
    LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS* pOutIfrParams
) const
{
    RC rc;
    vector<LaneError> laneRepairs;

    // Collect lane repair data
    //
    CHECK_RC(GetRepairedHbmLanes(&laneRepairs));

    // Associate repair data with link repair registers
    //
    map<UINT32, UINT32> regVals; // register address -> remap value

    for (const auto& laneError : laneRepairs)
    {
        UINT32 remapVal = GetRemapValue(laneError);
        const UINT32 bytePairIndex = HBMDevice::RemapValueBytePairIndex(remapVal);

        UINT32 regAddr;
        CHECK_RC(m_pSubdev->GetHbmLinkRepairRegister(laneError, &regAddr));

        // Check if there exists a remap value for this register address
        auto entry = regVals.find(regAddr);
        if (entry != regVals.end())
        {
            // Check if it can be repaired
            const UINT32 shift = bytePairIndex * 8;
            const UINT32 existingRemapVal = entry->second;
            if (((existingRemapVal >> shift) & 0xff) == 0xff)
            {
                // Repair can fit in the existing remap entry, merge them
                remapVal = existingRemapVal & remapVal;
            }
            else
            {
                Printf(Tee::PriError, "Multiple repairs found for the same byte pair\n");
                return RC::HBM_REPAIR_IMPOSSIBLE;
            }
        }

        // Record the remapping
        regVals[regAddr] = remapVal;
    }

    // Check if we have exceeded the maximum number of InfoROM entries
    const UINT32 maxInfoRomRepairEntries = LW90E7_CTRL_RPR_MAX_DATA_COUNT;
    if (regVals.size() > maxInfoRomRepairEntries)
    {
        Printf(Tee::PriError,
               "More repair entries required than total InfoROM RPR Soft "
               "Repair Object entries\n\tentries/max: %u/%u\n",
               static_cast<UINT32>(regVals.size()), maxInfoRomRepairEntries);
        return RC::HBM_REPAIR_IMPOSSIBLE;
    }

    // Populate the lane repair InfoROM struct
    //
    UINT32 ifrEntryIndex = 0;
    for (const auto& entry : regVals)
    {
        LW90E7_RPR_INFO* const pIfrEntry = &(pOutIfrParams->repairData[ifrEntryIndex]);
        ifrEntryIndex++;

        pIfrEntry->address = entry.first;
        pIfrEntry->data    = entry.second;
    }
    pOutIfrParams->entryCount = ifrEntryIndex;

    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Return true if the given HBM sites are all the same model, false
//! otherwise.
//!
//! \param sites HBM sites to check.
//!
bool HBMDevice::IsAllSameHbmModel(const vector<HBMSite>& sites) const
{
    MASSERT(sites.size() >= 2);

    for (UINT32 i = 0; i < sites.size() - 1; ++i)
    {
        // Only compare active sites
        if (!sites[i].IsSiteActive()) { continue; }

        // Find another active site
        UINT32 nextI = i + 1;
        while (nextI < sites.size())
        {
            if (sites[nextI].IsSiteActive())
            {
                if (!sites[i].GetSiteInfo().IsSameHbmModel(sites[nextI].GetSiteInfo()))
                {
                    return false;
                }

                break; // Move to next active site
            }

            nextI++;
        }

        i = nextI;
    }

    return true;
}

//-----------------------------------------------------------------------------
RC HBMDevice::GetLaneRemappingValues
(
    Gpu::LwDeviceId deviceId
    ,const vector<LaneError>& laneErrors
)
{
    RC rc;

    DramHalChipConfV1 chipConf;
    // Either HBM1 or HBM2 works
    chipConf.dramType = DRAM_TYPE_HBM2;
    // Values gathered from gpulist.h
    switch (deviceId)
    {
        case Gpu::GP100:
            chipConf.litter = LITTER_GMLIT4;
            break;
        case Gpu::GV100:
            chipConf.litter = LITTER_GVLIT1;
            break;
        default:
            Printf(Tee::PriError, "DeviceId not valid.\n");
            return RC::UNSUPPORTED_FUNCTION;
    }

    // Subpartition Bus Width is determined by:
    // (PseudoChannels per Channel) * (Beat Size) * (Bits per byte)
    // RAM Protocol | PseudoChannels | Beat Size
    // -----------------------------------------
    // HBM1         | 1              | 16
    // HBM2         | 2              | 8

    const UINT32 subpSize  = 128;
    const UINT32 dwordSize = 32;
    const UINT32 byteSize  = 8;

    for (LaneError laneError : laneErrors)
    {
        // Get subpartition
        UINT32 subp = laneError.laneBit / subpSize;
        // HwFbio is 1 to 1 with HwFbpa for all post-Maxwell GPUs
        UINT32 hwFbio = laneError.hwFbpa.Number();

        // Get hbmSite and hbmChannel from hwFbio and subpartition
        UINT32 hbmSite, hbmChannel;
        mapHwFbioSubpToFbioBrickIfV1(&chipConf, hwFbio, subp, &hbmSite, &hbmChannel);

        // Callwlate Dword and Byte for the specific lanebit
        UINT32 dword = ((laneError.laneBit % subpSize) / dwordSize);
        UINT32 byte  = ((laneError.laneBit % dwordSize) / byteSize);

        // Swizzling needed for GP100
        if (deviceId == Gpu::GP100)
        {
            if (hbmSite == 1 || hbmSite == 3)
            {
                dword = 3 - dword;
            }
        }

        // Callwlate lane mask and remap value
        UINT16 mask, remap;
        CHECK_RC(CalcLaneMaskAndRemapValue(laneError.laneType,
                                           laneError.laneBit,
                                           &mask, &remap));

        // Print results
        Printf(Tee::PriNormal,
            "Remap information for lane %s\n"
            "  GPU location: HW FBPA %u, subpartition %u, FBPA lane %u\n"
            "  HBM location: site %u, channel %u, dword %u, byte %u\n"
            "  Byte pair remap value: 0x%04x, mask 0x%04x\n",
            laneError.ToString().c_str(),
            laneError.hwFbpa.Number(), subp, laneError.laneBit,
            hbmSite, hbmChannel, dword, byte,
            remap, mask);
    }
    return rc;
}

//-----------------------------------------------------------------------------
HBMDevMgr::HBMDevMgr()
: m_Initialized(false)
, m_InitMethod(Gpu::InitHbmTempDefault)
{
}

//-----------------------------------------------------------------------------
RC HBMDevMgr::FindDevices()
{
    RC rc;

    // Create a HBM device per GPU
    GpuDevMgr* pGpuDevMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
    for (GpuSubdevice *pSubdev = pGpuDevMgr->GetFirstGpu();
        pSubdev != NULL; pSubdev = pGpuDevMgr->GetNextGpu(pSubdev))
    {
        if (pSubdev->GetHBMImpl() != nullptr)
        {
            HBMDevicePtr hbmdevPtr(new HBMDevice(pSubdev));
            m_Devices.push_back(hbmdevPtr);
            pSubdev->GetHBMImpl()->SetHBMDev(hbmdevPtr);
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC HBMDevMgr::InitializeAll()
{
    RC rc;
    if (m_Initialized)
    {
        return rc;
    }

    for (UINT32 i = 0; i < m_Devices.size(); i++)
    {
        if (m_InitMethod == Gpu::InitHbmHostToJtag)
        {
            m_Devices[i]->ForceHostToJtag();
        }

        CHECK_RC(m_Devices[i]->Initialize());
    }
    m_Initialized = true;
    return rc;
}

//-----------------------------------------------------------------------------
//!
//! \brief Initialize all HBM devices from pre-read HBM DEVICE_ID data.
//!
//! \param allGpusHbmSiteDevIds Pre-read HBM site device IDs for each GPU
//! subdevice. If an entry for a GPU subdevice is missing, reads device IDs from
//! the HBM.
//!
RC HBMDevMgr::InitializeAll(const vector<GpuDramDevIds>& allGpusHbmSiteDevIds)
{
    RC rc;
    if (m_Initialized)
    {
        return rc;
    }

    for (UINT32 i = 0; i < m_Devices.size(); i++)
    {
        HBMDevicePtr& pHbmDev = m_Devices[i];

        if (m_InitMethod == Gpu::InitHbmHostToJtag)
        {
            pHbmDev->ForceHostToJtag();
        }

        const GpuSubdevice* pSubdev = pHbmDev->GetGpuSubdevice();
        auto gpuDevIdsIter = std::find_if(allGpusHbmSiteDevIds.begin(),
                                          allGpusHbmSiteDevIds.end(),
                                          [pSubdev](const GpuDramDevIds& a)
                                          {
                                              return a.pSubdev == pSubdev;
                                          });
        if (gpuDevIdsIter == allGpusHbmSiteDevIds.end())
        {
            Printf(Tee::PriError, "No HBM device information for GPU to "
                   "initialize HBM devices\n");
            return RC::WAS_NOT_INITIALIZED;
        }

        if (gpuDevIdsIter->hasData)
        {
            CHECK_RC(pHbmDev->Initialize(gpuDevIdsIter->dramDevIds));
        }
        else
        {
            CHECK_RC(pHbmDev->Initialize());
        }
    }

    m_Initialized = true;
    return rc;
}


//-----------------------------------------------------------------------------
RC HBMDevMgr::ShutdownAll()
{
    StickyRC rc;

    for (UINT32 i = 0; i < m_Devices.size(); i++)
    {
        if (m_Initialized)
        {
            rc = m_Devices[i]->Shutdown();
        }
        m_Devices[i].reset();
    }

    m_Initialized = false;
    return rc;
}

//-----------------------------------------------------------------------------
UINT32 HBMDevMgr::NumDevices()
{
    return static_cast<UINT32>(m_Devices.size());
}

//-----------------------------------------------------------------------------
RC HBMDevMgr::GetDevice(UINT32 index, Device **pDev)
{
    RC rc;

    if (index >= NumDevices())
    {
        Printf(Tee::PriError, "Trying to access a non-existent HBM device idx: %u\n", index);
        CHECK_RC(RC::ILWALID_DEVICE_ID);
    }

    *pDev = m_Devices[index].get();

    return rc;
}

//--------------------------- HBM device JS Interface -------------------------

static void C_JsBridge_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsHBMDev * pJsHBMDev;
    //! Delete the C++
    pJsHBMDev = (JsHBMDev *)JS_GetPrivate(cx, obj);
    delete pJsHBMDev;
};

//-----------------------------------------------------------------------------
static JSClass JsHBMDev_class =
{
    "HBMDev",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsBridge_finalize
};

//-----------------------------------------------------------------------------
static SObject JsHBMDev_Object
(
    "HBMDev",
    JsHBMDev_class,
    0,
    0,
    "HBMDev JS Object"
);

JsHBMDev::JsHBMDev()
    : m_pJsHBMDevObj(nullptr)
{
    m_pHBMDev.reset();
}

//-----------------------------------------------------------------------------
RC JsHBMDev::CreateJSObject(JSContext *cx, JSObject *obj)
{
    //! Only create one JSObject per Perf object
    RC rc;
    if (m_pJsHBMDevObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this HBM device.\n");
        return OK;
    }

    m_pJsHBMDevObj = JS_DefineObject(cx,
                        obj, // HBMDevice object
                        "HBMDev", // Property name
                        &JsHBMDev_class,
                        JsHBMDev_Object.GetJSObject(),
                        JSPROP_READONLY);

    if (!m_pJsHBMDevObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsBridge instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsHBMDevObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsHBMDev.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    JS_DefineFunction(cx, m_pJsHBMDevObj, "Help", &C_Global_Help, 1, 0);
    return rc;
}

//------------------------------------------------------------------------------
HBMDevicePtr JsHBMDev::GetHBMDev()
{
    return m_pHBMDev;
}

//------------------------------------------------------------------------------
void JsHBMDev::SetHBMDev(HBMDevicePtr pHBMDev)
{
    m_pHBMDev = pHBMDev;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsHBMDev,
                StartMBist,
                1,
                "Start MBIST")
{
    STEST_HEADER(1, 1, "Usage: HBMDev.StartMBist(mbistType).");
    STEST_ARG(0, UINT32, mbistType);
    RC rc;
    STEST_PRIVATE(JsHBMDev, pJsHBMDev, "HBMDev");

    HBMDevicePtr pHBMDev = pJsHBMDev->GetHBMDev();
    C_CHECK_RC(pHBMDev->StartMBist(mbistType));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsHBMDev,
                CheckMBistCompletion,
                1,
                "Check if MBIST has completed")
{
    STEST_HEADER(1, 1, "Usage: HBMDev.CheckMBistCompletion(mbistType).");
    STEST_ARG(0, UINT32, mbistType);
    RC rc;
    STEST_PRIVATE(JsHBMDev, pJsHBMDev, "HBMDev");

    HBMDevicePtr pHBMDev = pJsHBMDev->GetHBMDev();
    C_CHECK_RC(pHBMDev->CheckMBistCompletion(mbistType));
    RETURN_RC(rc);
}

namespace
{
    RC ParseJsRowErrorData(const JsArray& jsRows, vector<Memory::Row>* pRows,
                           HBMDevice::RowErrorOriginTestIdMap* pOriginTestMap)
    {
        MASSERT(pRows);
        MASSERT(pOriginTestMap);
        RC rc;
        JavaScriptPtr pJavaScript;

        for (UINT32 i = 0; i < jsRows.size(); i++)
        {
            Memory::Row& row = (*pRows)[i];
            INT32 originTestId = s_UnknownTestId;

            JSObject* jsAddrObj;
            UINT32 eccRowOffset, nonEccRowOffset;
            CHECK_RC(pJavaScript->FromJsval(jsRows[i], &jsAddrObj));
            CHECK_RC(pJavaScript->UnpackFields(jsAddrObj, "sIIIII~i~I~I~I",
                                               "RowName",        &row.rowName,
                                               "HwFbio",         &row.hwFbio,
                                               "Subpartition",   &row.subpartition,
                                               "Rank",           &row.rank,
                                               "Bank",           &row.bank,
                                               "Row",            &row.row,
                                               // Optional field for backwards compatibility
                                               "OriginTestId",   &originTestId,
                                               "EccRowOffset",   &eccRowOffset,
                                               "NonEccRowOffset",&nonEccRowOffset,
                                               "PseudoChannel",  &row.pseudoChannel));

            // Associate the row with the test that found it
            //
            if (pOriginTestMap->find(row.rowName) != pOriginTestMap->end())
            {
                // TODO(stewarts): should be generating the names based on the row data, not in gpuargs.js.
                // Found a non-unique name
                Printf(Tee::PriError, "Found non-unique row name: %s\n", row.rowName.c_str());
                return RC::BAD_FORMAT;
            }

            (*pOriginTestMap)[row.rowName] = originTestId;
        }

        return rc;
    }
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsHBMDev,
                SoftRowRepair,
                1,
                "Do soft repair")
{
    STEST_HEADER(1, 1, "Usage: HBMDev.SoftRowRepair().");
    RC rc;
    STEST_PRIVATE(JsHBMDev, pJsHBMDev, "HBMDev");
    STEST_ARG(0, JsArray, jsRows);

    vector<Memory::Row> rows(jsRows.size());
    HBMDevice::RowErrorOriginTestIdMap originTestMap;
    C_CHECK_RC(ParseJsRowErrorData(jsRows, &rows, &originTestMap));

    HBMDevicePtr pHBMDev = pJsHBMDev->GetHBMDev();
    C_CHECK_RC(pHBMDev->RowRepair(true, rows, originTestMap, false, false));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsHBMDev,
                HardRowRepair,
                3,
                "Do Hard repair")
{
    STEST_HEADER(3, 3, "Usage: HBMDev.HardRowRepair().");
    RC rc;
    STEST_PRIVATE(JsHBMDev, pJsHBMDev, "HBMDev");
    STEST_ARG(0, JsArray, jsRows);
    STEST_ARG(1, bool, jsSkipVerif);
    STEST_ARG(2, bool, jsIsPseudoRepair);

    vector<Memory::Row> rows(jsRows.size());
    HBMDevice::RowErrorOriginTestIdMap originTestMap;
    C_CHECK_RC(ParseJsRowErrorData(jsRows, &rows, &originTestMap));

    HBMDevicePtr pHBMDev = pJsHBMDev->GetHBMDev();
    C_CHECK_RC(pHBMDev->RowRepair(false, rows, originTestMap, jsSkipVerif, jsIsPseudoRepair));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsHBMDev,
                DumpDevIdData,
                0,
                "Dump HBM Device ID")
{
    STEST_HEADER(0, 0, "Usage: HBMDev.DumpDevIdData().");

    RC rc;
    STEST_PRIVATE(JsHBMDev, pJsHBMDev, "HBMDev");
    HBMDevicePtr pHBMDev = pJsHBMDev->GetHBMDev();

    C_CHECK_RC(pHBMDev->DumpDevIdData());
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsHBMDev,
                DumpRepairedRows,
                0,
                "Dump Repaired Rows")
{
    STEST_HEADER(0, 0, "Usage: HBMDev.DumpRepairedRows().");

    RC rc;
    STEST_PRIVATE(JsHBMDev, pJsHBMDev, "HBMDev");
    HBMDevicePtr pHBMDev = pJsHBMDev->GetHBMDev();

    C_CHECK_RC(pHBMDev->DumpRepairedRows());
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsHBMDev,
                DumpRepairedLanes,
                0,
                "Dump Repaired Lanes")
{
    STEST_HEADER(0, 1, "Usage: HBMDev.DumpRepairedLanes().");

    STEST_OPTIONAL_ARG(0, bool, jsReadHBMData, false);

    RC rc;
    STEST_PRIVATE(JsHBMDev, pJsHBMDev, "HBMDev");
    HBMDevicePtr pHBMDev = pJsHBMDev->GetHBMDev();

    C_CHECK_RC(pHBMDev->DumpRepairedLanes(jsReadHBMData));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsHBMDev,
                LaneRepair,
                4,
                "Do lane-repair")
{
    STEST_HEADER(4, 4, "Usage: HBMDev.LaneRepair(isSoft, LanesToRepair, skipVerif, isPseudoRepair).");
    RC rc;
    STEST_PRIVATE(JsHBMDev, pJsHBMDev, "HBMDev");
    STEST_ARG(0, bool, jsIsSoft);
    STEST_ARG(1, JsArray, jsRepairLanes);
    STEST_ARG(2, bool, jsSkipVerif);
    STEST_ARG(3, bool, jsIsPseudoRepair);

    HBMDevicePtr pHBMDev = pJsHBMDev->GetHBMDev();

    vector<LaneError> repairLanes(jsRepairLanes.size());
    for (UINT32 i = 0; i < jsRepairLanes.size(); i++)
    {
        JSObject* jsAddrObj;
        string laneTypeStr;
        C_CHECK_RC(pJavaScript->FromJsval(jsRepairLanes[i], &jsAddrObj));

        const UINT32 subp = pHBMDev->GetSubpartition(repairLanes[i]);
        string laneName = repairLanes[i].ToString();
        C_CHECK_RC(pJavaScript->UnpackFields(
            jsAddrObj,      "sIIIs",
            "LaneName",     &laneName,
            "HwFbio",       &repairLanes[i].hwFbpa, // TODO(stewarts): need to change this field to hwFbpa, but lwstomers use this
            "Subpartition", &subp,
            "LaneBit",      &repairLanes[i].laneBit,
            "RepairType",   &laneTypeStr));

        repairLanes[i].laneType = LaneError::GetLaneTypeFromString(laneTypeStr);
    }

    // Look through the list of lanes to see if any are actually DBI errors, and adjust the list
    // accordingly
    MemError::DetectDBIErrors(&repairLanes);

    C_CHECK_RC(pHBMDev->LaneRepair(jsIsSoft, repairLanes, jsSkipVerif, jsIsPseudoRepair));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsHBMDev,
                PopulateInfoRomSoftRepairObj,
                0,
                "Populate the InfoROM RPR Soft Repair Object with HBM fuse data")
{
    STEST_HEADER(0, 0, "Usage: HBMDev.PopulateInfoRomSoftRepairObj()");
    RC rc;
    STEST_PRIVATE(JsHBMDev, pJsHBMDev, "HBMDev");

    HBMDevicePtr pHBMDev = pJsHBMDev->GetHBMDev();
    C_CHECK_RC(pHBMDev->PopulateInfoRomSoftRepairObj());
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsHBMDev,
                ClearInfoRomSoftSoftRepairObj,
                0,
                "Clear the InfoROM RPR Soft Repair Object")
{
    STEST_HEADER(0, 0, "Usage: HBMDev.ClearInfoRomSoftSoftRepairObj()");
    RC rc;
    STEST_PRIVATE(JsHBMDev, pJsHBMDev, "HBMDev");

    HBMDevicePtr pHBMDev = pJsHBMDev->GetHBMDev();
    C_CHECK_RC(pHBMDev->ClearInfoRomSoftSoftRepairObj());
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsHBMDev,
                ValidateInfoRomSoftRepairObj,
                0,
                "Validate the contents of the InfoROM RPR Soft Repair Object")
{
    STEST_HEADER(0, 0, "Usage: HBMDev.ValidateInfoRomSoftRepairObj()");
    RC rc;
    STEST_PRIVATE(JsHBMDev, pJsHBMDev, "HBMDev");

    HBMDevicePtr pHBMDev = pJsHBMDev->GetHBMDev();
    C_CHECK_RC(pHBMDev->ValidateInfoRomSoftRepairObj());
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsHBMDev,
                PrintInfoRomSoftRepairObj,
                0,
                "Print the contents of the InfoROM RPR Soft Repair Object")
{
    STEST_HEADER(0, 0, "Usage: HBMDev.PrintInfoRomSoftRepairObj()");
    RC rc;
    STEST_PRIVATE(JsHBMDev, pJsHBMDev, "HBMDev");

    HBMDevicePtr pHBMDev = pJsHBMDev->GetHBMDev();
    C_CHECK_RC(pHBMDev->PrintInfoRomSoftRepairObj());
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsHBMDev,
                AssertFBStop,
                0,
                "Assert FB stop")
{
    STEST_HEADER(0, 0, "Usage: HBMDev.AssertFBStop().");
    RC rc;
    STEST_PRIVATE(JsHBMDev, pJsHBMDev, "HBMDev");

    HBMDevicePtr pHBMDev = pJsHBMDev->GetHBMDev();
    C_CHECK_RC(pHBMDev->AssertFBStop());
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsHBMDev,
                DeAssertFBStop,
                0,
                "DeAssert FB stop")
{
    STEST_HEADER(0, 0, "Usage: HBMDev.DeAssertFBStop().");
    RC rc;
    STEST_PRIVATE(JsHBMDev, pJsHBMDev, "HBMDev");

    HBMDevicePtr pHBMDev = pJsHBMDev->GetHBMDev();
    C_CHECK_RC(pHBMDev->DeAssertFBStop());
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsHBMDev,
                ReadHToJHBMReg,
                4,
                "Read from HBM registers using Host to Jtag")
{
    STEST_HEADER(4, 4, "Usage: HBMDev.ReadHToJHBMReg(siteID, instID, [dataArray], length)");
    STEST_PRIVATE(JsHBMDev, pJsHBMDev, "HBMDev");
    STEST_ARG(0, UINT32, sid);
    STEST_ARG(1, UINT32, instID);
    STEST_ARG(2, JSObject *, data);
    STEST_ARG(3, UINT32, dataLen);
    RC rc;

    vector<UINT32> readData((dataLen+31)/32);

    HBMDevicePtr pHBMDev = pJsHBMDev->GetHBMDev();
    C_CHECK_RC(pHBMDev->GetGpuSubdevice()->GetHBMImpl()->ReadHostToJtagHBMReg(
        sid,
        instID,
        &readData,
        dataLen));

    for (UINT32 i = 0; i < readData.size(); i++)
    {
        C_CHECK_RC(pJavaScript->SetElement(data, (INT32)i, readData[i]));
    }

    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsHBMDev,
                WriteHToJHBMReg,
                4,
                "Write to HBM registers using Host to Jtag")
{
    STEST_HEADER(4, 4, "Usage: HBMDev.WriteHToJHBMReg(siteID, instID, dataArray, length)");
    STEST_PRIVATE(JsHBMDev, pJsHBMDev, "HBMDev");
    STEST_ARG(0, UINT32, sid);
    STEST_ARG(1, UINT32, instID);
    STEST_ARG(2, JsArray, data);
    STEST_ARG(3, UINT32, dataLen);
    RC rc;

    vector<UINT32> writeData(data.size());
    for (UINT32 i = 0; i < data.size(); i++)
    {
        UINT32 tmp;
        rc = pJavaScript->FromJsval(data[i], &tmp);
        if (OK != rc)
        {
            return JS_FALSE;
        }
        writeData[i] = (LwU32)tmp;
    }

    HBMDevicePtr pHBMDev = pJsHBMDev->GetHBMDev();
    C_CHECK_RC(pHBMDev->GetGpuSubdevice()->GetHBMImpl()->WriteHostToJtagHBMReg(
        sid,
        instID,
        writeData,
        dataLen));

    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsHBMDev,
                SetHBMResetDelayMs,
                1,
                "Set HBM reset delay (Milliseconds)")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    const char usage[] = "Usage: SetHBMResetDelayMs(timeMs)";
    UINT32 TimeMs;
    JavaScriptPtr pJS;
    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &TimeMs)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsHBMDev *pJsHBMDev;
    if ((pJsHBMDev = JS_GET_PRIVATE(JsHBMDev, pContext, pObject, "HBMDev")) != 0)
    {
        HBMDevicePtr pHBMDev = pJsHBMDev->GetHBMDev();
        pHBMDev->GetGpuSubdevice()->GetHBMImpl()->SetHBMResetDelayMs(TimeMs);
        return JS_TRUE;
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(JsHBMDev,
                GetLaneRemappingValues,
                2,
                "Get Lane Remapping Values.")
{
    STEST_HEADER(2, 2, "Usage: HBMDev.GetLaneRemappingValues(deviceId, laneError).");
    RC rc;

    STEST_ARG(0, UINT32, jsDeviceId);
    STEST_ARG(1, JsArray, jsLaneErrors);

    vector<LaneError> laneErrors(jsLaneErrors.size());
    for (UINT32 i = 0; i < jsLaneErrors.size(); i++)
    {
        JSObject* jsAddrObj;
        string laneTypeStr;
        C_CHECK_RC(pJavaScript->FromJsval(jsLaneErrors[i], &jsAddrObj));

        C_CHECK_RC(pJavaScript->UnpackFields(
            jsAddrObj,      "IIs",
            "HwFbio",       &laneErrors[i].hwFbpa,
            "LaneBit",      &laneErrors[i].laneBit,
            "RepairType",   &laneTypeStr));

        laneErrors[i].laneType = LaneError::GetLaneTypeFromString(laneTypeStr);
    }

    Gpu::LwDeviceId deviceId = static_cast<Gpu::LwDeviceId>(jsDeviceId);

    C_CHECK_RC(HBMDevice::GetLaneRemappingValues(deviceId, laneErrors));
    RETURN_RC(rc);
}

CLASS_PROP_READWRITE_LWSTOM(JsHBMDev, VoltageUv, "HBM Voltage in microvolts");
P_(JsHBMDev_Get_VoltageUv)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJs;
    JsHBMDev *pJsHbmDev = JS_GET_PRIVATE(JsHBMDev, pContext, pObject, "HBMDev");
    if (pJsHbmDev == nullptr)
    {
        pJs->Throw(pContext, RC::SOFTWARE_ERROR, "Error getting HBMDev");
        return JS_FALSE;
    }
    *pValue = JSVAL_NULL;

    HBMDevicePtr pHBMDev = pJsHbmDev->GetHBMDev();
    UINT32 voltageUv;
    RC rc = pHBMDev->GetGpuSubdevice()->GetHBMImpl()->GetVoltageUv(&voltageUv);
    if (OK != rc)
    {
        pJs->Throw(pContext, rc, "Error getting HBMDev.VoltageUv");
        return JS_FALSE;
    }
    rc = pJs->ToJsval(voltageUv, pValue);
    if (OK != rc)
    {
        pJs->Throw(pContext, rc,
                   "Error colwerting result in HBMDev.VoltageUv");
        return JS_FALSE;
    }
    return JS_TRUE;
}
P_(JsHBMDev_Set_VoltageUv)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJs;
    JsHBMDev *pJsHbmDev = JS_GET_PRIVATE(JsHBMDev, pContext, pObject, "HBMDev");
    if (pJsHbmDev == nullptr)
    {
        pJs->Throw(pContext, RC::SOFTWARE_ERROR, "Error getting HBMDev");
        return JS_FALSE;
    }
    HBMDevicePtr pHBMDev = pJsHbmDev->GetHBMDev();

    UINT32 voltageUv;
    RC rc = pJs->FromJsval(*pValue, &voltageUv);
    if (OK != rc)
    {
        pJs->Throw(pContext, rc,
                   "Error colwerting input in HBMDev.VoltageUv");
        return JS_FALSE;
    }

    rc = pHBMDev->GetGpuSubdevice()->GetHBMImpl()->SetVoltageUv(voltageUv);
    if (OK != rc)
    {
        pJs->Throw(pContext, rc, "Error setting HBMDev.VoltageUv");
        return JS_FALSE;
    }
    return JS_TRUE;
}
