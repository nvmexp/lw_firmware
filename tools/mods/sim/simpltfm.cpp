/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Simulation implementation of Platform

#include "core/include/chiplibtracecapture.h"
#include "core/include/cmdline.h"
#include "core/include/cpu.h"
#include "core/include/display.h"
#include "core/include/fileholder.h"
#include "core/include/gpu.h"
#include "core/include/heap.h"
#include "core/include/isrdata.h"
#include "core/include/jscript.h"
#include "core/include/mgrmgr.h"
#include "core/include/modsdrv.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "core/include/pool.h"
#include "core/include/script.h"
#include "core/include/simclk.h"
#include "core/include/tasker.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "core/platform/pltfmpvt.h"
#include "core/utility/sharedsysmem.h"
#include "ctrl/ctrl2080/ctrl2080lwlink.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/testdevice.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/include/vgpu.h"
#include "gpu/vmiop/vmiopelw.h"
#include "gpu/vmiop/vmiopelwmgr.h"
#include "IAModelBackdoor.h"
#include "IBusMem.h"
#include "IChip.h"
#include "ifspec3_shim_mods.h"
#include "IGpuEscape.h"
#include "IGpuEscape2.h"
#include "IInterrupt.h"
#include "IInterrupt3.h"
#include "IInterruptMgr.h"
#include "IInterruptMgr2.h"
#include "IIo.h"
#include "IMapMemory.h"
#include "IMapMemoryExt.h"
#include "IMemAlloc64.h"
#include "IMemory.h"
#include "IMultiHeap.h"
#include "IMultiHeap2.h"
#include "IPciDev.h"
#include "IPciDev2.h"
#include "IPpc.h"
#include "kepler/gk104/dev_master.h" // LW_PMC_BOOT_0_ARCHITECTURE_GK100
#include "mheapsim.h"
#include "random.h"
#include "shim.h"
#include "sim/chiplibtrace/replayer/replayer.h"
#include "simcallmutex.h"
#include "simpage.h"
#include "substitute_registers.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"

#include <map>
#include <vector>
#include <queue>
#include <set>
#include <algorithm>
#include <cstring> // memcpy
#include <sstream>

#ifndef _WIN32
#include <execinfo.h> // Linux backtrace and backtrace_symbols
#endif

// JavaScript hooks that are simulator-specific
extern SObject Platform_Object;
extern SProperty Platform_SwapEndian;

enum { TRACK_READ = 1, TRACK_WRITE = 2 };
enum { CALLSTACK, PRINT, BREAKPOINT, SIMCLK, WALLCLK };
enum { TRACK_PHYSICAL = -1, TRACK_BAR0, TRACK_BAR1, TRACK_BAR2 };

static SProperty Platform_OverrideSysMemBase
(
    Platform_Object,
    "OverrideSysMemBase",
    0,
    false,
    0,
    0,
    0,
    "Have MODS, rather than the chiplib, select the base of system memory."
);
static SProperty Platform_SysMemBase
(
    Platform_Object,
    "SysMemBase",
    0,
    0,
    0,
    0,
    0,
    "The physical address of the base of simulator system memory."
);
static SProperty Platform_SysMemSize
(
    Platform_Object,
    "SysMemSize",
    0,
    256*1024*1024,
    0,
    0,
    0,
    "The size of simulator system memory, in bytes."
);
static SProperty Platform_EnableSysmemMultiHeap
(
    Platform_Object,
    "EnableSysmemMultiHeap",
    0,
    false,
    0,
    0,
    0,
    "Enable multiple system memory heaps, one for each flavor of cache control (UC, WB, etc)."
);
static SProperty Platform_SysMemSizeUC
(
    Platform_Object,
    "SysMemSizeUC",
    0,
    128*1024*1024*4,
    0,
    0,
    0,
    "The size of UNCACHED simulator system memory, in bytes."
);
static SProperty Platform_SysMemSizeWC
(
    Platform_Object,
    "SysMemSizeWC",
    0,
    128*1024*1024*4,
    0,
    0,
    0,
    "The size of WRITE COMBINE simulator system memory, in bytes."
);
static SProperty Platform_SysMemSizeWT
(
    Platform_Object,
    "SysMemSizeWT",
    0,
    128*1024*1024*4,
    0,
    0,
    0,
    "The size of WRITE THROUGH simulator system memory, in bytes."
);
static SProperty Platform_SysMemSizeWP
(
    Platform_Object,
    "SysMemSizeWP",
    0,
    128*1024*1024*4,
    0,
    0,
    0,
    "The size of WRITE PROTECT simulator system memory, in bytes."
);
static SProperty Platform_SysMemSizeWB
(
    Platform_Object,
    "SysMemSizeWB",
    0,
    128*1024*1024*4,
    0,
    0,
    0,
    "The size of WRITE BACK simulator system memory, in bytes."
);
static SProperty Platform_ForceChiplibSysmem
(
    Platform_Object,
    "ForceChiplibSysmem",
    0,
    false,
    0,
    0,
    0,
    "If true, send system memory writes to the chiplib, even if the chiplib allows MODS to manage memory."
);
static SProperty Platform_PteReverse
(
    Platform_Object,
    "PteReverse",
    0,
    false,
    0,
    0,
    0,
    "If true, allocate all system memory from top down to create noncontiguous memory allocations."
);
static SProperty Platform_PteRandom
(
    Platform_Object,
    "PteRandom",
    0,
    false,
    0,
    0,
    0,
    "If true, allocate all system memory in randomly scattered pages."
);
static SProperty Platform_PteSeed
(
    Platform_Object,
    "PteSeed",
    0,
    0,
    0,
    0,
    0,
    "The random seed to use for scrambling system memory allocations."
);
static SProperty Platform_NoBackdoor
(
    Platform_Object,
    "NoBackdoor",
    0,
    false,
    0,
    0,
    0,
    "If true, send all transactions through the frontdoor path."
);
static SProperty Platform_ChipArgs
(
    Platform_Object,
    "ChipArgs",
    0,
    "",
    0,
    0,
    0,
    "Pass extra arguments to the chip library."
);
static SProperty Platform_FloorsweepArgs
(
    Platform_Object,
    "FloorsweepArgs",
    0,
    "",
    0,
    0,
    0,
    "Pass floorsweep arguments to the chip library."
);
static SProperty Platform_LoadSimSymbols
(
    Platform_Object,
    "LoadSimSymbols",
    0,
    true,
    0,
    0,
    0,
    "should gdb load sim symbols for stack dumping?"
);
static SProperty Platform_DumpVirtualAccess
(
    Platform_Object,
    "DumpVirtualAccess",
    0,
    false,
    0,
    0,
    0,
    "If true, MODS will dump all virtual read/write addresses and data"
);
static SProperty Platform_DumpPhysicalAccess
(
    Platform_Object,
    "DumpPhysicalAccess",
    0,
    false,
    0,
    0,
    0,
    "If true, MODS will dump all physical read/write addresses and data"
);
static SProperty Platform_SockCmdLine
(
    Platform_Object,
    "SockCmdLine",
    0,
    "",
    0,
    0,
    0,
    "Pass Command to start sockserver."
);
static SProperty Platform_ForceSockserv
(
    Platform_Object,
    "ForceSockserv",
    0,
    false,
    0,
    0,
    0,
    "Force use of automatic load of sockserv/sockchip instead direct chiplib"
);
static SProperty Platform_TraceSockserv
(
    Platform_Object,
    "TraceSockserv",
    0,
    false,
    0,
    0,
    0,
    "Enable tracing in sockserv"
);
static SProperty Platform_TraceSockchip
(
    Platform_Object,
    "TraceSockchip",
    0,
    false,
    0,
    0,
    0,
    "Enable tracing in sockchip"
);
static SProperty Platform_ReserveSysmemMB
(
    Platform_Object,
    "ReserveSysmemMB",
    0,
    0,
    0,
    0,
    0,
    "Tell heap to reserve sysmem"
);
static SProperty Platform_CarveoutSizeMb
(
    Platform_Object,
    "CarveoutSizeMb",
    0,
    0,
    0,
    0,
    0,
    "Force carveout size in MB (default 64)."
);
static SProperty Platform_VPRSizeMb
(
    Platform_Object,
    "VPRSizeMb",
    0,
    0,
    0,
    0,
    0,
    "Force VPR size in MB (default 0)."
);
static SProperty Platform_ACR1Size
(
    Platform_Object,
    "ACR1Size",
    0,
    0,
    0,
    0,
    0,
    "Force Access Control Region 1 size in bytes (default 0)."
);
static SProperty Platform_ACR2Size
(
    Platform_Object,
    "ACR2Size",
    0,
    0,
    0,
    0,
    0,
    "Force Access Control Region 2 size in bytes (default 0)."
);

static SProperty Platform_SimClk
(
    Platform_Object,
    "SimClk",
    0,
    0,
    0,
    0,
    0,
    "enable/disable simclk profiler (default disable)."
);

static SProperty Platform_SimClkMode
(
    Platform_Object,
    "SimClkMode",
    0,
    0,
    0,
    0,
    0,
    "set SimClkMode gild/check (default check)."
);

static SProperty Platform_SimClkProfile
(
    Platform_Object,
    "SimClkProfile",
    0,
    "SimClk_Profile.gold",
    0,
    0,
    0,
    "set SimClk profile path(default SimClk_Profile.gold)."
);

static SProperty Platform_SimClkThreshhold
(
    Platform_Object,
    "SimClkThreshhold",
    0,
    30,
    0,
    0,
    0,
    "set simclk profiler threshhold (default 30)."
);

static SProperty Platform_TrackAll
(
    Platform_Object,
    "TrackAll",
    0,
    false,
    0,
    0,
    0,
    "Track all of the addresses, limited by filters."
);

static SProperty Platform_TrackBAR
(
    Platform_Object,
    "TrackBAR",
    0,
    TRACK_PHYSICAL, //initial value set to Physical
    0,
    0,
    0,
    "Sets what kind of address to follow: Physical, Bar0, Bar1, or Bar2"
);

static SProperty Platform_TrackPhysicalAddress
(
    Platform_Object,
    "TrackPhysicalAddress",
    0,
    0, //initial value is set to NULL
    0,
    0,
    0,
    "If set, MODS will dump call stack of the specified physical address"
);

static SProperty Platform_TrackMode
(
    Platform_Object,
    "TrackMode",
    0,
    PRINT,
    0,
    0,
    0,
    "The mode of printing when following a specified address"
);

static SProperty Platform_TrackType
(
    Platform_Object,
    "TrackType",
    0,
    3, //rw
    0,
    0,
    0,
    "The type of access (read or write) to follow of a specified address"
);

static SProperty Platform_TrackValueEnable
(
    Platform_Object,
    "TrackValueEnable",
    0,
    false,
    0,
    0,
    0,
    "The whether tracking for a specific value is enabled for a specified address"
);

static SProperty Platform_TrackValue
(
    Platform_Object,
    "TrackValue",
    0,
    0,
    0,
    0,
    0,
    "The specific value to track for of a specified address"
);

typedef struct Carveout
{
    PHYSADDR base;
    UINT64   size;
    void    *pDesc;
    bool     valid;
    bool     descFromOthers; //mark whether desc points to the same content with other desc.
} Carveout;

static SProperty Platform_ChiplibTraceDump
(
    Platform_Object,
    "EnableChiplibTraceDump",
    0,
    0,
    0,
    0,
    0,
    "enable chiplib trace dump (default disable)."
);

static SProperty Platform_RawChiplibTraceDump
(
    Platform_Object,
    "EnableRawChiplibTraceDump",
    0,
    0,
    0,
    0,
    0,
    "enable raw chiplib trace dump (default disable)."
);

static SProperty Platform_RawChiplibTraceDumpLevel
(
    Platform_Object,
    "RawChiplibTraceDumpLevel",
    0,
    0,
    0,
    0,
    0,
    "set raw chiplib trace dump level."
);

static SProperty Platform_BackDoorPramin
(
    Platform_Object,
    "EnableBackdoorPramin",
    0,
    0,
    0,
    0,
    0,
    "enable backdoor Pramin (default disable)."
);

static SProperty Platform_IgnoredRegFile
(
    Platform_Object,
    "IgnoredRegFile",
    0,
    "",
    0,
    0,
    0,
    "File of registers to ignore."
);

static SProperty Platform_SkippedRegFile
(
    Platform_Object,
    "SkippedRegFile",
    0,
    "",
    0,
    0,
    0,
    "File of registers to skip."
);

static SProperty Platform_NoCompressCapture
(
    Platform_Object,
    "NoCompressCapture",
    0,
    0,
    0,
    0,
    0,
    "do not compress capture file"
);

static SProperty Platform_SysMemPageSize
(
    Platform_Object,
    "SysMemPageSize",
    0,
#ifdef PPC64LE
    64,
#else
    4,
#endif
    0,
    0,
    0,
    "Set the page size for system memory (in KB)"
);

static SProperty Platform_LwlinkForceAutoconfig
(
    Platform_Object,
    "LwlinkForceAutoconfig",
    0,
    false,
    0,
    0,
    0,
    "If true, tell RM to configure the lwlink connections; otherwise call chiplib to get the lwlink connections."
);

static SProperty Platform_MultiChipPaths
(
    Platform_Object,
    "MultiChipPaths",
    0,
    "",
    0,
    0,
    0,
    "Specify a colon-delimited list of chiplib paths."
);


static SProperty Platform_ChipLibDeepBind
(
    Platform_Object,
    "ChipLibDeepBind",
    0,
    false,
    0,
    0,
    0,
    "Deep bind the chip lib when loading"
);

static SProperty Platform_CrsTimeoutUs
(
    Platform_Object,
    "CrsTimeoutUs",
    0,
    90,
    0,
    0,
    0,
    "Set Reset CRS timeout to the specified value in microseconds."
);

static SProperty Platform_DelayAfterFlrUs
(
    Platform_Object,
    "DelayAfterFlrUs",
    0,
    180, // Delay twice the CRS timeout to be safe
    0,
    0,
    0,
    "Set delay time after FLR to the specified value in microseconds."
);
static SProperty Platform_EnableManualCaching
(
    Platform_Object,
    "EnableManualCaching",
    0,
    false,
    0,
    0,
    0,
    "Enable MODS to serialize ref manual file and reuse it to save manual loading time."
);

namespace Platform
{
    static RC VirtualToPhysicalInternal(PHYSADDR* pPhysAddr,
                                        const volatile void *VirtualAddress);
    static void HwMemCopy(volatile void *Dst, const volatile void *Src, size_t Count);

    static RC CreateCarveout(Carveout *c, UINT64 size, UINT64 align);
    static void DestroyCarveout(Carveout *c);

    static RC GetChipConnections(UINT32 gpuInst, UINT32 numLinks, UINT32 *pLinkArray, string connType);
}

// Chip library stuff
static bool s_Initialized;
static bool s_ForceChiplibSysmem;
static bool s_ChiplibSupportsExternalSysmem;
static bool s_ChiplibSupportsInterrupts;
static bool s_HybridEmulation = false;

// If true, disables attempts to access PCI cfg space (all functions will return FFs)
static bool s_StrictPciCfgSpace = true;

static IChip*               s_pChip             = 0;
static IMemory*             s_pMem              = 0;
static IBusMem*             s_pBusMem           = 0;
static IMapMemory*          s_pMapMem           = 0;
static IMapMemoryExt*       s_pMapMemExt        = 0;
static IMapDma*             s_pMapDma           = nullptr;
static IIo*                 s_pIo               = 0;
static IAModelBackdoor*     s_pAmod             = 0;
static IMemAlloc64*         s_pMemAlloc64       = 0;
static IInterruptMgr*       s_pInterruptMgr     = 0;
static IInterruptMgr2*      s_pInterruptMgr2    = 0;
static IInterruptMgr3*      s_pInterruptMgr3    = 0;
static IPciDev*             s_pPciDev           = 0;
static IPciDev2*            s_pPciDev2          = 0;
static IInterruptMask*      s_pInterruptMask    = 0;
static IPpc*                s_pPpc              = 0;

static char s_ModelIdentifier[4] = "";
static void *s_ChipSimModule;
static Tasker::ThreadID s_SimThreadID = Tasker::NULL_THREAD;
static Platform::SimulationMode s_SimulationMode = Platform::Hardware;
static bool s_TerminateSimThread;
static bool s_RemoteHardware = false;
static Xp::ClockManager* s_pClockManager = 0;

static vector<void*> s_DllModulesToUnloadOnExit;

typedef map<void*,void*> RemoteVirtual;
static RemoteVirtual s_RemoteVirtual;

typedef map<void*,void*> LocalVirtual;
static LocalVirtual s_LocalVirtual;

typedef map<void*,void*> AdjustedVirtual;
static AdjustedVirtual s_AdjustedVirtual;

// Simple chiplib implementation of the chipset
class Chipset: public IBusMem, public IInterrupt, public IInterrupt3
{
    // IIfaceObject implementation
    void AddRef();
    void Release();
    IIfaceObject *QueryIface(IID_TYPE id);

    // IBusMem implementation
    BusMemRet BusMemWrBlk(LwU064 address, const void *appdata, LwU032 count);
    BusMemRet BusMemRdBlk(LwU064 address, void *appdata, LwU032 count);
    BusMemRet BusMemCpBlk(LwU064 dest, LwU064 source, LwU032 count);
    BusMemRet BusMemSetBlk(LwU064 address, LwU032 size, void* data, LwU032 data_size);

    // IInterrupt implementation
    void HandleInterrupt();

    // IInterrupt3 implementation
    void HandleSpecificInterrupt(LwU032 irqNumber);
};
static Chipset s_Chipset;

static bool s_EnableManualCaching = false;

// Simulator system memory heap, shared between all devices
static bool s_PteReverse;
static bool s_PteRandom;
static PHYSADDR s_SimSysMemBase;
static PHYSADDR s_AllocatedSimSysMemBase = 0;
static UINT64 s_SimSysMemSize;
static Heap *s_SimSysHeap = NULL;
static Random s_SimSysHeapRandom;
static void* s_pReserveSysMem = 0;
static UINT32 s_ReservedSysmemMB = 0;

// Carveout for SOC
static Carveout s_Carveout  = {~0ULL, 0, NULL, false, false};

// VPR on SOC
static Carveout s_VPR  = {~0ULL, 0, NULL, false, false};

// General carveout on SOC (for WPR regions)
#define MAX_GEN_CARVEOUT 2
static UINT32   s_nC = 0;
static Carveout s_GenCarveout[MAX_GEN_CARVEOUT] = { {0, 0, NULL, false, false},
                                                    {0, 0, NULL, false, false} };
static UINT64 s_PagesPerAlloc = 1;
struct PageAllocDescriptor
{
    void *VirtualAddress;
    size_t NumBytes;
    bool Contiguous;
    bool Shared;
    void **RemoteVirtualAddresses;
    void *AddressSpace;
    size_t AddressSpaceBytes;
};

// CPU/GPU mailbox (reserved sysmem) mapping
static PageAllocDescriptor s_ReserveMapping = { };
static PageAllocDescriptor s_CarveoutReserveMapping = { };
static bool s_UnmapReserveMapping = false;
static bool s_UnmapCarveoutReserveMapping = false;
static UINT64 s_CarveoutUsedSize = 0;

//! Should gdb load sim symbols for stack dumping?
static bool s_LoadSimSymbols = true;
static bool s_DumpVirtualAccess = false;
static bool s_DumpPhysicalAccess = false;

static bool s_TrackAll = false;
static int s_TrackBAR = TRACK_PHYSICAL;
static PHYSADDR s_PhysicalAddress = 0;
static int s_TrackMode = PRINT;
static UINT64 s_TrackTime = 0;
static int s_TrackType = 3;
static bool s_TrackValueEnable = false;
static UINT32 s_TrackValue = 0;
static UINT32 s_CrsTimeoutUs = 90;
// Delay twice the CRS timeout to be safe
static UINT32 s_DelayAfterFlrUs = 180;

#ifdef DEBUG
#define STATIC_UNLESS_DEBUG
#else
#define STATIC_UNLESS_DEBUG static
#endif

// multi-gpu support
STATIC_UNLESS_DEBUG IGpuEscape * s_pChiplibGpuEscape;
STATIC_UNLESS_DEBUG IGpuEscape2* s_pChiplibGpuEscape2;

// multi-heap support
STATIC_UNLESS_DEBUG IMultiHeap * s_pChiplibMultiHeap;
STATIC_UNLESS_DEBUG IMultiHeap2 * s_pChiplibMultiHeap2;
STATIC_UNLESS_DEBUG bool  s_EnableSysmemMultiHeap;
STATIC_UNLESS_DEBUG bool  s_MultiHeapSupported;
STATIC_UNLESS_DEBUG UINT64 s_SimSysMemSizeUC;
STATIC_UNLESS_DEBUG UINT64 s_SimSysMemSizeWC;
STATIC_UNLESS_DEBUG UINT64 s_SimSysMemSizeWT;
STATIC_UNLESS_DEBUG UINT64 s_SimSysMemSizeWP;
STATIC_UNLESS_DEBUG UINT64 s_SimSysMemSizeWB;

// Interrupt state

typedef vector<UINT32> Irqs;

typedef struct
{
    Irqs     irqs;
    UINT32   flags;
} DevDesc;

typedef map<UINT32, DevDesc> DevMap;
static DevMap s_DevMap;

typedef vector<IsrData> IrqHooks;

typedef struct
{
    IrqHooks hooks;
    bool     disabled;
} IrqDesc;

typedef map<UINT32, IrqDesc > IrqMap;
static IrqMap s_IrqMap;
static queue<LwU032> s_PendingInterruptQueue;
static set<LwU032> s_PendingInterruptSet;

static bool s_InterruptsEnabled = true;
static LwU032 s_ProcessedInterruptCount = 0;

typedef struct
{
    UINT32   id;
    UINT16   vendorId;
    UINT16   deviceId;
    PHYSADDR bar0BaseAddress;
    UINT64   bar0Size;
} SkipDev;

static vector<SkipDev> s_SkipDevices;

static unique_ptr<SubstituteRegisters> s_SubstituteRegisters;

// Write combining state
#define WRITE_COMBINE_BUFFER_SIZE 256
static PHYSADDR s_WcBaseAddr;
static UINT32 s_WcSize;
static UINT08 s_WcBuffer[WRITE_COMBINE_BUFFER_SIZE];

// Other stuff
static map<void *, size_t> s_DeviceMappingSizes;
static bool s_LwlinkForceAutoconfig;

// 10 is arbitrary, don't want to flood the user's log if the chip is hung
#define TIMES_IDLE_THRESHOLD 10

RC LoadChiplib (const char * chiplibName, void **pmodule, IChip ** ppChip);

static PHYSADDR FixSysmemAddress(PHYSADDR Addr, UINT32*);

// This function pointer may be used as a callback to
// a function which takes care of unmapping memory mapped
// through MODS. This is used by LwWatch for simulation on Linux.
typedef void (*unmapFn_t)(void);
unmapFn_t unmapFn = NULL;

static void AddPendingInterrupt(LwU032 irqNumber)
{
    if (s_PendingInterruptSet.find(irqNumber) == s_PendingInterruptSet.end())
    {
        Printf(Tee::PriDebug, "Adding IRQ %d to pending interrupts queue\n", irqNumber);
        s_PendingInterruptQueue.push(irqNumber);
        s_PendingInterruptSet.insert(irqNumber);
    }
}

static void ServicePendingInterrupts()
{

    if (!s_InterruptsEnabled && !s_PendingInterruptQueue.empty())
    {
        Printf(Tee::PriDebug,
            "ServicePendingInterrupts have %ld pending interrupts but interrupts not enabled\n",
            s_PendingInterruptQueue.size());
    }

    while (s_InterruptsEnabled && !s_PendingInterruptQueue.empty())
    {
        LwU032 irqNumber = s_PendingInterruptQueue.front();

        // Mods must process interrupt before delete it from the queue.
        // Otherwise mods will receive same interrupt again from chiplib.
        // Make sure no transaction is issued between RmRearmMsi and IRQ
        // num removal. This is to avoid missing MSI/MSIX IRQ in fmod/RTL.
        // Platform::HandleInterrupt -> GpuSubdevice::ServiceInterrupt -> RmRearmMsi
        // Refer below case for MSIX IRQ missing:
        // Step1: HW trigger second MSIX IRQ with same "irqNumber" between RmRearmMsi and IRQ num removal.
        // Step2: Mods issue transaction between RmRearmMsi and IRQ num removal.
        // Step3: chiplib_f/chiplib_v report MSIX IRQ "irqNumber" to mods.
        // Step4: Mods drop the "irqNumber" because it is already in queue.
        // Step5: As the 2nd "irqNumber" is triggered after "RmRearmMsi", HW does not report it again.
        Platform::HandleInterrupt(irqNumber);
        s_PendingInterruptQueue.pop();
        s_PendingInterruptSet.erase(irqNumber);
        // allow any ISR threads a chance to run
        Tasker::Yield();
    }
}

inline UINT32 GetDeviceIrqId(UINT32 pciDomain, UINT32 pciBus, UINT32 pciDevice, UINT32 pciFunction)
{
    return Pci::GetConfigAddress(pciDomain, pciBus, pciDevice, pciFunction, 0);
}

///------------------------------------------------------------------------------
// If skipping some register acceses record any discovered Lwpu pci devices
// Optionally filtering out any devices which don't match a specific device id
//
static RC AddPCISkipDevice(UINT32 domain, UINT32 bus, UINT32 device, UINT32 function)
{
    RC rc;

    if (!s_SubstituteRegisters.get())
        return rc;

    UINT16 vendorId, deviceId;

    CHECK_RC(Platform::PciRead16(domain, bus, device, function, LW_PCI_VENDOR_ID, &vendorId));

    if (Pci::Lwpu != vendorId)
        return rc;

    CHECK_RC(Platform::PciRead16(domain, bus, device, function, LW_PCI_DEVICE_ID, &deviceId));

    UINT32 id = Pci::GetConfigAddress(domain, bus, device, function, 0);

    // Check if we already recorded this device
    for (SkipDev &d : s_SkipDevices)
        if (d.id == id)
            return rc;

    PHYSADDR bar0BaseAddress;
    UINT64   bar0Size;
    CHECK_RC(Platform::PciGetBarInfo(domain, bus, device, function, 0,
                                     &bar0BaseAddress, &bar0Size));

    SkipDev d = {id, vendorId, deviceId, bar0BaseAddress, bar0Size};

    s_SkipDevices.push_back(d);

    return rc;
}

static RC SubstitutePhysRd(PHYSADDR Addr, void *Data, UINT32 Count)
{
    if (s_SubstituteRegisters.get())
        for (SkipDev &d : s_SkipDevices)
            if ((d.bar0BaseAddress <= Addr) && (Addr < (d.bar0BaseAddress+d.bar0Size)))
                return s_SubstituteRegisters->RegRd(Addr-d.bar0BaseAddress, Data, Count);

    return RC::SOFTWARE_ERROR;
}

static RC SubstitutePhysWr(PHYSADDR Addr, const void *Data, UINT32 Count)
{
    if (s_SubstituteRegisters.get())
        for (SkipDev &d : s_SkipDevices)
            if ((d.bar0BaseAddress <= Addr) && (Addr < (d.bar0BaseAddress+d.bar0Size)))
                return s_SubstituteRegisters->RegWr(Addr-d.bar0BaseAddress, Data, Count);

    return RC::SOFTWARE_ERROR;
}

static void SimulationThread(void *)
{
    UINT32 Value = 0;
    UINT32 TimesInARowIdle = 0;
    bool IsIdle, IdleCheckSupported;

    // Some simulators support a query to determine when they are idle.  Useful
    // so that we don't uselessly clock them.
    Platform::EscapeRead(ESCAPE_SUPPORT_PREFIX "LWSIM_IDLECHECK", 0, 4, &Value);
    IdleCheckSupported = (Value == 1);
    if (IdleCheckSupported)
        Printf(Tee::PriDebug, "Simulator supports idle check\n");

    while (!s_TerminateSimThread)
    {
        // Let other threads execute
        Tasker::Yield();

        if (IdleCheckSupported)
        {
            // Ask whether idle or busy
            Platform::EscapeRead("LWSIM_IDLECHECK", 0, 4, &Value);
            IsIdle = (Value != 0);
        }
        else
        {
            // Assume busy
            IsIdle = false;
        }

        // Clock the simulator if it's not idle
        if (!IsIdle)
        {
            Platform::ClockSimulator(1);
            TimesInARowIdle = 0;
        }
        else
        {
            // Don't want to overflow TimesInARowIdle, we only care if it's been
            // idle TIMES_IDLE_THRESHOLD times in a row.  We incr it one past
            // this thresh so the checks below fail
            if(TimesInARowIdle <= TIMES_IDLE_THRESHOLD)
                ++TimesInARowIdle;

            if(TimesInARowIdle < TIMES_IDLE_THRESHOLD)
            {
                Printf(Tee::PriDebug, "SimulationThread - NOT CLOCKING SIMULATOR this time around:\n");
                Printf(Tee::PriDebug, "   it reported that it's idle\n");
            }
            else if(TimesInARowIdle == TIMES_IDLE_THRESHOLD)
            {
                Printf(Tee::PriDebug, "SimulationThread - NOT CLOCKING SIMULATOR for the %dth time in a row.\n",
                     TimesInARowIdle);
                Printf(Tee::PriDebug, "   WARNING: You won't see any more messages about not clocking simulator\n");
                Printf(Tee::PriDebug, "   WARNING: (unless it gets clocked and goes idle again)\n");
                Printf(Tee::PriDebug, "   WARNING: so we don't flood your log file\n");
            }

        }
    }
}

namespace
{
    bool s_InterruptThreadRunning = false;
    Tasker::ThreadID s_InterruptThreadID = Tasker::NULL_THREAD;

    static void InterruptThread(void*)
    {
        while (s_InterruptThreadRunning)
        {
            if (Platform::IsVirtFunMode())
            {
                // No need to create VmiopElwManager instance, just query here
                VmiopElwManager *pMgr = VmiopElwManager::GetInstance();
                if (pMgr)
                {
                    UINT32 gfid = pMgr->GetGfidAttachedToProcess();
                    VmiopElw* pVmiopElw = pMgr->GetVmiopElw(gfid);
                    if (pVmiopElw && !pVmiopElw->IsRemoteProcessRunning())
                    {
                        MASSERT(!"Remote PF process is down. The VF process can't survive without PF process.");
                    }
                }
            }

            if (s_pInterruptMgr)
                s_pInterruptMgr->PollInterrupts();

            ServicePendingInterrupts();

            Tasker::Yield();
        }
    }

    //Print call stack for the given address
    void ChecktoTrackAddress
    (
        const volatile void* Addr,
        bool isVirtual,
        const int Mode,
        const void* Data,
        UINT64 tempTrackTime
    );
    void AddressesCallStack
    (
        const volatile void* Addr,
        const char* Prefix,
        const void* Data
    );
    //used for getting time in nanoseconds for address tracking
    UINT64 GetTrackTimeNS();
    //Corrects the s_PhysicalAddress to offset based on BarN
    UINT64 SetupTracking();

    class SimClockManager: public Xp::ClockManager
    {
    public:
        SimClockManager(IClockMgr* pClockMgr)
        : m_pClockMgr(pClockMgr)
        {
            MASSERT(m_pClockMgr);
        }
        virtual ~SimClockManager()
        {
            m_pClockMgr->Release();
        }
        virtual RC GetResetHandle
        (
            const char* resetName,
            UINT64* pHandle
        )
        {
            //Not Supported, ignore
            MASSERT(0);
            return RC::UNSUPPORTED_FUNCTION;
        }
        virtual RC GetClockHandle
        (
            const char* device,
            const char* controller,
            UINT64* pHandle
        )
        {
            return m_pClockMgr->GetClockHandle(device, controller, pHandle)
                ? RC::ILWALID_CLOCK_DOMAIN : OK;
        }
        virtual RC EnableClock
        (
            UINT64 clockHandle
        )
        {
            return m_pClockMgr->SetClockEnabled(clockHandle, 1)
                ? RC::SOFTWARE_ERROR : OK;
        }
        virtual RC DisableClock
        (
            UINT64 clockHandle
        )
        {
            return m_pClockMgr->SetClockEnabled(clockHandle, 0)
                ? RC::SOFTWARE_ERROR : OK;
        }
        virtual RC GetClockEnabled
        (
            UINT64 clockHandle,
            UINT32 *pEnableCount
        )
        {
            return m_pClockMgr->GetClockEnabled(clockHandle, pEnableCount)
                ? RC::SOFTWARE_ERROR : OK;
        }
        virtual RC SetClockRate
        (
            UINT64 clockHandle,
            UINT64 rateHz
        )
        {
            return m_pClockMgr->SetClockRate(clockHandle, rateHz)
                ? RC::SOFTWARE_ERROR : OK;
        }
        virtual RC GetClockRate
        (
            UINT64 clockHandle,
            UINT64* pRateHz
        )
        {
            return m_pClockMgr->GetClockRate(clockHandle, pRateHz)
                ? RC::SOFTWARE_ERROR : OK;
        }
        virtual RC SetMaxClockRate
        (
            UINT64 clockHandle,
            UINT64 rateHz
        )
        {
            // Not supported, ignore
            return OK;
        }
        virtual RC GetMaxClockRate
        (
            UINT64 clockHandle,
            UINT64* pRateHz
        )
        {
            // Not supported, return current clock rate
            return GetClockRate(clockHandle, pRateHz);
        }
        virtual RC SetClockParent
        (
            UINT64 clockHandle,
            UINT64 parentClockHandle
        )
        {
            return m_pClockMgr->SetClockParent(clockHandle, parentClockHandle)
                ? RC::SOFTWARE_ERROR : OK;
        }
        virtual RC GetClockParent
        (
            UINT64 clockHandle,
            UINT64* pParentClockHandle
        )
        {
            return m_pClockMgr->GetClockParent(clockHandle, pParentClockHandle)
                ? RC::SOFTWARE_ERROR : OK;
        }
        virtual RC Reset
        (
            UINT64 handle,
            bool assert
        )
        {
            return m_pClockMgr->AssertClockReset(handle, assert ? 1 : 0)
                ? RC::SOFTWARE_ERROR : OK;
        }

    private:
        IClockMgr* m_pClockMgr;
    };

    // Takes a colon delimited list of chiplib paths
    // returns a vector containing the chip paths
    vector<string> ProcessMultiChipPaths(const string &multiChipPaths)
    {
        if (multiChipPaths.empty())
            return {};

        vector<string> chipPaths;
        // Split the input string on ":"
        stringstream ss(multiChipPaths);
        string chiplib;
        while (getline(ss, chiplib, ':')) {
            chipPaths.push_back(chiplib);
        }
        return chipPaths;
    }
}

//! \brief Passing sysmem allocation info to models via escapewrite
//-------------------------------------------------------------------------------
static void UpdateAllocationInfo
(
    PHYSADDR    PhysAddr,
    size_t      NumBytes,
    Memory::Attrib Attrib
)
{
    if (s_SimulationMode == Platform::RTL)
    {
        // FixSysmemAddress considers the difference between VF, PF and default modes
        UINT32 bdfAddr = 0;
        PHYSADDR fixedPhysAddr = FixSysmemAddress(PhysAddr, &bdfAddr);

        Platform::EscapeWrite("ALLOC_START_ADDR_HI", bdfAddr, 4, LwU64_HI32(fixedPhysAddr));
        Platform::EscapeWrite("ALLOC_START_ADDR_LO", bdfAddr, 4, LwU64_LO32(fixedPhysAddr));
        Platform::EscapeWrite("ALLOC_END_ADDR_HI", bdfAddr, 4, LwU64_HI32(fixedPhysAddr + NumBytes - 1));
        Platform::EscapeWrite("ALLOC_END_ADDR_LO", bdfAddr, 4, LwU64_LO32(fixedPhysAddr + NumBytes - 1));

        switch (Attrib)
        {
            case Memory::CA:
            case Memory::WB:
                Platform::EscapeWrite("COHERENT_REGION", bdfAddr, 4, 0);
                break;
            case Memory::UC:
                Platform::EscapeWrite("UNCACHED_REGION", bdfAddr, 4, 0);
                break;
            case Memory::WC:
                Platform::EscapeWrite("NONCOHERENT_REGION", bdfAddr, 4, 0);
                break;
            default:
                MASSERT(!"Unsupported sysmem type!\n");
        }
    }
}

static RC SendBdfToCpuModel(UINT32 bus, UINT32 device, UINT32 function, UINT32 gpuId)
{
    UINT32 data = 0;
    if ((Platform::GetSimulationMode() != Platform::Amodel) &&
        (0 == Platform::GpuEscapeRead(gpuId, "CPU_MODEL|CM_ENABLED", 0, 4, &data)))
    {
        if (data != 0)
        {
            string escapeString = Utility::StrPrintf("CPU_MODEL|CM_BDF|bus:=%u|device:=%u|function:=%u|gpuId:=%u",
                bus, device, function, gpuId);

            Printf(Tee::PriDebug, "CPU: %s\n", escapeString.c_str());
            Platform::GpuEscapeWrite(gpuId, escapeString.c_str(), 0, 4, 0);
        }
    }

    return OK;
}

static RC SendBdf(UINT32 domain, UINT32 bus, UINT32 device, UINT32 function)
{
    RC rc;

    UINT32 domainId = ~0;
    UINT32 busId = bus;
    UINT32 deviceId = device;
    UINT32 functionId = function;

    if (Platform::IsVirtFunMode())
    {
        VmiopElwManager * pMgr = VmiopElwManager::Instance();
        CHECK_RC(pMgr->CreateGfidToBdfMapping(domain, bus, device, function));
        UINT32 gfId = VmiopElwManager::Instance()->GetGfidAttachedToProcess();
        CHECK_RC(pMgr->GetDomainBusDevFun(gfId, &domainId, &busId, &deviceId, &functionId));
    }

    // Bug 3026626
    // FIX-ME: hack gpuId with 0 now, need fix gpuId in multi-gpu cases.
    // Consider to pass device instance instead.
    CHECK_RC(SendBdfToCpuModel(busId, deviceId, functionId, 0));

    return rc;
}

static vector<const char*> g_ChipLibArgV;
static string g_ChipArgsCpy;

