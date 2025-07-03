/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "class/cl502d.h"
#include "class/cla06fsubch.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "device/interface/pcie.h"
#include "device/interface/portpolicyctrl.h"
#include "device/interface/xusbhostctrl.h"
#include "fermi/gf100/dev_lw_xve.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gralloc.h"
#include "gpu/include/nfysema.h"
#include "gpu/include/pcicfg.h"
#include "gpu/utility/bglogger.h"
#include "gpu/utility/pcie/pexdev.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/surfrdwr.h"
#include "gpu/tests/gputest.h"
#include "cheetah/include/tegclk.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"

// For restoring Azalia on the GPU
#ifdef INCLUDE_AZALIA
#include "device/azalia/azactrlmgr.h"
#include "device/azalia/azactrl.h"
#endif

class GpuResetTest : public GpuTest
{
public:
    GpuResetTest();
    ~GpuResetTest() { }

    virtual void PrintJsProperties(Tee::Priority pri);
    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC InitFromJs();

    enum {
         c_TwoD = LWA06F_SUBCHANNEL_2D      // = 3, subchannel for 2D engine
        ,c_TwoDTransferWidth = 1024*1024    // transfer 1MB at a time
    };

    SETGET_PROP(UseOptimus, bool);
    SETGET_PROP(DelayAfterResetMs, UINT32);

private:
    RC InnerLoop();
    RC ResetGpu(PciCfgSpace* pPciCfgSpaceGPU);
    RC Setup2D();
    RC Cleanup2D();
    RC Verify();

    bool m_UseOptimus;
    UINT32 m_DelayAfterResetMs;

    UINT32 m_Width;
    UINT32 m_Height;
    PStateOwner m_PStateOwner;
    GpuTestConfiguration* m_pTestCfg;
    Channel* m_pCh;
    LwRm::Handle m_hCh;
    NotifySemaphore m_Semaphore;
    TwodAlloc m_TwoD;
    Surface m_Surf[2];
    Pcie *m_pGpuPcie;
};

JS_CLASS_INHERIT(GpuResetTest, GpuTest, "GPU reset test");

CLASS_PROP_READWRITE(GpuResetTest, UseOptimus, bool,
                     "Use Optimus to reset the GPU, if available (default is true)");
CLASS_PROP_READWRITE(GpuResetTest, DelayAfterResetMs, UINT32,
                     "Delay after reset, in ms (default is 100)");

GpuResetTest::GpuResetTest()
: m_UseOptimus(true)
, m_DelayAfterResetMs(100)
, m_Width(256)
, m_Height(256)
, m_pTestCfg(0)
, m_pCh(0)
, m_hCh(0)
, m_pGpuPcie(NULL)
{
}

RC GpuResetTest::InitFromJs()
{
    RC rc;

    // Call base-class function (for TestConfiguration and Golden properties).
    CHECK_RC(GpuTest::InitFromJs());

    return rc;
}

void GpuResetTest::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    Printf(pri, "GpuResetTest Js Properties:\n");
    Printf(pri, "\tUseOptimus:\t\t\t%s\n", m_UseOptimus?"true":"false");
    Printf(pri, "\tDelayAfterResetMs:\t\t%ums\n", m_DelayAfterResetMs);
}

bool GpuResetTest::IsSupported()
{
    // Deprecate Ada onwards
    if (GetBoundGpuDevice()->GetFamily() >= GpuDevice::GpuFamily::Ada)
    {
        return false;
    }

    // We can only do this on secondary otherwise we will screw up VBIOS
    if (GetBoundGpuSubdevice()->IsPrimary())
    {
        return false;
    }

    // This is not supported on SOC GPUs (TegraGpuRailgate is instead)
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        return false;
    }

    // The PCI reset is not supported on CheetAh chipset
    if (Platform::IsTegra())
    {
        return false;
    }

    // GT21x and newer GPUs have PMU which enables us to reset the GPU
    PMU* pPmu = 0;
    if (OK != GetBoundGpuSubdevice()->GetPmu(&pPmu))
    {
        return false;
    }

    // The GPU needs 2D class for us to perform verification after the reset
    m_TwoD.SetOldestSupportedClass(LW50_TWOD);
    return m_TwoD.IsSupported(GetBoundGpuDevice());
}

