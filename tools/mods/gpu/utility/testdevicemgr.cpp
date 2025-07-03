/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/testdevicemgr.h"
#include "core/include/jscript.h"
#include "core/include/mgrmgr.h"
#include "core/include/platform.h"
#include "core/include/registry.h"
#include "core/include/script.h"
#include "gpu/uphy/uphyreglogger.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_devif.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_devif_mgr.h"
#include "lwmisc.h"
#include "lwRmReg.h"

using namespace LwLinkDevIf;

//------------------------------------------------------------------------------
//! \brief Constructor
//!
TestDeviceMgr::TestDeviceMgr()
{
    SetName("TestDevMgr");
}

//------------------------------------------------------------------------------
//! \brief Find all Test Devices
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC TestDeviceMgr::FindDevices()
{
    RC rc;

    if (LwLinkDevIf::Manager::AllDevicesFound())
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "%s : Already found Test Devices! Doing nothing.\n",
               GetName().c_str());
        return OK;
    }

    CHECK_RC(LwLinkDevIf::Initialize());
    CHECK_RC(LwLinkDevIf::Manager::FindDevices(&m_Devices));

    if (LwLinkDevIf::Manager::AllDevicesFound())
    {
        if (m_Devices.empty() && !GpuPtr()->ForceNoDevices())
        {
            Printf(Tee::PriError, "There are no active LWPU devices to test\n");
            return RC::PCI_DEVICE_NOT_FOUND;
        }

        m_LwSwitchMapDriverIdToDevInst.reserve(NumDevicesType(TestDevice::TYPE_LWIDIA_LWSWITCH));

        Printf(Tee::PriLow, LwLinkDevIf::GetTeeModule()->GetCode(), "Found Test Devices :\n");
        for (const auto& pTestDev : m_Devices)
        {
            pTestDev->Print(Tee::PriLow, LwLinkDevIf::GetTeeModule()->GetCode(), "   ");

            // Construct map between driver id and device instance, used for logging
            if (pTestDev->GetType() == TestDevice::TYPE_LWIDIA_LWSWITCH)
            {
                const UINT32 devInst  = pTestDev->DevInst();
                const UINT32 driverId = pTestDev->GetDriverId();

                // Lwrrently LwSwitch devices are numbered from 0 to N-1 in the driver.
                // We have this assertion here to ensure that this number doesn't
                // change to some random values, which would explode the vector.
                MASSERT(driverId < 64U);

                if (driverId >= m_LwSwitchMapDriverIdToDevInst.size())
                {
                    m_LwSwitchMapDriverIdToDevInst.resize(driverId + 1, ~0U);
                }
                MASSERT(m_LwSwitchMapDriverIdToDevInst[driverId] == ~0U);
                m_LwSwitchMapDriverIdToDevInst[driverId] = devInst;
            }
        }
    }

    return rc;
}

TestDevicePtr TestDeviceMgr::GetFirstDevice()
{
    TestDevicePtr pDevice;

    if (NumDevices() > 0)
    {
        pDevice = m_Devices.front();
    }

    return pDevice;
}

//------------------------------------------------------------------------------
//! \brief Get a pointer to a specific Test Device
//!
//! \param index    : Index of Test Device to get
//! \param ppDevice : Pointer to returned Test Device pointer
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC TestDeviceMgr::GetDevice(UINT32 index, Device **pDevice)
{
    RC rc;
    TestDevicePtr pDev;
    CHECK_RC(GetDevice(index, &pDev));
    *pDevice = pDev.get();
    return rc;
}

/* virtual */ RC TestDeviceMgr::GetDevice(UINT32 devInst, TestDevicePtr *ppTestDevice)
{
    MASSERT(ppTestDevice);
    RC rc;

    if (devInst >= NumDevices())
    {
        Printf(Tee::PriLow, LwLinkDevIf::GetTeeModule()->GetCode(),
               "%s : Invalid index number! - Get Test Device failed\n",
               GetName().c_str());
        return RC::ILWALID_DEVICE_ID;
    }

    *ppTestDevice = m_Devices[devInst];
    MASSERT(ppTestDevice->get() != nullptr);
    MASSERT((*ppTestDevice)->DevInst() == devInst);

    return rc;
}

