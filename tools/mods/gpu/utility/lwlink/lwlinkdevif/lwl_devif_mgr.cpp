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

#include "lwl_devif_mgr.h"
#include "lwl_devif_fact.h"
#include "lwl_devif.h"
#include "core/include/massert.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include <set>

namespace LwLinkDevIf
{
    namespace Manager
    {
        //! All interfaces that are created through the factory
        vector<LwLinkDevIf::InterfacePtr> m_Interfaces;
        bool m_DevInterfacesCreated = false;
        bool m_IgnoreLwSwitch = false;
        bool m_IgnoreIbmNpu = false;
        bool m_IgnoreTegra = false;
        UINT32 m_NumSimGpu = 0;
#if defined(SIM_BUILD)
        UINT32 m_NumSimEbridge = 1; // Default value for TopologyManagerAuto
        UINT32 m_NumSimIbmNpu = 2; // Default value for TopologyManagerAuto
#else
        UINT32 m_NumSimEbridge = 0;
        UINT32 m_NumSimIbmNpu = 0;
#endif
        UINT32 m_NumSimLwSwitch = 0;
        UINT32 m_LwSwitchPrintLevel = Tee::LevNormal;
        Device::LwDeviceId m_SimLwSwitchType = Device::SIMLWSWITCH;

        // Comparison function for sorting LwLink devices by type through the STL
        // algorithms functions
        bool SortDevices(TestDevicePtr pDev1, TestDevicePtr pDev2)
        {
            if (pDev1->GetType() == pDev2->GetType())
            {
                // Sort real devices before software only simulated devices
                if (pDev1->IsModsInternalPlaceholder() != pDev2->IsModsInternalPlaceholder())
                    return pDev1->IsModsInternalPlaceholder() < pDev2->IsModsInternalPlaceholder();
                return pDev1->GetId() < pDev2->GetId();
            }

            return pDev1->GetType() < pDev2->GetType();
        }
    };
};

//------------------------------------------------------------------------------
//! \brief Determine if all interfaces have found devices
//!
//! \return true if all interfaces have found their devices, false otherwise
//!
bool LwLinkDevIf::Manager::AllDevicesFound()
{
    if (!m_DevInterfacesCreated)
        return false;

    for (size_t ii = 0; ii < m_Interfaces.size(); ii++)
    {
        if (!m_Interfaces[ii]->FoundDevices())
            return false;
    }

    return true;
}

//------------------------------------------------------------------------------
//! \brief Determine if all interfaces have initialized their devices
//!
//! \return true if all interfaces have initialized their devices, false otherwise
//!
bool LwLinkDevIf::Manager::AllDevicesInitialized()
{
    if (!m_DevInterfacesCreated)
        return false;

    for (size_t ii = 0; ii < m_Interfaces.size(); ii++)
    {
        if (!m_Interfaces[ii]->DevicesInitialized())
            return false;
    }

    return true;
}

//------------------------------------------------------------------------------
bool LwLinkDevIf::Manager::AllDevicesShutdown()
{
    if (!m_DevInterfacesCreated)
        return true;

    for (size_t ii = 0; ii < m_Interfaces.size(); ii++)
    {
        if (m_Interfaces[ii]->DevicesInitialized())
            return false;
    }

    return true;
}