//
// Helper function for Platform::Initialize(), to set up SockCmdLine.
//
static RC parseSockCmdLine(string *pSockCmdLine, string *pChipArgs, bool *pWaitForSocket)
{
    // Note: there are two mechanisms to specify a socket, either -socket or -sessionfile.
    //       If -sockCmdLine is given, figure out the sockserv command, but treat separately
    //       -socket versus -sessionfile.

    // colwenience
    string& SockCmdLine = *pSockCmdLine;
    string& ChipArgs = *pChipArgs;

    // note the embedded space in the string, it is the arg delimiter
    static const string socketStr("-socket ");
    static const string sessionStr("-sessionfile ");

    // the counts don't include the embedded space (the count is used to skip to the next white space)
    static const size_t socketSkipCount = socketStr.length() - 1;
    static const size_t sessionSkipCount = sessionStr.length() - 1;

    // persistent variables
    static UINT32 SockNumToUse = 0;
    static string origSockNumStr;
    static size_t origSockNumStrPos;
    static size_t origChipargsNumStrPos;
    static bool   argIsSocket;

    // Note: SockNumToUse is overloaded with session ID when using -sessionfile instead of -socket.
    if (SockNumToUse == 0)
    {
        //
        // Parse SockCmdLine and extract the socket no. (or session ID) for sockserver
        // only once, i.e. the first time we're in this function.
        //
        // Let's first extract the socket/session no. for sockserver from the cmd line.
        size_t pos; // position offset within string
        {
            const size_t posSocket = SockCmdLine.find(socketStr, 0);
            const size_t posSession = SockCmdLine.find(sessionStr, 0);
            if ((string::npos == posSocket && string::npos == posSession) || (string::npos != posSocket && string::npos != posSession))
            {
                // -socket/-sessionfile specification error in cmd line.
                Printf(Tee::PriHigh,
                       "Incorrect format for SockCmdLine (missing -socket or -sessionfile, or both supplied)\n");
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }

            // We need to remember this across calls.
            argIsSocket = (string::npos == posSession);

            pos = (argIsSocket ? posSocket : posSession);
        }

         // find session filename pos/len to compare against ChipArgs
        size_t sessionFilePos = 0, sessionFileLen = 0;
        if (argIsSocket)
        {
            pos += socketSkipCount; // get past -socket
        }
        else
        {
            pos += sessionSkipCount; // get past -sessionfile

            // get past whitespace (to the session filename)
            while (SockCmdLine[pos] == ' ')
            {
              pos++;
            }
            sessionFilePos = pos;

            // get past session filename (and get to the session ID)
            while(pos < SockCmdLine.length() && SockCmdLine[pos] != ' ')
            {
              pos++;
            }
            sessionFileLen = pos - sessionFilePos;
        }

        // Get past whitespace to socket number or session ID
        while (SockCmdLine[pos] == ' ')
        {
            pos++;
        }

        // Count total socket num characters until whitespace is found or end of string
        UINT32 subStrlength = 0;
        while(pos + subStrlength < SockCmdLine.length() && SockCmdLine[pos+subStrlength] != ' ')
        {
            subStrlength++;
        }

        // Extract socket no. from position 'pos' and length 'subStrlength'
        origSockNumStrPos = pos;
        origSockNumStr = SockCmdLine.substr(pos, subStrlength);

        // Colwert original socket no from string to integer and save in 'SockNumToUse'
        SockNumToUse = strtoul(origSockNumStr.c_str(), NULL, 0);

        //
        // if SockNumToUse is still 0, either the user has indeed specified
        // socket 0 or the SockCmdLine is in incorrect format, so flag error
        //
        if (SockNumToUse == 0)
        {
            Printf(Tee::PriHigh, "Illegal Socket no. (or session ID) in sockserv cmd line\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        // There is also a corresponding chiparg that needs to be tracked.
        const string& sockArgStr = (argIsSocket ? socketStr : sessionStr);
        const size_t skipCount = (argIsSocket ? socketSkipCount: sessionSkipCount);
        pos = ChipArgs.find(sockArgStr , 0);
        if (pos == string::npos)
        {
            Printf(Tee::PriHigh, "ChipArgs must match the socket/sessionfile of SockCmdLine\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        pos += skipCount; // skip "-socket/-sessionfile"

        // skip whitespace (to hostname/filename)
        while (ChipArgs[pos] == ' ')
        {
            pos++;
        }

        // sanity check that ChipArgs has the same sessionfile name as SockCmdLine
        if (!argIsSocket)
        {
            if (sessionFileLen == 0 || pos + sessionFileLen > ChipArgs.length() ||
                SockCmdLine.substr(sessionFilePos, sessionFileLen) != ChipArgs.substr(pos, sessionFileLen) ||
                (pos + sessionFileLen + 1 <= ChipArgs.length() &&
                 SockCmdLine.substr(sessionFilePos, sessionFileLen+1) != ChipArgs.substr(pos, sessionFileLen+1)))
            {
                Printf(Tee::PriHigh,
                       "Session filename from SockCmdLine does not match with session filename in chipargs argument\n");
                Printf(Tee::PriHigh, "SockCmdSubstr=%s, ChipArgs.substr=%s\n",
                       SockCmdLine.substr(sessionFilePos, sessionFileLen).c_str(),
                       ChipArgs.substr(pos, sessionFileLen + (pos + sessionFileLen + 1 <= ChipArgs.length() ? 1 : 0)).c_str());
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }

        // skip hostname/filename (to whitespace separator)
        while (pos < ChipArgs.length() && ChipArgs[pos] != ' ')
        {
            pos++;
        }

        // skip whitespace (to socket/session number)
        while (ChipArgs[pos] == ' ')
        {
            pos++;
        }

        // This is the position in ChipArgs to replace the socket/session number.
        origChipargsNumStrPos = pos;

        // identify the rest of the substring, go to next whitespace
        while (pos < ChipArgs.length() && ChipArgs[pos] != ' ')
        {
            pos++;
        }

        // the socket/session number must match origSockNumStr
        if (origSockNumStr != ChipArgs.substr(origChipargsNumStrPos, pos-origChipargsNumStrPos))
        {
            // socket no which was present in sockserver
            // cmd line not found in ChipArgs, so return error
            Printf(Tee::PriHigh,
                   "Socket/session no from SockCmdLine does not match with socket no in chipargs argument\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    //
    // Colwert SockNumToUse to string, and increment SockNumToUse so
    // we will connect to a different socket next time
    //
    char lwrSockNumStr[20];
    sprintf(lwrSockNumStr,"%u",SockNumToUse++);

    // Now replace the original socket no. with current socket no. in SockCmdLine
    SockCmdLine.replace(origSockNumStrPos, origSockNumStr.length(), lwrSockNumStr);

    // Add a '&' to run this in background and return immediately to mods
    SockCmdLine.insert(SockCmdLine.length(), " &");

    // Now replace the original socket no. in ChipArgs with the current socket no.
    ChipArgs.replace(origChipargsNumStrPos, origSockNumStr.length(), lwrSockNumStr);

    // When using -socket, we have to wait for the socket to be initialized.
    *pWaitForSocket = argIsSocket;

    return OK;
}

static UINT32 GetSimSysmemBdfShift()
{
    static bool bInitialized = false;
    static UINT32 simSysmemBdfShift = 48;
    if (!bInitialized)
    {
        MASSERT(s_pChip);
        if (!Platform::IsDefaultMode() &&
            (s_SimulationMode != Platform::Hardware || s_HybridEmulation))
        {
            UINT32 bdfShift = 0;
            if (0 == s_pChip->EscapeRead(const_cast<char*>("sriov_BDFShiftSizeInSysmemAddr"),
                    0, sizeof(bdfShift), &bdfShift))
            {
                // Bug 2974459
                // Before Hopper, sriov_BDFShiftSizeInSysmemAddr is expected to be 48 that
                // mods append 16 bits BDF(bus, device, function) to PA.
                // After Hopper, sriov_BDFShiftSizeInSysmemAddr is 56(PCIE fullchip tests) that
                // mods only append 8 bits BDF(function) to support 52 PA + SRIOV case, except for
                // Some high speed hub tests which requests 12 bits BDF(4 bits bus, 8 bits function)
                // that sriov_BDFShiftSizeInSysmemAddr would be 52(lwlink fullchip tests).
                simSysmemBdfShift = bdfShift;
            }
        }
        bInitialized = true;
    }

    return simSysmemBdfShift;
}

static string GetSessionFileName(UINT32 physGpuIndex)
{
    return Utility::StrPrintf("%s%s%d%s",
        Platform::IsVirtFunMode()? "../" : "",
        "sockserv_session",
        physGpuIndex,
        ".txt");
}

static RC GenerateSockchipSessionId(UINT32* pSessionId)
{
    RC rc;

    if (Platform::IsVirtFunMode())
    {
        // Read SessionId from session file created by PF mods process
        UINT32 physGpuIndex = 0; // should get it by parsing GFID
        string sessionFileName = GetSessionFileName(physGpuIndex);
        FileHolder file;
        CHECK_RC_MSG(file.Open(sessionFileName, "r"),
            "%s: Can't open file %s\n",
            __FUNCTION__, sessionFileName.c_str());

        long fileSize = 0;
        CHECK_RC(Utility::FileSize(file.GetFile(), &fileSize));
        vector<char> buffer(fileSize+1);
        char* line = &buffer[0];

        UINT32 sessionId = -1;
        while(fgets(line, (UINT32)buffer.size(), file.GetFile()) && !feof(file.GetFile()))
        {
            const char* key = "session=";
            if (!strncmp(line, key, strlen(key)))
            {
                string format = Utility::StrPrintf("%s%%d", key);
                if (1 != sscanf(line, format.c_str(), &sessionId))
                {
                    Printf(Tee::PriError, "%s: Could not parse file %s, session line \"%s\"\n",
                        __FUNCTION__, sessionFileName.c_str(), line);
                    return RC::CANNOT_OPEN_FILE;
                }

                Printf(Tee::PriDebug, "%s: Parse session line \"%s\". SessionId = %d\n",
                       __FUNCTION__, &buffer[0], sessionId);
                break;
            }
        }

        *pSessionId = sessionId;
        return rc;
    }

    // PF mods process
    // Create a random session ID
    Random RandomForSession;
    RandomForSession.SeedRandom((UINT32)Xp::QueryPerformanceCounter());

    const UINT32 randomBits = 16;
    *pSessionId = (Xp::GetPid() << randomBits) |
                  (RandomForSession.GetRandom(1, (1 << randomBits) - 1));
    return rc;
}

static const UINT32 LWLink_Max_Link_Count = 32;

enum LWLAddrTarget_e {
    LWLAddrTarget_Peer0 = 0x0,
    LWLAddrTarget_Peer1 = 0x1,
    LWLAddrTarget_Peer2 = 0x2,
    LWLAddrTarget_Peer3 = 0x3,
    LWLAddrTarget_Peer4 = 0x4,
    LWLAddrTarget_Peer5 = 0x5,
    LWLAddrTarget_Peer6 = 0x6,
    LWLAddrTarget_Peer7 = 0x7,
    LWLAddrTarget_Local = 0x8,
    LWLAddrTarget_Sysmem = 0x9,
    LWLAddrTarget_Total = 0xa,
    LWLAddrTarget_None = 0xb
};

static UINT32 enabledLWLinkMask = 0;
static LWLAddrTarget_e lwlink_conn[LWLink_Max_Link_Count][LWLAddrTarget_Total];
static UINT32 l2p_link[LWLink_Max_Link_Count];

static void ParseLwlinkConn(const char* str)
{
    if (!std::isdigit(static_cast<unsigned char>(str[7]))) {
        return;
    }
    char conn_state[80];
    UINT32 lwlink;
    auto set_phys_link = false;
    auto items = sscanf(str, "-lwlink%u=%s", &lwlink, conn_state);

    if (lwlink >= LWLink_Max_Link_Count) {
        Printf(Tee::PriError, "got chiparg: %s, since lwlink# exceeds max lwlink count %u\n",
            str, LWLink_Max_Link_Count);
        return;
    }
    if (items == 2) {
        char* states = strtok(&conn_state[0], "|");
        UINT32 target_idx = 0;
        bool link_disabled = true;
        do {
            if (strncmp(states, "DISABLED", 8) == 0) {
                lwlink_conn[lwlink][target_idx] = LWLAddrTarget_None;
                target_idx++;
            } else if (strncmp(states, "CPU", 3) == 0) {
                lwlink_conn[lwlink][target_idx] = LWLAddrTarget_Sysmem;
                link_disabled = false;
                target_idx++;
                enabledLWLinkMask |= 1 << lwlink;
            } else if (strncmp(states, "PEER", 4) == 0) {
                auto peer_num = strtoul(&states[4], 0, 0);
                if (peer_num >= 8)
                {
                    Printf(Tee::PriError, "num peers should be less than 8 got %lu\n", peer_num);
                    return;
                }
                lwlink_conn[lwlink][target_idx] = static_cast< LWLAddrTarget_e >(LWLAddrTarget_Peer0 + peer_num);
                link_disabled = false;
                target_idx++;
                enabledLWLinkMask |= 1 << lwlink;
            } else if (strncmp(states, "PhysLink", 8) == 0) {
                auto phys_link = strtoul(&states[8], 0, 0);

                if (phys_link >= LWLink_Max_Link_Count)
                {
                    Printf(Tee::PriError, "Physical link# should less than %d got %lu\n", LWLink_Max_Link_Count, phys_link);
                    return;
                }
                // Set physical link to 0xf if the link is disabled
                l2p_link[lwlink] = link_disabled ? 0xf : phys_link;
                set_phys_link = true;
            }
            states = strtok(0, "|");
        } while (states != NULL);
        if (set_phys_link == false) {
            l2p_link[lwlink] = link_disabled ? 0xf : lwlink;
        }
    }
}

//------------------------------------------------------------------------------
RC Platform::Initialize()
{
    JavaScriptPtr pJs;
    RC rc;
    string ChipPath;
    bool OverrideSysMemBase, NoBackdoor;
    UINT32 PteSeed;
    string ChipArgs;
    string FloorsweepArgs;
    string SockCmdLine;
    bool ForceSockserv = false;
    bool TraceSockserv = false;
    bool TraceSockchip = false;
    string MultiChipPaths;
    UINT32 systemCarveoutSizeMb;
    UINT64 totalCarveoutSize = 0;
    bool allocateEBridgeMailbox = false;
    const UINT32 EBridgeMailboxSizeBytes = 1024 * 1024;
    bool SimClk;
    bool SimClkMode;
    string SimClkProfile;
    UINT32 SimClkThreshhold;
    UINT64 SimPageSize;

    UINT32 PhysicalAddressInt;

    const UINT64 oneMb = 1024U *1024U;
    const UINT64 oneKb = 1024U;

    if(s_Initialized)
    {
        Printf(Tee::PriLow, "Platform already initialized\n");
        return OK;
    }

    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_SysMemPageSize, &SimPageSize));
    SimPageTable::InitializeSimPageSize(SimPageSize);
    s_PagesPerAlloc = (SimPageTable::SimPageSize >= 0x400000) ? 1 : 0x400000 / SimPageTable::SimPageSize;

    bool enableChiplibTraceDump = false;
    bool enableRawChiplibTraceDump = false;
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_ChiplibTraceDump, &enableChiplibTraceDump));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_RawChiplibTraceDump, &enableRawChiplibTraceDump));
    if (enableChiplibTraceDump)
    {
        bool non_compressed = false;
        CHECK_RC(pJs->GetProperty(Platform_Object, Platform_NoCompressCapture, &non_compressed));

        ChiplibTraceDumper::GetPtr()->SetDumpChiplibTrace();
        ChiplibTraceDumper::GetPtr()->SetDumpCompressed(!non_compressed);
        ChiplibTraceDumper::GetPtr()->EnableChiplibTraceDump();

        bool enableBackdoorPramin = false;
        CHECK_RC(pJs->GetProperty(Platform_Object, Platform_BackDoorPramin, &enableBackdoorPramin));
        if(enableBackdoorPramin)
        {
            ChiplibTraceDumper::GetPtr()->EnablePramin();
        }

        std::string ignoredRegFile;
        CHECK_RC(pJs->GetProperty(Platform_Object, Platform_IgnoredRegFile, &ignoredRegFile));
        if(!ignoredRegFile.empty())
        {
            ChiplibTraceDumper::GetPtr()->SetIgnoredRegFile(ignoredRegFile);
        }
    }
    if (enableRawChiplibTraceDump)
    {
        ChiplibTraceDumper::GetPtr()->SetRawDumpChiplibTrace();

        UINT32 level = 0;
        CHECK_RC(pJs->GetProperty(Platform_Object, Platform_RawChiplibTraceDumpLevel, &level));
        ChiplibTraceDumper::GetPtr()->SetRawDumpChiplibTraceLevel(level);

    }

    std::string skippedRegFile;
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_SkippedRegFile, &skippedRegFile));
    if (!skippedRegFile.empty())
        s_SubstituteRegisters = make_unique<SubstituteRegisters>(skippedRegFile);

    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_SimClk, &SimClk));
    if(SimClk)
    {
        CHECK_RC(pJs->GetProperty(Platform_Object, Platform_SimClkMode, &SimClkMode));
        CHECK_RC(pJs->GetProperty(Platform_Object, Platform_SimClkProfile, &SimClkProfile));
        CHECK_RC(pJs->GetProperty(Platform_Object, Platform_SimClkThreshhold, &SimClkThreshhold));
        SimClk::SimManager::Init(SimClkMode, SimClkProfile, SimClkThreshhold);
    }
    SimClk::EventWrapper temp(SimClk::Event::PF_INIT);

    if (0 == SimCallMutex::GetMutex())
    {
        SimCallMutex::SetMutex(Tasker::AllocMutex("SimCallMutex",
                                                  Tasker::mtxUnchecked));
    }

    // Get Platform JavaScript properties
    bool SwapEndian;
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_SwapEndian, &SwapEndian));
    MASSERT(!SwapEndian); // XXX endian swap is no longer supported -- rip me out!
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_ForceChiplibSysmem, &s_ForceChiplibSysmem));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_PteReverse, &s_PteReverse));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_PteRandom, &s_PteRandom));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_PteSeed, &PteSeed));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_OverrideSysMemBase, &OverrideSysMemBase));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_SysMemBase, &s_SimSysMemBase));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_SysMemSize, &s_SimSysMemSize));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_EnableSysmemMultiHeap, &s_EnableSysmemMultiHeap));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_SysMemSizeUC, &s_SimSysMemSizeUC));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_SysMemSizeWC, &s_SimSysMemSizeWC));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_SysMemSizeWT, &s_SimSysMemSizeWT));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_SysMemSizeWP, &s_SimSysMemSizeWP));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_SysMemSizeWB, &s_SimSysMemSizeWB));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_LoadSimSymbols, &s_LoadSimSymbols));

    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_NoBackdoor, &NoBackdoor));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_ChipArgs, &ChipArgs));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_FloorsweepArgs, &FloorsweepArgs));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_DumpVirtualAccess, &s_DumpVirtualAccess));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_DumpPhysicalAccess, &s_DumpPhysicalAccess));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_SockCmdLine, &SockCmdLine));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_ForceSockserv, &ForceSockserv));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_TraceSockserv, &TraceSockserv));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_TraceSockchip, &TraceSockchip));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_MultiChipPaths, &MultiChipPaths));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_ReserveSysmemMB, &s_ReservedSysmemMB));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_CarveoutSizeMb, &systemCarveoutSizeMb));

    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_TrackAll, &s_TrackAll));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_TrackMode, &s_TrackMode));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_TrackType, &s_TrackType));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_TrackValueEnable, &s_TrackValueEnable));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_TrackValue, &s_TrackValue));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_TrackPhysicalAddress, &PhysicalAddressInt));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_TrackBAR, &s_TrackBAR));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_LwlinkForceAutoconfig,
        &s_LwlinkForceAutoconfig));
    if(!s_TrackAll)
        s_PhysicalAddress = (PHYSADDR)PhysicalAddressInt;

    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_CrsTimeoutUs, &s_CrsTimeoutUs));
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_DelayAfterFlrUs, &s_DelayAfterFlrUs));

    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_EnableManualCaching, &s_EnableManualCaching));

    // if a sockserv cmdline is specified, we should load the sockserver here
    if(!SockCmdLine.empty())
    {
        //
        // The cmd line can instruct MODS to also load sockserver for fmodel by
        // adding args -sockCmdLine "<path to sockserv> -socket <socket no>"
        // We will read the socket no, for e.g. 1234, and increment it each time
        // we're in this function (We can come here multiple times, if a
        // suspend-resume is attempted, which calls Reset() and then
        // Initialize() is called). Incrementing the socket no. will ensure
        // we'll always connect with a different socket no. This is done because
        // sometimes retrying the connection immediately after closing the
        // connection with the same socket no. might result in a failure.
        //

        // Support sockserv only on Linux fmodel
        if (Xp::GetOperatingSystem() != Xp::OS_LINUXSIM && Xp::GetOperatingSystem() != Xp::OS_WINSIM)
        {
            Printf(Tee::PriHigh,
                   "Sockserv support only available on Windows & Linux simulator\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        // Parse SockCmdLine, extract the socket number or session ID, modify it.
        // Also do the same for ChipArgs, and sanity check the ChipArgs against SockCmdLine.
        bool waitForSocket = false;
        CHECK_RC(parseSockCmdLine(&SockCmdLine, &ChipArgs, &waitForSocket));

        // Try starting the sockserv
        if (system(SockCmdLine.c_str()) != 0)
        {
            // if sockserv couldn't start, return error
            Printf(Tee::PriHigh, "Error running sockserv\n");
            return RC::DLL_LOAD_FAILED;
        }
        else
        {
            Printf(Tee::PriHigh, "Loaded sockserv\n");
        }

        // Wait for sometime for sockserver to be initialized
        if (waitForSocket)
        {
            Tasker::Sleep(2000);
        }
    }

    vector<string> chipPaths;
    bool useSockserv = false;
    Xp::ExelwtableType chipLibType = Xp::EXE_TYPE_64;

    // Path to the chiplib comes from -chip in the command line. For
    // multi-chip simulations using sockserv, paths to the multiple
    // chiplibs are passed in a colon-delimited string with
    // -multichip_paths. If this is present, ignore -chip.
    if (!MultiChipPaths.empty())
    {
        chipPaths = ProcessMultiChipPaths(MultiChipPaths);
        useSockserv = true;
    }
    else
    {
        ChipPath = CommandLine::GpuChipLib();
        if (!ChipPath.empty())
        {
            chipPaths.push_back(ChipPath);

            // Detect if there is a 32/64 mismatch between MODS and chiplib
            // Use sockserv/sockchip in that case
            chipLibType = Xp::GetExelwtableType(ChipPath.c_str());

            if ( Xp::EXE_TYPE_UNKNOWN == chipLibType )
            {
                Printf(Tee::PriNormal, "Could not determine exelwtable type for chiplib: '%s'!\n",
                        ChipPath.c_str());
                return RC::SOFTWARE_ERROR;
            }

            const UINT32 MODSExeType = (sizeof(size_t) == 4) ? 32 : 64;

            if ( ( (chipLibType == Xp::EXE_TYPE_32) && (MODSExeType == 64) ) ||
                 ( (chipLibType == Xp::EXE_TYPE_64) && (MODSExeType == 32) ) )
            {
                useSockserv = true;
            }
        }
    }

    if (chipPaths.empty())
    {
        // The user didn't ask to load a simulator.  Run on hardware.
        s_Initialized = true;
        return OK;
    }

    if ( Platform::IsVirtFunMode() ||
         Platform::IsPhysFunMode() ||
         ForceSockserv ||
         useSockserv )
    {
        UINT32 sockchipSessionID = -1;
        if (GenerateSockchipSessionId(&sockchipSessionID) != OK)
        {
            Printf(Tee::PriHigh, "Error to find socket connection session ID\n");
            return RC::SOFTWARE_ERROR;
        }

        if (Platform::IsVirtFunMode() ||
            Platform::IsPhysFunMode())
        {
            Printf(Tee::PriWarn, "Running with SR-IOV enabled forcing the chiplib to manage system memory\n");
            s_ForceChiplibSysmem = 1;
        }

        if (Platform::IsPhysFunMode())
        {
            CHECK_RC(VmiopElwManager::Instance()->Init());
        }

        string sockchip_argument;

        if (Platform::IsDefaultMode() || Platform::IsPhysFunMode())
        {
            const char *SockservFileName =
                (chipLibType == Xp::EXE_TYPE_64) ?
                ((Xp::GetOperatingSystem() == Xp::OS_WINSIM) ? "sockserv64.exe" :"sockserv64") :
                ((Xp::GetOperatingSystem() == Xp::OS_WINSIM) ? "sockserv32.exe" :"sockserv32");

            string SockservFileNameWithPath = Utility::DefaultFindFile(SockservFileName, true);
            if (!Xp::DoesFileExist(SockservFileNameWithPath))
            {
                Printf(Tee::PriHigh, "Error finding %s exelwtable\n", SockservFileName);
                return RC::DLL_LOAD_FAILED;
            }

            for (unsigned int children = 0; children < chipPaths.size(); ++children)
            {
                string sockserv_debug = Xp::GetElw("MODS_SOCKSERV_DEBUG");

                if (!sockserv_debug.empty())
                {
                    sockserv_debug += " ";
                }

                auto sockserv_cmd = Utility::StrPrintf(
                        "%s%s%s%s -chip %s -sessionfile %s %d %s",
                        (Xp::GetOperatingSystem() == Xp::OS_WINSIM) ? "start /B " : "",
                        sockserv_debug.c_str(),
                        SockservFileNameWithPath.c_str(),
                        TraceSockserv ? " -verbose" : "",
                        chipPaths[children].c_str(),
                        GetSessionFileName(children).c_str(),
                        sockchipSessionID + children,
                        (Xp::GetOperatingSystem() == Xp::OS_LINUXSIM) ? "&" : "");

                Printf(Tee::PriWarn, "Starting sockserv with command \"%s\"\n", sockserv_cmd.c_str());

                // Try starting the sockserv:
                if (system(sockserv_cmd.c_str()) != 0)
                {
                    // if sockserv couldn't start, return error
                    Printf(Tee::PriError, "Error starting sockserv with command \"%s\"\n",
                            sockserv_cmd.c_str());
                    return RC::DLL_LOAD_FAILED;
                }

                sockchip_argument = Utility::StrPrintf(
                    "%s -sessionfile %s %d",
                    sockchip_argument.c_str(),
                    GetSessionFileName(children).c_str(),
                    sockchipSessionID + children);
            }
        }
        else
        {
            UINT32 physGpuIndex = 0; // should get it by parsing GFID
            sockchip_argument = Utility::StrPrintf(
                "%s -sessionfile %s %d",
                sockchip_argument.c_str(),
                GetSessionFileName(physGpuIndex).c_str(),
                sockchipSessionID);
        }

        if (Platform::IsPhysFunMode())
        {
            if (ChipArgs.find("sriov_vf_count") == string::npos)
            {
                ChipArgs += Utility::StrPrintf(" -sriov_vf_count=%d", VmiopElwManager::Instance()->GetVfProcessCount());
            }
            ChipArgs += " -sriov_vf_enable";
        }
        ChipArgs += sockchip_argument;
        ChipPath = "sockchip";
    }

    // Load the chip library DLL
    CHECK_RC(LoadChiplib(ChipPath.c_str(), &s_ChipSimModule, &s_pChip));

    // Add floorsweep args to chipargs if specified. And, do this only for dGPUs.
    if (!FloorsweepArgs.empty() && !IsTegra())
    {
        ChipArgs += Utility::StrPrintf(" -floorsweep=%s", FloorsweepArgs.c_str());
    }

    // Multiple inheritance ambiguity requires that we provide a cast
    IIfaceObject *pChipset = static_cast<IBusMem *>(&s_Chipset);

    // Construct the simulator arguments.  Blank argument is for to emulate
    // argv's first argument being an exelwtable name.
    g_ChipLibArgV.clear();
    g_ChipLibArgV.push_back(" ");

    string socket = CommandLine::GpuRTLArgs();
    socket = socket.substr(socket.find(' ') + 1);
    g_ChipLibArgV.push_back("-info");
    if (!socket.empty())
    {
        g_ChipLibArgV.push_back("-tstStream");
        g_ChipLibArgV.push_back(socket.c_str());
    }
    if (TraceSockchip)
    {
        g_ChipLibArgV.push_back("-trace_sockchip");
    }
    if (ChipArgs != "")
    {
        g_ChipArgsCpy = ChipArgs;
        for (char *ChipArgPtr = &g_ChipArgsCpy[0];;)
        {
            while (isspace(*ChipArgPtr))
                ChipArgPtr++;
            if (!(*ChipArgPtr))
                break;

            if (*ChipArgPtr == '\'')
            {
                ChipArgPtr++;
                g_ChipLibArgV.push_back(ChipArgPtr);
                ChipArgPtr = strchr(ChipArgPtr, '\'');
                if (!ChipArgPtr)
                {
                    Printf(Tee::PriHigh,
                        "No terminating single quote found in Platform.ChipArgs\n");
                    break;
                }
            }
            else
            {
                g_ChipLibArgV.push_back(ChipArgPtr);
                ChipArgPtr = strchr(ChipArgPtr, ' ');
                if (!ChipArgPtr)
                    break;
            }
            *ChipArgPtr++ = '\0';
        }
    }

    // check if we have option to setup physical ebridge mailbox

    for (vector<const char*>::iterator it=g_ChipLibArgV.begin(); it < g_ChipLibArgV.end(); it++ )
    {
        if ((strncmp(*it, "-physEbridgeRTL", 15) == 0) || (strncmp(*it, "'-physEbridgeRTL'", 17) == 0))
        {
            allocateEBridgeMailbox = true;
        } else if (strncmp(*it, "-lwlink", 7) == 0) {
            ParseLwlinkConn(*it);
        }
    }

    // IChip::Startup must be called FIRST if we want to be able to run over a socket.
    if (s_pChip->Startup(pChipset, const_cast<char**>(&g_ChipLibArgV[0]),
        static_cast<int>(g_ChipLibArgV.size())))
    {
        Printf(Tee::PriHigh, "Chip init failed\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }

    // Get the chip library and amodel interfaces
    s_pMem = (IMemory *)s_pChip->QueryIface(IID_MEMORY_IFACE);
    s_pBusMem = (IBusMem *)s_pChip->QueryIface(IID_BUSMEM_IFACE);
    s_pMemAlloc64 = (IMemAlloc64 *)s_pChip->QueryIface(IID_MEMALLOC64_IFACE);
    if (!s_pMem && !s_pBusMem)
    {
        Printf(Tee::PriHigh, "Could not obtain IMemory (iid=%d) or IBusMem (iid=%d) interface\n",
            IID_MEMORY_IFACE, IID_BUSMEM_IFACE);
        return RC::SIM_IFACE_NOT_FOUND;
    }

    s_pIo = (IIo *)s_pChip->QueryIface(IID_IO_IFACE);
    if (!s_pIo)
    {
        Printf(Tee::PriHigh, "Could not obtain IIo (iid=%d) interface from amodel\n", IID_IO_IFACE);
        return RC::SIM_IFACE_NOT_FOUND;
    }

    s_pInterruptMgr = static_cast<IInterruptMgr*>(
            s_pChip->QueryIface(IID_INTERRUPT_MGR_IFACE));

    s_pInterruptMgr2 = static_cast<IInterruptMgr2*>(
            s_pChip->QueryIface(IID_INTERRUPT_MGR2_IFACE));

    s_pInterruptMgr3 = static_cast<IInterruptMgr3*>(
            s_pChip->QueryIface(IID_INTERRUPT_MGR3_IFACE));

    s_pInterruptMask = static_cast<IInterruptMask*>(
            s_pChip->QueryIface(IID_INTERRUPT_MASK_IFACE));

    IClockMgr* pClockMgr = static_cast<IClockMgr*>(
            s_pChip->QueryIface(IID_CLOCK_MGR_IFACE));
    if (pClockMgr)
    {
        s_pClockManager = new SimClockManager(pClockMgr);
    }

    s_pPciDev = static_cast<IPciDev*>(s_pChip->QueryIface(IID_PCI_DEV_IFACE));
    s_pPciDev2 = static_cast<IPciDev2*>(s_pChip->QueryIface(IID_PCI_DEV2_IFACE));
    s_pPpc = static_cast<IPpc*>(s_pChip->QueryIface(IID_PPC_IFACE));

    if (IsAmodel())
    {
        if (!s_pBusMem)
        {
            Printf(Tee::PriHigh, "Could not obtain IBusMem (iid=%d) interface from amodel\n",
                IID_BUSMEM_IFACE);
            return RC::SIM_IFACE_NOT_FOUND;
        }
    }

    // Detect RTL vs. fmodel vs. hardware
    IChip::ELEVEL SimulationType = s_pChip->GetChipLevel();

    MASSERT(SimulationType != IChip::ELEVEL_UNKNOWN);

    if ((SimulationType == IChip::ELEVEL_HW) &&
        (ChipArgs.find("hybrid_emulation") != string::npos))
    {
        s_HybridEmulation = true;
    }


    if ((SimPageTable::SimPageSize != SimPageTable::NativePageSize) &&
        (SimulationType == IChip::ELEVEL_HW) &&
        !s_HybridEmulation)
    {
        // Using a none native page size is not supported on local hardware
        // until we can run mods on fullstack
        // for now force all virtual memory acceses to go through
        // VirtualToPhysical
        SimulationType = IChip::ELEVEL_REMOTE_HW;
    }

    if (IsAmodel())
    {
        // There is no ELEVEL value for amodel
        s_SimulationMode = Platform::Amodel;
    }
    else
    {
        switch (SimulationType)
        {
            case IChip::ELEVEL_HW:
                s_SimulationMode = Hardware;
                break;
            case IChip::ELEVEL_RTL:
                s_SimulationMode = RTL;
                break;
            case IChip::ELEVEL_CMODEL:
                s_SimulationMode = Fmodel;
                break;
            case IChip::ELEVEL_REMOTE_HW:
                s_SimulationMode = Hardware;
                s_RemoteHardware = true;
                break;
            default:
                MASSERT(!"Invalid SimulationType");
        }
    }

    // VF Init() expects s_SimulationMode to have been initialized correctly due to the poll function behind in Init().
    // Why PF Init() cannot be put here togegher? Belwase VmiopElwManager::Instance()->GetVfProcessCount() won't  work
    // before Init() being called
    if (Platform::IsVirtFunMode())
    {
        CHECK_RC(VmiopElwManager::Instance()->Init());
    }

    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        s_pMapMem = (IMapMemory *)(s_pChip->QueryIface(IID_MAP_MEMORY_IFACE));
        s_pMapMemExt = (IMapMemoryExt*)(s_pChip->QueryIface(IID_MAP_MEMORYEXT_IFACE));
        s_pMapDma = (IMapDma *)s_pChip->QueryIface(IID_MAP_DMA_IFACE);
    }

    if (s_RemoteHardware)
    {
        if (!s_pMapMemExt)
        {
            Printf(Tee::PriHigh, "Could not obtain IMapMemoryExt (iid=%d) interface, which is required for RemoteHardware\n",
                IID_MAP_MEMORYEXT_IFACE);
            return RC::SIM_IFACE_NOT_FOUND;
        }
        if (!s_pBusMem)
        {
            Printf(Tee::PriHigh, "Could not obtain IBusMem (iid=%d) interface, which is required for RemoteHardware\n",
                IID_BUSMEM_IFACE);
            return RC::SIM_IFACE_NOT_FOUND;
        }
    }

    // Allow NoBackdoor without blowing up on hardware, since on hardware
    // everything is frontdoor whether we like it or not.
    if (NoBackdoor && (s_SimulationMode != Hardware || s_HybridEmulation))
    {
        // Escape write to shut off backdoor
        EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 0);
    }

    // multi-gpu escapes?
    s_pChiplibGpuEscape = (IGpuEscape *)(s_pChip->QueryIface(IID_GPUESCAPE_IFACE));
    s_pChiplibGpuEscape2 = (IGpuEscape2 *)(s_pChip->QueryIface(IID_GPUESCAPE2_IFACE));

    // Reserve sysmem (for the CPU) in cases of SoC multiengine testing.
    if (s_ReservedSysmemMB > 0)
    {
        s_SimSysMemSize += s_ReservedSysmemMB * 1_MB;
    }

    // Reserve sysmem for Physical Ebridge Mailbox

    if (allocateEBridgeMailbox)
    {
        s_SimSysMemSize += EBridgeMailboxSizeBytes;
    }

    // Determine the system carveout size
    if (systemCarveoutSizeMb)
    {
        totalCarveoutSize += systemCarveoutSizeMb * oneMb + oneMb - 1;
    }

    // Determine the security carveout memory requirements on SoC.
    // In dGPU, they are allocated out of FB.
    if (IsTegra())
    {
        UINT32 VPRSizeMb = 0;
        UINT32 ACRSize   = 0;

        // Determine the Video Protected Region (VPR) carveout size
        CHECK_RC(pJs->GetProperty(Platform_Object, Platform_VPRSizeMb, &VPRSizeMb));
        if (VPRSizeMb)
        {
            totalCarveoutSize += (VPRSizeMb * oneMb + oneMb);
        }

        //
        // Generalized carveouts on CheetAh.
        // CheetAh Memory controller (MC) allows a number a contiguous system memory regions to be configured as carveouts.
        // These carveouts can be configured to have different access rules.
        // Access Controlled Region (ACR) is a generalized carveout that is used by GPU.
        //

        // Determine the First Access Control Region (ACR) carveout size
        CHECK_RC(pJs->GetProperty(Platform_Object, Platform_ACR1Size, &ACRSize));
        if (ACRSize)
        {
            // ACR region is 128 KB size aligned.
            totalCarveoutSize += (LW_ALIGN_UP(ACRSize, 128 * oneKb) + 128 * oneKb);
        }

        // Determine the Second Access Control Region (ACR) carveout size
        ACRSize = 0;
        CHECK_RC(pJs->GetProperty(Platform_Object, Platform_ACR2Size, &ACRSize));
        if (ACRSize)
        {
            // ACR region is 128 KB size aligned
            totalCarveoutSize += (LW_ALIGN_UP(ACRSize, 128 * oneKb) + 128 * oneKb);
        }

        //
        // Determine the Write Protected Region (WPR) carveout size.
        // WPR is a write protected ACR region.
        // It is used to store the Light Secure Falcon (LSF) ucodes.
        // For CheetAh, the ucode blob is less than 128 KB.
        //
        totalCarveoutSize += (2 * 128 * oneKb);

        s_SimSysMemSize += totalCarveoutSize;
    }

    s_pChiplibMultiHeap = (IMultiHeap *)(s_pChip->QueryIface(IID_MULTIHEAP_IFACE));
    s_pChiplibMultiHeap2 = (IMultiHeap2 *)(s_pChip->QueryIface(IID_MULTIHEAP2_IFACE));
    s_MultiHeapSupported = (s_pChiplibMultiHeap || s_pChiplibMultiHeap2) ? true : false;
    if (s_SimulationMode != Hardware || s_HybridEmulation)
    {
        s_MultiHeapSupported &= s_EnableSysmemMultiHeap; // cya...defaults to false
    }
#if !defined(PPC64LE)
    // PPC64LE can't use UC or WC system memory, but it still needs to use MultiHeap for multiple devices
    else if( s_MultiHeapSupported )
    {
        // MODS should not use multiheap if chiplib still talks to old kernel
        // mdiag.o (ie not support extended memory allocation)
        if (s_pChiplibMultiHeap2)
        {
            if (s_pChiplibMultiHeap2->Support64())
            {
                UINT64 phys_addr;

                if (s_pChiplibMultiHeap2->AllocSysMem64( 4096, Memory::UC, 1024, &phys_addr))
                {
                    Printf(Tee::PriNormal, "Extended memory allocation (UC/WC) is not supported\n");
                    s_MultiHeapSupported = false;
                }
                else
                {
                    s_pChiplibMultiHeap2->FreeSysMem64( phys_addr );
                    Printf(Tee::PriNormal, "Extended memory allocation (UC/WC/Normal) enabled\n");
                }
            }
            else
            {
                UINT32 phys_addr;

                if (s_pChiplibMultiHeap2->AllocSysMem32( 4096, Memory::UC, 1024, &phys_addr))
                {
                    Printf(Tee::PriNormal, "Extended memory allocation (UC/WC) is not supported\n");
                    s_MultiHeapSupported = false;
                }
                else
                {
                    s_pChiplibMultiHeap2->FreeSysMem32( phys_addr );
                    Printf(Tee::PriNormal, "Extended memory allocation (UC/WC/Normal) enabled\n");
                }
            }
        }
        else
        {
            if (s_pChiplibMultiHeap->Support64())
            {
                UINT64 phys_addr;

                if (s_pChiplibMultiHeap->AllocSysMem64( 4096, Memory::UC, 1024, &phys_addr))
                {
                    Printf(Tee::PriNormal, "Extended memory allocation (UC/WC) is not supported\n");
                    s_MultiHeapSupported = false;
                }
                else
                {
                    s_pChiplibMultiHeap->FreeSysMem64( phys_addr );
                    Printf(Tee::PriNormal, "Extended memory allocation (UC/WC/Normal) enabled\n");
                }
            }
            else
            {
                UINT32 phys_addr;

                if (s_pChiplibMultiHeap->AllocSysMem32( 4096, Memory::UC, 1024, &phys_addr))
                {
                    Printf(Tee::PriNormal, "Extended memory allocation (UC/WC) is not supported\n");
                    s_MultiHeapSupported = false;
                }
                else
                {
                    s_pChiplibMultiHeap->FreeSysMem32( phys_addr );
                    Printf(Tee::PriNormal, "Extended memory allocation (UC/WC/Normal) enabled\n");
                }
            }
        }
    }
#endif

    // VF mods must use same sysmem base and size as PF mods.
    // Ideally, PF mods should send these information to VF
    // through shared buffer to keep them consistent. Before that,
    // it still depends on VF cmdline to use same value as PF mods
    // to init s_SimSysMemBase and s_SimSysMemSize.
    //
    // Skip sending these infomation to simulator since it has been done
    // by PF mods. This can avoid the issue (Bug 2027514) that SetSysSize
    // temmporarily corrupts the value.
    if (OverrideSysMemBase && !IsVirtFunMode())
    {
        // XXX is multi-heap incompatible with s_SimSysMemBase?
        // The user told us where system memory lives.  Tell the chiplib.
        EscapeWrite("SetSysBaseLo", 0, 4, (UINT32)(s_SimSysMemBase & 0xFFFFFFFF));
        EscapeWrite("SetSysBaseHi", 0, 4, (UINT32)(s_SimSysMemBase >> 32));
        // Leave the 32-bit sys size call for legacy chiplibs.
        EscapeWrite("SetSysSize", 0, 4, (UINT32)s_SimSysMemSize);
        EscapeWrite("SetSysSizeLo", 0, 4, (UINT32)(s_SimSysMemSize & 0xFFFFFFFF));
        EscapeWrite("SetSysSizeHi", 0, 4, (UINT32)(s_SimSysMemSize >> 32));
    }

    // Bug 594880: -sysmem_size 512 fails if s_SimSysMemSizeUC is small,
    // due to alignment constraints.  This is mostly a problem in emulation.
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        if (s_SimSysMemSizeUC < 1024*1024*1024)
        {
            s_SimSysMemSizeUC = 1024*1024*1024;
        }
        if (s_SimSysMemSizeWB < 1024*1024*1024)
        {
            s_SimSysMemSizeWB = 1024*1024*1024;
        }
        if (s_SimSysMemSizeWC < 1024*1024*1024)
        {
            s_SimSysMemSizeWC = 1024*1024*1024;
        }
    }

    if ( s_MultiHeapSupported )
    {
        // just initialize the heap minima and maxima, and the alloc interfaces
        MHeapSimPtr::Install(new MHeapSim);
        MHeapSimPtr m;
        if (s_pChiplibMultiHeap2)
        {
            if (s_pChiplibMultiHeap2->Support64())
            {
                m->SetMemAlloc32_2(s_pChiplibMultiHeap2);
                m->SetMemAlloc64_2(s_pChiplibMultiHeap2);
            }
            else
            {
                m->SetMemAlloc32_2(s_pChiplibMultiHeap2);
                m->SetMemAlloc64_2(NULL);
            }
        }
        else
        {
            m->SetMemAlloc32_2(NULL);
            m->SetMemAlloc64_2(NULL);
        }
        if (s_pChiplibMultiHeap && s_pChiplibMultiHeap->Support64())
        {
            // In order to support heaps with PA < 4GB
            m->SetMemAlloc32( s_pChiplibMultiHeap );
            m->SetMemAlloc64( s_pChiplibMultiHeap );
        }
        else
        {
            m->SetMemAlloc32( s_pChiplibMultiHeap ); // bummer
            m->SetMemAlloc64( NULL );
        }
        // Alloc the heap at least one page at a time
#if defined(PPC64LE)
#define MIN_HEAP_SIZE (64*1024)
#else
#define MIN_HEAP_SIZE (4*1024)
#endif
        MASSERT(s_SimSysMemSizeUC>=MIN_HEAP_SIZE);
        MASSERT(s_SimSysMemSizeWC>=MIN_HEAP_SIZE);
        MASSERT(s_SimSysMemSizeWT>=MIN_HEAP_SIZE);
        MASSERT(s_SimSysMemSizeWP>=MIN_HEAP_SIZE);
        MASSERT(s_SimSysMemSizeWB>=MIN_HEAP_SIZE);
        m->SetHeapAllocMax(Memory::UC, s_SimSysMemSizeUC );
        m->SetHeapAllocMax(Memory::WC, s_SimSysMemSizeWC );
        m->SetHeapAllocMax(Memory::WT, s_SimSysMemSizeWT );
        m->SetHeapAllocMax(Memory::WP, s_SimSysMemSizeWP );
        m->SetHeapAllocMax(Memory::WB, s_SimSysMemSizeWB );

        m->SetHeapAllocMin(Memory::UC, (MIN_HEAP_SIZE<=s_SimSysMemSizeUC ? MIN_HEAP_SIZE:s_SimSysMemSizeUC) );
        m->SetHeapAllocMin(Memory::WC, (MIN_HEAP_SIZE<=s_SimSysMemSizeWC ? MIN_HEAP_SIZE:s_SimSysMemSizeWC) );
        m->SetHeapAllocMin(Memory::WT, (MIN_HEAP_SIZE<=s_SimSysMemSizeWT ? MIN_HEAP_SIZE:s_SimSysMemSizeWT) );
        m->SetHeapAllocMin(Memory::WP, (MIN_HEAP_SIZE<=s_SimSysMemSizeWP ? MIN_HEAP_SIZE:s_SimSysMemSizeWP) );
        m->SetHeapAllocMin(Memory::WB, (MIN_HEAP_SIZE<=s_SimSysMemSizeWB ? MIN_HEAP_SIZE:s_SimSysMemSizeWB) );
    }
    else if (!OverrideSysMemBase)
    {
        // Allocate simulator system memory from the chiplib.  This tells us what
        // the physical address of simulator system memory is. The MODS suballocator
        // will be used for all real memory allocations out of this chunk.
        if ( s_pMemAlloc64 )
        {
            s_SimSysMemBase = 0x0;
            if (s_pMemAlloc64->AllocSysMem64(s_SimSysMemSize, &s_SimSysMemBase))
            {
                return RC::CANNOT_ALLOCATE_MEMORY;
            }
            s_AllocatedSimSysMemBase = s_SimSysMemBase;
        }
        else
        {
            UINT32 SimSysMemBase32 = 0; // Initialized to WAR a chiplib bug/undolwmented behavior

            if (static_cast<INT64>(s_SimSysMemSize) > (std::numeric_limits<UINT32>::max)())
            {
                Printf(Tee::PriError, "Sysmem size 0x%016llX exceeds the maximum value.\n",
                    s_SimSysMemSize);
                return RC::SOFTWARE_ERROR;
            }
            if (s_pChip->AllocSysMem(static_cast<int>(s_SimSysMemSize), &SimSysMemBase32))
            {
                return RC::CANNOT_ALLOCATE_MEMORY;
            }

            s_SimSysMemBase = SimSysMemBase32;
            s_AllocatedSimSysMemBase = SimSysMemBase32;
        }
    }

    if ( s_MultiHeapSupported )
    {
        s_SimSysHeap = NULL;
    }
    else if (s_SimSysMemSize > 0)
    {
        Printf(Tee::PriDebug, "System memory physical base = 0x%016llX\n",
            s_SimSysMemBase);
        Printf(Tee::PriDebug, "                       size = 0x%016llX\n",
            s_SimSysMemSize);

        if (s_SimulationMode == Hardware && s_ReservedSysmemMB > 0 && !s_HybridEmulation)
        {
            s_SimSysMemSize -= s_ReservedSysmemMB * 1_MB;
            s_SimSysMemBase += s_ReservedSysmemMB * 1_MB;
        }

        if (s_SimulationMode == Hardware && totalCarveoutSize > 0 && !s_HybridEmulation)
        {
            s_SimSysMemSize -= totalCarveoutSize;
            s_SimSysMemBase += totalCarveoutSize;
        }

        if (allocateEBridgeMailbox)
        {
            // put mailbox after any carveouts
            char msg[256];
            sprintf(msg, "CPU_MODEL|CM_EBRIDGE|bridge:=PhysicalBridge|sim:=chiplib_v|platform:=ArchMODS|mailbox_phys_addr:=0x%llx|mailbox_virt_addr:=0x0|mailbox_size:=0x%x",
                    s_SimSysMemBase, EBridgeMailboxSizeBytes);
            s_SimSysMemSize -= EBridgeMailboxSizeBytes;
            Platform::EscapeWrite(msg, 0, 4, 0);
            s_SimSysMemBase += EBridgeMailboxSizeBytes;
        }

        // Create a heap to keep track of system memory allocations
        if (s_SimSysHeap == NULL)
        {
            s_SimSysHeap = new Heap(s_SimSysMemBase, s_SimSysMemSize, 0);
            if (!s_SimSysHeap)
            {
                return RC::CANNOT_ALLOCATE_MEMORY;
            }
            s_SimSysHeap->SetNext(NULL);
        }
    }
    s_SimSysHeapRandom.SeedRandom(PteSeed);

    if (s_SimulationMode != Hardware || s_HybridEmulation)
    {
        // Create a thread for the simulator to run in.
        Printf(Tee::PriLow, "Starting SimulationThread\n");
        s_SimThreadID = Tasker::CreateThread(SimulationThread, 0, 0, "Simulator");
    }

    s_Initialized = true;

    pJs->HookCtrlC();

    // Reserve sysmem (for the CPU) in cases of SoC multiengine testing.
    if (s_ReservedSysmemMB > 0)
    {
        const UINT64 oneMb = 1024U*1024U;
        const UINT64 size = s_ReservedSysmemMB * oneMb;
#define SOC_SYSMEM_BASE 0x80000000
        if (s_SimulationMode == Hardware && !s_HybridEmulation)
        {
            void *pReturnedVirtualAddress = nullptr;
            CHECK_RC(Xp::AllocAddressSpace(size,
                                           &pReturnedVirtualAddress,
                                           nullptr,
                                           Memory::ProtectDefault,
                                           Memory::VirualAllocDefault));
            if (SimPageTable::SimPageSize != SimPageTable::NativePageSize)
            {
                LwU064 virtAddr = reinterpret_cast<LwU064>(pReturnedVirtualAddress);
                virtAddr &= ~SimPageTable::SimPageMask;
                virtAddr += SimPageTable::SimPageSize;
                pReturnedVirtualAddress = reinterpret_cast<void*>(virtAddr);
            }
            Printf(Tee::PriDebug, "%s Reserved sysmem base CPU VA:%p\n",
                    __FUNCTION__, pReturnedVirtualAddress);

            s_ReserveMapping.VirtualAddress = pReturnedVirtualAddress;
            s_ReserveMapping.NumBytes       = size;
            PHYSADDR PhysAddr = SOC_SYSMEM_BASE;
            // Memory::UC
            UINT32 Flags = PAGE_READABLE | PAGE_WRITEABLE | PAGE_DIRECT_MAP;
            CHECK_RC(SimPageTable::PopulatePages(s_ReserveMapping.VirtualAddress, s_ReserveMapping.NumBytes));
            SimPageTable::MapPages(s_ReserveMapping.VirtualAddress, PhysAddr, s_ReserveMapping.NumBytes, Flags);
            s_UnmapReserveMapping = true;
        }
        else
        {
            CHECK_RC(AllocPagesAligned
            (
                size,
                &s_pReserveSysMem,
                oneMb,
                64,
                Memory::UC,
                Platform::UNSPECIFIED_GPUINST
            ));
            PHYSADDR reserveSysMemBase = GetPhysicalAddress(s_pReserveSysMem, 0);

            if (IsTegra() && reserveSysMemBase != SOC_SYSMEM_BASE)
            {
                return RC::CANNOT_ALLOCATE_MEMORY;
            }

        }
    }

    if (totalCarveoutSize && (s_SimulationMode == Hardware) && !s_HybridEmulation)
    {
        // Reserving carveout in sysmem.
        Printf(Tee::PriLow, "Reserving carveouts in sysmem.\n");

        if (s_ReservedSysmemMB > 0)
        {
            // Reserve next to reserveSysmemMB.
            s_CarveoutReserveMapping.VirtualAddress = (void*)((uintptr_t)s_ReserveMapping.VirtualAddress + s_ReserveMapping.NumBytes);
        }
        else
        {
            // Reserve at beginning of EMEM.
            s_CarveoutReserveMapping.VirtualAddress = (void*)(size_t)(SOC_SYSMEM_BASE);
        }

        s_CarveoutReserveMapping.NumBytes = totalCarveoutSize;

        // Memory::UC
        PHYSADDR PhysAddr = (static_cast<UINT64>((uintptr_t)s_CarveoutReserveMapping.VirtualAddress));
        //s_CarveoutReserveMapping.VirtualAddress;
        UINT32 Flags = PAGE_READABLE | PAGE_WRITEABLE | PAGE_DIRECT_MAP;
        CHECK_RC(SimPageTable::PopulatePages(s_CarveoutReserveMapping.VirtualAddress, s_CarveoutReserveMapping.NumBytes));
        SimPageTable::MapPages(s_CarveoutReserveMapping.VirtualAddress, PhysAddr, s_CarveoutReserveMapping.NumBytes, Flags);
        s_UnmapCarveoutReserveMapping = true;
    }

    if (systemCarveoutSizeMb > 0)
    {
        UINT64 size = systemCarveoutSizeMb * oneMb;

        Printf(Tee::PriDebug, "Trying to allocate %llu bytes of system carveout\n", size);
        CHECK_RC(CreateCarveout(&s_Carveout, size, oneKb));
        Printf(Tee::PriHigh, "Allocated system carveout at phys addr 0x%llX, size 0x%llX\n",
            s_Carveout.base, size);
    }

    if ( s_LwlinkForceAutoconfig )
    {
        Platform::EscapeWrite("CPU_MODEL|LWLINK_AUTOCONFIG_ENABLE", 0, 4, 0);
    }

    return OK;
}

