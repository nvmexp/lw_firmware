/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Cross platform (XP) interface.

#include "core/include/xp.h"


#include "core/include/cmdline.h"
#include "core/include/cpu.h"
#include "core/include/display.h"
#include "core/include/evntthrd.h"
#include "core/include/filesink.h"
#include "core/include/globals.h"
#include "core/include/isrdata.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/memcheck.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "core/include/pool.h"
#include "core/include/script.h"
#include "core/include/tasker.h"
#include "core/include/tasker_p.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "ctrl/ctrl2080.h"
#include "device/interface/portpolicyctrl.h"
#include "rm.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include "xp_mods_kd.h"

#define WE_NEED_LINUX_XP
#include "xp_linux_internal.h"

#ifndef USE_PTHREADS
    #error "LinuxMfg MODS must be compiled with pthreads!"
#endif

#if defined(INCLUDE_GPU)
    #include "acpidsmguids.h"
    #include "acpigenfuncs.h"
    #include "core/include/mgrmgr.h"
    #include "device/interface/pcie.h"
    #include "gpu/include/gpumgr.h"
    #include "gpu/include/gpusbdev.h"
    #include "mxm_spec.h"
    #include "nbci.h"
    #include "lwop.h"
#endif

#if defined(TEGRA_MODS) || defined(INCLUDE_PEATRANS)
    #include "cheetah/dev/watchdog/timerctrl.h"
    #include "cheetah/dev/watchdog/timerctrlmgr.h"
    #include "cheetah/dev/watchdog/wdtctrl.h"
    #include "cheetah/dev/watchdog/wdtctrlmgr.h"
    #include "cheetah/include/tegirq.h"
    #include "cheetah/include/tegkrn.h"
#endif
#ifdef INCLUDE_PEATRANS
    #include "cheetah/peatrans/peatclk.h"
#endif

#include <algorithm>
#include <asm/errno.h>
#include <atomic>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <utility>

#if !defined(LWCPU_FAMILY_ARM)
#include <linux/hdreg.h> //ioctl HDIO_GET_IDENTITY
#endif

#if defined(INCLUDE_GPU) && defined(LWCPU_FAMILY_X86)
#include <linux/screen_info.h>
#endif


#define MIN(a,b) ((a) < (b) ? (a) : (b))

namespace Platform
{
    bool GetSkipDisableUserInterface();
    RC SetSkipDisableUserInterface(bool b);
}

namespace
{
    Xp::MODSKernelDriver s_KD;
}

// Linux pages mask for mmap function
#define LINUX_PAGE_MASK (GetPageSize() - 1)

// Interrupts support
namespace
{
    bool s_DisableWC = false;

    string ReadAndCloseFile(int fd)
    {
        string buf;
        ssize_t numRead = 0;
        do
        {
            const size_t resize = 1024;
            const size_t pos = buf.size();
            buf.resize(pos+resize);
            numRead = read(fd, &buf[pos], resize);
            buf.resize(pos+((numRead > 0) ? numRead : 0));
        }
        while (numRead > 0);
        close(fd);
        return buf;
    }

    RC CheckUnsupportedModule(const string& procModules, const char* module)
    {
        string search = module;
        search += ' ';
        for (size_t pos=0; ; )
        {
            pos = procModules.find(search, pos);
            if (pos == string::npos)
                break;
            if ((pos == 0) || (procModules[pos-1] == '\n'))
            {
                Printf(Tee::PriAlways, "Please unload the '%s' kernel module!\n", module);
                return RC::UNSUPPORTED_SYSTEM_CONFIG;
            }
        }
        return OK;
    }

    RC CheckUnsupportedPciDevDriver(const char* driver)
    {
        const int procPciDevices = open("/proc/bus/pci/devices", O_RDONLY);
        const string buf = ReadAndCloseFile(procPciDevices);
        if (buf.find(driver) != string::npos)
        {
            Printf(Tee::PriError, "Please unbind %s driver from Pci GPUs\n", driver);
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }
        return OK;
    }

#if defined(TEGRA_MODS)
    const UINT32 WATCHDOG_TIMEOUT_INDEX = 2;
    const UINT32 WATCHDOG_HUNG_TEST_INDEX = 1;
    const UINT32 TIMER_TIMEOUT_INDEX = 1;
    const UINT32 TIMER_HUNG_TEST_INDEX = 2;
    UINT32 s_TimerWriteAddress = 0;
#endif

    Xp::ClockManager* s_pPeaTransClkMgr = nullptr;

#if defined(INCLUDE_GPU) && defined(LWCPU_FAMILY_X86)

    constexpr INT32 s_ConsPri = Tee::PriLow;

    atomic<UINT32> s_ConsoleSuspendCount{0};

// See bug 1591992
// orig_video_is_vga is set to 1 during early Linux kernel initialization,
// then if an FB device was chosen, it will be set to another value,
// such as VIDEO_TYPE_EFI.  The value '1' is not defined in screen_info.h,
// so we define it here.
#define VIDEO_TYPE_RAW_VGA 1

    MODS_SCREEN_INFO_2 s_ScreenInfo     = { };
    PHYSADDR           s_ScreenInfoBase = 0;

    // Read information about the OS console from MODS kernel driver
    void GetScreenInfo()
    {
        if (s_KD.Ioctl(MODS_ESC_GET_SCREEN_INFO_2, &s_ScreenInfo))
        {
            // Fall back to 32bit version
            s_ScreenInfo.ext_lfb_base = 0;
            if (s_KD.Ioctl(MODS_ESC_GET_SCREEN_INFO, &s_ScreenInfo.info))
            {
                Printf(s_ConsPri, "Reading screen info not supported\n");
                memset(&s_ScreenInfo, 0, sizeof(s_ScreenInfo));
                return;
            }
        }

        s_ScreenInfoBase = s_ScreenInfo.info.lfb_base
                           | (static_cast<UINT64>(s_ScreenInfo.ext_lfb_base) << 32);

        Printf(s_ConsPri, "Linux console type=0x%x mode=0x%x base=0x%llx size=0x%x\n",
               s_ScreenInfo.info.orig_video_is_vga,
               s_ScreenInfo.info.orig_video_mode,
               s_ScreenInfoBase,
               s_ScreenInfo.info.lfb_size);

        // For VGA console, the base address is already 0.
        // For other non-EFI console types, which we don't support, just set 0.
        if (s_ScreenInfo.info.orig_video_is_vga != VIDEO_TYPE_EFI)
        {
            s_ScreenInfoBase = 0;
        }
    }

    PHYSADDR s_PrimaryBar0     = 0;
    bool     s_MaybePrimaryVga = false;

    bool IsBootDevice(GpuSubdevice* pSubdev)
    {
        // We remember primary GPU by its BAR0
        const PHYSADDR bar0 = pSubdev->GetPhysLwBase();

        // If we have detected primary before, just check if this is the same device.
        if (s_PrimaryBar0)
        {
            return bar0 == s_PrimaryBar0;
        }

        // If the GPU says it is primary, we will treat it as primary VGA device.
        // This typically happens early in MODS boot sequence when MODS initially
        // detects primary GPU by looking at IO space enable bit.  RM later
        // overwrites that flag based on scratch bits.  However, the scratch bits
        // are not reliable, because if the GPU was reset and re-POSTed by RM,
        // the scratch bits are erased.  On the other hand, this can result with
        // multiple GPUs in the system having IO space enabled.
        // The primary flag can also be set in CSM (compatibility mode), but in this
        // case we would expect to see EFI console in normal cirlwmstances.
        if (pSubdev->IsPrimary() && (s_ScreenInfo.info.orig_video_is_vga != VIDEO_TYPE_EFI))
        {
            s_PrimaryBar0     = bar0;
            s_MaybePrimaryVga = true;
            Printf(s_ConsPri, "Detected potential primary VGA device: %s\n",
                   pSubdev->GpuIdentStr().c_str());
            return true;
        }

        if ((s_ScreenInfo.info.orig_video_is_vga != VIDEO_TYPE_EFI) || !s_ScreenInfoBase)
        {
            // Fall back in case querying screen info did not work:
            // If GPU says it is a boot EFI GPU, trust it.
            if (pSubdev->IsUefiBoot())
            {
                s_PrimaryBar0 = bar0;
                Printf(s_ConsPri, "Detected boot EFI device: %s\n",
                       pSubdev->GpuIdentStr().c_str());
                return true;
            }

            return false;
        }

        // Detect boot GPU in EFI mode by checking if OS console landed in BAR1 or BAR2.
        const PHYSADDR bar1Start = pSubdev->GetPhysFbBase();
        const PHYSADDR bar1End   = bar1Start + pSubdev->FbApertureSize();
        if ((s_ScreenInfoBase >= bar1Start) && (s_ScreenInfoBase < bar1End))
        {
            s_PrimaryBar0 = bar0;
            Printf(s_ConsPri, "Detected EFI console in BAR1 of %s\n",
                   pSubdev->GpuIdentStr().c_str());
            return true;
        }

        const PHYSADDR bar2Start = pSubdev->GetPhysInstBase();
        const PHYSADDR bar2End   = bar2Start + pSubdev->InstApertureSize();
        if ((s_ScreenInfoBase >= bar2Start) && (s_ScreenInfoBase < bar2End))
        {
            s_PrimaryBar0 = bar0;
            Printf(s_ConsPri, "Detected EFI console in BAR2 of %s\n",
                   pSubdev->GpuIdentStr().c_str());
            return true;
        }

        return false;
    }

    GpuSubdevice* FindPrimarySubdev()
    {
        GpuSubdevice* pSubdev = nullptr;

        if (DevMgrMgr::d_GraphDevMgr)
        {
            for (GpuSubdevice* pNextSubdev : *DevMgrMgr::d_GraphDevMgr)
            {
                if (IsBootDevice(pNextSubdev))
                {
                    pSubdev = pNextSubdev;
                    break;
                }
            }
        }

        return pSubdev;
    }
#else
    constexpr void GetScreenInfo()
    {
    }
#endif
} // anonymous namespace

#if defined(TEGRA_MODS)
#include "cheetah/include/clocklog.h"

RC CheetAh::ClockGetSys(string &device, string &controller, LwU64 *pClockHandle)
{
    MODS_GET_CLOCK_HANDLE param = {0};
    memcpy(param.device_name, device.c_str(), min(device.size(), sizeof(param.device_name)));
    memcpy(param.controller_name, controller.c_str(), min(controller.size(), sizeof(param.controller_name)));
    if (s_KD.Ioctl(MODS_ESC_GET_CLOCK_HANDLE, &param))
    {
        return RC::SOFTWARE_ERROR;
    }
    *pClockHandle = param.clock_handle;

    ClockLog::LogAction(false, device + "_" + controller, __FUNCTION__, *pClockHandle);
    return OK;
}
#endif

P_(Global_Get_KernelModuleVersion);
static SProperty Global_KernelModuleVersion
(
    "KernelModuleVersion",
    0,
    Global_Get_KernelModuleVersion,
    0,
    0,
    "Version of the MODS kernel module."
);

