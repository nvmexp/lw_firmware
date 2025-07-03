/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/platform.h"
#include "device/interface/pcie.h"
#include "device/interface/portpolicyctrl.h"
#include "device/interface/xusbhostctrl.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/pcie/pcieimpl.h"
#include "gpu/tests/gputest.h"

#ifdef INCLUDE_AZALIA
    #include "device/azalia/azactrlmgr.h"
    #include "device/azalia/azactrl.h"
#endif

class PciFunction
{
public:
    PciFunction(GpuSubdevice* pSubdev, UINT32 domain, UINT32 bus, UINT32 device, UINT32 function);
    virtual ~PciFunction() = default;

    bool IsTestable(const vector<UINT32>& skipClassCodes);
    RC Reset(UINT32 delayAfterResetMs, FLOAT64 timeoutMs);

    virtual RC ResetSetup() { return OK; }
    virtual RC ResetCleanup() { return OK; }

    UINT32 m_DomainNumber;
    UINT32 m_BusNumber;
    UINT32 m_DeviceNumber;
    UINT32 m_FunctionNumber;
    UINT32 m_ClassCode;
    bool   m_FlrSupported;
    GpuSubdevice* m_pSubdev;
};

class GpuFunction : public PciFunction
{
public:
    GpuFunction(GpuSubdevice* pSubdev, UINT32 domain, UINT32 bus, UINT32 device, UINT32 function)
        : PciFunction(pSubdev, domain, bus, device, function) {};

    RC ResetSetup() override
    {
        RC rc;

        CHECK_RC(m_pSubdev->StandBy());

        return rc;
    }
    RC ResetCleanup() override
    {
        RC rc;

        // wait for ifr completion
        Tasker::Sleep(150);

        CHECK_RC(m_pSubdev->Resume());

        return rc;
    }
};

#ifdef INCLUDE_AZALIA
class AzaliaFunction : public PciFunction
{
public:
    AzaliaFunction(GpuSubdevice* pSubdev, UINT32 domain, UINT32 bus, UINT32 device, UINT32 function)
        : PciFunction(pSubdev, domain, bus, device, function)
    {
        m_pAzalia = pSubdev->GetAzaliaController();
    };

    RC ResetSetup() override
    {
        RC rc;

        if (m_pAzalia)
        {
            CHECK_RC(m_pAzalia->Uninit());
        }

        return rc;
    }
    RC ResetCleanup() override
    {
        RC rc;
        if (m_pAzalia)
        {
            CHECK_RC(m_pAzalia->Init());
        }
        return rc;
    }
private:
    AzaliaController* m_pAzalia;
};
#endif

class UsbHostFunction : public PciFunction
{
public:
    UsbHostFunction(GpuSubdevice* pSubdev, UINT32 domain, UINT32 bus, UINT32 device, UINT32 function)
        : PciFunction(pSubdev, domain, bus, device, function) {};

    RC ResetSetup() override
    {
        RC rc;

        CHECK_RC(Xp::DisablePciDeviceDriver(m_DomainNumber, 
                                            m_BusNumber,
                                            m_DeviceNumber,
                                            m_FunctionNumber,
                                            &m_UsbDriverName));

        return rc;
    }

    RC ResetCleanup() override
    {
        RC rc;

        if (!m_UsbDriverName.empty())
        {
            CHECK_RC(Xp::EnablePciDeviceDriver(m_DomainNumber,
                                               m_BusNumber,
                                               m_DeviceNumber,
                                               m_FunctionNumber,
                                               m_UsbDriverName));
        }

        return rc;
    }

private:
    string m_UsbDriverName;
};


class FLReset: public GpuTest
{
public:
    FLReset();
    ~FLReset();

    RC   Setup() override;
    RC   Run() override;
    RC   Cleanup() override;
    bool IsSupported() override;

    void PrintJsProperties(Tee::Priority pri) override;

    SETGET_PROP(DelayAfterResetMs, UINT32);
    SETGET_JSARRAY_PROP_LWSTOM(SkipClassCodes);

private:

    RC FindFunctions();
    RC RunInnerLoop();

    vector<unique_ptr<PciFunction> > m_functions;

    GpuTestConfiguration* m_pTestCfg;

    UINT32 m_DelayAfterResetMs;
    vector<UINT32> m_SkipClassCodes;
};

