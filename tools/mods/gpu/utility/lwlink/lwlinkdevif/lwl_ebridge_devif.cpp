/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_ebridge_devif.h"
#include "lwl_libif.h"
#include "lwl_simebridge_dev.h"
#include "lwl_devif_fact.h"
#include "lwl_devif.h"
#include "lwl_topology_mgr.h"
#include "lwl_topology_mgr_impl.h"
#include "lwl_devif_mgr.h"
#include "core/include/platform.h"

namespace
{
    //--------------------------------------------------------------------------
    //! \brief Create an ebridge device interface
    //!
    //! \return Pointer to new ebridge device interface
    LwLinkDevIf::Interface * CreateEbridgeDevIf()
    {
        return new LwLinkDevIf::EbridgeInterface;
    }

    //! Register the ebridge interface with the factory
    //!
    //! The first parameter controls the order in which devices are found,
    //! initialized, and destroyed.
    LwLinkDevIf::Factory::DevIfFactoryFuncRegister m_EbridgeFact(4,
                                                                 "EbridgeDevIf",
                                                                 CreateEbridgeDevIf);
};

//------------------------------------------------------------------------------
//! \brief Constructor
//!
LwLinkDevIf::EbridgeInterface::EbridgeInterface()
 : m_bFoundDevices(false)
  ,m_bDevicesInitialized(false)
{
}

//------------------------------------------------------------------------------
//! \brief Destructor
//!
LwLinkDevIf::EbridgeInterface::~EbridgeInterface()
{
}

//------------------------------------------------------------------------------
//! \brief Find all ebridge LwLink devices
//!
//! \param pDevices : Pointer to returned vector of found devices
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::EbridgeInterface::FindDevices(vector<TestDevicePtr> *pDevices)
{
    if (m_bFoundDevices)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : Ebridge devices already found, skipping\n",
               __FUNCTION__);
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

    UINT32 numDevices = Manager::GetNumSimEbridge();
    for (UINT32 ii = 0; ii < numDevices; ++ii)
    {
        TestDevice::Id ebridgeId;
        ebridgeId.SetPciDBDF(0, TestDevice::Id::SIMULATED_EBRIDGE_PCI_BUS, static_cast<UINT16>(ii));
        if (TestDevice::IsPciDeviceInAllowedList(ebridgeId))
        {
            TestDevicePtr pNewDev(new SimEbridgeDev(ebridgeId));
            m_Devices.push_back(pNewDev);
            pDevices->push_back(pNewDev);
        }
    }

    m_bFoundDevices = true;

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Initialize all devices
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::EbridgeInterface::InitializeAllDevices()
{
    RC rc;

    if (!m_bFoundDevices)
    {
        Printf(Tee::PriHigh, GetTeeModule()->GetCode(),
               "%s : Ebridge devices not found, find devices before initialization\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    if (m_bDevicesInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : Ebridge devices already initialized\n",
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
/* virtual */ RC LwLinkDevIf::EbridgeInterface::Shutdown()
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

    return rc;
}
