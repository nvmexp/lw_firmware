/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_ibmnpu_devif.h"
#include "lwl_ibmnpu_p8p_dev.h"
#if LWCFG(GLOBAL_LWSWITCH_IMPL_LR10)
    #include "lwl_ibmnpu_p9p_dev.h"
#endif
#include "lwl_simibmnpu_dev.h"
#include "lwl_libif.h"
#include "lwl_devif_fact.h"
#include "lwl_topology_mgr.h"
#include "lwl_topology_mgr_impl.h"
#include "lwl_devif_mgr.h"
#include "core/include/platform.h"
#include "lw_ref.h"

namespace
{
    //--------------------------------------------------------------------------
    //! \brief Create an IBM NPU device interface
    //!
    //! \return Pointer to new IBM NPU device interface
    LwLinkDevIf::Interface * CreateIbmNpuDevIf()
    {
        return new LwLinkDevIf::IbmNpuDevInterface;
    }

    //! Register the IBM NPU interface with the factory
    //!
    //! The first parameter controls the order in which devices are found,
    //! initialized, and destroyed.
    LwLinkDevIf::Factory::DevIfFactoryFuncRegister m_IbmNpuFact(3, "IbmNpuDevIf", CreateIbmNpuDevIf);
};

//--------------------------------------------------------------------------
//! \brief Constructor
LwLinkDevIf::IbmNpuDevInterface::IbmNpuDevInterface()
 : m_bFoundDevices(false)
  ,m_bDevicesInitialized(false)
{
    m_pLibIf = LwLinkDevIf::LibInterface::CreateIbmNpuLibIf();
}

//------------------------------------------------------------------------------
//! \brief Destructor
//!
LwLinkDevIf::IbmNpuDevInterface::~IbmNpuDevInterface()
{
    if (m_pLibIf.get() && m_pLibIf->IsInitialized())
        m_pLibIf->Shutdown();
}

//------------------------------------------------------------------------------
//! \brief Find all IBM NPU LwLink devices
//!
//! \param pDevices : Pointer to returned vector of found devices
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::IbmNpuDevInterface::FindDevices(vector<TestDevicePtr> *pDevices)
{
    if (m_bFoundDevices)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : IbmNpu devices already found, skipping\n",
               __FUNCTION__);
        return OK;
    }

    if (Manager::GetIgnoreIbmNpu())
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : Ignoring IBMNPU devices\n",
               __FUNCTION__);
        m_bFoundDevices = true;
        m_bDevicesInitialized = true;
        return OK;
    }

    if (!Platform::HasClientSideResman())
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : Skipping, using kernel mode RM\n",
               __FUNCTION__);
        m_bFoundDevices = true;
        m_bDevicesInitialized = true;
        return OK;
    }

    RC rc;

    vector<LibInterface::LibDeviceHandle> deviceHandles;
    CHECK_RC(m_pLibIf->Initialize());
    CHECK_RC(m_pLibIf->FindDevices(&deviceHandles));

    map< TestDevice::Id, vector<LibInterface::LibDeviceHandle> > npuDevices;

    for (size_t ii = 0; ii < deviceHandles.size(); ii++)
    {
        TestDevice::Id lwrId;

        // Each link in an IbmNpu device will have different PCI B:D.F but
        // the same PCI Domain and need to be stored that way
        UINT32 domain = 0;
        CHECK_RC(m_pLibIf->GetPciInfo(deviceHandles[ii], &domain, nullptr, nullptr, nullptr));
        lwrId.SetPciDBDF(domain);
        npuDevices[lwrId].push_back(deviceHandles[ii]);
    }

    map<TestDevice::Id, vector<LibInterface::LibDeviceHandle> >::iterator pLwrNpuDev;
    for (pLwrNpuDev = npuDevices.begin(); pLwrNpuDev != npuDevices.end(); pLwrNpuDev++)
    {
        if (!TestDevice::IsPciDeviceInAllowedList(pLwrNpuDev->first))
            continue;

        UINT32 domain = 0;
        UINT32 bus    = 0;
        UINT32 dev    = 0;
        UINT32 func   = 0;
        CHECK_RC(m_pLibIf->GetPciInfo(pLwrNpuDev->second[0], &domain, &bus, &dev, &func));

        UINT32 revision;
        CHECK_RC(Platform::PciRead32(domain, bus, dev, func, LW_CONFIG_PCI_LW_2, &revision));
        revision = DRF_VAL(_CONFIG, _PCI_LW_2, _REVISION_ID, revision);

        TestDevicePtr pNewDev;
        switch (revision)
        {
            case 0:
            case 1:
                pNewDev.reset(new IbmP8PDev(m_pLibIf, pLwrNpuDev->first, pLwrNpuDev->second));
                break;
#if LWCFG(GLOBAL_LWSWITCH_IMPL_LR10)
            case 2:
                pNewDev.reset(new IbmP9PDev(m_pLibIf, pLwrNpuDev->first, pLwrNpuDev->second));
                break;
#endif
            default:
                Printf(Tee::PriError, GetTeeModule()->GetCode(), "Unknown NPU revision : %u\n", revision);
                return RC::UNSUPPORTED_DEVICE;
                break;
        }

        m_Devices.push_back(pNewDev);
        pDevices->push_back(pNewDev);
    }

    // Pad out the number of npus to the minimum required to handle any
    // forced configurations
    UINT32 numDevices = Manager::GetNumSimIbmNpu();
    for (UINT32 ii = 0; ii < numDevices; ++ii)
    {
        TestDevice::Id npuId;
        npuId.SetPciDBDF(TestDevice::Id::SIMULATED_NPU_PCI_DOMAIN + ii);
        if (TestDevice::IsPciDeviceInAllowedList(npuId))
        {
            TestDevicePtr pNewDev(new SimIbmNpuDev(npuId));
            m_Devices.push_back(pNewDev);
            pDevices->push_back(pNewDev);
        }
    }

    m_bFoundDevices = true;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Initialize all devices
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::IbmNpuDevInterface::InitializeAllDevices()
{
    RC rc;

    if (!m_bFoundDevices)
    {
        Printf(Tee::PriHigh, GetTeeModule()->GetCode(),
               "%s : IbmNpu devices not found, find devices before initialization\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    if (m_bDevicesInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : IbmNpu devices already initialized\n",
               __FUNCTION__);
        return OK;
    }

    for (size_t ii = 0; ii < m_Devices.size(); ii++)
    {
        CHECK_RC(m_Devices[ii]->Initialize());
    }

    m_bDevicesInitialized = true;

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Shutdown all devices controlled by the interface
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::IbmNpuDevInterface::Shutdown()
{
    StickyRC rc;
    for (size_t ii = 0; ii < m_Devices.size(); ii++)
    {
        if (!m_Devices[ii].unique())
        {
            Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                   "%s : Multiple references (%ld) to LwLink device %s during shutdown!\n",
                   __FUNCTION__, m_Devices[ii].use_count(), m_Devices[ii]->GetName().c_str());
        }
        rc = m_Devices[ii]->Shutdown();
    }
    m_bDevicesInitialized = false;

    m_Devices.clear();
    m_bFoundDevices = false;

    rc = m_pLibIf->Shutdown();

    return rc;
}