//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(FLReset, GpuTest, "FLR test");

CLASS_PROP_READWRITE(FLReset, DelayAfterResetMs, UINT32,
                     "Delay after reset, in ms (default is 100)");
CLASS_PROP_READWRITE_JSARRAY(FLReset, SkipClassCodes, JsArray,
                     "Array of class codes to skip.");

//-----------------------------------------------------------------------------

FLReset::FLReset()
: m_pTestCfg(nullptr)
, m_DelayAfterResetMs(100)
{
}
//-----------------------------------------------------------------------------
FLReset::~FLReset()
{
}

//-----------------------------------------------------------------------------
PciFunction::PciFunction(
    GpuSubdevice* pSubdev,
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function)
: m_DomainNumber(domain)
, m_BusNumber(bus)
, m_DeviceNumber(device)
, m_FunctionNumber(function)
, m_pSubdev(pSubdev)
{
    UINT32 revid = 0;
    Platform::PciRead32(m_DomainNumber,
                         m_BusNumber,
                         m_DeviceNumber,
                         m_FunctionNumber,
                         pSubdev->Regs().LookupAddress(MODS_EP_PCFG_GPU_REVISION_ID_AND_CLASSCODE),
                         &revid);
    m_ClassCode = DRF_VAL(_PCI, _REV_ID, _CLASS_CODE, revid);

    m_FlrSupported = Pci::GetFLResetSupport(m_DomainNumber,
                         m_BusNumber,
                         m_DeviceNumber,
                         m_FunctionNumber);
}

RC PciFunction::Reset(UINT32 delayAfterResetMs, FLOAT64 timeoutMs)
{
    RC rc;

    CHECK_RC(ResetSetup());

    DEFER
    {
        rc = ResetCleanup();
    };

    // Save PCI config space of the GPU
    unique_ptr<PciCfgSpace> pPciCfgSpaceGPU = m_pSubdev->AllocPciCfgSpace();

    CHECK_RC(pPciCfgSpaceGPU->Initialize(m_DomainNumber,
                                         m_BusNumber,
                                         m_DeviceNumber,
                                         m_FunctionNumber));
    CHECK_RC(pPciCfgSpaceGPU->Save());

    CHECK_RC(Pci::TriggerFLReset(m_DomainNumber,
                                 m_BusNumber,
                                 m_DeviceNumber,
                                 m_FunctionNumber));

    // Wait 100ms after reset according to PCI spec
    Tasker::Sleep(delayAfterResetMs);

    // confirm that cfg space is ready
    bool cfgSpaceReady = pPciCfgSpaceGPU->IsCfgSpaceReady();

    if (!cfgSpaceReady)
    {
        // try waiting a little longer before trying to restore cfg space
        CHECK_RC(pPciCfgSpaceGPU->CheckCfgSpaceReady(timeoutMs));
    }

    CHECK_RC(pPciCfgSpaceGPU->Restore());

    if (!cfgSpaceReady)
    {
        return RC::PCIE_BUS_ERROR;
    }

    return rc;
}

bool PciFunction::IsTestable(const vector<UINT32>& skipClassCodes)
{
    for (auto& c : skipClassCodes)
    {
        if (c == m_ClassCode)
        {
            return false;
        }
    }

    // on turing, FLR is only POR for function 0.
    if (m_pSubdev->HasBug(2108973) &&
        (m_ClassCode != Pci::CLASS_CODE_VIDEO_CONTROLLER_VGA) &&
        (m_ClassCode != Pci::CLASS_CODE_VIDEO_CONTROLLER_3D))
    {
        return false;
    }

    return m_FlrSupported;
}