void Platform::Cleanup(CleanupMode Mode)
{

    if (Mode == MininumCleanup) // Needed when calling from PrintStack.
    {
        VmiopElwManager::Instance()->ShutDownMinimum();

        // Flush mods.log.
        Tee::FlushToDisk();

        // Flush standard out.
        fflush(stdout);

        return;
    }

    VmiopElwManager::Instance()->ShutDown();
    ChiplibTraceDumper::GetPtr()->Cleanup();

    // Shouldn't be any harm in having Cleanup() called more than once

    // This function pointer may be used as a callback to
    // a function which takes care of unmapping memory mapped
    // through MODS. This is used by LwWatch for simulation on Linux.
    if(unmapFn)
        unmapFn();

    DestroyCarveout(&s_Carveout);

    DestroyCarveout(&s_VPR);

    for (int i = 0; i < MAX_GEN_CARVEOUT; i++)
    {
        DestroyCarveout(&s_GenCarveout[i]);
        s_nC--;
    }

    if (s_pReserveSysMem)
    {
        FreePages(s_pReserveSysMem);
    }

    if (s_UnmapReserveMapping)
    {
        SimPageTable::UnmapPages(s_ReserveMapping.VirtualAddress, s_ReserveMapping.NumBytes);
    }

    if (s_UnmapCarveoutReserveMapping)
    {
        SimPageTable::UnmapPages(s_CarveoutReserveMapping.VirtualAddress, s_CarveoutReserveMapping.NumBytes);
    }

    Platform::CallAndClearCleanupFunctions();

    // Shut down the interrupt thread
    if (s_InterruptThreadID != Tasker::NULL_THREAD)
    {
        s_InterruptThreadRunning = false;
        Tasker::Join(s_InterruptThreadID);
        s_InterruptThreadID = Tasker::NULL_THREAD;
    }

    // Shut down the simulation thread
    if (s_SimThreadID != Tasker::NULL_THREAD)
    {
        s_TerminateSimThread = true;
        Tasker::Join(s_SimThreadID);
        s_SimThreadID = Tasker::NULL_THREAD;
    }
//    if (Mode == DontCheckForLeaks)
//    {
//        ErrorLogger::Shutdown();
//    }
    if (s_AllocatedSimSysMemBase)
    {
        if (s_pMemAlloc64)
        {
            s_pMemAlloc64->FreeSysMem64(s_AllocatedSimSysMemBase);
        }
        else
        {
            s_pChip->FreeSysMem((LwU032)s_AllocatedSimSysMemBase);
        }
        s_SimSysMemBase = 0x0;
        s_AllocatedSimSysMemBase = 0;
    }
    // Close the amodel connection
    if (s_pAmod)
    {
        s_pAmod->Release();
        s_pAmod = NULL;
    }

    // Close the chip library connection
    if (s_pClockManager)
    {
        delete s_pClockManager;
        s_pClockManager = 0;
    }
    if (s_pInterruptMgr)
    {
        s_pInterruptMgr->Release();
        s_pInterruptMgr = 0;
    }
    if (s_pInterruptMgr2)
    {
        s_pInterruptMgr2->Release();
        s_pInterruptMgr2 = 0;
    }
    if (s_pInterruptMgr3)
    {
        s_pInterruptMgr3->Release();
        s_pInterruptMgr3 = 0;
    }
    if (s_pInterruptMask)
    {
        s_pInterruptMask->Release();
        s_pInterruptMask = 0;
    }
    if (s_pMem)
    {
        s_pMem->Release();
        s_pMem = NULL;
    }
    if (s_pBusMem)
    {
        s_pBusMem->Release();
        s_pBusMem = NULL;
    }
    if (s_pMapDma)
    {
        s_pMapDma->Release();
        s_pMapDma = nullptr;
    }
    if (s_pMapMem)
    {
        s_pMapMem->Release();
        s_pMapMem = NULL;
    }
    if (s_pMapMemExt)
    {
        s_pMapMemExt->Release();
        s_pMapMemExt = NULL;
    }
    if (s_pIo)
    {
        s_pIo->Release();
        s_pIo = NULL;
    }
    if (s_pMemAlloc64)
    {
        s_pMemAlloc64->Release();
        s_pMemAlloc64 = NULL;
    }
    if ( s_pChiplibGpuEscape )
    {
        s_pChiplibGpuEscape->Release();
        s_pChiplibGpuEscape = NULL;
    }
    if ( s_pChiplibGpuEscape2 )
    {
        s_pChiplibGpuEscape2->Release();
        s_pChiplibGpuEscape2 = NULL;
    }
    if (s_pPciDev)
    {
        s_pPciDev->Release();
        s_pPciDev = NULL;
    }
    // Free the simulator system memory
    if ( s_MultiHeapSupported )
    {
        MASSERT(s_SimSysHeap == NULL);
        MHeapSimPtr::Free();
        if (s_pChiplibMultiHeap)
        {
            s_pChiplibMultiHeap->Release();
            s_pChiplibMultiHeap = NULL;
        }
        if (s_pChiplibMultiHeap2)
        {
            s_pChiplibMultiHeap2->Release();
            s_pChiplibMultiHeap2 = NULL;
        }
    }
    if (s_pChip)
    {
        s_pChip->Shutdown();
        s_pChip->Release();
        s_pChip = NULL;
    }
    s_ChipSimModule = NULL;

    // Release chiplib arguments
    {
        vector<const char*> empty;
        g_ChipLibArgV.swap(empty);
    }

    while (!s_DllModulesToUnloadOnExit.empty())
    {
        void* module = s_DllModulesToUnloadOnExit.back();

        IFSPEC3::FreeProxies(module);
        Xp::UnloadDLL(module);

        s_DllModulesToUnloadOnExit.pop_back();
    }

    if ((Mode == CheckForLeaks) && s_SimSysHeap)
    {
        if (s_SimSysHeap->GetTotalAllocated() > 0)
        {
            Printf(Tee::PriHigh,
                   "Physical memory leak of %lld bytes detected!\n",
                   s_SimSysHeap->GetTotalAllocated());
            MASSERT(0);
        }
        delete s_SimSysHeap;
        s_SimSysHeap = NULL;
    }

    // Report memory mapping leaks
    if ((Mode == CheckForLeaks) && s_DeviceMappingSizes.size() > 0)
    {
        Printf(Tee::PriHigh, "%d device mappings were leaked!\n",
            (int)s_DeviceMappingSizes.size());
        // dump the leaked mappings:
        for( map<void *, size_t>::iterator iter = s_DeviceMappingSizes.begin();
             iter != s_DeviceMappingSizes.end();
             iter++
           )
        {
            Printf(Tee::PriHigh,"Virtual Addr:%p, Size:%ld\n",
                    iter->first, (long)iter->second);
        }
        MASSERT(0);
    }

    if (Mode == CheckForLeaks)
    {
        // Report ISRs that were not unhooked
        if (!s_IrqMap.empty())
        {
            Printf(Tee::PriHigh, "ISR was not unhooked from IRQ %d\n",
                    s_IrqMap.begin()->first);
            MASSERT(0);
        }
        SimPageTable::Cleanup();
    }

    s_Initialized = false;
    if (0 != SimCallMutex::GetMutex())
    {
        Tasker::FreeMutex(SimCallMutex::GetMutex());
        SimCallMutex::SetMutex(0);
    }

    if (Mode == DontCheckForLeaks)
    {
        // Flush mods.log.
        Tee::FlushToDisk();

        // Flush standard out.
        fflush(stdout);
    }
    s_SubstituteRegisters.reset(0);
}

RC Platform::Reset()
{
    // We are not done yet, we shouldn't be checking for memory/resource leaks now.
    Platform::Cleanup(DontCheckForLeaks);

    // Intialize global variables to init startup values.
    s_SimulationMode =  Platform::Hardware;
    s_RemoteHardware =  false;

    return Platform::Initialize();
}

bool Platform::IsInitialized()
{
    return s_Initialized;
}

///------------------------------------------------------------------------------
RC Platform::DisableUserInterface
(
    UINT32 Width,
    UINT32 Height,
    UINT32 Depth,
    UINT32 RefreshRate,
    UINT32 FilterTaps,
    UINT32 ColorComp,
    UINT32 DdScalerMode,
    GpuDevice *pGrDev
)
{
    RC rc;

    CHECK_RC(DisableUserInterface());

#ifdef INCLUDE_GPU
    GpuDevMgr* pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);

    GpuDevice* pDev = pGrDev ? pGrDev : pGpuDevMgr->GetFirstGpuDevice();

    while (pDev)
    {
        Display *pDisplay = pDev->GetDisplay();

        UINT32 Selected = pDisplay->Selected();

        // Only one display device can be selected.
        if (Selected & (Selected - 1))
        {
            Printf(Tee::PriHigh, "Only one active display can be selected.\n");
            return RC::ILWALID_DISPLAY_MASK;
        }

        if (OK != pDisplay->SetMode(
                pDisplay->Selected(),
                Width,
                Height,
                Depth,
                RefreshRate,
                Display::FilterTaps(FilterTaps),
                Display::ColorCompression(ColorComp),
                DdScalerMode))
        {
            return RC::CANNOT_DISABLE_USER_INTERFACE;
        }

        if (!pGrDev)
            break; // The function was request to run on one specific device.

        pDev = pGpuDevMgr->GetNextGpuDevice(pDev);
    }
#endif

    return rc;
}

bool Platform::IsTegra()
{
    // Calling this function prior to initialization will cause it to always
    // return false since (s_pChip == NULL) by default
    if (s_pChip == NULL)
    {
        Printf(Tee::PriHigh, "Platform::IsTegra() called before chiplib is initialized!\n");
        return false;
    }

    static bool bInitialized = false;
    static bool bIsSOC = false;

    if (!bInitialized)
    {
        MASSERT(s_pChip);
        bool bIsSocEscapeReadSupported = true;

        if (IsAmodel())
        {
            bIsSocEscapeReadSupported = false;
            UINT32 isSocEscapeReadSupported = 0;
            if (0 == s_pChip->EscapeRead("supported/isSoC", 0,
                    sizeof(isSocEscapeReadSupported),
                    &isSocEscapeReadSupported))
            {
                bIsSocEscapeReadSupported = isSocEscapeReadSupported != 0;
            }
        }

        if (bIsSocEscapeReadSupported)
        {
            UINT32 isSoC = 0;
            if (0 == s_pChip->EscapeRead(const_cast<char*>("isSoC"),
                      0, sizeof(isSoC), &isSoC))
            {
                bIsSOC = isSoC != 0;
            }
        }

        bInitialized = true;
    }
    return bIsSOC;
}
// Internal function to query pcie devices.
bool IsPcie()
{
    // Since AD10B, iGPU can be a PCIE device.
    if (Platform::IsTegra() && !Platform::IsAmodel())
    {
        static bool bInitialized = false;
        static bool bIsPcie = false;

        if (!bInitialized)
        {
            MASSERT(s_pChip);
            
            UINT32 isPcie = 0;
            if (0 == s_pChip->EscapeRead(const_cast<char*>("isPcie"),
                    0, sizeof(isPcie), &isPcie))
            {
                bIsPcie = isPcie != 0;
            }

            bInitialized = true;
        }
        return bIsPcie;
    }
    return true;
}

