/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_tegramfg_devif.h"
#include "lwl_tegramfg_dev.h"
#include "lwl_lwlink_libif.h"
#include "lwl_devif_fact.h"
#include "lwl_devif_mgr.h"

namespace
{
    //--------------------------------------------------------------------------
    //! \brief Create a cheetah mfg lwlink device interface
    //!
    //! \return Pointer to new gpu lwlink device interface
    LwLinkDevIf::Interface * CreateTegraMfgDevIf()
    {
        return new LwLinkDevIf::TegraMfgDevInterface;
    }

    //! Register the cheetah mfg lwlink device interface with the factory
    //!
    //! The first parameter controls the order in which devices are found,
    //! initialized, and destroyed.
    LwLinkDevIf::Factory::DevIfFactoryFuncRegister m_LwTegraMfgFact(2,
                                                                    "TegraMfgDevIf",
                                                                    CreateTegraMfgDevIf);
};

//------------------------------------------------------------------------------
//! \brief Find all GPU LwLink devices
//!
//! \param pDevices : Pointer to returned vector of found devices
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::TegraMfgDevInterface::FindDevices(vector<TestDevicePtr> *pDevices)
{
    if (FoundDevices())
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : CheetAh MFG lwlink devices already found, skipping\n",
               __FUNCTION__);
        return OK;
    }

    if (!Manager::GetIgnoreIbmNpu())
    {
        RC rc;
        vector<LibInterface::LibDeviceHandle> deviceHandles;
        CHECK_RC(LwLinkDevIf::GetTegraLibIf()->FindDevices(&deviceHandles));

        for (auto lwrHandle : deviceHandles)
        {
            auto pNewDev = make_shared<LwTegraMfgDev>(lwrHandle);
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
/* virtual */ RC LwLinkDevIf::TegraMfgDevInterface::InitializeAllDevices()
{
    if (!FoundDevices())
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : GPU LwLink devices not found, find devices before initialization\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    RC rc;
    for (auto & pLwrDev : m_Devices)
    {
        CHECK_RC(pLwrDev->Initialize());
    }

    m_bDevicesInitialized = true;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Shutdown all devices controlled by the interface
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::TegraMfgDevInterface::Shutdown()
{
    if (!m_bDevicesInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : GPU devices not initialized, skipping GPU lwlink shutdown\n",
               __FUNCTION__);
        return OK;
    }

    StickyRC rc;
    for (auto & pLwrDev : m_Devices)
    {
        rc = pLwrDev->Shutdown();
        if (!pLwrDev.unique())
        {
            Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                   "%s : Multiple references (%ld) to LwLink device %s during shutdown!\n",
                   __FUNCTION__, pLwrDev.use_count(), pLwrDev->GetName().c_str());
        }
    }
    m_bDevicesInitialized = false;

    m_Devices.clear();

    m_bFoundDevices = false;

    return rc;
}
