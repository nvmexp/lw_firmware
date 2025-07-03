/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "xp_mods_kd.h"

#include "core/include/cpu.h"
#include "core/include/massert.h"
#include "core/include/pool.h"
#include "core/include/utility.h"
#include "core/include/xp.h"

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#if defined(INCLUDE_GPU)
    #include "core/include/mgrmgr.h"
    #include "device/interface/pcie.h"
    #include "gpu/include/gpumgr.h"
#endif

#if defined(TEGRA_MODS)
    #include "cheetah/include/tegirq.h"
#endif

namespace
{
    constexpr UINT32 MIN_MODS_DRIVER_VERSION             = 0x387;
    constexpr UINT32 MSR_SUPPORT_VERSION                 = 0x388;
    constexpr UINT32 NO_PCI_DEV_FFFF_VERSION             = 0x389;
    constexpr UINT32 ACPI_ID_API_SUPPORT_VERSION         = 0x391;
    constexpr UINT32 PCI_REMOVE_SUPPORT_VERSION          = 0x392;
    constexpr UINT32 BETTER_ALLOC_SUPPORT_VERSION        = 0x393;
    constexpr UINT32 MERGE_PAGES_SUPPORT_VERSION         = 0x394;
    constexpr UINT32 PROC_SYS_SUPPORT_VERSION            = 0x398;
    constexpr UINT32 PCI_RESET_FUNCTION_SUPPORT_VERSION  = 0x400;
    constexpr UINT32 ACPI_DEV_CHILDREN_VERSION           = 0x406;

    const char s_DevMods[] = "/dev/mods";

#if defined(TEGRA_MODS)
    class LinuxClockManager: public Xp::ClockManager
    {
        public:
            explicit LinuxClockManager(Xp::MODSKernelDriver& kd): m_KD(kd) { }
            virtual ~LinuxClockManager()
            {
            }
#ifdef MODS_ESC_GET_RESET_HANDLE
            virtual RC GetResetHandle
            (
                const char* resetName,
                UINT64* pHandle
            )
            {
                MODS_GET_RESET_HANDLE param = {0};
                memcpy(param.reset_name, resetName, min(strlen(resetName), sizeof(param.reset_name)));
                if (m_KD.Ioctl(MODS_ESC_GET_RESET_HANDLE, &param))
                {
                    return RC::SOFTWARE_ERROR;
                }
                *pHandle = param.reset_handle;
                return OK;
            }
#else
            virtual RC GetResetHandle
            (
                const char* resetName,
                UINT64* pHandle
            )
            {
                return OK;
            }
#endif
            virtual RC GetClockHandle
            (
                const char* device,
                const char* controller,
                UINT64* pHandle
            )
            {
                MODS_GET_CLOCK_HANDLE param = {0};
                memcpy(param.device_name, device, min(strlen(device), sizeof(param.device_name)));
                memcpy(param.controller_name, controller, min(strlen(controller), sizeof(param.controller_name)));
                if (m_KD.Ioctl(MODS_ESC_GET_CLOCK_HANDLE, &param))
                {
                    return RC::SOFTWARE_ERROR;
                }
                *pHandle = param.clock_handle;
                return RC::OK;
            }
            virtual RC EnableClock
            (
                UINT64 clockHandle
            )
            {
                MODS_CLOCK_HANDLE param = {0};
                param.clock_handle = static_cast<UINT32>(clockHandle);
                if (m_KD.Ioctl(MODS_ESC_ENABLE_CLOCK, &param))
                {
                    return RC::SOFTWARE_ERROR;
                }
                return RC::OK;
            }
            virtual RC DisableClock
            (
                UINT64 clockHandle
            )
            {
                MODS_CLOCK_HANDLE param = {0};
                param.clock_handle = static_cast<UINT32>(clockHandle);
                if (m_KD.Ioctl(MODS_ESC_DISABLE_CLOCK, &param))
                {
                    return RC::SOFTWARE_ERROR;
                }
                return RC::OK;
            }
            virtual RC GetClockEnabled
            (
                UINT64 clockHandle,
                UINT32 *pEnableCount
            )
            {
                MODS_CLOCK_ENABLED param = {0};
                param.clock_handle = static_cast<UINT32>(clockHandle);
                if (m_KD.Ioctl(MODS_ESC_IS_CLOCK_ENABLED, &param))
                {
                    return RC::SOFTWARE_ERROR;
                }
                *pEnableCount = param.enable_count;
                return RC::OK;
            }
            virtual RC SetClockRate
            (
                UINT64 clockHandle,
                UINT64 rateHz
            )
            {
                MODS_CLOCK_RATE param = {0};
                param.clock_handle = static_cast<UINT32>(clockHandle);
                param.clock_rate_hz = rateHz;
                if (m_KD.Ioctl(MODS_ESC_SET_CLOCK_RATE, &param))
                {
                    return RC::SOFTWARE_ERROR;
                }
                return RC::OK;
            }
            virtual RC GetClockRate
            (
                UINT64 clockHandle,
                UINT64* pRateHz
            )
            {
                MODS_CLOCK_RATE param = {0};
                param.clock_handle = static_cast<UINT32>(clockHandle);
                if (m_KD.Ioctl(MODS_ESC_GET_CLOCK_RATE, &param))
                {
                    return RC::SOFTWARE_ERROR;
                }
                *pRateHz = param.clock_rate_hz;
                return RC::OK;
            }
            virtual RC SetMaxClockRate
            (
                UINT64 clockHandle,
                UINT64 rateHz
            )
            {
                MODS_CLOCK_RATE param = {0};
                param.clock_handle = static_cast<UINT32>(clockHandle);
                param.clock_rate_hz = rateHz;
                if (m_KD.Ioctl(MODS_ESC_SET_CLOCK_MAX_RATE, &param))
                {
                    return RC::SOFTWARE_ERROR;
                }
                return RC::OK;
            }
            virtual RC GetMaxClockRate
            (
                UINT64 clockHandle,
                UINT64* pRateHz
            )
            {
                MODS_CLOCK_RATE param = {0};
                param.clock_handle = static_cast<UINT32>(clockHandle);
                if (m_KD.Ioctl(MODS_ESC_GET_CLOCK_MAX_RATE, &param))
                {
                    return RC::SOFTWARE_ERROR;
                }
                *pRateHz = param.clock_rate_hz;
                return RC::OK;
            }
            virtual RC SetClockParent
            (
                UINT64 clockHandle,
                UINT64 parentClockHandle
            )
            {
                MODS_CLOCK_PARENT param = {0};
                param.clock_handle = static_cast<UINT32>(clockHandle);
                param.clock_parent_handle = static_cast<UINT32>(parentClockHandle);
                if (m_KD.Ioctl(MODS_ESC_SET_CLOCK_PARENT, &param))
                {
                    return RC::SOFTWARE_ERROR;
                }
                return RC::OK;
            }
            virtual RC GetClockParent
            (
                UINT64 clockHandle,
                UINT64* pParentClockHandle
            )
            {
                MODS_CLOCK_PARENT param = {0};
                param.clock_handle = static_cast<UINT32>(clockHandle);
                if (m_KD.Ioctl(MODS_ESC_GET_CLOCK_PARENT, &param))
                {
                    return RC::SOFTWARE_ERROR;
                }
                *pParentClockHandle = param.clock_parent_handle;
                return RC::OK;
            }
            virtual RC Reset
            (
                UINT64 handle,
                bool assert
            )
            {
#ifdef MODS_ESC_RESET_ASSERT
                MODS_RESET_HANDLE param = {0};
                param.handle = static_cast<UINT32>(handle);
                //Whether we are asserting or deasserting reset
                param.assert = assert;
                if (m_KD.Ioctl(MODS_ESC_RESET_ASSERT, &param))
                {
#else
                MODS_CLOCK_HANDLE param = {0};
                param.clock_handle = static_cast<UINT32>(handle);
                if (m_KD.Ioctl(
                            assert ? MODS_ESC_CLOCK_RESET_ASSERT
                                : MODS_ESC_CLOCK_RESET_DEASSERT,
                            &param))
                {
#endif
                    return RC::SOFTWARE_ERROR;
                }
                return RC::OK;
            }
        private:
            Xp::MODSKernelDriver& m_KD;
    };
#endif

    UINT64 GetDeviceIrqId(UINT32 pciDomain, UINT32 pciBus, UINT32 pciDevice, UINT32 pciFunction)
    {
        return ((static_cast<UINT64>(pciDomain   & 0xFFFF) << 48) |
                (static_cast<UINT64>(pciBus      & 0xFFFF) << 32) |
                (static_cast<UINT64>(pciDevice   & 0xFFFF) << 16) |
                 static_cast<UINT64>(pciFunction & 0xFFFF));
    }