///------------------------------------------------------------------------------
// Find a PCI device with given 'DeviceId', 'VendorId', and 'Index'.
// Return '*pBusNumber', '*pDeviceNumber', and '*pFunctionNumber'.
//
RC Platform::FindPciDevice
(
    INT32   DeviceId,
    INT32   VendorId,
    INT32   Index,
    UINT32* pDomainNumber   /* = 0 */,
    UINT32* pBusNumber      /* = 0 */,
    UINT32* pDeviceNumber   /* = 0 */,
    UINT32* pFunctionNumber /* = 0 */
)
{
    if (IsTegra())
    {
        Printf(Tee::PriNormal, "Warning: FindPciDevice used on a platform which does not have PCI bus!\n");
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    if (!s_pChip)
    {
        RC rc;

        CHECK_RC(Xp::FindPciDevice(DeviceId, VendorId, Index, pDomainNumber,
                                 pBusNumber, pDeviceNumber, pFunctionNumber));
    }
    else if (s_pPciDev)
    {
        LwPciDev pciDev = {};
        const LwErr ret = s_pPciDev->FindPciDevice(VendorId, DeviceId, Index, &pciDev);
        if (ret != LW_PASS)
            return RC::PCI_DEVICE_NOT_FOUND;

        if (pDomainNumber)
            *pDomainNumber = pciDev.domain;

        if (pBusNumber)
            *pBusNumber = pciDev.bus;

        if (pDeviceNumber)
            *pDeviceNumber = pciDev.device;

        if (pFunctionNumber)
            *pFunctionNumber = pciDev.function;
    }
    else
    {
        UINT32 addr;
        if (s_pChip->FindPCIDevice(VendorId, DeviceId, Index, &addr) < 0)
        {
            return RC::PCI_DEVICE_NOT_FOUND;
        }

        Pci::DecodeConfigAddress(addr, pDomainNumber, pBusNumber,
                                 pDeviceNumber, pFunctionNumber, nullptr);
    }

    return AddPCISkipDevice(*pDomainNumber, *pBusNumber, *pDeviceNumber, *pFunctionNumber);
}

//------------------------------------------------------------------------------
// @param ClassCode The entire 24 bit register baseclass/subclass/pgmIF,
//                  a byte for each field
//
RC Platform::FindPciClassCode
(
    INT32   ClassCode,
    INT32   Index,
    UINT32* pDomainNumber   /* = 0 */,
    UINT32* pBusNumber      /* = 0 */,
    UINT32* pDeviceNumber   /* = 0 */,
    UINT32* pFunctionNumber /* = 0 */
)
{
    if (IsTegra())
    {
        Printf(Tee::PriNormal, "Warning: FindPciClassCode used on a platform which does not have PCI bus!\n");
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    if (!s_pChip)
    {
        RC rc;

        CHECK_RC(Xp::FindPciClassCode(ClassCode, Index, pDomainNumber, pBusNumber,
                                    pDeviceNumber, pFunctionNumber));
    }
    else if (s_pPciDev)
    {
        LwPciDev pciDev = {};
        const LwErr ret = s_pPciDev->FindPciClassCode(ClassCode, Index, &pciDev);
        if (ret != LW_PASS)
            return RC::PCI_DEVICE_NOT_FOUND;

        if (pDomainNumber)
            *pDomainNumber = pciDev.domain;

        if (pBusNumber)
            *pBusNumber = pciDev.bus;

        if (pDeviceNumber)
            *pDeviceNumber = pciDev.device;

        if (pFunctionNumber)
            *pFunctionNumber = pciDev.function;
    }
    else
    {
        UINT32 addr;
        if (s_pChip->FindPCIClassCode(ClassCode, Index, &addr) < 0)
        {
            return RC::PCI_DEVICE_NOT_FOUND;
        }
        Pci::DecodeConfigAddress(addr, pDomainNumber, pBusNumber,
                                 pDeviceNumber, pFunctionNumber, nullptr);
    }

    if (ClassCode == Pci::CLASS_CODE_VIDEO_CONTROLLER_VGA ||
        ClassCode == Pci::CLASS_CODE_VIDEO_CONTROLLER_3D)
    {
        // Bug 3026626
        // Pass bdf to CPUModel before rm's first sysmem access in InitGpuInRm
        SendBdf(*pDomainNumber, *pBusNumber, *pDeviceNumber, *pFunctionNumber);
    }

    return AddPCISkipDevice(*pDomainNumber, *pBusNumber, *pDeviceNumber, *pFunctionNumber);
}

//------------------------------------------------------------------------------
// Force a rescan of PCI bus.
RC Platform::RescanPciBus
(
    INT32 domain,
    INT32 bus
)
{
    return Xp::RescanPciBus(domain, bus);
}

//------------------------------------------------------------------------------
RC Platform::PciResetFunction
(
    INT32 domainNumber,
    INT32 busNumber,
    INT32 deviceNumber,
    INT32 functionNumber
)
{
    if (IsTegra())
    {
        Printf(Tee::PriWarn,
               "PciResetFunction used on a platform which does not have PCI bus!\n");
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    if (s_pPciDev2)
    {
        const LwPciDev pciDevice =
        {
            static_cast<UINT16>(domainNumber),
            static_cast<UINT16>(busNumber),
            static_cast<UINT16>(deviceNumber),
            static_cast<UINT16>(functionNumber)
        };
        if (s_pPciDev2->PciResetFunction(pciDevice) != LW_PASS)
        {
            Printf(Tee::PriWarn,
                   "PciResetFunction failed through chiplib interface, reverting to MODS implementation!\n");
        }
        else
        {
            return RC::OK;
        }
    }

    // Per PCIe r4.0, sec 6.6.2, a device must complete an FLR within
    // 100ms, but may silently discard requests while the FLR is in
    // progress.  Wait 100ms before trying to access the device.
    UINT32 delayAfterResetUs = 100000;

    // Attempt to lower delay on non-HW platforms so reset doesnt take so long
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
        TestDevice::Id devId(domainNumber, busNumber, deviceNumber, functionNumber);
        TestDevicePtr pTestDevice;
        if ((pTestDeviceMgr->GetDevice(devId, &pTestDevice) == RC::OK) &&
            pTestDevice->SupportsInterface<Pcie>() &&
            (pTestDevice->GetInterface<Pcie>()->SetResetCrsTimeout(s_CrsTimeoutUs) == RC::OK))
        {
            delayAfterResetUs = s_DelayAfterFlrUs;
        }
    }
    return Pci::ResetFunction(domainNumber, busNumber, deviceNumber,
                              functionNumber, delayAfterResetUs);
}

//------------------------------------------------------------------------------
// Read and write virtual addresses
//
void Platform::VirtualRd(const volatile void *Addr, void *Data, UINT32 Count)
{
    RC rc;
    uintptr_t FirstByte, LastByte, FirstPage, LastPage;
    PHYSADDR physAddr;
    const bool hasPhysAddr = (OK == VirtualToPhysicalInternal(&physAddr, Addr));
    UINT64 tempTrackTime = SetupTracking();

    // substitute read
    if (s_SubstituteRegisters.get() && hasPhysAddr && (OK == SubstitutePhysRd(physAddr, Data, Count)))
        return;

    if (Platform::IsVirtFunMode() && hasPhysAddr)
    {
        VmiopElwManager* pMgr = VmiopElwManager::Instance();
        VmiopElw*        pVmiopElw;
        UINT32           regAddr;
        switch (pMgr->FindVfEmuRegister(physAddr, &pVmiopElw, &regAddr))
        {
            case OK:
                MASSERT(Count == 4);
                rc = pVmiopElw->CallPluginRegRead(regAddr, Count, Data);
                if (RC::RESET_IN_PROGRESS == rc)
                {
                    Printf(Tee::PriNormal, "Abort PhysRd(0x%llx)=0x%x to PF process due vGpu plugin unload or FLR reset.\n",
                           physAddr, *(const UINT32*)Data);
                    return;
                }
                MASSERT(rc == OK);
                Printf(Tee::PriDebug,
                       "Redirecting RegRd(0x%x)=0x%x to PF process\n",
                       regAddr, *static_cast<UINT32*>(Data));
                return;
            case RC::ILWALID_REGISTER_NUMBER:
                memset(Data, 0, Count);
                Printf(Tee::PriDebug,
                       "Skipping RegRd(0x%x) and 0 returned\n",
                       regAddr);
                return;
            default:
                break;
        }
    }

    if ((s_SimulationMode == Hardware) && !s_RemoteHardware && !s_HybridEmulation)
    {
        HwMemCopy(Data, Addr, Count);

        DumpAddrData( Addr, Data, Count, "vrd" );

        if (s_DumpPhysicalAccess)
        {
            // Only print if the address can be resolved
            if (hasPhysAddr)
            {
                DumpAddrData( physAddr, Data, Count, "prd" );
            }
        }
        ChecktoTrackAddress(Addr, true, TRACK_READ, Data, tempTrackTime);
        return;
    }

    // Split accesses that span a page boundary.
    FirstByte = uintptr_t(Addr);
    LastByte  = FirstByte + Count - 1;
    FirstPage = FirstByte >> SimPageTable::SimPageShift;
    LastPage  = LastByte  >> SimPageTable::SimPageShift;
    if (FirstPage != LastPage)
    {
        // Partial or full first page
        VirtualRd(Addr, Data,
            UNSIGNED_CAST(UINT32,
                SimPageTable::SimPageSize - (FirstByte & SimPageTable::SimPageMask)));

        // Full middle pages, if any
        for (uintptr_t Page = FirstPage+1; Page <= LastPage-1; Page++)
        {
            VirtualRd((const volatile UINT08 *)(Page << SimPageTable::SimPageShift),
                      (UINT08 *)Data + ((Page << SimPageTable::SimPageShift) - FirstByte),
                      UNSIGNED_CAST(UINT32, SimPageTable::SimPageSize));
        }

        // Partial or full last page
        VirtualRd((const volatile UINT08 *)(LastPage << SimPageTable::SimPageShift),
                  (UINT08 *)Data + ((LastPage << SimPageTable::SimPageShift) - FirstByte),
                  UNSIGNED_CAST(UINT32, (LastByte & SimPageTable::SimPageMask) + 1));

        return;
    }

    // Page must be valid and readable
    const PageTableEntry *Pte = SimPageTable::GetVirtualAddrPte(Addr);
    if (!Pte)
    {
        // It's not in the sim page table.  This might be plain old system memory
        // from malloc, or it might be a completely bogus address.  Just try it;
        // if it's bogus, it'll crash.
        // XXX memcpy is very dangerous if we use it for chip registers, etc.
        memcpy(Data, const_cast<const void *>(Addr), Count);

        DumpAddrData( Addr, Data, Count, "vrd" );
        return;
    }
    if (!(Pte->Flags & PAGE_READABLE))
    {
        Printf(Tee::PriHigh, "VirtualRd(%p) caused a read protection violation\n", Addr);
        MASSERT(0);
        return;
    }
    if (Pte->Flags & PAGE_DIRECT_MAP)
    {
        if (s_RemoteHardware)
        {
            MASSERT(hasPhysAddr);
            if (s_pBusMem->BusMemRdBlk(physAddr, Data, Count))
            {
                Printf(Tee::PriError,
                        "Error: Platform::%s Failed remote read physical address %llx Count=%d\n",
                        __FUNCTION__,
                        physAddr, Count);
                MASSERT(0);
            }
        }
        else
        {
            memcpy(Data, const_cast<const void *>(Addr), Count);
        }

        DumpAddrData( Addr, Data, Count, "vrd" );

        if (s_DumpPhysicalAccess)
        {
            MASSERT(hasPhysAddr);
            DumpAddrData(physAddr, Data, Count, "prd");
        }
        return;
    }

    // Presently, reads of WC memory (only) always flush the WC buffer.  This
    // is not strictly required in all cases; it is only done to ensure that
    // writes still in the WC buffer are visible to subsequent reads.  Note that
    // this implies that if the same memory is mapped both WC and UC, the two
    // mappings will not be coherent.
    if (Pte->Flags & PAGE_WRITE_COMBINED)
    {
        rc = FlushCpuWriteCombineBuffer();
        if (rc != OK)
        {
            Printf(Tee::PriHigh, "VirtualRd(%p) failed: %s\n", Addr, rc.Message());
            MASSERT(0);
        }
    }

    MASSERT(hasPhysAddr);

    rc = PhysRd(physAddr, Data, Count);
    if (rc != OK)
    {
        Printf(Tee::PriHigh, "VirtualRd(%p) failed: %s\n", Addr, rc.Message());
        MASSERT(0);
    }

    DumpAddrData( Addr, Data, Count, "vrd" );
}

void Platform::VirtualWr(volatile void *Addr, const void *Data, UINT32 Count)
{
    RC rc;
    uintptr_t FirstByte, LastByte, FirstPage, LastPage;
    PHYSADDR physAddr;
    const bool hasPhysAddr = (OK == VirtualToPhysicalInternal(&physAddr, Addr));
    UINT64 tempTrackTime = SetupTracking();

    //skip write
    if (s_SubstituteRegisters.get() && hasPhysAddr && (OK == SubstitutePhysWr(physAddr, Data, Count)))
        return;

    if (Platform::IsVirtFunMode() && hasPhysAddr)
    {
        VmiopElwManager* pMgr = VmiopElwManager::Instance();
        VmiopElw* pVmiopElw;
        UINT32    regAddr;
        switch (pMgr->FindVfEmuRegister(physAddr, &pVmiopElw, &regAddr))
        {
            case OK:
                MASSERT(Count == 4);
                rc = pVmiopElw->CallPluginRegWrite(regAddr, Count, Data);
                if (RC::RESET_IN_PROGRESS == rc)
                {
                    Printf(Tee::PriNormal, "Abort PhysWr(0x%llx)=0x%x to PF process due vGpu plugin unload or FLR reset.\n",
                           physAddr, *(const UINT32*)Data);
                    return;
                }
                MASSERT(rc == OK);
                Printf(Tee::PriDebug,
                       "Redirecting RegWr(0x%x)=0x%x to PF process\n",
                       regAddr, *static_cast<const UINT32*>(Data));
                return;
            case RC::ILWALID_REGISTER_NUMBER:
                Printf(Tee::PriDebug,
                       "Skipping RegWr(0x%x) and data dropped\n",
                       regAddr);
                return;
            default:
                break;
        }
    }

    if ((s_SimulationMode == Hardware) && !s_RemoteHardware && !s_HybridEmulation)
    {
        DumpAddrData( Addr, Data, Count, "vwr" );

        if (s_DumpPhysicalAccess)
        {
            // Only print if the address can be resolved
            if (hasPhysAddr)
            {
                DumpAddrData( physAddr, Data, Count, "pwr" );
            }
        }
        ChecktoTrackAddress(Addr, true, TRACK_WRITE, Data, tempTrackTime);
        HwMemCopy(Addr, Data, Count);
        return;
    }

    // Split accesses that span a page boundary.
    FirstByte = uintptr_t(Addr);
    LastByte  = FirstByte + Count - 1;
    FirstPage = FirstByte >> SimPageTable::SimPageShift;
    LastPage  = LastByte  >> SimPageTable::SimPageShift;
    if (FirstPage != LastPage)
    {
        // Partial or full first page
        VirtualWr(Addr, Data, UNSIGNED_CAST(UINT32,
            SimPageTable::SimPageSize - (FirstByte & SimPageTable::SimPageMask)));

        // Full middle pages, if any
        for (uintptr_t Page = FirstPage+1; Page <= LastPage-1; Page++)
        {
            VirtualWr((volatile UINT08 *)(Page << SimPageTable::SimPageShift),
                      (const UINT08 *)Data + ((Page << SimPageTable::SimPageShift) - FirstByte),
                      UNSIGNED_CAST(UINT32, SimPageTable::SimPageSize));
        }

        // Partial or full last page
        VirtualWr((volatile UINT08 *)(LastPage << SimPageTable::SimPageShift),
                  (const UINT08 *)Data + ((LastPage << SimPageTable::SimPageShift) - FirstByte),
                  UNSIGNED_CAST(UINT32, (LastByte & SimPageTable::SimPageMask) + 1));

        return;
    }

    DumpAddrData( Addr, Data, Count, "vwr" );

    // Page must be valid and writeable
    const PageTableEntry *Pte = SimPageTable::GetVirtualAddrPte(Addr);
    if (!Pte)
    {
        // It's not in the sim page table.  This might be plain old system memory
        // from malloc, or it might be a completely bogus address.  Just try it;
        // if it's bogus, it'll crash.
        memcpy(const_cast<void *>(Addr), Data, Count);
        return;
    }
    if (!(Pte->Flags & PAGE_WRITEABLE))
    {
        Printf(Tee::PriHigh, "VirtualRd(%p) caused a write protection violation\n", Addr);
        MASSERT(0);
        return;
    }
    if (Pte->Flags & PAGE_DIRECT_MAP)
    {
        if (s_RemoteHardware)
        {
            MASSERT(hasPhysAddr);
            if (s_pBusMem->BusMemWrBlk(physAddr, Data, Count))
            {
                Printf(Tee::PriError,
                       "Error: Platform::%s Failed remote write physical address %llx Count=%d\n",
                        __FUNCTION__,
                        physAddr, Count);
                MASSERT(0);
            }
        }
        else
        {
            memcpy(const_cast<void *>(Addr), Data, Count);
        }

        if (s_DumpPhysicalAccess)
        {
            MASSERT(hasPhysAddr);
            DumpAddrData(physAddr, Data, Count, "pwr");
        }
        return;
    }

    MASSERT(hasPhysAddr);

    // Simulate write combining
    // Note that writes to non-WC memory do *not* flush WC.
    if (Pte->Flags & PAGE_WRITE_COMBINED)
    {
        if (s_WcSize)
        {
            // There is already something in the WC buffer.  Can we add to it?
            // - Must immediately follow the previous write's address
            // - Must not overflow the buffer
            if ((physAddr == s_WcBaseAddr + s_WcSize) &&
                (s_WcSize + Count <= WRITE_COMBINE_BUFFER_SIZE))
            {
                // Yes; add it to the buffer and early-out without a real write
                memcpy(&s_WcBuffer[s_WcSize], Data, Count);
                s_WcSize += Count;
                return;
            }

            // No luck.  Flush this batch of writes.
            rc = FlushCpuWriteCombineBuffer();
            if (rc != OK)
            {
                Printf(Tee::PriHigh, "VirtualWr(%p) failed: %s\n", Addr, rc.Message());
                MASSERT(0);
            }
            MASSERT(!s_WcSize);
        }

        // WC buffer is empty.  If we can, start anew.
        if (Count <= WRITE_COMBINE_BUFFER_SIZE)
        {
            // Put the write in the buffer and early-out without a real write
            memcpy(s_WcBuffer, Data, Count);
            s_WcBaseAddr = physAddr;
            s_WcSize = Count;
            return;
        }
    }

    rc = PhysWr(physAddr, Data, Count);
    if (rc != OK)
    {
        Printf(Tee::PriHigh, "VirtualWr(%p) failed: %s\n", Addr, rc.Message());
        MASSERT(0);
    }
}

UINT08 Platform::VirtualRd08(const volatile void *Addr)
{
    UINT08 Data;
    if ((s_SimulationMode == Hardware) && !s_DumpVirtualAccess && !s_SubstituteRegisters.get() &&
        !s_DumpPhysicalAccess && !s_PhysicalAddress && !s_TrackAll && !s_RemoteHardware && !s_HybridEmulation)
        Data = *(const volatile UINT08 *)Addr;
    else
        VirtualRd(Addr, &Data, 1);
    return Data;
}

UINT16 Platform::VirtualRd16(const volatile void *Addr)
{
    UINT16 Data;
    if ((s_SimulationMode == Hardware) && !s_DumpVirtualAccess && !s_SubstituteRegisters.get() &&
        !s_DumpPhysicalAccess && !s_PhysicalAddress && !s_TrackAll && !s_RemoteHardware && !s_HybridEmulation)
        Data = *(const volatile UINT16 *)Addr;
    else
        VirtualRd(Addr, &Data, 2);
    return Data;
}

UINT32 Platform::VirtualRd32(const volatile void *Addr)
{
    UINT32 Data;
    if ((s_SimulationMode == Hardware) && !s_DumpVirtualAccess && !s_SubstituteRegisters.get() &&
        !s_DumpPhysicalAccess && !s_PhysicalAddress && !s_TrackAll && !s_RemoteHardware && !s_HybridEmulation)
        Data = *(const volatile UINT32 *)Addr;
    else
        VirtualRd(Addr, &Data, 4);
    return Data;
}

UINT64 Platform::VirtualRd64(const volatile void *Addr)
{
    UINT64 Data;
    if ((s_SimulationMode == Hardware) && !s_DumpVirtualAccess && !s_SubstituteRegisters.get() &&
        !s_DumpPhysicalAccess && !s_PhysicalAddress && !s_TrackAll && !s_RemoteHardware && !s_HybridEmulation)
        Data = *(const volatile UINT64 *)Addr;
    else
        VirtualRd(Addr, &Data, 8);
    return Data;
}

void Platform::VirtualWr08(volatile void *Addr, UINT08 Data)
{
    if ((s_SimulationMode == Hardware) && !s_DumpVirtualAccess && !s_SubstituteRegisters.get() &&
        !s_DumpPhysicalAccess && !s_PhysicalAddress && !s_TrackAll && !s_RemoteHardware && !s_HybridEmulation)
        *(volatile UINT08 *)Addr = Data;
    else
        VirtualWr(Addr, &Data, 1);
}

void Platform::VirtualWr16(volatile void *Addr, UINT16 Data)
{
    if ((s_SimulationMode == Hardware) && !s_DumpVirtualAccess && !s_SubstituteRegisters.get() &&
        !s_DumpPhysicalAccess  && !s_PhysicalAddress && !s_TrackAll && !s_RemoteHardware && !s_HybridEmulation)
        *(volatile UINT16 *)Addr = Data;
    else
        VirtualWr(Addr, &Data, 2);
}

void Platform::VirtualWr32(volatile void *Addr, UINT32 Data)
{
    if ((s_SimulationMode == Hardware) && !s_DumpVirtualAccess && !s_SubstituteRegisters.get() &&
        !s_DumpPhysicalAccess  && !s_PhysicalAddress && !s_TrackAll && !s_RemoteHardware && !s_HybridEmulation)
        *(volatile UINT32 *)Addr = Data;
    else
        VirtualWr(Addr, &Data, 4);
}

void Platform::VirtualWr64(volatile void *Addr, UINT64 Data)
{
    if ((s_SimulationMode == Hardware) && !s_DumpVirtualAccess && !s_SubstituteRegisters.get() &&
        !s_DumpPhysicalAccess  && !s_PhysicalAddress && !s_TrackAll && !s_RemoteHardware && !s_HybridEmulation)
        *(volatile UINT64 *)Addr = Data;
    else
        VirtualWr(Addr, &Data, 8);
}

UINT32 Platform::VirtualXchg32(volatile void *Addr, UINT32 Data)
{
    if ((s_SimulationMode == Hardware) && !s_DumpVirtualAccess && !s_SubstituteRegisters.get() &&
        !s_DumpPhysicalAccess  && !s_PhysicalAddress && !s_TrackAll && !s_RemoteHardware && !s_HybridEmulation)
    {
        return Cpu::AtomicXchg32((volatile UINT32 *)Addr, Data);
    }
    UINT32 Temp = VirtualRd32(Addr);
    VirtualWr32(Addr, Data);
    return Temp;
}

//------------------------------------------------------------------------------
// Read and write physical addresses
//

// This implementation of IsSysmemAccess is bogus if the sysmem range
// collides with addresses mapped via Platform::MapDeviceMemory()
bool IsSysmemAccess( PHYSADDR Addr )
{
    if ( s_MultiHeapSupported )
    {
        MHeapSimPtr m;
        if ( OK == m->SpansPhysAddr( Addr ) ) return true;
        return false;
    }
    else
    {
        return ( (Addr >= s_SimSysMemBase) && (Addr < s_SimSysMemBase+s_SimSysMemSize) );
    }
}

static PHYSADDR FixSysmemAddress(PHYSADDR Addr, UINT32* pAppendedBdf)
{
    // About s_SimulationMode check:
    // Amodel: no plan to support SRIOV(must be DefaultMode)
    // Fmodel/RTL: support to decode the appended sysmem PA
    // Silicon:no need to append bdf because CPU VA is mapped for different VMs
    //
    if (Platform::IsDefaultMode() ||
        (s_SimulationMode == Platform::Hardware && !s_HybridEmulation) ||
        !IsSysmemAccess(Addr))
    {
        if (pAppendedBdf)
            *pAppendedBdf = 0;

        return Addr;
    }

    static map<UINT32, UINT64> s_GfidTobdfAddrCach;

    VmiopElwManager *pMgr = VmiopElwManager::Instance();
    UINT32 gfid = pMgr->GetGfidAttachedToProcess();
    UINT64 bdfAddr = 0;

    if (s_GfidTobdfAddrCach.count(gfid))
    {
        bdfAddr = s_GfidTobdfAddrCach[gfid];
    }
    else
    {
        MASSERT(s_ForceChiplibSysmem); // chiplib managed sysmem
        MASSERT(s_pBusMem);            // 64-bit physical address support

        UINT32 domain = 0;
        UINT32 bus = 0;
        UINT32 device = 0;
        UINT32 fun = 0;

        // FIX-ME: the gfid in GetDomainBusDevFun take gpu instance id into consideration.
        // while GetGfidAttachedToProcess not.
        if (OK != pMgr->GetDomainBusDevFun(gfid, &domain, &bus, &device, &fun))
        {
            MASSERT(!"GetDomainBusDevFun() failed");
            return Addr;
        }

        bdfAddr = Pci::GetConfigAddress(domain, bus, device, fun, 0)
            >> DRF_SIZE(PCI_CONFIG_ADDRESS_OFFSET_LO);
        MASSERT((bdfAddr & 0xFFFF0000) == 0);
        s_GfidTobdfAddrCach[gfid] = bdfAddr;
    }

    if (pAppendedBdf)
    {
        *pAppendedBdf = static_cast<UINT32>(bdfAddr);
    }
    return Addr | (bdfAddr << GetSimSysmemBdfShift());
}

RC Platform::PhysRd(PHYSADDR Addr, void *Data, UINT32 Count)
{
    UINT64 tempTrackTime = SetupTracking();

    vector<Device *> Devices;

    if (s_SubstituteRegisters.get() && (SubstitutePhysRd(Addr, Data, Count) == OK))
    {
        return OK;
    }

    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        void *VirtAddr = PhysicalToVirtual(Addr);
        VirtualRd(VirtAddr, Data, Count);

        DumpAddrData( Addr, Data, Count, "prd" );

        ChecktoTrackAddress( (const volatile void*)Addr, false, TRACK_READ, Data, tempTrackTime);
        return OK;
    }

    // System memory access?
    if ( IsSysmemAccess( Addr ) )
    {
        // Are we responsible for system memory?
        if (s_ChiplibSupportsExternalSysmem)
        {
            // Copy from the original memory
            char *Src = (char *)PhysicalToVirtual(Addr);

            // Verify a very important assumption that no DMA will span allocations
            // This restriction could be lifted with some effort.
            MASSERT(Src + Count - 1 == PhysicalToVirtual(Addr + Count - 1));

            memcpy(Data, Src, Count);
            DumpAddrData( Addr, Data, Count, "prd" );

            ChecktoTrackAddress( (const volatile void*)Addr, false, TRACK_READ, Data, tempTrackTime);
            return OK;
        }
    }

    // NOTE : while multiple devices are lwrrently checked for munging the
    // physical address and whether the address is an Amodel address, we
    // still send the address to the static device allocated in platform
    // rather than a specific device based on the address.

    IMemory *pMem = s_pMem;
    IBusMem *pBusMem = s_pBusMem;

    // IBusMem is preferred over IMemory
    PHYSADDR fixedAddr = FixSysmemAddress(Addr, nullptr);
    if (pBusMem)
    {
        // Do the read transaction
        BusMemRet ret = pBusMem->BusMemRdBlk(fixedAddr, Data, Count);
        if (ret != BUSMEM_HANDLED)
            MASSERT(!"BusMemRdBlk failed!");
    }
    else
    {
        // IMemory doesn't support 64-bit physical addresses
        MASSERT(fixedAddr <= 0xFFFFFFFF);
        UINT32 Addr32 = UNSIGNED_CAST(UINT32, fixedAddr);

        // Do the read transaction
        pMem->MemRdBlk(Addr32, Data, Count);
    }

    DumpAddrData( fixedAddr, Data, Count, "prd" );

    ChecktoTrackAddress( (const volatile void*)fixedAddr, false, TRACK_READ, Data, tempTrackTime);
    return OK;
}

RC Platform::PhysWr(PHYSADDR Addr, const void *Data, UINT32 Count)
{
    UINT64 tempTrackTime = SetupTracking();

    if (s_SubstituteRegisters.get() && (SubstitutePhysWr(Addr, Data, Count) == OK))
    {
        return OK;
    }

    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        void *VirtAddr = PhysicalToVirtual(Addr);
        VirtualWr(VirtAddr, Data, Count);

        DumpAddrData( Addr, Data, Count, "pwr" );

        ChecktoTrackAddress( (const volatile void*)Addr, false, TRACK_WRITE, Data, tempTrackTime);
        return OK;
    }

    // System memory access?
    if ( IsSysmemAccess( Addr ) )
    {
        // Are we responsible for system memory?
        if (s_ChiplibSupportsExternalSysmem)
        {
            // Copy to the original memory
            char *Dst = (char *)PhysicalToVirtual(Addr);

            // Verify a very important assumption that no DMA will span allocations
            // This restriction could be lifted with some effort.
            MASSERT(Dst + Count - 1 == PhysicalToVirtual(Addr + Count - 1));

            memcpy(Dst, Data, Count);
            DumpAddrData( Addr, Data, Count, "pwr" );

            ChecktoTrackAddress( (const volatile void*)Addr, false, TRACK_WRITE, Data, tempTrackTime);
            return OK;
        }
    }

    // NOTE : while multiple devices are lwrrently checked for munging the
    // physical address and whether the address is an Amodel address, we
    // still send the address to the static device allocated in platform
    // rather than a specific device based on the address.

    IMemory *pMem = s_pMem;
    IBusMem *pBusMem = s_pBusMem;

    // IBusMem is preferred over IMemory
    PHYSADDR fixedAddr = FixSysmemAddress(Addr, nullptr);
    if (pBusMem)
    {
        // Do the write transaction
        BusMemRet ret = pBusMem->BusMemWrBlk(fixedAddr, Data, Count);
        if (ret != BUSMEM_HANDLED)
            MASSERT(!"BusMemWrBlk failed");
    }
    else
    {
        // IMemory doesn't support 64-bit physical addresses
        MASSERT(fixedAddr <= 0xFFFFFFFF);
        UINT32 Addr32 = UNSIGNED_CAST(UINT32, fixedAddr);

        // Do the write transaction
        pMem->MemWrBlk(Addr32, Data, Count);
    }

    DumpAddrData( fixedAddr, Data, Count, "pwr" );

    ChecktoTrackAddress( (const volatile void*)fixedAddr, false, TRACK_WRITE, Data, tempTrackTime);
    return OK;
}

UINT08 Platform::PhysRd08(PHYSADDR Addr)
{
    UINT08 Data;
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        void *VirtAddr = PhysicalToVirtual(Addr);
        return VirtualRd08(VirtAddr);
    }
    RC rc = PhysRd(Addr, &Data, 1);
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "PhysRd08 failed: %s\n", rc.Message());
        MASSERT(0);
        Data = 0xFF;
    }
    return Data;
}

UINT16 Platform::PhysRd16(PHYSADDR Addr)
{
    UINT16 Data;
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        void *VirtAddr = PhysicalToVirtual(Addr);
        return VirtualRd16(VirtAddr);
    }
    RC rc = PhysRd(Addr, &Data, 2);
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "PhysRd16 failed: %s\n", rc.Message());
        MASSERT(0);
        Data = 0xFFFF;
    }
    return Data;
}

UINT32 Platform::PhysRd32(PHYSADDR Addr)
{
    UINT32 Data;
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        void *VirtAddr = PhysicalToVirtual(Addr);
        return VirtualRd32(VirtAddr);
    }
    RC rc = PhysRd(Addr, &Data, 4);
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "PhysRd32 failed: %s\n", rc.Message());
        MASSERT(0);
        Data = 0xFFFFFFFF;
    }
    return Data;
}

UINT64 Platform::PhysRd64(PHYSADDR Addr)
{
    UINT64 Data;
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        void *VirtAddr = PhysicalToVirtual(Addr);
        return VirtualRd64(VirtAddr);
    }
    RC rc = PhysRd(Addr, &Data, 8);
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "PhysRd64 failed: %s\n", rc.Message());
        MASSERT(0);
        Data = 0xFFFFFFFFFFFFFFFFULL;
    }
    return Data;
}

void Platform::PhysWr08(PHYSADDR Addr, UINT08 Data)
{
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        void *VirtAddr = PhysicalToVirtual(Addr);
        VirtualWr08(VirtAddr, Data);
        return;
    }
    RC rc = PhysWr(Addr, &Data, 1);
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "PhysWr08 failed: %s\n", rc.Message());
        MASSERT(0);
    }
}

void Platform::PhysWr16(PHYSADDR Addr, UINT16 Data)
{
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        void *VirtAddr = PhysicalToVirtual(Addr);
        VirtualWr16(VirtAddr, Data);
        return;
    }
    RC rc = PhysWr(Addr, &Data, 2);
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "PhysWr16 failed: %s\n", rc.Message());
        MASSERT(0);
    }
}

void Platform::PhysWr32(PHYSADDR Addr, UINT32 Data)
{
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        void *VirtAddr = PhysicalToVirtual(Addr);
        VirtualWr32(VirtAddr, Data);
        return;
    }
    RC rc = PhysWr(Addr, &Data, 4);
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "PhysWr32 failed: %s\n", rc.Message());
        MASSERT(0);
    }
}

void Platform::PhysWr64(PHYSADDR Addr, UINT64 Data)
{
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        void *VirtAddr = PhysicalToVirtual(Addr);
        VirtualWr64(VirtAddr, Data);
        return;
    }
    RC rc = PhysWr(Addr, &Data, 8);
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "PhysWr64 failed: %s\n", rc.Message());
        MASSERT(0);
    }
}

UINT32 Platform::PhysXchg32(PHYSADDR Addr, UINT32 Data)
{
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        void *VirtAddr = PhysicalToVirtual(Addr);
        return VirtualXchg32(VirtAddr, Data);
    }
    UINT32 Temp = PhysRd32(Addr);
    PhysWr32(Addr, Data);
    return Temp;
}

//------------------------------------------------------------------------------
//! \brief Memory chunk copy on HW (silicon/emulator).
//!        Locally callable only.
static void Platform::HwMemCopy(volatile void *Dst, const volatile void *Src, size_t Count)
{
    MASSERT(s_SimulationMode == Hardware);

    if (GetUse4ByteMemCopy())
        Platform::Hw4ByteMemCopy (Dst, Src, Count);
    else
        Cpu::MemCopy(const_cast<void *>(Dst), const_cast<const void *>(Src), Count);
}

//------------------------------------------------------------------------------
void Platform::MemCopy(volatile void *Dst, const volatile void *Src, size_t Count)
{
    if ((s_SimulationMode == Hardware) && !s_SubstituteRegisters.get() &&
        !s_DumpPhysicalAccess && !s_PhysicalAddress && !s_TrackAll && !s_DumpVirtualAccess  && !s_RemoteHardware && !s_HybridEmulation)
    {
        HwMemCopy(Dst, Src, Count);
        return;
    }

    // XXX This could maybe be improved further with some attention to alignment
    const size_t BATCH_SIZE = 1024;
    UINT08 Temp[BATCH_SIZE];
    while (Count >= BATCH_SIZE)
    {
        VirtualRd(Src, Temp, BATCH_SIZE);
        VirtualWr(Dst, Temp, BATCH_SIZE);
        Count -= BATCH_SIZE;
        Src = (const volatile UINT08 *)Src + BATCH_SIZE;
        Dst = (volatile UINT08 *)Dst + BATCH_SIZE;
    }
    if (Count > 0)
    {
        VirtualRd(Src, Temp, (UINT32)Count);
        VirtualWr(Dst, Temp, (UINT32)Count);
    }
}

//------------------------------------------------------------------------------
void Platform::MemCopy4Byte(volatile void *Dst, const volatile void *Src, size_t Count)
{
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        Platform::Hw4ByteMemCopy (Dst, Src, Count);
    }
    else
    {
        Platform::MemCopy (Dst, Src, Count);
    }
}

//------------------------------------------------------------------------------
void Platform::MemSet(volatile void *Dst, UINT08 Data, size_t Count)
{
    if ((s_SimulationMode == Hardware) && !s_SubstituteRegisters.get() &&
        !s_DumpPhysicalAccess && !s_PhysicalAddress && !s_TrackAll && !s_DumpVirtualAccess && !s_RemoteHardware && !s_HybridEmulation)
    {
        Cpu::MemSet(const_cast<void *>(Dst), Data, Count);
    }
    else
    {
        const size_t BatchSize = 1024;
        UINT08 Temp[BatchSize];
        memset(Temp, Data, sizeof(Temp));
        while (Count > 0)
        {
            const size_t WriteSize = min(Count, sizeof(Temp));
            VirtualWr(Dst, Temp, (UINT32)WriteSize);
            Count -= WriteSize;
            Dst = static_cast<volatile UINT08*>(Dst) + WriteSize;
        }
    }
}

//------------------------------------------------------------------------------
//!< Return the page size in bytes for this platform.
size_t Platform::GetPageSize()
{
    if (s_pChip)
    {
        return SimPageTable::SimPageSize;
    }
    else
    {
        return Xp::GetPageSize();
    }
}

//------------------------------------------------------------------------------
//!< Dump the lwrrently mapped pages.
void Platform::DumpPageTable()
{
    if (s_pChip)
    {
        SimPageTable::Dump();
    }
    else
    {
        Xp::DumpPageTable();
    }
}

//------------------------------------------------------------------------------
// Helper function to get flags from MODS attributes/protections data types
static UINT32 GetPageFlags(Memory::Attrib Attrib, Memory::Protect Protect)
{
    // TODO: need to extend this to full set of Memory::Attrib?
    UINT32 Flags = 0;
    if (Protect & Memory::Readable)
        Flags |= PAGE_READABLE;
    if (Protect & Memory::Writeable)
        Flags |= PAGE_WRITEABLE;
    if (Attrib == Memory::WC)
        Flags |= PAGE_WRITE_COMBINED;
    return Flags;
}

//------------------------------------------------------------------------------
//!< Alloc system memory.
//!< A whole number of pages will be mapped (NumBytes will be rounded up).
//!< All pages will be locked and will be below physical address 2**32.
//!< If Contiguous is true, physical pages will be adjacent and in-order.
PHYSADDR GetSysmemBaseForPAddr( PHYSADDR Addr )
{
    if ( s_MultiHeapSupported )
    {
        PHYSADDR b = 0;
        MHeapSimPtr m;
        if ( OK == m->GetAllocBase( Addr, &b ) )
        {
            return b;
        }
        MASSERT( 0 && "Bad PHYSADDR in GetSysmemSizeForPAddr");
        return 0;
    }
    else
    {
        return s_SimSysMemBase;
    }
}

UINT64 GetSysmemSizeForPAddr( PHYSADDR Addr )
{
    if ( s_MultiHeapSupported )
    {
        UINT64 s = 0;
        MHeapSimPtr m;
        if ( OK == m->GetAllocSize( Addr, &s ) )
        {
            return s;
        }
        MASSERT( 0 && "Bad PHYSADDR in GetSysmemSizeForPAddr");
        return 0;
    }
    else
    {
        return s_SimSysMemSize;
    }
}

RC Platform::TestAllocPages
(
    size_t      NumBytes
)
{
    if ((s_SimulationMode == Hardware && !s_HybridEmulation) && s_MultiHeapSupported)
    {
        // Class to release allocated memory
        class AutoFree
        {
        public:
            AutoFree(vector<UINT64>& allocs) : m_Allocs(allocs) { }
            ~AutoFree()
            {
                if (s_pChiplibMultiHeap2)
                {
                    if (s_pChiplibMultiHeap2->Support64())
                    {
                        for (size_t i=0; i < m_Allocs.size(); i++)
                        {
                            s_pChiplibMultiHeap2->FreeSysMem64(m_Allocs[i]);
                        }
                    }
                    else
                    {
                        for (size_t i=0; i < m_Allocs.size(); i++)
                        {
                            s_pChiplibMultiHeap2->FreeSysMem32(
                                UNSIGNED_CAST(LwU032, m_Allocs[i]));
                        }
                    }
                }
                else
                {
                    if (s_pChiplibMultiHeap->Support64())
                    {
                        for (size_t i=0; i < m_Allocs.size(); i++)
                        {
                            s_pChiplibMultiHeap->FreeSysMem64(m_Allocs[i]);
                        }
                    }
                    else
                    {
                        for (size_t i=0; i < m_Allocs.size(); i++)
                        {
                            s_pChiplibMultiHeap->FreeSysMem32(
                                UNSIGNED_CAST(LwU032, m_Allocs[i]));
                        }
                    }
                }
            }
        private:
            vector<UINT64>& m_Allocs;
        };

        // Allocate requested amount of memory
        vector<UINT64> allocs;
        AutoFree autoFreeAllocs(allocs);
        size_t remainingSize = NumBytes;
        while (remainingSize > 0)
        {
            // Try to allocate a chunk as big as possible
            const size_t minChunkSize = 128*1024;
            size_t allocSize = remainingSize;
            bool ok = false;
            while (allocSize > minChunkSize)
            {
                UINT64 virtAddr = 0;
                int ret = -1;
                if (s_pChiplibMultiHeap2)
                {
                    if (s_pChiplibMultiHeap2->Support64())
                    {
                        ret = s_pChiplibMultiHeap2->AllocSysMem64(
                                allocSize, Memory::UC, 1024,
                                &virtAddr);
                    }
                    else
                    {
                        UINT32 virtAddr32 = 0;
                        ret = s_pChiplibMultiHeap2->AllocSysMem32(
                                static_cast<UINT32>(allocSize), Memory::UC, 1024,
                                &virtAddr32);
                        virtAddr = virtAddr32;
                    }
                }
                else
                {
                    if (s_pChiplibMultiHeap->Support64())
                    {
                        ret = s_pChiplibMultiHeap->AllocSysMem64(
                                allocSize, Memory::UC, 1024,
                                &virtAddr);
                    }
                    else
                    {
                        UINT32 virtAddr32 = 0;
                        ret = s_pChiplibMultiHeap->AllocSysMem32(
                                static_cast<UINT32>(allocSize), Memory::UC, 1024,
                                &virtAddr32);
                        virtAddr = virtAddr32;
                    }
                }
                if (ret)
                {
                    // With hwchip2.so we allocate contiguous sysmem
                    // from the kernel using regular alloc API and
                    // the largest contiguous allocation we can typically
                    // obtain is 4MB.
                    const size_t knownGoodAllocSize = 4*1024*1024;
                    if (allocSize > knownGoodAllocSize)
                    {
                        allocSize = knownGoodAllocSize;
                    }
                    else
                    {
                        allocSize /= 2;
                    }
                }
                else
                {
                    allocs.push_back(virtAddr);
                    ok = true;
                    break;
                }
            }
            if (!ok)
            {
                Printf(Tee::PriNormal, "Unable to allocate %llu bytes of system memory\n",
                        static_cast<UINT64>(NumBytes));
                return RC::CANNOT_ALLOCATE_MEMORY;
            }
            remainingSize -= allocSize;
        }
    }

    return OK;
}

//------------------------------------------------------------------------------
//!
//! Wrapper function for Xp::AllocAddressSpace to allow for aligning to
//! different sysmem page sizes.
//!
//! Corresponding Xp::FreeAddressSpace calls must use the AddressSpace and
//! AddressSpaceBytes fields of the PageAllocDescriptor object.
//!
RC AllocAlignedAddressSpace
(
    PageAllocDescriptor * pageDesc,
    UINT64                pageSize,
    void *                pBase,
    UINT32                protect,
    UINT32                vaFlags
)
{
    RC rc;

    pageDesc->AddressSpaceBytes = pageDesc->NumBytes;

    // Xp::AllocAddressSpace only supports 4K pages, so we need to add some
    // extra space for aligning the returned address.
    if (pageSize > 4096)
    {
        pageDesc->AddressSpaceBytes += pageSize;
    }

    CHECK_RC(Xp::AllocAddressSpace(
        pageDesc->AddressSpaceBytes,
        &pageDesc->AddressSpace,
        pBase,
        protect,
        vaFlags));

    size_t offset = ((size_t)pageDesc->AddressSpace) & (pageSize - 1);

    // If the returned address doesn't fall on the specified page size boundary,
    // align up to the next page boundary.
    if (offset > 0)
    {
        pageDesc->VirtualAddress = static_cast<char *>(pageDesc->AddressSpace) +
            pageSize - offset;
    }
    else
    {
        pageDesc->VirtualAddress = pageDesc->AddressSpace;
    }

    return rc;
}

namespace
{
    RC AllocVirtualPagesIfNeeded
    (
        PHYSADDR             physAddr,
        size_t               numBytes,
        Memory::Attrib       attrib,
        Memory::Protect      protect,
        PageAllocDescriptor *pDesc,
        UINT32              *pFlags
    )
    {
        MASSERT(pDesc);
        MASSERT(pFlags);
        RC rc;

        if (pDesc->VirtualAddress == nullptr)
        {
            if (pDesc->Shared)
            {
                // Contig required for such vGpu RPC shared mem.
                MASSERT(pDesc->Contiguous);
                const bool readOnly = (protect & Memory::Writeable) == 0;
                CHECK_RC(SharedSysmem::Alloc(
                                VGPU_GUEST_MEMORY_NAME, physAddr, numBytes,
                                readOnly, &pDesc->VirtualAddress));
                // Leverage Desc.AddressSpace to store PhysAddr for later use,
                // because when SimPageSize > 4K (e.g. 64k), VirtAddr may not
                // align with SimPageSize, hence this shared mem can't use
                // VirtAddr in GetPhysicalAddress() or FreePages() to find back
                // correct PhysAddr.
                MASSERT(0 == pDesc->AddressSpace);
                pDesc->AddressSpace = reinterpret_cast<void*>(physAddr);
                *pFlags = GetPageFlags(attrib, protect);
                *pFlags |= PAGE_DIRECT_MAP;
            }
            else if (s_ChiplibSupportsExternalSysmem)
            {
                // Allocate real page-aligned memory
                pDesc->VirtualAddress =
                    Pool::AllocAligned(numBytes, SimPageTable::SimPageSize);
                if (!pDesc->VirtualAddress)
                {
                    return RC::CANNOT_ALLOCATE_MEMORY;
                }

                // We should access the memory directly through the
                // mapping; this will increase performance
                *pFlags = GetPageFlags(attrib, Memory::ReadWrite);
                *pFlags |= PAGE_DIRECT_MAP;
            }
            else
            {
                // Allocate inaccessible virtual address space
                CHECK_RC(AllocAlignedAddressSpace(
                                pDesc,
                                SimPageTable::SimPageSize,
                                nullptr,
                                Memory::ProtectDefault,
                                Memory::VirualAllocDefault));

                *pFlags = GetPageFlags(attrib, Memory::ReadWrite);
                if (s_pMapMem || s_pMapMemExt)
                    *pFlags |= PAGE_DIRECT_MAP;
            }
        }

        if (!pDesc->Shared) // Skip SimPage CPU PTE if vGpu RPC shared mem.
        {
            CHECK_RC(SimPageTable::PopulatePages(pDesc->VirtualAddress, numBytes));
        }
        return rc;
    }
}

