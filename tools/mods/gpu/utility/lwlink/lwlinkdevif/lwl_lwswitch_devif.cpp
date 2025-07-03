/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <memory>

#include "ctrl_dev_lwswitch.h"
#include "lwl_lwswitch_devif.h"
#include "lwl_lwswitch_dev.h"
#include "lwl_limerock_dev.h"
#include "lwl_laguna_dev.h"
#include "lwl_simlwswitch_dev.h"
#include "lwl_libif.h"
#include "lwl_devif_fact.h"
#include "lwl_topology_mgr.h"
#include "lwl_topology_mgr_impl.h"
#include "lwl_devif_mgr.h"
#include "gpu/ism/lwswitchism.h"
#include "core/include/platform.h"

namespace
{
    //--------------------------------------------------------------------------
    //! \brief Create an LwSwitch device interface
    //!
    //! \return Pointer to new LwSwitch device interface
    LwLinkDevIf::Interface * CreateLwSwitchDevIf()
    {
        return new LwLinkDevIf::LwSwitchDevInterface;
    }

    //! Register the LwSwitch interface with the factory
    //!
    //! The first parameter controls the order in which devices are found,
    //! initialized, and destroyed.  LwSwitch devices should be found and
    //! destroyed first since lwswitch devices contain references to other
    //! device types in their routing structure
    LwLinkDevIf::Factory::DevIfFactoryFuncRegister m_LwSwitchFact(0,
                                                                  "LwSwitchDevIf",
                                                                  CreateLwSwitchDevIf);
};

//--------------------------------------------------------------------------
//! \brief Constructor
LwLinkDevIf::LwSwitchDevInterface::LwSwitchDevInterface()
 : m_bFoundDevices(false)
  ,m_bDevicesInitialized(false)
{
    m_pLibIf = LwLinkDevIf::LibInterface::CreateLwSwitchLibIf();
}

//--------------------------------------------------------------------------
//! \brief Destructor
/* virtual */ LwLinkDevIf::LwSwitchDevInterface::~LwSwitchDevInterface()
{
    if (m_pLibIf.get() && m_pLibIf->IsInitialized())
        m_pLibIf->Shutdown();
}

