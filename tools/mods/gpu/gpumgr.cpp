/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/gpumgr.h"

#include "class/cl0000.h"  // LW01_NULL_OBJECT
#include "class/cl503b.h"  // LW503B_ALLOC_PARAMETERS
#include "core/include/evntthrd.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/mgrmgr.h"
#include "core/include/lwrm.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "ctrl/ctrl0080.h" // LW0080_CTRL_CMD_GPU_GET_DISPLAY_OWNER
#include "device/interface/pcie.h"
#include "gpu/include/userdalloc.h"
#include "gpu/perf/thermsub.h"
#include "gpu/vmiop/vmiopelwmgr.h"
#include "gpuarch.h"
#include "hopper/gh100/dev_xtl_ep_pcfg_gpu.h"
#include "hopper/gh100/dev_xtl_ep_pcfg_vf.h"
#include "maxwell/gm200/dev_master.h"//LW_PMC_BOOT_0_ARCHITECTURE
#include "lw_ref.h"        // LW_CONFIG_PCI_LW_0_VENDOR_ID_LWIDIA
#include "rm.h"
#include "t12x/t124/project_relocation_table.h"
#include "cheetah/include/tegchip.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include "turing/tu102/dev_vm.h" // LW_VIRTUAL_FUNCTION_PRIV_xxx

#if defined(INCLUDE_OGL)
#include "opengl/modsgl.h"
#endif
#if defined(INCLUDE_LWDA) && !defined(USE_LWDA_SYSTEM_LIB)
#include "lwca.h"
extern "C" void lwiFinalize(void);
#endif

#include <set>
#include <algorithm>

static void ModsShutdownGL()
{
#if defined(INCLUDE_OGL)
    // Shut down the GL driver, it has persistent allocations that
    // prevent the RM from deleting the device.
    mglShutdownLibrary();
#endif
}

static void ModsShutdownLWDA()
{
#if defined(INCLUDE_LWDA) && !defined(USE_LWDA_SYSTEM_LIB)
    lwiFinalize();
#endif
}

class GpuSubdevice::GpuInstSetter
{
public:
    void operator=(UINT32 value) { m_Value = value; }
private:
    UINT32 m_Value;
};

GpuSubdevice::GpuInstSetter& GpuSubdevice::GetGpuInstSetter()
{
    MASSERT(sizeof(GpuInstSetter) == sizeof(m_pPciDevInfo->GpuInstance));
    return *reinterpret_cast<GpuInstSetter*>(&m_pPciDevInfo->GpuInstance);
}

//! Constructor
GpuDevMgr::GpuDevMgr() :
    m_PreInitialized(false),
    m_Initialized(false),
    m_Found(false),
    m_Exiting(false),
    m_AddingNewDevices(false),
    m_IgnoredAndObsoleteDevices(0),
    m_VasReverse(false),
    m_NumUserdAllocs(UserdManager::DEFAULT_USERD_ALLOCS),
    m_UserdLocation(Memory::Optimal),
    m_PollingInterruptThreadID(Tasker::NULL_THREAD),
    m_ContinuePollingInterruptThread(false)
{
    for (UINT32 dev = 0; dev < LW0080_MAX_DEVICES; dev ++)
    {
        m_GpuDevices[dev] = 0;
    }

    m_PeerMapMutex = Tasker::CreateMutex("peer map mutex", Tasker::mtxUnchecked);
    m_GpuInstMapMutex = Tasker::CreateMutex("gpuinst map mutex", Tasker::mtxUnchecked);
}

//! Destructor
GpuDevMgr::~GpuDevMgr()
{
    MASSERT(m_GpuSubdevices.empty());
    UnhookInterrupts(); // We should have called this already, but just in case
    FreeDevices();
    if (m_Found)
    {
        FreeDeviceObjects();
    }
    MASSERT(!m_Initialized && !m_Found);
}

namespace
{
    // Comparison function for sorting GPU subdevices by PCI bus
    bool SortByPciBus(GpuSubdevice* pSubdev1, GpuSubdevice* pSubdev2)
    {
        if (pSubdev1->IsSOC())
        {
            return true;
        }
        else if (pSubdev2->IsSOC())
        {
            return false;
        }

        auto pGpuPcie1 = pSubdev1->GetInterface<Pcie>();
        auto pGpuPcie2 = pSubdev2->GetInterface<Pcie>();
        PexDevice *pPexDev1, *pPexDev2;
        UINT32 port1, port2;
        UINT32 bridgeBus1 = 0;
        UINT32 bridgeBus2 = 0;
        UINT32 bridgeDev1 = 0;
        UINT32 bridgeDev2 = 0;
        pGpuPcie1->GetUpStreamInfo(&pPexDev1, &port1);
        pGpuPcie2->GetUpStreamInfo(&pPexDev2, &port2);

        if (pPexDev1)
        {
            PexDevice::PciDevice* pPciDev1 = pPexDev1->GetDownStreamPort(port1);
            bridgeBus1 = pPciDev1->Bus;
            bridgeDev1 = pPciDev1->Dev;
        }
        if (pPexDev2)
        {
            PexDevice::PciDevice* pPciDev2 = pPexDev2->GetDownStreamPort(port2);
            bridgeBus2 = pPciDev2->Bus;
            bridgeDev2 = pPciDev2->Dev;
        }

        if (pGpuPcie1->DomainNumber() < pGpuPcie2->DomainNumber())
            return true;
        else if (pGpuPcie1->DomainNumber() > pGpuPcie2->DomainNumber())
            return false;

        // check bridge bus and device number, this relation remain fixed
        // it's helpful in sorting for multi-gpu design like Gemini
        if (bridgeBus1 < bridgeBus2)
            return true;
        else if (bridgeBus1 > bridgeBus2)
            return false;

        if (bridgeDev1 < bridgeDev2)
            return true;
        else if (bridgeDev1 > bridgeDev2)
            return false;

        if (pGpuPcie1->BusNumber() < pGpuPcie2->BusNumber())
            return true;
        else if (pGpuPcie1->BusNumber() > pGpuPcie2->BusNumber())
            return false;

        if (pGpuPcie1->DeviceNumber() < pGpuPcie2->DeviceNumber())
            return true;
        else if (pGpuPcie1->DeviceNumber() > pGpuPcie2->DeviceNumber())
            return false;

        return pGpuPcie1->FunctionNumber() < pGpuPcie2->FunctionNumber();
    }
}

//! \brief Create GpuSubdevice objects for GPU devices existing in the system.
//!
//! This top-level function is ilwoked from Gpu::Initialize to collect
//! all supported LWPU GPU devices and creates GpuSubdevice objects for
//! them. The GPU devices are not initialized in RM yet, further initialization
//! is performed inside Gpu::Initialize.
//!
//! For MODS builds with client-side RM this function ilwokes
//! GpuDevMgr::FindDevicesClientSideResman to collect GPU devices.
//! For MODS builds with kernel-side RM this function ilwokes
//! RmFindDevices RM function which issues ioctl calls into RM to
//! obtain the list of devices initialized by RM and then calls
//! ModsDrvAddGpuSubdevice which in turn calls GpuDevMgr::AddGpuSubdevice
//! in order to create GpuSubdevice objects.
//!
//! \sa Gpu::Initialize, RmFindDevices, ModsDrvAddGpuSubdevice, GpuDevMgr::AddGpuSubdevice
RC GpuDevMgr::FindDevices()
{
    Printf(Tee::PriDebug, "GpuDevMgr::FindDevices\n");

    m_IgnoredAndObsoleteDevices = 0;

    RC rc;
    if (Platform::HasClientSideResman())
    {
        if (Platform::IsTegra() && CheetAh::IsInitialized())
        {
            CHECK_RC(FindDevicesWithoutPci());
        }
        CHECK_RC(FindDevicesClientSideResman(SubdevDiscoveryMode::Allocate));
    }
    else
    {
        m_AddingNewDevices = true;
        DEFER { m_AddingNewDevices = false; };
        CHECK_RC(RmApiStatusToModsRC(RmFindDevices()));
    }

    if (m_GpuSubdevices.empty())
    {
        if (m_IgnoredAndObsoleteDevices > 0)
        {
            Printf(Tee::PriError, "The only GPU devices found are obsolete or ignored\n");
            return RC::ONLY_OBSOLETE_DEVICES_FOUND;
        }
        else
        {
            Printf(Tee::PriLow, "No active LWPU GPU devices found to test\n");
        }
    }
    return OK;
}

//! \brief Collects GPU devices in the system without any help from RM.
//!
//! This function is used only when MODS is built with client-side RM,
//! i.e. RM is built in into MODS. The function:
//! 1. Collects all GPU devices in the system
//! 2. Determines which are supported
//! 3. Throws away devices that are skipped
//! 4. Creates GpuSubdevice objects for each "active" device
//! 5. Sorts devices by location on the PCI bus
//!
//! The actual guts of this algorithm are in GpuDevMgr::TryInitializePciDevice
//!
//! \sa GpuDevMgr::TryInitializePciDevice
RC GpuDevMgr::FindDevicesClientSideResman(SubdevDiscoveryMode mode)
{
    // Go through all supported classes and enumerate possible devices
    const UINT32 classes[] =
    {
        Pci::CLASS_CODE_VIDEO_CONTROLLER_VGA,
        Pci::CLASS_CODE_VIDEO_CONTROLLER_3D
    };
    for (UINT32 ic=0; ic < sizeof(classes)/sizeof(classes[0]); ic++)
    {
        for (UINT32 index = 0; /* until all devices found or error */; index++)
        {
            UINT32 domain, bus, device, function;
            RC rc = Platform::FindPciClassCode(classes[ic], index,
                        &domain, &bus, &device, &function);
            if (RC::PCI_DEVICE_NOT_FOUND == rc)
            {
                break;
            }
            else if (OK != rc)
            {
                return rc;
            }

            if (Platform::IsVirtFunMode())
            {
                CHECK_RC(TryInitializeVfs(domain, bus, device, function, classes[ic]));
            }
            else
            {
                CHECK_RC(TryInitializePciDevice(domain, bus, device, function, classes[ic], mode));
            }
        }
    }

    // Sort GPU subdevices by location on the PCI bus
    AssignSubdeviceInstances();
    return OK;
}

void GpuDevMgr::AssignSubdeviceInstances()
{
    if (Platform::HasClientSideResman())
    {
        // Sort GPU subdevices by location on the PCI
        sort(m_GpuSubdevices.begin(), m_GpuSubdevices.end(), SortByPciBus);

        // Assign GPU instance numbers
        for (UINT32 gpuInst=0; gpuInst < m_GpuSubdevices.size(); gpuInst++)
        {
            m_GpuSubdevices[gpuInst]->GetGpuInstSetter() = gpuInst;
        }
    }
}

namespace
{
    void UnSetMemRange(pair<PHYSADDR,UINT64> range)
    {
        Platform::UnSetMemRange(range.first, static_cast<size_t>(range.second));
    }
}

RC GpuDevMgr::FindDevicesWithoutPci()
{
    // Get GPU device(s) from CheetAh relocation table
    vector<CheetAh::DeviceDesc> desc;
    RC rc = CheetAh::GetDeviceDescByType(LW_DEVID_GPU, &desc);
    CHECK_RC(rc);

    // Register each GPU
    for (UINT32 iDev=0; iDev < desc.size(); iDev++)
    {
        // Get BARs
        PHYSADDR baseAddress[2] = { 0 };
        UINT64 barSize[2] = { 0 };
        baseAddress[0] = desc[iDev].apertureOffset;
        barSize[0] = 16_MB;

        if (desc[iDev].apertureSize < barSize[0])
        {
            Printf(Tee::PriError, "GPU aperture size too small 0x%llx, "
                    "expected more than 0x%llx\n",
                    desc[iDev].apertureSize, barSize[0]);
            return RC::DEVICE_NOT_FOUND;
        }

        const UINT32 numBars = (desc[iDev].apertureSize == barSize[0]) ? 1 : 2;
        if (numBars > 1)
        {
            baseAddress[1] = baseAddress[0] + barSize[0];
            barSize[1] = desc[iDev].apertureSize - barSize[0];
        }

        // Print BARs
        for (UINT32 i=0; i < numBars; i++)
        {
            Printf(Tee::PriLow, "BAR%u base = 0x%X_%08X,\n     size = 0x%08llX\n",
                    i,
                    static_cast<UINT32>(baseAddress[i]>>32),
                    static_cast<UINT32>(baseAddress[i]),
                    barSize[i]);
        }

        // Map GPU aperture
        void* pBar0 = 0;
        CHECK_RC(GpuPtr()->GetSOCAperture(desc[iDev].devIndex, &pBar0));
        Printf(Tee::PriLow, "reg virtual base  = %p\n", pBar0);

        // Indentify GPU on the SOC by the SOC version field in the id register
        UINT32 chipVer = 0;
        CheetAh::GetChipVersion(&chipVer, 0, 0);
        Printf(Tee::PriLow, "chip version : 0x%x\n", chipVer);

        // Legacy/dummy PCI device info struct
        PciDevInfo pciDevInfo = { 0 };
        pciDevInfo.PciClassCode     = Pci::CLASS_CODE_UNDETERMINED;
        pciDevInfo.PciDomain        = ~0U;
        pciDevInfo.PciBus           = ~0U;
        pciDevInfo.PciDevice        = ~0U;
        pciDevInfo.PciFunction      = ~0U;
        pciDevInfo.Irq              = ~0U; // SOC interrupts handled by GpuSubdevice
        pciDevInfo.SocDeviceIndex   = desc[iDev].devIndex;
        pciDevInfo.ChipId           = GPU_ARCHITECTURE(_TEGRA, chipVer);

        // Instead of PCI dev id use SOC version to identify the GPU
        pciDevInfo.PciDeviceId      = pciDevInfo.ChipId;
        pciDevInfo.PhysLwBase       = baseAddress[0];
        pciDevInfo.LwApertureSize   = UNSIGNED_CAST(UINT32, barSize[0]);
        pciDevInfo.LinLwBase        = pBar0;
        pciDevInfo.PhysFbBase       = baseAddress[1];
        pciDevInfo.FbApertureSize   = barSize[1];
        pciDevInfo.SbiosBoot        = 0; // SOC GPU is usually off when MODS starts
        pciDevInfo.PhysInstBase     = ~0ULL; // BAR2 is not supported

        // Perform any post BAR0 setup
        if (!GpuPtr()->PostBar0Setup(pciDevInfo))
        {
            // Skip device if PostBar0Setup failed
            return OK;
        }

        Printf(Tee::PriLow, "Chip ID   = 0x%08X\n", pciDevInfo.ChipId);

        CHECK_RC(AddSubdevice(pciDevInfo));
    }

    return OK;
}

