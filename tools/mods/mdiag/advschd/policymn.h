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

//! \file
//! \brief Defines the singleton that manages the PolicyManager module

#ifndef INCLUDED_POLICYMN_H
#define INCLUDED_POLICYMN_H

#ifndef INCLUDED_MASSERT_H
#include "core/include/massert.h"
#endif

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

#ifndef INCLUDED_TEE_H
#include "core/include/tee.h"
#endif

#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

#ifndef INCLUDED_RUNLIST_H
#include "gpu/utility/runlist.h"
#endif

#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

#ifndef INCLUDED_STL_LIST
#include <list>
#define INCLUDED_STL_LIST
#endif

#ifdef _WIN32
#   pragma warning(push)
#   pragma warning(disable: 4265) // class has virtual functions, but destructor is not virtual
#endif
#include <functional>
#ifdef _WIN32
#   pragma warning(pop)
#endif

#include "core/include/regexp.h"

class GpuDevice;
class GpuSubdevice;
class PmAction;
class PmAction_Goto;
class PmActionBlock;
class PmChannel;
class PmChannelDesc;
class PmChannelWrapper;
class PmVaSpace;
class PmVaSpaceDesc;
class PmVfTest;
class PmVfTestDesc;
class PmSmcEngine;
class PmSmcEngineDesc;
class PmCopyMethod;
class PmEvent;
class PmEventManager;
class PmGilder;
class PmGpuDesc;
class PmMemMapping;
class PmMemRange;
class PmNonStallInt;
class PmReplayableInt;
class PmNonReplayableInt;
class PmAccessCounterInt;
class PmErrorLoggerInt;
class PmParser;
class PmPotentialEvent;
class PmRunlistDesc;
class PmSubsurface;
class PmSurface;
class PmSurfaceDesc;
class PmTest;
class PmTestDesc;
class PmTrigger;
class RegExp;
class Runlist;
class MdiagSurf;
class ArgReader;
class LWGpuResource;
typedef vector<PmChannel*>    PmChannels;
typedef vector<PmVaSpace*>    PmVaSpaces;
typedef vector<PmVfTest*>     PmVfTests;
typedef vector<PmSmcEngine*>  PmSmcEngines;
typedef vector<PmMemMapping*> PmMemMappings;
typedef vector<PmMemRange>    PmMemRanges;
typedef vector<PmSubsurface*> PmSubsurfaces;
typedef vector<PmSurface*>    PmSurfaces;
typedef set<PmTest*>          PmTests;
typedef vector<GpuSubdevice*> GpuSubdevices;
typedef vector<GpuDevice*>    GpuDevices;
typedef vector<Runlist*>      Runlists;
typedef set<LWGpuResource*>   LWGpuResources;
typedef map<string, void *>   Mutexes;

#define UNITIALIZED_MMU_ENGINE_ID _UINT32_MAX