RC GpuResetTest::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());
    CHECK_RC(GpuTest::AllocDisplay()); // Allocate display so we can turn it off
    m_pTestCfg = GetTestConfiguration();
    CHECK_RC(m_PStateOwner.ClaimPStates(GetBoundGpuSubdevice()));
    m_pGpuPcie = GetBoundGpuSubdevice()->GetInterface<Pcie>();
    return rc;
}

RC GpuResetTest::Setup2D()
{
    RC rc;

    // Allocate channel
    m_pTestCfg->SetChannelType(TestConfiguration::GpFifoChannel);
    CHECK_RC(m_pTestCfg->AllocateChannelWithEngine(&m_pCh,
                                                   &m_hCh,
                                                   &m_TwoD));

    // Set 2D engine on our chosen subchannel c_TwoD
    m_pCh->SetObject(c_TwoD, m_TwoD.GetHandle());

    // Allocate semaphore
    m_Semaphore.Setup(NotifySemaphore::ONE_WORD,
            m_pTestCfg->NotifierLocation(),
            1);
    CHECK_RC(m_Semaphore.Allocate(m_pCh, 0));

    // Allocate surfaces
    m_Surf[0].SetLocation(Memory::Coherent);
    m_Surf[1].SetLocation(Memory::Optimal);
    for (int i=0; i < 2; i++)
    {
        m_Surf[i].SetPitch(m_Width*4);
        m_Surf[i].SetWidth(m_Width);
        m_Surf[i].SetHeight(m_Height);
        m_Surf[i].SetColorFormat(ColorUtils::A8R8G8B8);
        m_Surf[i].SetLayout(Surface::Pitch);
        CHECK_RC(m_Surf[i].Alloc(GetBoundGpuDevice()));
    }

    return rc;
}

RC GpuResetTest::Run()
{
    RC rc;
    for (UINT32 i = 0; i < m_pTestCfg->Loops(); i++)
    {
        CHECK_RC(InnerLoop());
    }
    return rc;
}

RC GpuResetTest::Cleanup2D()
{
    StickyRC rc;
    if (m_pCh)
    {
        rc = m_pCh->WaitIdle(m_pTestCfg->TimeoutMs());
        m_Surf[0].Free();
        m_Surf[1].Free();
        m_Semaphore.Free();
        m_TwoD.Free();
        rc = m_pTestCfg->FreeChannel(m_pCh);
        m_pCh = nullptr;
    }
    return rc;
}

RC GpuResetTest::Cleanup()
{
    StickyRC rc = Cleanup2D();
    m_PStateOwner.ReleasePStates();
    rc = GpuTest::Cleanup();
    return rc;
}