P_(Global_Get_KernelModuleVersion)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    const UINT32 version = s_KD.GetVersion();
    const string versionString =
        Utility::StrPrintf("%x.%02x", (version>>8), (version&0xFF));
    if (OK != JavaScriptPtr()->ToJsval(versionString, pValue))
    {
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

namespace
{

#ifdef LWCPU_AARCH64
    struct VendorDevId
    {
        UINT16 vendor;
        UINT16 device;
    };

    bool DetectOneOfDevices(const char* name, const VendorDevId* pVendorDevId, size_t num)
    {
        for (; num; ++pVendorDevId, --num)
        {
            UINT32 domain = 1;
            UINT32 dev    = 1;
            UINT32 bus    = 1;
            UINT32 func   = 1;
            const RC rc = s_KD.FindPciDevice(pVendorDevId->device, pVendorDevId->vendor, 0,
                                             &domain, &dev, &bus, &func);
            if (rc == RC::OK)
            {
                Printf(Tee::PriLow, "Detected %s at %04x:%02x:%02x.%x\n",
                       name, domain, dev, bus, func);
                return true;
            }
        }

        return false;
    }

    void ApplyAmpereAltraWAR()
    {
        static const VendorDevId altra[] =
        {
            { Pci::Ampere, 0xE000 },
            { Pci::Ampere, 0xE00D },
            { Pci::Ampere, 0xE00E },
            { Pci::Ampere, 0xE010 },
            { Pci::Ampere, 0xE100 },
            { Pci::Ampere, 0xE110 }
        };

        if (DetectOneOfDevices("Ampere Altra", altra, NUMELEMS(altra)))
        {
            Printf(Tee::PriLow, "Disabling WC on Ampere Altra\n");
            s_DisableWC = true;
        }
    }
#endif
}


// -----------------------------------------------------------------------------
// Check if kernel module is loaded and initialize communication between
// user mode MODS and kernel side
RC Xp::LinuxInternal::OpenDriver(const vector<string> &args)
{
    // Check minimum supported kernel version
    utsname name;
    if (0 == uname(&name))
    {
        int major=0, minor=0, release=0;
        if (3 == sscanf(name.release, "%d.%d.%d-", &major, &minor, &release))
        {
            const int minRelease = 29;
            if ((major == 2) && (minor == 6) && (release < minRelease))
            {
                Printf(Tee::PriAlways, "This system has an unsupported kernel version.\n"
                        "Please upgrade your kernel to at least 2.6.%d.\n",
                        minRelease);
                return RC::UNSUPPORTED_SYSTEM_CONFIG;
            }
        }
    }

    RC rc;

    // We expect the LWPU kernel module to not be loaded
    const int modules = open("/proc/modules", O_RDONLY);
    if (modules >= 0)
    {
        // Read /proc/modules
        const string buf = ReadAndCloseFile(modules);

        // Look for unsupported kernel modules
        CHECK_RC(CheckUnsupportedModule(buf, "lwpu"));
        CHECK_RC(CheckUnsupportedModule(buf, "nouveau"));
        CHECK_RC(CheckUnsupportedPciDevDriver("lwgpu"));
    }


    if (CommandLine::AccessToken() != ~0U)
        s_KD.SetAccessToken(CommandLine::AccessToken());
    if (CommandLine::AccessTokenAcquire())
        s_KD.SetAccessTokenMode(MODSKernelDriver::AccessTokenMode::ACQUIRE);
    else if (CommandLine::AccessTokenRelease())
        s_KD.SetAccessTokenMode(MODSKernelDriver::AccessTokenMode::RELEASE);

    CHECK_RC(s_KD.Open(args[0].c_str()));

    if (CommandLine::AccessTokenAcquire())
        Printf(Tee::PriNormal, "Access Token : %u\n", s_KD.GetAccessToken());
    else if (CommandLine::AccessTokenRelease())
        Printf(Tee::PriNormal, "Access Token %u released\n", CommandLine::AccessToken());

    GetScreenInfo();

#ifdef LWCPU_AARCH64
    ApplyAmpereAltraWAR();
#endif

#ifdef INCLUDE_PEATRANS
    s_pPeaTransClkMgr = new PeatClockManager;
#endif
    return rc;
}

namespace
{
    bool PAIsBelowAddrBits(void* descriptor, size_t size, bool contiguous, UINT32 addressBits)
    {
        MASSERT(*reinterpret_cast<UINT64*>(descriptor));
        if (!contiguous)
        {
            const size_t pageSize = Xp::GetPageSize();

            for (size_t lwrOff = 0; lwrOff < size; lwrOff += pageSize)
            {
                const PHYSADDR physAddr = Xp::GetPhysicalAddress(descriptor, lwrOff);
                if ((physAddr + pageSize) > (1ULL << addressBits))
                {
                    return false;
                }
            }
        }
        else
        {
            const PHYSADDR physAddr = Xp::GetPhysicalAddress(descriptor, 0);
            if ((physAddr + size) > (1ULL << addressBits))
            {
                return false;
            }
        }
        return true;
    }

    vector<UINT64> s_ReservedMemory;

    // Reserves as many regions that don't fit in addressBits as possible.
    // Returns number of bytes reserved.
    UINT64 SweepUnderTheRug
    (
        UINT32 addressBits,
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 function
    )
    {
        // To exhaust all memory with PAs that don't fit in addressBits,
        // we employ a strategy similar to lwmemtest:
        // 1. We allocate all contiguous blocks with 2MB in size until we find
        //    a usable block with PA that fits in addressBits.
        // 2. We free that usable block, so MODS can use it.
        // 3. We repeat steps 1 and 2 for 1MB blocks, 512KB blocks and so
        //    on, until we reach a single page.
        //
        // This will ensure that MODS will be able to allocate memory that is
        // usable by the GPU, since GPU address space is limited and the GPU
        // may not be able to map all system memory locations.
        //
        // This could eventually reserve *all* memory that the NUMA node has.
        // This typically oclwrs on systems with multiple NUMA nodes, so the
        // risk that the kernel will kill MODS due to OOM should be low (untested).
        // The expectation is that if a NUMA node does not have *any* memory
        // usable for a GPU, the allocation will eventually fail and MODS will
        // fail with proper error.  Newer GPUs support more PA bits, so the
        // situation is less likely with them.
        UINT32 size  = 2_MB;
        UINT64 total = 0U;
        const UINT32 pageSize = Xp::GetPageSize();
        for ( ; size >= pageSize; size >>= 1)
        {
            UINT64 handle = 0U;

            if (s_KD.AllocPages(size, true, 64U, Memory::WB,
                                domain, bus, device, function,
                                &handle) != OK)
            {
                continue;
            }

            while (!PAIsBelowAddrBits(&handle, size, true, addressBits))
            {
                total += size;
                s_ReservedMemory.push_back(handle);
                handle = 0U;

                if (s_KD.AllocPages(size, true, 64U, Memory::WB,
                                    domain, bus, device, function,
                                    &handle) != OK)
                {
                    break;
                }
            }

            if (handle != 0U)
            {
                s_KD.FreePages(handle);
            }
        }

        return total;
    }
}

RC Xp::LinuxInternal::CloseModsDriver()
{
    if (s_pPeaTransClkMgr)
    {
        delete s_pPeaTransClkMgr;
        s_pPeaTransClkMgr = nullptr;
    }

    if (!s_ReservedMemory.empty())
    {
        Printf(Tee::PriDebug, "Freeing reserved memory\n");
        do
        {
            s_KD.FreePages(s_ReservedMemory.back());
            s_ReservedMemory.pop_back();
        } while (!s_ReservedMemory.empty());
    }

    Printf(Tee::PriDebug, "Closing MODS kernel driver\n");
    s_KD.Close();

    return OK;
}

//------------------------------------------------------------------------------
// Get the Linux kernel version a.b.c
// returns ((a) << 16 + (b) << 8 + (c))
// -----------------------------------------------------------------------------
RC Xp::GetOSVersion(UINT64* pVer)
{
    return s_KD.GetKernelVersion(pVer);
}

namespace
{
#if defined(PPC64LE)
    // POWER8/9 have full I/O coherence and don't support UC/WC for memory.
    bool SupportsUCMemory()
    {
        return false;
    }

    bool SupportsWCMemory()
    {
        return false;
    }
#elif defined(LWCPU_AARCH64)
    bool SupportsUCMemory()
    {
        return false;
    }

    bool SupportsWCMemory()
    {
        // Note:
        // * UC is not advised on aarch64 due to massive perf hit.  Instead, we should
        //   use WC.
        // * For now we assume that only CheetAh supports WC.  The Cavium ThunderX2
        //   implementation in bug 2682265 does not support set_memory_uc/wc functions
        //   in the kernel.  But this implementation has full I/O coherence and
        //   so UC/WC aren't really needed for memory mappings anyway.
        // * In the future we may need to add better platform detection here if
        //   we ever encounter platforms which don't have I/O coherence and
        //   support set_memory_uc/wc functions in the kernel.  Perhaps MODS kernel
        //   driver should have an ioctl for that!
        return Platform::IsTegra() && !s_DisableWC;
    }
#else // x86
    bool SupportsUCMemory()
    {
        return true;
    }

    bool SupportsWCMemory()
    {
        return !s_DisableWC;
    }
#endif
}

RC Xp::AllocPages
(
    size_t         numBytes,
    void **        pDescriptor,
    bool           contiguous,
    UINT32         addressBits,
    Memory::Attrib attrib,
    UINT32         gpuInst
)
{
    // By default, if gpuInst doesn't select any GPU, use special values
    // to tell the driver that we're not allocating the memory for any
    // GPU.  In this case the driver will allocate the memory from the
    // NUMA node on which the code is running.
    UINT32 domain   = MODSKernelDriver::s_NoPCIDev;
    UINT32 bus      = MODSKernelDriver::s_NoPCIDev;
    UINT32 device   = MODSKernelDriver::s_NoPCIDev;
    UINT32 function = MODSKernelDriver::s_NoPCIDev;
    if (gpuInst != ~0U)
    {
        (void)s_KD.GetGpuPciLocation(gpuInst, &domain, &bus, &device, &function);
    }
    return AllocPages(numBytes, pDescriptor, contiguous, addressBits, attrib,
                      domain, bus, device, function);
}

//------------------------------------------------------------------------------
//! \brief Alloc system memory.
//!
//! \param numBytes    : Number of bytes to allocate (actual allocation will be
//!                      a whole number of pages (numBytes will be rounded up).
//! \param pDescriptor : Returned descriptor for the allocated memory
//! \param contiguous  : If true, physical pages will be adjacent and in-order.
//! \param addressBits : All pages will be locked and will be below physical
//!                      address 2**addressBits.
//! \param attrib      : Memory attributes to use when allocating
//! \param gpuInst     : If gpuInst is valid, then on a NUMA system the system
//!                      memory will be on the node local to the GPU
//!
//! \return OK if memory was successfully allocated and mapped, not OK otherwise
RC Xp::AllocPages
(
    size_t         numBytes,
    void **        pDescriptor,
    bool           contiguous,
    UINT32         addressBits,
    Memory::Attrib attrib,
    UINT32         domain,
    UINT32         bus,
    UINT32         device,
    UINT32         function
)
{
    RC rc;

#if defined(LWCPU_AARCH64)
    MASSERT(attrib != Memory::UC);
#endif

    if (attrib == Memory::WC && SupportsUCMemory() && !SupportsWCMemory())
    {
        attrib = Memory::UC;
        Printf(Tee::PriLow, "WC mapping colwerted to UC.\n");
    }

    if (attrib == Memory::UC && !SupportsUCMemory() && SupportsWCMemory())
    {
        attrib = Memory::WC;
        Printf(Tee::PriLow, "UC mapping colwerted to WC.\n");
    }

    if ((attrib == Memory::WC || attrib == Memory::UC) &&
        !SupportsUCMemory() && !SupportsWCMemory())
    {
        Printf(Tee::PriLow, "%s mapping colwerted to WB.\n",
               attrib == Memory::UC ? "UC" : "WC");
        attrib = Memory::WB;
    }

    UINT64 memoryHandle = 0;
    rc = s_KD.AllocPages(numBytes, contiguous, addressBits, attrib,
                         domain, bus, device, function,
                         &memoryHandle);

    // In case of OOM, drop caches, but attempt it only once
    static bool doneDropCaches = false;
    if ((rc == RC::CANNOT_ALLOCATE_MEMORY) && !doneDropCaches)
    {
        doneDropCaches = true;
        if (SystemDropCaches() == RC::OK)
        {
            // If dropping caches was successful, try to allocate memory again
            rc.Clear();
            rc = s_KD.AllocPages(numBytes, contiguous, addressBits, attrib,
                                 domain, bus, device, function,
                                 &memoryHandle);
        }
    }
    CHECK_RC(rc);

    UINT64* p = new UINT64;
    if (!p)
        return RC::CANNOT_ALLOCATE_MEMORY;
    *p = memoryHandle;

    // When addressBits is between 32 and 64 ensure that the actual
    // physical address meets the requirement.
    //
    // If not, save the current memory allocation, then continue allocating
    // memory until we get something that fits the criteria.  The problem is
    // that on NUMA systems with a lot of RAM, we hit the following corner
    // case:
    // * Maxwell GPUs require PA to fit in 40 bits (requirement from RM).
    // * Some NUMA nodes have memory that requires more address bits.
    // * When we get an allocation from the kernel that has PA exceeding 40 bits,
    //   we cannot simply fall back to 32 bits anymore, because such NUMA nodes
    //   often don't have RAM below 4GB in PA space.
    // * The kernel only allows us to limit allocations to 32 bits or not at all.
    if ((addressBits > 32) && (addressBits != 64))
    {
        UINT64 totalReserved = 0U;
        UINT64 bytesReserved = 1U; // start with non-zero to avoid reserving this allocation
        while (!PAIsBelowAddrBits(p, numBytes, contiguous, addressBits))
        {
            if (bytesReserved == 0U)
            {
                // For possible mixed allocations allocated even after
                // SweepUnderTheRug() did not reserve anyting, just reserve
                // them.  Otherwise we will never be able to get a valid
                // allocation.
                s_ReservedMemory.push_back(memoryHandle);
                totalReserved += numBytes;
            }
            else
            {
                s_KD.FreePages(memoryHandle);
            }

            bytesReserved = SweepUnderTheRug(addressBits, domain, bus, device, function);
            totalReserved += bytesReserved;

            rc = s_KD.AllocPages(numBytes, contiguous, addressBits, attrib,
                                 domain, bus, device, function,
                                 &memoryHandle);
            if (rc != OK)
            {
                Printf(Tee::PriError, "Failed to allocate memory that has PA "
                                      "which fits in %u address bits for device "
                                      "%04x:%02x:%02x.%x\n",
                                      addressBits, domain, bus, device, function);
                delete p;
                p = nullptr;
                break;
            }

            *p = memoryHandle;
        }

        if (totalReserved > 0U)
        {
            Printf(Tee::PriLow, "Reserved %llu bytes of memory for device %04x:%02x:%02x.%x "
                                "with PAs exceeding %u address bits\n",
                                totalReserved, domain, bus, device, function, addressBits);
        }
    }

    *pDescriptor = p;

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Alloc contiguous system memory with pages aligned as requested
//!
//! \param NumBytes    : Number of bytes to allocate (actual allocation will be
//!                      a whole number of pages (NumBytes will be rounded up).
//! \param pDescriptor : Returned descriptor for the allocated memory
//! \param AddressBits : All pages will be locked and will be below physical
//!                      address 2**AddressBits.
//! \param Attrib      : Memory attributes to use when allocating
//! \param GpuInst     : If GpuInst is valid, then on a NUMA system the system
//!                      memory will be on the node local to the GPU
//!
//! \return OK if memory was successfully allocated and mapped, not OK otherwise
RC Xp::AllocPagesAligned
(
    size_t         NumBytes,
    void **        pDescriptor,
    size_t         PhysicalAlignment,
    UINT32         AddressBits,
    Memory::Attrib Attrib,
    UINT32         GpuInst
)
{
    return AllocPages(NumBytes, pDescriptor, true, AddressBits, Attrib, GpuInst);
}

//------------------------------------------------------------------------------
//!< Free pages allocated by AllocPages or AllocPagesAligned.
void Xp::FreePages(void* descriptor)
{
    UINT64* const pHandle = reinterpret_cast<UINT64*>(descriptor);
    const UINT64 handle = *pHandle;
    delete pHandle;
    if (handle)
        s_KD.FreePages(handle);
}

RC Xp::MergePages(void** pOutDescriptor, void** pInDescBegin, void** pInDescEnd)
{
    MASSERT(pOutDescriptor && pInDescBegin && pInDescEnd);
    MASSERT(pInDescBegin < pInDescEnd);
    if (pInDescBegin >= pInDescEnd)
    {
        return RC::SOFTWARE_ERROR;
    }

    const UINT32 numInDesc = pInDescEnd - pInDescBegin;
    if (numInDesc == 1)
    {
        *pOutDescriptor = *pInDescBegin;
        return RC::OK;
    }

    UINT64 memoryHandle = *reinterpret_cast<UINT64*>(pInDescBegin[0]);
    *reinterpret_cast<UINT64*>(pInDescBegin[0]) = 0;

    constexpr UINT32 maxDescs = 64; // Matches what the kernel driver supports
    UINT64 inDesc[maxDescs];

    for (void** pDescIt = pInDescBegin + 1; pDescIt < pInDescEnd; )
    {
        const UINT32 numLeft = pInDescEnd - pDescIt;
        const UINT32 numToMerge = min(numLeft, maxDescs - 1);

        inDesc[0] = memoryHandle;
        for (UINT32 i = 0; i < numToMerge; i++)
        {
            inDesc[i + 1] = *reinterpret_cast<UINT64*>(pDescIt[i]);
        }

        const RC rc = s_KD.MergePages(&memoryHandle, inDesc, numToMerge + 1);

        if (rc != RC::OK)
        {
            *reinterpret_cast<UINT64*>(*(--pDescIt)) = memoryHandle;
            *pOutDescriptor = nullptr;
            return rc;
        }

        for (UINT32 i = 0; i < numToMerge; i++)
        {
            *reinterpret_cast<UINT64*>(*(pDescIt++)) = 0;
        }
    }

    UINT64* const p = new UINT64;
    if (!p)
    {
        *reinterpret_cast<UINT64*>(pInDescBegin[0]) = memoryHandle;
        *pOutDescriptor = nullptr;
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    *p = memoryHandle;
    *pOutDescriptor = p;

    return RC::OK;
}

//------------------------------------------------------------------------------
RC Xp::MapPages
(
    void**          pRetVirtAddr,
    void*           descriptor,
    size_t          offset,
    size_t          size,
    Memory::Protect protect
)
{
    MASSERT(*reinterpret_cast<UINT64*>(descriptor));
    const LwU64 physAddr = Xp::GetPhysicalAddress(descriptor, offset);
    if (physAddr == 0)
    {
        return RC::SOFTWARE_ERROR;
    }

    *pRetVirtAddr = s_KD.Mmap(physAddr, size, protect, Memory::DE);
    if (*pRetVirtAddr == nullptr)
    {
        Printf(Tee::PriHigh, "Error: Can't map sysmem at phys addr 0x%llx\n", physAddr);
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

//------------------------------------------------------------------------------
void Xp::UnMapPages(void* virtAddr)
{
    // Allow unmaps of NULL
    if (!virtAddr)
    {
        return;
    }

    s_KD.Munmap(virtAddr);
}

//------------------------------------------------------------------------------
PHYSADDR Xp::GetPhysicalAddress(void* descriptor, size_t offset)
{
    MASSERT(*reinterpret_cast<UINT64*>(descriptor));
    return s_KD.GetPhysicalAddress(*reinterpret_cast<UINT64*>(descriptor), offset);
}

//------------------------------------------------------------------------------
bool Xp::CanEnableIova
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func
)
{
    return s_KD.IsOpen();
}

//------------------------------------------------------------------------------
PHYSADDR Xp::GetMappedPhysicalAddress
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    void  *descriptor,
    size_t offset
)
{
    MASSERT(*reinterpret_cast<UINT64*>(descriptor));
    return s_KD.GetMappedPhysicalAddress(domain, bus, device, func,
                                         *reinterpret_cast<UINT64*>(descriptor),
                                         offset);
}

//------------------------------------------------------------------------------
RC Xp::DmaMapMemory
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    void  *descriptor
)
{
    if (!s_KD.IsOpen())
    {
        MASSERT(0);
        return RC::UNSUPPORTED_FUNCTION;
    }
    MASSERT(*reinterpret_cast<UINT64*>(descriptor));
    return s_KD.DmaMapMemory(domain, bus, device, func,
                             *reinterpret_cast<UINT64*>(descriptor));
}

//------------------------------------------------------------------------------
RC Xp::DmaUnmapMemory
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    void  *descriptor
)
{
    if (!s_KD.IsOpen())
    {
        MASSERT(0);
        return RC::UNSUPPORTED_FUNCTION;
    }
    MASSERT(*reinterpret_cast<UINT64*>(descriptor));
    return s_KD.DmaUnmapMemory(domain, bus, device, func,
                               *reinterpret_cast<UINT64*>(descriptor));
}

//------------------------------------------------------------------------------
RC Xp::SetDmaMask(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, UINT32 numBits)
{
    return s_KD.SetDmaMask(domain, bus, device, func, numBits);
}

//------------------------------------------------------------------------------
// Colwert physical address to virtual address.
//
void* Xp::PhysicalToVirtual(PHYSADDR physAddr, Tee::Priority pri)
{
#ifdef INCLUDE_PEATRANS
    return reinterpret_cast<void *>(physAddr);
#endif
    return s_KD.PhysicalToVirtual(physAddr, pri);
}

//------------------------------------------------------------------------------
// Colwert virtual address to physical address.
//
PHYSADDR Xp::VirtualToPhysical(volatile void* virtAddr, Tee::Priority pri)
{
#ifdef INCLUDE_PEATRANS
    // we mask the higher 32 bits since in some cases we see all ones
    // in those bits after typecasting is done.
    PHYSADDR mask = 0x00000000FFFFFFFFULL;
    PHYSADDR tmp = (PHYSADDR)(virtAddr) & mask;
    Printf(Tee::PriLow,"Colwerted to 0x%llx\n", tmp);
    return tmp;
#endif
    return s_KD.VirtualToPhysical(virtAddr, pri);
}

RC Xp::IommuDmaMapMemory(void *pDescriptor, string DevName, PHYSADDR *pIova)
{
    if (!s_KD.IsOpen())
    {
        MASSERT(0);
        return RC::UNSUPPORTED_FUNCTION;
    }
    return s_KD.IommuDmaMapMemory(*reinterpret_cast<UINT64 *>(pDescriptor), DevName, pIova);
}

RC Xp::IommuDmaUnmapMemory(void *pDescriptor, string DevName)
{
    if (!s_KD.IsOpen())
    {
        MASSERT(0);
        return RC::UNSUPPORTED_FUNCTION;
    }
    return s_KD.IommuDmaUnmapMemory(*reinterpret_cast<UINT64 *>(pDescriptor), DevName);
}

//!< Map device memory into the page table, get virtual address to use.
//!< Multiple mappings are legal, and give different virtual addresses
//!< on paging systems.
RC Xp::MapDeviceMemory
(
    void**          pRetVirtAddr,
    PHYSADDR        physAddr,
    size_t          size,
    Memory::Attrib  attrib,
    Memory::Protect protect
)
{
#ifdef INCLUDE_PEATRANS
    *pRetVirtAddr = reinterpret_cast<void*>(physAddr);
    return OK;
#endif
    // Alter caching only for physical addresses above one megabyte.
    //
    // The x86 emulator from RM used by MODS to call VBIOS needs
    // to map some very low physical addresses (like 0x0 and 0xA0000).
    // We don't want to alter caching type of these addresses,
    // especially because it would clash with caching type already
    // used by Linux. The default caching type will be used for
    // these addresses (which is probably WB for 0x0 and UC-
    // for 0xA0000).
    //
    // Higher physical addresses mapped by MODS belong to BARs
    // and this is what SET_MEMORY_TYPE is for.
    // We are not checking or enforcing it here though.
    RC rc;
    if (!Platform::IsTegra() && (physAddr < 1_MB))
    {
        attrib = Memory::DE;
    }

#if defined(LWCPU_AARCH64)
    if (attrib == Memory::UC)
    {
        Printf(Tee::PriLow, "Warning: MAP 0x%08llx (0x%08llx) as UC\n", physAddr, (UINT64)size);
    }
#endif

    if (s_DisableWC && attrib == Memory::WC)
    {
        attrib = Memory::UC;
        Printf(Tee::PriLow, "WC mapping colwerted to UC.\n");
    }

    *pRetVirtAddr = s_KD.Mmap(physAddr, size, protect, attrib);

    if (*pRetVirtAddr == nullptr)
    {
        Printf(Tee::PriHigh, "Error: Can't map device memory at phys addr 0x%llx\n", physAddr);
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

//------------------------------------------------------------------------------
//!< Undo the mapping created by MapDeviceMemory.
void Xp::UnMapDeviceMemory(void* virtAddr)
{
#ifdef INCLUDE_PEATRANS
    return;
#endif
    // Allow unmaps of NULL
    if (!virtAddr)
    {
        return;
    }

    s_KD.Munmap(virtAddr);
}

///------------------------------------------------------------------------------
// Find a PCI device with given 'DeviceId', 'VendorId', and 'Index'.
// Return '*pDomainNumber', '*pBusNumber', '*pDeviceNumber', and '*pFunctionNumber'.
//
RC Xp::FindPciDevice
(
    int32   DeviceId,
    int32   VendorId,
    int32   Index,
    UINT32* pDomainNumber   /* = 0 */,
    UINT32* pBusNumber      /* = 0 */,
    UINT32* pDeviceNumber   /* = 0 */,
    UINT32* pFunctionNumber /* = 0 */
)
{
    return s_KD.FindPciDevice(DeviceId, VendorId, Index, pDomainNumber, pBusNumber, pDeviceNumber, pFunctionNumber);
}

//------------------------------------------------------------------------------
// @param ClassCode The entire 24 bit register baseclass/subclass/pgmIF,
//                  a byte for each field
//
RC Xp::FindPciClassCode
(
    INT32   ClassCode,
    INT32   Index,
    UINT32* pDomainNumber   /* = 0 */,
    UINT32* pBusNumber      /* = 0 */,
    UINT32* pDeviceNumber   /* = 0 */,
    UINT32* pFunctionNumber /* = 0 */
)
{
    return s_KD.FindPciClassCode(ClassCode, Index, pDomainNumber, pBusNumber, pDeviceNumber, pFunctionNumber);
}

//------------------------------------------------------------------------------
// Force a rescan of PCI bus.
RC Xp::RescanPciBus
(
    INT32 domain,
    INT32 bus
)
{
#if defined(MODS_ESC_PCI_BUS_RESCAN)
    MODS_PCI_BUS_RESCAN data = {};
    data.domain = domain;
    data.bus = bus;

    if (s_KD.Ioctl(MODS_ESC_PCI_BUS_RESCAN, &data))
    {
        return RC::PCI_FUNCTION_NOT_SUPPORTED;
    }
#else
    MODS_PCI_BUS_ADD_DEVICES data = {};
    data.bus = bus;

    if (s_KD.Ioctl(MODS_ESC_PCI_BUS_ADD_DEVICES, &data))
    {
        return RC::PCI_FUNCTION_NOT_SUPPORTED;
    }
#endif
    return RC::OK;
}

RC Xp::PciGetBarInfo
(
    INT32 DomainNumber,
    INT32 BusNumber,
    INT32 DeviceNumber,
    INT32 FunctionNumber,
    INT32 BarIndex,
    UINT64* pBaseAddress,
    UINT64* pBarSize
)
{
    return s_KD.PciGetBarInfo(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, BarIndex, pBaseAddress, pBarSize);
}

RC Xp::PciEnableSriov
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT32 vfCount
)
{
    RC rc;

    // Kernel returns error if we try to enable SRIOV when it's
    // already enabled, so disable it first.
    CHECK_RC(s_KD.PciEnableSriov(domain, bus, device, function, 0));

    if (vfCount != 0)
    {
        CHECK_RC(s_KD.PciEnableSriov(domain, bus, device, function, vfCount));
    }
    return rc;
}

RC Xp::PciGetIRQ
(
    INT32 DomainNumber,
    INT32 BusNumber,
    INT32 DeviceNumber,
    INT32 FunctionNumber,
    UINT32* pIrq
)
{
    return s_KD.PciGetIRQ(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, pIrq);
}

//------------------------------------------------------------------------------
// Read byte, word, or dword at 'Address' from a PCI device with the given
// 'DomainNumber', 'BusNumber', 'DeviceNumber', and 'FunctionNumber'.
// Return read data in '*pData'.
//
RC Xp::PciRead08
(
    INT32    DomainNumber,
    INT32    BusNumber,
    INT32    DeviceNumber,
    INT32    FunctionNumber,
    INT32    Address,
    UINT08 * pData
)
{
    return s_KD.PciRead(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, pData);
}

RC Xp::PciRemove
(
    INT32    domainNumber,
    INT32    busNumber,
    INT32    deviceNumber,
    INT32    functionNumber
)
{
    return s_KD.PciRemove(domainNumber, busNumber, deviceNumber, functionNumber);
}

RC Xp::PciResetFunction
(
    INT32    domainNumber,
    INT32    busNumber,
    INT32    deviceNumber,
    INT32    functionNumber
)
{
    return s_KD.PciResetFunction(domainNumber, busNumber, deviceNumber, functionNumber);
}

RC Xp::PciRead16
(
    INT32    DomainNumber,
    INT32    BusNumber,
    INT32    DeviceNumber,
    INT32    FunctionNumber,
    INT32    Address,
    UINT16 * pData
)
{
    return s_KD.PciRead(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, pData);
}

RC Xp::PciRead32
(
    INT32    DomainNumber,
    INT32    BusNumber,
    INT32    DeviceNumber,
    INT32    FunctionNumber,
    INT32    Address,
    UINT32 * pData
)
{
    return s_KD.PciRead(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, pData);
}

//------------------------------------------------------------------------------
// Write 'Data' at 'Address' to a PCI device with the given
// 'DomainNumber', 'BusNumber', 'DeviceNumber', and 'FunctionNumber'.
//
RC Xp::PciWrite08
(
    INT32  DomainNumber,
    INT32  BusNumber,
    INT32  DeviceNumber,
    INT32  FunctionNumber,
    INT32  Address,
    UINT08 Data
)
{
    return s_KD.PciWrite(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, Data);
}

RC Xp::PciWrite16
(
    INT32  DomainNumber,
    INT32  BusNumber,
    INT32  DeviceNumber,
    INT32  FunctionNumber,
    INT32  Address,
    UINT16 Data
)
{
    return s_KD.PciWrite(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, Data);
}

RC Xp::PciWrite32
(
    INT32  DomainNumber,
    INT32  BusNumber,
    INT32  DeviceNumber,
    INT32  FunctionNumber,
    INT32  Address,
    UINT32 Data
)
{
    return s_KD.PciWrite(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, Data);
}

#if defined(TEGRA_MODS)
static long TimeoutInterrupt(void*)
{
    Xp::IsrResult isrR = Xp::ISR_RESULT_SERVICED;

    // Reset the hung system watchdog

    TimerController *pTimerCtrl = NULL;
    TimerControllerMgr::Mgr()->Get(0, reinterpret_cast<Controller**>(&pTimerCtrl));
    if (!pTimerCtrl)
    {
        Printf(Tee::PriError,"Fail to get handle of Timer %i", 0);
        return isrR;
    }
    pTimerCtrl->RegWr(s_TimerWriteAddress, 0x40000000);

    WdtController *pHungCtrl = NULL;
    WdtControllerMgr::Mgr()->Get(WATCHDOG_HUNG_TEST_INDEX, (Controller**)(&pHungCtrl));
    if (!pHungCtrl)
    {
        Printf(Tee::PriError,"Fail to get handle of Wdt %i", WATCHDOG_HUNG_TEST_INDEX);
        return isrR;
    }
    pHungCtrl->StartCounter();

    // Check to see if the timeout wacthdog has expired
    UINT32 expirationCount = 0;
    WdtController *pTimeoutCtrl = NULL;
    WdtControllerMgr::Mgr()->Get(WATCHDOG_TIMEOUT_INDEX, (Controller**)(&pTimeoutCtrl));
    if (!pTimeoutCtrl)
    {
        Printf(Tee::PriError,"Fail to get handle of Wdt %i", WATCHDOG_TIMEOUT_INDEX);
        return isrR;
    }
    pTimeoutCtrl->GetExpirationCount(&expirationCount);

    if (expirationCount > 0)
    {
        // Turn off the watchdogs and report a timeout error
        isrR = Xp::ISR_RESULT_TIMEOUT;

        pTimeoutCtrl->Cleanup();
        pHungCtrl->Cleanup();
    }
    return isrR;
}
#endif

RC Xp::InitWatchdogs(UINT32 timeoutValue)
{
#if defined(TEGRA_MODS)
    RC rc = OK;

    // Callwlate the register offset for resetting the timer
    UINT32 value = (TIMER_HUNG_TEST_INDEX - 1) * 8;
    UINT32 offset = 0x4;
    if (TIMER_HUNG_TEST_INDEX > 2)
        offset = 0x44;
    s_TimerWriteAddress = offset + value;

    // Callwlate the correct trigger value/timeout value combination
    UINT32 triggerValue = 1000000; // Represents one second
    if (timeoutValue > 255) //Watchdog register is 8-bit, max value is 255
    {
        UINT32 mult = (timeoutValue + 255)/255; // Ceiling of timeoutValue / 255
        triggerValue *= mult;
        timeoutValue /= mult;
    }

    // Initialize the timeout timer
    WdtController *pTimeoutCtrl = NULL;
    CHECK_RC(WdtControllerMgr::Mgr()->Get(WATCHDOG_TIMEOUT_INDEX, (Controller**)(&pTimeoutCtrl)));
    CHECK_RC(pTimeoutCtrl->Init());
    CHECK_RC(pTimeoutCtrl->Setup( TIMER_TIMEOUT_INDEX,
                         timeoutValue, triggerValue, TimeoutInterrupt));

    // Initialize the hung test timer (the timeout value is set to 4
    // seconds so that the system will be reset in 16 seconds if test
    // has hung)
    WdtController *pHungCtrl = NULL;
    CHECK_RC(WdtControllerMgr::Mgr()->Get(WATCHDOG_HUNG_TEST_INDEX, (Controller**)(&pHungCtrl)));
    CHECK_RC(pHungCtrl->Init());
    CHECK_RC(pHungCtrl->Setup(TIMER_HUNG_TEST_INDEX,
                         4, triggerValue, TimeoutInterrupt));

    return rc;
#else
    Printf(Tee::PriDebug, "Skipping watchdog initialization since"
            " it is not supported\n");
    return OK;
#endif
}

RC Xp::ResetWatchdogs()
{
#if defined(TEGRA_MODS)
    RC rc = OK;

    WdtController *pTimeoutCtrl;
    CHECK_RC(WdtControllerMgr::Mgr()->Get(WATCHDOG_TIMEOUT_INDEX, (Controller**)(&pTimeoutCtrl)));
    CHECK_RC(pTimeoutCtrl->Cleanup());

    WdtController *pHungCtrl;
    CHECK_RC(WdtControllerMgr::Mgr()->Get(WATCHDOG_HUNG_TEST_INDEX, (Controller**)(&pHungCtrl)));
    CHECK_RC(pHungCtrl->Cleanup());

    return rc;
#else
    Printf(Tee::PriDebug, "Skipping watchdog resetting since it is"
            " not supported\n");
    return OK;
#endif
}

// Interrupt functions.
//

bool Xp::IRQTypeSupported(UINT32 irqType)
{
    switch (irqType)
    {
        case MODS_XP_IRQ_TYPE_INT:
        case MODS_XP_IRQ_TYPE_MSI:
        case MODS_XP_IRQ_TYPE_CPU:
        case MODS_XP_IRQ_TYPE_MSIX:
            return true;
    }

    Printf(Tee::PriError, "Unknown interrupt type %d\n", irqType);
    MASSERT(!"Unknown interrupt type");
    return false;
}

void Xp::ProcessPendingInterrupts()
{
}

RC Xp::MapIRQ(UINT32 * logicalIrq, UINT32 physIrq, string dtName, string fullName)
{
#if defined(TEGRA_MODS)
    if (s_KD.IsOpen())
    {
        return s_KD.MapIRQ(logicalIrq, physIrq, dtName, fullName);
    }
    else
    {
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

RC Xp::HookIRQ(UINT32 Irq, ISR func, void* cookie)
{
#if defined(TEGRA_MODS)
    return s_KD.HookIsr(0, Irq, 0xff, 0xff, MODS_XP_IRQ_TYPE_CPU, func, cookie);
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

RC Xp::UnhookIRQ(UINT32 Irq, ISR func, void* cookie)
{
#if defined(TEGRA_MODS)
    return s_KD.UnHookIsr(0, Irq, 0xff, 0xff, MODS_XP_IRQ_TYPE_CPU, func, cookie);
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

RC Xp::HookInt(const IrqParams& irqInfo, ISR func, void* cookie)
{
    return s_KD.HookIsr(irqInfo, func, cookie);
}

RC Xp::UnhookInt(const IrqParams& irqInfo, ISR func, void* cookie)
{
    return s_KD.UnHookIsr(irqInfo.pciDev.domain, irqInfo.pciDev.bus,
                          irqInfo.pciDev.device, irqInfo.pciDev.function,
                          irqInfo.irqType, func, cookie);
}

void Xp::EnableInterrupts()
{
    s_KD.EnableInterrupts();
}

void Xp::DisableInterrupts()
{
    s_KD.DisableInterrupts();
}

Platform::Irql Xp::GetLwrrentIrql()
{
    return s_KD.GetLwrrentIrql();
}

Platform::Irql Xp::RaiseIrql(Platform::Irql newIrql)
{
    return s_KD.RaiseIrql(newIrql);
}

void Xp::LowerIrql(Platform::Irql newIrql)
{
    s_KD.LowerIrql(newIrql);
}

//-----------------------------------------------------------------------------
// Implementation for Xp::PioRead(08,16,32)
//
RC static PioRead
(
    UINT16  Port,
    UINT32  *pData,
    INT32   DataSize
)
{
    INT32   ret;
    MODS_PIO_READ   ModsPioReadData = {0};

    MASSERT(pData != 0);

    // set private members
    ModsPioReadData.port         = Port;
    ModsPioReadData.data_size    = DataSize;

    ret = s_KD.Ioctl(MODS_ESC_PIO_READ, &ModsPioReadData);

    if (ret)
    {
        Printf(Tee::PriHigh, "Error: Can't read from Port\n");
        return RC::SOFTWARE_ERROR;

    }

    *pData = ModsPioReadData.data;

    return OK;
}

//------------------------------------------------------------------------------
// Port read and write functions.
//
RC Xp::PioRead08(UINT16 Port, UINT08 * pData)
{
    RC rc;
    UINT32 Data;

    rc = PioRead(Port, &Data, 1);

    *pData = (UINT08)Data;
    return rc;
}

RC Xp::PioRead16(UINT16 Port, UINT16 * pData)
{
    RC rc;
    UINT32 Data;

    rc = PioRead(Port, &Data, 2);

    *pData = (UINT16)Data;
    return rc;
}

RC Xp::PioRead32(UINT16 Port, UINT32 * pData)
{
    RC rc;
    UINT32 Data;

    rc = PioRead(Port, &Data, 4);

    *pData = Data;
    return rc;
}

//-----------------------------------------------------------------------------
// Implementation for Xp::PioWrite(08,16,32)
//
RC static PioWrite
(
    UINT16  Port,
    UINT32  Data,
    INT32   DataSize
)
{
    INT32   ret;
    MODS_PIO_WRITE   ModsPioWriteData = {0};

    // set private members
    ModsPioWriteData.port         = Port;
    ModsPioWriteData.data         = Data;
    ModsPioWriteData.data_size    = DataSize;

    ret = s_KD.Ioctl(MODS_ESC_PIO_WRITE, &ModsPioWriteData);

    if (ret)
    {
        Printf(Tee::PriHigh, "Error: Can't write to Port\n");
        return RC::SOFTWARE_ERROR;

    }

    return OK;
}

RC Xp::PioWrite08(UINT16 Port, UINT08 Data)
{
    return PioWrite(Port, Data, 1);
}

RC Xp::PioWrite16(UINT16 Port, UINT16 Data)
{
    return PioWrite(Port, Data, 2);
}

RC Xp::PioWrite32(UINT16 Port, UINT32 Data)
{
    return PioWrite(Port, Data, 4);
}

static RC CallACPI_SLIA_Eval(UINT08 *pOut, UINT32 *pOutSize)
{
    if (pOut == NULL)
        return RC::SOFTWARE_ERROR;

    const UINT32  LW_ACPI_METHOD_SLIA = 0x41 << 24 | 0x49 << 16 | 0x4c << 8 | 0x53;
    MODS_EVAL_ACPI_METHOD ModsEvalAcpiMethodData;

    ModsEvalAcpiMethodData.argument_count = 3;
    strncpy(ModsEvalAcpiMethodData.method_name, "WMMX", ACPI_MAX_METHOD_LENGTH);

    ModsEvalAcpiMethodData.argument[0].type  = ACPI_MODS_TYPE_INTEGER;
    ModsEvalAcpiMethodData.argument[0].integer.value = 0;
    ModsEvalAcpiMethodData.argument[1].type  = ACPI_MODS_TYPE_INTEGER;
    ModsEvalAcpiMethodData.argument[1].integer.value = 0;
    ModsEvalAcpiMethodData.argument[2].type = ACPI_MODS_TYPE_INTEGER;
    ModsEvalAcpiMethodData.argument[2].integer.value = LW_ACPI_METHOD_SLIA;

    ModsEvalAcpiMethodData.out_data_size = ACPI_MAX_BUFFER_LENGTH;
    INT32   ret;
    ret = s_KD.Ioctl(MODS_ESC_EVAL_ACPI_METHOD, &ModsEvalAcpiMethodData);

    if (ret)
    {
        Printf(Tee::PriLow, "ACPI: Unable to retrieve SLIA structure \n");
        return RC::SOFTWARE_ERROR;
    }

    *pOutSize =  ModsEvalAcpiMethodData.out_data_size;
    memcpy(pOut, ModsEvalAcpiMethodData.out_buffer, *pOutSize);
    Printf(Tee::PriDebug, "ACPI: SLIA structure successfuly read (0x%x bytes)\n", *pOutSize);
                                                                                return OK;
}

static RC CallACPI_MXD_Eval
(
    UINT32 GpuInst,
    UINT32 acpiId,
    const char *pMethodName,
    UINT32 *pMuxQueryStatus
)
{
    RC rc;
    MASSERT(pMuxQueryStatus);
    MASSERT(pMethodName);

    if ((strcmp(pMethodName, "MXDS") != 0)
        && (strcmp(pMethodName, "MXDM") != 0))
    {
        Printf(Tee::PriError,
               "Unsupported ACPI method (%s)\n",
               pMethodName);
        MASSERT(!("This function cannot be called with requested ACPI method "
                  "(refer previous error message)"));
        return RC::SOFTWARE_ERROR;
    }

    constexpr UINT32 expectedOutputSize = 4;
    MODS_EVAL_ACPI_METHOD ModsEvalAcpiMethodData = {};
    ModsEvalAcpiMethodData.argument_count = 1;
    ModsEvalAcpiMethodData.argument[0].type = ACPI_MODS_TYPE_INTEGER;
    ModsEvalAcpiMethodData.argument[0].integer.value = *pMuxQueryStatus;
    strncpy(ModsEvalAcpiMethodData.method_name,
            pMethodName,
            MIN(strlen(pMethodName), ACPI_MAX_BUFFER_LENGTH));

    CHECK_RC(s_KD.EvalDevAcpiMethod(GpuInst, acpiId, &ModsEvalAcpiMethodData));

    if (ModsEvalAcpiMethodData.out_data_size != expectedOutputSize)
    {
        Printf(Tee::PriError,
               "ACPI: Incorrect output returned by SB_PCI_PEG0_XXXX_%s "
               "method with ACPI ID = 0x%x. Expected = %u, Received = %u\n",
               pMethodName,
               acpiId,
               expectedOutputSize,
               ModsEvalAcpiMethodData.out_data_size);
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }
    *pMuxQueryStatus = (static_cast<UINT32>(ModsEvalAcpiMethodData.out_buffer[3]) << 24)
                       | (static_cast<UINT32>(ModsEvalAcpiMethodData.out_buffer[2]) << 16)
                       | (static_cast<UINT32>(ModsEvalAcpiMethodData.out_buffer[1]) << 8)
                       | (static_cast<UINT32>(ModsEvalAcpiMethodData.out_buffer[0]));
    return rc;
}

static RC ACPI_MXDS_Eval(UINT32 GpuInst, UINT32 acpiId, UINT32 *pMuxQueryStatus)
{
    return CallACPI_MXD_Eval(GpuInst, acpiId, "MXDS", pMuxQueryStatus);
}

static RC ACPI_MXDM_Eval(UINT32 GpuInst, UINT32 acpiId, UINT32 *pMuxQueryStatus)
{
    return CallACPI_MXD_Eval(GpuInst, acpiId, "MXDM", pMuxQueryStatus);
}

#if defined(INCLUDE_GPU)
RC Xp::LinuxInternal::AcpiDsmEval
(
      UINT16 domain
    , UINT16 bus
    , UINT08 dev
    , UINT08 func
    , UINT32 Function
    , UINT32 DSMMXMSubfunction
    , UINT32 *pInOut
    , UINT16 *size
)
{
    RC rc;
    if (pInOut == NULL)
    {
        return RC::SOFTWARE_ERROR;
    }

    // On CheetAh there is no ACPI, so we lwrrently fake the DCB.
    // In the future we will read the DCB from the kernel's display tree.
    if (Platform::IsTegra() && Function == MODS_DRV_CALL_ACPI_GENERIC_DSM_NBCI)
    {
        const UINT32 NBSI_TEGRA_DCB = 0x5444;
        const UINT32 NBSI_TEGRA_DSI = 0x5453;
        const UINT32 NBSI_TEGRA_TMDS = 0x5454;
        if (DSMMXMSubfunction == LW_NBCI_FUNC_SUPPORT && *size >= 4)
        {
            *pInOut = (1<<LW_NBCI_FUNC_SUPPORT) | (1<<LW_NBCI_FUNC_GETOBJBYTYPE);
            *size = 4;
            return OK;
        }
        else if (DSMMXMSubfunction == LW_NBCI_FUNC_GETOBJBYTYPE &&
                 *pInOut == ((NBSI_TEGRA_DCB)<<16))
        {
            // Fake first 4 DWORDs (DSM header)
            const UINT32 fourDwords = 4*sizeof(UINT32);
            const UINT32 minDcbSize = 5*sizeof(UINT32);
            if (*size < fourDwords + minDcbSize)
            {
                return RC::MEMORY_SIZE_ERROR;
            }
            memset(pInOut, 0, fourDwords);
            UINT32 dcbSize = *size - fourDwords;
            const RC rc = CheetAh::SocPtr()->GetDCB(pInOut+4, &dcbSize);
            *size = dcbSize + fourDwords;
            return rc;
        }
        else if (DSMMXMSubfunction == LW_NBCI_FUNC_GETOBJBYTYPE &&
                 *pInOut == ((NBSI_TEGRA_DSI)<<16))
        {
            vector<UINT08> dsi;
            const RC rc = CheetAh::SocPtr()->GetDsiInfoBlock(&dsi);
            const UINT32 dsiSize = dsi.size();
            if (*size < dsiSize) {
                return RC::DATA_TOO_LARGE;
            }
            memset(pInOut, 0, *size);
            *(pInOut) = 0x0100;
            *(pInOut + 2) = (((dsiSize*4) & 0xFFFF) << 16) | NBSI_TEGRA_DSI;
            *(pInOut + 3) = ((dsiSize*4) >> 16);
            memcpy(pInOut + 4, &dsi[0], dsiSize);
            return rc;
        }
        else if (DSMMXMSubfunction == LW_NBCI_FUNC_GETOBJBYTYPE &&
                 *pInOut == ((NBSI_TEGRA_TMDS)<<16))
        {
            // See https://wiki.lwpu.com/engwiki/index.php/Tegra_TMDS_Configuration_Block
            vector<UINT08> ttcb;
            CheetAh::SocPtr()->GetTTCBData(&ttcb);
            const UINT32 ttcbSize = ttcb.size();
            if (ttcbSize == 0)
            {
                // If board JS file does not specify a TTCB, ignore
                return OK;
            }

            // Need 4 dwords for DSM header
            const UINT32 dsmHeaderSize = 4 * sizeof(UINT32);
            const UINT32 totSize = ttcbSize + dsmHeaderSize;
            if (*size < totSize) {
                return RC::DATA_TOO_LARGE;
            }

            memset(pInOut, 0, *size);
            *(pInOut + 2) = (((totSize * 4) & 0xFFFF) << 16) | NBSI_TEGRA_TMDS;
            *(pInOut + 3) = ((totSize * 4) >> 16);
            memcpy(pInOut + 4, &ttcb[0], ttcbSize);
            return OK;
        }
        // fall through in case Linux adds ACPI in the future on CheetAh
    }

    static const UINT08 nbsiId[] = NBSI_DSM_GUID_STR;
    static const UINT08 lwhgId[] = LWHG_DSM_GUID_STR;
    static const UINT08 mxmId[]  = MXM_DSM_GUID_STR;
    static const UINT08 nbciId[] = NBCI_DSM_GUID_STR;
    static const UINT08 lwopId[] = LWOP_DSM_GUID_STR;
    static const UINT08 jtId[]   = JT_DSM_GUID_STR;

    const UINT08* pAcpiDsmGuid = 0;
    UINT32 acpiDsmRev = 0;
    switch (Function)
    {
        case MODS_DRV_CALL_ACPI_GENERIC_DSM_NBSI:
            pAcpiDsmGuid = nbsiId;
            acpiDsmRev  = NBSI_REVISION_ID;
            break;
        case MODS_DRV_CALL_ACPI_GENERIC_DSM_LWHG:
            pAcpiDsmGuid = lwhgId;
            acpiDsmRev  = LWHG_REVISION_ID;
            break;
        case MODS_DRV_CALL_ACPI_GENERIC_DSM_MXM:
            pAcpiDsmGuid = mxmId;
            acpiDsmRev  = ACPI_MXM_REVISION_ID;
            break;
        case MODS_DRV_CALL_ACPI_GENERIC_DSM_NBCI:
            pAcpiDsmGuid = nbciId;
            acpiDsmRev  = NBCI_REVISION_ID;
            break;
        case MODS_DRV_CALL_ACPI_GENERIC_DSM_LWOP:
            pAcpiDsmGuid = lwopId;
            acpiDsmRev  = LWOP_REVISION_ID;
            break;
        case MODS_DRV_CALL_ACPI_GENERIC_DSM_JT:
            pAcpiDsmGuid = jtId;
            acpiDsmRev = JT_REVISION_ID;
            break;
        default:
            return RC::UNSUPPORTED_FUNCTION;
    }

    MODS_EVAL_ACPI_METHOD ModsEvalAcpiMethodData = {{0}};

    ModsEvalAcpiMethodData.argument_count = 4;
    strcpy(ModsEvalAcpiMethodData.method_name, "_DSM");

    const UINT32 acpiGuidLength = 0x10;
    ModsEvalAcpiMethodData.argument[0].type             = ACPI_MODS_TYPE_BUFFER;
    ModsEvalAcpiMethodData.argument[0].buffer.length    = acpiGuidLength;
    ModsEvalAcpiMethodData.argument[0].buffer.offset    = 0;
    memcpy(ModsEvalAcpiMethodData.in_buffer, pAcpiDsmGuid, acpiGuidLength);

    ModsEvalAcpiMethodData.argument[1].type             = ACPI_MODS_TYPE_INTEGER;
    ModsEvalAcpiMethodData.argument[1].integer.value    = acpiDsmRev;

    ModsEvalAcpiMethodData.argument[2].type             = ACPI_MODS_TYPE_INTEGER;
    ModsEvalAcpiMethodData.argument[2].integer.value    = DSMMXMSubfunction;

    ModsEvalAcpiMethodData.argument[3].type             = ACPI_MODS_TYPE_BUFFER;
    ModsEvalAcpiMethodData.argument[3].buffer.length    = 4;
    ModsEvalAcpiMethodData.argument[3].buffer.offset    = acpiGuidLength;
    memcpy(&ModsEvalAcpiMethodData.in_buffer[acpiGuidLength], pInOut, 4);

    CHECK_RC(s_KD.EvalDevAcpiMethod(domain, bus, dev, func, ACPI_MODS_IGNORE_ACPI_ID,
                                    &ModsEvalAcpiMethodData));

    if ((*size >= ModsEvalAcpiMethodData.out_data_size) ||
        // According to Linux driver code, RM could pass 0 expecting an int
        ((*size == 0) && (ModsEvalAcpiMethodData.out_data_size == 4)))
    {
        *size = ModsEvalAcpiMethodData.out_data_size;
        memcpy(pInOut, ModsEvalAcpiMethodData.out_buffer, *size);
    }
    else
    {
        Printf(Tee::PriLow, "ACPI: Output buffer too small for _DSM structure (0x%x bytes, need 0x%x)\n",
                (unsigned)*size, (unsigned)ModsEvalAcpiMethodData.out_data_size);
        return RC::MEMORY_SIZE_ERROR;
    }
    Printf(Tee::PriDebug, "ACPI: _DSM structure successfuly read (0x%x bytes)\n", (unsigned)*size);

    return rc;
}

RC Xp::LinuxInternal::AcpiRootPortEval
(
    UINT16 domain
    , UINT16 bus
    , UINT08 device
    , UINT08 function
    , Xp::LinuxInternal::AcpiPowerMethod method
    , UINT32 *pStatus
)
{
    RC rc;

    MODS_EVAL_ACPI_METHOD acpiMethod = {};
    string methodName;

    constexpr UINT32 PR0_SUPPORT_VERSION = 0x399;
    if (s_KD.GetVersion() >= PR0_SUPPORT_VERSION)
    {
        MODS_EVAL_ACPI_METHOD rootMethod = {};
        rootMethod.argument_count = 0;

        // Fetch the root port handler.
        strncpy(rootMethod.method_name, "_PR0", sizeof(rootMethod.method_name) - 1);
        CHECK_RC(s_KD.EvalDevAcpiMethod(domain, bus, device, function,
                ACPI_MODS_IGNORE_ACPI_ID, &rootMethod));

        acpiMethod.argument_count = 1;
        acpiMethod.argument[0].type = ACPI_MODS_TYPE_METHOD;
        memcpy(&acpiMethod.argument[0].method.handle,
            rootMethod.out_buffer, rootMethod.out_data_size);
    }
    else
    {
        acpiMethod.argument_count = 0;
        methodName += "PG00.";
    }

    switch (method)
    {
        case Xp::LinuxInternal::AcpiPowerMethod::AcpiOn:
            methodName += "_ON";
            break;
        case Xp::LinuxInternal::AcpiPowerMethod::AcpiOff:
            methodName += "_OFF";
            break;
        case Xp::LinuxInternal::AcpiPowerMethod::AcpiStatus:
            methodName += "_STA";
            break;
        default:
            return RC::SOFTWARE_ERROR;
    }

    // Call the power cycle methods using root port handler
    strncpy(acpiMethod.method_name, methodName.c_str(), sizeof(acpiMethod.method_name) - 1);
    CHECK_RC(s_KD.EvalDevAcpiMethod(domain, bus, device, function, ACPI_MODS_IGNORE_ACPI_ID,
                                    &acpiMethod));
    if (pStatus)
    {
        memcpy(pStatus, acpiMethod.out_buffer, acpiMethod.out_data_size);
    }
    return rc;
}

static RC AcpiDsmEval
(
    UINT32 GpuInst
    , UINT32 Function
    , UINT32 DSMMXMSubfunction
    , UINT32 *pInOut
    , UINT16 *size
)
{
    RC rc;
    mods_pci_dev_2 dev;
#ifdef INCLUDE_GPU
    GpuDevMgr* const pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
    if (!pGpuDevMgr)
        return RC::SOFTWARE_ERROR;
    GpuSubdevice* const pSubdev = pGpuDevMgr->GetSubdeviceByGpuInst(GpuInst);
    if (!pSubdev)
        return RC::SOFTWARE_ERROR;
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();
    dev.domain   = pGpuPcie->DomainNumber();
    dev.bus      = pGpuPcie->BusNumber();
    dev.device   = pGpuPcie->DeviceNumber();
    dev.function = pGpuPcie->FunctionNumber();
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif

    if (Function == MODS_DRV_CALL_ACPI_GENERIC_DSM_JT)
    {
        Xp::JTMgr * pJT = Xp::GetJTMgr(GpuInst);
        if (pJT)
        {
            return(pJT->DSM_Eval(dev.domain, dev.bus, dev.device, dev.function,
                    Function, DSMMXMSubfunction, pInOut, size));
        }
    }

    return Xp::LinuxInternal::AcpiDsmEval(dev.domain, dev.bus, dev.device, dev.function,
                            Function, DSMMXMSubfunction, pInOut, size);
}
#endif

static RC CallACPI_LWHG_ROM_Eval(UINT32 GpuInst, UINT32 *pIn, UINT32 *pOut)
{
    RC rc;

    if ((pIn == NULL) || (pOut == NULL))
        return  RC::SOFTWARE_ERROR;

    MODS_EVAL_ACPI_METHOD ModsEvalAcpiMethodData = {{0}};

    ModsEvalAcpiMethodData.argument_count = 2;
    strcpy(ModsEvalAcpiMethodData.method_name, "_ROM");

    ModsEvalAcpiMethodData.argument[0].type             = ACPI_MODS_TYPE_INTEGER;
    ModsEvalAcpiMethodData.argument[0].integer.value    = pIn[0];
    ModsEvalAcpiMethodData.argument[1].type             = ACPI_MODS_TYPE_INTEGER;
    ModsEvalAcpiMethodData.argument[1].integer.value    = pIn[1];
    const UINT32 Size = pIn[1];

    CHECK_RC(s_KD.EvalDevAcpiMethod(GpuInst, ACPI_MODS_IGNORE_ACPI_ID, &ModsEvalAcpiMethodData));

    if (Size <= ModsEvalAcpiMethodData.out_data_size)
    {
        memcpy(pOut, ModsEvalAcpiMethodData.out_buffer, Size);
    }
    else
    {
        Printf(Tee::PriLow, "ACPI: Output buffer too small for _ROM structure (0x%x bytes, need 0x%x)\n",
                Size, (UINT32)ModsEvalAcpiMethodData.out_data_size);
        return RC::MEMORY_SIZE_ERROR;
    }
    Printf(Tee::PriDebug, "ACPI: _ROM structure successfuly read (0x%x bytes)\n", Size);

    return OK;
}

static RC CallACPI_DDC_Eval(UINT32 GpuInst, UINT32 displayIndex, UINT08 *pOut, UINT32 *pOutSize)
{
    RC rc;

    if ((pOut == NULL) || (pOutSize == NULL))
        return RC::SOFTWARE_ERROR;

    UINT32 outDataSize;
    UINT08 outBuffer[ACPI_MAX_BUFFER_LENGTH];
    CHECK_RC(s_KD.AcpiGetDDC(GpuInst, &outDataSize, &outBuffer[0]));

    if (outDataSize < *pOutSize)
    {
        *pOutSize = outDataSize;
    }
    if (outDataSize != *pOutSize)
    {
        Printf(Tee::PriLow, "ACPI: Output buffer too small for _DDC (EDID) structure (0x%x bytes, need 0x%x)\n",
                *pOutSize, outDataSize);
        return RC::MEMORY_SIZE_ERROR;
    }
    memcpy(pOut, outBuffer, *pOutSize);
    Printf(Tee::PriDebug, "ACPI: _DDC (EDID) structure successfuly read (0x%x bytes)\n", *pOutSize);

    return rc;
}

static RC CallACPI_LWHG_DOD_Eval
(
    UINT32 GpuInst,
    UINT08 *pInOut,
    UINT32 *pOutSize
)
{
    RC rc;

    if (pInOut == NULL)
        return RC::SOFTWARE_ERROR;

    MODS_EVAL_ACPI_METHOD ModsEvalAcpiMethodData = {{0}};

    ModsEvalAcpiMethodData.argument_count = 0;
    strcpy(ModsEvalAcpiMethodData.method_name, "_DOD");

    CHECK_RC(s_KD.EvalDevAcpiMethod(GpuInst, ACPI_MODS_IGNORE_ACPI_ID, &ModsEvalAcpiMethodData));

    if (ModsEvalAcpiMethodData.out_data_size < *pOutSize)
    {
        *pOutSize = ModsEvalAcpiMethodData.out_data_size;
    }
    if (ModsEvalAcpiMethodData.out_data_size != *pOutSize)
    {
        Printf(Tee::PriLow, "ACPI: Output buffer too small for _DOD structure (0x%x bytes, need 0x%x)\n",
                *pOutSize, (UINT32)ModsEvalAcpiMethodData.out_data_size);
        return RC::MEMORY_SIZE_ERROR;
    }
    memcpy(pInOut, ModsEvalAcpiMethodData.out_buffer, *pOutSize);
    Printf(Tee::PriDebug, "ACPI: _DOD structure successfuly read (0x%x bytes)\n", *pOutSize);

    return OK;
}

//------------------------------------------------------------------------------
RC Xp::CallACPIGeneric(UINT32 GpuInst,
                       UINT32 Function,
                       void *Param1,
                       void *Param2,
                       void *Param3)
{
    if (0 != GpuInst)
    {
        Printf(Tee::PriLow, "WARNING: ACPI on Linux for non-primary GPUs are not "
               "fully supported\n");
    }

    switch (Function)
    {
        case MODS_DRV_CALL_ACPI_GENERIC_SLIA:
            return CallACPI_SLIA_Eval((UINT08 *)Param2, (UINT32 *)Param3);
        case MODS_DRV_CALL_ACPI_GENERIC_MXDS:
            return ACPI_MXDS_Eval(GpuInst, (UINT32)(size_t)Param1, (UINT32 *)Param2);
        case MODS_DRV_CALL_ACPI_GENERIC_MXDM:
            return ACPI_MXDM_Eval(GpuInst, (UINT32)(size_t)Param1, (UINT32 *)Param2);
#if defined(INCLUDE_GPU)
        case MODS_DRV_CALL_ACPI_GENERIC_DSM_NBSI:
        case MODS_DRV_CALL_ACPI_GENERIC_DSM_LWHG:
        case MODS_DRV_CALL_ACPI_GENERIC_DSM_SPB:
        case MODS_DRV_CALL_ACPI_GENERIC_DSM_NBCI:
        case MODS_DRV_CALL_ACPI_GENERIC_DSM_LWOP:
        case MODS_DRV_CALL_ACPI_GENERIC_DSM_MXM:
        case MODS_DRV_CALL_ACPI_GENERIC_DSM_JT:
            return AcpiDsmEval(GpuInst, Function, (UINT32)(size_t)Param1, (UINT32*)Param2, (UINT16*)Param3);

#endif
        case MODS_DRV_CALL_ACPI_GENERIC_LWHG_ROM:
            return CallACPI_LWHG_ROM_Eval(GpuInst, (UINT32*)Param1, (UINT32*)Param2);
        case MODS_DRV_CALL_ACPI_GENERIC_DDC:
            return CallACPI_DDC_Eval(GpuInst, (UINT32)(size_t)Param1, (UINT08*)Param2, (UINT32*)Param3);
        case MODS_DRV_CALL_ACPI_GENERIC_LWHG_DOD:
            return CallACPI_LWHG_DOD_Eval(GpuInst, (UINT08*)Param1, (UINT32*)Param2);
        default:
            Printf(Tee::PriLow, "ACPI: Function %u not implemented\n", Function);
            return RC::UNSUPPORTED_FUNCTION;
    }

    return OK;
}

//------------------------------------------------------------------------------
RC Xp::LWIFMethod
(
    UINT32 Function,
    UINT32 SubFunction,
    void   *InParams,
    UINT16 InParamSize,
    UINT32 *OutStatus,
    void   *OutData,
    UINT16 *OutDataSize
)
{
    INT32   ret;
    UINT32  bufferPosition = 0;
    UINT32  LocalOutStatus = 0;
    MODS_EVAL_ACPI_METHOD ModsEvalAcpiMethodData = {{0}};

    if (Function != LW2080_CTRL_LWIF_FUNC_HDCP)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // set private members
    ModsEvalAcpiMethodData.argument_count = 3;
    strcpy(ModsEvalAcpiMethodData.method_name, "LWIF");

    ModsEvalAcpiMethodData.argument[0].type             = ACPI_MODS_TYPE_INTEGER;
    ModsEvalAcpiMethodData.argument[0].integer.value    = Function;

    ModsEvalAcpiMethodData.argument[1].type             = ACPI_MODS_TYPE_INTEGER;
    ModsEvalAcpiMethodData.argument[1].integer.value    = SubFunction;

    ModsEvalAcpiMethodData.argument[2].type             = ACPI_MODS_TYPE_BUFFER;

    if (InParams && (InParamSize > 0))
    {
        if (InParamSize + bufferPosition > ACPI_MAX_BUFFER_LENGTH)
        {
            Printf(Tee::PriHigh, "ACPI: Input buffer is too small for LWIF method param structure (0x%x bytes, need 0x%x)\n",
                    ACPI_MAX_BUFFER_LENGTH, InParamSize+bufferPosition);
            return RC::SOFTWARE_ERROR;
        }

        ModsEvalAcpiMethodData.argument[2].buffer.length    = InParamSize;
        ModsEvalAcpiMethodData.argument[2].buffer.offset    = bufferPosition;
        memcpy(ModsEvalAcpiMethodData.in_buffer, InParams, InParamSize);
        bufferPosition += InParamSize;
    }
    else
    {
        ModsEvalAcpiMethodData.argument[2].buffer.length = 8;
        ModsEvalAcpiMethodData.argument[2].buffer.offset = bufferPosition;
        memset(ModsEvalAcpiMethodData.in_buffer, 0, 8);
        bufferPosition += 8;
    }

    if (OutData && OutDataSize)
    {
        if (*OutDataSize > ACPI_MAX_BUFFER_LENGTH)
        {
            Printf(Tee::PriLow, "ACPI: Output buffer too small for LWIF method param structure (0x%x bytes, need 0x%x)\n",
                   ACPI_MAX_BUFFER_LENGTH, *OutDataSize);
            *OutDataSize = ACPI_MAX_BUFFER_LENGTH;
        }

        ModsEvalAcpiMethodData.out_data_size = *OutDataSize;
    }
    else
    {
        ModsEvalAcpiMethodData.out_data_size = 0;
    }

    ret = s_KD.Ioctl(MODS_ESC_EVAL_ACPI_METHOD, &ModsEvalAcpiMethodData);

    if (ret)
    {
        Printf(Tee::PriHigh, "Error: Unable to execute LWIF method\n");
        return RC::SOFTWARE_ERROR;
    }

    LocalOutStatus = *(unsigned long *) ModsEvalAcpiMethodData.out_buffer;
    if (OutStatus)
        *OutStatus = LocalOutStatus;

    if (LocalOutStatus == LW2080_CTRL_LWIF_STATUS_SUCCESS)
    {
        if (OutData && OutDataSize)
        {
            *OutDataSize = ModsEvalAcpiMethodData.out_data_size - 4;
            memcpy(OutData, ModsEvalAcpiMethodData.out_buffer + 4, *OutDataSize);
        }
        Printf(Tee::PriDebug, "ACPI: LWIF structure successfuly read (0x%x bytes)\n",
                ModsEvalAcpiMethodData.out_data_size);
    }
    else
    {
        Printf(Tee::PriHigh, "Error: Incorrect LWIF method return status 0x%x\n", LocalOutStatus);
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

#ifdef INCLUDE_GPU
namespace
{
    class LinuxOptimusMgr: public Xp::OptimusMgr
    {
    public:
        LinuxOptimusMgr(UINT16 domain, UINT08 bus, UINT08 device, UINT08 function, UINT32 caps)
            : m_Domain(domain), m_Bus(bus), m_Device(device), m_Function(function), m_Caps(caps) { }
        virtual ~LinuxOptimusMgr() { }
        virtual bool IsDynamicPowerControlSupported() const;
        virtual RC EnableDynamicPowerControl();
        virtual RC DisableDynamicPowerControl();
        virtual RC PowerOff();
        virtual RC PowerOn();
        static RC GetSetOptimusCaps(UINT16 domain, UINT08 bus, UINT08 device, UINT08 func, UINT32* caps);

    private:
        RC SetPowerState(bool on);

        UINT16 m_Domain;
        UINT08 m_Bus;
        UINT08 m_Device;
        UINT08 m_Function;
        UINT32 m_Caps;
    };

    class RTD3GCOffMgr : public Xp::OptimusMgr
    {
    public:
        RTD3GCOffMgr(UINT16 domain, UINT08 bus, UINT08 device, UINT08 function)
            : m_Domain(domain), m_Bus(bus), m_Device(device), m_Function(function) { }
        virtual ~RTD3GCOffMgr() { }
        bool IsDynamicPowerControlSupported() const override { return false; };
        RC EnableDynamicPowerControl() override { return OK; };
        RC DisableDynamicPowerControl() override { return OK; };
        RC PowerOff() override;
        RC PowerOn() override;
        static RC GetRootPortStatus
        (
            UINT16 domain,
            UINT08 bus,
            UINT08 device,
            UINT08 func,
            UINT32* caps
        );

    private:
        RC SetPowerState(Xp::PowerState state);
        UINT16 m_Domain;
        UINT08 m_Bus;
        UINT08 m_Device;
        UINT08 m_Function;
    };
} // anonymous namespace

bool LinuxOptimusMgr::IsDynamicPowerControlSupported() const
{
    return LWOP_FUNC_OPTIMUSCAPS_OPTIMUS_CAPABILITIES_DYNAMIC_POWER_CONTROL
                == DRF_VAL(OP, _FUNC_OPTIMUSCAPS, _OPTIMUS_CAPABILITIES, m_Caps);
}

RC LinuxOptimusMgr::EnableDynamicPowerControl()
{
    UINT32 caps = DRF_DEF(OP, _FUNC_OPTIMUSCAPS, _FLAGS, _PRESENT)
                | DRF_DEF(OP, _FUNC_OPTIMUSCAPS, _POWER_CONTROL, _POWER_DOWN_DGPU);
    return GetSetOptimusCaps(m_Domain, m_Bus, m_Device, m_Function, &caps);
}

RC LinuxOptimusMgr::DisableDynamicPowerControl()
{
    UINT32 caps = DRF_DEF(OP, _FUNC_OPTIMUSCAPS, _FLAGS, _PRESENT)
                | DRF_DEF(OP, _FUNC_OPTIMUSCAPS, _POWER_CONTROL, _DONOT_POWER_DOWN_DGPU);
    return GetSetOptimusCaps(m_Domain, m_Bus, m_Device, m_Function, &caps);
}

RC LinuxOptimusMgr::PowerOff()
{
    return SetPowerState(false);
}

RC LinuxOptimusMgr::PowerOn()
{
    return SetPowerState(true);
}

RC LinuxOptimusMgr::SetPowerState(bool on)
{
    RC rc;

    MODS_EVAL_ACPI_METHOD acpiMethod = {{0}};

    acpiMethod.argument_count = 0;

    const char* method = on ? "_PS0" : "_PS3";
    strcpy(acpiMethod.method_name, method);

    CHECK_RC(s_KD.EvalDevAcpiMethod(m_Domain, m_Bus, m_Device, m_Function,
                                    ACPI_MODS_IGNORE_ACPI_ID, &acpiMethod));

    // The Optimus spec DG-05012-001_v090 - see bug 1222833
    // doesn't define any return values from _PS0 and _PS3,
    // so don't check for them.

    return rc;
}

RC LinuxOptimusMgr::GetSetOptimusCaps(UINT16 domain, UINT08 bus, UINT08 dev, UINT08 func, UINT32* caps)
{
    UINT16 argLen = sizeof(*caps);
    RC rc;
    CHECK_RC(Xp::LinuxInternal::AcpiDsmEval(
                domain,bus,dev,func,
                MODS_DRV_CALL_ACPI_GENERIC_DSM_LWOP,
                LWOP_FUNC_OPTIMUSCAPS,
                caps,
                &argLen));
    return (argLen == sizeof(*caps)) ? OK : RC::UNSUPPORTED_FUNCTION;
}

RC RTD3GCOffMgr::PowerOff()
{
    return SetPowerState(Xp::PowerState::PowerOff);
}

RC RTD3GCOffMgr::PowerOn()
{
    return SetPowerState(Xp::PowerState::PowerOn);
}

RC RTD3GCOffMgr::SetPowerState(Xp::PowerState state)
{
    RC rc;
    Xp::LinuxInternal::AcpiPowerMethod method;

    switch (state)
    {
        case Xp::PowerState::PowerOff:
            method = Xp::LinuxInternal::AcpiPowerMethod::AcpiOff;
            break;
        case Xp::PowerState::PowerOn:
            method = Xp::LinuxInternal::AcpiPowerMethod::AcpiOn;
            break;
        default:
            return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(Xp::LinuxInternal::AcpiRootPortEval(
        m_Domain, m_Bus, m_Device, m_Function, method, nullptr));
    return rc;
}

RC RTD3GCOffMgr::GetRootPortStatus
(
    UINT16 domain,
    UINT08 bus,
    UINT08 device,
    UINT08 func,
    UINT32* caps
)
{
    RC rc;
    CHECK_RC(Xp::LinuxInternal::AcpiRootPortEval(domain, bus, device, func,
        Xp::LinuxInternal::AcpiPowerMethod::AcpiStatus, caps));
    return rc;
}
#endif

Xp::OptimusMgr* Xp::GetOptimusMgr(UINT32 domain, UINT32 bus, UINT32 device, UINT32 function)
{
#if defined(INCLUDE_GPU)
    if ((domain > 0xFFFFU) || (bus > 0xFFU) || (device > 0xFFU) || (function > 0xFFU))
    {
        Printf(Tee::PriLow, "Invalid PCI device passed to GetOptimusMgr - %04x:%x:%02x.%x\n",
                domain, bus, device, function);
        return 0;
    }

    UINT32 caps = 0;
    if (OK == RTD3GCOffMgr::GetRootPortStatus(domain, bus, device, function, &caps))
    {
        // If Root port methods are accessible, status should be on i.e, _STA = 0x1
        if (caps == 0x1)
        {
            return new RTD3GCOffMgr(domain, bus, device, function);
        }
        return 0;
    }

    if (OK != LinuxOptimusMgr::GetSetOptimusCaps(domain, bus, device, function, &caps))
    {
        return 0;
    }

    /*
       As per Optimus spec:
       LWOP_ERROR_SUCCESS     - 0x0XXXXXXX Success
       LWOP_ERROR_UNSPECIFIED - 0x80000001 Generic unspecified error code
       LWOP_ERROR_UNSUPPORTED - 0x80000002 Function/Subfunction code not supported
    */
    static const UINT32 LWOP_ERR_UNSPECIFIED = 0x80000001;
    static const UINT32 LWOP_ERR_UNSUPPORTED = 0x80000002;
    if (caps == LWOP_ERR_UNSPECIFIED)
    {
        // E.g. case is when optimus GUID/UUID doesn't match
        Printf(Tee::PriHigh, "Generic error returned by optimus ACPI call\n");
        return 0;
    }
    else if (caps == LWOP_ERR_UNSUPPORTED)
    {
        Printf(Tee::PriHigh, "Optimus caps subfunction is not implemented\n");
        return 0;
    }

    Printf(Tee::PriLow, "Optimus capabilities: 0x%x\n", caps);

    if (LWOP_FUNC_OPTIMUSCAPS_OPTIMUS_STATE_ENABLED
            != DRF_VAL(OP, _FUNC_OPTIMUSCAPS, _OPTIMUS_STATE, caps))
    {
        return 0;
    }

    return new LinuxOptimusMgr(domain, bus, device, function, caps);
#else
    return 0;
#endif
}

//------------------------------------------------------------------------------
// Set display to a specified VESA 'Mode'.
// If 'Windowed is true, use a windowed frame buffer model,
// otherwise linear/flat model.
// If 'ClearMemory' is true, clear the display memory.
// If an invalid mode is requested, the mode will not change,
// and an error code will be returned.
//
RC Xp::SetVesaMode
(
    UINT32 Mode,
    bool   Windowed,     // = true
    bool   ClearMemory   // = true
)
{
#if defined(INCLUDE_GPU) && defined(LWCPU_FAMILY_X86)
    if (Platform::IsUserInterfaceDisabled())
    {
        // UI disabled == RM owns the gpu right now.
        Printf(Tee::PriLow, "Skipping SetVesaMode, UserInterfaceDisabled.\n");
        return OK;
    }

    JavaScriptPtr pJs;
    UINT32 SkipVgaModeset = false;
    pJs->GetProperty(pJs->GetGlobalObject(), "g_SkipVgaModeset", &SkipVgaModeset);

    if (SkipVgaModeset)
    {
        Printf(s_ConsPri, "Skipping SetVesaMode, g_SkipVgaModeset=true.\n");
        return OK;
    }

    GpuSubdevice* const pSubdev = FindPrimarySubdev();
    if (!pSubdev)
    {
        return OK;
    }
    if (!s_MaybePrimaryVga)
    {
        return RC::OK;
    }
    if (!pSubdev->IsPrimary())
    {
        Printf(Tee::PriWarn, "Skipping SetVesaMode on %s which turned from primary to secondary, "
               "possibly due to GPU reset\n", pSubdev->GpuIdentStr().c_str());
        return RC::OK;
    }

    SuspendConsole(pSubdev);

    UINT32 eax = 0x4F02;
    UINT32 ebx = (Mode & 0x00003FFF)
            | (Windowed ? 0 : 1 << 14)
            | (ClearMemory ? 0 : 1 << 15);
    UINT32 ecx = 0, edx = 0;

    Printf(s_ConsPri, "Calling SetVesaMode mode=0x%x %s clear=%s on %s\n",
           Mode, Windowed ? "windowed" : "contiguous", ClearMemory ? "true" : "false",
           pSubdev->GpuIdentStr().c_str());

    RmCallVideoBIOS(pSubdev->GetGpuInst(), false, &eax, &ebx, &ecx, &edx, NULL);

    ResumeConsole(pSubdev);

    // Command completed successfully if AX=0x004F.
    if (0x004F != (eax & 0x0000FFFF))
    {
        Printf(Tee::PriError, "Unable to set VESA mode 0x%x, ax=0x%04x\n", Mode, eax&0xFFFF);
        return RC::SET_MODE_FAILED;
    }
    Printf(Tee::PriDebug, "Successfuly set VESA mode 0x%x\n", Mode);
#endif
    return OK;
}

#if defined(LWCPU_FAMILY_ARM)
RC Xp::FlushWriteCombineBuffer()
{
    // This ioctl will call kernel's wmb() macro which:
    // - uses dsb instruction to do a memory barrier and flush WC buffers
    // - calls outer_sync() to dump L2 to sysmem
    const int ret = s_KD.Ioctl(MODS_ESC_MEMORY_BARRIER, nullptr);
    return (0 == ret) ? OK : RC::UNSUPPORTED_FUNCTION;
}

RC Xp::FlushCpuCacheRange(void * StartAddress, void * EndAddress, CacheFlushFlags Flags)
{
    // This ioctl will flush a range of User Cache
    MODS_FLUSH_CPU_CACHE_RANGE range = {0};
    range.virt_addr_start = reinterpret_cast<uintptr_t>(StartAddress);
    range.virt_addr_end = reinterpret_cast<uintptr_t>(EndAddress);
    range.flags = (LwU32)Flags;
    const int ret = s_KD.Ioctl(MODS_ESC_FLUSH_CPU_CACHE_RANGE, &range);
    return (0 == ret) ? OK : RC::UNSUPPORTED_FUNCTION;
}
#endif

P_(Get_LinuxDisableWriteCombining);
P_(Set_LinuxDisableWriteCombining);
static SProperty s_LinuxDisableWriteCombining
(
    "DisableWriteCombining",
    0,
    0,
    Get_LinuxDisableWriteCombining,
    Set_LinuxDisableWriteCombining,
    0,
    "If true, use uncached in place of write-combined for device and system memory."
);
P_(Get_LinuxDisableWriteCombining)
{
    if (OK != JavaScriptPtr()->ToJsval(s_DisableWC, pValue))
    {
        JS_ReportError(pContext, "Failed to get DisableWriteCombining");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}
P_(Set_LinuxDisableWriteCombining)
{
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &s_DisableWC))
    {
        JS_ReportError(pContext, "Failed to set DisableWriteCombining");
        return JS_FALSE;
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
Xp::ClockManager* Xp::GetClockManager()
{
    Xp::ClockManager* pClkMgr = s_KD.GetClockManager();

    if (!pClkMgr)
    {
        pClkMgr = s_pPeaTransClkMgr;
    }

    if (!pClkMgr)
    {
        Printf(Tee::PriLow, "System-level clock setting is not supported on this platform\n");
    }
    return pClkMgr;
}

RC Xp::GetDriverStats(DriverStats *pDriverStats)
{
    if (pDriverStats == nullptr)
    {
        return RC::ILWALID_ARGUMENT;
    }

    memset(pDriverStats, 0, sizeof(DriverStats));

    MODS_GET_DRIVER_STATS data = {};

    if (s_KD.Ioctl(MODS_ESC_MODS_GET_DRIVER_STATS, &data))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    pDriverStats->allocationCount = data.num_allocs;
    pDriverStats->pagesCount = data.num_pages;

    return RC::OK;
}

#if defined(LWCPU_FAMILY_ARM)
//------------------------------------------------------------------------------
RC Xp::ReadSystemUUID(string *systemuuid)
{
    return RC::UNSUPPORTED_FUNCTION;
}
RC Xp::ReadHDiskSerialnum(string * strserialnum)
{
    return RC::UNSUPPORTED_FUNCTION;
}
#else
namespace
{
    //Assuming little endian targets
#define SHORT_UINT(x) (UINT16)(((x)[1] << 8) + (x)[0])
#define WORD_UINT(x) (UINT32)(((x)[3] << 24) + ((x)[2] << 16) + ((x)[1] << 8) + ((x)[0]))

    const UINT32 s_Smbiosbase = 0xF0000;
    const UINT32 s_SmbiosLen = 0x10000;
    const UINT32 s_LastSupportedVer = 0x207; //SMbios version
    const UINT32 s_SmbiosEndBoundary = 0xFFF0;
    const UINT32 s_Uuidstrsize = 38; //Big enough to accomodate UUID string

    //The following files require root access permissions to be opened in root mode
    const char s_DevMem[] = "/dev/mem";

    //For linux kernel version till 2.6.6: /proc/efi/systab
    //For kernel version 2.6.7 and up: /sys/firmware/efi/systab
    const char * s_EfiSystabFilePath[] = {"/sys/firmware/efi/systab", "/proc/efi/systab", NULL};

    enum EFISTATUS
    {
        s_NOEFI, s_EFINOSMBIOS, s_EFISMBIOS
    };

    EFISTATUS SmbiosOffsetforEFI(UINT32 *pBaseaddr)
    {
        const char ** pFilePath = s_EfiSystabFilePath;
        for (; (*pFilePath); pFilePath++)
        {
            if (Xp::DoesFileExist(*pFilePath))
                break;
        }
        FILE *pFile = NULL;
        if (!(*pFilePath) || (OK != Utility::OpenFile(*pFilePath, &pFile, "r")))
        {
            return s_NOEFI;
        }
        EFISTATUS status = s_EFINOSMBIOS;
        char linebuf[64] = {"\0"};
        while (fgets(linebuf, sizeof(linebuf) - 1, pFile))
        {
            //systab file has format of <FIELD>=<VALUE>
            //Find first line of form "SMBIOS=0xADDR\n"
            char *pSeparator = strchr(linebuf, '=');
            *(pSeparator++) = '\0';
            if (strcmp(linebuf, "SMBIOS") == 0)
            {
                *pBaseaddr = static_cast <UINT32> (Utility::Strtoull(pSeparator, nullptr, 0));
                Printf(Tee::PriLow,"SMBIOS entry point at 0x%08lx\n",
                        (unsigned long)*pBaseaddr);
                status = s_EFISMBIOS;
                break;
            }
        }
        if (status == s_EFINOSMBIOS)
        {
            Printf(Tee::PriLow," SMBIOS entry is missing from %s\n", *pFilePath);
        }
        fclose(pFile);
        return status;
    }

    bool CallwlateCheckSum(const UINT08 *buf, UINT32 len)
    {
        UINT08 sum = 0;
        for (UINT32 offset= 0; offset < len; offset++)
        {
            sum += buf[offset];
        }
        return (sum == 0);
    }

    class filedes
    {
        //Container for file descriptor
        public:
            filedes():m_Fd(-1){};
            filedes(int fd):m_Fd(fd){};
            ~filedes()
            {
                if (m_Fd >= 0)
                {
                    close(m_Fd);
                }
            }
            operator int() const {return m_Fd;};
            filedes & operator=(int fd){m_Fd = fd; return *this;};
            filedes & operator=(const filedes &fd)
            {
                if (this == &fd)
                    return *this;
                m_Fd = fd;
                return *this;
            };
        private:
            int m_Fd;
    };

    RC ReadFromDevMem(UINT32 base, UINT32 len, UINT08 **pDmem)
    {
        filedes fd;

        fd = open(s_DevMem, O_RDONLY);
        if (fd < 0)
        {
            Printf(Tee::PriLow, "Error in accessing /dev/mem file\n");
            return RC::CANNOT_OPEN_FILE;
        }
        *pDmem = (UINT08 *) malloc(len);
        if (!(*pDmem))
        {
            return RC::CANNOT_ALLOCATE_MEMORY;
        }

        UINT32 Offset = base & (Xp::GetPageSize() -1);
        UINT08 * buf = (UINT08 *)mmap(nullptr, Offset + len, PROT_READ, MAP_SHARED, fd,
                base - Offset);
        if (buf == MAP_FAILED)
        {
            free(*pDmem);
            Printf(Tee::PriHigh, "MMAP has failed for /dev/mem file\n");
            return RC::CANNOT_ALLOCATE_MEMORY;
        }
        else
        {
            memcpy(*pDmem, buf + Offset, len);
            munmap(buf, Offset + len);
            return OK;
        }
    }

    class DmiHeader
    {
        public:
            DmiHeader(UINT08 *pData)
                :m_Type(pData[0]),
                m_Length(pData[1]),
                m_Handle(SHORT_UINT(pData + 2)),
                m_pData(pData){};
            UINT08 GetLength(){return m_Length;};
            UINT08 GetType(){return m_Type;};
            UINT08 * GetData(){return m_pData;};
            UINT16 GetDmiHandle(){return m_Handle;};
        private:
            UINT08 m_Type;
            UINT08 m_Length;
            UINT16 m_Handle;
            UINT08 *m_pData;
    };

    class DmiTable
    {
        private:
            UINT32 m_Base;
            UINT16 m_Len;
            UINT16 m_Number;
            UINT16 m_Version;
        public:
            DmiTable(UINT32 base, UINT16 Len, UINT16 Number, UINT16 Version)
                : m_Base(base),
                m_Len(Len),
                m_Number(Number),
                m_Version(Version) {};
            void DumpDmiTable();
            RC GetSystemUUID(string *sysuuid);
    };

    RC DmiTable::GetSystemUUID(string *sysuuid)
    {
        UINT08 *buf;
        UINT08 *data;
        UINT32 i;
        RC rc;
        if (m_Version > s_LastSupportedVer)
        {
            Printf(Tee::PriLow, "Smbios version [0x%x] is not supported\n",
                    m_Version);
            return RC::SOFTWARE_ERROR;
        }
        CHECK_RC(ReadFromDevMem(m_Base, m_Len, &buf));
        data = buf;
        for (i = 0; (i < m_Number) && (data + 4 <= buf + m_Len); i++)
        {
            UINT08 *next;
            DmiHeader dmiheader(data);
            /*look for next handle*/
            next = data + dmiheader.GetLength();
            while ((next - buf + 1 < m_Len) &&
                    (next[0] || next[1]))
            {
                next++;
            }
            next += 2;

            if (next - buf <= m_Len)
            {
                const UINT08 *pHeaderData = dmiheader.GetData();
                //Type 1 is system information
                if (dmiheader.GetType() == 1)
                {
                    const UINT08 *uuiddata = pHeaderData + 0x08;
                    bool All_FF = true;
                    bool All_00 = true;
                    for(UINT32 i =0;i < 16 && (All_FF || All_00); i++)
                    {
                        if (uuiddata[i] !=0)
                            All_00 = false;
                        if (uuiddata[i] !=0xFF)
                            All_FF = false;
                    }
                    if (All_00 || All_FF)
                    {
                        Printf(Tee::PriLow, "Bad format in DMI\n");
                        free(buf);
                        return RC::SOFTWARE_ERROR;
                    }
                    else
                    {
                        char uuid_str[s_Uuidstrsize];
                        if (m_Version >= 0x206)
                        {
                            snprintf(uuid_str, s_Uuidstrsize,
                                    "%02X%02X%02X%02X-%02X%02X-"
                                    "%02X%02X-%02X%02X-"
                                    "%02X%02X%02X%02X%02X%02X",
                                    uuiddata[3],uuiddata[2],uuiddata[1],uuiddata[0],
                                    uuiddata[5],uuiddata[4],uuiddata[7],uuiddata[6],
                                    uuiddata[8],uuiddata[9],uuiddata[10],uuiddata[11],
                                    uuiddata[12],uuiddata[13],uuiddata[14],uuiddata[15]);
                        }
                        else
                        {
                            snprintf(uuid_str, s_Uuidstrsize,
                                    "%02X%02X%02X%02X-%02X%02X-"
                                    "%02X%02X-%02X%02X-"
                                    "%02X%02X%02X%02X%02X%02X",
                                    uuiddata[0],uuiddata[1],uuiddata[2],uuiddata[3],
                                    uuiddata[4],uuiddata[5],uuiddata[6],uuiddata[7],
                                    uuiddata[8],uuiddata[9],uuiddata[10],uuiddata[11],
                                    uuiddata[12],uuiddata[13],uuiddata[14],uuiddata[15]);
                        }
                        *sysuuid = uuid_str;
                    }
                    break; //for (i = 0; i < m_Number && data + 4 <= buf + m_Len; i++)
                } //if (dmiheader.GetType())
                data = next;
            }
        }
        free(buf);
        return rc;
    }

    class SmbiosData
    {
        private:
            UINT08 *m_pSmbiosBuf;
            UINT16 m_SmbiosVersion;
        public:
            SmbiosData(UINT08 *buf)
                :m_pSmbiosBuf(buf)
            {
                m_SmbiosVersion =  (buf[0x06] << 8) + buf[0x07];
                switch (m_SmbiosVersion)
                {
                    case 0x21F:
                    case 0x221:
                        m_SmbiosVersion = 0x203;
                        break;
                    case 0x233:
                        m_SmbiosVersion = 0x206;
                        break;
                }
            };
            UINT16 GetVersion() { return m_SmbiosVersion;};
            bool ValidateSmbiosData();
    };
    bool SmbiosData :: ValidateSmbiosData()
    {
        if (
                !CallwlateCheckSum(m_pSmbiosBuf, m_pSmbiosBuf[0x5])
                || (memcmp(m_pSmbiosBuf + 0x10, "_DMI_", 5) !=0)
                || !CallwlateCheckSum(m_pSmbiosBuf + 0x10, 0x0f)
            )
            return false;
        else
            return true;
    }
}

//------------------------------------------------------------------------------
RC Xp::ReadSystemUUID(string *SystemUUID)
{
    RC rc;

    UINT08 *pDevMem;
    UINT08 *pSmbiosbase = nullptr;
    UINT32 Smbiosbaseaddr;
    UINT16 SmbiosVersion = s_LastSupportedVer;

    if (SmbiosOffsetforEFI(&Smbiosbaseaddr) == s_EFISMBIOS)
    {
        CHECK_RC(ReadFromDevMem(Smbiosbaseaddr, 0x20, &pDevMem));
        pSmbiosbase = pDevMem;
    }
    else
    {
        CHECK_RC(ReadFromDevMem(s_Smbiosbase, s_SmbiosLen, &pDevMem));
        for (UINT32 fp = 0; fp <= s_SmbiosEndBoundary; fp +=16)
        {
            //Check the Anchor String
            if (memcmp(pDevMem + fp, "_SM_", 4) == 0)
            {
                pSmbiosbase = pDevMem + fp;
                break;
            }
        }
    }

    if (!pSmbiosbase)
    {
        Printf(Tee::PriLow,"Could not locate SMbios\n");
        rc = RC::SOFTWARE_ERROR;
    }
    else
    {

        SmbiosData Smbios(pSmbiosbase);
        if (false == Smbios.ValidateSmbiosData())
        {
            return RC::SOFTWARE_ERROR;
        }
        SmbiosVersion = Smbios.GetVersion();
        // Check the Intermediate Anchor string(DMI offset).
        // Following magic numbers are from:
        //http://dmtf.org/sites/default/files/standards/dolwments/DSP0134_2.7.1.pdf
        DmiTable DmiData(WORD_UINT(pSmbiosbase + 0x18), //DMI table BaseAddr
                SHORT_UINT(pSmbiosbase + 0x16), // DMI table length
                SHORT_UINT(pSmbiosbase + 0x1c), //Number of SMbios structures
                SmbiosVersion);
        rc = DmiData.GetSystemUUID(SystemUUID);
    }
    free(pDevMem);
    return rc;
}

//------------------------------------------------------------------------------
C_(Global_ReadSysUUID)
{
    MASSERT((pContext !=0) || (pArguments !=0));

    const char usage[] = "Usage: Platform.ReadSysUUID(ReturnUUIDstring)";
    JavaScriptPtr pJs;
    JSObject * pReturnArray = 0;
    if ((NumArguments != 1) ||
       (OK != pJs->FromJsval(pArguments[0], &pReturnArray)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    string SystemUUID;
    if (OK != (Xp::ReadSystemUUID(&SystemUUID)))
    {
        Printf(Tee::PriLow, "Failed to Read system UUID, "
                "this requires root permissions to run\n");
        RETURN_RC(pJs->SetElement(pReturnArray, 0,""));
    }
    RETURN_RC(pJs->SetElement(pReturnArray, 0, SystemUUID));
}

static STest Global_ReadSysUUID
(
    "ReadSysUUID",
    C_Global_ReadSysUUID,
    1,
    "Read the motherboard uuid."
);

//------------------------------------------------------------------------------
namespace
{
    //HDD info for linux
    //--------------------------------------------------------------------------
    //! \brief Scan one line proc/mounts file. The file format is fixed.
    //! @param fp - Input file pointer.
    //! @param dev - Output string of mount device.
    //! @param mountpt -  Output string of MountDev.
    //! @param fstype - Output string of filesystem type.
    RC ScanProcMountLine(FILE *fp, string &dev, string &mountpt, string &fstype)
    {
        const unsigned maxPath = 4096;
        char device[maxPath];
        char mpt[maxPath];
        char ftype[maxPath];

        if (nullptr == fp)
            return RC::FILE_DOES_NOT_EXIST;
        if (3 != fscanf(fp, "%s %s %s %*s %*d %*d\n", device, mpt, ftype))
            return RC::FILE_READ_ERROR;

        dev = device;
        mountpt = mpt;
        fstype = ftype;
        return OK;
    }

    //List of filesystems that are not hdisk
    const string s_NonHddFstypes[] = {
        "rootfs", "fuse.sshfs", "tmpfs", "sysfs", "usbfs", "debugfs", "proc",
        "configfs", "usbfs", "nfs", "devpts", "cgroup", "devtmpfs"};
    const UINT32 s_knownFstypes = 13;//Update this when you update the list above

    //--------------------------------------------------------------------------
    //! \brief Find the linux hdd device where the current working directory is
    //!        located.
    //! @param MountedDev - Output String of Mounted hdd device. Empty MountDev
    //!        represents failure
    void FindMountDevice(string &MountedDev)
    {
        FILE *fp;
        RC rc;
        string op, dev, mpt, filesys;

        MountedDev.clear();
        if (nullptr == (fp = fopen("/proc/mounts", "r" )))
        {
            return;
        }
        //Get the current working directory
        const unsigned maxPath = 4096;
        char buf[maxPath];
        if (!getcwd(buf, maxPath))
        {
            fclose(fp);
            return;
        }
        string LwrWorkDir(buf);

        // Mountpoint = linux directory path where the device is mounted.
        // pair of mounted device and length of mountpoints
        // this will be used ahead to determine the best match
        map <int,string> mountpoints;

        while(!feof(fp))
        {
            rc = ScanProcMountLine(fp, dev, mpt, filesys);
            if(rc != OK)
            {
                fclose(fp);
                return;
            }
            if ((s_NonHddFstypes + s_knownFstypes) !=
                    find(s_NonHddFstypes, s_NonHddFstypes + s_knownFstypes, filesys))
            {
                //Skip known non hdd based filesystems
                continue;
            }

            //_lwrrent_workdir_ = _mountpointdir_/relativepath.
            //Discover mountpoints that match the above structure
            if (mpt.size() <= LwrWorkDir.size())
            {
                int Isdiff = mpt.size() -1;
                for (; (Isdiff>=0) && (mpt[Isdiff] == LwrWorkDir[Isdiff]); Isdiff--);
                if (Isdiff == -1)
                {
                    //Possible match of mountpoint.
                    mountpoints.insert(pair<int, string>(mpt.size(), dev));
                }
            }
        }
        fclose(fp);

        if (mountpoints.empty())
        {
            //No valid mountpoints are found
            return;
        }

        //Select greatest element of the map as MountedDev
        MountedDev = mountpoints.rbegin()->second;
        if (MountedDev.compare(0, 5, "/dev/") !=0)
        {
            MountedDev.clear();
            Printf(Tee::PriLow,"Most probably you are not using hdisk media\n");
            return;
        }
    }
};

//-----------------------------------------------------------------------------
//! \brief Find the Hddiskserial number.
//! @param strserialnum - Pointer to output string which will return hddisk sno
RC Xp::ReadHDiskSerialnum(string * strserialnum)
{
    RC rc;
    string MountDev;

    FindMountDevice(MountDev);
    if (!(MountDev.length()))
    {
        return RC::SOFTWARE_ERROR;
    }

    INT32 HdFd = open(MountDev.c_str(), O_RDONLY);
    if (HdFd == -1)
    {
        Printf(Tee::PriLow, "Error: Unable to open MountDev: %s.\n", MountDev.c_str());
        return RC::CANNOT_OPEN_FILE;
    }

    struct hd_driveid HdIde;
    if (!ioctl(HdFd, HDIO_GET_IDENTITY, &HdIde))
    {
        UINT08 *pLtrim;
        for(pLtrim = HdIde.serial_no; (*pLtrim) == ' '; pLtrim++);
        strserialnum->assign((char *)pLtrim);
        close(HdFd);
        return OK;
    }
    else
    {
        close(HdFd);
        return RC::UNSUPPORTED_FUNCTION;
    }
}

//------------------------------------------------------------------------------
C_(Global_ReadHDiskSerno)
{
    MASSERT((pContext !=0) || (pArguments !=0));
    const char usage[] = "Usage: Platform.ReadHDiskSerno(ReturnSernostr)";
    JavaScriptPtr pJs;
    JSObject *pReturnArray = 0;
    if ((NumArguments != 1)||
            (OK != pJs->FromJsval(pArguments[0], &pReturnArray)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if (Platform::IsTegra())
    {
        Printf(Tee::PriLow, "Reading hddisk is not supported on mobile platforms\n");
        return JS_FALSE;
    }
    string HDdiskSerno;
    if (OK != (Xp::ReadHDiskSerialnum(&HDdiskSerno)))
    {
        Printf(Tee::PriLow, "Failed to Read Hdisk Serial num, "
                "this requires root permissions to run\n");
        RETURN_RC(pJs->SetElement(pReturnArray, 0,""));
    }
    RETURN_RC(pJs->SetElement(pReturnArray, 0, HDdiskSerno));
}

static STest Global_ReadHDiskSerno
(
    "ReadHDiskSerno",
    C_Global_ReadHDiskSerno,
    1,
    "Read the hdisk serial number."
);
#endif //if defined(LWCPU_FAMILY_ARM)

//------------------------------------------------------------------------------
//! \brief Get the CPU mask of CPUs that are local to the specified PCI device
//!        for NUMA enabled systems
//!
//! \param DomainNumber   : PCI domain number of the device.
//! \param BusNumber      : PCI bus number of the device.
//! \param DeviceNumber   : PCI device number of the device
//! \param FunctionNumber : PCI function number of the device
//! \param pLocalCpuMasks : If running on a correctly configured NUMA system
//!                         this will be an array of masks indicating which CPUs
//!                         are on the same NUMA node as the specified device.
//!                         Index 0 in the masks vector contains the LSBs of all
//!                         the masks.  To compute CPU number from mask use
//!                             CPU = (index * 32) + bit
//!                         So bit 27 in index 3 would correspond to
//!                             CPU = (3 * 32) + 27 = 123
//!                         If the vector is empty then MODS was not able to
//!                         determine which CPUs are local to the device
//!
//! \return OK if retrieving CPUs on specified device, not OK otherwise
RC Xp::GetDeviceLocalCpus
(
    INT32 DomainNumber,
    INT32 BusNumber,
    INT32 DeviceNumber,
    INT32 FunctionNumber,
    vector<UINT32> *pLocalCpuMasks
)
{
    return s_KD.DeviceNumaInfo(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, pLocalCpuMasks);
}

INT32 Xp::GetDeviceNumaNode
(
    INT32 domainNumber,
    INT32 busNumber,
    INT32 deviceNumber,
    INT32 functionNumber
)
{
    return s_KD.GetNumaNode(domainNumber, busNumber, deviceNumber, functionNumber);
}

#if defined(INCLUDE_GPU) && defined(LWCPU_FAMILY_X86)
RC Xp::GetFbConsoleInfo(PHYSADDR *pBaseAddress, UINT64 *pSize)
{
    *pBaseAddress = s_ScreenInfoBase;
    *pSize        = s_ScreenInfo.info.lfb_size;

    return RC::OK;
}

void Xp::SuspendConsole(GpuSubdevice* pSubdev)
{
    if (IsBootDevice(pSubdev))
    {
        if (s_ConsoleSuspendCount++ == 0)
        {
            if (s_KD.Ioctl(MODS_ESC_SUSPEND_CONSOLE, NULL))
            {
                Printf(Tee::PriWarn, "Failed to suspend Linux console\n");
                s_ConsoleSuspendCount.store(0);
            }
            else
            {
                Printf(s_ConsPri, "Suspended Linux console\n");
            }
        }
    }
}

void Xp::ResumeConsole(GpuSubdevice* pSubdev)
{
    if (s_ConsoleSuspendCount.load() &&
        IsBootDevice(pSubdev) &&
        (--s_ConsoleSuspendCount == 0))
    {
        if (!s_KD.Ioctl(MODS_ESC_RESUME_CONSOLE, NULL))
        {
            Printf(s_ConsPri, "Resumed Linux console\n");
        }
    }
}
#else
RC Xp::GetFbConsoleInfo(PHYSADDR *pBaseAddress, UINT64 *pSize)
{
    return RC::UNSUPPORTED_FUNCTION;
}

void Xp::SuspendConsole(GpuSubdevice* pSubdev)
{
}

void Xp::ResumeConsole(GpuSubdevice* pSubdev)
{
}
#endif

//------------------------------------------------------------------------------
// Returns the type of operating system.
//
Xp::OperatingSystem Xp::GetOperatingSystem()
{
    return OS_LINUX;
}

/**
 *------------------------------------------------------------------------------
 * @function(Xp::ValidateGpuInterrupt)
 *
 * Confirm that the GPU's interrupt mechanism is working.
 *------------------------------------------------------------------------------
 */

RC Xp::ValidateGpuInterrupt(GpuSubdevice  *pGpuSubdevice,
                            UINT32 whichInterrupts)
{
#if defined(INCLUDE_GPU)
    UINT32 intrToCheck = 0;
    if (whichInterrupts & IntaIntrCheck)
    {
        intrToCheck |= GpuSubdevice::HOOKED_INTA;
    }
    if (whichInterrupts & MsiIntrCheck)
    {
        intrToCheck |= GpuSubdevice::HOOKED_MSI;
    }
    if (whichInterrupts & MsixIntrCheck)
    {
        intrToCheck |= GpuSubdevice::HOOKED_MSIX;
    }
    RC rc = pGpuSubdevice->ValidateInterrupts(intrToCheck);
    Tee::FlushToDisk();
    CHECK_RC(rc);
#endif
    return OK;
}

//------------------------------------------------------------------------------
RC Xp::GetNTPTime
(
   UINT32 &secs,
   UINT32 &msecs,
   const char *hostname
)
{
    return OK;
}

//------------------------------------------------------------------------------
void* Xp::AllocOsEvent(UINT32 hClient, UINT32 hDevice)
{
    return nullptr;
}

//------------------------------------------------------------------------------
void Xp::FreeOsEvent(void* pFd, UINT32 hClient, UINT32 hDevice)
{
}

//------------------------------------------------------------------------------
// Determine boot VESA mode
#if defined(INCLUDE_GPU) && defined(LWCPU_FAMILY_X86)
namespace
{
    bool   s_VesaModeRead     = false;
    UINT32 s_VesaMode         = 3;    // By default use standard text mode 80x25
    bool   s_VesaModeWindowed = true; // By default use windowed framebuffer model

    void GetVesaMode(GpuSubdevice* pSubdev)
    {
        MASSERT(s_MaybePrimaryVga);

        if (s_VesaModeRead)
        {
            return;
        }

        // Do this only once
        s_VesaModeRead = true;

        // If we read this from the Linux kernel, use this value
        if (s_ScreenInfo.info.orig_video_is_vga == VIDEO_TYPE_RAW_VGA)
        {
            s_VesaMode         = s_ScreenInfo.info.orig_video_mode;
            s_VesaModeWindowed = false;
            return;
        }

        JavaScriptPtr pJs;
        UINT32 skipVgaModeset = false;
        pJs->GetProperty(pJs->GetGlobalObject(), "g_SkipVgaModeset", &skipVgaModeset);

        if (skipVgaModeset)
        {
            Printf(s_ConsPri, "Skipping GetVesaMode, g_SkipVgaModeset=true.\n");
            return;
        }

        Xp::SuspendConsole(pSubdev);

        Printf(s_ConsPri, "Calling GetVesaMode on %s\n", pSubdev->GpuIdentStr().c_str());

        UINT32 eax = 0x4F03;
        UINT32 ebx = 0, ecx = 0, edx = 0;

        RmCallVideoBIOS(pSubdev->GetGpuInst(), false, &eax, &ebx, &ecx, &edx, NULL);

        Xp::ResumeConsole(pSubdev);

        // Command completed successfully if AX=0x004F.
        if (0x004F != (eax & 0x0000FFFF))
        {
            if ((eax & 0xFFU) == 0x4FU)
            {
                Printf(Tee::PriError, "Get VESA mode returned error 0x%02x\n", (eax >> 8) & 0xFFU);
            }
            else
            {
                Printf(Tee::PriError, "VESA calls not supported, get mode returned 0x%04x\n",
                       eax & 0xFFFFU);
            }
            s_MaybePrimaryVga = false;
            return;
        }

        s_VesaMode         = ebx & 0x3FFF;
        s_VesaModeWindowed = (ebx & 0x4000) == 0;

        Printf(s_ConsPri, "Read old VESA mode 0x%x %s\n", s_VesaMode,
               s_VesaModeWindowed ? "windowed" : "contiguous");
    }
}
#endif

//------------------------------------------------------------------------------
RC Xp::WaitOsEvents
(
   void**  pOsEvents,
   UINT32  numOsEvents,
   UINT32* pCompletedIndices,
   UINT32  maxCompleted,
   UINT32* pNumCompleted,
   FLOAT64 timeoutMs
)
{
    return RC::TIMEOUT_ERROR;
}

//------------------------------------------------------------------------------
void Xp::SetOsEvent(void* pFd)
{
}

//------------------------------------------------------------------------------
// Disable the user interface, i.e. disable VGA.
//
RC Xp::DisableUserInterface()
{
    RC rc = OK;

#if defined(INCLUDE_GPU) && defined(LWCPU_FAMILY_X86)
    GpuSubdevice* const pSubdev = FindPrimarySubdev();
    if (!pSubdev)
    {
        Printf(s_ConsPri, "Primary GPU not detected, skipping DisableUserInterface.\n");
        return OK;
    }

    SuspendConsole(pSubdev);

    // Save original VESA mode
    if (s_MaybePrimaryVga)
    {
        GetVesaMode(pSubdev);
    }

    if (DevMgrMgr::d_GraphDevMgr)
    {
        // Sim mods, i.e. client-side resman.
        // We can call right into RmDisableVga (rmdos.c) to enable RM's ISR and
        // take ownership of the gpus.
        // Note that we start up in !vga mode in linux-sim anyway, so this is
        // is probably harmless but unnecessary.

        if (Platform::GetSimulationMode() == Platform::Hardware)
        {
            RmDisableVga(pSubdev->GetParentDevice()->GetDeviceInst());
        }
    }
#endif // INCLUDE_GPU

    return rc;
}

//------------------------------------------------------------------------------
// Enable the user interface.
//
RC Xp::EnableUserInterface()
{
    RC rc = OK;

#if defined(INCLUDE_GPU) && defined(LWCPU_FAMILY_X86)
    // We do not touch user interface on devices we did not enumerate
    if (!DevMgrMgr::d_GraphDevMgr)
    {
        Printf(s_ConsPri, "GPU infrastructure not initialized, skipping EnableUserInterface.\n");
        return RC::OK;
    }

    GpuSubdevice* const pSubdev = FindPrimarySubdev();

    JavaScriptPtr pJs;
    UINT32 SkipVgaModeset = false;
    pJs->GetProperty(pJs->GetGlobalObject(), "g_SkipVgaModeset", &SkipVgaModeset);

    if (SkipVgaModeset)
    {
        Printf(s_ConsPri, "Skipping SetVesaMode in EnableUserInterface, g_SkipVgaModeset=true.\n");
        ResumeConsole(pSubdev);
        return RC::OK;
    }

    // Check if user needs MODS to restore the default display that was running
    // *before* MODS started, There are multi-display scenarios where this
    // needs to be disabled (see bug 1424056)
    bool RestoreDefaultDisplay = true;
    pJs->GetProperty(pJs->GetGlobalObject(),
                     "g_RestoreDefaultDisplayOnExit", &RestoreDefaultDisplay);

    if (pSubdev)
    {
        Display* const pDisplay = pSubdev->GetDisplay();

        if (s_ScreenInfo.info.orig_video_is_vga == VIDEO_TYPE_EFI)
        {
            RC dispRc;
            if (pDisplay && !pDisplay->IsBootDisplayRestored())
            {
                const bool origDisableUserInterface = Platform::GetSkipDisableUserInterface();
                Platform::SetSkipDisableUserInterface(true);

                const ColorUtils::Format colorFormat =
                        ColorUtils::ColorDepthToFormat(s_ScreenInfo.info.lfb_depth);
                dispRc = pDisplay->DoRestoreModeset(s_ScreenInfo.info.lfb_width,
                                                    s_ScreenInfo.info.lfb_height,
                                                    colorFormat,
                                                    s_ScreenInfo.info.lfb_linelength);

                Platform::SetSkipDisableUserInterface(origDisableUserInterface);

                // Free our persistent GPU display resources.
                pDisplay->FreeGpuResources(pSubdev->GetParentDevice()->GetDeviceInst());
            }

            // Restore VBIOS ownership in CSM (compatibility mode)
            RmEnableVga(pSubdev->GetParentDevice()->GetDeviceInst(),
                        RestoreDefaultDisplay);

            if (dispRc != RC::OK)
            {
                Printf(Tee::PriWarn, "Unable to restore video mode for EFI console.  "
                       "System booted in EFI mode but %s seems to be corrupted, "
                       "possibly due to GPU reset.\n",
                       pSubdev->GpuIdentStr().c_str());
            }
        }
        else if (pSubdev->IsPrimary())
        {
            // Free our persistent GPU display resources.
            if (pDisplay)
            {
                pDisplay->FreeGpuResources(pSubdev->GetParentDevice()->GetDeviceInst());
            }

            // This is required to avoid VGA bundle timeout error we see in Volta.
            // It was marked invalid during initialization of display channels
            // in LwDisplay::InitializeDisplayHW
            if (pSubdev->HasBug(1923924))
            {
                pSubdev->Regs().Write32(MODS_PDISP_VGA_BASE_STATUS_VALID);
            }

            // This is the VGA boot device, turn it over to vbios.
            RmEnableVga(pSubdev->GetParentDevice()->GetDeviceInst(),
                        RestoreDefaultDisplay);

            // Call VBIOS to set original VESA mode.
            rc = SetVesaMode(s_VesaMode, s_VesaModeWindowed);
        }
        else
        {
            Printf(Tee::PriWarn, "Unable to restore video mode for VGA console.  "
                   "System booted in VGA mode but %s seems to be corrupted, "
                   "possibly due to GPU reset.\n",
                   pSubdev->GpuIdentStr().c_str());
        }
    }

    ResumeConsole(pSubdev);
#endif

    return rc;
}

RC Xp::SetupDmaBase
(
    UINT16           domain,
    UINT08           bus,
    UINT08           dev,
    UINT08           func,
    PpcTceBypassMode bypassMode,
    UINT64           devDmaMask,
    PHYSADDR *       pDmaBaseAddr
)
{
    UINT32 kdBypassMode;
    switch (bypassMode)
    {
        case Xp::PPC_TCE_BYPASS_DEFAULT:
            kdBypassMode = MODS_PPC_TCE_BYPASS_DEFAULT;
            break;
        case Xp::PPC_TCE_BYPASS_FORCE_ON:
            kdBypassMode = MODS_PPC_TCE_BYPASS_ON;
            break;
        case Xp::PPC_TCE_BYPASS_FORCE_OFF:
            kdBypassMode = MODS_PPC_TCE_BYPASS_OFF;
            break;
        default:
            Printf(Tee::PriHigh, "Error: Unknown bypass mode %d\n", bypassMode);
            return RC::SOFTWARE_ERROR;
    }
    return s_KD.SetupDmaBase(domain, bus, dev, func, kdBypassMode, devDmaMask, pDmaBaseAddr);
}

RC Xp::SetLwLinkSysmemTrained(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, bool trained)
{
    return s_KD.SetLwLinkSysmemTrained(domain, bus, device, func, trained);
}

#ifdef LINUX_MFG
//! PPC-specific function to get the lwlink speed supported by NPU from device-tree
RC Xp::GetLwLinkLineRate(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, UINT32 *pSpeed)
{
    return s_KD.GetLwLinkLineRate(domain, bus, device, func, pSpeed);
}
#endif

//------------------------------------------------------------------------------
bool Xp::HasFeature(Feature feat)
{
    switch (feat)
    {
        case HAS_KERNEL_LEVEL_ACCESS:
            return s_KD.IsOpen();
        case HAS_UVA_SUPPORT:
            return true;
        default:
            MASSERT(!"Not implemented");
            return false;
    }
}

Xp::MODSKernelDriver *Xp::GetMODSKernelDriver()
{
    return &s_KD;
}

//------------------------------------------------------------------------------
#ifdef LINUX_MFG
RC Xp::GetAtsTargetAddressRange
(
    UINT32  gpuInst,
    UINT32* pAddr,
    UINT32* pGuestAddr,
    UINT32* pAddrWidth,
    UINT32* pMask,
    UINT32* pMaskWidth,
    UINT32* pGranularity,
    bool    bIsPeer,
    UINT32  peerIndex
)
{
#if defined(INCLUDE_GPU) && defined(PPC64LE)
    // Implementation for IBM P9.
    // The contents of this function are mostly copied from Linux driver's
    // lw.c and os.c.

    const UINT32 guestAddrWidth       = 56;
    const UINT32 guestAddrGranularity = 37;
    const UINT32 compressedSPAWidth   = 47;

    Printf(Tee::PriDebug, "GetAtsTargetAddressRange gpuInst=%u bIsPeer=%s peerIndex=%u\n",
           gpuInst, bIsPeer ? "true" : "false", peerIndex);

    if (bIsPeer)
    {
        *pAddr        = 0U;
        *pGuestAddr   = 0U;
        *pAddrWidth   = 10U;
        *pMask        = 0U;
        *pMaskWidth   = 0x10U;
        *pGranularity = guestAddrGranularity;
        return OK;
    }

    RC rc;
    UINT64 physAddr;
    UINT64 guestAddr;
    UINT64 apertureSize;
    INT32  numaNode;
    CHECK_RC(s_KD.GetAtsTargetAddressRange(gpuInst,
                                           0, // NPU index for this GPU
                                           &physAddr,
                                           &guestAddr,
                                           &apertureSize,
                                           &numaNode));

    const UINT64 sysPaMask = (1ULL << compressedSPAWidth) - 1ULL;
    const UINT64 guestMask = (1ULL << guestAddrWidth    ) - 1ULL;

    *pAddr        = (physAddr  & sysPaMask) >> guestAddrGranularity;
    *pGuestAddr   = (guestAddr & guestMask) >> guestAddrGranularity;
    *pAddrWidth   = compressedSPAWidth - guestAddrGranularity;
    *pMask        = 0x3FFU;
    *pMaskWidth   = 0x10U;
    *pGranularity = guestAddrGranularity;

    Printf(Tee::PriDebug, "GetAtsTargetAddressRange phys=0x%x (0x%llx) guest=0x%x (0x%llx)\n",
           *pAddr, physAddr, *pGuestAddr, guestAddr);

    return OK;
#elif defined(INCLUDE_GPU) && defined(LWCPU_AARCH64)
    // Implementation for T194.

    Printf(Tee::PriDebug, "GetAtsTargetAddressRange gpuInst=%u bIsPeer=%s peerIndex=%u\n",
           gpuInst, bIsPeer ? "true" : "false", peerIndex);

    const UINT32 guestAddrWidth       = 56;
    const UINT32 guestAddrGranularity = 37;
    const UINT32 compressedSPAWidth   = 47;

    const UINT64 physAddr     = 1ULL << 37;
    const UINT64 guestAddr    = 1ULL << 37;

    const UINT64 sysPaMask = (1ULL << compressedSPAWidth) - 1ULL;
    const UINT64 guestMask = (1ULL << guestAddrWidth    ) - 1ULL;

    *pAddr        = (physAddr  & sysPaMask) >> guestAddrGranularity;
    *pGuestAddr   = (guestAddr & guestMask) >> guestAddrGranularity;
    *pAddrWidth   = compressedSPAWidth - guestAddrGranularity;
    *pMask        = 0x3FFU;
    *pMaskWidth   = 0x10U;
    *pGranularity = guestAddrGranularity;

    Printf(Tee::PriDebug, "GetAtsTargetAddressRange phys=0x%x (0x%llx) guest=0x%x (0x%llx)\n",
           *pAddr, physAddr, *pGuestAddr, guestAddr);

    return OK;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}
#endif

//------------------------------------------------------------------------------
RC Xp::RdMsrOnCpu(UINT32 cpu, UINT32 reg, UINT32 *pHigh, UINT32 *pLow)
{
    return s_KD.RdMsrOnCpu(cpu, reg, pHigh, pLow);
}

RC Xp::WrMsrOnCpu(UINT32 cpu, UINT32 reg, UINT32 high, UINT32 low)
{
    return s_KD.WrMsrOnCpu(cpu, reg, high, low);
}

RC Xp::SystemDropCaches()
{
    sync();

    RC rc = s_KD.SysctlWrite("vm/drop_caches", 3);

    if (rc == RC::UNSUPPORTED_SYSTEM_CONFIG)
    {
        rc.Clear();
        rc = InteractiveFileWrite("/proc/sys/vm/drop_caches", 3);
    }

    return rc;
}

RC Xp::SystemCompactMemory()
{
    RC rc = s_KD.SysctlWrite("vm/compact_memory", 1);

    if (rc == RC::UNSUPPORTED_SYSTEM_CONFIG)
    {
        rc.Clear();
        rc = InteractiveFileWrite("/proc/sys/vm/compact_memory", 1);
    }

    return rc;
}