//--------------------------------------------------------------------
//! \brief Singleton that manages the PolicyManager module
//!
//! The PolicyManager module is used to test advanced scheduling in
//! mods.  This is the big granddaddy object that owns all the other
//! objects in the module.
//!
//! OVERVIEW OF POLICYMANAGER
//!
//! PolicyManager is a very event-driven module.  While a test runs,
//! it generates "events" (such as page-fault).  PolicyManager can
//! create "triggers" that fire when an event oclwrs.  When a trigger
//! fires, it exelwtes a block of "actions" such as remapping the
//! faulting page, resubmitting the runlist, unmapping some other
//! page, etc.
//!
//! The triggers and actions are created by parsing a "policy file"
//! when mods starts.  When a test end, we "gild" the results by
//! comparing the events that oclwrred with the "potential events".
//! For example, unmapping a page creates a potential page fault, and
//! it's an error to get a page fault otherwise.
//!
//! PolicyManager supports tests run in series (e.g. mods production
//! tests), but this hasn't been exploited yet.  Gilding oclwrs at the
//! end of each test.  There can also be several tests run in
//! parallel; PolicyManager considers these to be subtests of a single
//! big test.
//!
//! POLICYMANAGER CLASSES:
//!
//! - PmParser:         Parses the policy file
//! - PmEventManager:   Holds the triggers & action blocks created by the
//!                     parser, and handles incoming events.
//! - PmTrigger:        Abstract base class for all triggers
//! - PmAction:         Abstract base class for all actions
//! - PmActionBlock:    Container for a group of actions that execute in series
//!                     when an associated trigger fires.
//! - PmEvent:          Abstract base class for all events
//! - PmGilder:         Runs at the end of a test, determining whether there
//!                     were any errors & writing the output file.
//! - PmPotentialEvent: Abstract base class for potential events, for gilding
//!
//! There are also some helper classes.  PmChannel is an abstract
//! wrapper around the various channel classes in mods, to provide a
//! consistent (stripped-down) API for PolicyManager.  PmSurface is a
//! thin wrapper around Surface2D, which keeps track of memory-related
//! data with the aid of helper classes PmMemRange, PmSubsurface, and
//! PmMemMapping.  See pmutils.h for several other small classes.
//!
//! EXTERNAL SUPPORT:
//!
//! There are various ways to set assorted options for PolicyManager,
//! but the important calls that have to occur are:
//!
//! - The command line must contain the policyfile, which makes the
//!   command-line processor call PmParser::Parse().
//! - The tests must call Add[Sub]Channel() and AddSurface() to
//!   register their channels/surfaces with PolicyManager.
//! - Someone has to call StartTest() and EndTest() at the start/end
//!   of the test.
//!
//! RULES FOR POLICYMANAGER INCLUDE FILES
//!
//! - The general rule for PolicyManager include files is, "if a class
//!   is owned by another class, it can #include the container class".
//!   As a special case, anyone can include pmutils.h except policymn.h.
//!   This results in the following hierarchy:
//!   - Anyone can include policymn.h
//!   - Anyone can include pmutils.h except policymn.h
//!   - pmevent.h can include pmsurf.h
//!   - pmaction.h and pmtrigg.h can include pmevntmn.h
//!   - Other than that, .h files can't include other .h files.
//! - policymn.h contains forward declarations of all the PolicyManager
//!   classes, so no other .h file should have to do those forward
//!   declarations.
//! - policymn.h also includes some files that almost everyone needs,
//!   so no other PolicyManager file has to include them:
//!   - massert.h
//!   - rc.h
//!   - tee.h
//!   - <string>
//!   - <vector>
//!   policymn.h may include some things that aren't on the above
//!   list, but those other includes shouldn't be relied on.
//!
class PolicyManager
{
public:
    ~PolicyManager();
    static PolicyManager *Instance();
    static void           ShutDown();
    static bool          HasInstance();

    PmParser            *GetParser()        const { return m_pParser; }
    PmEventManager      *GetEventManager()  const { return m_pEventMgr; }
    PmGilder            *GetGilder()        const { return m_pGilder; }
    const PmChannels    &GetActiveChannels() const { return m_ActiveChannels; }
    const PmChannels    &GetInactiveChannels() const { return m_InactiveChannels; }
    const PmVfTests         &GetActiveVfs() const { return m_ActiveVfs; }
    const PmVfTests         &GetInactiveVfs() const { return m_InactiveVfs; }
    const PmSmcEngines      &GetActiveSmcEngines() const { return m_ActiveSmcEngines; }
    const PmSmcEngines      &GetInactiveSmcEngines() const { return m_InactiveSmcEngines; }
    const PmVaSpaces    &GetActiveVaSpaces() const { return m_ActiveVaSpaces; }
    const PmVaSpaces    &GetInactiveVaSpaces() const { return m_InactiveVaSpaces; }
    PmChannels           GetActiveChannels(const GpuDevice *pGpuDevice) const;
    PmChannel *          GetChannel(LwRm::Handle hCh) const;
    PmVaSpace *          GetVaSpace(LwRm::Handle hVaSpace, PmTest * pTest) const;
    PmVaSpace *          GetActiveVaSpace(LwRm::Handle hVaSpace, LwRm* pLwRm) const;
    PmSurfaces           GetActiveSurfaces()      const { return m_ActiveSurfaces; }
    PmSubsurfaces        GetActiveSubsurfaces()   const;
    PmTest *             GetTestFromUniquePtr(void *pTestUniquePtr) const;
    PmTests              GetActiveTests() const { return m_ActiveTests; }
    GpuSubdevices        GetGpuSubdevices() const;
    const GpuDevices    &GetGpuDevices()        const { return m_GpuDevices; }
    UINT32               GetTestNum()           const { return m_TestNum; }
    bool                 IsInitialized()        const;
    void                *GetMutex() const { return m_Mutex; }
    void                 DisableMutexInActions(bool Disable);
    bool                 MutexDisabledInActions() const;
    void                 StartTimer(const string &TimerName);
    UINT64               GetTimerStartUS(const string &TimerName) const;
    void                 AddUtlMutex(void * mutex, const string & name);
    RC                   DeleteUtlMutex(const string & name);
    Mutexes              GetUtlMutex(const RegExp & name);
    auto GetNonReplayableInts() const { return m_NonReplayableIntMap; }
    auto GetAccessCounterInts() const { return m_AccessCounterIntMap; }