RC GpuResetTest::InnerLoop()
{
    RC rc;
    BgLogger::PauseBgLogger DisableBgLogger; // To prevent accessing GPU by
                                             // other threads during power off

    GpuSubdevice* const pSubdev = GetBoundGpuSubdevice();

#ifdef INCLUDE_AZALIA
    // Uninitialize Azalia device
    AzaliaController* const pAzalia = pSubdev->GetAzaliaController();
    if (pAzalia)
    {
        CHECK_RC(pAzalia->Uninit());
    }
#endif

    CHECK_RC(Cleanup2D());

    // Put the GPU in standby mode in RM
    CHECK_RC(pSubdev->StandBy());

    // Save PCI config space of the GPU and other functions
    vector<unique_ptr<PciCfgSpace> > cfgSpaceList;
    PciCfgSpace* pPciCfgSpaceGPU = nullptr;
    PciCfgSpace* pPciCfgSpaceUSB = nullptr;
    PciCfgSpace* pPciCfgSpacePPC = nullptr;
#ifdef INCLUDE_AZALIA
    PciCfgSpace* pPciCfgSpaceAzalia = nullptr;
#endif
    string usbDriverName;
    string ppcDriverName;

    if (!pSubdev->IsSOC())
    {
        cfgSpaceList.push_back(pSubdev->AllocPciCfgSpace());
        cfgSpaceList.back()->Initialize(m_pGpuPcie->DomainNumber(),
                                      m_pGpuPcie->BusNumber(),
                                      m_pGpuPcie->DeviceNumber(),
                                      m_pGpuPcie->FunctionNumber());
        pPciCfgSpaceGPU = cfgSpaceList.back().get();

#ifdef INCLUDE_AZALIA
        if (pAzalia)
        {
            SysUtil::PciInfo azaPciInfo = {0};
            CHECK_RC(pAzalia->GetPciInfo(&azaPciInfo));

            cfgSpaceList.push_back(pSubdev->AllocPciCfgSpace());
            CHECK_RC(cfgSpaceList.back()->Initialize(azaPciInfo.DomainNumber,
                                                   azaPciInfo.BusNumber,
                                                   azaPciInfo.DeviceNumber,
                                                   azaPciInfo.FunctionNumber));
            pPciCfgSpaceAzalia = cfgSpaceList.back().get();
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

            cfgSpaceList.push_back(pSubdev->AllocPciCfgSpace());
            CHECK_RC(cfgSpaceList.back()->Initialize(domain, bus, device, function));

            pPciCfgSpaceUSB = cfgSpaceList.back().get();
            CHECK_RC(Xp::DisablePciDeviceDriver(domain, bus, device, function, &usbDriverName));

        }

        if (pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_USB) &&
            pSubdev->SupportsInterface<PortPolicyCtrl>())
        {
            UINT32 domain, bus, device, function;
            CHECK_RC(pSubdev->GetInterface<PortPolicyCtrl>()->GetPciDBDF(&domain,
                                                                         &bus,
                                                                         &device,
                                                                         &function));

            cfgSpaceList.push_back(pSubdev->AllocPciCfgSpace());
            CHECK_RC(cfgSpaceList.back()->Initialize(domain, bus, device, function));

            pPciCfgSpacePPC = cfgSpaceList.back().get();
            CHECK_RC(Xp::DisablePciDeviceDriver(domain, bus, device, function, &ppcDriverName));
        }

        for (auto cfgSpaceIter = cfgSpaceList.begin();
             cfgSpaceIter != cfgSpaceList.end();
             ++cfgSpaceIter)
        {
            CHECK_RC((*cfgSpaceIter)->Save());
        }
    }

    // Reset the GPU
    CHECK_RC(ResetGpu(pPciCfgSpaceGPU));

    // Restore PCI config space of the GPU and other functions
    bool pciRestoreFailed = false;
    for (auto cfgSpaceIter = cfgSpaceList.begin();
         cfgSpaceIter != cfgSpaceList.end();
         ++cfgSpaceIter)
    {
        PciCfgSpace* pCfgSpace = cfgSpaceIter->get();
        if (!pCfgSpace->IsCfgSpaceReady())
        {
            Printf(Tee::PriError, "%04x:%02x:%02x.%x did not come back properly after reset.\n",
                        pCfgSpace->PciDomainNumber(),
                        pCfgSpace->PciBusNumber(),
                        pCfgSpace->PciDeviceNumber(),
                        pCfgSpace->PciFunctionNumber());
            pciRestoreFailed = true;
        }

        pCfgSpace->Restore();
    }

#ifdef INCLUDE_AZALIA
    bool azaliaAlive = pPciCfgSpaceAzalia && pPciCfgSpaceAzalia->IsCfgSpaceReady();
    #ifdef MACOSX_MFG
    // Bug 926751 workaround
    // When resuming a GPU on a Mac with Azalia enabled, we are able to detect HDA device
    // However Mac MODS lwrrently is unable to bring the Azalia device out of reset
    // For now assume that HDA device is not alive
    azaliaAlive = false;
    #endif