//------------------------------------------------------------------------------
//! \brief Find all LwSwitch LwLink devices
//!
//! \param pDevices : Pointer to returned vector of found devices
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::LwSwitchDevInterface::FindDevices(vector<TestDevicePtr> *pDevices)
{
    if (m_bFoundDevices)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : LwSwitch devices already found, skipping\n",
               __FUNCTION__);
        return OK;
    }

    RC rc;

    if (Manager::GetIgnoreLwSwitch())
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : Ignoring LwSwitch devices\n",
               __FUNCTION__);
        m_bFoundDevices = true;
        m_bDevicesInitialized = true;
        return OK;
    }

    vector<LibInterface::LibDeviceHandle> deviceHandles;
    CHECK_RC(m_pLibIf->Initialize());
    CHECK_RC(m_pLibIf->FindDevices(&deviceHandles));

    for (auto const & lwrrentHandle : deviceHandles)
    {
        TestDevice::Id lwrId;
        UINT32 domain   = 0;
        UINT32 bus      = 0;
        UINT32 device   = 0;
        CHECK_RC(m_pLibIf->GetPciInfo(lwrrentHandle, &domain, &bus, &device, nullptr));
        lwrId.SetPciDBDF(domain, bus, device);

        if (!TestDevice::IsPciDeviceInAllowedList(lwrId))
            continue;

        LWSWITCH_GET_INFO infoParams;
        infoParams.index[0] = LWSWITCH_GET_INFO_INDEX_ARCH;
        infoParams.index[1] = LWSWITCH_GET_INFO_INDEX_IMPL;
        infoParams.count = 2;
        CHECK_RC(m_pLibIf->Control(lwrrentHandle,
                                   LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_GET_INFO,
                                   &infoParams, sizeof(infoParams)));
        UINT32 arch = infoParams.info[0];
        UINT32 impl = infoParams.info[1];

        TestDevicePtr pNewDev;
        switch (arch)
        {
            case LWSWITCH_GET_INFO_INDEX_ARCH_SV10:
                switch (impl)
                {
                    case LWSWITCH_GET_INFO_INDEX_IMPL_SV10:
                        pNewDev = make_shared<WillowDev>(m_pLibIf,
                                                         lwrId,
                                                         lwrrentHandle);
                        break;
                    default:
                        Printf(Tee::PriError, GetTeeModule()->GetCode(),
                               "%s : Unknown LwSwitch Impl!\n",
                               __FUNCTION__);
                        return RC::UNSUPPORTED_DEVICE;
                }
                break;
#if LWCFG(GLOBAL_LWSWITCH_IMPL_LR10)
            case LWSWITCH_GET_INFO_INDEX_ARCH_LR10:
                switch (impl)
                {
                    case LWSWITCH_GET_INFO_INDEX_IMPL_LR10:
                        pNewDev = make_shared<LR10Dev>(m_pLibIf,
                                                       lwrId,
                                                       lwrrentHandle);
                        break;
// Lwrrently the switch driver still returns ARCH_LR10 on S000.  LS10 in the
// switch driver is lwrrently a work in progress
#if LWCFG(GLOBAL_LWSWITCH_IMPL_LS10)
                    case LWSWITCH_GET_INFO_INDEX_IMPL_S000:
                        pNewDev = make_shared<S000Dev>(m_pLibIf,
                                                       lwrId,
                                                       lwrrentHandle);
                        break;
#endif
                    default:
                        Printf(Tee::PriError, GetTeeModule()->GetCode(),
                               "%s : Unknown LwSwitch Impl!\n",
                               __FUNCTION__);
                        return RC::UNSUPPORTED_DEVICE;
                }
                break;
#endif
#if LWCFG(GLOBAL_LWSWITCH_IMPL_LS10)
            case LWSWITCH_GET_INFO_INDEX_ARCH_LS10:
                switch (impl)
                {
                    case LWSWITCH_GET_INFO_INDEX_IMPL_LS10:
                        pNewDev = make_shared<LS10Dev>(m_pLibIf,
                                                       lwrId,
                                                       lwrrentHandle);
                        break;
                    case LWSWITCH_GET_INFO_INDEX_IMPL_S000:
                        pNewDev = make_shared<S000Dev>(m_pLibIf,
                                                       lwrId,
                                                       lwrrentHandle);
                        break;
                    default:
                        Printf(Tee::PriError, GetTeeModule()->GetCode(),
                               "%s : Unknown LwSwitch Impl!\n",
                               __FUNCTION__);
                        return RC::UNSUPPORTED_DEVICE;
                }
                break;
#endif
            default:
                Printf(Tee::PriError, GetTeeModule()->GetCode(),
                       "%s : Unknown LwSwitch Arch!\n",
                       __FUNCTION__);
                return RC::UNSUPPORTED_DEVICE;
        }

        m_Devices.push_back(pNewDev);
        pDevices->push_back(pNewDev);
    }

    UINT32 numDevices = Manager::GetNumSimLwSwitch();
    for (UINT32 ii = 0; ii < numDevices; ++ii)
    {
        TestDevice::Id switchId;
        switchId.SetPciDBDF(0,
                            TestDevice::Id::SIMULATED_LWSWITCH_PCI_BUS,
                            static_cast<UINT16>(ii));
        if (TestDevice::IsPciDeviceInAllowedList(switchId))
        {
            TestDevicePtr pNewDev;
            switch (Manager::GetSimLwSwitchType())
            {
                case Device::SIMLWSWITCH:
                    pNewDev = make_shared<SimWillowDev>(switchId);
                    break;
#if LWCFG(GLOBAL_LWSWITCH_IMPL_LR10)
                case Device::SIMLR10:
                    pNewDev = make_shared<SimLimerockDev>(switchId);
                    break;
#endif
                default:
                    Printf(Tee::PriError, GetTeeModule()->GetCode(),
                           "%s : Unknown Sim LwSwitch Type!\n",
                           __FUNCTION__);
                    return RC::ILWALID_DEVICE_ID;
            }
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
/* virtual */ RC LwLinkDevIf::LwSwitchDevInterface::InitializeAllDevices()
{
    RC rc;

    if (!m_bFoundDevices)
    {
        Printf(Tee::PriHigh, GetTeeModule()->GetCode(),
               "%s : LwSwitch devices not found, find devices before initialization\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    if (m_bDevicesInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : LwSwitch devices already initialized\n",
               __FUNCTION__);
        return OK;
    }

    CHECK_RC(m_pLibIf->InitializeAllDevices());

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
/* virtual */ RC LwLinkDevIf::LwSwitchDevInterface::Shutdown()
{
    StickyRC rc;
    for (size_t ii = 0; ii < m_Devices.size(); ii++)
    {
        rc = m_Devices[ii]->Shutdown();
        if (!m_Devices[ii].unique())
        {
            Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                   "%s : Multiple references (%ld) to LwLink device %s during shutdown!\n",
                   __FUNCTION__, m_Devices[ii].use_count(), m_Devices[ii]->GetName().c_str());
        }
    }
    m_bDevicesInitialized = false;

    m_Devices.clear();
    m_bFoundDevices = false;

    rc = m_pLibIf->Shutdown();

    return rc;
}