    RC AddTest(PmTest *pTest);
    RC AddChannel(PmChannel *pChannel);
    RC AddVaSpace(PmVaSpace *pVaSpace);
    RC AddVf(PmVfTest *pVf);
    RC AddSmcEngine(PmSmcEngine * pSmcEngine);
    RC RemoveSmcEngine(LwRm * pLwRm);
    RC AddLWGpuResource(LWGpuResource* lwgpu);
    LWGpuResources GetLWGpuResources();
    LWGpuResource* GetLWGpuResourceBySubdev(GpuSubdevice *pSubdev);
    UINT32 GetMaxSubCtxForSmcEngine(LwRm* pLwRm, GpuSubdevice *pSubdev);
    bool MultipleGpuPartitionsExist();
    PmSmcEngine* GetMatchingSmcEngine(UINT32 engineId, LwRm* pLwRm);
    bool IsSmcEngineAdded(PmSmcEngine* pSmcEngine);
    RC AddSurface(PmTest *pTest, MdiagSurf *pSurface2D, bool ExternalSurf2D,
        PmSurface **ppPmSurface = 0);
    RC AddSubsurface(PmTest *pTest, MdiagSurf *pSurface2D, UINT64 offset,
                     UINT64 size, const string &Name, const string &TypeName);
    RC AddAvailableCEEngineType(UINT32 ceEngineType, bool isGrCopy);
    RC StartTest(PmTest *pTest);
    RC EndTest(PmTest *pTest);
    RC AbortTest(PmTest *pTest);
    bool InTest(PmTest *pTest) const;
    bool InUnabortedTest(PmTest *pTest) const;
    bool InAnyTest() const { return !m_ActiveTests.empty(); }
    RC OnEvent();
    RC WaitForIdle();
    RC HandleRmEvent(GpuSubdevice *pSubdev, UINT32 EventId);
    RC SetEnableRobustChannel(bool val);
    bool GetEnableRobustChannel()  const { return m_RobustChannelEnabled; }
    RC SetEnableGpuReset(bool val);
    bool GetEnableGpuReset() const { return m_GpuResetEnabled; }
    void AddPreemptRlEntries(Runlist *pRunList,
                             const Runlist::Entries &entries);
    void SaveRemovedRlEntries(Runlist *pRunList,
                              const Runlist::Entries &entries);
    void RemovePreemptRlEntries(Runlist *pRunList);
    Runlist::Entries GetPreemptRlEntries(Runlist *pRunList) const;
    Runlist::Entries GetRemovedRlEntries(Runlist *pRunList) const;
    void SetCtxSwIntMode(bool isBlocking) { m_bBlockingCtxswInt = isBlocking; }
    bool GetCtxSwIntMode() { return m_bBlockingCtxswInt; };
    void SetRandomSeed(UINT32 seed);
    UINT32 GetRandomSeed() { return m_RandomSeed; }
    bool IsRandomSeedSet() { return m_bRandomSeedSet; }
    void SetStrictMode(bool enable) { m_StrictModeEnabled = enable; }
    bool GetStrictMode() const { return m_StrictModeEnabled; }
    void SetFaultBufferNeeded() { m_FaultBufferNeeded = true; }
    void SetNonReplayableFaultBufferNeeded() { m_NonReplayableFaultBufferNeeded = true; }
    void SetAccessCounterBufferNeeded() { m_AccessCounterBufferNeeded = true; }
    FLOAT64 GetAccessCounterNotificationWaitMs() const { return m_AccessCounterNotificationWaitMs; }
    void SetAccessCounterNotificationWaitMs(FLOAT64 val) { m_AccessCounterNotificationWaitMs = val; }
    void SetPoisonErrorNeeded() { m_PoisonErrorNeeded = true; }
    void AddErrorLoggerInterruptName(const std::string &name)
        { m_ErrorLoggerInterruptNames.insert(name); }
    void SetUpdateFaultBufferGetPointer(bool update)
        { m_UpdateFaultBufferGetPointer = update; }
    bool GetUpdateFaultBufferGetPointer() const
        { return m_UpdateFaultBufferGetPointer; }
    string ColwPm2GpuEngineName(const string &PmEngineName) const;
    RC ClearFaultBuffer(GpuSubdevice *gpuSubdevice);