//! \brief Creates GpuSubdevice object based on PCI device description.
//!
//! The function is ilwoked from RmFindDevices in MODS builds
//! with kernel-side RM. The function is not used in MODS builds
//! with client-side RM.
//!
//! The only way to ilwoke this function is during GpuDevMgr::FindDevices.
//! If not ilwoked during GpuDevMgr::FindDevices, this function will MASSERT
//! or return RC::SOFTWARE_ERROR.
//!
//! \sa RmFindDevices, GpuDevMgr::FindDevices
RC GpuDevMgr::AddGpuSubdevice(const PciDevInfo& pciDevInfo)
{
    MASSERT(m_AddingNewDevices);
    if (!m_AddingNewDevices)
    {
        return RC::SOFTWARE_ERROR;
    }

    RC rc;
    const size_t oldSize = m_GpuSubdevices.size();
    CHECK_RC(AddSubdevice(pciDevInfo));
    const size_t newSize = m_GpuSubdevices.size();

    if (oldSize == newSize)
    {
        return OK;
    }

    // Assign GPU instance
    const UINT32 gpuInst = pciDevInfo.GpuInstance;
    m_GpuSubdevices.back()->GetGpuInstSetter() = gpuInst;
    const size_t lwrPos = m_GpuSubdevices.size() - 1;
    if (lwrPos != gpuInst)
    {
        if (gpuInst > lwrPos)
        {
            m_GpuSubdevices.resize(gpuInst+1, 0);
        }
        MASSERT(m_GpuSubdevices[gpuInst] == 0);
        m_GpuSubdevices[gpuInst] = m_GpuSubdevices[lwrPos];
        m_GpuSubdevices[lwrPos] = 0;
    }

    return OK;
}