RC TestDeviceMgr::GetDevice(const TestDevice::Id & deviceId, TestDevicePtr* ppTestDevice)
{
    MASSERT(ppTestDevice);
    ppTestDevice->reset();

    for (auto & lwrTestDev : m_Devices)
    {
        if (lwrTestDev->GetId() == deviceId)
        {
            *ppTestDevice = lwrTestDev;
            return RC::OK;
        }
    }

    Printf(Tee::PriLow, LwLinkDevIf::GetTeeModule()->GetCode(),
           "%s : Invalid Test Device Id! - Get Test Device failed\n",
           GetName().c_str());
    return RC::ILWALID_DEVICE_ID;
}

/* virtual */ RC TestDeviceMgr::GetDevice(TestDevice::Type type, UINT32 index, TestDevicePtr* ppTestDevice)
{
    MASSERT(ppTestDevice);
    ppTestDevice->reset();

    const size_t numDevices = m_Devices.size();
    for (UINT32 devInst = 0, typeIdx = 0; devInst < numDevices; devInst++)
    {
        auto& pTestDev = m_Devices[devInst];
        if (pTestDev->GetType() == type)
        {
            MASSERT(devInst == pTestDev->DevInst());
            if (index == typeIdx)
            {
                *ppTestDevice = pTestDev;
                return RC::OK;
            }
            ++typeIdx;
        }
    }

    Printf(Tee::PriLow, LwLinkDevIf::GetTeeModule()->GetCode(),
           "%s : Invalid index number! - Get Test Device failed\n",
           GetName().c_str());
    return RC::ILWALID_DEVICE_ID;
}