    RC GetCEEngineTypeByIndex(UINT32 ceIndex, bool bAsyncCE, UINT32 *pCEEngineType);

    // Get sysmem base address per device/subdev
    // The final Get method would be
    // RC GetSysMemBaseAddrPerDev(const GpuDevice* pDev, UINT64* pAddrOut, const UINT32 subdevIdx) const;
    // and here's a temporary method for use ahead of readiness of caller's subdevice ID.
    // After switching back to above final one, a broken compiling will force caller code to get updated.
    RC GetSysMemBaseAddrPerDev(const GpuDevice* pDev, UINT64* pAddrOut) const;

    RC HandleChannelReset(LwRm::Handle hObject, LwRm* pLwRm);

    void SetChannelLogging(bool channelLogging) { m_ChannelLogging = channelLogging; }

    template<class T>
    void RegisterRegRestore(T&& func) { m_RegRestoreList.push_front(forward<T>(func)); }

    bool GetChannelLogging() const { return m_ChannelLogging; }

    void SetInbandCELaunchFlushEnable(bool flag) { m_EnableCEFlush = flag; }
    bool GetInbandCELaunchFlushENable() const { return m_EnableCEFlush; }

    // SRIOV support
    bool GetStartVfTestInPolicyManager() const { return m_StartVfTestInPolicyMgr;}
    void SetStartVfTestInPolicyManger(bool flag) { m_StartVfTestInPolicyMgr = flag; }
    
    // Hopper Mmu support
    bool HasUpdateSurfaceInMmu(const string & name) const; 
    void SetUpdateSurfaceInMmu(const string &name) { 
        RegExp regName;
        regName.Set(name);
        m_OwnedSurfacesMmu.push_back(regName); 
    }
    void SetUpdateSurfaceInMmu(const RegExp &name) { m_OwnedSurfacesMmu.push_back(name); }

    PmErrorLoggerInt* GetErrorLoggerIntMap(GpuSubdevice *pGpuSubdevice)
        { return m_ErrorLoggerIntMap[pGpuSubdevice]; }
    enum MovePolicy
    {
        DELETE_MEM,
        SCRAMBLE_MEM,
        DUMP_MEM,
        CRC_MEM
    };

    enum LoopBackPolicy
    {
        LOOPBACK,
        NO_LOOPBACK
    };

    enum PageSize
    {
        LWRRENT_PAGE_SIZE,
        ALL_PAGE_SIZES,
        BIG_PAGE_SIZE,
        SMALL_PAGE_SIZE,
        HUGE_PAGE_SIZE,
        PAGE_SIZE_512MB
    };

    enum Level
    {
        LEVEL_ALL,
        LEVEL_PTE,
        LEVEL_PDE0,
        LEVEL_PDE1,
        LEVEL_PDE2,
        LEVEL_PDE3,
        LEVEL_MAX
    };

    enum Wfi
    {
        WFI_UNDEFINED,
        WFI_SCOPE_LWRRENT_SCG_TYPE,
        WFI_SCOPE_ALL,
        WFI_POLL_HOST_SEMAPHORE
    };

    static const UINT32 BYTES_IN_SMALL_PAGE = 4096;
    static const UINT32 BYTES_IN_HUGE_PAGE  = 2*1024*1024;
    static const UINT32 BYTES_IN_512MB_PAGE = 512*1024*1024;

    static const FLOAT64 PollWaitTimeLimit;

    // Similar to Memory::Location, but with a few extra values.
    // Re-uses Memory::Location values where possible.
    //
    enum Aperture
    {
        APERTURE_FB = Memory::Fb,
        APERTURE_COHERENT = Memory::Coherent,
        APERTURE_NONCOHERENT = Memory::NonCoherent,
        APERTURE_PEER,
        APERTURE_ILWALID,
        APERTURE_ALL
    };