#endif

    if (pPciCfgSpaceUSB && !usbDriverName.empty())
    {
        Xp::EnablePciDeviceDriver(pPciCfgSpaceUSB->PciDomainNumber(),
                                  pPciCfgSpaceUSB->PciBusNumber(),
                                  pPciCfgSpaceUSB->PciDeviceNumber(),
                                  pPciCfgSpaceUSB->PciFunctionNumber(),
                                  usbDriverName);
    }

    if (pPciCfgSpacePPC && !ppcDriverName.empty())
    {
        Xp::EnablePciDeviceDriver(pPciCfgSpacePPC->PciDomainNumber(),
                                  pPciCfgSpacePPC->PciBusNumber(),
                                  pPciCfgSpacePPC->PciDeviceNumber(),
                                  pPciCfgSpacePPC->PciFunctionNumber(),
                                  ppcDriverName);
    }

    // Resume the GPU in RM
    rc = pSubdev->Resume();
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "Error: GPU resume failed. The GPU remains in an unknown state.\n");
        return rc;
    }

#ifdef INCLUDE_AZALIA
    if (pAzalia && azaliaAlive)
    {
        CHECK_RC(pAzalia->Init());
    }
#endif

    // Check if the GPU is operational after resume
    CHECK_RC(Setup2D());
    CHECK_RC(Verify());

    if ((OK == rc) && pciRestoreFailed)
    {
        rc = RC::PCIE_BUS_ERROR;
    }

    return rc;
}

RC GpuResetTest::ResetGpu(PciCfgSpace* pPciCfgSpaceGPU)
{
    RC rc;

    const UINT32 domain   = m_pGpuPcie->DomainNumber();
    const UINT32 bus      = m_pGpuPcie->BusNumber();
    const UINT32 device   = m_pGpuPcie->DeviceNumber();
    const UINT32 function = m_pGpuPcie->FunctionNumber();

    RegHal &regs = GetBoundGpuSubdevice()->Regs();

    UINT32 port;
    PexDevice* pPexDev;
    CHECK_RC(m_pGpuPcie->GetUpStreamInfo(&pPexDev, &port));

    bool gpuLtrEnable = false;
    if (!GetBoundGpuSubdevice()->IsSOC())
    {
        rc = m_pGpuPcie->GetLTREnabled(&gpuLtrEnable);
        if ((rc == RC::UNSUPPORTED_HARDWARE_FEATURE) || (rc == RC::UNSUPPORTED_FUNCTION))
            rc.Clear();
        else if (rc != OK)
            return rc;
    }

    // Use Optimus if available
    unique_ptr<Xp::OptimusMgr> pOptimus(Xp::GetOptimusMgr(
                domain, bus, device, function));
    if ((pOptimus.get() != 0) && m_UseOptimus)
    {
        Printf(Tee::PriLow, "Performing GPU reset using Optimus ACPI\n");

        // Enable dynamic power control, if available
        if (pOptimus->IsDynamicPowerControlSupported())
        {
            pOptimus->EnableDynamicPowerControl();
        }

        // Power off the GPU
        CHECK_RC(pOptimus->PowerOff());

        // If dynamic power control is available, wait for the
        // GPU to disappear from the PCI bus
        if (pOptimus->IsDynamicPowerControlSupported())
        {
            CHECK_RC(pPciCfgSpaceGPU->CheckCfgSpaceNotReady(
                                                m_pTestCfg->TimeoutMs()));
        }

        // Power on the GPU
        CHECK_RC(pOptimus->PowerOn());
    }
    else if (GetBoundGpuSubdevice()->IsSOC())
    {
        Printf(Tee::PriLow, "Performing SOC GPU reset using clock clamps\n");

        CheetAh::SocPtr()->DisableModuleClocks(CheetAh::CLK_MODULE_3D);
        CheetAh::SocPtr()->EnableModuleClocks(CheetAh::CLK_MODULE_3D);
    }
    else if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_GPU_RESET))
    {
        Printf(Tee::PriLow, "Performing GPU reset through XVE\n");

        // If Optimus is not available, perform GPU reset
        UINT32 value = 0;
        regs.SetField(&value, MODS_XVE_RESET_CTRL_GPU_RESET, 1);
        regs.SetField(&value, MODS_XVE_RESET_CTRL_RESET_COUNTER_VAL, 256);
        Platform::PciWrite32(domain, bus, device, function, LW_XVE_RESET_CTRL, value);

        // Wait 1ms for the reset to take place
        Tasker::Sleep(1);
    }
    else
    {
        Printf(Tee::PriLow, "Performing GPU reset through hot reset\n");

        pPexDev->ResetDownstreamPort(port);
    }

    // Wait 100ms after reset according to PCI spec
    Tasker::Sleep(m_DelayAfterResetMs);

    if (pPciCfgSpaceGPU != nullptr)
    {
        // Wait for PCI cfg space to become operational
        CHECK_RC(pPciCfgSpaceGPU->CheckCfgSpaceReady(m_pTestCfg->TimeoutMs()));
    }

    // Read PCI errors to clear them, typically there are host correctable
    // errors when we perform the GPU reset
    if (pPexDev)
    {
        // the reset will disable LTR on the upstream device. enable if gpu had it enabled.
        if (!GetBoundGpuSubdevice()->IsSOC() && gpuLtrEnable)
        {
            rc = pPexDev->SetDownstreamPortLTR(port, true);
            if (rc == RC::UNSUPPORTED_HARDWARE_FEATURE)
                rc.Clear();
            else if (rc != OK)
                return rc;
        }

        Pci::PcieErrorsMask hostErrors = Pci::NO_ERROR;
        pPexDev->GetDownStreamErrors(&hostErrors, port);
    }

    return OK;
}