    bool IsNumaSystem()
    {
        const char possibleNodesFn[] = "/sys/devices/system/node/possible";
        Xp::InteractiveFile possibleNodes;
        string posNodesStr;
        if (!Xp::DoesFileExist(possibleNodesFn) ||
            (OK != possibleNodes.Open(possibleNodesFn,
                                      Xp::InteractiveFile::ReadOnly)) ||
            (OK != possibleNodes.Read(&posNodesStr)) ||
            posNodesStr.empty())
        {
            return false;
        }

        // Format of the possible nodes is (for instance) "x-y,z,w,b-c".  This
        // would indicate that nodes x through y and b through c are present as
        // well as individual nodes z and w.  If there is a single node in the
        // system this file will be a single number (indicating the node that is
        // present).  If there are any ',' or '-' in the file then there must
        // be multiple nodes present
        return (posNodesStr.find(',') != string::npos) ||
               (posNodesStr.find('-') != string::npos);
    }
}

bool Xp::MODSKernelDriver::GpuIsSoc(UINT32 gpuInst)
{
#ifdef INCLUDE_GPU
    if (gpuInst == static_cast<UINT32>(Platform::UNSPECIFIED_GPUINST))
        return false;

    GpuDevMgr* const pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
    if (!pGpuDevMgr)
        return false;
    GpuSubdevice* const pSubdev = pGpuDevMgr->GetSubdeviceByGpuInst(gpuInst);
    if (!pSubdev)
        return false;

    return pSubdev->IsSOC();
#else
    return false;
#endif
}

RC Xp::MODSKernelDriver::GetGpuPciLocation
(
    UINT32  gpuInst,
    UINT32* pDomainNumber,
    UINT32* pBusNumber,
    UINT32* pDeviceNumber,
    UINT32* pFunctionNumber
)
{
#ifdef INCLUDE_GPU
    if (gpuInst == static_cast<UINT32>(Platform::UNSPECIFIED_GPUINST))
        return RC::UNSUPPORTED_DEVICE;

    GpuDevMgr* const pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
    if (!pGpuDevMgr)
        return RC::DEVICE_NOT_FOUND;
    GpuSubdevice* const pSubdev = pGpuDevMgr->GetSubdeviceByGpuInst(gpuInst);
    if (!pSubdev)
        return RC::DEVICE_NOT_FOUND;
    if (pSubdev->IsSOC())
        return RC::UNSUPPORTED_DEVICE;
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();
    if (!pGpuPcie)
        return RC::UNSUPPORTED_DEVICE;

    *pDomainNumber   = pGpuPcie->DomainNumber();
    *pBusNumber      = pGpuPcie->BusNumber();
    *pDeviceNumber   = pGpuPcie->DeviceNumber();
    *pFunctionNumber = pGpuPcie->FunctionNumber();
    return RC::OK;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

Xp::MODSKernelDriver::~MODSKernelDriver()
{
    Close();
}

RC Xp::MODSKernelDriver::Open(const char* exePath)
{
    MASSERT(!m_bOpen);
    RC rc;

    MASSERT(m_AddressMap.empty());
    if (!m_AddressMapMutex.get())
    {
        m_AddressMapMutex = Tasker::CreateMutex("AddressMapMutex", Tasker::mtxUnchecked);
    }

    // Check if the MODS device exists
    struct stat buf;
    if (0 != stat(s_DevMods, &buf))
    {
#if defined(TEGRA_MODS)
        Printf(Tee::PriLow, "%s not found\n", s_DevMods);

        m_DevMemFd = open("/dev/mem", O_RDWR | O_SYNC);
        if (m_DevMemFd == -1)
        {
            Printf(Tee::PriError, "Unable to open /dev/mem\n");
            return RC::CANNOT_OPEN_FILE;
        }
        m_bDevMemOpen = true;
        m_bSetMemTypeSupported = false;

        // Indicate to the caller that /dev/mods was not opened
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
#else
        // Driver is not present in the system, we have to run install script
        Printf(Tee::PriNormal, "Installing kernel module...\n");

        // MODSKernelDriver::Open() is called before CommandLine::Parse(),
        // therefore we are not able to use CommandLine::ProgramPath()
        // or Utility::DefaultFindFile() to find the install script, which
        // should be in the same directory as the MODS exelwtable.
        // We're passing argv[0] as an argument instead.
        MASSERT(exePath);
        string path(exePath);
        const size_t pos = path.rfind('/');
        if (pos == string::npos)
        {
            path = "./";
        }
        else
        {
            path.resize(pos+1);
        }

        const int status = system(("sh " + path + "install_module.sh --insert").c_str());
        if (status != 0)
        {
            Printf(Tee::PriError, "Installation script has failed with status %d\n", status);
            Printf(Tee::PriNormal, "\n"
                    "Please run the `install_module.sh --install' script as root\n"
                    "to install the MODS kernel module.\n\n");
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }
#endif
    }

    // Check access permissions
    if (0 != access(s_DevMods, R_OK | W_OK))
    {
        Printf(Tee::PriError, "Insufficient access rights for %s.\n",
               s_DevMods);
        return RC::FILE_PERM;
    }

    // Open the driver
    m_KrnFd = open(s_DevMods, O_RDWR);
    if (m_KrnFd == -1)
    {
        Printf(Tee::PriError, "Unable to open %s. Another instance of MODS may be running.\n",
               s_DevMods);
        return RC::FILE_BUSY;
    }

    MASSERT(!m_InterruptMutex.get());
    MASSERT(!m_IrqRegisterMutex.get());
    m_InterruptMutex   = Tasker::CreateMutex("InterruptMutex",   Tasker::mtxUnchecked);
    m_IrqRegisterMutex = Tasker::CreateMutex("IrqRegisterMutex", Tasker::mtxUnchecked);

    m_bOpen = true;

    // Retrieve kernel driver version
    {
        MODS_GET_VERSION modsGetVersion = {0};
        const int ret = ioctl(m_KrnFd, MODS_ESC_GET_API_VERSION, &modsGetVersion);
        if (ret)
        {
            Printf(Tee::PriError, "Can't get MODS kernel module API version\n");
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }
        m_Version = modsGetVersion.version;
    }
    // Fail if the driver is too old
    if (m_Version < MIN_MODS_DRIVER_VERSION)
    {
        Printf(Tee::PriError,
               "The installed MODS kernel driver version %x.%02x is not supported.\n"
               "Please upgrade the MODS kernel driver to version %x.%02x or newer.\n",
               m_Version >> 8, m_Version & 0xFF,
               MIN_MODS_DRIVER_VERSION >> 8, MIN_MODS_DRIVER_VERSION & 0xFF);
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }
    Printf(Tee::PriLow, "MODS kernel driver version is %x.%02x\n", m_Version>>8, m_Version&0xFF);

    m_MsrSupport       = (m_Version >= MSR_SUPPORT_VERSION);
    m_NoPciDevFFFF     = (m_Version >= NO_PCI_DEV_FFFF_VERSION);
    m_AcpiIdApiSupport = (m_Version >= ACPI_ID_API_SUPPORT_VERSION);
    m_BetterAlloc      = (m_Version >= BETTER_ALLOC_SUPPORT_VERSION);
    m_MergePages       = (m_Version >= MERGE_PAGES_SUPPORT_VERSION);
    m_ProcSysWrite     = (m_Version >= PROC_SYS_SUPPORT_VERSION);
    m_AcpiDevChildren  = (m_Version >= ACPI_DEV_CHILDREN_VERSION);

#ifdef LWCPU_FAMILY_X86
    char foundry[13] = { };
    Cpu::CPUID(0, 0, nullptr,
               reinterpret_cast<UINT32*>(&foundry[0]),
               reinterpret_cast<UINT32*>(&foundry[8]),
               reinterpret_cast<UINT32*>(&foundry[4]));
    if (strcmp(foundry, "AuthenticAMD") == 0)
    {
        if (!m_BetterAlloc)
        {
            Printf(Tee::PriWarn,
                   "The installed MODS kernel driver version %x.%02x is old and\n"
                   "may lead to degraded performance on this AMD CPU.\n"
                   "Consider upgrading the MODS kernel driver to version %x.%02x or newer.\n",
                   m_Version >> 8, m_Version & 0xFF,
                   BETTER_ALLOC_SUPPORT_VERSION >> 8, BETTER_ALLOC_SUPPORT_VERSION & 0xFF);
        }
    }
#endif

    // Always verify the access token.  In normal runs it will be
    // MODS_ACCESS_TOKEN_NONE which will only succeed verification if there
    // are no other MODS instances running
    MODS_ACCESS_TOKEN modsAccessToken = { m_AccessToken };
    int ret = ioctl(m_KrnFd, MODS_ESC_VERIFY_ACCESS_TOKEN, &modsAccessToken);
    if (ret)
    {
        Printf(Tee::PriError,
               "Access token %u doesn't match current mode of operation.  "
               "Another instance of MODS may be running.\n",
               m_AccessToken);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // Acquire the access token if specified.  No access token may have been
    // provided on since only one access token is allowed at a time
    if (m_AccessTokenMode == AccessTokenMode::ACQUIRE)
    {
        MASSERT(m_AccessToken == MODS_ACCESS_TOKEN_NONE);
        int ret = ioctl(m_KrnFd, MODS_ESC_ACQUIRE_ACCESS_TOKEN, &modsAccessToken);
        if (ret)
        {
            Printf(Tee::PriError, "Can't acquire access token on MODS kernel module.  "
                                  "Another instance of MODS may be running.\n");
            return RC::FILE_PERM;
        }
    }
    m_AccessToken  = modsAccessToken.token;

    return rc;
}

RC Xp::MODSKernelDriver::Close()
{
    StickyRC rc;

    m_IrqRegisterMutex = Tasker::NoMutex();
    m_InterruptMutex   = Tasker::NoMutex();

    if (m_bOpen)
    {
        // Only release the kernel access token if specifically told to do so
        if ((m_AccessToken != MODS_ACCESS_TOKEN_NONE) &&
            (m_AccessTokenMode == AccessTokenMode::RELEASE))
        {
            MODS_ACCESS_TOKEN modsAccessToken = { m_AccessToken };
            const int ret = ioctl(m_KrnFd, MODS_ESC_RELEASE_ACCESS_TOKEN, &modsAccessToken);
            if (ret)
            {
                Printf(Tee::PriError, "Can't release access token on MODS kernel module.\n");
                rc = RC::FILE_PERM;
            }
            m_AccessToken = MODS_ACCESS_TOKEN_NONE;
        }

        m_bOpen = false;
        if (close(m_KrnFd) == -1)
        {
            Printf(Tee::PriError, "Unable to close %s\n", s_DevMods);
            rc = RC::FILE_UNKNOWN_ERROR;
        }
        m_KrnFd = -1;
    }

    if (m_bDevMemOpen)
    {
        m_bDevMemOpen = false;
        if (close(m_DevMemFd) == -1)
        {
            Printf(Tee::PriError, "Unable to close /dev/mem\n");
            rc = RC::FILE_UNKNOWN_ERROR;
        }
        m_DevMemFd = -1;
    }

    m_AddressMap.clear();
    m_AddressMapMutex = Tasker::NoMutex();
    m_pClockManager.reset(nullptr);
    return rc;
}

RC Xp::MODSKernelDriver::GetKernelVersion(UINT64* pVersion)
{
    utsname name;
    if (0 == uname(&name))
    {
        int major=0, minor=0, release=0;
        if (3 == sscanf(name.release, "%d.%d.%d-", &major, &minor, &release))
        {
            *pVersion = (major << 16) | (minor << 8) | release;
            return RC::OK;
        }
    }
    Printf(Tee::PriWarn, "Can't retrieve kernel version\n");
    *pVersion = 0;
    return RC::OK;
}

int Xp::MODSKernelDriver::Ioctl(unsigned long fn, void* param) const
{
    MASSERT(m_bOpen);
    return ioctl(m_KrnFd, fn, param);
}

void* Xp::MODSKernelDriver::Mmap(PHYSADDR physAddr, UINT64 size, Memory::Protect protect, Memory::Attrib attrib)
{
    MASSERT(m_bOpen || m_bDevMemOpen);

    // Mapped address must be page-aligned
    const UINT64 pageSize = Xp::GetPageSize();
    const UINT64 pageOffset = physAddr % pageSize;
    physAddr -= pageOffset;
    size += pageOffset;
    size += pageSize - ((size - 1) % pageSize) - 1;

    // Translate protection attribute
    int mmapProtect = 0;
    if (protect & Memory::Readable)
        mmapProtect |= PROT_READ;
    if (protect & Memory::Writeable)
        mmapProtect |= PROT_WRITE;
    if (protect & Memory::Exelwtable)
        mmapProtect |= PROT_EXEC;

    // Set memory type
    if (m_bSetMemTypeSupported && (attrib != Memory::DE))
    {
        UINT32 type = 0;
        if (OK != GetMemoryType(attrib, &type))
        {
            return nullptr;
        }

        MODS_MEMORY_TYPE param = {0};
        param.physical_address = physAddr;
        param.size             = size;
        param.type             = type;
        if (Ioctl(MODS_ESC_SET_MEMORY_TYPE, &param))
        {
            Printf(Tee::PriLow, "Warning: MODS_ESC_SET_MEMORY_TYPE is not supported\n");
            m_bSetMemTypeSupported = false;
        }
    }

    const int fd = m_bOpen ? m_KrnFd : m_DevMemFd;
    void* addr = mmap64(nullptr, size, mmapProtect, MAP_SHARED, fd, physAddr);

    if (addr == MAP_FAILED)
    {
        return nullptr;
    }

    Tasker::MutexHolder lock(m_AddressMapMutex);

    Pool::AddNonPoolAlloc(addr, size);

    // Account for page alignment
    const uintptr_t retaddr = reinterpret_cast<uintptr_t>(addr) + pageOffset;

    m_AddressMap[retaddr] = make_pair(addr, size);

    return reinterpret_cast<void*>(retaddr);
}

void Xp::MODSKernelDriver::Munmap(void* virtAddr)
{
    MASSERT(m_bOpen || m_bDevMemOpen);
    if (!m_bOpen && !m_bDevMemOpen)
    {
        Printf(Tee::PriWarn, "Munmap called after the driver was closed!\n");
        return;
    }

    void*  addr = nullptr;
    UINT64 size = 0;

    {
        Tasker::MutexHolder lock(m_AddressMapMutex);

        const uintptr_t virtAddrInt = reinterpret_cast<uintptr_t>(virtAddr);
        const AddressMap::iterator it = m_AddressMap.find(virtAddrInt);
        MASSERT(it != m_AddressMap.end());

        addr = reinterpret_cast<void*>(it->second.first);
        size = it->second.second;

        m_AddressMap.erase(it);

        Pool::RemoveNonPoolAlloc(addr, size);
    }

    munmap(addr, size);
}

RC Xp::MODSKernelDriver::GetMemoryType(Memory::Attrib attrib, UINT32* memType)
{
    switch (attrib)
    {
        case Memory::UC:
            *memType = MODS_MEMORY_UNCACHED;
            return RC::OK;
        case Memory::WC:
            *memType = MODS_MEMORY_WRITECOMBINE;
            return RC::OK;
        case Memory::WB:
            *memType = MODS_MEMORY_CACHED;
            return RC::OK;
        case Memory::WT:
            MASSERT(!"Invalid memory type");
            Printf(Tee::PriError, "Memory type Write-Through is not supported on Linux\n");
            return RC::SOFTWARE_ERROR;
        case Memory::WP:
            MASSERT(!"Invalid memory type");
            Printf(Tee::PriError, "Memory type Write-Protect is not supported on Linux\n");
            return RC::SOFTWARE_ERROR;
        default:
            MASSERT(!"Invalid memory type");
            Printf(Tee::PriError, "Unrecognized memory type %d\n", static_cast<int>(attrib));
            return RC::SOFTWARE_ERROR;
    }
}

RC Xp::MODSKernelDriver::AllocPagesOld
(
    UINT64         numBytes,
    bool           contiguous,
    UINT32         addressBits,
    Memory::Attrib attrib,
    UINT32         domain,
    UINT32         bus,
    UINT32         device,
    UINT32         function,
    UINT64*        pMemoryHandle
)
{
    MASSERT(m_bOpen);
    RC rc;

    if (numBytes >= 0x100000000ULL)
    {
        Printf(Tee::PriError, "Unable to allocate 0x%llx bytes of sysmem in one "
               "shot - allocation size must be less than 4GB\n", numBytes);
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    MODS_DEVICE_ALLOC_PAGES_2 ModsAllocPagesData = { };
    UINT32 memoryType = 0;
    CHECK_RC(Xp::MODSKernelDriver::GetMemoryType(attrib, &memoryType));

    // The MODS kernel driver only pays attention to 2 values for address bits:
    // 32 and 64.  Any value other than 32 will effectively not limit the
    // allocation in any way.  For values in between 32 and 64, get a 64 bit
    // address first
    ModsAllocPagesData.num_bytes    = static_cast<UINT32>(numBytes);
    ModsAllocPagesData.contiguous   = contiguous ? 1 : 0;
    ModsAllocPagesData.address_bits = (addressBits > 32) ? 64 : 32;
    ModsAllocPagesData.attrib       = memoryType;

    ModsAllocPagesData.pci_device.domain   = domain;
    ModsAllocPagesData.pci_device.bus      = bus;
    ModsAllocPagesData.pci_device.device   = device;
    ModsAllocPagesData.pci_device.function = function;

    const bool havePciDev = domain != s_NoPCIDev && bus != s_NoPCIDev;

    // In 3.89 we've changed the way of allocating memory without binding
    // it to any specific PCI device.
    if (!m_NoPciDevFFFF && !havePciDev)
    {
        ModsAllocPagesData.pci_device.domain   = 0U;
        ModsAllocPagesData.pci_device.bus      = 0U;
        ModsAllocPagesData.pci_device.device   = 0U;
        ModsAllocPagesData.pci_device.function = 0U;
    }

    if (Ioctl(MODS_ESC_DEVICE_ALLOC_PAGES_2, &ModsAllocPagesData))
    {
        if (havePciDev)
        {
            const char* numaText = "";
            if (IsNumaSystem())
            {
                numaText = ", please check if there is sufficient memory in the GPU's NUMA node";
            }
            Printf(Tee::PriError,
                   "Can't allocate system pages for dev %04x:%02x:%02x.%x%s\n",
                   domain, bus, device, function, numaText);
        }
        else
        {
            Printf(Tee::PriError, "Can't allocate system pages\n");
        }
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    *pMemoryHandle = ModsAllocPagesData.memory_handle;
    return rc;
}

RC Xp::MODSKernelDriver::AllocPages
(
    UINT64         numBytes,
    bool           contiguous,
    UINT32         addressBits,
    Memory::Attrib attrib,
    UINT32         domain,
    UINT32         bus,
    UINT32         device,
    UINT32         function,
    UINT64*        pMemoryHandle
)
{
    MASSERT(m_bOpen);

    if (!m_BetterAlloc)
    {
        return AllocPagesOld(numBytes, contiguous, addressBits, attrib,
                             domain, bus, device, function, pMemoryHandle);
    }

    MODS_ALLOC_PAGES_2 allocParams = { };

    allocParams.num_bytes = numBytes;
    allocParams.numa_node = MODS_ANY_NUMA_NODE;

    switch (attrib)
    {
#ifndef PPC64LE
        case Memory::UC:
            allocParams.flags |= MODS_ALLOC_UNCACHED;
            break;
        case Memory::WC:
            allocParams.flags |= MODS_ALLOC_WRITECOMBINE;
            break;
#endif
        case Memory::WB:
            allocParams.flags |= MODS_ALLOC_CACHED;
            break;
        default:
            Printf(Tee::PriError, "Unsupported cache attribute: %u\n",
                   static_cast<unsigned>(attrib));
            MASSERT(!"Invalid cache attributes");
            return RC::CANNOT_ALLOCATE_MEMORY;
    }

    if (contiguous)
    {
        allocParams.flags |= MODS_ALLOC_CONTIGUOUS;
    }

    if (addressBits <= 32)
    {
        allocParams.flags |= MODS_ALLOC_DMA32;
    }

    const bool havePciDev = domain != s_NoPCIDev && bus != s_NoPCIDev;

    if (havePciDev)
    {
        static bool s_DefDmaMap   = true;
        static bool s_Initialized = false;
        if (!s_Initialized)
        {
            // Don't DMA-map allocated pages by default to PCI device if IOVA is disabled
            JavaScriptPtr pJs;
            bool disableIOVA = false;
            string reason;
            if (pJs->GetProperty(pJs->GetGlobalObject(), "g_DisableIOVA", &disableIOVA) == RC::OK)
            {
                s_DefDmaMap = !disableIOVA;
                reason = Utility::StrPrintf(" because g_DisableIOVA=%s",
                                            disableIOVA ? "true" : "false");
            }

            // Avoid default DMA mapping for VF devices when SR-IOV is enabled,
            // because the DMA mapping is only relevant for PF.
            if (Platform::IsVirtFunMode())
            {
                s_DefDmaMap = false;
                reason = " because we're in VF mode";
            }

            Printf(Tee::PriDebug, "%s default DMA mapping%s\n",
                   s_DefDmaMap ? "Enabled" : "Disabled", reason.c_str());

            s_Initialized = true;
        }

        // DMA mapping is necessary when HW IOMMU is enabled.  And we don't have a way
        // to tell whether HW IOMMU is enabled or not for a particular device.
        // Here we do default DMA mapping for speed and to conserve memory inside the driver.
        if (s_DefDmaMap)
        {
            allocParams.flags |= MODS_ALLOC_MAP_DEV;
        }

        if (addressBits > 32)
        {
            allocParams.flags |= MODS_ALLOC_FORCE_NUMA;
        }

        allocParams.pci_device.domain   = domain;
        allocParams.pci_device.bus      = bus;
        allocParams.pci_device.device   = device;
        allocParams.pci_device.function = function;
    }
    else
    {
        allocParams.flags |= MODS_ALLOC_USE_NUMA;
    }

    if (Ioctl(MODS_ESC_ALLOC_PAGES_2, &allocParams))
    {
        if (havePciDev)
        {
            const char* numaText = "";
            if (IsNumaSystem())
            {
                numaText = ", please check if there is sufficient memory in the GPU's NUMA node";
            }
            Printf(Tee::PriError,
                   "Can't allocate system pages for dev %04x:%02x:%02x.%x%s\n",
                   domain, bus, device, function, numaText);
        }
        else
        {
            Printf(Tee::PriError, "Can't allocate system pages\n");
        }
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    *pMemoryHandle = allocParams.memory_handle;
    return RC::OK;
}

RC Xp::MODSKernelDriver::FindPciDevice(INT32 deviceId, INT32 vendorId, INT32 index,
                                       UINT32* pDomainNumber, UINT32* pBusNumber,
                                       UINT32* pDeviceNumber, UINT32* pFunctionNumber)
{
    MASSERT(m_bOpen);
    RC rc;

    MODS_FIND_PCI_DEVICE_2 ModsFindPciDeviceData = { };
    ModsFindPciDeviceData.device_id = deviceId;
    ModsFindPciDeviceData.vendor_id = vendorId;
    ModsFindPciDeviceData.index     = index;

    if (Ioctl(MODS_ESC_FIND_PCI_DEVICE_2, &ModsFindPciDeviceData))
    {
        Printf(Tee::PriLow, "Warning: Can't find Pci device\n");
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    *pDomainNumber      = ModsFindPciDeviceData.pci_device.domain;
    *pBusNumber         = ModsFindPciDeviceData.pci_device.bus;
    *pDeviceNumber      = ModsFindPciDeviceData.pci_device.device;
    *pFunctionNumber    = ModsFindPciDeviceData.pci_device.function;
    return RC::OK;
}

RC Xp::MODSKernelDriver::FindPciClassCode(INT32 classCode, INT32 index,
                                          UINT32* pDomainNumber, UINT32* pBusNumber,
                                          UINT32* pDeviceNumber, UINT32* pFunctionNumber)
{
    MASSERT(m_bOpen);

    MODS_FIND_PCI_CLASS_CODE_2 ModsFindPciClassCodeData = { };
    ModsFindPciClassCodeData.class_code = classCode;
    ModsFindPciClassCodeData.index      = index;

    if (Ioctl(MODS_ESC_FIND_PCI_CLASS_CODE_2, &ModsFindPciClassCodeData))
    {
        Printf(Tee::PriLow, "Warning: Can't find Pci device with class code 0x%x, index %d \n",
               classCode, index);
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    *pDomainNumber   = ModsFindPciClassCodeData.pci_device.domain;
    *pBusNumber      = ModsFindPciClassCodeData.pci_device.bus;
    *pDeviceNumber   = ModsFindPciClassCodeData.pci_device.device;
    *pFunctionNumber = ModsFindPciClassCodeData.pci_device.function;
    return RC::OK;
}

RC Xp::MODSKernelDriver::PciGetBarInfo(INT32 domainNumber, INT32 busNumber,
                                       INT32 deviceNumber, INT32 functionNumber,
                                       INT32 barIndex, UINT64* pBaseAddress,
                                       UINT64* pBarSize)
{
    MASSERT(m_bOpen);

    MODS_PCI_GET_BAR_INFO_2 ModsPciGetBarInfoData = { };
    ModsPciGetBarInfoData.pci_device.domain   = domainNumber;
    ModsPciGetBarInfoData.pci_device.bus      = busNumber;
    ModsPciGetBarInfoData.pci_device.device   = deviceNumber;
    ModsPciGetBarInfoData.pci_device.function = functionNumber;
    ModsPciGetBarInfoData.bar_index           = barIndex;

    if (Ioctl(MODS_ESC_PCI_GET_BAR_INFO_2, &ModsPciGetBarInfoData))
    {
        Printf(Tee::PriError, "Can't get pci bar info\n");
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    *pBaseAddress = ModsPciGetBarInfoData.base_address;
    *pBarSize     = ModsPciGetBarInfoData.bar_size;
    return RC::OK;
}

RC Xp::MODSKernelDriver::PciEnableSriov
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT32 vfCount
)
{
    MASSERT(m_bOpen);

    MODS_SET_NUM_VF param = { };
    param.dev.domain   = static_cast<UINT16>(domain);
    param.dev.bus      = static_cast<UINT16>(bus);
    param.dev.device   = static_cast<UINT16>(device);
    param.dev.function = static_cast<UINT16>(function);
    param.numvfs       = vfCount;

    if (Ioctl(MODS_ESC_SET_NUM_VF, &param) != static_cast<int>(vfCount))
    {
        if (vfCount == 0)
        {
            Printf(Tee::PriError,
                   "Can't disable SRIOV on %04x:%02x:%02x.%x\n",
                   domain, bus, device, function);
        }
        else
        {
            Printf(Tee::PriError,
                   "Can't enable SRIOV on %04x:%02x:%02x.%x with %u VFs\n",
                   domain, bus, device, function, vfCount);
        }
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    return RC::OK;
}

RC Xp::MODSKernelDriver::PciGetIRQ(INT32 domainNumber, INT32 busNumber,
                                   INT32 deviceNumber, INT32 functionNumber,
                                   UINT32* pIrq)
{
    MASSERT(m_bOpen);

    MODS_PCI_GET_IRQ_2 ModsPciGetIrqData  = { };
    ModsPciGetIrqData.pci_device.domain   = domainNumber;
    ModsPciGetIrqData.pci_device.bus      = busNumber;
    ModsPciGetIrqData.pci_device.device   = deviceNumber;
    ModsPciGetIrqData.pci_device.function = functionNumber;

    if (Ioctl(MODS_ESC_PCI_GET_IRQ_2, &ModsPciGetIrqData))
    {
        Printf(Tee::PriError, "Can't get pci IRQ\n");
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    *pIrq = ModsPciGetIrqData.irq;
    return RC::OK;
}

template<typename UINTXX>
RC Xp::MODSKernelDriver::PciRead(INT32 domainNumber, INT32 busNumber,
                                 INT32 deviceNumber, INT32 functionNumber,
                                 INT32 address, UINTXX* pData)
{
    MASSERT(m_bOpen);

    MODS_PCI_READ_2 ModsPciReadData     = { };
    ModsPciReadData.pci_device.domain   = domainNumber;
    ModsPciReadData.pci_device.bus      = busNumber;
    ModsPciReadData.pci_device.device   = deviceNumber;
    ModsPciReadData.pci_device.function = functionNumber;
    ModsPciReadData.address             = address;
    ModsPciReadData.data_size           = sizeof(UINTXX);

    if (Ioctl(MODS_ESC_PCI_READ_2, &ModsPciReadData))
    {
        *pData = static_cast<UINTXX>(0xffffffff);
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    *pData = static_cast<UINTXX>(ModsPciReadData.data);
    return RC::OK;
}
// Define template implementations
template RC Xp::MODSKernelDriver::PciRead<UINT08>(INT32 domainNumber, INT32 busNumber,
                                         INT32 deviceNumber, INT32 functionNumber,
                                         INT32 address, UINT08* pData);
template RC Xp::MODSKernelDriver::PciRead<UINT16>(INT32 domainNumber, INT32 busNumber,
                                         INT32 deviceNumber, INT32 functionNumber,
                                         INT32 address, UINT16* pData);
template RC Xp::MODSKernelDriver::PciRead<UINT32>(INT32 domainNumber, INT32 busNumber,
                                         INT32 deviceNumber, INT32 functionNumber,
                                         INT32 address, UINT32* pData);

template <typename UINTXX>
RC Xp::MODSKernelDriver::PciWrite(INT32 domainNumber, INT32 busNumber, INT32 deviceNumber,
                                  INT32 functionNumber, INT32 address, UINTXX data)
{
    MASSERT(m_bOpen);

    MODS_PCI_WRITE_2 ModsPciWriteData    = { };
    ModsPciWriteData.pci_device.domain   = domainNumber;
    ModsPciWriteData.pci_device.bus      = busNumber;
    ModsPciWriteData.pci_device.device   = deviceNumber;
    ModsPciWriteData.pci_device.function = functionNumber;
    ModsPciWriteData.address             = address;
    ModsPciWriteData.data_size           = sizeof(UINTXX);
    ModsPciWriteData.data                = static_cast<UINT32>(data);

    if (Ioctl(MODS_ESC_PCI_WRITE_2, &ModsPciWriteData))
    {
        Printf(Tee::PriError, "Can't write to PCI\n");
        return RC::PCI_DEVICE_NOT_FOUND;
    }
    return RC::OK;
}
// Define template implementations
template RC Xp::MODSKernelDriver::PciWrite<UINT08>(INT32 domainNumber, INT32 busNumber, INT32 deviceNumber,
                                                   INT32 functionNumber, INT32 address, UINT08 data);
template RC Xp::MODSKernelDriver::PciWrite<UINT16>(INT32 domainNumber, INT32 busNumber, INT32 deviceNumber,
                                                   INT32 functionNumber, INT32 address, UINT16 data);
template RC Xp::MODSKernelDriver::PciWrite<UINT32>(INT32 domainNumber, INT32 busNumber, INT32 deviceNumber,
                                                   INT32 functionNumber, INT32 address, UINT32 data);

RC Xp::MODSKernelDriver::PciRemove(INT32 domainNumber, INT32 busNumber,
                                 INT32 deviceNumber, INT32 functionNumber)
{
#if defined(MODS_ESC_PCI_BUS_REMOVE_DEV)
    MASSERT(m_bOpen);

    if (m_Version < PCI_REMOVE_SUPPORT_VERSION)
    {
        Printf(Tee::PriError, "PCI removal is supported from kernel version >= 3.92,"
                " please install updated kernel driver\n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    MODS_PCI_BUS_REMOVE_DEV ModsPciRemove = { };
    ModsPciRemove.pci_device.domain   = domainNumber;
    ModsPciRemove.pci_device.bus      = busNumber;
    ModsPciRemove.pci_device.device   = deviceNumber;
    ModsPciRemove.pci_device.function = functionNumber;

    if (Ioctl(MODS_ESC_PCI_BUS_REMOVE_DEV, &ModsPciRemove))
    {
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    return RC::OK;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

RC Xp::MODSKernelDriver::PciResetFunction(INT32 domainNumber, INT32 busNumber,
                                          INT32 deviceNumber, INT32 functionNumber)
{
    MASSERT(m_bOpen);

    if (m_Version < PCI_RESET_FUNCTION_SUPPORT_VERSION)
    {
        Printf(Tee::PriError, "PCI reset function is supported from kernel version >= 4.00,"
                              " please install updated kernel driver\n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    mods_pci_dev_2 pci_device = { };
    pci_device.domain   = domainNumber;
    pci_device.bus      = busNumber;
    pci_device.device   = deviceNumber;
    pci_device.function = functionNumber;

    if (Ioctl(MODS_ESC_PCI_RESET_FUNCTION, &pci_device))
    {
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    return RC::OK;
}

RC Xp::MODSKernelDriver::SetPcieStateByBpmp(INT32 controller, INT32 enable)
{
    MASSERT(m_bOpen);

    MODS_SET_PCIE_STATE modsSetPcieState = { };
    modsSetPcieState.controller = controller;
    modsSetPcieState.enable     = enable;

    const int ret = Ioctl(MODS_ESC_BPMP_SET_PCIE_STATE, &modsSetPcieState);
    if (ret)
    {
        Printf(Tee::PriError,
               "Can't set PCIE controller(%d) to %s state\n",
               controller, enable ? "enable" : "disable");
        Printf(Tee::PriError, "Error code from ioctl(): %d \n", ret);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    return RC::OK;
}

RC Xp::MODSKernelDriver::InitPcieEpPllByBpmp(INT32 epId)
{
    MASSERT(m_bOpen);

    MODS_INIT_PCIE_EP_PLL modsInitPcieEpPll = { };
    modsInitPcieEpPll.ep_id = epId;

    const int ret = Ioctl(MODS_ESC_BPMP_INIT_PCIE_EP_PLL, &modsInitPcieEpPll);
    if (ret)
    {
        Printf(Tee::PriError, "Can't initialize PCIE EP-%d PLL\n", epId);
        Printf(Tee::PriError, "Error code from ioctl(): %d \n", ret);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    return RC::OK;
}

RC Xp::MODSKernelDriver::EvalDevAcpiMethod(UINT32 domainNumber, UINT32 busNumber,
                                           UINT32 deviceNumber, UINT32 functionNumber,
                                           UINT32 acpiId, MODS_EVAL_ACPI_METHOD* method)
{
    MASSERT(m_bOpen);
    MASSERT(method);

    if (m_AcpiIdApiSupport)
    {
        MODS_EVAL_DEV_ACPI_METHOD_3 ModsEvalDevAcpiMethodData = {};
        memcpy(&ModsEvalDevAcpiMethodData.method, method,
               sizeof(ModsEvalDevAcpiMethodData.method));
        ModsEvalDevAcpiMethodData.device.domain   = domainNumber;
        ModsEvalDevAcpiMethodData.device.bus      = busNumber;
        ModsEvalDevAcpiMethodData.device.device   = deviceNumber;
        ModsEvalDevAcpiMethodData.device.function = functionNumber;
        ModsEvalDevAcpiMethodData.acpi_id         = acpiId;

        if (Ioctl(MODS_ESC_EVAL_DEV_ACPI_METHOD_3, &ModsEvalDevAcpiMethodData))
        {
            Printf(Tee::PriLow,
                    "ACPI: Unable to evaluate dev method %s, id 0x%x on %04x:%02x:%02x.%x\n",
                   method->method_name, acpiId, domainNumber,
                   busNumber, deviceNumber, functionNumber);
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }

        memcpy(method, &ModsEvalDevAcpiMethodData.method,
               sizeof(ModsEvalDevAcpiMethodData.method));
        return RC::OK;

    }
    else
    {
        MODS_EVAL_DEV_ACPI_METHOD_2 ModsEvalDevAcpiMethodData = { };
        memcpy(&ModsEvalDevAcpiMethodData.method, method, sizeof(ModsEvalDevAcpiMethodData.method));
        ModsEvalDevAcpiMethodData.device.domain   = domainNumber;
        ModsEvalDevAcpiMethodData.device.bus      = busNumber;
        ModsEvalDevAcpiMethodData.device.device   = deviceNumber;
        ModsEvalDevAcpiMethodData.device.function = functionNumber;

        if (Ioctl(MODS_ESC_EVAL_DEV_ACPI_METHOD_2, &ModsEvalDevAcpiMethodData))
        {
            Printf(Tee::PriLow, "ACPI: Unable to evaluate dev method %s on %04x:%02x:%02x.%x\n",
                   method->method_name, domainNumber, busNumber, deviceNumber, functionNumber);
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }

        memcpy(method, &ModsEvalDevAcpiMethodData.method, sizeof(ModsEvalDevAcpiMethodData.method));
        return RC::OK;
    }
}

RC Xp::MODSKernelDriver::EvalDevAcpiMethod(UINT32 gpuInst, UINT32 acpiId,
                                           MODS_EVAL_ACPI_METHOD* method)
{
    RC rc;
    UINT32 domain   = 0;
    UINT32 bus      = 0;
    UINT32 device   = 0;
    UINT32 function = 0;
    CHECK_RC(GetGpuPciLocation(gpuInst, &domain, &bus, &device, &function));

    return EvalDevAcpiMethod(domain, bus, device, function, acpiId, method);
}

RC Xp::MODSKernelDriver::AcpiGetDDC(UINT32 domainNumber, UINT32 busNumber, UINT32 deviceNumber, UINT32 functionNumber,
                                    UINT32* pOutDataSize, UINT08* pOutBuffer)
{
    MASSERT(m_bOpen);
    MASSERT(pOutDataSize);
    MASSERT(pOutBuffer);

    RC rc;

#ifdef MODS_ESC_GET_ACPI_DEV_CHILDREN
    constexpr UINT32 maxDevChildrenNum = ACPI_MAX_DEV_CHILDREN;
#else
    constexpr UINT32 maxDevChildrenNum = 1;
#endif

    UINT32 devChildrenNum = 0;
    UINT32 devChildren[maxDevChildrenNum] = { };
    if (m_AcpiDevChildren)
    {
        CHECK_RC(AcpiGetDevChildren(domainNumber, busNumber, deviceNumber, functionNumber,
                    &devChildrenNum, &devChildren[0]));
    }
    else
    {
        // 1 of the usage of ACPI _DDC method is to read EDID from panels not conencted to LW GPU.
        // _DDC method maintains a hardcoded list of devices supported. Just so that we don't need
        // to update MODS Kernel driver for each new panel, we are creating the list on MODS client
        // side and passing it to MODS_ESC_EVAL_DEV_ACPI_METHOD_3 (more generic function).
        // On systems with newer kernel driver, this list can be fetched using AcpiGetDevChildren.
        // Add to this list if we find more panels which are not covered by _DDC call and needs to
        // be supported on system with older (no support for AcpiGetDevChildren) MODS kernel driver
        devChildrenNum = 1;
        devChildren[0] = 0x8000a450;
    }

    for (UINT32 devChild = 0; devChild < devChildrenNum; ++devChild)
    {
        constexpr UINT32 maxEdidBlocks = 4;
        for (UINT32 blocks = 1; blocks <= maxEdidBlocks; ++blocks)
        {
            MODS_EVAL_DEV_ACPI_METHOD_3 ModsEvalDevAcpiMethodData = { };
            memcpy(&ModsEvalDevAcpiMethodData.method, "_DDC", sizeof("_DDC"));
            ModsEvalDevAcpiMethodData.method.argument_count = 1;
            ModsEvalDevAcpiMethodData.method.argument[0].type = ACPI_MODS_TYPE_INTEGER;
            ModsEvalDevAcpiMethodData.method.argument[0].integer.value = blocks;
            ModsEvalDevAcpiMethodData.device.domain = domainNumber;
            ModsEvalDevAcpiMethodData.device.bus = busNumber;
            ModsEvalDevAcpiMethodData.device.device = deviceNumber;
            ModsEvalDevAcpiMethodData.device.function = functionNumber;
            ModsEvalDevAcpiMethodData.acpi_id = devChildren[devChild];

            constexpr UINT32 minBlockSize = 128; // Minimum EDID is 128 bytes
            if (!Ioctl(MODS_ESC_EVAL_DEV_ACPI_METHOD_3, &ModsEvalDevAcpiMethodData)
                && ((ModsEvalDevAcpiMethodData.method.out_data_size / minBlockSize) != 0))
            {
                *pOutDataSize = ModsEvalDevAcpiMethodData.method.out_data_size;
                memcpy(pOutBuffer, ModsEvalDevAcpiMethodData.method.out_buffer, *pOutDataSize);
                return RC::OK;
            }
        }
    }

    // fall through to older method only if AcpiGetDevChildren is not supported.
    // AcpiGetDevChildren + MODS_ESC_EVAL_DEV_ACPI_METHOD_3 forms a superset of
    // MODS_ESC_ACPI_GET_DDC_2
    if (m_AcpiDevChildren)
    {
        Printf(Tee::PriError, "ACPI: Unable to retrieve _DDC (EDID) structure.\n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    MODS_ACPI_GET_DDC_2 ModsAcpiGetDDCData = { };
    ModsAcpiGetDDCData.device.domain   = domainNumber;
    ModsAcpiGetDDCData.device.bus      = busNumber;
    ModsAcpiGetDDCData.device.device   = deviceNumber;
    ModsAcpiGetDDCData.device.function = functionNumber;

    if (Ioctl(MODS_ESC_ACPI_GET_DDC_2, &ModsAcpiGetDDCData))
    {
        Printf(Tee::PriError, "ACPI: Unable to retrieve _DDC (EDID) structure\n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    *pOutDataSize = ModsAcpiGetDDCData.out_data_size;
    memcpy(pOutBuffer, ModsAcpiGetDDCData.out_buffer, sizeof(ModsAcpiGetDDCData.out_buffer));

    return RC::OK;
}

RC Xp::MODSKernelDriver::AcpiGetDDC(UINT32 gpuInst, UINT32* pOutDataSize, UINT08* pOutBuffer)
{
    MASSERT(pOutDataSize);
    MASSERT(pOutBuffer);
    RC rc;
    UINT32 domain   = 0;
    UINT32 bus      = 0;
    UINT32 device   = 0;
    UINT32 function = 0;
    CHECK_RC(GetGpuPciLocation(gpuInst, &domain, &bus, &device, &function));

    return AcpiGetDDC(domain, bus, device, function, pOutDataSize, pOutBuffer);
}

RC Xp::MODSKernelDriver::AcpiGetDevChildren
(
    UINT32 domainNumber,
    UINT32 busNumber,
    UINT32 deviceNumber,
    UINT32 functionNumber,
    UINT32* pOutDataSize,
    UINT32* pOutBuffer
)
{
    if (!m_AcpiDevChildren)
    {
        Printf(Tee::PriError, "ACPI to fetch device children not supported "
                "with current MODS kernel driver version\n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

#ifdef MODS_ESC_GET_ACPI_DEV_CHILDREN
    MASSERT(m_bOpen);
    MASSERT(pOutDataSize);
    MASSERT(pOutBuffer);

    MODS_GET_ACPI_DEV_CHILDREN ModsGetAcpiDevChildren = { };
    ModsGetAcpiDevChildren.device.domain   = domainNumber;
    ModsGetAcpiDevChildren.device.bus      = busNumber;
    ModsGetAcpiDevChildren.device.device   = deviceNumber;
    ModsGetAcpiDevChildren.device.function = functionNumber;

    if (Ioctl(MODS_ESC_GET_ACPI_DEV_CHILDREN, &ModsGetAcpiDevChildren))
    {
        Printf(Tee::PriError, "ACPI: Unable to retrieve list of device children\n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    *pOutDataSize = ModsGetAcpiDevChildren.num_children;
    memcpy(pOutBuffer, ModsGetAcpiDevChildren.children, sizeof(ModsGetAcpiDevChildren.children));
    return RC::OK;
#else
    return RC::UNSUPPORTED_SYSTEM_CONFIG;
#endif
}

RC Xp::MODSKernelDriver::DeviceNumaInfo(INT32 domainNumber, INT32 busNumber,
                                        INT32 deviceNumber, INT32 functionNumber,
                                        vector<UINT32> *pLocalCpuMasks)
{
    MASSERT(m_bOpen);
    MASSERT(pLocalCpuMasks);
    pLocalCpuMasks->clear();

    // Skip for SOC devices
    if ((domainNumber < 0) || (busNumber < 0) || (deviceNumber < 0) || (functionNumber < 0))
        return RC::OK;

    if (!IsNumaSystem())
        return RC::OK;

    INT32 node = 0;
    UINT32 nodeCount = 0;
    UINT32 nodeCpuMask[MAX_CPU_MASKS];
    UINT32 cpuCount = 0;

    MODS_DEVICE_NUMA_INFO_2 ModsDeviceNumaInfoData = { };
    ModsDeviceNumaInfoData.pci_device.domain   = domainNumber;
    ModsDeviceNumaInfoData.pci_device.bus      = busNumber;
    ModsDeviceNumaInfoData.pci_device.device   = deviceNumber;
    ModsDeviceNumaInfoData.pci_device.function = functionNumber;

    if (Ioctl(MODS_ESC_DEVICE_NUMA_INFO_2, &ModsDeviceNumaInfoData))
        return RC::OK;

    node      = ModsDeviceNumaInfoData.node;
    nodeCount = ModsDeviceNumaInfoData.node_count;
    memcpy(nodeCpuMask,
           &ModsDeviceNumaInfoData.node_cpu_mask,
           sizeof(ModsDeviceNumaInfoData.node_cpu_mask));
    cpuCount  = ModsDeviceNumaInfoData.cpu_count;

    string dataStr = Utility::StrPrintf("Device %04x:%02x:%02x.%x NUMA Info:\n"
                                        "    Node        : %d\n"
                                        "    NodeCount   : %d\n"
                                        "    CpuCount    : %d\n"
                                        "    NodeCpuMask : ",
                                        domainNumber, busNumber,
                                        deviceNumber, functionNumber,
                                        node, nodeCount, cpuCount);
    for (UINT32 idx = 0; idx < MAX_CPU_MASKS; idx++)
    {
        if ((idx != 0) && (idx % 4 == 0))
        {
            dataStr += "\n                  ";
        }
        dataStr += Utility::StrPrintf("0x%08x ",
                                      nodeCpuMask[idx]);
    }
    Printf(Tee::PriDebug, "%s\n", dataStr.c_str());
    if (node != -1)
    {
        pLocalCpuMasks->assign(nodeCpuMask, nodeCpuMask + MAX_CPU_MASKS);

        vector<UINT32> noCpusFound(pLocalCpuMasks->size(), 0);
        if (noCpusFound == *pLocalCpuMasks)
        {
            Printf(Tee::PriWarn,
                   "No CPUs found for Node %d on NUMA system!\n",
                   node);
            pLocalCpuMasks->clear();
        }
    }
    else
    {
        Printf(Tee::PriWarn,
               "System appears to be NUMA capable, but unable to locate NUMA"
               " node for PCI device at %04x:%02x:%02x.%x!\n",
               domainNumber, busNumber, deviceNumber, functionNumber);
    }
    return RC::OK;
}

INT32 Xp::MODSKernelDriver::GetNumaNode(INT32 domain, INT32 bus, INT32 device, INT32 function)
{
    MASSERT(m_bOpen);

    // Skip for SOC devices
    if ((domain < 0) || (bus < 0) || (device < 0) || (function < 0))
        return -1;

    if (!IsNumaSystem())
        return -1;

    MODS_DEVICE_NUMA_INFO_2 numaParam = { };
    numaParam.pci_device.domain   = domain;
    numaParam.pci_device.bus      = bus;
    numaParam.pci_device.device   = device;
    numaParam.pci_device.function = function;

    if (Ioctl(MODS_ESC_DEVICE_NUMA_INFO_2, &numaParam))
        return -1;

    return numaParam.node;
}

RC Xp::MODSKernelDriver::GetAtsTargetAddressRange(UINT32 gpuInst, UINT32 npuInst,
                                                  UINT64* pPhysAddr, UINT64* pGuestAddr,
                                                  UINT64* pApertureSize, INT32* numaMemNode)
{
    RC rc;
    UINT32 domain   = 0;
    UINT32 bus      = 0;
    UINT32 device   = 0;
    UINT32 function = 0;
    CHECK_RC(GetGpuPciLocation(gpuInst, &domain, &bus, &device, &function));

    MODS_GET_ATS_ADDRESS_RANGE atsData = { };

    atsData.pci_device.domain   = domain;
    atsData.pci_device.bus      = bus;
    atsData.pci_device.device   = device;
    atsData.pci_device.function = function;
    atsData.npu_index           = npuInst;

    if (Ioctl(MODS_ESC_GET_ATS_ADDRESS_RANGE, &atsData))
    {
        Printf(Tee::PriNormal, "Failed to obtain ATS data from the system\n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    *pPhysAddr     = atsData.phys_addr;
    *pGuestAddr    = atsData.guest_addr;
    *pApertureSize = atsData.aperture_size;
    *numaMemNode   = atsData.numa_memory_node;

    return RC::OK;
}

void Xp::MODSKernelDriver::FreePages(UINT64 handle)
{
    MASSERT(m_bOpen);

    MODS_FREE_PAGES modsFreePages = { };
    modsFreePages.memory_handle = handle;
    if (Ioctl(MODS_ESC_FREE_PAGES, &modsFreePages))
    {
        Printf(Tee::PriError, "Can't free system pages\n");
    }
}

RC Xp::MODSKernelDriver::MergePages(UINT64* pHandle, UINT64* pInHandles, UINT32 numInHandles)
{
    MASSERT(m_bOpen);

    MASSERT(numInHandles >= 2 && numInHandles <= MODS_MAX_MERGE_HANDLES);
    if (numInHandles < 2 || numInHandles > MODS_MAX_MERGE_HANDLES)
    {
        return RC::BAD_PARAMETER;
    }

    if (m_MergePages)
    {
        MODS_MERGE_PAGES mergePages = { };
        mergePages.num_in_handles = numInHandles;
        for (UINT32 i = 0; i < numInHandles; i++)
        {
            mergePages.in_memory_handles[i] = pInHandles[i];
        }

        if (Ioctl(MODS_ESC_MERGE_PAGES, &mergePages))
        {
            return RC::CANNOT_ALLOCATE_MEMORY;
        }

        *pHandle = mergePages.memory_handle;
    }
    else
    {
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    return RC::OK;
}

PHYSADDR Xp::MODSKernelDriver::GetPhysicalAddress(UINT64 handle, size_t offset)
{
    MODS_GET_PHYSICAL_ADDRESS_3 modsGetPhysAddr3 = { };
    modsGetPhysAddr3.memory_handle     = handle;
    modsGetPhysAddr3.offset            = offset;
    if (Ioctl(MODS_ESC_GET_PHYSICAL_ADDRESS_2, &modsGetPhysAddr3))
    {
        Printf(Tee::PriError, "Can't get physical address of given allocation\n");
        return 0;
    }
    return modsGetPhysAddr3.physical_address;
}

PHYSADDR Xp::MODSKernelDriver::GetMappedPhysicalAddress
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    UINT64 handle,
    size_t offset
)
{
    MODS_GET_PHYSICAL_ADDRESS_3 modsGetPhysAddr3 = { };
    modsGetPhysAddr3.pci_device.domain   = domain;
    modsGetPhysAddr3.pci_device.bus      = bus;
    modsGetPhysAddr3.pci_device.device   = device;
    modsGetPhysAddr3.pci_device.function = func;
    modsGetPhysAddr3.memory_handle       = handle;
    modsGetPhysAddr3.offset              = offset;
    if (Ioctl(MODS_ESC_GET_MAPPED_PHYSICAL_ADDRESS_3, &modsGetPhysAddr3))
    {
        Printf(Tee::PriError, "Can't get physical address of given allocation\n");
        return 0;
    }
    return modsGetPhysAddr3.physical_address;
}

RC Xp::MODSKernelDriver::DmaMapMemory
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    UINT64 handle
)
{
    MODS_DMA_MAP_MEMORY modsDmaMapMemory = { };
    modsDmaMapMemory.pci_device.domain   = domain;
    modsDmaMapMemory.pci_device.bus      = bus;
    modsDmaMapMemory.pci_device.device   = device;
    modsDmaMapMemory.pci_device.function = func;
    modsDmaMapMemory.memory_handle       = handle;
    if (Ioctl(MODS_ESC_DMA_MAP_MEMORY, &modsDmaMapMemory))
    {
        Printf(Tee::PriError, "Can't map memory to the specified device\n");
        return RC::COULD_NOT_MAP_PHYSICAL_ADDRESS;
    }
    return RC::OK;
}

RC Xp::MODSKernelDriver::DmaUnmapMemory
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    UINT64 handle
)
{
    MODS_DMA_MAP_MEMORY modsDmaUnmapMemory = { };
    modsDmaUnmapMemory.pci_device.domain   = domain;
    modsDmaUnmapMemory.pci_device.bus      = bus;
    modsDmaUnmapMemory.pci_device.device   = device;
    modsDmaUnmapMemory.pci_device.function = func;
    modsDmaUnmapMemory.memory_handle       = handle;
    if (Ioctl(MODS_ESC_DMA_UNMAP_MEMORY, &modsDmaUnmapMemory))
    {
        Printf(Tee::PriError, "Can't unmap memory to the specified device\n");
        return RC::COULD_NOT_MAP_PHYSICAL_ADDRESS;
    }
    return RC::OK;
}

RC Xp::MODSKernelDriver::SetDmaMask
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    UINT32 numBits
)
{
    MODS_PCI_DMA_MASK modsDmaMask   = { };
    modsDmaMask.pci_device.domain   = domain;
    modsDmaMask.pci_device.bus      = bus;
    modsDmaMask.pci_device.device   = device;
    modsDmaMask.pci_device.function = func;
    modsDmaMask.num_bits            = numBits;
    if (Ioctl(MODS_ESC_PCI_SET_DMA_MASK, &modsDmaMask))
    {
        Printf(Tee::PriError, "Can't set DMA mask (%u bits) for device %04x:%02x:%02x:%x\n",
               numBits, domain, bus, device, func);
        return RC::COULD_NOT_MAP_PHYSICAL_ADDRESS;
    }

    return RC::OK;
}

void* Xp::MODSKernelDriver::PhysicalToVirtual(PHYSADDR physAddr, Tee::Priority pri)
{
    MODS_PHYSICAL_TO_VIRTUAL modsPhysToVirt = {0};
    modsPhysToVirt.physical_address = physAddr;
    if (Ioctl(MODS_ESC_PHYSICAL_TO_VIRTUAL, &modsPhysToVirt))
    {
        Printf(pri, "Unable to colwert physical address 0x%llx to virtual\n", physAddr);
        return nullptr;
    }
    return reinterpret_cast<void*>(modsPhysToVirt.virtual_address);
}

PHYSADDR Xp::MODSKernelDriver::VirtualToPhysical(volatile void* virtAddr, Tee::Priority pri)
{
    MODS_VIRTUAL_TO_PHYSICAL modsVirtToPhys = {0};
    modsVirtToPhys.virtual_address = reinterpret_cast<uintptr_t>(virtAddr);
    if (Ioctl(MODS_ESC_VIRTUAL_TO_PHYSICAL, &modsVirtToPhys))
    {
        Printf(pri, "Unable to colwert virtual address %p to physical\n", virtAddr);
        return 0;
    }
    return modsVirtToPhys.physical_address;
}


RC  Xp::MODSKernelDriver::IommuDmaMapMemory(UINT64 handle, string DevName, PHYSADDR *pIova)
{
#ifdef MODS_ESC_IOMMU_DMA_MAP_MEMORY
    MODS_IOMMU_DMA_MAP_MEMORY modsDmaMapMemory = {};
    modsDmaMapMemory.memory_handle       = handle;
    modsDmaMapMemory.flags = MODS_IOMMU_MAP_CONTIGUOUS;
    strncpy(modsDmaMapMemory.dev_name, DevName.c_str(), sizeof(modsDmaMapMemory.dev_name)-1);
    if (Ioctl(MODS_ESC_IOMMU_DMA_MAP_MEMORY, &modsDmaMapMemory))
    {
        Printf(Tee::PriHigh, "Error: Can't map memory by iommu, handle=0x%llx\n", handle);
        return RC::COULD_NOT_MAP_PHYSICAL_ADDRESS;
    }
    if (pIova)
    {
        *pIova = modsDmaMapMemory.physical_address;
    }
    return OK;
#else
    return RC::UNSUPPORTED_SYSTEM_CONFIG;
#endif
}

RC  Xp::MODSKernelDriver::IommuDmaUnmapMemory(UINT64 handle, string DevName)
{
#ifdef MODS_ESC_IOMMU_DMA_UNMAP_MEMORY
    MODS_IOMMU_DMA_MAP_MEMORY modsDmaMapMemory = {};
    modsDmaMapMemory.memory_handle       = handle;
    strncpy(modsDmaMapMemory.dev_name, DevName.c_str(), sizeof(modsDmaMapMemory.dev_name)-1);
    if (Ioctl(MODS_ESC_IOMMU_DMA_UNMAP_MEMORY, &modsDmaMapMemory))
    {
        Printf(Tee::PriHigh, "Error: Can't unmap memory by iommu, handle = 0x%llx\n", handle);
        return RC::COULD_NOT_MAP_PHYSICAL_ADDRESS;
    }
    return OK;
#else
    return RC::UNSUPPORTED_SYSTEM_CONFIG;
#endif
}

RC Xp::MODSKernelDriver::SetupDmaBase
(
    UINT16    domain,
    UINT08    bus,
    UINT08    dev,
    UINT08    func,
    UINT08    bypassMode,
    UINT64    devDmaMask,
    PHYSADDR *pDmaBaseAddr
)
{
    MASSERT(pDmaBaseAddr);
#ifdef LWCPU_PPC64LE
    MODS_SET_PPC_TCE_BYPASS modsDmaBypass = {};
    modsDmaBypass.pci_device.domain   = domain;
    modsDmaBypass.pci_device.bus      = bus;
    modsDmaBypass.pci_device.device   = dev;
    modsDmaBypass.pci_device.function = func;
    modsDmaBypass.mode                = bypassMode;
    modsDmaBypass.device_dma_mask     = devDmaMask;

    if (Ioctl(MODS_ESC_SET_PPC_TCE_BYPASS, &modsDmaBypass))
    {
        Printf(Tee::PriError, "Unable setup dma bypass\n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }
    *pDmaBaseAddr = static_cast<PHYSADDR>(modsDmaBypass.dma_base_address);
#else
    *pDmaBaseAddr = static_cast<PHYSADDR>(0ULL);
    Printf(Tee::PriLow, "Setup dma base only supported on PPC, skipping\n");
#endif
    return RC::OK;
}

RC Xp::MODSKernelDriver::SetLwLinkSysmemTrained
(
    UINT16    domain,
    UINT08    bus,
    UINT08    dev,
    UINT08    func,
    bool      trained
)
{
#ifdef LWCPU_PPC64LE
    MODS_SET_LWLINK_SYSMEM_TRAINED modsSetTrained = { };
    modsSetTrained.pci_device.domain   = domain;
    modsSetTrained.pci_device.bus      = bus;
    modsSetTrained.pci_device.device   = dev;
    modsSetTrained.pci_device.function = func;
    modsSetTrained.trained = (trained)?0x1:0x0;

    if (Ioctl(MODS_ESC_SET_LWLINK_SYSMEM_TRAINED, &modsSetTrained))
    {
        Printf(Tee::PriError, "Unable to set LwLink sysmem trained\n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }
#else
    Printf(Tee::PriLow, "SetLwLinkSysmemTrained only necessary on PPC, skipping\n");
#endif
    return RC::OK;
}

//! PPC-specific function to get the lwlink speed supported by NPU from device-tree
RC Xp::MODSKernelDriver::GetLwLinkLineRate
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    UINT32 *pSpeed
)
{
    MASSERT(pSpeed);
#ifdef LWCPU_PPC64LE
    MODS_GET_LWLINK_LINE_RATE getLineRate = {};
    getLineRate.pci_device.domain   = domain;
    getLineRate.pci_device.bus      = bus;
    getLineRate.pci_device.device   = device;
    getLineRate.pci_device.function = func;
    getLineRate.npu_index           = 0;

    // Initialize speed to 0
    getLineRate.speed               = 0;

    if (Ioctl(MODS_ESC_GET_LWLINK_LINE_RATE, &getLineRate))
    {
        Printf(Tee::PriError, "Unable to get LWLink line rate\n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    *pSpeed = getLineRate.speed;

    return RC::OK;
#else
    Printf(Tee::PriError, "GetLwLinkLineRate only supported on PPC, skipping\n");

    return RC::UNSUPPORTED_FUNCTION;
#endif
}

Xp::ClockManager* Xp::MODSKernelDriver::GetClockManager()
{
    if (!m_bOpen)
    {
        return nullptr;
    }

#if defined(TEGRA_MODS)
    if (!m_pClockManager.get())
    {
        m_pClockManager = make_unique<LinuxClockManager>(*this);
    }
#endif

    return m_pClockManager.get();
}

RC Xp::MODSKernelDriver::MapIRQ(UINT32 * logicalIrq, UINT16 physIrq, string dtName, string fullName)
{

    RC rc;
    struct MODS_DT_INFO ModsDTInfo = {};
    strncpy(ModsDTInfo.dt_name, dtName.c_str(), sizeof(ModsDTInfo.dt_name) - 1);
    strncpy(ModsDTInfo.full_name, fullName.c_str(), sizeof(ModsDTInfo.full_name) - 1);
    ModsDTInfo.index = physIrq;

    if (Ioctl(MODS_ESC_MAP_INTERRUPT, &ModsDTInfo))
    {
        return RC::CANNOT_HOOK_INTERRUPT;
    }

    *logicalIrq = ModsDTInfo.irq;
    return rc;
}

RC Xp::MODSKernelDriver::HookIsr(const IrqParams& irqInfo, ISR func, void* cookie)
{
    MASSERT(m_bOpen);
    RC rc;

    // Protect from HookIsr and UnHookIsr being called simultaneously
    Tasker::MutexHolder lock(m_IrqRegisterMutex);
    const bool firstHook = m_Isrs.empty();

    {
        // Protect m_Isrs from being changed while PollThread uses it
        Tasker::MutexHolder lock(m_InterruptMutex);

        // Compute identifier
        const UINT64 idx = GetDeviceIrqId(irqInfo.pciDev.domain, irqInfo.pciDev.bus,
                                          irqInfo.pciDev.device, irqInfo.pciDev.function);

        // Add handler function to the table
        if (m_Isrs.count(idx) == 0)
        {
            MODS_REGISTER_IRQ_4 irqParams = { };

            irqParams.dev.domain   = irqInfo.pciDev.domain;
            irqParams.dev.bus      = irqInfo.pciDev.bus;
            irqParams.dev.device   = irqInfo.pciDev.device;
            irqParams.dev.function = irqInfo.pciDev.function;
            irqParams.irq_count    = irqInfo.irqType == MODS_XP_IRQ_TYPE_MSIX ? irqInfo.irqNumber : 1;
            irqParams.irq_flags    = irqInfo.irqType | irqInfo.irqFlags << 8;
            irqParams.aperture_addr = irqInfo.barAddr;
            irqParams.aperture_size = irqInfo.barSize;
            irqParams.mask_info_cnt = irqInfo.maskInfoCount;

            for (UINT32 ii = 0; ii < irqInfo.maskInfoCount; ii++)
            {
                irqParams.mask_info[ii].mask_type = irqInfo.maskInfoList[ii].maskType;
                irqParams.mask_info[ii].irq_pending_offset =
                    irqInfo.maskInfoList[ii].irqPendingOffset;
                irqParams.mask_info[ii].irq_enabled_offset =
                    irqInfo.maskInfoList[ii].irqEnabledOffset;
                irqParams.mask_info[ii].irq_enable_offset =
                    irqInfo.maskInfoList[ii].irqEnableOffset;
                irqParams.mask_info[ii].irq_disable_offset =
                    irqInfo.maskInfoList[ii].irqDisableOffset;
                irqParams.mask_info[ii].and_mask = irqInfo.maskInfoList[ii].andMask;
                irqParams.mask_info[ii].or_mask = irqInfo.maskInfoList[ii].orMask;
            }

            const int err = Ioctl(MODS_ESC_REGISTER_IRQ_4, &irqParams);
            if (err)
            {
                const int code = errno;
                if (code == EBUSY && irqInfo.irqType != MODS_XP_IRQ_TYPE_CPU)
                {
                    Printf(Tee::PriError, "Another MODS instance is accessing PCI device "
                           "%04x:%02x:%02x.%x\n",
                           irqInfo.pciDev.domain,
                           irqInfo.pciDev.bus,
                           irqInfo.pciDev.device,
                           irqInfo.pciDev.function);
                    return RC::RESOURCE_IN_USE;
                }
                Printf(Tee::PriError, "%d %s\n", code, strerror(code));
                return RC::CANNOT_HOOK_INTERRUPT;
            }
        }

        m_Isrs[idx].hooks.push_back(IsrData(func, cookie));
        m_Isrs[idx].type = irqInfo.irqType;
    }

    // Create PollThread only once, when first HookIRQ is called
    if (firstHook)
    {
        m_IrqStats.minIrqDelay   = ~0U;
        m_IrqStats.maxIrqDelay   = 0U;
        m_IrqStats.totalIrqDelay = 0U;
        m_IrqStats.numIrqs       = 0U;

        CHECK_RC(CreateIrqHookPipe());
        m_IrqThread = Tasker::CreateThread(IsrThreadFunc, this, Tasker::MIN_STACK_SIZE, "interrupt");
    }
    return rc;
}

RC Xp::MODSKernelDriver::CreateIrqHookPipe()
{
    int fd[2] = {
        static_cast<int>(Cpu::AtomicRead(&m_IrqHookPipe[0])),
        static_cast<int>(Cpu::AtomicRead(&m_IrqHookPipe[1]))
    };
    if (fd[0] == -1 && fd[1] == -1)
    {
        if (pipe(fd) < 0)
        {
            Printf(Tee::PriError, "Cannot create ISR keep-alive pipe\n");
            return RC::CANNOT_HOOK_INTERRUPT;
        }
        Cpu::AtomicWrite(&m_IrqHookPipe[0], static_cast<UINT32>(fd[0]));
        Cpu::AtomicWrite(&m_IrqHookPipe[1], static_cast<UINT32>(fd[1]));
    }
    else
    {
        Printf(Tee::PriError, "Previous ISR unhooked unsucessfully\n");
        return RC::CANNOT_HOOK_INTERRUPT;
    }
    return RC::OK;
}

RC Xp::MODSKernelDriver::UnHookIsr(UINT32 pciDomain, UINT32 pciBus, UINT32 pciDevice,
                                   UINT32 pciFunction, UINT32 type, ISR func, void* cookie)
{
    MASSERT(m_bOpen);

    // Protect from HookIsr and UnHookIsr being called simultaneously
    Tasker::MutexHolder lock(m_IrqRegisterMutex);
    {
        // Protect m_Isrs from being changed while PollThread uses it
        Tasker::MutexHolder lock(m_InterruptMutex);

        // Compute identifier
        const UINT64 idx = GetDeviceIrqId(pciDomain, pciBus, pciDevice, pciFunction);

        // Remove handler function from the table
        Isrs::iterator isrIt = m_Isrs.find(idx);
        if (isrIt != m_Isrs.end())
        {
            IrqHooks &Hooks = isrIt->second.hooks;

            for (IrqHooks::iterator Hooks_iter = Hooks.begin();
                 Hooks_iter != Hooks.end(); Hooks_iter++)
            {
                if ((*Hooks_iter) == IsrData(func, cookie))
                {
                    Hooks.erase(Hooks_iter);
                    if (Hooks.empty())
                    {
                        m_Isrs.erase(isrIt);

                        // Unhook the interrupt in the kernel driver
                        MODS_REGISTER_IRQ_2 ModsUnregisterIrqData = {};
                        ModsUnregisterIrqData.dev.domain   = pciDomain;
                        ModsUnregisterIrqData.dev.bus      = pciBus;
                        ModsUnregisterIrqData.dev.device   = pciDevice;
                        ModsUnregisterIrqData.dev.function = pciFunction;
                        ModsUnregisterIrqData.type         = type;

                        if (Ioctl(MODS_ESC_UNREGISTER_IRQ_2, &ModsUnregisterIrqData))
                        {
                            return RC::CANNOT_UNHOOK_ISR;
                        }
                    }
                    break;
                }
            }
        }
    }

    // Destroy InterruptThread and InterruptEvent only when last UnhookIRQ is called
    if (m_Isrs.empty())
    {
        bool closedPipe = false;
        const UINT32 fd = Cpu::AtomicXchg32(&m_IrqHookPipe[1], ~0U);
        if (close(static_cast<int>(fd)) < 0)
        {
            Printf(Tee::PriError, "Unable to close write-end of ISR pipe\n");
        }
        else
        {
            closedPipe = true;
        }
        Tasker::Join(m_IrqThread);
        m_IrqThread = -1;

        // Display IRQ statistics
        Printf(Tee::PriLow, "IRQ statistics:\n");
        Printf(Tee::PriLow, "  num IRQs:      %u\n", m_IrqStats.numIrqs);
        if (m_IrqStats.numIrqs > 0)
        {
            Printf(Tee::PriLow, "  avg IRQ delay: %lluus\n", m_IrqStats.totalIrqDelay/m_IrqStats.numIrqs);
            Printf(Tee::PriLow, "  min IRQ delay: %uus\n", m_IrqStats.minIrqDelay);
            Printf(Tee::PriLow, "  max IRQ delay: %uus\n", m_IrqStats.maxIrqDelay);
        }
        if (!closedPipe)
        {
            return RC::CANNOT_UNHOOK_ISR;
        }
    }

    return RC::OK;
}

void Xp::MODSKernelDriver::IsrThreadFunc(void* args)
{
    static_cast<MODSKernelDriver*>(args)->IsrThread();
}

void Xp::MODSKernelDriver::IsrThread()
{
    Tasker::DetachThread detach;

    DEFER
    {
        const UINT32 fd = Cpu::AtomicXchg32(&m_IrqHookPipe[0], ~0U);
        if (close(static_cast<int>(fd)) < 0)
        {
            Printf(Tee::PriError, "Cannot close read-end of ISR pipe\n");
        }
    };

    // number of file descriptors for poll
    const nfds_t nfds = 2;

    // Set up the initial listening socket
    struct pollfd fds[2];
    fds[0].fd     = m_KrnFd;
    fds[0].events = POLLIN;
    fds[1].fd     = static_cast<int>(Cpu::AtomicRead(&m_IrqHookPipe[0])); // read-end of the pipe
    fds[1].events = POLLIN;

    while (Cpu::AtomicRead(&m_IrqHookPipe[1]) != ~0U)
    {
        const int rc = poll(fds, nfds, 10000);

        if (rc > 0)
        {
            bool handledIrq = false;
            if (fds[0].revents & POLLIN)
            {
                Tasker::MutexHolder lock(m_InterruptMutex);
                HandleIrq();
                handledIrq = true;
            }
            else if (fds[0].revents & POLLERR)
            {
                Printf(Tee::PriDebug, "poll() quit\n");
                break;
            }

            // When UnhookIsr() closes the write-end of the pipe, poll() will
            // send a POLLHUP (hang-up) signal, which lets us know we should
            // finish servicing IRQs since all ISRs are disabled.
            if (fds[1].revents & POLLHUP)
            {
                Printf(Tee::PriDebug, "IsrThread signaled to stop\n");
                break;
            }

            if (!handledIrq)
            {
                Printf(Tee::PriWarn,
                       "poll() unknown events: KrnFd.revents = 0x%x, "
                       "IsrPipe.revents = 0x%x\n",
                       static_cast<unsigned>(fds[0].revents),
                       static_cast<unsigned>(fds[1].revents));
            }
        }
        else if (rc == 0)
        {
            continue;
        }
        else // rc < 0
        {
            Printf(Tee::PriDebug, "poll() failed rc %x\n", rc);
            continue;
        }
    }
}

namespace
{
    struct ModsIrq
    {
        UINT32 domain, bus, device, function, delay;
    };
}

void Xp::MODSKernelDriver::HandleIrq()
{
    // Handle IRQs while the write-end of m_IrqHookPipe is still open
    UINT08 moreIrqs = 1;
    while (Cpu::AtomicRead(&m_IrqHookPipe[1]) != ~0U && moreIrqs)
    {
        moreIrqs = 0;
        ModsIrq irqList[MODS_MAX_IRQS];
        MODS_QUERY_IRQ_2 ModsQueryIrqData = { };

        if (Ioctl(MODS_ESC_QUERY_IRQ_2, &ModsQueryIrqData))
        {
            Printf(Tee::PriError, "Unable to obtain IRQs from the driver\n");
            break;
        }
        for (UINT32 i = 0; i < MODS_MAX_IRQS; i++)
        {
            irqList[i].domain   = ModsQueryIrqData.irq_list[i].dev.domain;
            irqList[i].bus      = ModsQueryIrqData.irq_list[i].dev.bus;
            irqList[i].device   = ModsQueryIrqData.irq_list[i].dev.device;
            irqList[i].function = ModsQueryIrqData.irq_list[i].dev.function;
            irqList[i].delay    = ModsQueryIrqData.irq_list[i].delay;
        }
        moreIrqs = ModsQueryIrqData.more;

        for (UINT32 i = 0; (i < MODS_MAX_IRQS) && (irqList[i].bus != 0xFFFFU); i++)
        {
            const UINT32& pciDomain   = irqList[i].domain;
            const UINT32& pciBus      = irqList[i].bus;
            const UINT32& pciDevice   = irqList[i].device;
            const UINT32& pciFunction = irqList[i].function;
            const UINT32& delay       = irqList[i].delay;

            // Gather IRQ statistics
            m_IrqStats.totalIrqDelay += delay;
            if (delay < m_IrqStats.minIrqDelay) m_IrqStats.minIrqDelay = delay;
            if (delay > m_IrqStats.maxIrqDelay) m_IrqStats.maxIrqDelay = delay;
            m_IrqStats.numIrqs++;

            const UINT64 id = GetDeviceIrqId(pciDomain, pciBus, pciDevice, pciFunction);
            const Isrs::const_iterator iter = m_Isrs.find(id);
            const bool isCpuIrq = (iter == m_Isrs.end())
                    ? (pciDevice == 0xFFU && pciFunction == 0xFFU)
                    : (iter->second.type == MODS_XP_IRQ_TYPE_CPU);
            const UINT32 cpuIrq = isCpuIrq ? pciBus : ~0U;
            bool handlerFound = false;
            bool wasServiced = false;

            if (iter != m_Isrs.end())
            {
                const IrqHooks& hooks = iter->second.hooks;
                if (!hooks.empty())
                {
                    handlerFound = true;
                }
#ifdef INCLUDE_GPU
                bool rmIsrCalled = false;
#endif
                // Ilwoke hooks for this device
                for (UINT32 iHook = 0; iHook < hooks.size(); iHook++)
                {
                    if (hooks[iHook].GetIsr() == 0)
                    {
#ifdef INCLUDE_GPU
                        if (!rmIsrCalled)
                        {
                            GpuDevMgr* const pGpuDevMgr = static_cast<GpuDevMgr*>(
                                    DevMgrMgr::d_GraphDevMgr);
                            RC rc;
                            if (isCpuIrq)
                            {
                                rc = pGpuDevMgr->ServiceInterrupt(cpuIrq);
                            }
                            else
                            {
                                const UINT32 irqNum = ~0U; // TODO: update for Turing
                                rc = pGpuDevMgr->ServiceInterrupt(
                                        pciDomain, pciBus, pciDevice, pciFunction, irqNum);
                            }
                            if (OK == rc)
                            {
                                rmIsrCalled = true;
                                wasServiced = true;
                            }
                        }
#else
                        MASSERT(0);
#endif
                    }
                    else
                    {
                        if (Xp::ISR_RESULT_SERVICED == hooks[iHook]())
                        {
                            wasServiced = true;
                        }
                    }
                }
            }

            if (!handlerFound)
            {
                if (isCpuIrq)
                {
                    Printf(Tee::PriError, "No handler found for IRQ %u\n",
                           cpuIrq);
                }
                else
                {
                    Printf(Tee::PriError, "No handler found for device %04x:%02x:%02x.%x\n",
                           pciDomain, pciBus, pciDevice, pciFunction);
                }
            }
            else if (!wasServiced && (iter->second.type == MODS_XP_IRQ_TYPE_MSI))
            {
                Printf(Tee::PriLow, "Warning: MSI for device %04x:%02x:%02x.%x not serviced\n",
                       pciDomain, pciBus, pciDevice, pciFunction);
            }
        }
    }
    if (moreIrqs)
    {
        Printf(Tee::PriWarn, "IRQ(s) not serviced after UnHookIsr()\n");
    }
}

void Xp::MODSKernelDriver::EnableInterrupts()
{
    MASSERT(m_bOpen);
    MASSERT(m_IrqlNestCount > 0);

    m_IrqlNestCount--;
    if (m_IrqlNestCount == 0)
    {
        Cpu::AtomicWrite(&m_LwrrentIrql, static_cast<UINT32>(Platform::NormalIrql));
    }
    else
    {
        MASSERT(0);
    }
    Tasker::ReleaseMutex(m_InterruptMutex.get());
}

void Xp::MODSKernelDriver::DisableInterrupts()
{
    MASSERT(m_bOpen);
    Tasker::AcquireMutex(m_InterruptMutex.get());
    m_IrqlNestCount++;
    if (m_IrqlNestCount == 1)
    {
        Cpu::AtomicWrite(&m_LwrrentIrql, static_cast<UINT32>(Platform::ElevatedIrql));
    }
    else
    {
        MASSERT(!"Nesting DisableInterrupts may cause a deadlock!");
    }
}

Platform::Irql Xp::MODSKernelDriver::GetLwrrentIrql() const
{
    return static_cast<Platform::Irql>(
            Cpu::AtomicRead(const_cast<volatile UINT32*>(
                    &m_LwrrentIrql)));
}

Platform::Irql Xp::MODSKernelDriver::RaiseIrql(Platform::Irql newIrql)
{
    MASSERT(m_bOpen);

    Tasker::MutexHolder lock(m_InterruptMutex);

    const Platform::Irql oldIrql = GetLwrrentIrql();
    MASSERT(newIrql >= oldIrql);

    if ((newIrql == Platform::ElevatedIrql) &&
        (newIrql != oldIrql))
    {
        DisableInterrupts();
    }

    return oldIrql;
}

void Xp::MODSKernelDriver::LowerIrql(Platform::Irql newIrql)
{
    MASSERT(m_bOpen);

    Tasker::MutexHolder lock(m_InterruptMutex);

    MASSERT(newIrql <= GetLwrrentIrql());
    if ((newIrql == Platform::NormalIrql) &&
        (newIrql != GetLwrrentIrql()))
    {
        EnableInterrupts();
    }
}

RC Xp::MODSKernelDriver::RdMsrOnCpu(UINT32 cpu, UINT32 reg, UINT32 *pHigh, UINT32 *pLow)
{
    MASSERT(pLow);
    MASSERT(pHigh);

    if (!m_MsrSupport)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    MODS_MSR param = {};
    param.reg      = reg;
    param.cpu_num  = cpu;

    if (Ioctl(MODS_ESC_READ_MSR, &param))
    {
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    *pLow  = param.low;
    *pHigh = param.high;

    return RC::OK;
}

RC Xp::MODSKernelDriver::WrMsrOnCpu(UINT32 cpu, UINT32 reg, UINT32 high, UINT32 low)
{
    if (!m_MsrSupport)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    MODS_MSR param = {};
    param.reg      = reg;
    param.cpu_num  = cpu;
    param.low      = low;
    param.high     = high;

    if (Ioctl(MODS_ESC_WRITE_MSR, &param))
    {
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    return RC::OK;
}

RC Xp::MODSKernelDriver::WriteSysfsNode(string filePath, string fileContents)
{
    RC rc;

    if (!Xp::DoesFileExist(filePath))
    {
        Printf(Tee::PriError,
               "%s node not found.\n",
               filePath.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }

    MODS_SYSFS_NODE param = { };
    // MODS kernel driver prepends the input file path with /sys/
    // Need to remove /sys/ from file path before copying the path
    // to the IOCTL parameters
    filePath = filePath.substr(0x5, sizeof(param.path) - 1);

    param.size = MIN(fileContents.size(), sizeof(param.contents));
    memcpy(param.contents, fileContents.c_str(), param.size);
    memcpy(param.path, filePath.c_str(), filePath.size());

    if (Ioctl(MODS_ESC_WRITE_SYSFS_NODE, &param))
    {
        Printf (Tee::PriError,
                "Unable to update file /sys/%s with contents: %s\n",
                filePath.c_str(), fileContents.c_str());
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }
    Printf(Tee::PriDebug,
           "File /sys/%s updated with value %s\n",
            filePath.c_str(),
            fileContents.c_str());


    return rc;
}

RC Xp::MODSKernelDriver::SysctlWrite(const char* path, INT64 value)
{
    if (!m_ProcSysWrite)
    {
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

#ifdef MODS_ESC_SYSCTL_WRITE_INT
    MODS_SYSCTL_INT param = { };
    strncpy(param.path, path, sizeof(param.path) - 1);
    param.value = value;

    return Ioctl(MODS_ESC_SYSCTL_WRITE_INT, &param) ? RC::UNSUPPORTED_SYSTEM_CONFIG : RC::OK;
#else
    return RC::UNSUPPORTED_SYSTEM_CONFIG;
#endif
}