    enum TlbIlwalidateReplay
    {
        TLB_ILWALIDATE_REPLAY_NONE,
        TLB_ILWALIDATE_REPLAY_START,
        TLB_ILWALIDATE_REPLAY_CANCEL_TARGETED,
        TLB_ILWALIDATE_REPLAY_CANCEL_TARGETED_FAULTING,
        TLB_ILWALIDATE_REPLAY_CANCEL_GLOBAL,
        TLB_ILWALIDATE_REPLAY_CANCEL_VA_GLOBAL,
        TLB_ILWALIDATE_REPLAY_START_ACK_ALL
    };

    enum TlbIlwalidateAccessType
    {
        TLB_ILWALIDATE_ACCESS_TYPE_NONE,
        TLB_ILWALIDATE_ACCESS_TYPE_VIRT_READ,
        TLB_ILWALIDATE_ACCESS_TYPE_VIRT_WRITE,
        TLB_ILWALIDATE_ACCESS_TYPE_VIRT_ATOMIC_STRONG,
        TLB_ILWALIDATE_ACCESS_TYPE_VIRT_RSVRVD,
        TLB_ILWALIDATE_ACCESS_TYPE_VIRT_ATOMIC_WEAK,
        TLB_ILWALIDATE_ACCESS_TYPE_VIRT_ATOMIC_ALL,
        TLB_ILWALIDATE_ACCESS_TYPE_VIRT_WRITE_AND_ATOMIC,
        TLB_ILWALIDATE_ACCESS_TYPE_VIRT_ALL
    };

    enum TlbIlwalidateAckType
    {
        TLB_ILWALIDATE_ACK_TYPE_NONE,
        TLB_ILWALIDATE_ACK_TYPE_GLOBALLY,
        TLB_ILWALIDATE_ACK_TYPE_INTRANODE
    };

    enum class TlbIlwalidateIlwalScope
    {
        ALL_TLBS,
        LINK_TLBS,
        NON_LINK_TLBS
    };

    enum AddressSpaceType
    {
        GmmuVASpace
        ,GpuSmmuVASpace
        ,TegraSmmuVASpace
        ,DefaultVASpace
    };

    enum CpuMappingMode
    {
        CpuMapDefault
        ,CpuMapDirect
        ,CpuMapReflected
    };

    enum CEAllocMode
    {
        CEAllocAsync
        ,CEAllocSync
        ,CEAllocAll
        ,CEAllocDefault
    };
    //--------------------------------------------------------------------
    //! \brief RAII class for saving/restoring the policy manager mutex
    //!
    //! If the current thread owns the policy manager mutex, then an
    //! instantiation of this class will release the mutex and save the
    //! current lock count.When the object is destroyed, the mutex is
    //! restored to its original condition by re-acquiring the mutex
    //! the required number of times.
    //!
    class PmMutexSaver
    {
    public:
        PmMutexSaver(PolicyManager *pPolicyMan);
        ~PmMutexSaver();
    private:
        PolicyManager *m_pPolicyManager; //!< Pointer to the policy manager
        UINT32         m_LockCount;      //!< Lock count for the policy manager
                                         //!< mutex
    };

    //--------------------------------------------------------------------
    //! \brief PreAllocSurface class to pre-allocate a big surface to avoid
    //!        small internal surface in PolicyManager.
    //!
    //! Use case1:
    //!      Src surface of in-band mmu entries modification.
    class PreAllocSurface
    {
    public:
        PreAllocSurface(PolicyManager *pPolicyMn, LwRm* pLwRm);

        RC AllocSmallChunk(UINT64 size);
        UINT64 GetPhysAddress() const;
        UINT64 GetGpuVirtAddress() const;
        Memory::Location GetLocation() const;

        RC CreatePreAllocSurface();

        RC WritePreAllocSurface(const void * Buf, size_t BufSize,
                                UINT32 Subdev, Channel * pInbandChannel);

    private:
        static const UINT32 SIZE_LIMIT_IN_BYTES = 3 * 1024 * 1024;

        UINT64 m_PutOffset;    //!< latest available offset
        UINT64 m_LastChunkSize;//!< size of last small chunk allocation
        PmSurface *m_pPmSurface;
        PolicyManager *m_pPolicyManager;
        LwRm* m_pLwRm;
    };
    