//------------------------------------------------------------------------------
//! \brief Initialize all contained Test Devices
//!
//! \return OK if successful, not OK otherwise
RC TestDeviceMgr::InitializeAll()
{
    RC rc;

    if (LwLinkDevIf::Manager::AllDevicesInitialized())
    {
        Printf(Tee::PriNormal, LwLinkDevIf::GetTeeModule()->GetCode(),
               "%s : Devices already initialized, skipping.\n",
               GetName().c_str());
        return OK;
    }

    CHECK_RC(UphyRegLogger::Initialize());
    CHECK_RC(LwLinkDevIf::Manager::InitializeAllDevices());

    if (LwLinkDevIf::Manager::AllDevicesInitialized()&&
        UphyRegLogger::GetLoggingEnabled() &&
        (UphyRegLogger::GetLogPointMask() & UphyRegLogger::LogPoint::PostLinkTraining))
    {
        const UphyRegLogger::UphyInterface loggedIf =
            static_cast<UphyRegLogger::UphyInterface>(UphyRegLogger::GetLoggedInterface());
        if ((loggedIf == UphyRegLogger::UphyInterface::Pcie) ||
            (loggedIf == UphyRegLogger::UphyInterface::All))
        {
            // Per-device logging settings have not been initialized at this point, so it is
            // necessary to force the log (do not log device perf data as at this point MODS
            // has not initialized far enough to be able to query it)
            CHECK_RC(UphyRegLogger::ForceLogRegs(UphyRegLogger::UphyInterface::Pcie,
                                                 UphyRegLogger::PostLinkTraining,
                                                 RC::OK,
                                                 false));
        }
        if ((loggedIf == UphyRegLogger::UphyInterface::C2C) ||
            (loggedIf == UphyRegLogger::UphyInterface::All))
        {
            // Per-device logging settings have not been initialized at this point, so it is
            // necessary to force the log (do not log device perf data as at this point MODS
            // has not initialized far enough to be able to query it)
            CHECK_RC(UphyRegLogger::ForceLogRegs(UphyRegLogger::UphyInterface::C2C,
                                                 UphyRegLogger::PostLinkTraining,
                                                 RC::OK,
                                                 false));
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Return true if all devices are initialized
//!
//! \return true if all devices are initialized, false otherwise
bool TestDeviceMgr::IsInitialized()
{
    return LwLinkDevIf::Manager::AllDevicesInitialized();
}

UINT32 TestDeviceMgr::NumDevicesType(TestDevice::Type type) const
{
    const size_t numDevices = m_Devices.size();
    UINT32 count = 0;
    for (UINT32 i = 0; i < numDevices; i++)
    {
        if (m_Devices[i]->GetType() == type)
            count++;
    }
    return count;
}

UINT32 TestDeviceMgr::GetDevInst(TestDevice::Type type, UINT32 idx) const
{
    MASSERT(type == TestDevice::TYPE_LWIDIA_LWSWITCH); // TODO add support for GPUs
    return (idx < m_LwSwitchMapDriverIdToDevInst.size()) ? m_LwSwitchMapDriverIdToDevInst[idx] : ~0U;
}

UINT32 TestDeviceMgr::GetDriverId(UINT32 devInst) const
{
    return (devInst < m_Devices.size()) ? m_Devices[devInst]->GetDriverId() : ~0U;
}

//------------------------------------------------------------------------------
RC TestDeviceMgr::ShutdownAll()
{
    StickyRC rc;

    m_Devices.clear();

    rc = UphyRegLogger::Shutdown();
    rc = LwLinkDevIf::Manager::Shutdown();

    return rc;
}

//------------------------------------------------------------------------------
void TestDeviceMgr::OnExit()
{
    // This shutdown completely deinitializes lwlink and must be called after all
    // MODS and RM lwlink shutdowns have oclwrred.
    MASSERT(LwLinkDevIf::Manager::AllDevicesShutdown());
    if (!LwLinkDevIf::Manager::AllDevicesShutdown())
    {
        Printf(Tee::PriError,
               "%s : Shutting down device interface with devices initialized!",
               __FUNCTION__);
    }

    RC rc = LwLinkDevIf::Shutdown();
    if (OK != rc)
        Printf(Tee::PriError, "%s : Failed with %s (%d)\n", __FUNCTION__, rc.Message(), rc.Get());
}

//-----------------------------------------------------------------------------
// TestDeviceMgr JS Interface
//
static JSClass JsTestDeviceMgr_class =
{
    "TestDeviceMgr",
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
static SObject JsTestDeviceMgr_Object
(
    "TestDeviceMgr",
    JsTestDeviceMgr_class,
    0,
    0,
    "TestDeviceMgr JS Object"
);

PROP_READONLY(JsTestDeviceMgr, ((TestDeviceMgr*)(DevMgrMgr::d_TestDeviceMgr)),
              NumDevices, UINT32, "Return the number of test devices");

JS_SMETHOD_LWSTOM(JsTestDeviceMgr, NumDevicesType, 1, "Get the number of LwLink devices of a particular type")
{
    STEST_HEADER(0, 1, "Usage: TestDeviceMgr.NumDevicesType(type)");
    STEST_OPTIONAL_ARG(0, UINT32, typeVal, static_cast<UINT32>(TestDevice::TYPE_UNKNOWN));
    TestDevice::Type type = static_cast<TestDevice::Type>(typeVal);

    if (OK != pJavaScript->ToJsval(((TestDeviceMgr *)(DevMgrMgr::d_TestDeviceMgr))->NumDevicesType(type),
                                   pReturlwalue))
    {
        JS_ReportError(pContext,
                       "Error oclwrred in TestDeviceMgr.NumDevicesType()");
        return JS_FALSE;
    }
    return JS_TRUE;
}