//------------------------------------------------------------------------------
//! \brief Allocate system memory.
//!
//! \param numBytes    : Number of bytes to allocate (actual allocation will be
//!                      a whole number of pages (numBytes will be rounded up).
//! \param pDescriptor : Returned descriptor for the allocated memory
//! \param contiguous  : If true, physical pages will be adjacent and in-order.
//! \param addressBits : All pages will be locked and will be below physical
//!                      address 2**AddressBits.
//! \param attrib      : Memory attributes to use when allocating
//! \param domain      : PCI domain of the device for which to allocate the memory.
//! \param bus         : PCI bus of the device for which to allocate the memory.
//! \param device      : PCI device of the device for which to allocate the memory.
//! \param function    : PCI function of the device for which to allocate the memory.
//!
//! \return OK if memory was successfully allocated and mapped, not OK otherwise
RC Platform::AllocPages
(
    size_t          numBytes,
    void **         pDescriptor,
    bool            contiguous,
    UINT32          addressBits,
    Memory::Attrib  attrib,
    UINT32          domain,
    UINT32          bus,
    UINT32          device,
    UINT32          function
)
{
    return AllocPages(numBytes, pDescriptor, contiguous, addressBits,
                      attrib, 0U, Memory::ProtectDefault);
}

//------------------------------------------------------------------------------
//! \brief Alloc system memory.
//!
//! \param NumBytes    : Number of bytes to allocate (actual allocation will be
//!                      a whole number of pages (NumBytes will be rounded up).
//! \param pDescriptor : Returned descriptor for the allocated memory
//! \param Contiguous  : If true, physical pages will be adjacent and in-order.
//! \param AddressBits : All pages will be locked and will be below physical
//!                      address 2**AddressBits.
//! \param Attrib      : Memory attributes to use when allocating
//! \param GpuInst     : If GpuInst is valid, then on a NUMA system the system
//!                      memory will be on the node local to the GPU
//!
//! \return OK if memory was successfully allocated and mapped, not OK otherwise
RC Platform::AllocPages
(
    size_t          NumBytes,
    void **         pDescriptor,
    bool            Contiguous,
    UINT32          AddressBits,
    Memory::Attrib  Attrib,
    UINT32          GpuInst,
    Memory::Protect Protect
)
{
    RC rc;
    PageAllocDescriptor *Desc;

    PHYSADDR MaxPhysAddr;
    AddressBits = Platform::GetAllowedAddressBits(AddressBits);

#if defined(PPC64LE)
    if (Attrib != Memory::WB)
    {
        Attrib = Memory::WB;
        Printf(Tee::PriHigh, "AllocPages attrib colwerted to WB. PPC64LE must use cached sysmem pages.\n");
    }
#endif

    if (!s_pChip)
    {
        return Xp::AllocPages(NumBytes, pDescriptor,
                              Contiguous, AddressBits, Attrib,
                              GpuInst);
    }

    if (AddressBits > 64)
        AddressBits = 64;
    MaxPhysAddr = (2ULL << (AddressBits-1)) - 1;

    // Must allocate memory in multiples of a page
    NumBytes = (NumBytes + SimPageTable::SimPageMask) & ~SimPageTable::SimPageMask;

    // We need a descriptor
    Desc = new PageAllocDescriptor;
    if (!Desc)
        return RC::CANNOT_ALLOCATE_MEMORY;
    Desc->VirtualAddress         = NULL;
    Desc->NumBytes               = NumBytes;
    Desc->Shared                 = false;
    Desc->RemoteVirtualAddresses = NULL;
    Desc->AddressSpace           = 0;
    Desc->AddressSpaceBytes      = 0;

    if ((Protect & Memory::ProcessShared) && Platform::IsVirtFunMode())
    {
        // Inter-process shared memory used by vGpu RPC.
        Desc->Shared = true;

        if (!Contiguous)
        {
            // For inter-process shared memory used by RPC, PA will be part of shared memory object name.
            // The existed shared memory allocation code doesn't really support multiple PAs
            // (see code: only 1st allocated PA is passed into AllocVirtualPagesIfNeeded). So this is a
            // limitation caused by how mods support RPC shared buffer. It doesn't exist in production sw stack.
            //
            // Since the biggest RPC memory is 64k, forcing it to be contiguous is the simplest and affordable
            // way to fix it in mods.
            Printf(Tee::PriDebug, "%s: Inter-process shared memory allocation for RPC. Override the "
                "Contiguous=false request with Contiguous=true\n", __FUNCTION__);
            Contiguous = true;
        }
    }
    Desc->Contiguous             = Contiguous;

    if (Contiguous)
    {
        // Allocate and map the physical memory in one big chunk
        PHYSADDR PhysAddr;
        Heap *h;
        if ( s_MultiHeapSupported )
        {
            MHeapSimPtr m;
            h = m->Alloc(Attrib, NumBytes, MaxPhysAddr, GpuInst);
        }
        else
        {
            h = s_SimSysHeap;
        }
        if (!h)
        {
            rc = RC::CANNOT_ALLOCATE_MEMORY;
            goto Cleanup;
        }
        CHECK_RC_CLEANUP(h->AllocRandom(
            s_PteRandom ? &s_SimSysHeapRandom : NULL,
            NumBytes, &PhysAddr,
            0, MaxPhysAddr, SimPageTable::SimPageSize));

        // If vGpu RPC requested shared mem, skip pte_reverse step.
        if (s_PteReverse && !Desc->Shared)
        {
            PHYSADDR base = GetSysmemBaseForPAddr( PhysAddr );
            UINT64   size = GetSysmemSizeForPAddr( PhysAddr );
            PhysAddr -= base;
            PhysAddr = size - PhysAddr - NumBytes;
            PhysAddr += base;
        }

        UINT32 Flags = 0;
        CHECK_RC_CLEANUP(AllocVirtualPagesIfNeeded(PhysAddr, NumBytes, Attrib,
                                                   Protect, Desc, &Flags));

        if (!Desc->Shared)
        {
            // Alloc non vGpu RPC shared mem flow need the following steps.
            if (s_RemoteHardware)
            {
                void *RemoteVirtualAddress = 0;
                // When you allocate system memory, you specify the caching type during
                // allocation and that propagates to mappings. The kernel enforces that
                // the caching type during the mapping matches the caching type during allocation.
                // You cannot map sysmem pages with an attribute other than WB.
                // If you specify WB it will be ignored.

                if (s_pMapMemExt->MapMemoryRegion(&RemoteVirtualAddress,
                    PhysAddr, NumBytes, Memory::WB, Memory::ReadWrite))
                {
                    Printf(Tee::PriAlways,
                           "Error: Failed to remotely map physical address %llx\n", PhysAddr);
                    rc = RC::CANNOT_ALLOCATE_MEMORY;
                    goto Cleanup;
                }
                Desc->RemoteVirtualAddresses = new void*[1];
                Desc->RemoteVirtualAddresses[0] = RemoteVirtualAddress;
            } else if (s_pMapMemExt)
            {
                if (s_pMapMemExt->RemapPages(Desc->VirtualAddress, PhysAddr,
                        NumBytes, Memory::ReadWrite))
                {
                    Printf(Tee::PriAlways,
                           "Error: Failed to remap address %llx\n", PhysAddr);
                    rc = RC::CANNOT_ALLOCATE_MEMORY;
                    goto Cleanup;
                }
            }
            else if (s_pMapMem)
            {
                CHECK_RC_CLEANUP(Xp::RemapPages(
                    Desc->VirtualAddress, PhysAddr,
                    NumBytes, Attrib, Memory::ReadWrite));
            }
        }
        if (!Desc->Shared) // Skip SimPage CPU PTE if vGpu RPC shared mem.
        {
            SimPageTable::MapPages(Desc->VirtualAddress,
                               PhysAddr, NumBytes, Flags);
        }

        UpdateAllocationInfo(PhysAddr, NumBytes, Attrib);
    }
    else
    {
        // Allocate and map the physical memory one page at a time
        UINT64 i, NumPages;
        UINT32 Flags = 0;
        NumPages = NumBytes >> SimPageTable::SimPageShift;
        vector<PHYSADDR> PhysAddrs(NumPages);
        if (PhysAddrs.empty())
        {
            rc = RC::CANNOT_ALLOCATE_MEMORY;
            goto Cleanup;
        }
        for (i = 0; i < NumPages; i++)
        {
            Heap *h = NULL;
            if ( s_MultiHeapSupported )
            {
                MHeapSimPtr m;

                h = m->Alloc(Attrib, SimPageTable::SimPageSize, MaxPhysAddr, GpuInst);
            }
            else
            {
                h = s_SimSysHeap;
            }
            if (!h)
            {
                rc = RC::CANNOT_ALLOCATE_MEMORY;
                goto Cleanup;
            }
            CHECK_RC_CLEANUP(h->AllocRandom(
                s_PteRandom ? &s_SimSysHeapRandom : NULL,
                SimPageTable::SimPageSize, &PhysAddrs[i],
                0, MaxPhysAddr, SimPageTable::SimPageSize));

            if (s_PteReverse)
            {
                PHYSADDR base = GetSysmemBaseForPAddr( PhysAddrs[i] );
                UINT64   size = GetSysmemSizeForPAddr( PhysAddrs[i] );
                PhysAddrs[i] -= base;
                PhysAddrs[i] = size - PhysAddrs[i] - SimPageTable::SimPageSize;
                PhysAddrs[i] += base;
            }

            UpdateAllocationInfo(PhysAddrs[i], SimPageTable::SimPageSize, Attrib);
        }

        CHECK_RC_CLEANUP(AllocVirtualPagesIfNeeded(PhysAddrs[0], NumBytes,
                                                   Attrib, Protect,
                                                   Desc, &Flags));

        if (s_RemoteHardware)
            Desc->RemoteVirtualAddresses = new void*[(size_t)NumPages];

        for (i = 0; i < NumPages; i++)
        {
            if (s_RemoteHardware)
            {
                void *RemoteVirtualAddress = 0;
                if (s_pMapMemExt->MapMemoryRegion(&RemoteVirtualAddress,
                    PhysAddrs[i], SimPageTable::SimPageSize, Memory::WB, Memory::ReadWrite))
                {
                    Printf(Tee::PriAlways,
                           "Error: Failed to remotely map physical address %llx\n", PhysAddrs[i]);
                    rc = RC::CANNOT_ALLOCATE_MEMORY;
                    goto Cleanup;
                }
                Desc->RemoteVirtualAddresses[i] = RemoteVirtualAddress;
            }
            else if (s_pMapMemExt)
            {
                if (s_pMapMemExt->RemapPages(
                        (char *)Desc->VirtualAddress + i*SimPageTable::SimPageSize,
                        PhysAddrs[i], SimPageTable::SimPageSize, Memory::ReadWrite))
                {
                    Printf(Tee::PriAlways,
                           "Error: Failed to remap address %llx\n",
                           PhysAddrs[i]);
                    rc = RC::CANNOT_ALLOCATE_MEMORY;
                    goto Cleanup;
                }
            }
            else if (s_pMapMem)
            {
                CHECK_RC_CLEANUP(Xp::RemapPages(
                    (char *)Desc->VirtualAddress + i*SimPageTable::SimPageSize,
                    PhysAddrs[i], SimPageTable::SimPageSize, Attrib, Memory::ReadWrite));
            }
            SimPageTable::MapPages((char *)Desc->VirtualAddress + i*SimPageTable::SimPageSize,
                                   PhysAddrs[i], SimPageTable::SimPageSize, Flags);
        }
    }

    *pDescriptor = Desc;
    return OK;

Cleanup:
    // Something went wrong.  We have to clean things up.
    if (Desc->VirtualAddress)
    {
        if (Desc->Shared)
            SharedSysmem::Free(Desc->VirtualAddress);
        else if (s_ChiplibSupportsExternalSysmem)
            Pool::Free(Desc->VirtualAddress);
        else
            Xp::FreeAddressSpace(Desc->AddressSpace, Desc->AddressSpaceBytes,
                                 Memory::VirtualFreeDefault);
        SimPageTable::UnpopulatePages(Desc->VirtualAddress, NumBytes);
    }
    // XXX Free physical pages
    delete Desc;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Alloc contiguous system memory with pages aligned as requested
//!
//! \param NumBytes    : Number of bytes to allocate (actual allocation will be
//!                      a whole number of pages (NumBytes will be rounded up).
//! \param pDescriptor : Returned descriptor for the allocated memory
//! \param PhysAlign   : alignment mask, should be power of 2
//! \param AddressBits : All pages will be locked and will be below physical
//!                      address 2**AddressBits.
//! \param Attrib      : Memory attributes to use when allocating
//! \param GpuInst     : IGNORED (For other platforms, if GpuInst is valid, then
//!                      on a NUMA system the system memory will be on the node
//!                      local to the GPU)
//!
//! \return OK if memory was successfully allocated and mapped, not OK otherwise
RC Platform::AllocPagesAligned
(
    size_t         NumBytes,
    void **        pDescriptor,
    size_t         PhysAlign,
    UINT32         AddressBits,
    Memory::Attrib Attrib,
    UINT32         GpuInst
)
{
    RC rc;
    PageAllocDescriptor *Desc;
    PHYSADDR PhysAddr, MaxPhysAddr;

    AddressBits = Platform::GetAllowedAddressBits(AddressBits);

#if defined(PPC64LE)
    if (Attrib != Memory::WB)
    {
        Attrib = Memory::WB;
        Printf(Tee::PriHigh, "AllocPages attrib colwerted to WB. PPC64LE must use cached sysmem pages.\n");
    }
#endif

    if (!s_pChip)
    {
        return Xp::AllocPagesAligned(NumBytes, pDescriptor,
                                     PhysAlign, AddressBits, Attrib, GpuInst);
    }

    if (AddressBits > 64)
        AddressBits = 64;
    MaxPhysAddr = (2ULL << (AddressBits-1)) - 1;

    if (0 != (PhysAlign & (PhysAlign-1)))
    {
        Printf(Tee::PriHigh, "PhysAlign must be a power of 2,\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    // Must allocate and align memory in multiples of a page
    // XXX would be nice to lift this restriction
    NumBytes = (NumBytes + SimPageTable::SimPageMask) & ~SimPageTable::SimPageMask;
    PhysAlign = (PhysAlign + SimPageTable::SimPageMask) & ~SimPageTable::SimPageMask;

    // We need a descriptor
    Desc = new PageAllocDescriptor;
    if (!Desc)
        return RC::CANNOT_ALLOCATE_MEMORY;
    Desc->VirtualAddress    = NULL;
    Desc->NumBytes          = NumBytes;
    Desc->Contiguous        = true;
    Desc->Shared            = false;
    Desc->AddressSpace      = 0;
    Desc->AddressSpaceBytes = 0;

    // First, allocate the required virtual address space
    UINT32 Flags = GetPageFlags(Attrib, Memory::ReadWrite);
    if (s_ChiplibSupportsExternalSysmem)
    {
        // Allocate real page-aligned memory
        Desc->VirtualAddress = Pool::AllocAligned(NumBytes, SimPageTable::SimPageSize);
        if (!Desc->VirtualAddress)
        {
            rc = RC::CANNOT_ALLOCATE_MEMORY;
            goto Cleanup;
        }

        // We should access the memory directly through the mapping; this will
        // increase performance
        Flags |= PAGE_DIRECT_MAP;
    }
    else
    {
        // Allocate inaccessible virtual address space
        CHECK_RC_CLEANUP(AllocAlignedAddressSpace(
            Desc,
            SimPageTable::SimPageSize,
            nullptr,
            Memory::ProtectDefault,
            Memory::VirualAllocDefault));

        if (s_pMapMem || s_pMapMemExt)
            Flags |= PAGE_DIRECT_MAP;
    }
    CHECK_RC_CLEANUP(SimPageTable::PopulatePages(Desc->VirtualAddress, NumBytes));

    // Allocate and map the physical memory in one big chunk
    Heap *h;
    if ( s_MultiHeapSupported )
    {
        MHeapSimPtr m;
        h = m->AllocAligned(Attrib, NumBytes, PhysAlign, MaxPhysAddr, GpuInst);
    }
    else
    {
        h = s_SimSysHeap;
    }
    if (!h)
    {
        rc = RC::CANNOT_ALLOCATE_MEMORY;
        goto Cleanup;
    }
    CHECK_RC_CLEANUP(h->AllocRandom(
        s_PteRandom ? &s_SimSysHeapRandom : NULL,
        NumBytes, &PhysAddr,
        0, MaxPhysAddr, PhysAlign));

    if (s_PteReverse)
    {
        PHYSADDR base = GetSysmemBaseForPAddr( PhysAddr );
        UINT64   size = GetSysmemSizeForPAddr( PhysAddr );
        PhysAddr -= base;
        PhysAddr = size - PhysAddr - NumBytes;
        PhysAddr += base;
    }

    if (s_RemoteHardware)
    {
        void *RemoteVirtualAddress = 0;
        if (s_pMapMemExt->MapMemoryRegion(&RemoteVirtualAddress,
            PhysAddr, NumBytes, Memory::WB, Memory::ReadWrite))
        {
            Printf(Tee::PriAlways,
                   "Error: Failed to remotely map physical address %llx\n", PhysAddr);
            rc = RC::CANNOT_ALLOCATE_MEMORY;
            goto Cleanup;
        }
        Desc->RemoteVirtualAddresses = new void*[1];
        Desc->RemoteVirtualAddresses[0] = RemoteVirtualAddress;
    }
    else if (s_pMapMemExt)
    {
        if (s_pMapMemExt->RemapPages(Desc->VirtualAddress, PhysAddr,
                NumBytes, Memory::ReadWrite))
        {
            Printf(Tee::PriAlways,
                   "Error: Failed to remap address %llx\n", PhysAddr);
            rc = RC::CANNOT_ALLOCATE_MEMORY;
            goto Cleanup;
        }
    }
    else if (s_pMapMem)
    {
        CHECK_RC_CLEANUP(Xp::RemapPages(
            Desc->VirtualAddress, PhysAddr,
            NumBytes, Attrib, Memory::ReadWrite));
    }
    SimPageTable::MapPages(Desc->VirtualAddress,
                           PhysAddr, NumBytes, Flags);

    UpdateAllocationInfo(PhysAddr, NumBytes, Attrib);
    *pDescriptor = Desc;
    return OK;

Cleanup:
    // Something went wrong.  We have to clean things up.
    if (Desc->VirtualAddress)
    {
        if (s_ChiplibSupportsExternalSysmem)
            Pool::Free(Desc->VirtualAddress);
        else
            Xp::FreeAddressSpace(Desc->AddressSpace, Desc->AddressSpaceBytes, Memory::VirtualFreeDefault);
        SimPageTable::UnpopulatePages(Desc->VirtualAddress, NumBytes);
    }
    // XXX Free physical pages
    delete Desc;
    return rc;
}

//------------------------------------------------------------------------------
//!< Alloc system memory.
//!< A whole number of pages will be mapped (NumBytes will be rounded up).
//!< All pages will be locked and will be below physical address 2**32.
//!< If Contiguous is true, physical pages will be adjacent and in-order.
//! The pages will be allocated in the 4GB arena in which the address ConstrainAddr
//! is located.
RC Platform::AllocPagesConstrained
(
    size_t      NumBytes,
    void **     pDescriptor,
    bool        Contiguous,
    UINT32      AddressBits,
    Memory::Attrib Attrib,
    PHYSADDR    ConstrainAddr,
    UINT32      GpuInst
)
{
    if (s_RemoteHardware)
    {
        Printf(Tee::PriHigh, "AllocPagesConstrained is not supported on RemoteHardware\n");
        MASSERT(!"AllocPagesConstrained is not supported on RemoteHardware\n");
        return RC::SOFTWARE_ERROR;
    }
    if (s_MultiHeapSupported)
    {
        Printf(Tee::PriAlways,
            "Platform::AllocPagesConstrained not multiheap ready\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    RC rc;
    PageAllocDescriptor *Desc;
    PHYSADDR MaxPhysAddr;
    PHYSADDR MinPhysAddr;

    MinPhysAddr = (ConstrainAddr >> 32) << 32;
    MaxPhysAddr = (((ConstrainAddr >> 32 ) + 1) << 32) - 1 ;

#if defined(PPC64LE)
    if (Attrib != Memory::WB)
    {
        Attrib = Memory::WB;
        Printf(Tee::PriHigh, "AllocPages attrib colwerted to WB. PPC64LE must use cached sysmem pages.\n");
    }
#endif

    if (!s_pChip)
    {
        return Xp::AllocPages(NumBytes, pDescriptor,
                              Contiguous,
                              GetAllowedAddressBits(AddressBits),
                              Attrib,
                              GpuInst);
    }

    if (AddressBits > 64)
        AddressBits = 64;

    // Must allocate memory in multiples of a page
    NumBytes = (NumBytes + SimPageTable::SimPageMask) & ~SimPageTable::SimPageMask;

    // We need a descriptor
    Desc = new PageAllocDescriptor;
    if (!Desc)
        return RC::CANNOT_ALLOCATE_MEMORY;
    Desc->VirtualAddress    = NULL;
    Desc->NumBytes          = NumBytes;
    Desc->Contiguous        = Contiguous;
    Desc->Shared            = 0;
    Desc->AddressSpace      = 0;
    Desc->AddressSpaceBytes = 0;

    // First, allocate the required virtual address space
    UINT32 Flags = GetPageFlags(Attrib, Memory::ReadWrite);
    if (s_ChiplibSupportsExternalSysmem)
    {
        // Allocate real page-aligned memory
        Desc->VirtualAddress = Pool::AllocAligned(NumBytes, SimPageTable::SimPageSize);
        if (!Desc->VirtualAddress)
        {
            rc = RC::CANNOT_ALLOCATE_MEMORY;
            goto Cleanup;
        }

        // We should access the memory directly through the mapping; this will
        // increase performance
        Flags |= PAGE_DIRECT_MAP;
    }
    else
    {
        // Allocate inaccessible virtual address space
        CHECK_RC_CLEANUP(AllocAlignedAddressSpace(
            Desc,
            SimPageTable::SimPageSize,
            nullptr,
            Memory::ProtectDefault,
            Memory::VirualAllocDefault));

        if (s_pMapMem || s_pMapMemExt)
            Flags |= PAGE_DIRECT_MAP;
    }
    CHECK_RC_CLEANUP(SimPageTable::PopulatePages(Desc->VirtualAddress, NumBytes));

    Printf(Tee::PriNormal, "Constrained Memory Min physical base = 0x%016llX\n", MinPhysAddr);
    Printf(Tee::PriNormal, "Constrained Memory Max physical base = 0x%016llX\n", MaxPhysAddr);

    if (Contiguous)
    {
        // Allocate and map the physical memory in one big chunk
        PHYSADDR PhysAddr;

        CHECK_RC_CLEANUP(s_SimSysHeap->AllocRandom(
            s_PteRandom ? &s_SimSysHeapRandom : NULL,
            NumBytes, &PhysAddr,
            MinPhysAddr, MaxPhysAddr, SimPageTable::SimPageSize));

        if (s_PteReverse)
        {
            PhysAddr -= s_SimSysMemBase;
            PhysAddr = s_SimSysMemSize - PhysAddr - NumBytes;
            PhysAddr += s_SimSysMemBase;
        }
        if (s_pMapMemExt)
        {
            if (s_pMapMemExt->RemapPages(Desc->VirtualAddress, PhysAddr,
                    NumBytes, Memory::ReadWrite))
            {
                Printf(Tee::PriAlways,
                       "Error: Failed to remap address %llx\n", PhysAddr);
                rc = RC::CANNOT_ALLOCATE_MEMORY;
                goto Cleanup;
            }
        }
        else if (s_pMapMem)
        {
            CHECK_RC_CLEANUP(Xp::RemapPages(
                Desc->VirtualAddress, PhysAddr,
                NumBytes, Attrib, Memory::ReadWrite));
        }
        SimPageTable::MapPages(Desc->VirtualAddress,
                               PhysAddr, NumBytes, Flags);

        UpdateAllocationInfo(PhysAddr, NumBytes, Attrib);
    }
    else
    {
        // Allocate and map the physical memory one page at a time
        UINT64 i, NumPages;
        NumPages = NumBytes >> SimPageTable::SimPageShift;
        vector<PHYSADDR> PhysAddrs(NumPages);
        if (PhysAddrs.empty())
        {
            rc = RC::CANNOT_ALLOCATE_MEMORY;
            goto Cleanup;
        }
        for (i = 0; i < NumPages; i++)
        {
            CHECK_RC_CLEANUP(s_SimSysHeap->AllocRandom(
                s_PteRandom ? &s_SimSysHeapRandom : NULL,
                SimPageTable::SimPageSize, &PhysAddrs[i],
                MinPhysAddr, MaxPhysAddr, SimPageTable::SimPageSize));
            if (s_PteReverse)
            {
                PhysAddrs[i] -= s_SimSysMemBase;
                PhysAddrs[i] = s_SimSysMemSize - PhysAddrs[i] - SimPageTable::SimPageSize;
                PhysAddrs[i] += s_SimSysMemBase;
            }
            UpdateAllocationInfo(PhysAddrs[i], SimPageTable::SimPageSize, Attrib);
        }
        for (i = 0; i < NumPages; i++)
        {
            if (s_pMapMemExt)
            {
                if (s_pMapMemExt->RemapPages(
                    (char *)Desc->VirtualAddress + i*SimPageTable::SimPageSize,
                    PhysAddrs[i], SimPageTable::SimPageSize, Memory::ReadWrite))
                {
                    Printf(Tee::PriAlways,
                           "Error: Failed to map address %llx\n", PhysAddrs[i]);
                    rc = RC::CANNOT_ALLOCATE_MEMORY;
                    goto Cleanup;
                }
            }
            else if (s_pMapMem)
            {
                CHECK_RC_CLEANUP(Xp::RemapPages(
                    (char *)Desc->VirtualAddress + i*SimPageTable::SimPageSize,
                    PhysAddrs[i], SimPageTable::SimPageSize, Attrib, Memory::ReadWrite));
            }
            SimPageTable::MapPages((char *)Desc->VirtualAddress + i*SimPageTable::SimPageSize,
                                   PhysAddrs[i], SimPageTable::SimPageSize, Flags);
        }
    }

    *pDescriptor = Desc;
    return OK;

Cleanup:
    // Something went wrong.  We have to clean things up.
    if (Desc->VirtualAddress)
    {
        if (s_ChiplibSupportsExternalSysmem)
            Pool::Free(Desc->VirtualAddress);
        else
            Xp::FreeAddressSpace(Desc->AddressSpace, Desc->AddressSpaceBytes, Memory::VirtualFreeDefault);
        SimPageTable::UnpopulatePages(Desc->VirtualAddress, NumBytes);
    }
    // XXX Free physical pages
    delete Desc;
    return rc;
}

//------------------------------------------------------------------------------
//!< Alloc contiguous System memory pages, with the starting physical
//!< address aligned as requested.
//!< A whole number of pages will be mapped (NumBytes will be rounded up).
//!< All pages will be locked and will be below physical address 2**32.
//!< PhysicalAlignment must be a power of 2, values less than the system
//!< page size will be ignored.
//! The pages will be allocated in the 4GB arena in which the address ConstrainAddr
//! is located.
RC Platform::AllocPagesConstrainedAligned
(
    size_t      NumBytes,
    void **     pDescriptor,
    size_t      PhysAlign,
    UINT32      AddressBits,
    Memory::Attrib Attrib,
    PHYSADDR    ConstrainAddr,
    UINT32      GpuInst
)
{
    if (s_RemoteHardware)
    {
        Printf(Tee::PriHigh, "AllocPagesConstrainedAligned is not supported on RemoteHardware\n");
        MASSERT(!"AllocPagesConstrainedAligned is not supported on RemoteHardware\n");
        return RC::SOFTWARE_ERROR;
    }
    if (s_MultiHeapSupported)
    {
        Printf(Tee::PriAlways,
           "Platform::AllocPagesConstrainedAligned not multiheap ready\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    RC rc;
    PageAllocDescriptor *Desc;
    PHYSADDR PhysAddr, MaxPhysAddr;
    PHYSADDR MinPhysAddr;

    MinPhysAddr = (ConstrainAddr >> 32) << 32;
    MaxPhysAddr = (((ConstrainAddr >> 32 ) + 1) << 32) - 1 ;

#if defined(PPC64LE)
    if (Attrib != Memory::WB)
    {
        Attrib = Memory::WB;
        Printf(Tee::PriHigh, "AllocPages attrib colwerted to WB. PPC64LE must use cached sysmem pages.\n");
    }
#endif

    if (!s_pChip)
    {
        return Xp::AllocPagesAligned(NumBytes, pDescriptor,
                                     PhysAlign,
                                     GetAllowedAddressBits(AddressBits),
                                     Attrib, GpuInst);
    }

    if (AddressBits > 64)
        AddressBits = 64;

    if (0 != (PhysAlign & (PhysAlign-1)))
    {
        Printf(Tee::PriHigh, "PhysAlign must be a power of 2,\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    // Must allocate and align memory in multiples of a page
    // XXX would be nice to lift this restriction
    NumBytes = (NumBytes + SimPageTable::SimPageMask) & ~SimPageTable::SimPageMask;
    PhysAlign = (PhysAlign + SimPageTable::SimPageMask) & ~SimPageTable::SimPageMask;

    // We need a descriptor
    Desc = new PageAllocDescriptor;
    if (!Desc)
        return RC::CANNOT_ALLOCATE_MEMORY;
    Desc->VirtualAddress    = NULL;
    Desc->NumBytes          = NumBytes;
    Desc->Contiguous        = true;
    Desc->Shared            = 0;
    Desc->AddressSpace      = 0;
    Desc->AddressSpaceBytes = 0;

    // First, allocate the required virtual address space
    UINT32 Flags = GetPageFlags(Attrib, Memory::ReadWrite);
    if (s_ChiplibSupportsExternalSysmem)
    {
        // Allocate real page-aligned memory
        Desc->VirtualAddress = Pool::AllocAligned(NumBytes, SimPageTable::SimPageSize);
        if (!Desc->VirtualAddress)
        {
            rc = RC::CANNOT_ALLOCATE_MEMORY;
            goto Cleanup;
        }

        // We should access the memory directly through the mapping; this will
        // increase performance
        Flags |= PAGE_DIRECT_MAP;
    }
    else
    {
        // Allocate inaccessible virtual address space
        CHECK_RC_CLEANUP(AllocAlignedAddressSpace(
            Desc,
            SimPageTable::SimPageSize,
            nullptr,
            Memory::ProtectDefault,
            Memory::VirualAllocDefault));

        if (s_pMapMem || s_pMapMemExt)
            Flags |= PAGE_DIRECT_MAP;
    }
    CHECK_RC_CLEANUP(SimPageTable::PopulatePages(Desc->VirtualAddress, NumBytes));

    Printf(Tee::PriNormal, "Constrained Memory Min physical base = 0x%016llX\n", MinPhysAddr);
    Printf(Tee::PriNormal, "Constrained Memory Max physical base = 0x%016llX\n", MaxPhysAddr);

    // Allocate and map the physical memory in one big chunk
    CHECK_RC_CLEANUP(s_SimSysHeap->AllocRandom(
        s_PteRandom ? &s_SimSysHeapRandom : NULL,
        NumBytes, &PhysAddr,
        MinPhysAddr, MaxPhysAddr, SimPageTable::SimPageSize));
    if (s_PteReverse)
    {
        PhysAddr -= s_SimSysMemBase;
        PhysAddr = s_SimSysMemSize - PhysAddr - NumBytes;
        PhysAddr += s_SimSysMemBase;
    }
    if (s_pMapMemExt)
    {
        if (s_pMapMemExt->RemapPages(Desc->VirtualAddress, PhysAddr,
                NumBytes, Memory::ReadWrite))
        {
            Printf(Tee::PriAlways,
                   "Error: Failed to map address %llx\n", PhysAddr);
            rc = RC::CANNOT_ALLOCATE_MEMORY;
            goto Cleanup;
        }
    }
    else if (s_pMapMem)
    {
        CHECK_RC_CLEANUP(Xp::RemapPages(
            Desc->VirtualAddress, PhysAddr,
            NumBytes, Attrib, Memory::ReadWrite));
    }
    SimPageTable::MapPages(Desc->VirtualAddress,
                           PhysAddr, NumBytes, Flags);

    UpdateAllocationInfo(PhysAddr, NumBytes, Attrib);
    *pDescriptor = Desc;
    return OK;

Cleanup:
    // Something went wrong.  We have to clean things up.
    if (Desc->VirtualAddress)
    {
        if (s_ChiplibSupportsExternalSysmem)
            Pool::Free(Desc->VirtualAddress);
        else
            Xp::FreeAddressSpace(Desc->AddressSpace, Desc->AddressSpaceBytes, Memory::VirtualFreeDefault);
        SimPageTable::UnpopulatePages(Desc->VirtualAddress, NumBytes);
    }
    // XXX Free physical pages
    delete Desc;
    return rc;
}

//------------------------------------------------------------------------------
//!< Free pages allocated by AllocPages or AllocPagesAligned.
void Platform::FreePages(void * Descriptor)
{
    PHYSADDR PhysAddr;

    PageAllocDescriptor *Desc = (PageAllocDescriptor *)Descriptor;

    if (!s_pChip)
    {
        Xp::FreePages(Descriptor);
        return;
    }

    // Free the physical memory
    if (Desc->Contiguous)
    {
        if (Desc->Shared)
        {
            // For vGpu RPC shared mem, need get back the recorded phys addr.
            // The one got from Desc->VirtualAddress maybe incorrect.
            MASSERT(Desc->AddressSpace != 0);
            PhysAddr = reinterpret_cast<PHYSADDR>(Desc->AddressSpace);
        }
        else
        {
            // Need skip the following steps if freeing vGpu RPC shared mem.
            PhysAddr = VirtualToPhysical(Desc->VirtualAddress);
            if (s_PteReverse)
            {
                PHYSADDR base = GetSysmemBaseForPAddr( PhysAddr );
                UINT64   size = GetSysmemSizeForPAddr( PhysAddr );
                PhysAddr -= base;
                PhysAddr = size - PhysAddr - Desc->NumBytes;
                PhysAddr += base;
            }
            if (s_RemoteHardware)
            {
                s_pMapMemExt->UnMapMemoryRegion(Desc->RemoteVirtualAddresses[0]);
                delete[] Desc->RemoteVirtualAddresses;
                Desc->RemoteVirtualAddresses = NULL;
            }
            else if (s_pMapMemExt)
            {
                s_pMapMemExt->UnMapMemoryRegion(Desc->VirtualAddress);
            }
        }
        if ( s_MultiHeapSupported )
        {
            MHeapSimPtr m;
            if ( OK != m->Free( PhysAddr ) )
            {
                MASSERT(0 && "Attempt to free bad PHYSADDR");
            }
        }
        else
        {
            s_SimSysHeap->Free(PhysAddr);
        }
    }
    else
    {
        for (UINT64 i = 0; i < Desc->NumBytes; i += SimPageTable::SimPageSize)
        {
            PhysAddr = VirtualToPhysical((char *)Desc->VirtualAddress + i);
            if (s_PteReverse)
            {
                PHYSADDR base = GetSysmemBaseForPAddr( PhysAddr );
                UINT64   size = GetSysmemSizeForPAddr( PhysAddr );
                PhysAddr -= base;
                PhysAddr = size - PhysAddr - SimPageTable::SimPageSize;
                PhysAddr += base;
            }
            if (s_RemoteHardware)
            {
                s_pMapMemExt->UnMapMemoryRegion(Desc->RemoteVirtualAddresses[i >> SimPageTable::SimPageShift]);
            }
            else if (s_pMapMemExt)
            {
                s_pMapMemExt->UnMapMemoryRegion((char *)Desc->VirtualAddress + i);
            }
            if ( s_MultiHeapSupported )
            {
                MHeapSimPtr m;
                if ( OK != m->Free( PhysAddr ) )
                {
                    MASSERT(0 && "Attempt to free bad PHYSADDR");
                }
            }
            else
            {
                s_SimSysHeap->Free(PhysAddr);
            }
        }
        if (s_RemoteHardware)
        {
            delete[] Desc->RemoteVirtualAddresses;
            Desc->RemoteVirtualAddresses = NULL;
        }
    }

    if (Desc->Shared)
    {
        // Skipped SimPage CPU PTE if vGpu RPC shared mem.

        // Free the shared memory that we allocated
        SharedSysmem::Free(Desc->VirtualAddress);
    }
    else
    {
        // Remove the memory from the page table
        SimPageTable::UnmapPages(Desc->VirtualAddress, Desc->NumBytes);

        if (s_ChiplibSupportsExternalSysmem)
        {
            // Free the real memory that we allocated
            Pool::Free(Desc->VirtualAddress);
        }
        else
        {
            // Free the address space we allocated
            Xp::FreeAddressSpace(Desc->AddressSpace, Desc->AddressSpaceBytes, Memory::VirtualFreeDefault);
        }
    }

    // Finally, free the descriptor
    delete Desc;
}

RC Platform::MergePages(void** pOutDescriptor, void** pInDescBegin, void** pInDescEnd)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Platform::MapPages
(
    void **pReturnedVirtualAddress,
    void * Descriptor,
    size_t Offset,
    size_t Size,
    Memory::Protect Protect
)
{
    if (!s_pChip)
    {
        return Xp::MapPages(pReturnedVirtualAddress, Descriptor, Offset, Size, Protect);
    }

    // XXX We should revisit this and create the real mappings at MapPages time.
    PageAllocDescriptor *Desc = (PageAllocDescriptor *)Descriptor;
    *pReturnedVirtualAddress = (UINT08 *)Desc->VirtualAddress + Offset;
    return OK;
}

//------------------------------------------------------------------------------
void Platform::UnMapPages(void * VirtualAddress)
{
    if (!s_pChip)
    {
        Xp::UnMapPages(VirtualAddress);
        return;
    }

    // Don't have to do anything
}

//------------------------------------------------------------------------------
PHYSADDR Platform::GetPhysicalAddress(void *Descriptor, size_t Offset)
{
    if (!s_pChip)
    {
        return Xp::GetPhysicalAddress(Descriptor, Offset);
    }

    PageAllocDescriptor *Desc = (PageAllocDescriptor *)Descriptor;
    PHYSADDR physAddr = 0;
    if (Desc->Shared)
    {
        // When SimPageSize > 4K (e.g. 64k), this vGpu RPC shared mem VirtAddr
        // may not align with SimPageSize, hence can't use VirtAddr to find back
        // expected PhysAddr using Sim PDE/PTE.
        // Need use saved PhysAddr in Desc.AddressSpace directly.
        MASSERT(Desc->Contiguous);
        MASSERT(Desc->AddressSpace != 0);
        physAddr = reinterpret_cast<PHYSADDR>(Desc->AddressSpace) + Offset;
    }
    else
    {
        physAddr = VirtualToPhysical((UINT08 *)Desc->VirtualAddress + Offset);
    }
    return physAddr;
}

//------------------------------------------------------------------------------
PHYSADDR Platform::GetMappedPhysicalAddress
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    void*  descriptor,
    size_t offset
)
{
    if (!s_pChip)
    {
        return Xp::GetMappedPhysicalAddress(domain, bus, device, func, descriptor, offset);
    }

    if (s_pPciDev)
    {
        // Return the non-device specific version (prevents simulation from sharing
        // sysmem allocations between GPUs until it is plumbed into hwchip
        PageAllocDescriptor* desc = (PageAllocDescriptor*)descriptor;
        PHYSADDR addr = VirtualToPhysical((UINT08*)desc->VirtualAddress + offset);
        // colwert to page boundary
        PHYSADDR pageAddr = (addr >> SimPageTable::SimPageShift) << SimPageTable::SimPageShift;
        UINT64 pageOffset = addr - pageAddr;

        PHYSADDR mappedAddress = ~0U;

        s_pPciDev->GetPciMappedPhysicalAddress(pageAddr,
                                               UNSIGNED_CAST(LwU032, pageOffset),
                                               &mappedAddress);
        return mappedAddress;
    }

    // On sim platforms without s_pPciDev, the mapped physical address is just the physical
    // address.  There is no IOMMU on these platforms
    return Platform::GetPhysicalAddress(descriptor, offset);
}

//------------------------------------------------------------------------------
RC Platform::DmaMapMemory
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    void  *descriptor
)
{
    if (!s_pPpc)
    {
        return Xp::DmaMapMemory(domain, bus, device, func, descriptor);
    }

    // Not yet supported on simulation (and not sure it is required).  Doing
    // nothing will cause simulation failures if a mapped physical address is
    // requested for anything but the device that the system memory was
    // allocated on.  I.e. until this is implemented simulation builds will
    // not support shared sysmem surfaces on a system with an IOMMU
    // Use s_pMapDma if this is ever needed.
    return OK;
}

//------------------------------------------------------------------------------
RC Platform::DmaUnmapMemory
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    void  *descriptor
)
{
    if (!s_pPpc)
    {
        return Xp::DmaUnmapMemory(domain, bus, device, func, descriptor);
    }

    // Not yet supported on simulation (and not sure it is required).  Doing
    // nothing will cause simulation failures if a mapped physical address is
    // requested for anything but the device that the system memory was
    // allocated on.  I.e. until this is implemented simulation builds will
    // not support shared sysmem surfaces on a system with an IOMMU
    // Use s_pMapDma if this is ever needed.
    return OK;
}

//------------------------------------------------------------------------------
RC Platform::SetDmaMask(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, UINT32 numBits)
{
    if (!s_pMapDma)
    {
        return Xp::SetDmaMask(domain, bus, device, func, numBits);
    }

    const LwPciDev pciDevice = {
        static_cast<UINT16>(domain),
        static_cast<UINT16>(bus),
        static_cast<UINT16>(device),
        static_cast<UINT16>(func)
    };

    return s_pMapDma->SetDmaMask(pciDevice, numBits) ? RC::UNSUPPORTED_FUNCTION : RC::OK;
}

//------------------------------------------------------------------------------
//!< Reserve resources for subsequent allocations of the same Attrib
//!< by AllocPages or AllocPagesAligned.
//!< Under DOS, this will pre-allocate a contiguous heap and MTRR reg.
//!< Platforms that use real paging memory managers may ignore this.
void Platform::ReservePages
(
    size_t NumBytes,
    Memory::Attrib Attrib
)
{
    if (s_pChip)
    {
        // nop
    }
    else
    {
        Xp::ReservePages(NumBytes, Attrib);
    }
}

//------------------------------------------------------------------------------
// Constants are from asm/mtrr.h, ick
static UINT16 GetMtrrType(Memory::Attrib Attrib)
{
    // TODO: properly map from attrib to mtrr kind
    if (Attrib == Memory::WB)
        return 6;
    else if (Attrib == Memory::WC)
        return 1;
    else
        return 0;
}

RC Platform::GetCarveout(PHYSADDR* pAddr, UINT64* pSize)
{
    MASSERT(pAddr);
    MASSERT(pSize);

    *pAddr = s_Carveout.base;
    *pSize = s_Carveout.size;
    return OK;
}

RC Platform::GetVPR(PHYSADDR* pAddr, UINT64* pSize, void **pMemDesc)
{
    MASSERT(pAddr);
    MASSERT(pSize);

    if (!s_VPR.valid)
    {
        UINT32 VPRSizeMb;
        RC rc;
        JavaScriptPtr pJs;
        CHECK_RC(pJs->GetProperty(Platform_Object, Platform_VPRSizeMb, &VPRSizeMb));
        const UINT64 oneMb = 1024U*1024U;
        const UINT64 size = VPRSizeMb * oneMb;

        if (IsTegra())
        {
            // On PEATrans and linsim, allocate VPR from device sysmem heap
            if (VPRSizeMb > 0)
            {
                Printf(Tee::PriDebug, "Trying to allocate %llu bytes of VPR\n", size);

                CHECK_RC(CreateCarveout(&s_VPR, size, oneMb));

                Printf(Tee::PriLow, "Allocated VPR at phys addr 0x%llX, size 0x%llX\n",
                        s_VPR.base, size);
            }
        }
        // On standalone GPU Fmodel, don't really allocate, but tell RM the size
        else
        {
            *pAddr = ~0ULL;
            *pSize = size;
            return OK;
        }
    }

    *pAddr = s_VPR.base;
    *pSize = s_VPR.size;
    if (pMemDesc)
    {
        *pMemDesc = s_VPR.pDesc;
    }

    return OK;
}

// Generic carveout allocation routine
static RC Platform::CreateCarveout(Carveout *c, UINT64 size, UINT64 align)
{
    RC rc;

    if (!IsTegra())
    {
        MASSERT(0);
        return RC::LWRM_NOT_SUPPORTED;
    }

    if ((size == 0) ||
        (c == NULL) ||
        (c->valid))
    {
        MASSERT(0);
        return RC::ILWALID_ARGUMENT;
    }

    if((align & (align - 1)) != 0)
    {
        MASSERT(0);
        return RC::ILWALID_ARGUMENT;
    }

    if (align == 0)
    {
        align = 1024;
    }

    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        LwU64 alignaddr, addr, alignOffset;

        addr        = (static_cast<UINT64>((uintptr_t)s_CarveoutReserveMapping.VirtualAddress)) + s_CarveoutUsedSize;
        //((UINT64)(s_CarveoutReserveMapping.VirtualAddress)) + s_CarveoutUsedSize;
        alignaddr   = LW_ALIGN_UP(addr, align);
        alignOffset = alignaddr - addr;

        if ((s_CarveoutUsedSize + alignOffset + size) <= s_CarveoutReserveMapping.NumBytes)
        {
            c->base             = (PHYSADDR)alignaddr;
            c->pDesc            = &s_CarveoutReserveMapping;
            c->descFromOthers   = true;
            s_CarveoutUsedSize += (size + alignOffset);
        }
        else
        {
            MASSERT(0);
            return RC::LWRM_INSUFFICIENT_RESOURCES;
        }
    }
    else if ((s_SimulationMode == Fmodel) || (s_SimulationMode == RTL) || s_HybridEmulation)
    {
        // Allocate carveout on simulation (linsim)
        CHECK_RC(Platform::AllocPagesAligned
        (
            size,
            &c->pDesc,
            align,
            64,
            Memory::UC,
            Platform::UNSPECIFIED_GPUINST
        ));

        c->base = Platform::GetPhysicalAddress(c->pDesc, 0);
    }
    else
    {
        MASSERT(0);
        return RC::ILWALID_ARGUMENT;
    }

    c->size  = size;
    c->valid = true;

    return OK;
}

static void Platform::DestroyCarveout(Carveout *c)
{
    MASSERT(c);

    if (!c->valid)
    {
        return;
    }

    if (c->pDesc && (!c->descFromOthers))
    {
        FreePages(c->pDesc);
        c->pDesc = NULL;
    }
    else
    {
        MASSERT(s_SimulationMode == Hardware && !s_HybridEmulation);
    }

    c->base  = ~0ULL;
    c->size  = 0;
    c->valid = false;
}

//
// Allocate Generalized carveouts on CheetAh.
// CheetAh Memory controller (MC) allows a number a contiguous system memory regions to be configured as carveouts.
// These carveouts can be configured to have different access rules.
// Access Controlled Region (ACR) is a generalized carveout that is used by GPU.
//
RC Platform::GetGenCarveout(PHYSADDR *pAddr, UINT64 *pSize, UINT32 id, UINT64 align)
{
    RC    rc;

    MASSERT(pAddr);
    MASSERT(pSize);

    if (id >= MAX_GEN_CARVEOUT)
    {
        return RC::LWRM_INSUFFICIENT_RESOURCES;
    }

    if (!s_GenCarveout[id].valid)
    {
        MASSERT(*pSize);
        CHECK_RC(CreateCarveout(&s_GenCarveout[id], *pSize, align));
        s_nC++;
    }
    else
    {
        MASSERT(*pSize <= s_GenCarveout[id].size);
    }

   *pAddr = s_GenCarveout[id].base;
   *pSize = s_GenCarveout[id].size;

    return OK;
}

//!< Map device memory into the page table, get virtual address to use.
//!< Multiple mappings are legal, and give different virtual addresses
//!< on paging systems.
//!< Under DOS, this is faked more-or-less correctly using MTRRs.
RC Platform::MapDeviceMemory
(
    void **     pReturnedVirtualAddress,
    PHYSADDR    PhysicalAddress,
    size_t      NumBytes,
    Memory::Attrib Attrib,
    Memory::Protect Protect
)
{
    RC rc;
    PHYSADDR PageBaseAddress;

    if (!s_pChip)
    {
        return Xp::MapDeviceMemory(pReturnedVirtualAddress, PhysicalAddress,
                                   NumBytes, Attrib, Protect);
    }

    bool IsSimSysMem = false;

    // We can only map integral numbers of pages.  Round down to the next page
    // boundary, and round the size up to a multiple of the page size.
    PageBaseAddress = PhysicalAddress & ~(PHYSADDR)SimPageTable::SimPageMask;
    NumBytes += size_t(PhysicalAddress - PageBaseAddress);
    NumBytes = (NumBytes + SimPageTable::SimPageMask) & ~(size_t)SimPageTable::SimPageMask;

    // If the chiplib has the ability to map memory for us, use it
    if (s_pMapMemExt)
    {
        // Add extra page in case we need to align to none native page size
        if (s_RemoteHardware && (SimPageTable::SimPageSize != SimPageTable::NativePageSize))
        {
            NumBytes += SimPageTable::SimPageSize;
        }

        if (s_pMapMemExt->MapMemoryRegion(pReturnedVirtualAddress,
                PageBaseAddress, NumBytes, Attrib, Protect))
        {
            Printf(Tee::PriAlways,
                   "Error: Failed to map address %llx\n", PageBaseAddress);
            return( RC::CANNOT_ALLOCATE_MEMORY );
        }
        if (s_RemoteHardware)
        {
            // Remote virtual address gets overwritten with a local here:
            void* remoteVirtual = *pReturnedVirtualAddress;
            CHECK_RC(Xp::AllocAddressSpace(NumBytes,
                                           pReturnedVirtualAddress,
                                           nullptr,
                                           Memory::ProtectDefault,
                                           Memory::VirualAllocDefault));

            void* localVirtual =  *pReturnedVirtualAddress;
            if (SimPageTable::SimPageSize != SimPageTable::NativePageSize)
            {
                *pReturnedVirtualAddress = reinterpret_cast<void*>((reinterpret_cast<LwU064>(*pReturnedVirtualAddress) & ~SimPageTable::SimPageMask) + SimPageTable::SimPageSize);
            }

            // Save remote virtual address for unmap
            s_RemoteVirtual[*pReturnedVirtualAddress] = remoteVirtual;
            // Save unaligned local virtual address for unmap
            s_LocalVirtual[*pReturnedVirtualAddress] = localVirtual;
        }
    }
    else if (s_pMapMem)
    {
        // Call the chip library to set up an MTRR, if needed
        UINT16 type = GetMtrrType(Attrib);
        if ((PageBaseAddress > 0xFFFFFFFF) || (NumBytes > 0xFFFFFFFF))
        {
            return RC::DATA_TOO_LARGE;
        }
        if (s_pMapMem->MapMemoryRegion((LwU032)PageBaseAddress, (LwU032)NumBytes, type))
        {
            Printf(Tee::PriError, "Failed to map address %llx\n", PageBaseAddress);
            return RC::SOFTWARE_ERROR;
        }

        // Call the OS implementation of MapDeviceMemory to create a mapping
        CHECK_RC(Xp::MapDeviceMemory(pReturnedVirtualAddress, PageBaseAddress,
                                     NumBytes, Attrib, Protect));
    }
    else
    {
        // Allocate virtual address space for the device
        if (SimPageTable::SimPageSize != SimPageTable::NativePageSize)
        {
            // Overallocate by one page
            CHECK_RC(Xp::AllocAddressSpace(NumBytes + SimPageTable::SimPageSize,
                                           pReturnedVirtualAddress,
                                           nullptr,
                                           Memory::ProtectDefault,
                                           Memory::VirualAllocDefault));
            void* OriginalAddress = *pReturnedVirtualAddress; // Save original virtual address

            // Align virtual address to SimPageSize
            *pReturnedVirtualAddress = reinterpret_cast<void*>((reinterpret_cast<LwU064>(*pReturnedVirtualAddress) & ~SimPageTable::SimPageMask) + SimPageTable::SimPageSize);
            s_AdjustedVirtual[*pReturnedVirtualAddress] = OriginalAddress; // Store original address so it can be used to free this mapping later
        }
        else
        {
            CHECK_RC(Xp::AllocAddressSpace(NumBytes,
                                           pReturnedVirtualAddress,
                                           nullptr,
                                           Memory::ProtectDefault,
                                           Memory::VirualAllocDefault));
        }

        IsSimSysMem = IsSysmemAccess( PhysicalAddress );
    }

    // ensure page is aligned

    MASSERT(((uintptr_t)*pReturnedVirtualAddress & (uintptr_t)SimPageTable::SimPageMask) == 0);

    CHECK_RC(SimPageTable::PopulatePages(*pReturnedVirtualAddress, NumBytes));

    // Add the device to the page table
    UINT32 Flags = GetPageFlags(Attrib, Protect);
    if (s_pMapMem || s_pMapMemExt)
        Flags |= PAGE_DIRECT_MAP;

    if (IsSimSysMem)
    {
        Flags |= PAGE_DIRECT_MAP_DUP;
        // This is simulated system memory.
        //
        // This sim-paddr already exists in a PTE entry for a different
        // sim-vaddr, where the sim-vaddr == real-vaddr (i.e. PAGE_DIRECT_MAP).
        //
        // Mark this new sim-vaddr->sim-paddr PTE specially so that we don't use
        // it in PhysicalToVirtual later.  We need PhysicalToVirtual to find
        // the original mapping containing the real-vaddr!
    }

    SimPageTable::MapPages(*pReturnedVirtualAddress,
                           PageBaseAddress, NumBytes, Flags);

    // Remember the mapping
    s_DeviceMappingSizes[*pReturnedVirtualAddress] = NumBytes;

    // Adjust the final pointer to takes into account the offset within the page.
    *pReturnedVirtualAddress =
        (UINT08 *)*pReturnedVirtualAddress + (PhysicalAddress & SimPageTable::SimPageMask);

    return OK;
}

//------------------------------------------------------------------------------
//!< Undo the mapping created by MapDeviceMemory.
void Platform::UnMapDeviceMemory(void * VirtualAddress)
{
    if (!s_pChip)
    {
        Xp::UnMapDeviceMemory(VirtualAddress);
        return;
    }

    // Allow unmaps of NULL
    if (!VirtualAddress)
        return;

    // This may have been a mapping that wasn't page-aligned.  Get the *real* base
    // address by rounding down.
    VirtualAddress = (void *)((uintptr_t)VirtualAddress & ~(uintptr_t)SimPageTable::SimPageMask);

    // Look up the size of the mapping
    MASSERT(s_DeviceMappingSizes.find(VirtualAddress) != s_DeviceMappingSizes.end());
    size_t NumBytes = s_DeviceMappingSizes[VirtualAddress];
    s_DeviceMappingSizes.erase(VirtualAddress);

    if (s_RemoteHardware)
    {
        SimPageTable::UnmapPages(VirtualAddress, NumBytes);
        RemoteVirtual::iterator remIt = s_RemoteVirtual.find(VirtualAddress);
        MASSERT(remIt != s_RemoteVirtual.end());
        s_pMapMemExt->UnMapMemoryRegion(remIt->second);
        s_RemoteVirtual.erase(remIt);
        LocalVirtual::iterator locIt = s_LocalVirtual.find(VirtualAddress);
        MASSERT(locIt != s_LocalVirtual.end());
        Xp::FreeAddressSpace(locIt->second, NumBytes, Memory::VirtualFreeDefault);
        s_LocalVirtual.erase(locIt);
        return;
    }

    // Remove the device from the sim page table
    SimPageTable::UnmapPages(VirtualAddress, NumBytes);

    if (s_pMapMemExt)
    {
        s_pMapMemExt->UnMapMemoryRegion(VirtualAddress);
    }
    else if (s_pMapMem)
    {
        // There is no "unmap" API that destroys MTRRs.  Go figure.
        Xp::UnMapDeviceMemory(VirtualAddress);
    }
    else
    {
        // Free virtual address space
        if (SimPageTable::SimPageSize != SimPageTable::NativePageSize)
        {
            Xp::FreeAddressSpace(s_AdjustedVirtual[VirtualAddress], NumBytes,
                                 Memory::VirtualFreeDefault);
        }
        else
        {
            Xp::FreeAddressSpace(VirtualAddress, NumBytes, Memory::VirtualFreeDefault);
        }

    }
}

//------------------------------------------------------------------------------
//!< Program a Memory Type and Range Register to control cache mode
//!< for a range of physical addresses.
//!< Called by the resman to make the (unmapped) AGP aperture write-combined.
RC Platform::SetMemRange
(
   PHYSADDR    PhysicalAddress,
   size_t      NumBytes,
   Memory::Attrib Attrib
)
{
    if (s_pChip)
    {
        if (s_pMapMemExt)
        {
            // not implemented
            return OK;
        }
        else if (s_pMapMem)
        {
            // Do a pretend map call to set up an MTRR.
            UINT16 type = GetMtrrType(Attrib);
            if ((PhysicalAddress > 0xFFFFFFFF) || (NumBytes > 0xFFFFFFFF))
            {
                return RC::DATA_TOO_LARGE;
            }
            if (s_pMapMem->MapMemoryRegion((LwU032)PhysicalAddress, (LwU032)NumBytes, type))
            {
                Printf(Tee::PriError, "Failed to map address %llx\n", PhysicalAddress);
                return RC::SOFTWARE_ERROR;
            }

            return OK;
        }
        else
        {
            // Not implemented, skip this.
            return OK;
        }
    }
    else
    {
        return Xp::SetMemRange(PhysicalAddress, NumBytes, Attrib);
    }
}

//------------------------------------------------------------------------------
RC Platform::UnSetMemRange
(
   PHYSADDR    PhysicalAddress,
   size_t      NumBytes
)
{
    if (s_pChip)
    {
        if (s_pMapMem || s_pMapMemExt)
        {
            // There is no "unmap" API.  Go figure.
            return OK;
        }
        else
        {
            // Not implemented, skip this.
            return OK;
        }
    }
    else
    {
        return Xp::UnSetMemRange(PhysicalAddress, NumBytes);
    }
}

//------------------------------------------------------------------------------
// Get ATS target address range.
//
RC Platform::GetAtsTargetAddressRange
(
    UINT32     gpuInst,
    UINT32    *pAddr,
    UINT32    *pAddrGuest,
    UINT32    *pAddrWidth,
    UINT32    *pMask,
    UINT32    *pMaskWidth,
    UINT32    *pGranularity,
    MODS_BOOL  bIsPeer,
    UINT32     peerIndex
)
{
    if (!s_pChip)
        return RC::UNSUPPORTED_FUNCTION;

    GpuDevMgr* const pGpuDevMgr =
        static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
    if (!pGpuDevMgr)
        return RC::UNSUPPORTED_FUNCTION;

    GpuSubdevice *pSubdev = pGpuDevMgr->GetSubdeviceByGpuInst(gpuInst);
    if (nullptr == pSubdev)
    {
        Printf(Tee::PriHigh, "%s : Unable to find subdevice with instance %u\n",
               __FUNCTION__, gpuInst);
        return RC::SUB_DEVICE_ILWALID_INDEX;
    }

    string atsRequest[8] = { "CPU_MODEL|CM_RM|AtsTargetAddressHi",
                             "CPU_MODEL|CM_RM|AtsTargetAddressLo",
                             "CPU_MODEL|CM_RM|AtsTargetAddressWidth",
                             "CPU_MODEL|CM_RM|AtsTargetMask",
                             "CPU_MODEL|CM_RM|AtsTargetMaskWidth",
                             "CPU_MODEL|CM_RM|AtsTargetGranularity",
                             "CPU_MODEL|CM_RM|AtsGuestAddressHi",
                             "CPU_MODEL|CM_RM|AtsGuestAddressLo" };

    const UINT32 Apertures = 9;
    string atsAperture[Apertures] = { "Peer0", "Peer1", "Peer2", "Peer3", "Peer4", "Peer5", "Peer6", "Peer7", "Local" };

    UINT64 addr;
    UINT32 addrHi, addrLo, guestAddrLo = 0, guestAddrHi = 0;
    UINT32 result = 0;
    UINT32 gpuId = pSubdev->GetGpuId();
    UINT32 index = bIsPeer ? peerIndex : Apertures - 1;

    MASSERT(index < Apertures);
    atsRequest[0] += atsAperture[index];
    atsRequest[1] += atsAperture[index];
    atsRequest[2] += atsAperture[index];
    atsRequest[3] += atsAperture[index];
    atsRequest[4] += atsAperture[index];
    atsRequest[5] += atsAperture[index];
    atsRequest[6] += atsAperture[index];
    atsRequest[7] += atsAperture[index];

    // AtsTargetAddressHi
    result = GpuEscapeRead(gpuId, atsRequest[0].c_str(), 0, 4, &addrHi);

    if (result != 0)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // AtsTargetAddressLo
    result = GpuEscapeRead(gpuId, atsRequest[1].c_str(), 0, 4, &addrLo);

    if (result != 0)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // AtsTargetAddressWidth
    result = GpuEscapeRead(gpuId, atsRequest[2].c_str(), 0, 4, pAddrWidth);

    if (result != 0)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // AtsTargetMask
    result = GpuEscapeRead(gpuId, atsRequest[3].c_str(), 0, 4, pMask);

    if (result != 0)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // AtsTargetMaskWidth
    result = GpuEscapeRead(gpuId, atsRequest[4].c_str(), 0, 4, pMaskWidth);

    if (result != 0)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // AtsTargetGranularity
    result = GpuEscapeRead(gpuId, atsRequest[5].c_str(), 0, 4, pGranularity);

    if (result != 0)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // AtsGuestAddressHi (lwrrently optional)
    result = GpuEscapeRead(gpuId, atsRequest[6].c_str(), 0, 4, &guestAddrHi);

    if (result != 0)
      guestAddrHi = 0;

    // AtsGuestAddressLo (lwrrently optional)
    result = GpuEscapeRead(gpuId, atsRequest[7].c_str(), 0, 4, &guestAddrLo);

    if (result != 0)
      guestAddrLo = 0;

    addr = (static_cast<UINT64>(addrHi) << 32) | addrLo;
    *pAddr = static_cast<UINT32>(addr >> *pGranularity);

    addr = (static_cast<UINT64>(guestAddrHi) << 32) | guestAddrLo;
    *pAddrGuest = static_cast<UINT32>(addr >> *pGranularity);

    return OK;
}

//------------------------------------------------------------------------------
// Colwert physical address to virtual address.
//
void * Platform::PhysicalToVirtual(PHYSADDR PhysicalAddress, Tee::Priority pri)
{
    if (s_pChip)
    {
        void* result = SimPageTable::PhysicalToVirtual(PhysicalAddress, IsTegra() || IsSelfHosted());
        return result;
    }
    else
    {
        return Xp::PhysicalToVirtual(PhysicalAddress, pri);
    }
}

//------------------------------------------------------------------------------
// Colwert virtual address to physical address.
//

// Locally-callable version that avoids the assertion
static RC Platform::VirtualToPhysicalInternal
(
    PHYSADDR *pPhysAddr,
    const volatile void * VirtualAddress
)
{
    const PageTableEntry *Pte = SimPageTable::GetVirtualAddrPte(VirtualAddress);
    UINT32 PteOffset = SimPageTable::GetVirtualAddrPteOffset(VirtualAddress);
    if (Pte)
    {
        // Construct address
        *pPhysAddr = Pte->PhysAddr | PteOffset;
        return OK;
    }
    return RC::SOFTWARE_ERROR;
}

PHYSADDR Platform::VirtualToPhysical(volatile void * VirtualAddress, Tee::Priority pri)
{
    if (!s_pChip)
    {
        return Xp::VirtualToPhysical(VirtualAddress, pri);
    }

    PHYSADDR PhysAddr;

    if (OK != VirtualToPhysicalInternal(&PhysAddr, VirtualAddress))
    {
        Printf(pri, "VirtualToPhysical on invalid page (address = %p)\n",
            VirtualAddress);
        MASSERT(0);
    }

    return PhysAddr;
}

UINT32 Platform::VirtualToPhysical32(volatile void * VirtualAddress)
{
    // This is a shortlwt used because a lot of hardware only accepts 32 bits.
    // Assert to try to catch bugs caused by using this function where it's unsafe.
    PHYSADDR PhysicalAddress = VirtualToPhysical(VirtualAddress);
    MASSERT(PhysicalAddress <= 0xFFFFFFFF);
    return (UINT32)PhysicalAddress;
}

RC Platform::CallACPIGeneric(UINT32 GpuInst,
                             UINT32 Function,
                             void *Param1,
                             void *Param2,
                             void *Param3)
{
    return Xp::CallACPIGeneric(GpuInst, Function, Param1, Param2, Param3);
}

RC Platform::LWIFMethod
(
    UINT32 Function,
    UINT32 SubFunction,
    void *InParams,
    UINT16 InParamSize,
    UINT32 *OutStatus,
    void *OutData,
    UINT16 *OutDataSize
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Ilwalidate the CPU's cache without flushing it.  (Very dangerous!)
//
RC Platform::IlwalidateCpuCache()
{
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        return Cpu::IlwalidateCache();
    }
    else
    {
        return OK;
    }
}

//------------------------------------------------------------------------------
// Flush the CPU's cache.
//
RC Platform::FlushCpuCache()
{
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        return Cpu::FlushCache();
    }
    else
    {
        return OK;
    }
}

//------------------------------------------------------------------------------
// Flush the CPU's cache from 'StartAddress' to 'EndAddress'.
//
RC Platform::FlushCpuCacheRange
(
   void * StartAddress /* = 0 */,
   void * EndAddress   /* = 0xFFFFFFFF */,
   UINT32 Flags
)
{
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        return Cpu::FlushCpuCacheRange(StartAddress, EndAddress, Flags);
    }
    else
    {
        return OK;
    }
}

//------------------------------------------------------------------------------
// Flush the CPU's write combine buffer.
//
RC Platform::FlushCpuWriteCombineBuffer()
{
    RC rc;
    Platform::Irql oldIrq = GetLwrrentIrql();

    if (s_SimulationMode != Platform::Hardware || s_HybridEmulation) // RTL, FModel or AModel
    {
        oldIrq = RaiseIrql(Platform::ElevatedIrql);
    }

    if ((s_SimulationMode == Hardware) && !s_RemoteHardware && !s_HybridEmulation)
    {
        rc = Cpu::FlushWriteCombineBuffer();
    }
    else
    {
        // Is there anything in the simulated WC buffer?
        if (s_WcSize)
        {
            // Flush it out
            rc = PhysWr(s_WcBaseAddr, s_WcBuffer, s_WcSize);
            if( OK == rc)
            {
                s_WcSize = 0;
            }
        }
    }

    if (s_SimulationMode != Platform::Hardware || s_HybridEmulation)
    {
        LowerIrql(oldIrq);
    }

    return rc;
}

RC Platform::SetupDmaBase
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
    MASSERT(pDmaBaseAddr);

    if (!s_pPpc)
    {
        Xp::PpcTceBypassMode xpBypassMode;
        switch (bypassMode)
        {
            case PPC_TCE_BYPASS_DEFAULT:
                xpBypassMode = Xp::PPC_TCE_BYPASS_DEFAULT;
                break;
            case PPC_TCE_BYPASS_FORCE_ON:
                xpBypassMode = Xp::PPC_TCE_BYPASS_FORCE_ON;
                break;
            case PPC_TCE_BYPASS_FORCE_OFF:
                xpBypassMode = Xp::PPC_TCE_BYPASS_FORCE_OFF;
                break;
            default:
                Printf(Tee::PriHigh, "Error: Unknown bypass mode %d\n",
                       bypassMode);
                return RC::SOFTWARE_ERROR;
        }
        return Xp::SetupDmaBase(domain, bus, dev, func, xpBypassMode,
                                devDmaMask, pDmaBaseAddr);
    }

    TCE_BYPASS_MODE simBypassMode;
    switch (bypassMode)
    {
        case PPC_TCE_BYPASS_DEFAULT:
            simBypassMode = TCE_BYPASS_DEFAULT;
            break;
        case PPC_TCE_BYPASS_FORCE_ON:
            simBypassMode = TCE_BYPASS_ON;
            break;
        case PPC_TCE_BYPASS_FORCE_OFF:
            simBypassMode = TCE_BYPASS_OFF;
            break;
        default:
            Printf(Tee::PriHigh, "Error: Unknown bypass mode %d\n",
                   bypassMode);
            return RC::SOFTWARE_ERROR;
    }

    LwU064 baseAddr;
    const LwPciDev pciDevice = {
        static_cast<UINT16>(domain),
        static_cast<UINT16>(bus),
        static_cast<UINT16>(dev),
        static_cast<UINT16>(func)
    };
    const LwErr ret = s_pPpc->SetupDmaBase(pciDevice, simBypassMode, devDmaMask, &baseAddr);
    if (ret != LW_PASS)
    {
        Printf(Tee::PriHigh,
               "Error: Unable to setup DMA base address on %x:%x:%x.%x\n",
               domain, bus, dev, func);
        return RC::SOFTWARE_ERROR;
    }
    *pDmaBaseAddr = static_cast<PHYSADDR>(baseAddr);
    return OK;
}

//-----------------------------------------------------------------------------
RC Platform::SetLwLinkSysmemTrained(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, bool trained)
{
    if (!s_pChip)
    {
        return Xp::SetLwLinkSysmemTrained(domain, bus, device, func, trained);
    }
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Platform::PciGetBarInfo
(
    UINT32    domainNumber,
    UINT32    busNumber,
    UINT32    deviceNumber,
    UINT32    functionNumber,
    UINT32    barIndex,
    PHYSADDR* pBaseAddress,
    UINT64*   pBarSize
)
{
    if (IsVirtFunMode())
    {
        VmiopElwManager *pMgr = VmiopElwManager::Instance();
        const UINT32 gfid = pMgr->GetGfidAttachedToProcess();
        VmiopElw* pVmiopElw = pMgr->GetVmiopElw(gfid);
        MASSERT(pVmiopElw != nullptr);
        return pVmiopElw->SendReceivePcieBarInfo(barIndex,
                                                 pBaseAddress, pBarSize);
    }

    if (!s_pPciDev)
    {
        return Xp::PciGetBarInfo(domainNumber, busNumber,
                                 deviceNumber, functionNumber,
                                 barIndex, pBaseAddress, pBarSize);
    }

    if (domainNumber > 0xF)
    {
        Printf(Tee::PriHigh, "ERROR: Domain = 0x%x. Mods does not support"
                             " PCI domain > 0xF at this time.\n", domainNumber);
        return RC::SOFTWARE_ERROR;
    }
    const LwPciDev pciDevice = {
        static_cast<UINT16>(domainNumber),
        static_cast<UINT16>(busNumber),
        static_cast<UINT16>(deviceNumber),
        static_cast<UINT16>(functionNumber)
    };
    const LwErr ret = s_pPciDev->GetPciBarInfo(pciDevice, barIndex, pBaseAddress, pBarSize);

    if (ret == LW_UNSUPPORTED)
        return Pci::GetBarInfo(domainNumber, busNumber,
                               deviceNumber, functionNumber,
                               barIndex, pBaseAddress, pBarSize);
    else if (ret != LW_PASS)
        return RC::PCI_DEVICE_NOT_FOUND;

    return OK;
}

//------------------------------------------------------------------------------
RC Platform::PciEnableSriov
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT32 vfCount
)
{
    MASSERT(Platform::IsPhysFunMode());
    RC rc;

    // Enable SRIOV on hwchip2.  Do nothing in simulation, since SRIOV
    // was enabled by passing "-sriov_vf_enable -sriov_vf_count=n" to
    // the chiplib library's Startup() function.
    if (GetSimulationMode() == Hardware && !s_HybridEmulation)
    {
        CHECK_RC(Xp::PciEnableSriov(domain, bus, device, function, vfCount));
    }
    return rc;
}

//------------------------------------------------------------------------------
RC Platform::PciGetIRQ
(
    UINT32  domainNumber,
    UINT32  busNumber,
    UINT32  deviceNumber,
    UINT32  functionNumber,
    UINT32* pIrq
)
{
    if (!s_pPciDev)
    {
        return Xp::PciGetIRQ(domainNumber, busNumber,
                             deviceNumber, functionNumber, pIrq);
    }

    if (domainNumber > 0xF)
    {
        Printf(Tee::PriHigh, "ERROR: Domain = 0x%x. Mods does not support"
                             " PCI domain > 0xF at this time.\n", domainNumber);
        return RC::SOFTWARE_ERROR;
    }
    const LwPciDev pciDevice = {
        static_cast<UINT16>(domainNumber),
        static_cast<UINT16>(busNumber),
        static_cast<UINT16>(deviceNumber),
        static_cast<UINT16>(functionNumber)
    };
    const LwErr ret = s_pPciDev->GetPciIrq(pciDevice, pIrq);

    if (ret == LW_UNSUPPORTED)
        return Pci::GetIRQ(domainNumber, busNumber,
                           deviceNumber, functionNumber, pIrq);
    else if (ret != LW_PASS)
        return RC::PCI_DEVICE_NOT_FOUND;

    return OK;
}

//------------------------------------------------------------------------------
// Read byte, word, or dword at 'Address' from a PCI device with the given
// 'BusNumber', 'DeviceNumber', and 'FunctionNumber'.
// Return read data in '*pData'.
//
RC Platform::PciRead08
(
    INT32    DomainNumber,
    INT32    BusNumber,
    INT32    DeviceNumber,
    INT32    FunctionNumber,
    INT32    Address,
    UINT08 * pData
)
{
    RC rc;
    if (!IsPcie())
    {
        Printf(Tee::PriNormal, "Warning: PciRead used on a platform which does not have PCI bus!\n");
        if (s_StrictPciCfgSpace)
        {
            MASSERT(!"PciRead not supported on SOC!");
            *pData = 0xFF;
            return OK;
        }
    }

    if (!s_pChip)
    {
        rc = Xp::PciRead08(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, pData);
    }
    else
    {
        if (s_pPciDev)
        {
            LwPciDev pciDev = {
                static_cast<UINT16>(DomainNumber),
                static_cast<UINT16>(BusNumber),
                static_cast<UINT16>(DeviceNumber),
                static_cast<UINT16>(FunctionNumber)
            };
            LwErr ret = s_pPciDev->PciCfgRd08(pciDev, Address, pData);
            if (ret != LW_PASS)
                return RC::SOFTWARE_ERROR;
        }
        else
        {
            MASSERT(s_pIo);

            if (DomainNumber > 0xF)
            {
                Printf(Tee::PriHigh, "ERROR: Domain = 0x%x. Use IPciDev::PciCfgRd08 "
                                     "for PCI domain > 0xF.\n", DomainNumber);
                return RC::SOFTWARE_ERROR;
            }
            UINT32 CompAddr = Pci::GetConfigAddress(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address);

            *pData = s_pIo->CfgRd08(CompAddr);
        }
    }

    if (rc == OK)
    {
        DumpCfgAccessData(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address,
                          pData, sizeof(*pData), "cfgrd");
    }

    return rc;
}

RC Platform::PciRead16
(
    INT32    DomainNumber,
    INT32    BusNumber,
    INT32    DeviceNumber,
    INT32    FunctionNumber,
    INT32    Address,
    UINT16 * pData
)
{
    RC rc;

    if (!IsPcie())
    {
        Printf(Tee::PriNormal, "Warning: PciRead used on a platform which does not have PCI bus!\n");
        if (s_StrictPciCfgSpace)
        {
            MASSERT(!"PciRead not supported on SOC!");
            *pData = 0xFFFF;
            return OK;
        }
    }

    if (!s_pChip)
    {
        rc = Xp::PciRead16(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, pData);
    }
    else
    {
        if (s_pPciDev)
        {
            LwPciDev pciDev = {
                static_cast<UINT16>(DomainNumber),
                static_cast<UINT16>(BusNumber),
                static_cast<UINT16>(DeviceNumber),
                static_cast<UINT16>(FunctionNumber)
            };
            LwErr ret = s_pPciDev->PciCfgRd16(pciDev, Address, pData);
            if (ret != LW_PASS)
                return RC::SOFTWARE_ERROR;
        }
        else
        {
            MASSERT(s_pIo);

            if (DomainNumber > 0xF)
            {
                Printf(Tee::PriHigh, "ERROR: Domain = 0x%x. Use IPciDev::PciCfgRd16 "
                                     "for PCI domain > 0xF.\n", DomainNumber);
                return RC::SOFTWARE_ERROR;
            }
            UINT32 CompAddr = Pci::GetConfigAddress(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address);

            *pData = s_pIo->CfgRd16(CompAddr);
        }
    }

    if (rc == OK)
    {
        DumpCfgAccessData(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address,
                          pData, sizeof(*pData), "cfgrd");
    }

    return rc;
}

RC Platform::PciRead32
(
    INT32    DomainNumber,
    INT32    BusNumber,
    INT32    DeviceNumber,
    INT32    FunctionNumber,
    INT32    Address,
    UINT32 * pData
)
{
    RC rc;

    if (!IsPcie())
    {
        Printf(Tee::PriNormal, "Warning: PciRead used on a platform which does not have PCI bus!\n");
        if (s_StrictPciCfgSpace)
        {
            MASSERT(!"PciRead not supported on SOC!");
            *pData = ~0U;
            return OK;
        }
    }

    if (!s_pChip)
    {
        rc = Xp::PciRead32(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, pData);
    }
    else
    {
        if (s_pPciDev)
        {
            LwPciDev pciDev = {
                static_cast<UINT16>(DomainNumber),
                static_cast<UINT16>(BusNumber),
                static_cast<UINT16>(DeviceNumber),
                static_cast<UINT16>(FunctionNumber)
            };
            LwErr ret = s_pPciDev->PciCfgRd32(pciDev, Address, pData);
            if (ret != LW_PASS)
                return RC::SOFTWARE_ERROR;
        }
        else
        {
            MASSERT(s_pIo);

            if (DomainNumber > 0xF)
            {
                Printf(Tee::PriHigh, "ERROR: Domain = 0x%x. Use IPciDev::PciCfgRd32 "
                                     "for PCI domain > 0xF.\n", DomainNumber);
                return RC::SOFTWARE_ERROR;
            }
            UINT32 CompAddr = Pci::GetConfigAddress(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address);

            *pData = s_pIo->CfgRd32(CompAddr);
        }
    }

    if (rc == OK)
    {
        DumpCfgAccessData(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address,
                          pData, sizeof(*pData), "cfgrd");
    }

    return rc;
}

//------------------------------------------------------------------------------
// Write 'Data' at 'Address' to a PCI device with the given
// 'BusNumber', 'DeviceNumber', and 'FunctionNumber'.
//
RC Platform::PciWrite08
(
    INT32  DomainNumber,
    INT32  BusNumber,
    INT32  DeviceNumber,
    INT32  FunctionNumber,
    INT32  Address,
    UINT08 Data
)
{
    RC rc;

    if (!IsPcie())
    {
        Printf(Tee::PriNormal, "Warning: PciWrite used on a platform which does not have PCI bus!\n");
        if (s_StrictPciCfgSpace)
        {
            MASSERT(!"PciWrite not supported on SOC!");
            return OK;
        }
    }

    if (!s_pChip)
    {
        rc = Xp::PciWrite08(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, Data);
    }
    else
    {
        if (s_pPciDev)
        {
            LwPciDev pciDev = {
                static_cast<UINT16>(DomainNumber),
                static_cast<UINT16>(BusNumber),
                static_cast<UINT16>(DeviceNumber),
                static_cast<UINT16>(FunctionNumber)
            };
            LwErr ret = s_pPciDev->PciCfgWr08(pciDev, Address, Data);
            if (ret != LW_PASS)
                return RC::SOFTWARE_ERROR;
        }
        else
        {
            MASSERT(s_pIo);

            if (DomainNumber > 0xF)
            {
                Printf(Tee::PriHigh, "ERROR: Domain = 0x%x. Use IPciDev::PciCfgWr08 "
                                         "for PCI domain > 0xF.\n", DomainNumber);
                return RC::SOFTWARE_ERROR;
            }
            UINT32 CompAddr = Pci::GetConfigAddress(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address);

            s_pIo->CfgWr08(CompAddr, Data);
        }
    }

    if (rc == OK)
    {
        DumpCfgAccessData(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address,
                          &Data, sizeof(Data), "cfgwr");
    }

    return rc;
}

RC Platform::PciWrite16
(
    INT32  DomainNumber,
    INT32  BusNumber,
    INT32  DeviceNumber,
    INT32  FunctionNumber,
    INT32  Address,
    UINT16 Data
)
{
    RC rc;

    if (!IsPcie())
    {
        Printf(Tee::PriNormal, "Warning: PciWrite used on a platform which does not have PCI bus!\n");
        if (s_StrictPciCfgSpace)
        {
            MASSERT(!"PciWrite not supported on SOC!");
            return OK;
        }
    }

    if (!s_pChip)
    {
        rc = Xp::PciWrite16(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, Data);
    }
    else
    {
        if (s_pPciDev)
        {
            LwPciDev pciDev = {
                static_cast<UINT16>(DomainNumber),
                static_cast<UINT16>(BusNumber),
                static_cast<UINT16>(DeviceNumber),
                static_cast<UINT16>(FunctionNumber)
            };
            LwErr ret = s_pPciDev->PciCfgWr16(pciDev, Address, Data);
            if (ret != LW_PASS)
                return RC::SOFTWARE_ERROR;
        }
        else
        {
            MASSERT(s_pIo);

            if (DomainNumber > 0xF)
            {
                Printf(Tee::PriHigh, "ERROR: Domain = 0x%x. Use IPciDev::PciCfgWr16 "
                                         "for PCI domain > 0xF.\n", DomainNumber);
                return RC::SOFTWARE_ERROR;
            }
            UINT32 CompAddr = Pci::GetConfigAddress(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address);

            s_pIo->CfgWr16(CompAddr, Data);
        }
    }

    if (rc == OK)
    {
        DumpCfgAccessData(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address,
                          &Data, sizeof(Data), "cfgwr");
    }

    return rc;
}

RC Platform::PciWrite32
(
    INT32  DomainNumber,
    INT32  BusNumber,
    INT32  DeviceNumber,
    INT32  FunctionNumber,
    INT32  Address,
    UINT32 Data
)
{
    RC rc;

    if (!IsPcie())
    {
        Printf(Tee::PriNormal, "Warning: PciWrite used on a platform which does not have PCI bus!\n");
        if (s_StrictPciCfgSpace)
        {
            MASSERT(!"PciWrite not supported on SOC!");
            return OK;
        }
    }

    if (!s_pChip)
    {
        rc = Xp::PciWrite32(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, Data);
    }
    else
    {
        if (s_pPciDev)
        {
            LwPciDev pciDev = {
                static_cast<UINT16>(DomainNumber),
                static_cast<UINT16>(BusNumber),
                static_cast<UINT16>(DeviceNumber),
                static_cast<UINT16>(FunctionNumber)
            };
            LwErr ret = s_pPciDev->PciCfgWr32(pciDev, Address, Data);
            if (ret != LW_PASS)
                return RC::SOFTWARE_ERROR;
        }
        else
        {
            MASSERT(s_pIo);

            if (DomainNumber > 0xF)
            {
                Printf(Tee::PriHigh, "ERROR: Domain = 0x%x. Use IPciDev::PciCfgWr32 "
                                         "for PCI domain > 0xF.\n", DomainNumber);
                return RC::SOFTWARE_ERROR;
            }
            UINT32 CompAddr = Pci::GetConfigAddress(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address);

            s_pIo->CfgWr32(CompAddr, Data);
        }
    }

    if (rc == OK)
    {
        DumpCfgAccessData(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address,
                          &Data, sizeof(Data), "cfgwr");
    }

    return rc;
}

RC Platform::PciRemove
(
   INT32    domainNumber,
   INT32    busNumber,
   INT32    deviceNumber,
   INT32    functionNumber
)
{
    return Xp::PciRemove(domainNumber, busNumber, deviceNumber, functionNumber);
}

//------------------------------------------------------------------------------
// Port read and write functions.
//
RC Platform::PioRead08(UINT16 Port, UINT08 * pData)
{
    if (IsTegra())
    {
        Printf(Tee::PriNormal, "Warning: PioRead used on a platform which does not have PCI bus!\n");
        if (s_StrictPciCfgSpace)
        {
            MASSERT(!"PioRead not supported on SOC!");
            *pData = 0xFF;
            return OK;
        }
    }

    if (!s_pChip)
    {
        return Xp::PioRead08(Port, pData);
    }

    MASSERT(s_pIo);

    *pData = s_pIo->IoRd08(Port);

    return OK;
}

RC Platform::PioRead16(UINT16 Port, UINT16 * pData)
{
    if (IsTegra())
    {
        Printf(Tee::PriNormal, "Warning: PioRead used on a platform which does not have PCI bus!\n");
        if (s_StrictPciCfgSpace)
        {
            MASSERT(!"PioRead not supported on SOC!");
            *pData = 0xFFFF;
            return OK;
        }
    }

    if (!s_pChip)
    {
        return Xp::PioRead16(Port, pData);
    }

    MASSERT(s_pIo);

    *pData = s_pIo->IoRd16(Port);

    return OK;
}

RC Platform::PioRead32(UINT16 Port, UINT32 * pData)
{
    if (IsTegra())
    {
        Printf(Tee::PriNormal, "Warning: PioRead used on a platform which does not have PCI bus!\n");
        if (s_StrictPciCfgSpace)
        {
            MASSERT(!"PioRead not supported on SOC!");
            *pData = ~0U;
            return OK;
        }
    }

    if (!s_pChip)
    {
        return Xp::PioRead32(Port, pData);
    }

    MASSERT(s_pIo);

    *pData = s_pIo->IoRd32(Port);

    return OK;
}

RC Platform::PioWrite08(UINT16 Port, UINT08 Data)
{
    if (IsTegra())
    {
        Printf(Tee::PriNormal, "Warning: PioWrite used on a platform which does not have PCI bus!\n");
        if (s_StrictPciCfgSpace)
        {
            MASSERT(!"PioWrite not supported on SOC!");
            return OK;
        }
    }

    if (!s_pChip)
    {
        return Xp::PioWrite08(Port, Data);
    }

    MASSERT(s_pIo);

    s_pIo->IoWr08(Port, Data);

    return OK;
}

RC Platform::PioWrite16(UINT16 Port, UINT16 Data)
{
    if (IsTegra())
    {
        Printf(Tee::PriNormal, "Warning: PioWrite used on a platform which does not have PCI bus!\n");
        if (s_StrictPciCfgSpace)
        {
            MASSERT(!"PioWrite not supported on SOC!");
            return OK;
        }
    }

    if (!s_pChip)
    {
        return Xp::PioWrite16(Port, Data);
    }

    MASSERT(s_pIo);

    s_pIo->IoWr16(Port, Data);

    return OK;
}

RC Platform::PioWrite32(UINT16 Port, UINT32 Data)
{
    if (IsTegra())
    {
        Printf(Tee::PriNormal, "Warning: PioWrite used on a platform which does not have PCI bus!\n");
        if (s_StrictPciCfgSpace)
        {
            MASSERT(!"PioWrite not supported on SOC!");
            return OK;
        }
    }

    if (!s_pChip)
    {
        return Xp::PioWrite32(Port, Data);
    }

    MASSERT(s_pIo);

    s_pIo->IoWr32(Port, Data);

    return OK;
}

//------------------------------------------------------------------------------
// Interrupt functions.
//

bool Platform::IRQTypeSupported(UINT32 irqType)
{
    if (!s_pChip)
    {
        return Xp::IRQTypeSupported(irqType);
    }

    switch (irqType)
    {
        case MODS_IRQ_TYPE_INT:
            return true;
        case MODS_IRQ_TYPE_MSI:
            // MSI is not supported on RTL
            return NULL != s_pInterruptMgr3 && s_SimulationMode != RTL;
        case MODS_IRQ_TYPE_CPU:
            return false;
        case MODS_IRQ_TYPE_MSIX:
            return NULL != s_pInterruptMgr3;
        default:
            Printf(Tee::PriWarn, "Unknown interrupt type %d\n", irqType);
    }

    return false;
}

void Platform::HandleInterrupt(UINT32 Irq)
{
    ChiplibOpScope newScope("Handling interrupt", Irq,
                             ChiplibOpScope::SCOPE_IRQ, NULL);

    Printf(Tee::PriLow, "Platform::HandleInterrupt Irq=0x%x\n", Irq);

    MASSERT(s_InterruptsEnabled);

    // replaying chiplib trace
    if (Replayer::IsReplayingTrace())
    {
         return;
    }

    // When an interrupt comes in, automatically disable interrupts, just like
    // a real CPU would have done automatically.
    s_InterruptsEnabled = false;
    Tasker::SetCanThreadYield(false);

    DEFER
    {
        // Automatically reenable interrupts, just like a real CPU would have done
        // automatically on IRET.
        s_InterruptsEnabled = true;
        Tasker::SetCanThreadYield(true);
    };

    UINT32 NumHooksCalled = 0;
    IrqMap::iterator irqDesc = s_IrqMap.find(Irq);
    if (irqDesc != s_IrqMap.end())
    {
        // If the Irq is not enabled
        if (irqDesc->second.disabled)
        {
            Printf(Tee::PriLow, "Platform::HandleInterrupt Irq=0x%x disabled\n", Irq);
            return;
        }

        // Hack: call ISRs in reverse order of registration, so plugin ISRs get a chance.
        IrqHooks& hooks = irqDesc->second.hooks;
        for (auto rit = hooks.rbegin() ; rit != hooks.rend(); ++rit)
        {
            NumHooksCalled++;
            IsrData hook = *rit;
            if (hook.GetIsr())
            {
                if (hook() == Xp::ISR_RESULT_SERVICED)
                {
                    // Move the processed hook before the first ISR hook so it will be deprioritized.
                    // In this way, other hooks have a chance to run.

                    hooks.erase(--rit.base());
                    auto firstIsrHook = find_if(hooks.begin(), hooks.end(),
                                        [&](const IsrData& hook) { return hook.GetIsr() != nullptr; });
                    hooks.insert(firstIsrHook, hook);

                    // For each registered hook (e.g. a t3d plugin), if its ISR returns
                    // ISR_RESULT_SERVICED, it'll not need further handling, so here MODS
                    // stops the calling of other hooks including RM ISR.
                    break;
                }
            }
            else
            {
                GpuDevMgr* const pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
                MASSERT(pGpuDevMgr);
                // Temporary: the old behavior is that irq number is 0, we don't know
                // where it originates from, so we try to service interrupts for all
                // GPU subdevices.
                const UINT32 irqNum = Irq == 0 ? ~0U : Irq;
                pGpuDevMgr->ServiceInterrupt(irqNum);
            }
        }
    }
    if (!NumHooksCalled)
    {
        // No one serviced the interrupt.  This will almost certainly result in
        // an endless interrupt storm, so print an error and assert.  An example
        // of why this might happen is that a device generated an interrupt
        // before the ISR was registered, or after the ISR was unregistered.
        // Either way, we have a problem.
        MASSERT(!"Interrupt received but no ISR registered");
    }

    Printf(Tee::PriLow, "Platform::HandleInterrupt Irq=0x%x triggers %u hooks\n",
           Irq, NumHooksCalled);

    Platform::InterruptProcessed();

}

RC Platform::HookIRQ(UINT32 Irq, ISR func, void* cookie)
{
    if (!s_pChip)
    {
        return Xp::HookIRQ(UNSIGNED_CAST(UINT08, Irq), func, cookie);
    }
    if (!s_ChiplibSupportsInterrupts)
    {
        Printf(Tee::PriHigh,
               "Warning: This chiplib does not support interrupts."
               "  Please try -poll_interrupts.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    if (s_pInterruptMgr)
    {
        if (s_pInterruptMgr->HookInterrupt(Irq))
        {
            return RC::CANNOT_HOOK_INTERRUPT;
        }
    }
    if (s_InterruptThreadID == Tasker::NULL_THREAD)
    {
        Printf(Tee::PriWarn, "Starting InterruptThread\n");
        s_InterruptThreadRunning = true;
        s_InterruptThreadID =
            Tasker::CreateThread(InterruptThread, 0, 0, "Interrupt");
    }

    s_IrqMap[Irq].hooks.push_back(IsrData(func, cookie));

    return OK;
}

namespace
{
    // Colwert between hw struct IrqParams struct and IrqInfo2 struct
    // for sim platform
    void ColwertIrqStruct(const IrqParams& pModsIrqParams,
                          struct IrqInfo2& pSimModsIrqParams)
    {
        pSimModsIrqParams.irqNumber       = pModsIrqParams.irqNumber;
        pSimModsIrqParams.barAddr         = pModsIrqParams.barAddr;
        pSimModsIrqParams.barSize         = pModsIrqParams.barSize;
        pSimModsIrqParams.pciDev.domain   = pModsIrqParams.pciDev.domain;
        pSimModsIrqParams.pciDev.bus      = pModsIrqParams.pciDev.bus;
        pSimModsIrqParams.pciDev.device   = pModsIrqParams.pciDev.device;
        pSimModsIrqParams.pciDev.function = pModsIrqParams.pciDev.function;
        pSimModsIrqParams.irqType         = pModsIrqParams.irqType;
        pSimModsIrqParams.maskInfoCount   = pModsIrqParams.maskInfoCount;
        for (UINT32 index = 0; index < MASK_PARAMS_LIST_SIZE; index++)
        {
            pSimModsIrqParams.maskInfoList[index].maskType =
                                   pModsIrqParams.maskInfoList[index].maskType;
            pSimModsIrqParams.maskInfoList[index].irqPendingOffset =
                                   pModsIrqParams.maskInfoList[index].irqPendingOffset;
            pSimModsIrqParams.maskInfoList[index].irqEnabledOffset =
                                   pModsIrqParams.maskInfoList[index].irqEnabledOffset;
            pSimModsIrqParams.maskInfoList[index].irqEnableOffset =
                                   pModsIrqParams.maskInfoList[index].irqEnableOffset;
            pSimModsIrqParams.maskInfoList[index].irqDisableOffset =
                                   pModsIrqParams.maskInfoList[index].irqDisableOffset;
            pSimModsIrqParams.maskInfoList[index].andMask =
                                   pModsIrqParams.maskInfoList[index].andMask;
            pSimModsIrqParams.maskInfoList[index].orMask =
                                   pModsIrqParams.maskInfoList[index].orMask;
        }
    }
}

RC Platform::HookInt(const IrqParams &irqParams, ISR func, void* cookie)
{
    IrqInfo2 irqInfo;
    ColwertIrqStruct(irqParams, irqInfo);

    if (!s_pChip)
    {
        return (Xp::HookInt(irqParams, func, cookie));
    }
    if (!s_ChiplibSupportsInterrupts)
    {
        Printf(Tee::PriHigh,
               "Warning: This chiplib does not support interrupts."
               "  Please try -poll_interrupts.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (!Platform::IRQTypeSupported(irqInfo.irqType))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (s_pInterruptMgr3)
    {
        if (s_pInterruptMgr3->HookInterrupt3(irqInfo))
        {
            return RC::CANNOT_HOOK_INTERRUPT;
        }
    }
    else if (s_pInterruptMgr2)
    {
        if (s_pInterruptMgr2->HookInterrupt(irqInfo))
        {
            return RC::CANNOT_HOOK_INTERRUPT;
        }
    }

    if (s_InterruptThreadID == Tasker::NULL_THREAD)
    {
        Printf(Tee::PriWarn, "Starting InterruptThread\n");
        s_InterruptThreadRunning = true;
        s_InterruptThreadID =
            Tasker::CreateThread(InterruptThread, 0, 0, "Interrupt");
    }

    if (irqInfo.irqType == MODS_IRQ_TYPE_INT)
    {
        // For INTA interrupts, we must set s_DevMap in order to
        // unhook the int later.  For all other interrupts, s_DevMap
        // is set by Platform::AllocateIRQs().
        //
        const PciInfo& pciDev = irqInfo.pciDev;
        const UINT32 idx = GetDeviceIrqId(pciDev.domain, pciDev.bus,
                                          pciDev.device, pciDev.function);
        if (s_DevMap.count(idx))
        {
            Printf(Tee::PriError,
                   "Error: INTA interrupts already allocated "
                   "(domain=%u, bus=%u, device=%u, function=%u)!",
                   pciDev.domain, pciDev.bus, pciDev.device, pciDev.function);
            return RC::SOFTWARE_ERROR;
        }
        DevDesc& devDesc = s_DevMap[idx];
        devDesc.irqs.push_back(irqInfo.irqNumber);
        devDesc.flags = 0;
    }

    // Interrupts for FModel & RTL are handled completely differently. So add the
    // callback function in case we are using those models.
    s_IrqMap[irqInfo.irqNumber].hooks.push_back(IsrData(func, cookie));

    return OK;
}

RC Platform::UnhookInt(const IrqParams& irqParams, ISR func, void* cookie)
{
    IrqInfo2 irqInfo;
    ColwertIrqStruct(irqParams, irqInfo);

    if (!s_pChip)
    {
        return Xp::UnhookInt(irqParams, func, cookie);
    }

    const auto& pciDev = irqInfo.pciDev;
    const UINT32 idx = GetDeviceIrqId(pciDev.domain, pciDev.bus,
                                      pciDev.device, pciDev.function);
    DevMap::iterator devIt = s_DevMap.find(idx);

    if (devIt == s_DevMap.end())
    {
        Printf(Tee::PriError,
               "Error: device not hooked (domain=%u, bus=%u, device=%u, function:=%u)!\n",
               pciDev.domain, pciDev.bus, pciDev.device, pciDev.function);
        return RC::SOFTWARE_ERROR;
    }

    Irqs& irqs(devIt->second.irqs);

    for (Irqs::iterator irqIt = irqs.begin(); irqIt != irqs.end(); irqIt++)
    {
        Platform::UnhookIRQ(*irqIt, func, cookie);
    }

    return OK;
}

RC Platform::UnhookIRQ(UINT32 Irq, ISR func, void* cookie)
{
    // ensure any pending interrupts have been serviced
    Tasker::Yield();

    if (!s_pChip)
    {
        return Xp::UnhookIRQ(UNSIGNED_CAST(UINT08, Irq), func, cookie);
    }
    IrqMap::iterator irqDesc = s_IrqMap.find(Irq);
    if (irqDesc != s_IrqMap.end())
    {
        vector<IsrData>& isrs = irqDesc->second.hooks;
        vector<IsrData>::iterator isrIt = isrs.begin();
        for ( ; isrIt != isrs.end(); ++isrIt)
        {
            if ((*isrIt) == IsrData(func, cookie))
            {
                isrs.erase(isrIt);
                if (isrs.empty())
                {
                    s_IrqMap.erase(irqDesc);
                    if (s_pInterruptMgr)
                    {
                        if (s_pInterruptMgr->UnhookInterrupt(Irq))
                        {
                            return RC::CANNOT_UNHOOK_ISR;
                        }
                    }
                }
                return OK;
            }
        }
    }
    return RC::CANNOT_UNHOOK_ISR;
}

RC Platform::DisableIRQ(UINT32 Irq)
{
    s_IrqMap[Irq].disabled = true;
    return OK;
}

RC Platform::EnableIRQ(UINT32 Irq)
{
    s_IrqMap[Irq].disabled = false;
    return OK;
}

bool Platform::IsIRQHooked(UINT32 irq)
{
    const IrqMap::const_iterator irqDesc = s_IrqMap.find(irq);
    return irqDesc != s_IrqMap.end();
}

RC Platform::AllocateIRQs
(
    UINT32 pciDomain,
    UINT32 pciBus,
    UINT32 pciDevice,
    UINT32 pciFunction,
    UINT32 lwecs,
    UINT32 *irqs,
    UINT32 flags
)
{
    if (0==lwecs)
    {
        return RC::SOFTWARE_ERROR;
    }

    if (!s_pChip)
    {
        for (UINT32 ii = 0; ii < lwecs; ++ii)
        {
            irqs[ii] = ii;
        }
        return OK;
    }

    if (!s_pInterruptMgr3)
        return RC::UNSUPPORTED_FUNCTION;

    LwPciDev pciDev =
        {
            static_cast<UINT16>(pciDomain),
            static_cast<UINT16>(pciBus),
            static_cast<UINT16>(pciDevice),
            static_cast<UINT16>(pciFunction)
        };
    const UINT32 idx = GetDeviceIrqId(pciDomain, pciBus, pciDevice, pciFunction);
    DevMap::iterator devIt = s_DevMap.find(idx);
    RC rc;

    if (devIt != s_DevMap.end())
    {
        Printf(Tee::PriError, "Error: device interrupts already allocated "
                              "(domain=%u, bus=%u, device=%u, function=%u)!\n",
                   pciDev.domain, pciDev.bus, pciDev.device, pciDev.function);
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(s_pInterruptMgr3->AllocateIRQs(pciDev, lwecs, irqs, flags));

    DevDesc& dev(s_DevMap[idx]);

    Printf(Tee::PriLow,
        "Platform::AllocateIRQs(domain=%u, bus=%u, device=%u, function=%u, ",
        pciDev.domain, pciDev.bus, pciDev.device, pciDev.function);

    Printf(Tee::PriLow, "lwecs=%u, flags=0x%x) returns (irqs=[", lwecs, flags);

    for (UINT32 i=0; i < lwecs; i++)
    {
        Printf(Tee::PriLow, "0x%x%s", irqs[i], i<lwecs-1 ? ", ":"");
        dev.irqs.push_back(irqs[i]);
    }

    Printf(Tee::PriLow, "])\n");

    dev.flags = flags;

    return OK;
}

void Platform::FreeIRQs(UINT32 pciDomain, UINT32 pciBus, UINT32 pciDevice, UINT32 pciFunction)
{
    if (!s_pChip)
    {
        return;
    }

    if (!s_pInterruptMgr3)
    {
        return;
    }

    LwPciDev pciDev
        {
            (UINT16)pciDomain, (UINT16)pciBus,
            (UINT16)pciDevice, (UINT16)pciFunction
        };
    const UINT32 idx = GetDeviceIrqId(pciDomain, pciBus, pciDevice, pciFunction);
    DevMap::iterator devIt = s_DevMap.find(idx);

    if (devIt != s_DevMap.end())
    {
        s_DevMap.erase(idx);
    }

    s_pInterruptMgr3->FreeIRQs(pciDev);
}

void Platform::InterruptProcessed()
{
    s_ProcessedInterruptCount++;
}

void Platform::ProcessPendingInterrupts()
{
    MASSERT(s_InterruptsEnabled);
    bool processingInterrupts = false;

    do
    {
        LwU032 prevProcessedInterruptCount = s_ProcessedInterruptCount;

        // Give other threads a chance to run
        // (e.g. InterruptThread, GpuDevMgr::PollingInterruptThread,
        // RM ISR threads)

        Tasker::Yield();

        processingInterrupts = ((prevProcessedInterruptCount < s_ProcessedInterruptCount) ||
                                (s_InterruptThreadRunning && !s_PendingInterruptQueue.empty()));
    }
    while (processingInterrupts);
}

void Platform::EnableInterrupts()
{
    if (!s_pChip)
    {
        Xp::EnableInterrupts();
        return;
    }

    s_InterruptsEnabled = true;
}

void Platform::DisableInterrupts()
{
    if (!s_pChip)
    {
        Xp::DisableInterrupts();
        return;
    }

    s_InterruptsEnabled = false;
}

Platform::Irql Platform::GetLwrrentIrql()
{
    if (!s_pChip)
    {
        return Xp::GetLwrrentIrql();
    }

    if (s_InterruptsEnabled)
        return Platform::NormalIrql;
    else
        return Platform::ElevatedIrql;
}

Platform::Irql Platform::RaiseIrql(Platform::Irql NewIrql)
{
    if (!s_pChip)
    {
        return Xp::RaiseIrql(NewIrql);
    }

    Platform::Irql OldIrql = GetLwrrentIrql();
    MASSERT(NewIrql >= OldIrql);
    if (NewIrql == Platform::ElevatedIrql)
    {
        DisableInterrupts();
    }
    return OldIrql;
}

void Platform::LowerIrql(Platform::Irql NewIrql)
{
    if (!s_pChip)
    {
        Xp::LowerIrql(NewIrql);
        return;
    }

    MASSERT(NewIrql <= GetLwrrentIrql());
    if (NewIrql == Platform::NormalIrql)
    {
        EnableInterrupts();
    }
}

//------------------------------------------------------------------------------
void Platform::Pause()
{
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        Cpu::Pause();
    }
    else
    {
        ClockSimulator(1);
    }
}

//------------------------------------------------------------------------------
void Platform::DelayNS(UINT32 Nanoseconds)
{
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        Utility::DelayNS(Nanoseconds);
    }
    else
    {
        UINT64 timeIsUp = GetSimulatorTime() + (UINT64)(Nanoseconds / GetSimulatorTimeUnitsNS());

        do
        {
            ClockSimulator(1);
        }
        while (GetSimulatorTime() < timeIsUp);
    }
}

//------------------------------------------------------------------------------
void Platform::Delay(UINT32 Microseconds)
{
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        Utility::Delay(Microseconds);
    }
    else
    {
        s_pChip->Delay(Microseconds);
    }
}

//------------------------------------------------------------------------------
void Platform::SleepUS(UINT32 MinMicrosecondsToSleep)
{
    if(s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        Utility::SleepUS(MinMicrosecondsToSleep);
    }
    else
    {
        UINT64 now = GetSimulatorTime();  // number of native ticks
        double nsPerTick = GetSimulatorTimeUnitsNS();
        double usPerTick = nsPerTick / 1000.0;
        double nowInUsec = (double) now * usPerTick;
        double thenInUsec = nowInUsec + (double)MinMicrosecondsToSleep;

        do
        {
            // Guarantee at least one yield like Tasker::Sleep does
            Tasker::Yield();

            now = GetSimulatorTime();
            nowInUsec = (double) now * usPerTick;
        } while (nowInUsec < thenInUsec);
    }
}

// nanoseconds
UINT64 Platform::GetTimeNS()
{
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        return Xp::GetWallTimeNS();
    }
    else
    {
        double ticks = (double)GetSimulatorTime();
        double nsPerTick = GetSimulatorTimeUnitsNS();
        UINT64 ns = (UINT64)(ticks * nsPerTick);
        return ns;
    }
}

// microseconds
UINT64 Platform::GetTimeUS()
{
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        return Xp::GetWallTimeUS();
    }
    else
    {
        return GetTimeNS() / 1000;
    }
}

// milliseconds
UINT64 Platform::GetTimeMS()
{
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        return Xp::GetWallTimeMS();
    }
    else
    {
        return (GetTimeNS() / 1000) / 1000;
    }
}

///------------------------------------------------------------------------------
// Simulator functions
//
bool Platform::IsChiplibLoaded()
{
    return (s_pChip != NULL);
}

void Platform::ClockSimulator(int Cycles)
{
    MASSERT(s_SimulationMode != Hardware || s_HybridEmulation);
    MASSERT(Cycles > 0);

    // Note that on the LW4x fmodels, ClockSimulator is what polls for interrupts.
    // Need to clock both PF & VF to ensure interrupts get delivered in a timely
    // manner (see bug 200374151)
    // A synced clock mechanism will be developed later
    if (Platform::IsPhysFunMode() || Platform::IsDefaultMode())
    {
        s_pChip->ClockSimulator(Cycles);
    }
    else
    {
        MASSERT(Platform::IsVirtFunMode());
        // On VF we don't actually clock the simulator
        // we just check if we have any pending interrupts on sockserv side
        s_pChip->ClockSimulator(0);
    }
}

UINT64 Platform::GetSimulatorTime()
{
    if ((s_SimulationMode == Hardware) && Vgpu::IsSupported() && !s_HybridEmulation)
    {
        return 0;
    }

    MASSERT(s_SimulationMode != Hardware || s_HybridEmulation);

    LwU064 result;

    if (!s_pChip)
        return 0;

    s_pChip->GetSimulatorTime(&result);

    return result;
}

double Platform::GetSimulatorTimeUnitsNS()
{
    if ((s_SimulationMode == Hardware) && Vgpu::IsSupported() && !s_HybridEmulation)
    {
        return 0;
    }

    MASSERT(s_SimulationMode != Hardware || s_HybridEmulation);

    if (!s_pChip)
        return 0;

    double result = s_pChip->GetSimulatorTimeUnitsNS();

    return result;
}

Platform::SimulationMode Platform::GetSimulationMode()
{
    if (!s_Initialized)
    {
        return s_SimulationMode;
    }
    else if ((s_SimulationMode == Hardware) && Vgpu::IsSupported())
    {
        return Fmodel;
    }
    else
    {
        return s_SimulationMode;
    }
}

bool Platform::IsAmodel()
{
    if (s_pChip == NULL)
    {
        Printf(Tee::PriHigh, "isAmodel() called before chiplib is initialized!\n");
        return false;
    }

    static bool bInitialized = false;
    static bool bIsChipAmodel = false;
    if (!bInitialized)
    {
        MASSERT(s_pChip);
        s_pAmod = (IAModelBackdoor *)s_pChip->QueryIface(IID_AMODEL_BACKDOOR_IFACE);
        if (s_pAmod)
        {
            LwU032 amodelQuery = 0;
            if ((0 == s_pChip->EscapeRead(
                        const_cast<char*>(ESCAPE_SUPPORT_PREFIX "isAmodelChip"),
                        0, sizeof(amodelQuery), &amodelQuery)) &&
                amodelQuery == 1)
            {
                if (0 == s_pChip->EscapeRead(const_cast<char*>("isAmodelChip"),
                        0, sizeof(amodelQuery), &amodelQuery))
                {
                    bIsChipAmodel = !!amodelQuery;
                }
            }
        }

        if (!bIsChipAmodel)
        {
           if (s_pAmod)
           {
               s_pAmod->Release();
               s_pAmod = 0;
           }
        }

        bInitialized = true;
    }

    return bIsChipAmodel;
}

int Platform::EscapeWrite(const char *Path, UINT32 Index, UINT32 Size, UINT32 Value)
{
    if ((s_SimulationMode == Hardware) && Vgpu::IsSupported())
    {
        return Vgpu::GetPtr()->EscapeWrite(Path, Index, Size, Value);
    }

    // IChip interface requires a non-const Path argument, sigh.
    int result = s_pChip->EscapeWrite(const_cast<char *>(Path),
                                Index, Size, Value);

    if (result == 0)
    {
        DumpEscapeData(0xff, Path, Index, Size, &Value, "escwr");
    }

    return result;
}

int Platform::EscapeRead(const char *Path, UINT32 Index, UINT32 Size, UINT32 *Value)
{
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        Vgpu* pVgpu = Vgpu::GetPtr();
        if (pVgpu)
        {
            return pVgpu->EscapeRead(Path, Index, Size, Value);
        }
    }

    // IChip interface requires a non-const Path argument, sigh.
    int result = s_pChip->EscapeRead(const_cast<char *>(Path),
                               Index, Size, Value);

    if (result == 0)
    {
        DumpEscapeData(0xff, Path, Index, Size, Value, "escrd");
    }

    return result;
}