RC GpuResetTest::Verify()
{
    RC rc;

    // Randomize contents of the source surface and the scratch surfaces
    const UINT32 surfSize = m_Surf[0].GetPitch() * m_Surf[0].GetHeight();
    vector<UINT32> origSurface(surfSize/sizeof(UINT32));
    vector<UINT32> newSurface(surfSize/sizeof(UINT32));
    Random rand;
    rand.SeedRandom(m_pTestCfg->Seed());
    for (UINT32 i=0; i < origSurface.size(); i++)
    {
        origSurface[i] = rand.GetRandom();
        newSurface[i] = rand.GetRandom();
    }

    // Fill the source surface
    const UINT32 subdevInst = GetBoundGpuSubdevice()->GetSubdeviceInst();
    CHECK_RC(SurfaceUtils::WriteSurface(m_Surf[0], 0, &origSurface[0],
                surfSize, subdevInst));

    // Write the destination surface with random garbage
    CHECK_RC(SurfaceUtils::WriteSurface(m_Surf[1], 0, &newSurface[0],
                surfSize, subdevInst));

    // Prepare the semaphore
    m_Semaphore.SetPayload(0);
    m_Semaphore.SetTriggerPayload(0x538af04e);
    CHECK_RC(m_pCh->SetContextDmaSemaphore(m_Semaphore.GetCtxDmaHandleGpu(subdevInst)));
    CHECK_RC(m_pCh->SetSemaphoreOffset(m_Semaphore.GetCtxDmaOffsetGpu(subdevInst)));
    m_pCh->SetSemaphoreReleaseFlags(Channel::FlagSemRelWithWFI);

    // Set src and dest surface layout (pitch vs. BL)
    m_pCh->Write(c_TwoD, LW502D_SET_SRC_MEMORY_LAYOUT,
                 LW502D_SET_SRC_MEMORY_LAYOUT_V_PITCH);
    m_pCh->Write(c_TwoD, LW502D_SET_DST_MEMORY_LAYOUT,
                 LW502D_SET_DST_MEMORY_LAYOUT_V_PITCH);

    // Set src and dst surface and transfer sizes
    m_pCh->Write(c_TwoD, LW502D_SET_SRC_PITCH,
            m_Surf[0].GetPitch());
    m_pCh->Write(c_TwoD, LW502D_SET_SRC_WIDTH,
            m_Surf[0].GetWidth());
    m_pCh->Write(c_TwoD, LW502D_SET_SRC_HEIGHT,
            m_Surf[0].GetHeight());
    m_pCh->Write(c_TwoD, LW502D_SET_DST_PITCH,
            m_Surf[1].GetPitch());
    m_pCh->Write(c_TwoD, LW502D_SET_DST_WIDTH,
            m_Surf[1].GetWidth());
    m_pCh->Write(c_TwoD, LW502D_SET_DST_HEIGHT,
            m_Surf[1].GetHeight());
    m_pCh->Write(c_TwoD, LW502D_SET_PIXELS_FROM_MEMORY_DST_WIDTH,
            m_Surf[1].GetWidth());
    m_pCh->Write(c_TwoD, LW502D_SET_PIXELS_FROM_MEMORY_DST_HEIGHT,
            m_Surf[1].GetHeight());

    // Set operation to copy
    m_pCh->Write(c_TwoD, LW502D_SET_ROP,
            0xCC); // SRCCOPY - see BitBlt in MSDN
    m_pCh->Write(c_TwoD, LW502D_SET_OPERATION,
            LW502D_SET_OPERATION_V_SRCCOPY);

    // Set src and dest surface format
    m_pCh->Write(c_TwoD, LW502D_SET_SRC_FORMAT,
            LW502D_SET_DST_FORMAT_V_A8R8G8B8);
    m_pCh->Write(c_TwoD, LW502D_SET_DST_FORMAT,
            LW502D_SET_DST_FORMAT_V_A8R8G8B8);

    // Set ctx dmas
    if (LW50_TWOD == m_TwoD.GetClass())
    {
        m_pCh->Write(c_TwoD, LW502D_SET_SRC_CONTEXT_DMA,
                m_Surf[0].GetCtxDmaHandle());
        m_pCh->Write(c_TwoD, LW502D_SET_DST_CONTEXT_DMA,
                m_Surf[1].GetCtxDmaHandle());
    }

    // Set ctx dma offsets
    m_pCh->Write(c_TwoD, LW502D_SET_SRC_OFFSET_UPPER, 2,
            static_cast<UINT32>(m_Surf[0].GetCtxDmaOffsetGpu()>>32),
            static_cast<UINT32>(m_Surf[0].GetCtxDmaOffsetGpu()));
    m_pCh->Write(c_TwoD, LW502D_SET_DST_OFFSET_UPPER, 2,
            static_cast<UINT32>(m_Surf[1].GetCtxDmaOffsetGpu()>>32),
            static_cast<UINT32>(m_Surf[1].GetCtxDmaOffsetGpu()));

    // Send blit operation method
    m_pCh->Write(c_TwoD, LW502D_PIXELS_FROM_MEMORY_SRC_Y0_INT, 0);

    // Set host semaphore with WFI upon completion of the previous operation
    CHECK_RC(m_pCh->SemaphoreRelease(m_Semaphore.GetTriggerPayload(subdevInst)));

    // Flush the channel
    m_pCh->Flush();

    // Wait for the 2D copy to finish
    CHECK_RC(m_Semaphore.Wait(m_pTestCfg->TimeoutMs()));

    // Read back the destination surface
    CHECK_RC(SurfaceUtils::ReadSurface(m_Surf[1], 0, &newSurface[0],
                surfSize, subdevInst));

    // Compare against the original surface
    if (newSurface != origSurface)
    {
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }
    return OK;
}