//------------------------------------------------------------------------------
//! \brief Find all LwLink devices for all interfaces
//!
//! \param pDevices       : Pointer to returned vector of found devices
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::Manager::FindDevices(vector<TestDevicePtr> *pDevices)
{
    RC rc;

    if (!m_DevInterfacesCreated)
    {
        LwLinkDevIf::Factory::CreateAllDevInterfaces(&m_Interfaces);
        m_DevInterfacesCreated = true;
    }

    for (size_t ii = 0; ii < m_Interfaces.size(); ii++)
    {
        CHECK_RC(m_Interfaces[ii]->FindDevices(pDevices));
    }

    // Sort LwLink devices by Type and Id
    sort(pDevices->begin(), pDevices->end(), SortDevices);

    if (!pDevices->empty())
    {
        TestDevice::Type lwrDevType = TestDevice::TYPE_UNKNOWN;
        UINT32 lwrDevTypeInst = 0;
        UINT32 devInst = 0;
        for (auto & lwrDevPtr : *pDevices)
        {
            TestDevice* pLwrDev = lwrDevPtr.get();

            MASSERT(pLwrDev->DevInst() == ~0U);

            pLwrDev->SetDevInst(devInst);
            ++devInst;

            if (pLwrDev->GetType() != lwrDevType)
            {
                lwrDevType = pLwrDev->GetType();
                lwrDevTypeInst = 0;
            }
            else
            {
                ++lwrDevTypeInst;
            }
            pLwrDev->SetDeviceTypeInstance(lwrDevTypeInst);
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
bool LwLinkDevIf::Manager::GetIgnoreLwSwitch()
{
    return m_IgnoreLwSwitch;
}

//------------------------------------------------------------------------------
bool LwLinkDevIf::Manager::GetIgnoreIbmNpu()
{
    return m_IgnoreIbmNpu;
}

//------------------------------------------------------------------------------
bool LwLinkDevIf::Manager::GetIgnoreTegra()
{
    return m_IgnoreTegra;
}

//------------------------------------------------------------------------------
UINT32 LwLinkDevIf::Manager::GetNumSimEbridge()
{
    return m_NumSimEbridge;
}

//------------------------------------------------------------------------------
UINT32 LwLinkDevIf::Manager::GetNumSimIbmNpu()
{
    return m_NumSimIbmNpu;
}

//------------------------------------------------------------------------------
UINT32 LwLinkDevIf::Manager::GetNumSimGpu()
{
    return m_NumSimGpu;
}

//------------------------------------------------------------------------------
UINT32 LwLinkDevIf::Manager::GetNumSimLwSwitch()
{
    return m_NumSimLwSwitch;
}

//------------------------------------------------------------------------------
UINT32 LwLinkDevIf::Manager::GetLwSwitchPrintLevel()
{
    return m_LwSwitchPrintLevel;
}

//------------------------------------------------------------------------------
Device::LwDeviceId LwLinkDevIf::Manager::GetSimLwSwitchType()
{
    return m_SimLwSwitchType;
}

//------------------------------------------------------------------------------
//! \brief Initialize all devices
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::Manager::InitializeAllDevices()
{
    RC rc;
    for (size_t ii = 0; ii < m_Interfaces.size(); ii++)
    {
        if (m_Interfaces[ii]->FoundDevices())
        {
            CHECK_RC(m_Interfaces[ii]->InitializeAllDevices());
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Shutdown all interfaces that the manager controls
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::Manager::Shutdown()
{
    StickyRC rc;
    for (size_t ii = 0; ii < m_Interfaces.size(); ii++)
    {
        rc = m_Interfaces[ii]->Shutdown();
    }
    m_Interfaces.clear();
    m_DevInterfacesCreated = false;
    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::Manager::SetIgnoreLwSwitch(bool bIgnore)
{
    m_IgnoreLwSwitch = bIgnore;
    return OK;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::Manager::SetIgnoreIbmNpu(bool bIgnore)
{
    m_IgnoreIbmNpu = bIgnore;
    return OK;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::Manager::SetIgnoreTegra(bool bIgnore)
{
    m_IgnoreTegra = bIgnore;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::Manager::SetNumSimEbridge(UINT32 val)
{
    m_NumSimEbridge = val;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::Manager::SetNumSimIbmNpu(UINT32 val)
{
    m_NumSimIbmNpu = val;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::Manager::SetNumSimGpu(UINT32 val)
{
    m_NumSimGpu = val;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::Manager::SetNumSimLwSwitch(UINT32 val)
{
    m_NumSimLwSwitch = val;
    return OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::Manager::SetLwSwitchPrintLevel(UINT32 printLevel)
{
    m_LwSwitchPrintLevel = printLevel;
    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::Manager::SetSimLwSwitchType(Device::LwDeviceId type)
{
    m_SimLwSwitchType = type;
    return OK;
}

//-----------------------------------------------------------------------------
static JSClass Manager_class =
{
    "DevIfManager",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    JS_FinalizeStub
};

//-----------------------------------------------------------------------------
static SObject Manager_Object
(
    "DevIfManager",
    Manager_class,
    0,
    0,
    "DevIfManager JS Object"
);

using namespace LwLinkDevIf;
//-----------------------------------------------------------------------------
PROP_READWRITE_NAMESPACE(Manager, IgnoreLwSwitch, bool, "Do not load the LwSwitch driver and ignore all LwSwitch devices (default = false)");
PROP_READWRITE_NAMESPACE(Manager, IgnoreIbmNpu, bool, "Do not load the IBM NPU driver and ignore all IBM NPU devices (default = false)");
PROP_READWRITE_NAMESPACE(Manager, IgnoreTegra, bool, "Do not load the CheetAh LwLink driver and ignore all CheetAh devices (default = false)");
PROP_READWRITE_NAMESPACE(Manager, NumSimEbridge, UINT32, "Number of simulated Ebridge devices to create (default = 0)");
PROP_READWRITE_NAMESPACE(Manager, NumSimGpu, UINT32, "Number of simulated GPU devices to create (default = 0)");
PROP_READWRITE_NAMESPACE(Manager, NumSimIbmNpu, UINT32,  "Number of simulated IbmNpu devices to create (default = 0)");
PROP_READWRITE_NAMESPACE(Manager, NumSimLwSwitch, UINT32, "Number of simulated LwSwitch devices to create (default = 0)");
PROP_READWRITE_NAMESPACE(Manager, LwSwitchPrintLevel, UINT32, "Set the print level for the lwswitch driver (default = Normal)");

P_(Manager_Get_SimLwSwitchType);
P_(Manager_Set_SimLwSwitchType);
P_(Manager_Get_SimLwSwitchType)
{
    Device::LwDeviceId result;
    RC rc = RC_WRAP_NAMESPACE(Manager::GetSimLwSwitchType, &result);
    JavaScriptPtr pJs;
    if (rc != OK)
    {
        pJs->Throw(pContext, rc, "Error getting Manager.SimLwSwitchType");
        return JS_FALSE;
    }
    rc = pJs->ToJsval(static_cast<UINT32>(result), pValue);
    if (OK != rc)
    {
        pJs->Throw(pContext, rc, "Error colwerting result in Manager.SimLwSwitchType");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}
P_(Manager_Set_SimLwSwitchType)
{
    UINT32 Value;
    JavaScriptPtr pJs;
    RC rc = pJs->FromJsval(*pValue, &Value);
    if (OK != rc)
    {
        pJs->Throw(pContext, rc, "Error colwerting input in Manager.SimLwSwitchType");
        return JS_FALSE;
    }
    rc = Manager::SetSimLwSwitchType(static_cast<Device::LwDeviceId>(Value));
    if (OK != rc)
    {
        pJs->Throw(pContext, rc, "Error setting Manager.SimLwSwitchType");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}
static SProperty Manager_SimLwSwitchType
(
    Manager_Object,
    "SimLwSwitchType",
    0,
    0,
    Manager_Get_SimLwSwitchType,
    Manager_Set_SimLwSwitchType,
    0,
    "The chip type of the simulated LwSwitch devices (default = Device.SIMLWSWITCH)"
);