//! \brief Peforms basic initialization of a GPU device on PCI bus.
//!
//! This function contains guts of the algorithm described/performed
//! by GpuDevMgr::FindDevicesClientSideResman. It is used only in
//! MODS builds with client-side RM.
//!
//! \sa GpuDevMgr::FindDevicesClientSideResman
RC GpuDevMgr::TryInitializePciDevice
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT32 classCode,
    SubdevDiscoveryMode mode
)
{
    Printf(Tee::PriLow, "Physical Graphics controller at %04x:%02x:%02x.%x\n",
           domain, bus, device, function);

    // Skip trying to initialize devices that are not in the allowed list
    if (!GpuPtr()->IsPciDeviceInAllowedList(domain, bus, device, function))
    {
        Printf(Tee::PriLow, "Graphics controller not in -pci_dev_only list, skipping its initialization\n");
        return OK;
    }

    // Check if this is an LWPU device
    UINT16 vendor = 0;
    RC rc;
    CHECK_RC(Platform::PciRead16(domain, bus, device, function, 0, &vendor));
    if (vendor == 0xFFFF || vendor == 0)
    {
        Printf(Tee::PriWarn,
               "PCI device %04x:%02x:%02x.%x has invalid vendor id: 0x%04x\n",
               domain, bus, device, function, vendor);
    }
    if (vendor != LW_CONFIG_PCI_LW_0_VENDOR_ID_LWIDIA)
    {
        Printf(Tee::PriLow, "Bypassing non-LWPU card\n");
        return OK;
    }

    // Get the BAR configuration offsets as well as whether each BAR is 64bit
    const UINT32 numBars = 3;

    // Get BAR0 information and setup BAR0
    PHYSADDR baseAddress[numBars] = { 0 };
    UINT64 barSize[numBars] = { 0 };
    CHECK_RC(Platform::PciGetBarInfo(domain, bus, device, function, 0,
                                     &baseAddress[0], &barSize[0]));

    // Make sure that the SBIOS assigned us a valid BAR0 address
    if ((0 == baseAddress[0]) ||
        (~0xFU == baseAddress[0]) ||
        (~UINT64(0xFU) == baseAddress[0]) ||
        (0 == barSize[0]))
    {
        Printf(Tee::PriHigh, "SBIOS assigned incorrect BAR0 - offset 0x%llx, size 0x%llx\n",
                baseAddress[0], barSize[0]);
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    // Enable the memory space and bus mastering, map the memory.
    UINT32 pciLw1 = 0;
    CHECK_RC(Platform::PciRead32(domain, bus, device, function,
                LW_CONFIG_PCI_LW_1, &pciLw1));
    pciLw1 |= DRF_DEF(_CONFIG, _PCI_LW_1, _MEMORY_SPACE, _ENABLED)
            | DRF_DEF(_CONFIG, _PCI_LW_1, _BUS_MASTER, _ENABLED);
    CHECK_RC(Platform::PciWrite32(domain, bus, device, function,
                LW_CONFIG_PCI_LW_1, pciLw1));

    // Map GPU registers
    void* pBar0 = 0;
    CHECK_RC(Platform::MapDeviceMemory
    (
        &pBar0,
        baseAddress[0],
        static_cast<UINT32>(barSize[0]),
        Memory::UC,
        Memory::ReadWrite
    ));
    ObjectHolder<void*> unmapOnReturn(
            pBar0, Platform::UnMapDeviceMemory);
    Printf(Tee::PriLow, "reg virtual base = %p\n", pBar0);

    // XXX Probably this entire block of code can get ripped out now that we only support little endian.
    // All we're doing now is forcing LE mode, but the chip resets into LE mode so it should be redundant.
    UINT32 pmcBoot1 = MEM_RD32((UINT08*)pBar0 + LW_PMC_BOOT_1);
    if (FLD_TEST_DRF(_PMC, _BOOT_1, _ENDIAN00, _BIG, pmcBoot1) &&
        FLD_TEST_DRF(_PMC, _BOOT_1, _ENDIAN24, _BIG, pmcBoot1) &&
        (pmcBoot1 != ~0U))
    {
        // The value we are writing is the same whether swapped or not.
        const UINT32 switchEndiannessValue =
            DRF_DEF(_PMC, _BOOT_1, _ENDIAN00, _BIG) |
            DRF_DEF(_PMC, _BOOT_1, _ENDIAN24, _BIG);
        MEM_WR32((UINT08*)pBar0 + LW_PMC_BOOT_1,
                 switchEndiannessValue);

        // WAR for bug 508402. Change of endian mode is not immediate.
        // Need to ensure change of mode took place before other reads
        UINT32 readCnt = 0;
        const UINT32 numOfTrials = 100; // 100 trials to read LW_PMC_BOOT_1 is enough.
        while ((DRF_DEF(_PMC, _BOOT_1, _ENDIAN00, _LITTLE) |
                DRF_DEF(_PMC, _BOOT_1, _ENDIAN24, _LITTLE))
               != MEM_RD32((UINT08*)pBar0 + LW_PMC_BOOT_1))
        {
            readCnt++;

            // If cycle "tried out", scream, because it is fatal.
            if (readCnt > numOfTrials)
            {
                Printf(Tee::PriError, "Timeout on LW_PMC_BOOT_1 polling\n"
                                      "Endianess mode switch failed\n");
                return RC::PCI_DEVICE_NOT_FOUND;
            }
        }
    }

    UINT32 pmcBoot0 = MEM_RD32((UINT08*)pBar0 + LW_PMC_BOOT_0);

    // Sanity check
    if (pmcBoot0 == 0 || pmcBoot0 == ~0U)
    {
        Printf(Tee::PriHigh, "Error: PCI GPU %x:%x:%x.%x has PMC_BOOT_0=0x%08x\n",
               domain, bus, device, function, pmcBoot0);
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    UINT32 pciLw0;
    CHECK_RC(Platform::PciRead32(domain, bus, device, function,
                                 LW_EP_PCFG_GPU_ID, &pciLw0));

    UINT32 pciIrq;
    CHECK_RC(Platform::PciGetIRQ(domain, bus, device, function, &pciIrq));

    // PCI device info struct
    PciDevInfo pciDevInfo = { 0 };
    pciDevInfo.PciClassCode     = classCode;
    pciDevInfo.PciDomain        = domain;
    pciDevInfo.PciBus           = bus;
    pciDevInfo.PciDevice        = device;
    pciDevInfo.PciFunction      = function;
    pciDevInfo.Irq              = pciIrq;
    pciDevInfo.SocDeviceIndex   = ~0U;
    pciDevInfo.ChipId      =
        (DRF_VAL(_PMC, _BOOT_0, _ARCHITECTURE, pmcBoot0) << DRF_SIZE(LW_PMC_BOOT_0_IMPLEMENTATION)) |
         DRF_VAL(_PMC, _BOOT_0, _IMPLEMENTATION, pmcBoot0);
    pciDevInfo.PciDeviceId      = (pciLw0 >> 16) & 0xFFFF;
    pciDevInfo.PhysLwBase       = baseAddress[0];
    pciDevInfo.LwApertureSize   = UNSIGNED_CAST(UINT32, barSize[0]);
    pciDevInfo.LinLwBase        = pBar0;
    if (Xp::CanEnableIova(domain, bus, device, function))
    {
        JavaScriptPtr pJs;
        bool disableIova = false;
        if ((pJs->GetProperty(pJs->GetGlobalObject(), "g_DisableIOVA", &disableIova)
            != RC::OK) || !disableIova)
        {
            pciDevInfo.InitNeeded |= MODSRM_INIT_IOVASPACE;
        }
    }
    Printf(Tee::PriLow, "IOVA %s\n", (pciDevInfo.InitNeeded & MODSRM_INIT_IOVASPACE) ?
                                     "enabled" : "disabled");

    // Perform any post BAR0 setup
    if (!GpuPtr()->PostBar0Setup(pciDevInfo))
    {
        // Skip device if PostBar0Setup failed
        return OK;
    }

    // Initialize the rest of BARs
    for (UINT32 i=1; i < numBars; i++)
    {
        CHECK_RC(Platform::PciGetBarInfo(domain, bus, device, function, i,
                                         &baseAddress[i], &barSize[i]));

        // Validate BAR
        if ((0 == baseAddress[i]) ||
            (~0xFU == baseAddress[i]) ||
            (~UINT64(0xFU) == baseAddress[i]) ||
            (0 == barSize[i]))
        {
            Printf(Tee::PriError, "SBIOS assigned incorrect BAR%u - offset 0x%llx, size 0x%llx\n",
                    i, baseAddress[i], barSize[i]);
            return RC::PCI_DEVICE_NOT_FOUND;
        }
    }
    pciDevInfo.PhysFbBase       = baseAddress[1];
    pciDevInfo.FbApertureSize   = barSize[1];
    pciDevInfo.PhysInstBase     = baseAddress[2];
    pciDevInfo.InstApertureSize = barSize[2];

    for (UINT32 i=0; i < numBars; i++)
    {
        Printf(Tee::PriLow, "BAR%u base = 0x%X_%08X,\n     size = 0x%08llX\n",
                i,
                static_cast<UINT32>(baseAddress[i]>>32),
                static_cast<UINT32>(baseAddress[i]),
                barSize[i]);
    }
    Printf(Tee::PriLow, "Vendor ID = 0x%04X\n", pciLw0 & 0xFFFF);
    Printf(Tee::PriLow, "Device ID = 0x%04X\n", pciDevInfo.PciDeviceId);
    Printf(Tee::PriLow, "Chip ID   = 0x%08X\n", pciDevInfo.ChipId);

    // Make sure that the SBIOS assigned us valid base addresses
    if (0 == baseAddress[1])
    {
        Printf(Tee::PriError, "FB BAR is zero\n");
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    // Search for MSI cap
    UINT08 msiCapOffset;
    if (OK == Pci::FindCapBase(domain, bus, device, function, PCI_CAP_ID_MSI, &msiCapOffset))
    {
        pciDevInfo.MsiCapOffset = msiCapOffset;
        CHECK_RC(Platform::PciRead32(domain, bus, device, function,
                    pciDevInfo.MsiCapOffset, &pciDevInfo.MsiCapCtrl));
    }

    // Search for MSI-X cap
    UINT08 msixCapOffset;
    if (OK == Pci::FindCapBase(domain, bus, device, function, PCI_CAP_ID_MSIX, &msixCapOffset))
    {
        pciDevInfo.MsixCapOffset = msixCapOffset;
        CHECK_RC(Platform::PciRead32(domain, bus, device, function,
                    pciDevInfo.MsixCapOffset, &pciDevInfo.MsixCapCtrl));
    }

    // Mark all potentially primary adapters.
    // These are silicon VGA controllers.
    // Non-silicon and 3D controllers will not be marked as primary.
    // Later on in RmInitGpu we will determine whether
    // the device has really been posted by VBIOS to ultimately
    // determine whether it is really primary.
    if ((Platform::GetSimulationMode() != Platform::Hardware) ||
        (DRF_VAL(_CONFIG, _PCI_LW_1, _IO_SPACE, pciLw1)
            == LW_CONFIG_PCI_LW_1_IO_SPACE_DISABLED)        ||
        (classCode != Pci::CLASS_CODE_VIDEO_CONTROLLER_VGA) )
    {
        // Not the VGA boot device, resman needs to POST it later.
        pciDevInfo.SbiosBoot = 0;
    }
    else
    {
        // Potentially primary boot device, SBIOS might have
        // already called the VBIOS to POST this board.
        pciDevInfo.SbiosBoot = 1;
    }

    // Map BAR1 as Write-Combined
    if ((Platform::GetSimulationMode() != Platform::Amodel) &&
        (static_cast<size_t>(barSize[1]) != barSize[1]))
    {
        // Amodel calls to SetMemRange don't do anything.
        MASSERT(!"ModsDrvSetMemRange is not 64bit safe");
    }
    rc = Platform::SetMemRange(baseAddress[1],
                static_cast<size_t>(barSize[1]), Memory::WC);
    if (OK != rc)
    {
        Printf(Tee::PriLow, "Failed to map BAR1 as Write-Combined\n");
        return rc;
    }
    ObjectHolder<pair<PHYSADDR,UINT64> > unsetMemRangeOnReturn(
            make_pair(baseAddress[1], barSize[1]), UnSetMemRange);

    pciDevInfo.InitDone = MODSRM_INIT_BAR0 | MODSRM_INIT_BAR1WC;

    if (mode == SubdevDiscoveryMode::Allocate)
    {
        CHECK_RC(AddSubdevice(pciDevInfo));
    }
    else
    {
        for (UINT32 gpuInst = 0; gpuInst < m_GpuSubdevices.size(); gpuInst++)
        {
            // Assign this pci device to previously allocated gpusubdevice object
            // of same bus address
            if (m_GpuSubdevices[gpuInst]->IsSamePciAddress(domain, bus, device, function))
            {
                m_GpuSubdevices[gpuInst]->ReassignPciDev(pciDevInfo);
                break;
            }
        }
    }

    // Don't restore BAR1 caching type nor unmap BAR0 if the device was created.
    // The GpuSubdevice class' destructor will handle this.
    unsetMemRangeOnReturn.Cancel();
    unmapOnReturn.Cancel();

    return OK;
}

RC GpuDevMgr::TryInitializeVfs
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT32 classCode
)
{
    Printf(Tee::PriLow, "SRIOV: Physical Graphics controller at %04x:%02x:%02x.%x\n",
           domain, bus, device, function);

    // Skip trying to initialize devices that are not in the allowed list
    if (!GpuPtr()->IsPciDeviceInAllowedList(domain, bus, device, function))
    {
        Printf(Tee::PriLow, "SRIOV: Graphics controller not in -pci_dev_only list, skipping its initialization\n");
        return OK;
    }

    // Check if this is an LWPU device
    UINT32 devId;
    RC rc;
    CHECK_RC(Platform::PciRead32(domain, bus, device, function, 0, &devId));

    UINT16 vendor = devId & 0xffff;

    if (vendor == 0)
    {
        Printf(Tee::PriWarn,
               "SRIOV: PCI device %04x:%02x:%02x.%x has invalid vendor id: 0x%04x\n",
               domain, bus, device, function, vendor);
    }
    if (vendor != LW_CONFIG_PCI_LW_0_VENDOR_ID_LWIDIA)
    {
        Printf(Tee::PriLow, "SRIOV: Bypassing non-LWPU card\n");
        return OK;
    }

    VmiopElwManager * pMgr = VmiopElwManager::Instance();
    if (OK != pMgr->CreateGfidToBdfMapping(domain, bus, device, function))
    {
        Printf(Tee::PriLow, "SRIOV: Bypassing LWPU card which doesn't support SRIOV\n");
        return OK;
    }

    UINT16 capAddr = 0;
    UINT16 capSize = 0;
    CHECK_RC(Pci::GetExtendedCapInfo(domain, bus, device, function,
                                     Pci::SRIOV, 0, &capAddr, &capSize));
    const INT32 capAdjust = capAddr - LW_EP_PCFG_GPU_SRIOV_HEADER;

    UINT32 cfgHdr4 = 0;
    CHECK_RC(Platform::PciRead32(domain, bus, device, function,
                             capAdjust + LW_EP_PCFG_GPU_SRIOV_NUM_VF_FUNC_DEP,
                             &cfgHdr4));
    UINT32 numVisibleVfs = DRF_VAL(_EP_PCFG_GPU, _SRIOV_NUM_VF_FUNC_DEP,
                                   _NUM_VFS, cfgHdr4);

    // 1 Virtual function mods process only has 1 GFID now
    // Only initialize 1 GFID now. This might be extended in the future
    UINT32 gfId = VmiopElwManager::Instance()->GetGfidAttachedToProcess();
    if (gfId > numVisibleVfs)
    {
        Printf(Tee::PriHigh, "Invalid GFID(%d) argument. The PF only supports "
            "%d visible VFs. Please use -chip_args sriov_vf_count to enlarge the "
            "supported VF count\n", gfId, numVisibleVfs);
        return RC::ILWALID_ARGUMENT;
    }

    // Figure out VF bus/function number
    UINT32 vfDomain = ~0;
    UINT32 vfBus = ~0;
    UINT32 vfDevice = ~0;
    UINT32 vfFunction = ~0;
    CHECK_RC(pMgr->GetDomainBusDevFun(gfId, &vfDomain, &vfBus,
                                      &vfDevice, &vfFunction));
    Printf(Tee::PriLow, "SRIOV: Found VF%d on %04x:%02x:%02x.%x\n",
           gfId, vfDomain, vfBus, vfDevice, vfFunction);

    // Enable the bus mastering for the VFs
    UINT32 vfCfgHdr1 = 0;
    CHECK_RC(Platform::PciRead32(vfDomain, vfBus, vfDevice, vfFunction,
                                 LW_EP_PCFG_VF_CTRL_CMD_AND_STATUS,
                                 &vfCfgHdr1));
    vfCfgHdr1 |= DRF_DEF(_EP_PCFG_VF, _CTRL_CMD_AND_STATUS,
                         _CMD_BUS_MASTER, _ENABLE);
    CHECK_RC(Platform::PciWrite32(vfDomain, vfBus, vfDevice, vfFunction,
                                  LW_EP_PCFG_VF_CTRL_CMD_AND_STATUS,
                                  vfCfgHdr1));

    // Get the BAR configuration offsets as well as whether each BAR is 64bit
    const UINT32 numBars = 3;

    // Get BAR0 information and setup BAR0
    PHYSADDR baseAddress[numBars] = { 0 };
    UINT64 barSize[numBars] = { 0 };
    CHECK_RC(Platform::PciGetBarInfo(vfDomain, vfBus, vfDevice, vfFunction, 0,
                                     &baseAddress[0], &barSize[0]));

    // Make sure that the SBIOS assigned us a valid BAR0 address
    if ((0 == baseAddress[0]) ||
        (~0xFU == baseAddress[0]) ||
        (~UINT64(0xFU) == baseAddress[0]) ||
        (0 == barSize[0]))
    {
        Printf(Tee::PriHigh, "SRIOV: SBIOS assigned incorrect BAR0 - offset 0x%llx, size 0x%llx\n",
                baseAddress[0], barSize[0]);
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    // Map GPU registers
    void* pBar0 = 0;
    CHECK_RC(Platform::MapDeviceMemory
    (
        &pBar0,
        baseAddress[0],
        static_cast<UINT32>(barSize[0]),
        Memory::UC,
        Memory::ReadWrite
    ));
    ObjectHolder<void*> unmapOnReturn(
            pBar0, Platform::UnMapDeviceMemory);

    Printf(Tee::PriLow, "SRIOV: reg virtual base = %p\n", pBar0);

    UINT32 pmcBoot0 = MEM_RD32((UINT08*)pBar0 + LW_VIRTUAL_FUNCTION_PRIV_BOOT_0);
    UINT32 pmcBoot42 = MEM_RD32((UINT08*)pBar0 + LW_VIRTUAL_FUNCTION_PRIV_BOOT_42);

    // Sanity check
    if (pmcBoot0 == 0 || pmcBoot0 == ~0U ||
        DRF_VAL(_VIRTUAL_FUNCTION_PRIV, _BOOT_0, _ARCHITECTURE, pmcBoot0) <
        LW_VIRTUAL_FUNCTION_PRIV_BOOT_0_ARCHITECTURE_LW50)
    {
        Printf(Tee::PriHigh, "SRIOV: Error: PCI GPU %x:%x:%x.%x has "
            "LW_VIRTUAL_FUNCTION_PRIV_BOOT_0=0x%08x\n",
            vfDomain, vfBus, vfDevice, vfFunction, pmcBoot0);
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    Printf(Tee::PriLow, "SRIOV: LW_VIRTUAL_FUNCTION_PRIV_BOOT_0 = 0x%x\n", pmcBoot0);
    Printf(Tee::PriLow, "SRIOV: LW_VIRTUAL_FUNCTION_PRIV_BOOT_42 = 0x%x\n", pmcBoot42);

    // PCI device info struct
    PciDevInfo pciDevInfo = { 0 };
    pciDevInfo.PciClassCode     = classCode;
    pciDevInfo.PciDomain        = vfDomain;
    pciDevInfo.PciBus           = vfBus;
    pciDevInfo.PciDevice        = vfDevice;
    pciDevInfo.PciFunction      = vfFunction;
    pciDevInfo.Irq              = 0; //??? Using MSI-X, not sure what's the Irq.
    pciDevInfo.SocDeviceIndex   = ~0U;
    pciDevInfo.ChipId           =
        (DRF_VAL(_VIRTUAL_FUNCTION_PRIV, _BOOT_0, _ARCHITECTURE, pmcBoot0) <<
                      DRF_SIZE(LW_VIRTUAL_FUNCTION_PRIV_BOOT_0_IMPLEMENTATION)) |
         DRF_VAL(_VIRTUAL_FUNCTION_PRIV, _BOOT_0, _IMPLEMENTATION, pmcBoot0);
    pciDevInfo.PciDeviceId      = (devId >> 16) & 0xFFFF;
    pciDevInfo.PhysLwBase       = baseAddress[0];
    pciDevInfo.LwApertureSize   = UNSIGNED_CAST(UINT32, barSize[0]);
    pciDevInfo.LinLwBase        = pBar0;
    pciDevInfo.SbiosBoot        = 0;

    // Perform any post BAR0 setup for PF
    /*if (!GpuPtr()->PostBar0Setup(pciDevInfo))
    {
        // Skip device if PostBar0Setup failed
        return OK;
    }*/

    // Initialize the rest of BARs
    for (UINT32 i=1; i < numBars; i++)
    {
        CHECK_RC(Platform::PciGetBarInfo(vfDomain, vfBus, vfDevice, vfFunction,
                                         i, &baseAddress[i], &barSize[i]));

        // Validate BAR
        if ((0 == baseAddress[i]) ||
            (~0xFU == baseAddress[i]) ||
            (~UINT64(0xFU) == baseAddress[i]) ||
            (0 == barSize[i]))
        {
            Printf(Tee::PriHigh, "SRIOV: SBIOS assigned incorrect BAR%u - offset 0x%llx, size 0x%llx\n",
                    i, baseAddress[i], barSize[i]);
            return RC::PCI_DEVICE_NOT_FOUND;
        }
    }
    pciDevInfo.PhysFbBase       = baseAddress[1];
    pciDevInfo.FbApertureSize   = barSize[1];
    pciDevInfo.PhysInstBase     = baseAddress[2];
    pciDevInfo.InstApertureSize = barSize[2];

    for (UINT32 i=0; i < numBars; i++)
    {
        Printf(Tee::PriLow, "BAR%u base = 0x%X_%08X,\n     size = 0x%08llX\n",
                i,
                static_cast<UINT32>(baseAddress[i]>>32),
                static_cast<UINT32>(baseAddress[i]),
                barSize[i]);
    }
    Printf(Tee::PriLow, "Vendor ID = 0x%04X\n", vendor);
    Printf(Tee::PriLow, "Device ID = 0x%04X\n", pciDevInfo.PciDeviceId);
    Printf(Tee::PriLow, "Chip ID   = 0x%08X\n", pciDevInfo.ChipId);

    // Make sure that the SBIOS assigned us valid base addresses
    if (0 == baseAddress[1])
    {
        Printf(Tee::PriError, "SRIOV: FB BAR is zero\n");
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    // Search for MSI cap
    UINT08 msiCapOffset;
    if (OK == Pci::FindCapBase(vfDomain, vfBus, vfDevice, vfFunction, PCI_CAP_ID_MSI, &msiCapOffset))
    {
        pciDevInfo.MsiCapOffset = msiCapOffset;
        CHECK_RC(Platform::PciRead32(vfDomain, vfBus, vfDevice, vfFunction,
                    pciDevInfo.MsiCapOffset, &pciDevInfo.MsiCapCtrl));
    }

    // Search for MSI-X cap
    UINT08 msixCapOffset;
    if (OK == Pci::FindCapBase(vfDomain, vfBus, vfDevice, vfFunction, PCI_CAP_ID_MSIX, &msixCapOffset))
    {
        pciDevInfo.MsixCapOffset = msixCapOffset;
        CHECK_RC(Platform::PciRead32(vfDomain, vfBus, vfDevice, vfFunction,
                    pciDevInfo.MsixCapOffset, &pciDevInfo.MsixCapCtrl));
    }

    // Map BAR1 as Write-Combined
    if ((Platform::GetSimulationMode() != Platform::Amodel) &&
        (static_cast<size_t>(barSize[1]) != barSize[1]))
    {
        // Amodel calls to SetMemRange don't do anything.
        MASSERT(!"ModsDrvSetMemRange is not 64bit safe");
    }
    rc = Platform::SetMemRange(baseAddress[1],
                static_cast<size_t>(barSize[1]), Memory::WC);
    if (OK != rc)
    {
        Printf(Tee::PriLow, "SRIOV: Failed to map BAR1 as Write-Combined\n");
        return rc;
    }
    ObjectHolder<pair<PHYSADDR,UINT64> > unsetMemRangeOnReturn(
            make_pair(baseAddress[1], barSize[1]), UnSetMemRange);

    pciDevInfo.InitDone = MODSRM_INIT_BAR0 | MODSRM_INIT_BAR1WC;

    CHECK_RC(AddSubdevice(pciDevInfo));

    // Don't restore BAR1 caching type nor unmap BAR0 if the device was created.
    // The GpuSubdevice class' destructor will handle this.
    unsetMemRangeOnReturn.Cancel();
    unmapOnReturn.Cancel();

    return OK;
}

//! \brief Add a new GPU subdevice to the list of all known GPU subdevices
RC GpuDevMgr::AddSubdevice(const PciDevInfo& pciDevInfo)
{
    // Add subdevice to the list if this is an active GPU device
    const Gpu::Chip* pChip = 0;
    set <string> gpus;
    GpuPtr()->GetGpusUnderTest(&gpus);

    const UINT32 status = GpuPtr()->GetSubdeviceStatus(pciDevInfo, &pChip, gpus);
    if (status == Gpu::gpuStatusValid)
    {
        MASSERT(pChip);

        if (m_GpuSubdevices.size() >= LW0000_CTRL_GPU_MAX_ATTACHED_GPUS)
        {
            Printf(Tee::PriHigh, "Too many active GPU devices in the system!\n");
            return RC::PCI_DEVICE_NOT_FOUND;
        }

        unique_ptr<GpuSubdevice> pSubdev(pChip->CreateGpuSubdevice(
                static_cast<Gpu::LwDeviceId>(pChip->LwId), &pciDevInfo));

        pSubdev->EarlyInitialization();
        // Limit the maximum number of address bits passed to alloc pages to 34
        // i.e. 16GB if bug 632241 is present on any GPU
        if (pSubdev->HasBug(632241))
        {
            Platform::SetMaxAllocPagesAddressBits(34);
        }

        m_GpuSubdevices.push_back(pSubdev.release());
    }
    else
    {
        if (status & Gpu::gpuStatusIgnoredPciDev)
        {
            Printf(Tee::PriNormal,
                    "PCI device %04x:%02x:%02x.%x (CID=%08x,DID=%04x) was "
                    "explicitly ignored.\n",
                    pciDevInfo.PciDomain, pciDevInfo.PciBus, pciDevInfo.PciDevice, pciDevInfo.PciFunction,
                    pciDevInfo.ChipId, pciDevInfo.PciDeviceId);
            m_IgnoredAndObsoleteDevices++;
        }
        else if (status & Gpu::gpuStatusFellOffTheBus)
        {
            Printf(Tee::PriNormal,
                    "PCI device %04x:%02x:%02x.%x is not responding and likely "
                    "fell off the PCI bus\n",
                    pciDevInfo.PciDomain, pciDevInfo.PciBus, pciDevInfo.PciDevice, pciDevInfo.PciFunction);
        }
        else if (status & Gpu::gpuStatusUnknown)
        {
            Printf(Tee::PriNormal,
                    "PCI device %04x:%02x:%02x.%x (CID=0x%08x,DID=0x%04x) is an "
                    "unrecognized GPU.\n Possibly you need a newer MODS "
                    "package.\n",
                    pciDevInfo.PciDomain, pciDevInfo.PciBus, pciDevInfo.PciDevice, pciDevInfo.PciFunction,
                    pciDevInfo.ChipId, pciDevInfo.PciDeviceId);
        }
        else if (status & Gpu::gpuStatusObsolete)
        {
            Printf(Tee::PriNormal,
                    "A %s was recognized (CID=0x%08x,DID=0x%04x) but is no longer "
                    "supported.\nUse an earlier MODS version.\n",
                    Gpu::DeviceIdToString((Gpu::LwDeviceId)(pChip->LwId)).c_str(),
                    pciDevInfo.ChipId, pciDevInfo.PciDeviceId);
            m_IgnoredAndObsoleteDevices++;
        }
        else if (status & Gpu::gpuStatusIgnoredGpuClass)
        {
            Printf(Tee::PriNormal,
                    "A %s was recognized (CID=0x%08x,DID=0x%04x) but was "
                    "explicitly ignored.\n",
                    Gpu::DeviceIdToString((Gpu::LwDeviceId)(pChip->LwId)).c_str(),
                    pciDevInfo.ChipId, pciDevInfo.PciDeviceId);
            m_IgnoredAndObsoleteDevices++;
        }
    }
    return OK;
}

//! \brief Find and allocate the software classes for the devices.
//!
//! This function first iterates over all the Gpu devices, binding their
//! index to an RM GpuId.  This ensures that "Gpu 0" stays "Gpu 0" as far
//! as the user is concerned, even if it happens to be
//! "Device 6 Subdevice 2".
//! This should be called before InitializeAll().
//! \sa InitializeAll()
RC GpuDevMgr::CreateGpuDeviceObjects()
{
    RC rc;
    if (m_Found)
    {
        Printf(Tee::PriHigh, "Already found devices!  Doing nothing.\n");
        return OK;
    }

    LwRmPtr pLwRm;

    LW0000_CTRL_GPU_ATTACH_IDS_PARAMS gpuAttachIdsParams = { {0} };
    gpuAttachIdsParams.gpuIds[0] = LW0000_CTRL_GPU_ATTACH_ALL_PROBED_IDS;
    gpuAttachIdsParams.gpuIds[1] = LW0000_CTRL_GPU_ILWALID_ID;
    CHECK_RC(pLwRm->Control(pLwRm->GetClientHandle(),
                            LW0000_CTRL_CMD_GPU_ATTACH_IDS,
                            &gpuAttachIdsParams,
                            sizeof(gpuAttachIdsParams)));

    LW0000_CTRL_GPU_GET_ATTACHED_IDS_PARAMS gpuAttachedIdsParams = { {0} };

    CHECK_RC(pLwRm->Control(pLwRm->GetClientHandle(),
                            LW0000_CTRL_CMD_GPU_GET_ATTACHED_IDS,
                            &gpuAttachedIdsParams,
                            sizeof(gpuAttachedIdsParams)));

    for (UINT32 gpuInst = 0; gpuInst < m_GpuSubdevices.size(); gpuInst++)
    {
        if (m_GpuSubdevices[gpuInst] == NULL)
            continue;

        MASSERT(gpuAttachedIdsParams.gpuIds[gpuInst] != GpuDevice::ILWALID_DEVICE);

        m_GpuSubdevices[gpuInst]->SetGpuId(gpuAttachedIdsParams.gpuIds[gpuInst]);
        Printf(Tee::PriDebug, "Found attached GpuInstance %d with ID 0x%08x\n",
               gpuInst, m_GpuSubdevices[gpuInst]->GetGpuId());

        UINT32 checkInst;

        // get gpu id info record
        UINT32 deviceInstance, subdeviceInstance;
        CHECK_RC(GetGpuInfo(gpuAttachedIdsParams.gpuIds[gpuInst], &checkInst,
                            &deviceInstance, &subdeviceInstance));

        // attached boards are guaranteed to be in gpuInstance order
        MASSERT(checkInst == gpuInst);

        if (Platform::GetSimulationMode() != Platform::Hardware)
        {
            // verify gpu id on escape interface
            auto pGpuPcie = m_GpuSubdevices[gpuInst]->GetInterface<Pcie>();
            CHECK_RC(Platform::VerifyGpuId(pGpuPcie->DomainNumber(),
                    pGpuPcie->BusNumber(),
                    pGpuPcie->DeviceNumber(),
                    pGpuPcie->FunctionNumber(),
                    m_GpuSubdevices[gpuInst]->GetGpuId()));
        }

        // create the GpuDevice
        if (!m_GpuDevices[deviceInstance])
        {
            m_GpuDevices[deviceInstance] = new GpuDevice();
            CHECK_RC(m_GpuDevices[deviceInstance]->
                     SetDeviceInst(deviceInstance));
        }

        if (m_PreInitialized)
        {
            if (m_GpuDevices[deviceInstance]->GetSubdevice(subdeviceInstance)
                    != m_GpuSubdevices[gpuInst])
            {
                Printf(Tee::PriHigh, "Subdevice enumeration mismatch between PreCreateGpuDeviceObjects and CreateGpuDeviceObjects\n");
                return RC::SOFTWARE_ERROR;
            }
        }
        else
        {
            CHECK_RC(m_GpuDevices[deviceInstance]->
                       AddSubdevice(m_GpuSubdevices[gpuInst], subdeviceInstance));
        }
        m_GpuDevicesByGpuInst[gpuInst] = deviceInstance;
    }

    Printf(Tee::PriLow, "GpuDevMgr found devices.\n");

    m_PreInitialized = true;
    m_Found = true;

    return OK;
}

//! \brief Replacement for CreateGpuDeviceObjects() that bypasses the RM
//!
//! This function creates a set of GpuDevices, just like
//! CreateGpuDeviceObjects(), but without calling the the RM.  Instead, this
//! function uses the structure returned by RmFindDevices() to decide
//! which subdevices to group into devices.  The result isn't as good
//! as CreateGpuDeviceObjects(), but it's good enough for simple tests that only
//! read/write registers.
//!
//! This function is called instead of CreateGpuDeviceObjects() when the
//! -skip_dev_init option is used to supress most of the RM
//! initialization.
//!
//! \param pModsGpuDevices List of devices returned by RmFindDevices()
//!
RC GpuDevMgr::CreateGpuDeviceObjectsWithoutResman()
{
    if (m_Found)
    {
        Printf(Tee::PriHigh, "Already found devices!  Doing nothing.\n");
        return OK;
    }

    RC rc;
    CHECK_RC(PreCreateGpuDeviceObjects());

    m_Found = true;

    return OK;
}

RC GpuDevMgr::PreCreateGpuDeviceObjects()
{
    if (m_PreInitialized)
    {
        return OK;
    }

    RC rc;
    if (m_Found)
    {
        Printf(Tee::PriHigh, "Already found devices!  Doing nothing.\n");
        return OK;
    }

    // Create GpuDevices to hold the GpuSubdevices.  Assume that the
    // deviceInstance numbers are contiguous.
    //
    for (UINT32 gpuInst=0; gpuInst < m_GpuSubdevices.size(); gpuInst++)
    {
        if (m_GpuSubdevices[gpuInst])
        {
            GpuSubdevice* const pGpuSubdevice = m_GpuSubdevices[gpuInst];
            MASSERT(gpuInst == pGpuSubdevice->GetGpuInst());

            // Create the parent device, if needed.
            //
            UINT32 deviceInstance = NumDevices();
            GpuDevice* const pGpuDevice = new GpuDevice();
            m_GpuDevices[deviceInstance] = pGpuDevice;
            CHECK_RC(pGpuDevice->SetDeviceInst(deviceInstance));

            // Add the GpuSubdevice to the GpuDevice.
            //
            MASSERT(pGpuSubdevice->GetParentDevice() == NULL);
            UINT32 subdeviceInstance = pGpuDevice->GetNumSubdevices();
            pGpuSubdevice->SetGpuId(gpuInst);
            CHECK_RC(pGpuDevice->AddSubdevice(pGpuSubdevice,
                        subdeviceInstance));
            CHECK_RC(pGpuSubdevice->SetParentDevice(pGpuDevice));
            m_GpuDevicesByGpuInst[gpuInst] = pGpuDevice->GetDeviceInst();
        }
    }

    m_PreInitialized = true;

    return OK;
}

//! Helper function to get the gpu info from the RM
RC GpuDevMgr::GetGpuInfo(UINT32 GpuId, UINT32 *GpuInstance,
                         UINT32 *DeviceInstance, UINT32 *SubdeviceInstance)
{
    RC rc = OK;

    LW0000_CTRL_GPU_GET_ID_INFO_PARAMS gpuIdInfoParams = {0};

    gpuIdInfoParams.szName = LwP64_NULL;
    gpuIdInfoParams.gpuId = GpuId;
    CHECK_RC(LwRmPtr()->Control(LwRmPtr()->GetClientHandle(),
                                LW0000_CTRL_CMD_GPU_GET_ID_INFO,
                                &gpuIdInfoParams, sizeof(gpuIdInfoParams)));

    *GpuInstance = gpuIdInfoParams.gpuInstance;
    *DeviceInstance = gpuIdInfoParams.deviceInstance;
    *SubdeviceInstance = gpuIdInfoParams.subDeviceInstance;

    return rc;
}

//! \brief Destroy GPU objects in RM.
void GpuDevMgr::DestroySubdevicesInRm()
{
    for (UINT32 i=0; i < m_GpuSubdevices.size(); i++)
    {
        // RM needs us to destroy the GPU instances from highest to lowest
        const UINT32 gpuInst = static_cast<UINT32>(m_GpuSubdevices.size() - i - 1);
        if (m_GpuSubdevices[gpuInst] && !m_GpuSubdevices[gpuInst]->IsSOC())
        {
            m_GpuSubdevices[gpuInst]->DestroyInRm();
        }
    }
}

//! \brief Free all GPU subdevice objects.
void GpuDevMgr::FreeDevices()
{
    if (m_Initialized)
    {
        Printf(Tee::PriHigh,
               "Warning: freeing devices without calling ShutdownAll()!\n");
    }

    for (UINT32 gpuInst=0; gpuInst < m_GpuSubdevices.size(); gpuInst++)
    {
        if (m_GpuSubdevices[gpuInst])
        {
            delete m_GpuSubdevices[gpuInst];
            m_GpuSubdevices[gpuInst] = 0;
        }
    }
    m_GpuSubdevices.clear();
}

//! \brief Free the software classes for the devices.
//!
//! This function doesn't do a shutdown of the devices.  It should be
//! called only after ShutdownAll().
//! \sa ShutdownAll()
void GpuDevMgr::FreeDeviceObjects()
{
    if (m_Initialized)
    {
        Printf(Tee::PriHigh,
               "Warning: freeing devices without calling ShutdownAll()!\n");
    }

    for (UINT32 dev = 0; dev < LW0080_MAX_DEVICES; dev ++)
    {
        if (m_GpuDevices[dev])
            delete m_GpuDevices[dev];
        m_GpuDevices[dev] = 0;
    }

    m_Found = false;
}

//! \brief Return the number of devices.
//!
//! This function returns the number of active devices.  Keep in mind that
//! the active device instances are not necessarily contiguous.
UINT32 GpuDevMgr::NumDevices()
{
    UINT32 count = 0;

    for (UINT32 dev = 0; dev < LW0080_MAX_DEVICES; dev ++)
    {
        if (m_GpuDevices[dev])
            count++;
    }

    return count;
}

//! \brief Return the number of sub devices.
//!
//! This function returns the number of active sub devices.
UINT32 GpuDevMgr::NumGpus()
{
    UINT32 count = 0;

    for (UINT32 dev = 0; dev < m_GpuSubdevices.size(); dev++)
    {
        if (m_GpuSubdevices[dev])
            count++;
    }

    return count;
}

//--------------------------------------------------------------------
//! \brief Hook the ISRs for all subdevices.
//!
//! This method either adds the ISRs to the interrupt-vector table or
//! starts the interrupt-polling function, depending on whether we are
//! using hardware interrupts or polling.
//!
//! Note: UnhookInterrupts() is called after FreeDevices(), so don't
//! add anything to the interrupt-processing algorithms that uses
//! GpuDevices.  It's OK to use GpuSubdevices.
//!
//! \sa UnhookInterrupts()
//!
//! \param pollInterrupts If true, then poll for interrupts instead of
//!     using hardware interrupts.
//! \param useMsi If true, then use MSI interrupts.  This argument is
//!     ignored if pollInterrupts is set.
//!
RC GpuDevMgr::HookInterrupts(bool pollInterrupts, UINT32 irqType)
{
    // If this MODS runs against the driver running in kernel mode,
    // there is no way to hook interrupts, so don't even try.
    // If we did try, in the worst case we would have spawned the poll interrupt
    // thread, which could have unintended consequences.
    if (!Platform::HasClientSideResman())
    {
        return RC::OK;
    }

    RC rc;

    MASSERT(MODS_XP_IRQ_NUM_TYPES > irqType);

    UnhookInterrupts();
    Platform::LowerIrql(Platform::NormalIrql);

    bool haveSocGpu = false;
    bool havePciGpu = false;
    for (GpuSubdevice *pGpuSubdevice = GetFirstGpu();
         pGpuSubdevice != NULL;
         pGpuSubdevice = GetNextGpu(pGpuSubdevice))
    {
        if (pGpuSubdevice->IsSOC())
        {
            haveSocGpu = true;
        }
        else
        {
            havePciGpu = true;
        }
    }

    // Hook PCI interrupts
    if (havePciGpu)
    {
        if (pollInterrupts)
        {
            Printf(Tee::PriLow, "Polling interrupts instead of hooking\n");
            m_ContinuePollingInterruptThread = true;
            m_PollingInterruptThreadID =
                Tasker::CreateThread(PollingInterruptThread, this,
                                     0, "GPU polling interrupt");
            Printf(Tee::PriDebug, "Hooked polling GPU ISR.\n");
        }
        else
        {
            const char* irqName = "unknown";
            switch (irqType)
            {
                case MODS_XP_IRQ_TYPE_INT:  irqName = "legacy interrupts"; break;
                case MODS_XP_IRQ_TYPE_MSI:  irqName = "MSI"; break;
                case MODS_XP_IRQ_TYPE_MSIX: irqName = "MSI-X"; break;
                default:                 break;
            }
            Printf(Tee::PriLow, "Hooking %s\n", irqName);
            for (GpuSubdevice *pGpuSubdevice = GetFirstGpu();
                 pGpuSubdevice != NULL;
                 pGpuSubdevice = GetNextGpu(pGpuSubdevice))
            {
                if (!pGpuSubdevice->IsSOC())
                {
                    rc = pGpuSubdevice->HookInterrupts(irqType);

                    if (OK != rc)
                    {
                        Printf(Tee::PriNormal,
                               "Failed to hook %s interrupts\n",
                               GpuSubdevice::GetIRQTypeName(irqType));

                        if (MODS_XP_IRQ_TYPE_INT == irqType)
                        {
                            Printf(Tee::PriNormal,
                               "Failed to hook INTA interrupts. falling back to MSI.\n");
                            CHECK_RC(pGpuSubdevice->HookInterrupts(MODS_XP_IRQ_TYPE_MSI));
                        }
                    }
                }
            }
        }
    }

    // Hook SOC interrupts
    if (haveSocGpu)
    {
        for (GpuSubdevice *pGpuSubdevice = GetFirstGpu();
             pGpuSubdevice != NULL;
             pGpuSubdevice = GetNextGpu(pGpuSubdevice))
        {
            if (pGpuSubdevice->IsSOC())
            {
                CHECK_RC(pGpuSubdevice->HookInterrupts(MODS_XP_IRQ_TYPE_INT));
            }
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Unhook the ISRs for all subdevices
//!
//! This method undoes any ISRs that were set up by HookInterrupts().
//! It should be safe to call this method multiple times, even without
//! calling HookInterrupts() first.
//!
void GpuDevMgr::UnhookInterrupts()
{
#ifdef SIM_BUILD
    // Disable interrupts in sim since handlers will be unavailable after
    // unhooking interrupts.
    // This behavior may be unwanted in multi-GPU cases so it's only applied
    // for sim MODS for now.
    if (Platform::GetLwrrentIrql() == Platform::NormalIrql)
    {
        Platform::ProcessPendingInterrupts();
        Platform::RaiseIrql(Platform::ElevatedIrql);
    }
#endif

    // Kill the polling-interrupt thread, if any
    //
    if (m_PollingInterruptThreadID != Tasker::NULL_THREAD)
    {
        m_ContinuePollingInterruptThread = false;
        Tasker::Join(m_PollingInterruptThreadID);
        m_PollingInterruptThreadID = Tasker::NULL_THREAD;
    }

    // Unhook interrupts
    //
    for (GpuSubdevice *pGpuSubdevice = GetFirstGpu();
            pGpuSubdevice != NULL;
            pGpuSubdevice = GetNextGpu(pGpuSubdevice))
    {
        pGpuSubdevice->UnhookISR();
    }
}

//--------------------------------------------------------------------
//! \brief Service interrupts for the given PCI device
//!
RC GpuDevMgr::ServiceInterrupt
(
    UINT32 pciDomain,
    UINT32 pciBus,
    UINT32 pciDevice,
    UINT32 pciFunction,
    UINT32 irqNum
)
{
    for (UINT32 gpuInst=0; gpuInst < m_GpuSubdevices.size(); gpuInst++)
    {
        GpuSubdevice* const pSubdev = m_GpuSubdevices[gpuInst];
        auto pGpuPcie = pSubdev->GetInterface<Pcie>();
        if ((pGpuPcie->DomainNumber() == pciDomain) &&
            (pGpuPcie->BusNumber() == pciBus) &&
            (pGpuPcie->DeviceNumber() == pciDevice) &&
            (pGpuPcie->FunctionNumber() == pciFunction))
        {
            return pSubdev->ServiceInterrupt(irqNum);
        }
    }
    return RC::PCI_DEVICE_NOT_FOUND;
}

//--------------------------------------------------------------------
//! \brief Service interrupts for devices with the specified IRQ number
//!
//! If IRQ number is ~0U, the function attempts to service interrupts
//! for all GPU subdevices.
//!
RC GpuDevMgr::ServiceInterrupt(UINT32 irqNum)
{
    bool found = false;
    for (UINT32 gpuInst=0; gpuInst < m_GpuSubdevices.size(); gpuInst++)
    {
        GpuSubdevice* const pSubdev = m_GpuSubdevices[gpuInst];
        if ((~0U == irqNum) || pSubdev->HasIrq(irqNum))
        {
            const RC rc = pSubdev->ServiceInterrupt(irqNum);
            if (OK == rc)
            {
                found = true;
            }
            else if (RC::UNEXPECTED_INTERRUPTS != rc)
            {
                return rc;
            }
        }
    }
    return found ? OK : RC::UNEXPECTED_INTERRUPTS;
}

//--------------------------------------------------------------------
//! \brief Run 1Hz callbacks in RM
//!
void GpuDevMgr::Run1HzCallbacks()
{
    for (UINT32 gpuInst=0; gpuInst < m_GpuSubdevices.size(); gpuInst++)
    {
        if (m_GpuSubdevices[gpuInst])
        {
            RmRun1HzCallbackNow(gpuInst);
        }
    }
}

//--------------------------------------------------------------------
//! \brief Reset any fuses, registers for all subdevices
//!
//! This method undoes fuse and register changes that were performed by the MODS
//! command line.
//!
void GpuDevMgr::PostRmShutdown()
{

    for (GpuSubdevice *pGpuSubdevice = GetFirstGpu();
         pGpuSubdevice != NULL;
         pGpuSubdevice = GetNextGpu(pGpuSubdevice))
    {
        pGpuSubdevice->PostRmShutdown();
    }
}

//--------------------------------------------------------------------
//! \brief Thread that polls the GPUs for an interrupt
//!
//! This thread is used when mods is in polled-interrupt mode.  It
//! polls each GPU, and calls RmIsr() when a GPU has a pending
//! interrupt.  See HookInterrupts().
//!
/* static */ void GpuDevMgr::PollingInterruptThread(void *pGpuDevMgrArg)
{
    GpuDevMgr *pGpuDevMgr = (GpuDevMgr*)pGpuDevMgrArg;

    UINT64 waitNsec = 0;
    if (Tasker::GetMaxHwPollHz())
        waitNsec = (UINT64) (1e9 / Tasker::GetMaxHwPollHz());

    while (pGpuDevMgr->m_ContinuePollingInterruptThread)
    {
        Tasker::SetCanThreadYield(false);
        for (GpuSubdevice *pGpuSubdevice = pGpuDevMgr->GetFirstGpu();
             pGpuSubdevice != NULL;
             pGpuSubdevice = pGpuDevMgr->GetNextGpu(pGpuSubdevice))
        {
            if (!pGpuSubdevice->IsSOC() && pGpuSubdevice->HasPendingInterrupt())
            {
                RmIsr(pGpuSubdevice->GetGpuInst());
                Platform::InterruptProcessed();
            }
        }
        Tasker::SetCanThreadYield(true);
        if (waitNsec)
        {
            UINT64 tmp = Platform::GetTimeNS() + waitNsec;
            do
            {
                Tasker::Yield();
            }
            while (tmp > Platform::GetTimeNS());
        }
        else
        {
            Tasker::Yield();
        }
    }
}

RC GpuDevMgr::InitializeAllWithoutRm()
{
    RC rc;
    for (UINT32 dev = 0; dev < LW0080_MAX_DEVICES; dev++)
    {
        if (m_GpuDevices[dev])
        {
            CHECK_RC(m_GpuDevices[dev]->InitializeWithoutRm());
        }
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Initialize all of the found devices.
//!
//! This should only be called after CreateGpuDeviceObjects()
//! \sa CreateGpuDeviceObjects()
RC GpuDevMgr::InitializeAll()
{
    RC rc = OK;

    if (!m_Found)
    {
        Printf(Tee::PriHigh,
               "Cannot initialize devices before calling CreateGpuDeviceObjects()!\n");
        return RC::SOFTWARE_ERROR;
    }

    bool AssumeBattery;
    JavaScriptPtr pJs;
    LwRmPtr pLwRm;
    LW0000_CTRL_SYSTEM_NOTIFY_EVENT_PARAMS powerParam = {0};

    powerParam.eventType = LW0000_CTRL_SYSTEM_EVENT_TYPE_POWER_SOURCE;

    CHECK_RC(pJs->GetProperty(pJs->GetGlobalObject(), "g_AssumeBatteryPower", &AssumeBattery));
    if(!AssumeBattery)
    {
        powerParam.eventData = LW0000_CTRL_SYSTEM_EVENT_DATA_POWER_AC;
    }
    else
    {
        powerParam.eventData = LW0000_CTRL_SYSTEM_EVENT_DATA_POWER_BATTERY;
    }

    UINT32 forcePerf20DevInstMask;
    CHECK_RC(pJs->GetProperty(pJs->GetGlobalObject(),
                              "g_ForcePS20DevInstMask",
                              &forcePerf20DevInstMask));

    rc = pLwRm->Control(pLwRm->GetClientHandle(),
                   LW0000_CTRL_CMD_SYSTEM_NOTIFY_EVENT,
                   &powerParam,
                   sizeof(powerParam));
    if (RC::LWRM_NOT_SUPPORTED == rc)
    {
        rc.Clear();
    }
    CHECK_RC(rc);

    if (m_Initialized)
      Printf(Tee::PriNormal, "GpuDevMgr already initialized.  Some device "
             "initializations may do nothing.\n");

    for (UINT32 dev = 0; dev < LW0080_MAX_DEVICES; dev++)
    {
        if (m_GpuDevices[dev])
        {
            if (forcePerf20DevInstMask & (1 << dev))
            {
                m_GpuDevices[dev]->SetForcePerf20(true);
            }
            CHECK_RC(m_GpuDevices[dev]->Initialize(m_NumUserdAllocs, m_UserdLocation));
        }
    }

    m_Initialized = true;

    return rc;
}

//! \brief Shutdown all of the found devices.
//!
//! This should be called before FreeDevices().
//! \sa FreeDevices()
RC GpuDevMgr::ShutdownAll()
{
    StickyRC firstRc;

    if (!m_Initialized)
    {
        Printf(Tee::PriLow,
               "GpuDevMgr not initialized.  "
               "Device shutdowns will likely do nothing.\n");
    }

    ShutdownDrivers();

    for (auto* pDevice : m_GpuDevices)
    {
        if (pDevice)
        {
            firstRc = pDevice->Shutdown();
        }
    }

    m_Initialized = false;

    return firstRc;
}

//! \brief Get the subdevice by gpu instance
GpuSubdevice *GpuDevMgr::GetSubdeviceByGpuInst(UINT32 GpuInstance) const
{
    if (GpuInstance >= m_GpuSubdevices.size())
    {
        return 0;
    }
    return m_GpuSubdevices[GpuInstance];
}

UINT32 GpuDevMgr::GetDevInstByGpuInst(UINT32 gpuInst) const
{
    GpuSubdevice* const pSubdev = GetSubdeviceByGpuInst(gpuInst);
    return pSubdev ? pSubdev->DevInst() : ~0U;
}

//! \brief Get the device of a certain instance
RC GpuDevMgr::GetDevice(UINT32 DevInst, Device **Device)
{
    if (DevInst >= LW0080_MAX_DEVICES)
    {
        Printf(Tee::PriAlways, "Device instance too high!\n");
        return RC::ILWALID_DEVICE_ID;
    }

    if (!m_GpuDevices[DevInst])
    {
        Printf(Tee::PriAlways, "Device %d does not exist!\n", DevInst);
        return RC::ILWALID_DEVICE_ID;
    }

    *Device = m_GpuDevices[DevInst];

    return OK;
}

//! \brief Finds GPU device which has primary subdevice (boot VGA).
//!
//! Returns NULL if there is no primary LWPU GPU in the system.
GpuDevice* GpuDevMgr::GetPrimaryDevice()
{
    GpuDevice* pGpuDev = GetFirstGpuDevice();
    GpuDevice* pPrimaryGpuDev = 0;
    for ( ; pGpuDev != NULL; pGpuDev = GetNextGpuDevice(pGpuDev))
    {
        for (UINT32 i = 0; i < pGpuDev->GetNumSubdevices(); i++)
        {
            GpuSubdevice* pSubdev = pGpuDev->GetSubdevice(i);
            if (pSubdev && pSubdev->IsPrimary())
            {
                if (pPrimaryGpuDev)
                {
                    Printf(Tee::PriHigh, "Found two primary subdevices!\n");
#ifndef SIM_BUILD
                    MASSERT(!"Found two primary subdevices!");
#endif
                }
                else
                {
                    pPrimaryGpuDev = pGpuDev;
                }
            }
        }
    }
    return pPrimaryGpuDev;
}

//! \brief Easier "looping" get function, deals with "gappy-ness".
//!
//! Gpu device instance numbers can be "gappy".
//! I.e. we might have two devices, instances 0 and 2.
//!
//! If prev is NULL, returns first device.
//! Else, returns GpuDevice* with next higher instance number.
//! If prev is the highest instance number GpuDevice, returns NULL.
GpuDevice * GpuDevMgr::GetNextGpuDevice(GpuDevice * prev)
{
    if (!m_PreInitialized)
        return NULL;

    UINT32 nextGpuDevInst;
    if (prev)
    {
        // We will return the next GpuDevice after this one
        // or NULL if it is the last one.
        nextGpuDevInst = prev->GetDeviceInst()+1;
    }
    else
    {
        // Given NULL, we will return the first GpuDevice.
        nextGpuDevInst = 0;
    }

    // Skip over gaps, return first allocated GpuDevice.
    while (nextGpuDevInst < LW0080_MAX_DEVICES)
    {
        if (m_GpuDevices[nextGpuDevInst])
            return m_GpuDevices[nextGpuDevInst];

        nextGpuDevInst++;
    }

    // "prev" must have been the last GpuDevice.
    return NULL;
}

//! \brief "Looping" get function
//!
//! Returns GpuSubdevices in order of increasing GpuInstance numbers.
//!
//! If prev is NULL, returns first subdevice.
//! Else, returns GpuSubdevice* with next higher instance number.
//! If prev is the highest instance number GpuSubdevice, returns NULL.
GpuSubdevice * GpuDevMgr::GetNextGpu(GpuSubdevice * prev)
{
    UINT32 nextGpuInstance;
    if (prev)
    {
        nextGpuInstance = prev->GetGpuInst()+1;
    }
    else
    {
        // Given NULL, we will return the first GpuSubdevice.
        nextGpuInstance = 0;
    }

    // Skip over gaps, return first allocated GpuDevice.
    while (nextGpuInstance < m_GpuSubdevices.size())
    {
        if (m_GpuSubdevices[nextGpuInstance])
            return m_GpuSubdevices[nextGpuInstance];

        nextGpuInstance++;
    }

    // "prev" must have been the last GpuDevice.
    return NULL;
}

//! \brief List the current Gpu<->Device mappings
void GpuDevMgr::ListDevices(Tee::Priority pri)
{
    Printf(pri, "GPU INST:\t\tDEVICE:\n");
    for (UINT32 gpuInst = 0; gpuInst < LW0000_CTRL_GPU_MAX_ATTACHED_GPUS;
         gpuInst++)
    {
        if (m_GpuDevicesByGpuInst[gpuInst] != GpuDevice::ILWALID_DEVICE)
          Printf(pri, "%d\t\t\t%d\n", gpuInst,
                 m_GpuDevicesByGpuInst[gpuInst]);
    }
}
//! \brief Prints error message idicating an error happened when trying to
//!        initialize a device.
void PrintBadRC()
{
    Printf(Tee::PriAlways,
           "Bad RC in initialization of old or new devices.  Potentially fatal error!\n");
    MASSERT(!"Bad RC in initialization of old or new devices.  Potentially fatal error!\n");
}

//! \brief Attempt to unlink a device by device instance.
RC GpuDevMgr::UnlinkDevice(UINT32 DevInst)
{
    RC rc = OK;

    if (!m_GpuDevices[DevInst])
    {
        Printf(Tee::PriHigh, "Invalid device instance %d\n", DevInst);
        return RC::ILWALID_ARGUMENT;
    }

    if (m_GpuDevices[DevInst]->GetNumSubdevices() == 1)
    {
        Printf(Tee::PriHigh,
               "Device instance %d is not in an SLI configuration.\n",
               DevInst);
        return RC::ILWALID_ARGUMENT;
    }

    LW0080_CTRL_GPU_GET_DISPLAY_OWNER_PARAMS displayParams = {0};

    CHECK_RC(LwRmPtr()->ControlByDevice(m_GpuDevices[DevInst],
                                        LW0080_CTRL_CMD_GPU_GET_DISPLAY_OWNER,
                                        &displayParams,
                                        sizeof (displayParams)));

    UINT32 ids[LW2080_MAX_SUBDEVICES];

    for (UINT32 gpu = 0; gpu < LW2080_MAX_SUBDEVICES; gpu++)
    {
        if (m_GpuDevices[DevInst]->GetSubdevice(gpu))
          ids[gpu] = m_GpuDevices[DevInst]->GetSubdevice(gpu)->GetGpuId();
        else ids[gpu] = GpuDevice::ILWALID_DEVICE;
    }

    ShutdownDrivers();

    // free in the RM and in code the old devices/subdevices
    Printf(Tee::PriLow, "Shutting down and freeing old device.\n");
    m_GpuDevices[DevInst]->Shutdown();

    RC unlinkRc;

    // Cleanup
    bool printError = true;
    DEFER
    {
        if (printError)
        {
            PrintBadRC();
        }
    };

    Printf(Tee::PriDebug, "Unlinking device in lwrm.cpp...\n");
    // unlink the device into a new device
    if ((unlinkRc = LwRmPtr()->UnlinkDevice(DevInst)) != OK)
    {
        Printf(Tee::PriHigh, "Unlinking failed!  Restoring old state.\n");
        // reinitialize old device
        CHECK_RC(m_GpuDevices[DevInst]->Initialize(m_NumUserdAllocs, m_UserdLocation));
        return unlinkRc;
    }
    else
    {
        Printf(Tee::PriLow, "Unlinking succeeded.  Freeing old device.\n");
        delete m_GpuDevices[DevInst];
        m_GpuDevices[DevInst] = 0;
    }

    // any failure at this point should be considered fatal

    for (UINT32 gpu = 0; gpu < LW2080_MAX_SUBDEVICES; gpu++)
    {
        if (ids[gpu] == GpuDevice::ILWALID_DEVICE)
            continue;

        UINT32 deviceInst = 0, subdeviceInst = 0, gpuInst = 0;
        CHECK_RC(GetGpuInfo(ids[gpu], &gpuInst,
                                  &deviceInst, &subdeviceInst));

        // this device should not exist yet since it's a single-GPU device
        MASSERT(!m_GpuDevices[deviceInst]);
        Printf(Tee::PriLow,
               "Allocating new GpuDevice %d (gpu inst %d, gpuId 0x%x)...\n",
               deviceInst, gpuInst, ids[gpu]);
        // allocate the new device in code
        m_GpuDevices[deviceInst] = new GpuDevice();
        // add the GPU as subdevices
        CHECK_RC(m_GpuDevices[deviceInst]->
                         AddSubdevice(GetSubdeviceByGpuInst(gpuInst),
                                      subdeviceInst));
        m_GpuDevicesByGpuInst[gpuInst] = deviceInst;
        // initialize the new devices, allocating them and the subdevices
        // in the RM
        Printf(Tee::PriLow, "Reinitializing new device %d...\n", deviceInst);
        CHECK_RC(m_GpuDevices[deviceInst]->Initialize(m_NumUserdAllocs, m_UserdLocation, deviceInst));
        Printf(Tee::PriNormal,
               "Successfully unlinked GPU inst %d as device %d\n",
               gpuInst, deviceInst);
    }

    if (rc == OK)
        printError = false;
    return rc;
}

//
//! \brief Translates Gpu Instances to Gpu IDs (helper for the Link function).
//
RC GpuDevMgr::BuildGpuIdList
(
    UINT32         NumGpus,       //!< Number of UINT32s in pInsts and pRetIds arrays.
    const UINT32 * pInsts,        //!< Array of Gpu Instance numbers to translate.
    UINT32         DispInst,      //!< Gpu Instance that is the desired display Gpu.
    UINT32 *       pRetIds,       //!< Output Gpu ID array.
    UINT32 *       pRetDispId     //!< Output Gpu ID that matches DispInst.
)
{
    UINT64 seenMask = 0;
    UINT32 dispInstFound = 0;
    UINT32 gpu;
    UINT32 gpuInst;
    for (gpu = 0; gpu < NumGpus; gpu++)
    {
        gpuInst = pInsts[gpu];
        if (gpuInst >= m_GpuSubdevices.size()
            || (seenMask & ((UINT64)1 << gpuInst)))
        {
            Printf(Tee::PriHigh, "Invalid GPU instance.\n");
            return RC::ILWALID_ARGUMENT;
        }
        seenMask |= ((UINT64)1 << gpuInst);
        dispInstFound |= (gpuInst == DispInst);

        if (NULL == m_GpuSubdevices[gpuInst] ||
            m_GpuSubdevices[gpuInst]->GetGpuId() == GpuDevice::ILWALID_DEVICE)
        {
            Printf(Tee::PriHigh,
                   "GPU instance %d doesn't exist (ILWALID_DEVICE).\n",
                   gpuInst);
            return RC::ILWALID_ARGUMENT;
        }

        pRetIds[gpu] = m_GpuSubdevices[gpuInst]->GetGpuId();
        Printf(Tee::PriDebug, "Using 0x%x for ids[%d]\n",pRetIds[gpu], gpu);

        UINT32 devInst = m_GpuDevicesByGpuInst[gpuInst];

        if (m_GpuDevices[devInst]->GetNumSubdevices() > 1)
        {
            Printf(Tee::PriHigh,
                   "Gpu instance %d is already in a SLI device.\n", gpuInst);
            return RC::SLI_GPU_LINKED;
        }
    }
    if (!dispInstFound)
    {
        Printf(Tee::PriHigh,
               "Gpu display instance %d is not in gpuInst list.\n", DispInst);
        return RC::ILWALID_ARGUMENT;
    }
    else
    {
        *pRetDispId = m_GpuSubdevices[DispInst]->GetGpuId();
    }
    return OK;
}

//! \brief Query the RM to get a valid SLI configuration using the user
//!  supplied gpuIds
//!
//! This method will query the RM for all possible valid SLI configurations an
//! pick the first configuration that has all of the user supplied gpuIds.
//
RC GpuDevMgr::GetValidConfiguration
(
    UINT32         NumGpus,      //!< Number of UINT32s in input and output arrays.
    const UINT32 * pIds,         //!< Array of Gpu IDs that we're requesting to
                                 //!<   link, which might be in the wrong order.
    UINT32         DispId,       //!< ID of the requested display Gpu.
    UINT32 *       pRetIds,      //!< Output array of Gpu IDs.  This contains the
                                 //!<   same IDs as pIDs but possibly reordered.
    UINT32 *       pRetDispIndex //!< Output index of DispId in pRetIds.
)
{
    RC rc = OK;
    UINT32 gpuMask = (1 << NumGpus)-1;
    vector<LW0000_CTRL_GPU_SLI_CONFIG> SliConfigList;
    vector<UINT32> SliIlwalidReasonList;

    // Get all the valid SliConfigurations
    CHECK_RC(LwRmPtr()->GetValidConfigurations(&SliConfigList));

    rc = RC::ILWALID_ARGUMENT;
    if (SliConfigList.size() != 0)
    {
        ListSLIConfigurations(Tee::PriLow,
                              SliConfigList,
                              SliIlwalidReasonList);

        for (UINT32 thisCfg=0; thisCfg<(UINT32)SliConfigList.size(); thisCfg++)
        {
            // Wrong count move along to next config
            if( NumGpus != SliConfigList[thisCfg].gpuCount)
                continue;

            UINT32 foundMask=0;
            UINT32 requestedDispIndex = ~0U;
            UINT32 firstPossibleDispIndex = ~0U;

            for (UINT32 gpu=0; gpu<NumGpus; gpu++)
            {
                for (UINT32 j=0; j<NumGpus; j++)
                {
                    if (pIds[gpu] == SliConfigList[thisCfg].gpuIds[j])
                    {
                        foundMask |= (1 << gpu);
                        const bool canDisplay = 0 ==
                            (SliConfigList[thisCfg].noDisplayGpuMask & (1 << j));
                        if (canDisplay)
                        {
                            if (DispId == SliConfigList[thisCfg].gpuIds[j])
                                requestedDispIndex = j;
                            if (firstPossibleDispIndex == ~0U)
                                firstPossibleDispIndex = j;
                        }
                        break;
                    }
                }
            }
            // Did we find all of the Ids?
            if (foundMask == gpuMask)
            {
                UINT32 dispIndex = requestedDispIndex;
                if ((dispIndex == ~0U) && (firstPossibleDispIndex != ~0U))
                {
                    dispIndex = firstPossibleDispIndex;
                    Printf(Tee::PriNormal, "Couldn't make 0x%x GPU to be display"
                       " master, using 0x%x instead.\n", DispId, SliConfigList[thisCfg].gpuIds[dispIndex]);
                }
                if (dispIndex != ~0U)
                {
                    // found a valid config so copy the ids
                    for (UINT32 j=0; j<NumGpus; j++)
                        pRetIds[j] = SliConfigList[thisCfg].gpuIds[j];
                    *pRetDispIndex = dispIndex;
                    rc.Clear();
                }
            }
        }
    }

    if (rc != OK)
    {
        RC ilwalidSliRc;

        Printf(Tee::PriHigh, "No valid/matching SLI configurations found "
                "for display GPU %u.\n", DispId);

        SliConfigList.clear();
        ilwalidSliRc = LwRmPtr()->GetIlwalidConfigurations(&SliConfigList,
                                                        &SliIlwalidReasonList);
        if (ilwalidSliRc == OK)
        {
            ListSLIConfigurations(Tee::PriLow,
                                  SliConfigList,
                                  SliIlwalidReasonList);
        }
    }

    return rc;
}

//! \brief Confirm this list of GpuIds is a valid SLI configuration
//!
RC GpuDevMgr::ValidateConfiguration
(
    UINT32 NumGpus, //!< Number of UINT32s in input and output arrays.
    UINT32 *pIds    //!< Array of Gpu IDs that we're requesting to
                    //!< link, which must be in the correct order.
)
{
    RC rc = OK;
    UINT32 sliStatus = 0;
    UINT32 displayGpuIndex = 0xBAD;
    CHECK_RC(LwRmPtr()->ValidateConfiguration(NumGpus, pIds, &displayGpuIndex, &sliStatus));

    if (sliStatus)
    {
        struct LinkFailBit
        {
            UINT32 Mask;
            const char * String;
        };
        LinkFailBit LinkFailBits[] =
        {
            { LW0000_CTRL_SLI_STATUS_ILWALID_GPU_COUNT,             "ILWALID_GPU_COUNT" },
            { LW0000_CTRL_SLI_STATUS_OS_NOT_SUPPORTED,              "OS_NOT_SUPPORTED" },
            { LW0000_CTRL_SLI_STATUS_OS_ERROR,                      "OS_ERROR" },
            { LW0000_CTRL_SLI_STATUS_NO_VIDLINK,                    "NO_VIDLINK" },
            { LW0000_CTRL_SLI_STATUS_INSUFFICIENT_LINK_WIDTH,       "INSUFFICIENT_LINK_WIDTH" },
            { LW0000_CTRL_SLI_STATUS_CPU_NOT_SUPPORTED,             "CPU_NOT_SUPPORTED" },
            { LW0000_CTRL_SLI_STATUS_GPU_NOT_SUPPORTED,             "GPU_NOT_SUPPORTED" },
            { LW0000_CTRL_SLI_STATUS_BUS_NOT_SUPPORTED,             "BUS_NOT_SUPPORTED" },
            { LW0000_CTRL_SLI_STATUS_CHIPSET_NOT_SUPPORTED,         "CHIPSET_NOT_SUPPORTED" },
            { LW0000_CTRL_SLI_STATUS_GPU_MISMATCH,                  "GPU_MISMATCH" },
            { LW0000_CTRL_SLI_STATUS_ARCH_MISMATCH,                 "ARCH_MISMATCH" },
            { LW0000_CTRL_SLI_STATUS_IMPL_MISMATCH,                 "IMPL_MISMATCH" },
            { LW0000_CTRL_SLI_STATUS_SLI_WITH_TCC_NOT_SUPPORTED,    "SLI_WITH_TCC_NOT_SUPPPORTED" },
            { LW0000_CTRL_SLI_STATUS_PCI_ID_MISMATCH,               "PCI_ID_MISMATCH" },
            { LW0000_CTRL_SLI_STATUS_FB_MISMATCH,                   "FB_MISMATCH" },
            { LW0000_CTRL_SLI_STATUS_VBIOS_MISMATCH,                "VBIOS_MISMATCH" },
            { LW0000_CTRL_SLI_STATUS_QUADRO_MISMATCH,               "QUADRO_MISMATCH" },
            { LW0000_CTRL_SLI_STATUS_BUS_TOPOLOGY_ERROR,            "BUS_TOPOLOGY_ERROR" },
            { LW0000_CTRL_SLI_STATUS_CONFIGSPACE_ACCESS_ERROR,      "CONFIGSPACE_ACCESS_ERROR" },
            { LW0000_CTRL_SLI_STATUS_INCONSISTENT_CONFIG_SPACE,     "INCONSISTENT_CONFIG_SPACE" },
            { LW0000_CTRL_SLI_STATUS_CONFIG_NOT_SUPPORTED,          "CONFIG_NOT_SUPPORTED" },
            { LW0000_CTRL_SLI_STATUS_RM_NOT_SUPPORTED,              "RM_NOT_SUPPORTED" },
            { LW0000_CTRL_SLI_STATUS_MOBILE_MISMATCH,               "MOBILE_MISMATCH" },
            { LW0000_CTRL_SLI_STATUS_ECC_MISMATCH,                  "ECC_MISMATCH" },
            { LW0000_CTRL_SLI_STATUS_INSUFFICIENT_FB,               "INSUFFICIENT_FB" },
            { LW0000_CTRL_SLI_STATUS_SLI_COOKIE_NOT_PRESENT,        "SLI_COOKIE_NOT_PRESENT" },
            { LW0000_CTRL_SLI_STATUS_SLI_FINGER_NOT_SUPPORTED,      "SLI_FINGER_NOT_SUPPORTED" },
            { LW0000_CTRL_SLI_STATUS_SLI_WITH_ECC_NOT_SUPPORTED,    "SLI_WITH_ECC_NOT_SUPPPORTED" },
            { LW0000_CTRL_SLI_STATUS_GR_MISMATCH,                   "GR_MISMATCH" },
        };

        UINT32 reasonsNotReportedYet = sliStatus;
        Printf(Tee::PriNormal, "Invalid SLI configuration: 0x%x\n", sliStatus);
        for(UINT32 b = 0; b < sizeof(LinkFailBits)/sizeof(LinkFailBits[0]); b++)
        {
            if (reasonsNotReportedYet & LinkFailBits[b].Mask)
            {
                Printf(Tee::PriNormal, "    %s\n", LinkFailBits[b].String);
                reasonsNotReportedYet &= ~LinkFailBits[b].Mask;
            }
        }
        // Assert that LinkFailBits is not out of sync with ctrl0000gpu.h.
        MASSERT(0 == reasonsNotReportedYet);

        // Map the failure to a mods RC.
        // Most common in practice will be that the operator forgot to attach
        // the video bridge cable.
        if(sliStatus & LW0000_CTRL_SLI_STATUS_NO_VIDLINK)
              return RC::SLI_NO_VIDLINK;

        if(sliStatus &
                (LW0000_CTRL_SLI_STATUS_GPU_MISMATCH
                 | LW0000_CTRL_SLI_STATUS_PCI_ID_MISMATCH
                 | LW0000_CTRL_SLI_STATUS_QUADRO_MISMATCH
                 | LW0000_CTRL_SLI_STATUS_VBIOS_MISMATCH
                 | LW0000_CTRL_SLI_STATUS_FB_MISMATCH))
                return RC::SLI_GPU_MISMATCH;

         return RC::SLI_ERROR;
    }
    return rc;
}

//! \brief Attempt to link up GPUs by GPU instance.
//!
RC GpuDevMgr::LinkGpus
(
    UINT32 NumGpus, //!< Number of UINT32s in input and output arrays.
    UINT32 *pInsts, //!< Array of Gpu instances that we're requesting
                    //!< to link, which may be in the wrong order.
    UINT32 DispInst //!< Instance of the requested display Gpu.
)
{
    RC rc = OK;

    UINT32 gpu;
    UINT32 gpuIds[LW0000_CTRL_GPU_MAX_ATTACHED_GPUS];
    UINT32 validIds[LW0000_CTRL_GPU_MAX_ATTACHED_GPUS];
    UINT32 dispId;    // The Id of the display gpu
    UINT32 dispIndex; // The index into the validIds for the display gpu
    // if we can ever have more than 64 GPUs, this will break
    // also, we should probably consider laying off the SLI crack-pipe
    MASSERT(LW0000_CTRL_GPU_MAX_ATTACHED_GPUS <= 64);

    CHECK_RC(BuildGpuIdList(NumGpus, pInsts, DispInst, gpuIds, &dispId));

    CHECK_RC(GetValidConfiguration( NumGpus, gpuIds, dispId, validIds, &dispIndex));

    CHECK_RC(ValidateConfiguration( NumGpus, validIds));

    // Remove all peer mappings from the subdevices prior to shutting down the
    // gpu devices
    for (gpu = 0; gpu < NumGpus; gpu++)
    {
        MASSERT(m_GpuSubdevices[pInsts[gpu]]);
        CHECK_RC(RemoveAllPeerMappings(m_GpuSubdevices[pInsts[gpu]]));
    }

    ModsShutdownGL();

    // Shutdown the devices associated with each gpuInstance
    for (gpu = 0; gpu < NumGpus; gpu++)
    {
        UINT32 oldDevInst = m_GpuDevicesByGpuInst[pInsts[gpu]];

        MASSERT(m_GpuDevices[oldDevInst]);

        Printf(Tee::PriLow, "Shutting down old device %d (GPU inst %d)...\n",
               oldDevInst, pInsts[gpu]);
        // free in the RM the old devices/subdevices
        m_GpuDevices[oldDevInst]->Shutdown();
    }

    UINT32 linkedDeviceInst;
    RC linkRc;

    // Cleanup
    bool printError = true;
    DEFER
    {
        if (printError)
        {
            PrintBadRC();
        }
    };

    Printf(Tee::PriLow, "Linking %d GPUs in lwrm.cpp...\n",NumGpus);
    // link the GPUs into a new device
    if ((linkRc = LwRmPtr()->LinkGpus(NumGpus, validIds, dispIndex,
                                      &linkedDeviceInst)) != OK)
    {
        Printf(Tee::PriHigh, "Linking failed!  Restoring old state:\n");
        for (gpu = 0; gpu < NumGpus; gpu++)
        {
            UINT32 oldDevInst = m_GpuDevicesByGpuInst[pInsts[gpu]];
            // reinitialize old devices
            Printf(Tee::PriHigh, "Restoring device %d (GpuInst %d)\n",
                   oldDevInst, pInsts[gpu]);
            CHECK_RC(m_GpuDevices[oldDevInst]->Initialize(m_NumUserdAllocs, m_UserdLocation));
        }
        return linkRc;
    }

    Printf(Tee::PriLow, "LinkedDeviceInst:%d \nDispIndex:%d\n",
           linkedDeviceInst,dispIndex);
    Printf(Tee::PriLow, "Linking succeeded.  Freeing old devices:\n");
    for (gpu = 0; gpu < NumGpus; gpu++)
    {
        UINT32 oldDevInst = m_GpuDevicesByGpuInst[pInsts[gpu]];

        // free old devices
        Printf(Tee::PriLow, "Freeing old device %d (GPU inst %d)...\n",
               oldDevInst, pInsts[gpu]);

        delete m_GpuDevices[oldDevInst];
        m_GpuDevices[oldDevInst] = 0;
    }

    // any failure at this point should be considered fatal

    Printf(Tee::PriLow, "Allocating new GpuDevice...\n");
    // allocate the new device in code
    m_GpuDevices[linkedDeviceInst] = new GpuDevice();

    // add the GPUs as subdevices
    for (gpu = 0; gpu < NumGpus; gpu++)
    {
        UINT32 gpuInst;
        UINT32 deviceInst = 0, subdeviceInst = 0;
        // Ask the RM for the new GpuInfo
        CHECK_RC(GetGpuInfo(validIds[gpu], &gpuInst,
                                    &deviceInst, &subdeviceInst));
        MASSERT(deviceInst == linkedDeviceInst);
        CHECK_RC(m_GpuDevices[deviceInst]->
                         AddSubdevice(GetSubdeviceByGpuInst(gpuInst),
                                      subdeviceInst));
        m_GpuDevicesByGpuInst[gpuInst] = deviceInst;
    }

    // initialize the new device, allocating it and the subdevices
    // in the RM
    Printf(Tee::PriLow, "Reinitializing new device...\n");
    CHECK_RC(m_GpuDevices[linkedDeviceInst]->
                     Initialize(m_NumUserdAllocs, m_UserdLocation, linkedDeviceInst));

    Printf(Tee::PriNormal, "Successfully linked GPUs as new device %d\n",
           linkedDeviceInst);

    if (rc == OK)
        printError = false;
    return rc;
}

//! \brief Unlink all device
//!
//! On teardown, the RM requires that we be in an unlinked state. (DOS)
void GpuDevMgr::UnlinkAll()
{
    // We've already tried, and run the risk of dereferencing the destroyed
    // JS engine under OSes like WinXP.
    if (m_Exiting)
        return;

    for (UINT32 dev = 0; dev < LW0080_MAX_DEVICES; dev ++)
    {
        if (m_GpuDevices[dev] &&
            m_GpuDevices[dev]->GetNumSubdevices() > 1)
        {
            Printf(Tee::PriNormal, "Forcing unlinking of GPU device %d...\n",
                   dev);
            UnlinkDevice(dev);
        }
    }
}

//! This function gets called on exit, and it forces an unlink of
//! all devices.
void GpuDevMgr::OnExit()
{
    for (auto* pSubDevice : m_GpuSubdevices)
    {
        if (pSubDevice)
        {
            (void) RemoveAllPeerMappings(pSubDevice);
        }
    }
    if (Platform::HasClientSideResman())
    {
        UnlinkAll();
    }
    m_Exiting = true;
}

//! \brief List the valid SLI configuration returned from RM
void GpuDevMgr::ListSLIConfigurations
(
    Tee::Priority Pri,      //!< Priority to use for output
    const vector<LW0000_CTRL_GPU_SLI_CONFIG> &SliConfigList,
    const vector<UINT32> &SliIlwalidReasonList
)
{
    Printf(Pri,"***** %s SLI Configurations *****\n",
           (SliIlwalidReasonList.size() == 0) ? "Valid" : "Invalid");
    Printf(Pri,"\tNumSLIConfigs:%d\n", (UINT32)SliConfigList.size());
    for (UINT32 i=0; i<SliConfigList.size(); i++)
    {
        Printf(Pri,"SLI Configuration %d:\n",i);
        Printf(Pri,"\tNumGpus:%x\n", (UINT32)SliConfigList[i].gpuCount);
        Printf(Pri,"\tNoDisplayGpuMask:0x%08x\n",
                (UINT32)SliConfigList[i].noDisplayGpuMask);
        Printf(Pri,"\tGpuIDs:");
        for (UINT32 j=0; j<SliConfigList[i].gpuCount; j++) {
            Printf(Pri,"0x%x ",(UINT32)SliConfigList[i].gpuIds[j]);
        }
        Printf(Pri,"\n\tDisplayGpuIndex:%x\n\tSliInfo:0x%08x\n",
               (UINT32)SliConfigList[i].displayGpuIndex,
               (UINT32)SliConfigList[i].sliInfo);
        Printf(Pri,"\t\t%s\n",
               DRF_VAL(0000_CTRL,_GPU_SLI_INFO,
                       _ACTIVE,SliConfigList[i].sliInfo) ?
                        "SLI_INFO_ACTIVE_TRUE" : "SLI_INFO_ACTIVE_FALSE");
        Printf(Pri,"\t\t%s\n",
               DRF_VAL(0000_CTRL,_GPU_SLI_INFO,
                       _VIDLINK,SliConfigList[i].sliInfo) ?
                        "VIDEO_LINK_PRESENT" : "VIDEO_LINK_NOT_PRESENT");
        Printf(Pri,"\t\t%s\n",
               DRF_VAL(0000_CTRL,_GPU_SLI_INFO,
                       _ENABLE_SLI_BY_DEFAULT,SliConfigList[i].sliInfo) ?
                       "ENABLE_SLI_BY_DEFAULT_TRUE" : "ENABLE_SLI_BY_DEFAULT_FALSE");
        Printf(Pri,"\t\t%s\n",
               DRF_VAL(0000_CTRL,_GPU_SLI_INFO,
                       _MULTI_GPU,SliConfigList[i].sliInfo) ?
                       "MULTI_GPU_TRUE" : "MULTI_GPU_FALSE");
        if (SliIlwalidReasonList.size() > 0)
        {
            Printf(Pri,"\tSliStatus:0x%08x\n", SliIlwalidReasonList[i]);
        }
    }
    return;
}

//! \brief Get GpuSubDevice by its GpuId, return NULL if not found
GpuSubdevice *GpuDevMgr::GetSubdeviceByGpuId(UINT32 GpuId)
{
    for (UINT32 i = 0; i < m_GpuSubdevices.size(); i++)
    {
        if ((NULL != m_GpuSubdevices[i]) &&
            (GpuId == m_GpuSubdevices[i]->GetGpuId()))
        {
            return m_GpuSubdevices[i];
        }
    }
    return NULL;
}

//------------------------------------------------------------------------------
//! \brief Add a peer mapping between two subdevices.
//!
//! \param pLwRm : RM client to add a mapping on
//! \param pSubdev1 : First subdevice to add a mapping on
//! \param pSubdev2 : Second subdevice to add a mapping on
//!
//! \return OK if adding a mapping succeeds, not OK otherwise.
RC GpuDevMgr::AddPeerMapping(LwRm* pLwRm, GpuSubdevice *pSubdev1, GpuSubdevice *pSubdev2)
{
    return AddPeerMapping(pLwRm, pSubdev1, pSubdev2, USE_DEFAULT_RM_MAPPING, PeerAccessType::GPA);
}

//--------------------------------------------------------------------------------------------------
//! Provide a Peer to Peer mapping between two Gpu subdevices.
//! If pSubdev1 = pSubdev2, then a loopback mapping will be created.
//! If peerId = USE_DEFAULT_RM_MAPPING then RM will determine what peerId masks to use, otherwise
//! peerIdBit must be <= 7.
RC GpuDevMgr::AddPeerMapping
(
    LwRm* pLwRm,
    GpuSubdevice *pSubdev1,
    GpuSubdevice *pSubdev2,
    UINT32 peerIdBit
)
{
    return AddPeerMapping(pLwRm, pSubdev1, pSubdev2, peerIdBit, PeerAccessType::GPA);
}

//--------------------------------------------------------------------------------------------------
//! Provide a Peer to Peer mapping between two Gpu subdevices.
//! If peerAccessType = PeerAccessType::SPA, test uses ats mapping to access memory in peer GPU.
RC GpuDevMgr::AddPeerMapping
(
    LwRm* pLwRm,
    GpuSubdevice *pSubdev1,
    GpuSubdevice *pSubdev2,
    UINT32 peerIdBit,
    PeerAccessType peerAccessType
)
{
    MASSERT(m_Initialized);
    MASSERT(pLwRm);
    MASSERT(pSubdev1);
    MASSERT(pSubdev2);

    if ((pSubdev1->GetParentDevice() == pSubdev2->GetParentDevice()) &&
        pSubdev1->GetParentDevice()->GetNumSubdevices() > 1)
    {
        Printf(Tee::PriHigh,
               "Unable to add mapping between subdevices %d:%d and %d:%d!\n",
               pSubdev1->GetParentDevice()->GetDeviceInst(),
               pSubdev1->GetSubdeviceInst(),
               pSubdev2->GetParentDevice()->GetDeviceInst(),
               pSubdev2->GetSubdeviceInst());
        Printf(Tee::PriHigh, "They are already in an SLI configuration!\n");
        return RC::ILWALID_DEVICE_ID;
    }

    if ((peerIdBit != USE_DEFAULT_RM_MAPPING) && (peerIdBit >= 8))
    {
        Printf(Tee::PriHigh,
            "peerId of %d is too large, only values -1..7 are valid\n",
            peerIdBit);
        return RC::BAD_PARAMETER;
    }

    PeerMappingIndex mapIndex(pLwRm, pSubdev1, pSubdev2, peerIdBit, peerAccessType);
    Tasker::MutexHolder lock(m_PeerMapMutex);
    if (m_SubdevicePeerMappings.count(mapIndex) > 0)
        return OK;

    if (peerIdBit != USE_DEFAULT_RM_MAPPING)
    {
        PeerMappingIndex mapGenericIndex(pLwRm, pSubdev1, pSubdev2, USE_DEFAULT_RM_MAPPING, peerAccessType);
        if ((m_SubdevicePeerMappings.count(mapGenericIndex) > 0) &&
            (m_SubdevicePeerMappings[mapGenericIndex].m_PeerId1 == peerIdBit) &&
            (m_SubdevicePeerMappings[mapGenericIndex].m_PeerId2 == peerIdBit))
        {
            return RC::OK;
        }
    }

    RC      rc;
    LwRm::Handle hP2P;
    LW503B_ALLOC_PARAMETERS p2pAllocParams = {0};
    p2pAllocParams.hSubDevice = pLwRm->GetSubdeviceHandle(pSubdev1);
    p2pAllocParams.hPeerSubDevice = pLwRm->GetSubdeviceHandle(pSubdev2);
    if (peerIdBit != USE_DEFAULT_RM_MAPPING)
    {
        p2pAllocParams.subDevicePeerIdMask = 1 << peerIdBit;
        p2pAllocParams.peerSubDevicePeerIdMask = 1 << peerIdBit;
    }
    switch (peerAccessType)
    {
        case SPA:
            p2pAllocParams.flags = LW503B_FLAGS_P2P_TYPE_SPA;
            break;
        case GPA:
            p2pAllocParams.flags = LW503B_FLAGS_P2P_TYPE_GPA;
            break;
        default:
            MASSERT(!"Invalid peer access type");
    }
    rc = pLwRm->Alloc(pLwRm->GetClientHandle(),
                      &hP2P,
                      LW50_P2P,
                      &p2pAllocParams);

    if (OK == rc)
    {
        LW0000_CTRL_SYSTEM_GET_P2P_CAPS_V2_PARAMS  p2pCapsParams = { };
        p2pCapsParams.gpuIds[0] = mapIndex.m_pSubdev1->GetGpuId();
        p2pCapsParams.gpuIds[1] = mapIndex.m_pSubdev2->GetGpuId();
        p2pCapsParams.gpuCount = 2;
        rc = pLwRm->Control(pLwRm->GetClientHandle(),
                            LW0000_CTRL_CMD_SYSTEM_GET_P2P_CAPS_V2,
                            &p2pCapsParams,
                            sizeof(p2pCapsParams));

        PeerMapping mapping = { hP2P, USE_DEFAULT_RM_MAPPING, USE_DEFAULT_RM_MAPPING };
        if (rc == RC::OK)
        {
            // busPeerIds is a peer ID matrix. It is a one-dimentional array.
            // busPeerIds[X * gpuCount + Y] maps from index X to index Y in
            // the gpuIds[] table - note that it is indexes and not actual GpuIds
            mapping.m_PeerId1 = p2pCapsParams.busPeerIds[1];
            mapping.m_PeerId2 = p2pCapsParams.busPeerIds[2];
        }
        // Links are bi-directional
        m_SubdevicePeerMappings[mapIndex] = mapping;
    }

    return rc;
}

RC GpuDevMgr::RemovePeerMapping(const PeerMappingIndex& peerMappingIndex)
{
    Tasker::MutexHolder lock(m_PeerMapMutex);

    // Cannot have peer mappings if not initialized
    if (!m_Initialized && !m_SubdevicePeerMappings.empty())
    {
        Printf(Tee::PriError,
            "RemovePeerMappings() called before initialization of GpuDevMgr\n");
        MASSERT(!"RemovePeerMappings() called before initialization of GpuDevMgr."
                " Please review code logic");
        return RC::SOFTWARE_ERROR;
    }

    const auto& it = m_SubdevicePeerMappings.find(peerMappingIndex);
    if (it == m_SubdevicePeerMappings.end())
    {
        Printf(Tee::PriError, "On RM client 0x%x, Gpu %d:%d and %d:%d using "
                            "loopbackPeerId 0x%x are not %s peer mapped\n",
            peerMappingIndex.m_pLwRm->GetClientHandle(),
            peerMappingIndex.m_pSubdev1->GetParentDevice()->GetDeviceInst(),
            peerMappingIndex.m_pSubdev1->GetSubdeviceInst(),
            peerMappingIndex.m_pSubdev2->GetParentDevice()->GetDeviceInst(),
            peerMappingIndex.m_pSubdev2->GetSubdeviceInst(),
            peerMappingIndex.m_PeerId,
            (peerMappingIndex.m_PeerAccessType == SPA) ? "SPA" : "GPA");
        MASSERT(!"Could not remove unexistent peer mapping. Please review code logic");
        return RC::SOFTWARE_ERROR;
    }

    auto* rmClient = peerMappingIndex.m_pLwRm;
    const auto& rmHandle = it->second.m_hP2P;

    // Links are bi-directional, and point to the same handle
    rmClient->Free(rmHandle);

    m_SubdevicePeerMappings.erase(it);

    return RC::OK;
}

RC GpuDevMgr::RemovePeerMapping(PeerMappings::const_iterator& peerMappingIt)
{
    Tasker::MutexHolder lock(m_PeerMapMutex);

    // Cannot have peer mappings if not initialized
    if (!m_Initialized)
    {
        Printf(Tee::PriError,
            "RemovePeerMappings() called before initialization of GpuDevMgr\n");
        MASSERT(!"RemovePeerMappings() called before initialization of GpuDevMgr."
                " Please review code logic");
        return RC::SOFTWARE_ERROR;
    }

    auto* rmClient = peerMappingIt->first.m_pLwRm;
    const auto& rmHandle = peerMappingIt->second.m_hP2P;

    // Links are bi-directional, and point to the same handle
    rmClient->Free(rmHandle);

    // Remove from the map and update with next valid iterator
    peerMappingIt = m_SubdevicePeerMappings.erase(peerMappingIt);

    return RC::OK;
}

RC GpuDevMgr::RemoveAllPeerMappings(GpuSubdevice *pSubdev)
{
    MASSERT(pSubdev);
    RC rc;

    Tasker::MutexHolder lock(m_PeerMapMutex);

    // Cannot have peer mappings if not initialized
    if (!m_Initialized && !m_SubdevicePeerMappings.empty())
    {
        Printf(Tee::PriError,
            "RemoveAllPeerMappings() called before initialization of GpuDevMgr\n");
        MASSERT(!"RemovePeerMappings() called before initialization of GpuDevMgr."
                " Please review code logic");
        return RC::SOFTWARE_ERROR;
    }

    for (auto it = m_SubdevicePeerMappings.cbegin();
         it != m_SubdevicePeerMappings.cend();
         /*no increment*/)
    {
        const PeerMappingIndex& peerMappingIndex = it->first;
        if (peerMappingIndex.Contains(pSubdev))
        {
            CHECK_RC(RemovePeerMapping(it));
        }
        else
        {
            ++it;
        }
    }
    return rc;
}

//! \brief Get any allocated PeerId on pSubdev1 used to access pSubdev2
RC GpuDevMgr::GetPeerId
(
    LwRm* pLwRm,
    GpuSubdevice *pSubdev1,
    GpuSubdevice *pSubdev2,
    PeerAccessType peerAccessType,
    UINT32 *pPeerId
)
{
    MASSERT(pPeerId);
    *pPeerId = USE_DEFAULT_RM_MAPPING;

    Tasker::MutexHolder lock(m_PeerMapMutex);

    // Cannot have peer mappings if not initialized
    if (!m_Initialized && !m_SubdevicePeerMappings.empty())
    {
        Printf(Tee::PriError,
            "GetPeerId() called before initialization of GpuDevMgr\n");
        MASSERT(!"GetPeerId() called before initialization of GpuDevMgr."
                " Please review code logic");
        return RC::SOFTWARE_ERROR;
    }

    auto mapping = find_if(m_SubdevicePeerMappings.begin(),
                           m_SubdevicePeerMappings.end(),
                           [&](const pair<PeerMappingIndex, PeerMapping>& mapping) -> bool
    {
        return ((mapping.first.m_pLwRm == pLwRm) &&
                (((mapping.first.m_pSubdev1 == pSubdev1) && (mapping.first.m_pSubdev2 == pSubdev2)) ||
                 ((mapping.first.m_pSubdev1 == pSubdev2) && (mapping.first.m_pSubdev2 == pSubdev1))) &&
                (mapping.first.m_PeerAccessType == peerAccessType));

    });

    if (mapping == m_SubdevicePeerMappings.end())
    {
        Printf(Tee::PriError, "On RM client 0x%x, Gpu %d:%d and %d:%d using "
                            " are not %s peer mapped\n",
            pLwRm->GetClientHandle(),
            pSubdev1->GetParentDevice()->GetDeviceInst(),
            pSubdev1->GetSubdeviceInst(),
            pSubdev2->GetParentDevice()->GetDeviceInst(),
            pSubdev2->GetSubdeviceInst(),
            (peerAccessType == SPA) ? "SPA" : "GPA");
        MASSERT(!"Could find nonexistent peer mapping. Please review code logic");
        return RC::SOFTWARE_ERROR;
    }

    if (pSubdev1 == mapping->first.m_pSubdev1)
        *pPeerId = mapping->second.m_PeerId1;
    else
        *pPeerId = mapping->second.m_PeerId2;
    return RC::OK;
}

bool GpuDevMgr::PeerMappingIndex::operator<(const PeerMappingIndex& rhs) const
{
    if (m_pLwRm < rhs.m_pLwRm)
        return true;
    else if (m_pLwRm > rhs.m_pLwRm)
        return false;
    else if (m_pSubdev1 < rhs.m_pSubdev1)
        return true;
    else if (m_pSubdev1 > rhs.m_pSubdev1)
        return false;
    else if (m_pSubdev2 < rhs.m_pSubdev2)
        return true;
    else if (m_pSubdev2 > rhs.m_pSubdev2)
        return false;
    else if (m_PeerId < rhs.m_PeerId)
        return true;
    else if (m_PeerId > rhs.m_PeerId)
        return false;
    return m_PeerAccessType < rhs.m_PeerAccessType;
}

bool GpuDevMgr::PeerMappingIndex::Contains(const GpuSubdevice* const pSubdevice) const
{
    return (pSubdevice == m_pSubdev1) || (pSubdevice == m_pSubdev2);
}

UINT32 GpuDevMgr::GetUserdLocation() const
{
    return static_cast<UINT32>(m_UserdLocation);
}

RC GpuDevMgr::SetUserdLocation(UINT32 userdLocation)
{
    if (userdLocation > static_cast<UINT32>(Memory::Optimal))
    {
        Printf(Tee::PriError, "Invalid USERD location specified : %u\n", userdLocation);
        return RC::BAD_PARAMETER;
    }
    m_UserdLocation = static_cast<Memory::Location>(userdLocation);
    return OK;
}

void GpuDevMgr::ShutdownDrivers()
{
    ModsShutdownLWDA();
    ModsShutdownGL();
}