// multi-gpu support for backdoor (escapes)
RC Platform::VerifyGpuId(UINT32 pciDomainNumber, UINT32 pciBusNumber, UINT32 pciDeviceNumber, UINT32 pciFunctionNumber, UINT32 resmanGpuId)
{
    MASSERT(s_SimulationMode != Hardware || s_HybridEmulation);
    if ((s_pChiplibGpuEscape == NULL) && (s_pChiplibGpuEscape2 == NULL))
    {
       // if the chiplib doesn't support the interface, just assume things are ok (multi-gpu escapes turn
       // into broadcast
       return OK;
    }

    UINT32 chipLibGpuId = 0;
    if (s_pChiplibGpuEscape2)
    {
        chipLibGpuId = s_pChiplibGpuEscape2->GetGpuId( pciBusNumber, pciDeviceNumber, pciFunctionNumber );
    }
    else
    {
        chipLibGpuId = s_pChiplibGpuEscape->GetGpuId( pciBusNumber, pciDeviceNumber, pciFunctionNumber );
    }

    if ( chipLibGpuId == resmanGpuId )
    {
       return OK;
    }
    else
    {
       Printf(Tee::PriNormal, "WARNING: Platform::VerifyGpuId : chipLibGpuId (0x%x) != resmanGpuId (0x%x)\n",
          chipLibGpuId, resmanGpuId );
       return RC::SOFTWARE_ERROR;
    }
}