    void SetInband() { m_IsInband = true; }
    bool GetInband() { return m_IsInband; }
    PreAllocSurface* GetPreAllocSurface(LwRm* pLwRm);
    void SavePteKind(PmMemRanges ranges);

    typedef std::set<std::string> InterruptNames;

private:
    PolicyManager();
    RC AddGpuDevice(GpuDevice *pGpuDevice);
    RC EndOrAbortTest(PmTest *pTest, bool Aborted);
    RC FreeTestData();
    RC CreateReplayableInts();
    RC CreateNonReplayableInts();
    RC CreateAccessCounterInts();
    RC CreateErrorLoggerInts(const InterruptNames *pNames);
    RC CreatePoisonErrorInts();
    // Init cache for sysmem base address per device/subdev
    void InitSysMemBaseAddrCache(const GpuDevice* pDev);

    static PolicyManager *m_Instance;  //!< The instance of this singleton
    PmParser       *m_pParser;      //!< The parser
    PmEventManager *m_pEventMgr;    //!< The event manager
    PmGilder       *m_pGilder;      //!< The gilder

    PmTests     m_ActiveTests;      //!< Conlwrrent tests that are running
    PmTests     m_InactiveTests;    //!< Conlwrrent tests that are not running
    PmChannels  m_ActiveChannels;   //!< Channels used by running tests
    PmChannels  m_InactiveChannels; //!< Channels for inactive tests
    PmVfTests       m_ActiveVfs;   //!< Vfs used by running tests
    PmVfTests       m_InactiveVfs; //!< Vfs for inactive tests
    PmSmcEngines    m_ActiveSmcEngines;     //!< SmcEngines used by running tests
    PmSmcEngines    m_InactiveSmcEngines;   //!< SmcEngines for inactive tests
    PmVaSpaces  m_ActiveVaSpaces;   //!< VaSpacs used by running tests
    PmVaSpaces  m_InactiveVaSpaces; //!< VaSpaces for inactive tests
    PmSurfaces  m_ActiveSurfaces;   //!< Surfaces used by running tests
    PmSurfaces  m_InactiveSurfaces; //!< Surfaces for inactive tests

    GpuDevices m_GpuDevices;           //!< All devices under test; extracted
                                       //!< from m_MemSpaces.
    UINT32     m_TestNum;              //!< Current test being run. Starts at 0
    bool       m_RobustChannelEnabled; //!< set by EnableRobustChannel()
    bool       m_GpuResetEnabled;      //!< set by EnableGpuReset()
    bool       m_bBlockingCtxswInt;    //!< Blocking mode for ctxsw interrupts
    void      *m_Mutex;                //!< Mutex for accessing the policy manager

    //!< Mutex for accessing active and inactive test lists
    void      *m_TestMutex;

    UINT32     m_DisableMutexInActions;//!< Disable m_Mutex in actions
    map<string, UINT64> m_TimerStartUS;//!< Time the timer(s) started
    map<Runlist*, Runlist::Entries>
                   m_PreemptRlEntries; //!< Preempted Rl<->Entries map
    map<Runlist*, Runlist::Entries>
                   m_RemovedEntries;   //!< Save removed entries for preempted rl

    UINT32 m_RandomSeed;               //!< Random seed for policy manager,
                                       //!< overrides policy manager settings
    bool   m_bRandomSeedSet;           //!< true if the random seed has been set

    //! true if strict mode is enabled, this causes errors for things that would
    //! otherwise be silently ignored (e.g. triggers that will never fire due to
    //! the GPU where the policy file is being run)
    bool   m_StrictModeEnabled;

    //! Set to true if access to a fault buffer is needed.
    bool m_FaultBufferNeeded;

    //! Set to true if access to a non-replaybale fault buffer is needed.
    bool m_NonReplayableFaultBufferNeeded;

    //! Set to true if access to a access counter buffer is needed.
    bool m_AccessCounterBufferNeeded;

    //! Set timeout value for VALID bit check and turn to check Nack bit
    FLOAT64 m_AccessCounterNotificationWaitMs;

    //! Set to true if poison error is needed
    bool m_PoisonErrorNeeded;

    //! Set if logged interrupt events are needed.
    InterruptNames m_ErrorLoggerInterruptNames;

    //! A map of pointers to fault buffer info (one per GPU subdevice).
    map<GpuSubdevice*,PmReplayableInt*> m_ReplayableIntMap;