//-----------------------------------------------------------------------------
RC FLReset::FindFunctions()
{
    RC rc;
    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    m_functions.clear();
    {
        Pcie* pPcie = pSubdev->GetInterface<Pcie>();

        m_functions.push_back(make_unique<GpuFunction>(
                pSubdev,
                pPcie->DomainNumber(),
                pPcie->BusNumber(),
                pPcie->DeviceNumber(),
                pPcie->FunctionNumber()));
    }

#ifdef INCLUDE_AZALIA
    AzaliaController* const pAzalia = pSubdev->GetAzaliaController();
    if (pAzalia)
    {
        SysUtil::PciInfo pciInfo = {0};
        pAzalia->GetPciInfo(&pciInfo);

        m_functions.push_back(make_unique<AzaliaFunction>(
                pSubdev,
                pciInfo.DomainNumber,
                pciInfo.BusNumber,
                pciInfo.DeviceNumber,
                pciInfo.FunctionNumber));
    }
#endif

    if (pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_USB) &&
        pSubdev->SupportsInterface<XusbHostCtrl>())
    {
        UINT32 domain, bus, device, function;
        CHECK_RC(pSubdev->GetInterface<XusbHostCtrl>()->GetPciDBDF(&domain,
                                   &bus,
                                   &device,
                                   &function));

        m_functions.push_back(
                make_unique<UsbHostFunction>(pSubdev, domain, bus, device, function));
    }

    if (pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_USB) &&
        pSubdev->SupportsInterface<PortPolicyCtrl>())
    {
        UINT32 domain, bus, device, function;
        CHECK_RC(pSubdev->GetInterface<PortPolicyCtrl>()->GetPciDBDF(&domain,
                                   &bus,
                                   &device,
                                   &function));

        m_functions.push_back(make_unique<PciFunction>(pSubdev, domain, bus, device, function));
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC FLReset::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    m_pTestCfg = GetTestConfiguration();

    if (m_functions.empty())
    {
        FindFunctions();
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC FLReset::Run()
{
    RC rc;

    for (UINT32 i = 0; i < m_pTestCfg->Loops(); i++)
    {
        CHECK_RC(RunInnerLoop());
        Tasker::Yield();
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC FLReset::RunInnerLoop()
{
    RC rc;

    for (auto& function : m_functions)
    {

        if (!function->IsTestable(m_SkipClassCodes))
            continue;

        Printf(GetVerbosePrintPri(), "Resetting %d:%d:%d.%d - 0x%x\n",
                            function->m_DomainNumber,
                            function->m_BusNumber,
                            function->m_DeviceNumber,
                            function->m_FunctionNumber,
                            function->m_ClassCode);

        CHECK_RC(function->Reset(m_DelayAfterResetMs, m_pTestCfg->TimeoutMs()));

        // sanity check the config space of all the functions
        for (auto& f : m_functions)
        {
            unique_ptr<PciCfgSpace> pPciCfgSpaceGPU = f->m_pSubdev->AllocPciCfgSpace();

            CHECK_RC(pPciCfgSpaceGPU->Initialize(f->m_DomainNumber,
                                                 f->m_BusNumber,
                                                 f->m_DeviceNumber,
                                                 f->m_FunctionNumber));

            if (!pPciCfgSpaceGPU->IsCfgSpaceReady())
            {
                Printf(Tee::PriError, "%d:%d:%d.%d cfg space not ready\n",
                       f->m_DomainNumber, f->m_BusNumber, f->m_DeviceNumber, f->m_FunctionNumber);

                return RC::PCIE_BUS_ERROR;
            }
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC FLReset::Cleanup()
{
    RC rc;

    CHECK_RC(GpuTest::Cleanup());

    return rc;
}

//-----------------------------------------------------------------------------
bool FLReset::IsSupported()
{
    if (GetBoundGpuSubdevice()->IsSOC())
        return false;

    if (m_functions.empty())
    {
        FindFunctions();
    }
    for (auto& function : m_functions)
    {
        if (function->IsTestable(m_SkipClassCodes))
        {
            return true;
        }
    }

    return false;
}

void FLReset::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    Printf(pri, "FLReset Js Properties:\n");
    Printf(pri, "\tDelayAfterResetMs:              %ums\n", m_DelayAfterResetMs);
    Printf(pri, "\tSkipClassCodes:                 ");
    for (auto& c : m_SkipClassCodes)
    {
        Printf(pri, "0x%x ", c);
    }
    Printf(pri, "\n");

}

RC FLReset::GetSkipClassCodes(JsArray *val) const
{
    JavaScriptPtr pJavaScript;
    return pJavaScript->ToJsArray(m_SkipClassCodes, val);
}

RC FLReset::SetSkipClassCodes(JsArray *val)
{
    JavaScriptPtr pJavaScript;
    return pJavaScript->FromJsArray(*val, &m_SkipClassCodes);
}