static bool GpuEscapeWarningPrinted = false;

int Platform::GpuEscapeWrite(UINT32 GpuId, const char *Path, UINT32 Index, UINT32 Size, UINT32 Value)
{
    if ((s_SimulationMode == Hardware) && Vgpu::IsSupported() && !s_HybridEmulation)
    {
        return Vgpu::GetPtr()->EscapeWrite(Path, Index, Size, Value);
    }

    if ( s_pChiplibGpuEscape == NULL )
    {
       if (GpuEscapeWarningPrinted == false)
       {
          Printf(Tee::PriNormal, "WARNING: Platform::GpuEscapeWrite called but chiplib does not support per-gpu escapes\n" );
          GpuEscapeWarningPrinted = true;
       }
       return EscapeWrite( Path, Index, Size, Value );
    }

    int result = s_pChiplibGpuEscape->EscapeWrite(GpuId, Path, Index, Size, Value);

    if (result == 0)
    {
        DumpEscapeData(GpuId, Path, Index, Size, &Value, "escwr");
    }

    return result;
}

int Platform::GpuEscapeRead(UINT32 GpuId, const char *Path, UINT32 Index, UINT32 Size, UINT32 *Value)
{
    if (s_SimulationMode == Hardware && !s_HybridEmulation)
    {
        Vgpu* pVgpu = Vgpu::GetPtr();
        if (pVgpu)
        {
            return pVgpu->EscapeRead(Path, Index, Size, Value);
        }
    }

    if ( s_pChiplibGpuEscape == NULL )
    {
       if (GpuEscapeWarningPrinted == false)
       {
          Printf(Tee::PriNormal, "WARNING: Platform::GpuEscapeRead called but chiplib does not support per-gpu escapes\n" );
          GpuEscapeWarningPrinted = true;
       }
       return EscapeRead( Path, Index, Size, Value );
    }

    int result = s_pChiplibGpuEscape->EscapeRead(GpuId, Path, Index, Size, Value);

    if (result == 0)
    {
        DumpEscapeData(GpuId, Path, Index, Size, Value, "escrd");
    }

    return result;
}