    //! A map of pointers to non-replaybale buffer info (one per GPU subdevice).
    map<GpuSubdevice*,PmNonReplayableInt*> m_NonReplayableIntMap;

    //! A map of pointers to access counter buffer info (one per GPU subdevice).
    map<GpuSubdevice*,PmAccessCounterInt*> m_AccessCounterIntMap;

    //! List to store lambdas for each register restore
    void RestoreRegisters();
    list<function<void()>> m_RegRestoreList;

    //! A map of pointers to error logger interrupts (one per GPU subdevice).
    map<GpuSubdevice*,PmErrorLoggerInt*> m_ErrorLoggerIntMap;

    //! True if the fault buffer get pointer should be updated when a
    //! fault buffer entry is processed (defaults to true).
    bool m_UpdateFaultBufferGetPointer;

    //! True if -channel_logging is enabled
    bool m_ChannelLogging;

    //! True if Policy.SetInbandCELaunchFlushEnable(true) and all CE will append the flush, default: true
    bool m_EnableCEFlush;

    

    struct CEEngine
    {
        UINT32 CEEngineType;
        bool   IsGrCopy;

        CEEngine(UINT32 engineType, bool isGrCopy) :
            CEEngineType(engineType),
            IsGrCopy(isGrCopy)
        {
        }
    };
    vector<CEEngine> m_CEEngines;

    map<LwRm*, unique_ptr<PreAllocSurface>> m_pPreAllocSurface;
    bool m_IsInband; //!< if the pcy use Policy.SetInband, the prealloc surface need to be created

    RC CreatePhysicalPageFaultInts();

    // Fixing bug#1699998 GP100 Bringup : lwlink functional testing on the Garrison system.
    //   - Comment 30: "MASSERT(dstSize >= Size) failed" problem.
    // The physical addr got from PTE need to be added sysmem base addr.
    // So here we cache sysmem base addr of every device/subdev for use
    // Get per device cached sysmem base addr
    typedef vector<UINT64> SysMemBaseAddrArray;
    typedef map<const GpuDevice*, SysMemBaseAddrArray > SysMemBaseAddrMap;
    SysMemBaseAddrMap m_SysMemBaseAddrCache;
    LWGpuResources m_LwGpuResourceSet;

public:
    struct EngineInfo
    {
        UINT32 startMMUEngineId;
        UINT32 maxSubCtx;
        bool   bSubCtxAware;
    };

    UINT32 VEIDToMMUEngineId(UINT32 VEID, GpuSubdevice * pGpuSubDev, LwRm* pLwRm, UINT32 engineId);
    UINT32 MMUEngineIdToVEID(UINT32 mmuEngineId, GpuSubdevice * pGpuSubDev, LwRm* pLwRm, UINT32 engineId);

private:
    EngineInfo * GetEngineInfo(UINT32 subdevIdx, LwRm* pLwRm, UINT32 engineId);
    RC CreateEngineInfo(GpuSubdevice * pGpuSubDev, LwRm* pLwRm, UINT32 engineId);

    map<tuple<UINT32, LwRm*, UINT32>, unique_ptr<EngineInfo>> m_EngineInfos;
    bool m_StartVfTestInPolicyMgr;
    vector<RegExp> m_OwnedSurfacesMmu;
    vector<pair<PmMemRanges, UINT32>> m_SavedPteKinds;
    
    map<GpuDevice*, LWGpuResource*>  m_Dev2Res;
    Mutexes m_MutexLists;     //!< mutex between UTL and 
                              //!< policy manager
};

class PolicyManagerMessageDebugger
{
public:
    static PolicyManagerMessageDebugger * Instance();
    RC Init(const ArgReader * reader);
    void Release();
    void AddTeeModule(const string & actionBlockName,
                      const string & actionName);
    UINT32 GetTeeModuleCode(const string & actionBlockName,
                            const string & actionName);

private:
    PolicyManagerMessageDebugger() {};
    ~PolicyManagerMessageDebugger();
    void ParseEnDisMessages(const ArgReader * reader,
                            const UINT32 count,
                            const string & keyword,
                            bool enable);
    void EnDisMessages(const string & ModuleNames, bool enable);
    void UpdateName(string * name);
    bool RealAddTeeModule(const string & name);

    map<string, TeeModule *> m_TeeModule;

    static PolicyManagerMessageDebugger * m_Instance;
        
};
#endif // INCLUDED_POLICYMN_H