int Platform::GpuEscapeWriteBuffer(UINT32 GpuId, const char *Path, UINT32 Index, size_t Size, const void* Buf)
{
    MASSERT(Buf != nullptr);

    if (s_pChiplibGpuEscape2 == nullptr)
    {
        // Attempt to colwert to older interface for accesses up to 4 bytes.
        if (Size <= 4)
        {
            return GpuEscapeWrite(GpuId, Path, Index, static_cast<UINT32>(Size), *static_cast<const UINT32*>(Buf));
        }

        if (!GpuEscapeWarningPrinted)
        {
            Printf(Tee::PriNormal, "WARNING: Platform::GpuEscapeWrite called but chiplib does not support per-gpu escapes2\n");
            GpuEscapeWarningPrinted = true;
        }

        return 1;
    }

    LwErr result = s_pChiplibGpuEscape2->EscapeWriteBuffer(GpuId, Path, Index, Size, Buf);

    if (result == LW_PASS)
    {
        DumpEscapeData(GpuId, Path, Index, Size, Buf, "escwr");
    }

    return (result == LW_PASS)?0:1;
}

int Platform::GpuEscapeReadBuffer(UINT32 GpuId, const char *Path, UINT32 Index, size_t Size, void *Buf)
{
    MASSERT(Buf != nullptr);

    if (s_pChiplibGpuEscape2 == nullptr)
    {
        // Attempt to colwert to older interface for accesses up to 4 bytes.
        if (Size <= 4)
        {
            return GpuEscapeRead(GpuId, Path, Index, static_cast<UINT32>(Size), static_cast<UINT32*>(Buf));
        }

        if (!GpuEscapeWarningPrinted)
        {
            Printf(Tee::PriNormal, "WARNING: Platform::GpuEscapeReadBuffer called but chiplib does not support per-gpu escapes2\n");
            GpuEscapeWarningPrinted = true;
        }

        return 1;
    }

    LwErr result = s_pChiplibGpuEscape2->EscapeReadBuffer(GpuId, Path, Index, Size, Buf);

    if (result == LW_PASS)
    {
        DumpEscapeData(GpuId, Path, Index, Size, Buf, "escrd");
    }

    return (result == LW_PASS)?0:1;
}

// end multi-gpu support for backdoor (escapes)
///------------------------------------------------------------------------------
// Amodel hooks to communicate information normally stored in instmem
//
void Platform::AmodAllocChannelDma(UINT32 ChID, UINT32 Class, UINT32 CtxDma,
                                   UINT32 ErrorNotifierCtxDma)
{
    if (s_pAmod)
    {
        s_pAmod->AllocChannelDma(ChID, Class, CtxDma, ErrorNotifierCtxDma);
    }
}

void Platform::AmodAllocChannelGpFifo(UINT32 ChID, UINT32 Class, UINT32 CtxDma,
                                      UINT64 GpFifoOffset, UINT32 GpFifoEntries,
                                      UINT32 ErrorNotifierCtxDma)
{
    if (s_pAmod)
    {
        s_pAmod->AllocChannelGpFifo(ChID, Class, CtxDma, GpFifoOffset, GpFifoEntries,
                                    ErrorNotifierCtxDma);
    }
}

void Platform::AmodFreeChannel(UINT32 ChID)
{
    if (s_pAmod)
    {
        s_pAmod->FreeChannel(ChID);
    }
}

void Platform::AmodAllocContextDma(UINT32 ChID, UINT32 Handle, UINT32 Class,
                                   UINT32 Target, UINT32 Limit, UINT32 Base,
                                   UINT32 Protect)
{
    if (s_pAmod)
    {
        s_pAmod->AllocContextDma(ChID, Handle, Class, Target, Limit, Base,
                                 Protect, NULL);
    }
}

void Platform::AmodAllocObject(UINT32 ChID, UINT32 Handle, UINT32 Class)
{
    if (s_pAmod)
    {
        s_pAmod->AllocObject(ChID, Handle, Class);
    }
}

void Platform::AmodFreeObject(UINT32 ChID, UINT32 Handle)
{
    if (s_pAmod)
    {
        s_pAmod->FreeObject(ChID, Handle);
    }
}

bool Platform::PassAdditionalVerification(GpuDevice *pGpuDevice, const char* path)
{
    // Go through escape interface if available, for per-device support.
    bool ret = true;
    UINT32 isEscapeSupported = 0;
    if (s_SimulationMode == Platform::Amodel &&
        pGpuDevice != nullptr &&
        pGpuDevice->EscapeReadBuffer("supported/PassAdditionalVerification", 0,
            sizeof(UINT32), &isEscapeSupported) == 0 &&
        isEscapeSupported != 0)
    {
        Printf(Tee::PriNormal, "Performing custom verification via escape...\n");
        ret = !pGpuDevice->EscapeWriteBuffer("PassAdditionalVerification",
            0, strlen(path) + 1, path);
        Printf(Tee::PriNormal, "Custom verification %s\n", ret ? "passed" : "failed");
    }
    else if (s_pAmod)
    {
        // amodel started to support versioning and custom verification step about the same time
        // using that to check if PassAdditionalVerification() is supported
        ChipLibVerFn pChipLib = (ChipLibVerFn)Xp::GetDLLProc(s_ChipSimModule, CHIP_LIB_VERSION);
        if (!pChipLib)
        {
            Printf(Tee::PriNormal, "WARNING: custom verification step is not supported, skipping it\n");
            return true;
        }

        Printf(Tee::PriNormal, "Performing custom verification via amodel backdoor...\n");
        ret = s_pAmod->PassAdditionalVerification(path);
        Printf(Tee::PriNormal, "Custom verification %s\n", ret ? "passed" : "failed");

    }
    return ret;
}

UINT32 Platform::ChipLibVersion()
{
    // only amodel supports versions now
    if (s_SimulationMode == Platform::Amodel)
    {
        ChipLibVerFn pChipLib = (ChipLibVerFn)Xp::GetDLLProc(s_ChipSimModule, CHIP_LIB_VERSION);
        if (!pChipLib)
            return 0;

        UINT32 version;
        if (sscanf(pChipLib(), "%d", &version) != 1)
        {
            Printf(Tee::PriNormal, "WARNING: unknown chiplib version\n");
            return 0;
        }

        return version;
    }
    return 0;
}

const char* Platform::ModelIdentifier()
{
    if (strlen(s_ModelIdentifier) == 0)
    {
        if (s_pAmod && s_pAmod->GetModelIdentifierString())
        {
            // lwrrently supported by amodel based sims only
            const char* identifier = s_pAmod->GetModelIdentifierString();
            strcpy(s_ModelIdentifier, identifier);
        }
        else
        {
            UINT32 identifier = 0;
            // "a" for FSA.
            // "p" for FSP.
            EscapeRead("CrcChainOverride", 0, 4, &identifier);
            strcpy(s_ModelIdentifier, (char*)&identifier);
        }
    }

    return s_ModelIdentifier;
}

bool Platform::LoadSimSymbols()
{
    return s_LoadSimSymbols;
}

bool Platform::ExtMemAllocSupported()
{
    return (s_MultiHeapSupported && (s_SimulationMode == Hardware) && !s_HybridEmulation);
}

RC Platform::SetupQueryIface(void* Module)
{
    // setup callbacks so chiplib could call us
    typedef bool (*SetupClientQueryIfaceFn)(call_IIfaceObject);
    SetupClientQueryIfaceFn setup_QueryIface =
        (SetupClientQueryIfaceFn)Xp::GetDLLProc(Module, "SetupClientQueryIface");

    if (!setup_QueryIface)
    {
        Printf(Tee::PriHigh, "Can not obtain SetupClientQueryIface interface from chiplib\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }
    if (!setup_QueryIface(call_mods_ifspec2_QueryIface))
    {
        Printf(Tee::PriHigh, "SetupClientQueryIface failed\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }
    return OK;
}

RC Platform::SetupIBusMem(void* Module)
{
    typedef bool (*SetupBusMemVTableFn)(const BusMemVTable&);
    SetupBusMemVTableFn call_SetupBusMemVTable =
        (SetupBusMemVTableFn)Xp::GetDLLProc(Module, "SetupBusMemVTable");

    if (!call_SetupBusMemVTable)
    {
        Printf(Tee::PriHigh, "Can not obtain SetupBusMemVTable interface from chiplib\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }

    BusMemVTable busmem;
    busmem.Release = call_mods_ifspec2_Release;
    busmem.BusMemWrBlk = call_mods_ifspec2_BusMemWrBlk;
    busmem.BusMemRdBlk = call_mods_ifspec2_BusMemRdBlk;
    busmem.BusMemCpBlk = call_mods_ifspec2_BusMemCpBlk;
    busmem.BusMemSetBlk = call_mods_ifspec2_BusMemSetBlk;
    if (!call_SetupBusMemVTable(busmem))
    {
        Printf(Tee::PriHigh, "SetupBusMemVTable failed\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }
    return OK;
}

RC Platform::SetupIInterrupt(void* Module)
{
    typedef bool (*SetupInterruptVTableFn)(const InterruptVTable&);
    SetupInterruptVTableFn call_SetupInterruptVTable =
        (SetupInterruptVTableFn)Xp::GetDLLProc(Module, "SetupInterruptVTable");

    if (!call_SetupInterruptVTable)
    {
        Printf(Tee::PriHigh, "Can not obtain SetupInterruptVTable interface from chiplib\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }

    InterruptVTable interrupt;
    interrupt.Release         = call_mods_ifspec2_Release;
    interrupt.HandleInterrupt = call_mods_ifspec2_HandleInterrupt;
    if (!call_SetupInterruptVTable(interrupt))
    {
        Printf(Tee::PriHigh, "SetupInterruptVTable failed\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }
    return OK;
}

RC Platform::SetupIInterrupt3(void* Module)
{
    typedef bool (*SetupInterrupt3VTableFn)(const Interrupt3VTable&);
    SetupInterrupt3VTableFn call_SetupInterrupt3VTable =
        (SetupInterrupt3VTableFn)Xp::GetDLLProc(Module, "SetupInterrupt3VTable");

    if (!call_SetupInterrupt3VTable)
    {
        Printf(Tee::PriLow, "Can not obtain SetupInterrupt3VTable interface from chiplib\n");
        // It's not an error if chiplib doesn't support this function, we return OK.
        return OK;
    }

    Interrupt3VTable interrupt;
    interrupt.Release = call_mods_ifspec2_Release;
    interrupt.HandleSpecificInterrupt
        = call_mods_ifspec2_HandleSpecificInterrupt;
    if (!call_SetupInterrupt3VTable(interrupt))
    {
        Printf(Tee::PriHigh, "SetupInterrupt3VTable failed\n");
        return RC::SIM_IFACE_NOT_FOUND;
    }
    return OK;
}

void Platform::SetDumpPhysicalAccess(bool flag)
{
    s_DumpPhysicalAccess = flag;
}

bool Platform::GetDumpPhysicalAccess()
{
    return s_DumpPhysicalAccess;
}

void Platform::DumpAddrData
(
    const volatile void* Addr,
    const void* Data,
    UINT32 Count,
    const char* Prefix
)
{
    const PHYSADDR addr2 = static_cast<PHYSADDR>(reinterpret_cast<uintptr_t>(Addr));
    DumpAddrData(addr2, Data, Count, Prefix);
}

void Platform::DumpAddrData
(
    const PHYSADDR Addr,
    const void* Data,
    UINT32 Count,
    const char* Prefix
)
{
    if(!Prefix)
    {
        return;
    }

    // physical dump
    if((!s_DumpPhysicalAccess) &&
        (Prefix[0] == 'p'))
    {
        return;
    }

    // virtual dump
    if ((!s_DumpVirtualAccess) &&
            (Prefix[0] == 'v'))
    {
        return;
    }

    if (ChiplibTraceDumper::GetPtr()->DumpChiplibTrace())
    {
        ChiplibOpBlock *lwrBlock = ChiplibTraceDumper::GetPtr()->GetLwrrentChiplibOpBlock();
        lwrBlock->AddBusMemOp(Prefix, Addr, Data, Count);
    }
    else
    {
        Printf(Tee::PriHigh, "%s 0x%llx: ", Prefix, Addr);
        const UINT08* data_ptr = (const UINT08*) Data;
        for (UINT32 i=0; i<Count; ++i)
        {
            Printf(Tee::PriHigh, "%2x ", data_ptr[i]);
        }
        Printf(Tee::PriHigh, "\n");
    }

}

void Platform::DumpCfgAccessData
(
    INT32       DomainNumber,
    INT32       BusNumber,
    INT32       DeviceNumber,
    INT32       FunctionNumber,
    INT32       Address,
    const       void* Data,
    UINT32      Count,
    const char* Prefix
)
{
    if (!Platform::GetDumpPciAccess() || !Prefix)
    {
        return;
    }

    if (ChiplibTraceDumper::GetPtr()->DumpChiplibTrace())
    {
        ChiplibOpBlock *lwrBlock = ChiplibTraceDumper::GetPtr()->GetLwrrentChiplibOpBlock();
        lwrBlock->AddCfgPioOp(Prefix, DomainNumber, BusNumber, DeviceNumber,
                              FunctionNumber, Address, Data, Count);
    }
    else
    {
        bool read = !strcmp(Prefix, "cfgrd");
        string msg = Utility::StrPrintf(
            "Platform::%s%02d : Addr:0x%08x -> Value:",
            read? "PciRead" : "PciWrite", Count*8, Address);

        const UINT08* data_ptr = (const UINT08*) Data;
        for (UINT32 i=0; i<Count; ++i)
        {
            msg += Utility::StrPrintf("%02x ", data_ptr[i]);
        }
        Printf(Tee::PriHigh, "%s\n", msg.c_str());
    }
}

void Platform::DumpEscapeData(UINT32 GpuId, const char *Path,
                              UINT32 Index, size_t Size,
                              const void* Buf, const char* Prefix)
{
    if (ChiplibTraceDumper::GetPtr()->DumpChiplibTrace())
    {
        if (Size > (std::numeric_limits<UINT32>::max)())
        {
            Printf(Tee::PriHigh, "Error: Platform::DumpEscapeData Size value larger than UINT32 - ignored\n");
            MASSERT(!"Platform::DumpEscapeData Size value larger than UINT32");
            return;
        }
        ChiplibOpBlock *lwrBlock = ChiplibTraceDumper::GetPtr()->GetLwrrentChiplibOpBlock();
        lwrBlock->AddEscapeOp(Prefix, GpuId, Path, Index, static_cast<UINT32>(Size), Buf);
    }
}

bool Platform::PollfuncWrap
(
    Tasker::PollFunc pollFunc,
    void *pArgs,
    const char* pollFuncName
)
{
    bool ret = false;

    if (ChiplibTraceDumper::GetPtr()->DumpChiplibTrace() && Platform::IsChiplibLoaded())
    {
        bool reEnableInterruptState = false;
        if (Platform::GetLwrrentIrql() == Platform::NormalIrql)
        {
            // In case chiplib is loaded, NormalIrql means interrupt
            // is enabled. Disable it temporarily
            Platform::DisableInterrupts();
            reEnableInterruptState = true;
        }

        {
            ChiplibOpScope pollScope(pollFuncName ? pollFuncName : "", NON_IRQ,
                                     ChiplibOpScope::SCOPE_POLL,
                                     NULL);

            ret = pollFunc(pArgs);

            if (!ret)
            {
                // Cancel chiplibOp dump for failed poll.
                pollScope.CancelOpBlock();
            }
        }

        if (reEnableInterruptState)
        {
            Platform::EnableInterrupts();
        }
    }
    else
    {
        ret = pollFunc(pArgs);
    }

    return ret;
}

void Platform::PreGpuMonitorThread()
{
    if (ChiplibTraceDumper::GetPtr()->DumpChiplibTrace() &&
        !ChiplibTraceDumper::GetPtr()->RawDumpChiplibTrace())
    {
        ChiplibTraceDumper::GetPtr()->DisableChiplibTraceDump();
    }
}

// Null implementation of refcounting
void Chipset::AddRef()
{
}

void Chipset::Release()
{
}

IIfaceObject* Chipset::QueryIface(IID_TYPE id)
{
    // We make some inferences about the chiplib's support for certain features
    // based on what interfaces it queries from us.  This is a hack but anything
    // else would require touching code in a lot of fmodels.
    switch (id)
    {
    case IID_BUSMEM_IFACE:
        if (s_ForceChiplibSysmem)
        {
            // Lie and pretend we don't support this
            return NULL;
        }
        else
        {
            Printf(Tee::PriDebug, "IFSPEC: mods supplies IBusMem to sim.\n");
            s_ChiplibSupportsExternalSysmem = true;
            return (IBusMem *)this;
        }
    case IID_INTERRUPT_IFACE:
        Printf(Tee::PriDebug, "IFSPEC: mods supplies IInterrupt to sim.\n");
        s_ChiplibSupportsInterrupts = true;
        return (IInterrupt *)this;
    case IID_INTERRUPT3_IFACE:
        Printf(Tee::PriDebug, "IFSPEC: mods supplies IInterrupt3 to sim.\n");
        s_ChiplibSupportsInterrupts = true;
        return (IInterrupt3 *)this;
    default:
        return NULL;
    }
}

void Chipset::HandleInterrupt()
{
    // This interface doesn't tell us which IRQ fired.  So, in the absence of
    // any real information, we are forced to assume it was IRQ 0 (which we
    // generally seem to end up with by default).  An alternative would be to
    // call the handler for *all* IRQs, but this would probably be bad too.
    AddPendingInterrupt(0);
}

void Chipset::HandleSpecificInterrupt(LwU032 irqNumber)
{
    AddPendingInterrupt(irqNumber);
}

BusMemRet Chipset::BusMemRdBlk(LwU064 address, void *appdata, LwU032 count)
{
    RC rc;
    rc = Platform::PhysRd(address, appdata, count);
    return (rc == OK) ? BUSMEM_HANDLED : BUSMEM_NOTHANDLED;
}

BusMemRet Chipset::BusMemWrBlk(LwU064 address, const void *appdata, LwU032 count)
{
    RC rc;
    rc = Platform::PhysWr(address, appdata, count);
    return (rc == OK) ? BUSMEM_HANDLED : BUSMEM_NOTHANDLED;
}

BusMemRet Chipset::BusMemCpBlk(LwU064 dest, LwU064 source, LwU032 count)
{
    return BUSMEM_NOTHANDLED;
}

BusMemRet Chipset::BusMemSetBlk(LwU064 address, LwU032 size, void* data, LwU032 data_size)
{
    return BUSMEM_NOTHANDLED;
}

//------------------------------------------------------------------------------
RC LoadDLLVerbose
(
    const char * filename,
    void **pmodule
)
{
    Printf(Tee::PriHigh, "Loading %s\n", filename);

    UINT32 loadFlags = Xp::UnloadDLLOnExit;
    bool chipLibDeepBind = false;

    RC rc;
    JavaScriptPtr pJs;
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_ChipLibDeepBind, &chipLibDeepBind));
    if (chipLibDeepBind)
        loadFlags |= Xp::DeepBindDLL;

    rc = Xp::LoadDLL(filename, pmodule, loadFlags);
    if (OK != rc)
    {
        string searched_location = Utility::DefaultFindFile(filename, true);
        if (searched_location.size() > 0)
        {
            Printf(Tee::PriHigh, "Trying loading again from %s\n", searched_location.c_str());
            rc.Clear();
            rc = Xp::LoadDLL(searched_location.c_str(), pmodule, loadFlags);
        }
    }

    if (OK == rc)
    {
        s_DllModulesToUnloadOnExit.push_back(*pmodule);
    }
    else
    {
        Printf(Tee::PriHigh, "Cannot load library %s\n", filename);
    }

    return rc;
}

//------------------------------------------------------------------------------
static RC GetDLLProcVerbose
(
    void* module,
    const char * symname,
    void ** ppFunc,
    const char * libname
)
{
    *ppFunc = Xp::GetDLLProc(module, symname);
    if (!*ppFunc)
    {
        Printf(Tee::PriHigh, "Cannot find %s in %s.\n", symname, libname);
        return RC::SIM_IFACE_NOT_FOUND;
    }
    return OK;
}

//------------------------------------------------------------------------------
// This function is called for the main sim, and optionally again for the
// amodel sim.
//
RC LoadChiplib (const char * chiplibName, void **pmodule, IChip ** ppChip)
{
    void * chiplibModule;
    IChip * pChip;
    RC rc;

    CHECK_RC( LoadDLLVerbose(chiplibName, &chiplibModule));

    // Get our initial IChip* from the chiplib.
    QueryIfaceFn pQueryFn;
    CHECK_RC( GetDLLProcVerbose(chiplibModule, QUERY_PROC_NAME,
            (void**)&pQueryFn, chiplibName));

    pChip = (IChip *)pQueryFn(IID_CHIP_IFACE);

    if (!pChip)
    {
        Printf(Tee::PriHigh, "Could not obtain IChip (iid=%d) interface\n",
            IID_CHIP_IFACE);
        return RC::SIM_IFACE_NOT_FOUND;
    }

    // Identify what ifspec this sim supports.
    //
    //  - "ifspec1" sims give mods raw C++ object pointers to use.
    //    These are not safe on linux if mods & sim use different gcc versions.
    //      Examples: pre-fermi fmodel/cmodel, hwchip.so
    //
    //  - ifspec-2 sims have a safe "C" interface.
    //      Examples: fermi fmodel, amodel
    //
    //  - IFSPEC3 is ifspec-2 extended to more interfaces, plus some other
    //    minor improvements.
    //
    //  We have a shim.so that can colwert gcc 2.95 ifspec1 sims to IFSPEC3.

    ChipLibVerFn pChipLibVerFn = (ChipLibVerFn)
            Xp::GetDLLProc(chiplibModule, CHIP_LIB_VERSION);

    int ifspecVersion = (NULL == pChipLibVerFn) ? 1 : 2;

    if ((sizeof(size_t) == 4) && // shim is only needed in 32 bit MODS
        (1 == ifspecVersion) &&
        (Xp::OS_LINUXSIM == Xp::GetOperatingSystem()))
    {
        Printf(Tee::PriHigh, "Could not find %s in %s, try ifspec3 shim.\n",
                CHIP_LIB_VERSION, chiplibName);

        const char * shimName = "shim.so";
        void * shimModule;
        RC tmpRc = LoadDLLVerbose(shimName, &shimModule);
        if (OK == tmpRc)
        {
            // Try again to get the ChipLibVersion function-pointer.
            CHECK_RC( GetDLLProcVerbose(shimModule, CHIP_LIB_VERSION,
                    (void**)&pChipLibVerFn, shimName));
            chiplibModule = shimModule;
        }
    }

    // Ifspec3 has a mechanism to free the proxy objects.
    // Use this to detect this ifspec rather than the version-string.
    // It turns out amodel uses ifspec-2 but returns version "3".
    if (NULL != Xp::GetDLLProc(chiplibModule, "FreeSimProxies"))
        ifspecVersion = 3;

    const char * chiplibVersionString = "unknown";

    if (pChipLibVerFn)
        chiplibVersionString = (*pChipLibVerFn)();

    Printf(Tee::PriHigh, "Chiplib version is \"%s\", ifspec %d\n",
            chiplibVersionString, ifspecVersion);

    if (3 == ifspecVersion)
    {
        pChip = (IChip*) IFSPEC3::CreateProxyForSimObject(
                IID_CHIP_IFACE, pChip, chiplibModule);

        CHECK_RC(IFSPEC3::PushIIfaceObjectFptrsToSim (chiplibModule));
        CHECK_RC(IFSPEC3::PushIInterruptFptrsToSim (chiplibModule));
        IFSPEC3::PushIInterrupt3FptrsToSim (chiplibModule); // Optional interface
        CHECK_RC(IFSPEC3::PushIBusMemFptrsToSim (chiplibModule));
    }
    else if (2 == ifspecVersion)
    {
        Printf(Tee::PriDebug, "IFSPEC2: sim supplies IChip to mods.\n");
        pChip = new ChipProxy(chiplibModule, pChip);

        // Pass pointers to the mods-side services to the chiplib, so that it
        // can call us back.
        CHECK_RC(Platform::SetupQueryIface (chiplibModule));
        CHECK_RC(Platform::SetupIInterrupt (chiplibModule));
        CHECK_RC(Platform::SetupIInterrupt3(chiplibModule));
        CHECK_RC(Platform::SetupIBusMem (chiplibModule));
    }
    // Else:
    //     Proceed with the naked IChip*.
    //     Hopefully it was built with an ABI-compatible C++ compiler.

    // Success, probably.  Return results.
    *pmodule = chiplibModule;
    *ppChip = pChip;

    return rc;
}

namespace Platform
{
    Xp::ClockManager* GetClockManager()
    {
        // Lazily allocate clock manager.
        // We can't allocate it in Platform::Initialize(), because
        // CheetAh::SocPtr() has not been initialized there yet.
        if (!s_pClockManager)
        {
            unique_ptr<Xp::ClockManager> pClkMgr(
                    CheetAh::SocPtr()->AllocBuiltinClockManager());
            // We take ownership of the clock manager and release
            // it later in Platform::Cleanup().
            s_pClockManager = pClkMgr.release();
        }
        return s_pClockManager;
    }
}

namespace
{
    void ChecktoTrackAddress
    (
        const volatile void *Addr,
        bool isVirtual,
        const int Mode,
        const void *Data,
        UINT64 tempTrackTime
    )
    {

        if((s_TrackAll || s_PhysicalAddress) && //If tracking all or has a Physical Address
           (s_TrackType & Mode) && //Tracking Read/Write values
           (!s_TrackValueEnable || s_TrackValue == *(UINT32 *)const_cast<void *>(Data) ) ) //fulfills the value filter
        {
            const char* prefix = TRACK_READ == Mode ? "track_rd" : "track_wr";
            if(isVirtual)
            {
                PHYSADDR physAddr;
                if(OK == Platform::VirtualToPhysicalInternal(&physAddr, Addr) &&
                  (s_TrackAll || physAddr == s_PhysicalAddress)) //the physical address matches (or tracking all)
                {
                    if(SIMCLK == s_TrackMode || WALLCLK == s_TrackMode)
                        s_TrackTime += GetTrackTimeNS() - tempTrackTime;
                    AddressesCallStack( (void *)physAddr, prefix, Data);
                }
            }
            else
            {
                if(s_TrackAll || Addr == (void *)s_PhysicalAddress)
                {
                    if(SIMCLK == s_TrackMode || WALLCLK == s_TrackMode)
                        s_TrackTime += GetTrackTimeNS() - tempTrackTime;
                    AddressesCallStack(Addr, prefix, Data);
                }
            }
        }

    }

    void AddressesCallStack(const volatile void *Addr, const char *Prefix, const void *Data )
    {
        if(CALLSTACK == s_TrackMode) //Gets the call stack when the address is accessed
        {
#ifdef _WIN32
        Printf(Tee::PriNormal, "Warning: Address call track tracking is not supported in Windows.\n");
#else
            Printf(Tee::PriHigh, "%s: %p with value 0x%08x was accessed. Call Stack: \n", Prefix, Addr,
                                                                  *(UINT32 *)const_cast<void *>(Data) );
            int j, nptrs;
            void *buffer[100];
            char **strings;

            nptrs = backtrace(buffer, 100);
            strings = backtrace_symbols(buffer, nptrs);

            if (strings == NULL)
            {
                Printf(Tee::PriHigh, "Could not find the function symbols\n");
                return;
            }
            for(j = 0; j < nptrs; j++)
            {
                Printf(Tee::PriHigh, "          %s\n", strings[j]);
            }
            free(strings);
#endif
        }
        else if(PRINT == s_TrackMode) // Just print that the address was accessed
        {
            Printf(Tee::PriHigh, "%s: %p with value 0x%08x was accessed.\n", Prefix, Addr,
                                                     *(UINT32 *)const_cast<void *>(Data) );
        }
        else if(BREAKPOINT == s_TrackMode) //Breakpoints when addressed is accessed
        {
            Printf(Tee::PriHigh, "%s: %p with value 0x%08x was accessed.\n", Prefix, Addr,
                                                     *(UINT32 *)const_cast<void *>(Data) );
            Platform::BreakPoint(OK, __FILE__, __LINE__);
        }
        else if(SIMCLK == s_TrackMode) //prints the current cumulative sim time
        {
            Printf(Tee::PriHigh, "%s: %p with value 0x%08x was accessed. Current Sim Time is: %lld ns\n", Prefix, Addr,
                                                                     *(UINT32 *)const_cast<void *>(Data), s_TrackTime );
        }
        else if(WALLCLK == s_TrackMode) //prints the current cumulative wall time
        {
            Printf(Tee::PriHigh, "%s: %p with value 0x%08x was accessed. Current Wall Time is: %lld ns\n", Prefix, Addr,
                                                                      *(UINT32 *)const_cast<void *>(Data), s_TrackTime );
        }
    }

    UINT64 GetTrackTimeNS()
    {
        if (WALLCLK == s_TrackMode)
        {
            return Xp::GetWallTimeNS(); //return the current wall time in nano seconds
        }
        else //SIMCLK == s_TrackMode
        {
            double ticks = (double)Platform::GetSimulatorTime();
            double nsPerTick = Platform::GetSimulatorTimeUnitsNS();
            UINT64 ns = (UINT64)(ticks * nsPerTick);
            return ns; //return the current sim time in nano seconds
        }
    }

    UINT64 SetupTracking()
    {
        if(s_TrackBAR != TRACK_PHYSICAL) //Tracking is barN, so get the barN offset
        {
            GpuDevMgr* const pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
            GpuSubdevice *pGpuSubDev = pGpuDevMgr->GetSubdeviceByGpuInst(0);
            if(NULL != pGpuSubDev)
            {
                switch(s_TrackBAR)
                {
                    case TRACK_BAR0:
                        s_PhysicalAddress += pGpuSubDev->GetPhysLwBase();
                        s_TrackBAR = TRACK_PHYSICAL;
                        break;
                    case TRACK_BAR1:
                        s_PhysicalAddress += pGpuSubDev->GetPhysFbBase();
                        s_TrackBAR = TRACK_PHYSICAL;
                        break;
                    case TRACK_BAR2:
                        s_PhysicalAddress += pGpuSubDev->GetPhysInstBase();
                        s_TrackBAR = TRACK_PHYSICAL;
                        break;
                    default:
                        s_TrackBAR = TRACK_PHYSICAL;
                        break;
                }
            }
        }

        if(SIMCLK == s_TrackMode || WALLCLK == s_TrackMode) //Tracking Mode set to get time
            return GetTrackTimeNS();
        else
            return 0;
    }
}

// Copied from drivers/resman/kernel/inc/objlwlink.h
// (MODS can't include that file.)
#define LW_LWLINK_ARCH_CONNECTION                         31:0
#define LW_LWLINK_ARCH_CONNECTION__SIZE_1                   32
#define LW_LWLINK_ARCH_CONNECTION_DISABLED          0x00000000
#define LW_LWLINK_ARCH_CONNECTION_PEER_MASK                7:0
#define LW_LWLINK_ARCH_CONNECTION_ENABLED                  8:8
#define LW_LWLINK_ARCH_CONNECTION_PHYSICAL_LINK          20:16
#define LW_LWLINK_ARCH_CONNECTION_RESERVED               29:21
#define LW_LWLINK_ARCH_CONNECTION_PEERS_COMPUTE_ONLY     30:30
#define LW_LWLINK_ARCH_CONNECTION_CPU                    31:31

RC Platform::GetForcedLwLinkConnections(UINT32 gpuInst, UINT32 numLinks, UINT32 *pLinkArray)
{
    RC rc;

    if (!s_pChip || s_LwlinkForceAutoconfig)
        return RC::UNSUPPORTED_FUNCTION;

    return GetChipConnections(gpuInst, numLinks, pLinkArray, "LWLink");
}

// Copied from drivers/resman/kernel/inc/hshub.h
// (MODS can't include that file.)
#define LW_C2C_ARCH_CONNECTION                         31:0
#define LW_C2C_ARCH_CONNECTION__SIZE_1                   32
#define LW_C2C_ARCH_CONNECTION_DISABLED          0x00000000
#define LW_C2C_ARCH_CONNECTION_PEER_MASK                7:0
#define LW_C2C_ARCH_CONNECTION_ENABLED                  8:8
#define LW_C2C_ARCH_CONNECTION_PHYSICAL_LINK          20:16
#define LW_C2C_ARCH_CONNECTION_RESERVED               30:21
#define LW_C2C_ARCH_CONNECTION_CPU                    31:31

RC Platform::GetForcedC2CConnections(UINT32 gpuInst, UINT32 numLinks, UINT32 *pLinkArray)
{
    if (!s_pChip)
        return RC::UNSUPPORTED_FUNCTION;

    return GetChipConnections(gpuInst, numLinks, pLinkArray, "C2C");
}

static RC Platform::GetChipConnections(UINT32 gpuInst, UINT32 numLinks, UINT32 *pLinkArray, string connType)
{
    RC rc;

    GpuDevMgr* const pGpuDevMgr =
        static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
    if (!pGpuDevMgr)
        return RC::UNSUPPORTED_FUNCTION;

    if (connType != "C2C" && connType != "LWLink")
    {
        Printf(Tee::PriError, "Invalid connection type\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    GpuSubdevice *pSubdev = pGpuDevMgr->GetSubdeviceByGpuInst(gpuInst);
    if (nullptr == pSubdev)
    {
        Printf(Tee::PriHigh, "%s : Unable to find subdevice with instance %u\n",
               __FUNCTION__, gpuInst);
        return RC::SUB_DEVICE_ILWALID_INDEX;
    }

    UINT32 gpuId = pSubdev->GetGpuId();
    string key;

    for (UINT32 link = 0; link < numLinks; ++link)
    {
        key = Utility::StrPrintf("CPU_MODEL|CM_ATS_ADDRESS|%s%u", connType.c_str(), link);
        UINT32 data = 0;
        UINT32 result = GpuEscapeRead(gpuId, key.c_str(), 0, 4, &data);

        if (result != 0)
        {
            return RC::UNSUPPORTED_FUNCTION;
        }

        pLinkArray[link] = data;
    }

    return OK;
}

RC Platform::VirtualAlloc(void **ppReturnedAddr, void *pBase, size_t size, UINT32 protect, UINT32 vaFlags)
{
    if (!s_pChip)
    {
        return Xp::AllocAddressSpace(size,
                                     ppReturnedAddr,
                                     pBase,
                                     protect,
                                     vaFlags);
    }
    return RC::UNSUPPORTED_FUNCTION;
}

void *Platform::VirtualFindFreeVaInRange(size_t size, void *pBase, void *pLimit, size_t align)
{
    if (!s_pChip)
    {
        return Xp::VirtualFindFreeVaInRange(size, pBase, pLimit, align);
    }
    return NULL;
}

RC Platform::VirtualFree(void *pAddr, size_t size, Memory::VirtualFreeMethod vfMethod)
{
    if (!s_pChip)
    {
        return Xp::FreeAddressSpace(pAddr, size, vfMethod);
    }
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::VirtualProtect(void *pAddr, size_t size, UINT32 protect)
{
    if (!s_pChip)
    {
        return Xp::VirtualProtect(pAddr, size, protect);
    }
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::VirtualMadvise(void *pAddr, size_t size, Memory::VirtualAdvice advice)
{
    if (!s_pChip)
    {
        return Xp::VirtualMadvise(pAddr, size, advice);
    }
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::GetLwLinkLineRate
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    UINT32 *speed
)
{
    if (!s_pChip)
    {
        return Xp::GetLwLinkLineRate(domain, bus, device, func, speed);
    }

    return RC::UNSUPPORTED_FUNCTION;
}

bool Platform::IsVirtFunMode()
{
    static bool s_IsVirtFunModeCache = !Xp::GetElw("SRIOV_GFID").empty();
    return s_IsVirtFunModeCache;
}

bool Platform::IsPhysFunMode()
{
    static bool s_IsPhysFunModeCache =
        !VmiopElwManager::Instance()->GetConfigFile().empty();
    return s_IsPhysFunModeCache;
}

bool Platform::IsDefaultMode()
{
    return !IsVirtFunMode() && !IsPhysFunMode();
}

const vector<const char*>& Platform::GetChipLibArgV()
{
    return g_ChipLibArgV;
}

UINT32 Platform::GetReservedSysmemMB()
{
    return s_ReservedSysmemMB;
}

int Platform::VerifyLWLinkConfig(size_t Size, const void* Buf, bool supersetOK)
{
    MASSERT(Size == sizeof(LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS));
    const LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS * pStatusParams =
                                               (const LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS *)Buf;
    int match = supersetOK ? (enabledLWLinkMask == (enabledLWLinkMask & pStatusParams->enabledLinkMask)) :
                             (enabledLWLinkMask == pStatusParams->enabledLinkMask);
    return !match;
}

bool Platform::ManualCachingEnabled()
{
    return s_EnableManualCaching;
}

bool Platform::IsSelfHosted()
{
    MASSERT(s_pChip);

    static bool s_FirstTimeCall = true;
    static bool s_IsSelfHosted = false;

    if (s_FirstTimeCall)
    {
        UINT32 isSelfHosted = 0;
        UINT32 isSelfHostedEscapeReadSupported = 0;

        if ((GetSimulationMode() != Platform::Amodel)
            ||
            ((0 == s_pChip->EscapeRead(
                    "supported/IsSelfHosted",
                    0,
                    sizeof(isSelfHostedEscapeReadSupported),
                    &isSelfHostedEscapeReadSupported)) &&
                isSelfHostedEscapeReadSupported != 0)
           )
        {
            if (0 == s_pChip->EscapeRead(const_cast<char*>("IsSelfHosted"),
                  0, sizeof(isSelfHosted), &isSelfHosted))
            {
                s_IsSelfHosted = (isSelfHosted != 0);
            }
        }
        s_FirstTimeCall = false;
    }
    return s_IsSelfHosted;
}
