/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_LWGPU_H
#define INCLUDED_LWGPU_H

#include "mdiag/utils/randstrm.h"
#include "sysspec.h"
#include "mdiag/utils/surfutil.h"

#include "mdiag/utils/IGpuSurfaceMgr.h"

#include "IRegisterMap.h"
#include "core/include/refparse.h"

#ifndef INCLUDED_GPUDEV_H
#include "gpu/include/gpudev.h"
#endif

class GpuSubdevice;
class Trace3DTest;
class SmcEngine;
class SmcResourceController;
class GpuPartition;
class SharedSurfaceController;
class SmcRegHal;
class TestDirectory;

namespace DispPipeConfig
{
    class DispPipeConfigurator;
}

#include "vaspace.h"
#include "subctx.h"
#include "t3dresourcecontainer.h"
#include "zbctable.h"
#include <iterator>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <boost/smart_ptr.hpp>

extern const char *AAstrings[];

class PerformanceMonitor;
class CRegisterControl;
class Test;

// why doesn't this get generated somewhere?
typedef volatile struct
{
    struct {                     /*                                   0000-    */
        UINT32 nanoseconds[2];   /* nanoseconds since Jan. 1, 1970       0-   7*/
    } timeStamp;                 /*                                       -0007*/
    UINT32 info32;               /* info returned depends on method   0008-000b*/
    UINT16 info16;               /* info returned depends on method   000c-000d*/
    UINT16 status;               /* user sets bit 15, LW sets status  000e-000f*/
} LwNotificationStruct;

// define some new fifo access commands to be used
#ifndef BIT
#define BIT(b)                  (1U<<(b))
#endif

typedef int (*LwGpuErrorHandler)(class LWGpuResource *, void *);

struct RegName
{
    enum RegAddressSpace
    {
        Unknow = 0,
        Conf,
        Priv
    };

    string RegNameStr;
    RegAddressSpace RegAddrSpace;
    bool operator < (const RegName & T1) const
    {
        return RegNameStr < T1.RegNameStr;
    }

    RegName() {RegAddrSpace = Priv;}
};

struct RegRange
{
    enum RegAddressSpace
    {
        Unknow = 0,
        Conf,
        Priv
    };

    pair<UINT32, UINT32> RegAddrRange;
    RegAddressSpace RegAddrSpace;
    bool operator < (const RegRange & T1) const
    {
        return RegAddrRange < T1.RegAddrRange;
    }

    RegRange() {RegAddrSpace = Priv;}
};

class LWGpuSubChannel;
unique_ptr<IRegisterMap> CreateRegisterMap(const RefManual *Manual);
class LWGpuTsg;
class LWGpuContexScheduler;

//
// This is a class embedded in LWGpuReource to schedule Trace3D/V2D instances
//    Context: it's a SW concept here. One context can include:
//      1. One Trace3D instance thread
//         OR
//      2. Multiple Trace3D instances merged by cmdline (-use_channel)
//
//    Operations:
//      1. SwitchToNextCtxThread()
//          Stop exelwting the current thread;
//          Select the active thread in NEXT context to run (contextswith)
//      2. SwitchToNextSubchThread()
//          Stop exelwting the current thread;
//          Select the next thread in SAME context to run (subch switch)
class LWGpuContexScheduler
{
public:

    LWGpuContexScheduler();

    RC RegisterTestToContext(const string& name, const Test *test);
    RC UnRegisterTestFromContext();
    RC BindThreadToTest(const Test *test);

    RC AcquireExelwtion();
    RC ReleaseExelwtion();
    RC YieldToNextCtxThread();
    RC YieldToNextSubchThread();

    typedef shared_ptr<LWGpuContexScheduler> Pointer;

    class SyncPoint
    {
    public:
        SyncPoint();
        SyncPoint(const string& point);
        bool operator!=(const SyncPoint& otherPoint) const;
        string ToString() const;
    private:
        string m_PointName;
    };

    RC SyncAllRegisteredCtx(const LWGpuContexScheduler::SyncPoint& syncPoint);

private:

    //<!  Not allow copy&&assignment operation
    LWGpuContexScheduler & operator= (const LWGpuContexScheduler & other);
    LWGpuContexScheduler(const LWGpuContexScheduler& other);

    enum TestRunningState
    {
        Registered = 0,
        UnRegistered,
        Waiting,
        Running
    };

    bool IsTestInSharedContext(const string& ctxName) const { return !ctxName.empty(); }

    UINT32 GetLwrrentTestId() const;
    RC WaitWakenUp();
    RC GetLwrrentCtx(size_t* pIndex) const;
    bool IsAllRegisteredTestsSynced() const;
    bool IsRegisteredTest(UINT32 testId) const;

    struct TraceContext
    {
        TraceContext(const string& name, UINT32 testId);

        string m_ContextName; //<! context name. Empty if it's unnamed.
        UINT32 m_CtxActiveTestId; //<! id of active test in the context

        vector<UINT32> m_TestIds; //<! registered test IDs
    };
    vector<TraceContext> m_TraceContexts; //<! registered contexts

    struct TestRunningInfo
    {
        TestRunningInfo() : m_TestState(Registered), m_ThreadId(Tasker::NULL_THREAD) {}
        TestRunningState m_TestState;
        Tasker::ThreadID m_ThreadId;
    };
    map<UINT32, TestRunningInfo> m_TestStates; //<! Database to record info of all tests,
                                               //<! TestId is the key
    set<UINT32> m_TestsReachedSyncPoint;

    TraceContext* m_GlobalActiveContext; //<! Lwrrrent active trace context
                                         //<! NULL means it's not cared who is the active one

    SyncPoint m_SyncPoint; //<! Lwrrrent sync point
};

#define SYNCPOINT(str) LWGpuContexScheduler::SyncPoint(string(__FUNCTION__) + " : " + str)
#define UNSPECIFIED_SMC_ENGINE "unspecified_smc_engine"

class LWGpuResource
{
public:
    LWGpuResource(ArgDatabase *args, GpuDevice *pGpuDevice);
    ~LWGpuResource();

    static const ParamDecl ParamList[];

    static const UINT32 TEST_ID_GLOBAL = ~0x0;

    static const string FLA_VA_SPACE;

    static int ScanSystem(ArgDatabase *args);

    // Return a GPU that supports the classes passed
    static LWGpuResource* GetGpuByClassSupported(UINT32 numClasses,
                                                 const UINT32 classIds[]);

    // Return the passed device instance GPU
    static LWGpuResource* GetGpuByDeviceInstance(UINT32 devInst);

    static TestDirectory *s_pTestDirectory;
    static vector<LWGpuResource*> s_LWGpuResources;

    static TestDirectory *GetTestDirectory() { return s_pTestDirectory; }
    static void SetTestDirectory(TestDirectory *pTestDirectory);
    static void DescribeResourceParams(const char *name);
    static RC InitSmcResource();
    static void PrintSmcInfo();
    static RC RegisterOperationsResources();
    static LWGpuResource *FindFirstResource();
    static void FreeResources();
    static vector<LWGpuResource*> GetAllResources() { return s_LWGpuResources; }

    // gets a new channel (returns NULL if no channels are free) - delete the
    //  channel when you're done
    LWGpuChannel *CreateChannel(LwRm* pLwRm = nullptr, SmcEngine* pSmcEngine = nullptr);

    // Method to create a spearate channel for DMA loading/reading for trace buffers.
    // Ilwoked when -load_new_channel/-crc_new_channel option is specified or GrCE is not available
    LWGpuChannel *GetDmaChannel(const shared_ptr<VaSpace> &pVaSpace, const ArgReader *pParams,
                                LwRm* pLwRm, SmcEngine* pSmcEngine, UINT32 engineId);
    UINT32 GetDmaObjHandle( const shared_ptr<VaSpace> &pVaSpace,
                            UINT32 Class, LwRm* pLwRm, UINT32 *retClass = NULL);

    // PerfMon related functions
    bool CreatePerformanceMonitor();
    bool PerfMon() const;
    int  PerfMonInitialize(const char *option_type, const char *name);
    void PerfMonStart(LWGpuChannel* ch, UINT32 subch, UINT32 class3d);
    void PerfMonEnd(LWGpuChannel* ch, UINT32 subch, UINT32 class3d);

    RC AllocSurfaceRC(Memory::Protect Protect,
                         UINT64 Bytes,
                         _DMA_TARGET Target,
                         _PAGE_LAYOUT Layout,
                         LwRm::Handle hVASpace,
                         UINT32 *Handle,
                         uintptr_t *Buf,
                         LwRm* pLwRm,
                         UINT32 Attr = 0,
                         UINT64 *GpuOffset = 0,
                         UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);

    void DeleteSurface(uintptr_t address);

    Surface2D* GetSurface(uintptr_t address);

    RC GetGrCopyEngineType(vector<UINT32>& engineType, LwRm* pLwRm);
    RC GetGrCopyEngineType(UINT32 * pRtnEngineType, UINT32 runqueue, LwRm* pLwRm);
    RC GetSupportedCeNum(GpuSubdevice *subdev, vector<UINT32> *supportedCE, LwRm* pLwRm);

    //@{
    //! All Interrupt hooks are deprecated.  Please do not add any additional
    //! code that uses them.  Please do not add any more functions that install
    //! callbacks with the Resource Manager, that approach does not work on
    //! real operating systems where the RM lives in the kernel
    // handle graphics device errors, if possible (1 on success, 0 on failure)
    static bool MdiagGrIntrHook(UINT32 GrIdx, UINT32 GrIntr, UINT32 ClassError,
                                UINT32 Addr, UINT32 DataLo,
                                void *CallbackData);
    //@}

    bool IsHandleIntrTest() { return m_HandleIntr; }
    bool SkipIntrCheck();

    UINT32 GetArchitecture(UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV) const;

    const char *GetArchString(UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV) const { return "unknown"; }

    IGpuSurfaceMgr* CreateGpuSurfaceMgr(Trace3DTest* pTest, const ArgReader *params, LWGpuSubChannel* in_sch);

    DispPipeConfig::DispPipeConfigurator* GetDispPipeConfigurator()
    {
        return m_pDispPipeConfigurator.get();
    }
    void DispPipeConfiguratorSetScanout(bool* pAborted);
    RC DispPipeConfiguratorRunTest
    (
        bool* pAborted, 
        MdiagSurf* pLumaSurface, 
        MdiagSurf* pChromaSurface, 
        bool bSyncScanoutEnd
    );
    RC DispPipeConfiguratorCleanup();

    bool GetChannelLogging() const { return m_ChannelLogging; }

    UINT64 GetFBMemSize(UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);

    unique_ptr<IRegisterClass> GetRegister(const char* name,
                                UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV,
                                UINT32 *pRegIdxI = NULL,
                                UINT32 *pRegIdxJ = NULL);
    unique_ptr<IRegisterClass> GetRegister(UINT32 address,
                                UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);
    IRegisterMap* GetRegisterMap(UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);

    RC ReinitializeRegSettings(); // called in between each test's Run()

    enum EREG_CONTROLS
    {
        RC_OPTIONAL       = 0,
        RC_PRODUCTION     = 1
    };

    RC SetContextOverride(UINT32 regAddr,
                          UINT32 andMask,
                          UINT32 orMask,
                          SmcEngine *pSmcEngine);

    RC HandleRegisterAction(CRegisterControl *regc,
                            UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV,
                            SmcEngine* pSmcEngine = nullptr);

    RC HandleOptionalRegisters(vector<CRegisterControl> *regvec,
                               UINT32 subdevInst = Gpu::UNSPECIFIED_SUBDEV,
                               SmcEngine* pSmcEngine = nullptr);

    RC ProcessOptionalRegisters(vector<CRegisterControl> *regvec, const char *regs);

    static UINT32 GetMemSpaceCtxDma(_DMA_TARGET Target,
                                    LwRm *pLwRm,
                                    GpuDevice  *pGpuDevice);

    const ArgReader* GetLwgpuArgs() const { return params.get(); }
    void GetBuffInfo(BuffInfo* inf);

    // these three generic functions are used to set gpu specific registers
    // if field is null, set the full register, it is invalid
    int SetRegFldDef(const char *regName, 
                     const char *regFieldName, 
                     const char *regFieldDef, 
                     UINT32 *myvalue = 0,
                     const char *regSpace = nullptr,
                     UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV,
                     SmcEngine *pSmcEngine = nullptr,
                     bool overrideCtx = false);
    // if field is null, set the full register
    int SetRegFldNum(const char *regName, 
                     const char *regFieldName, 
                     UINT32 value, 
                     UINT32 *myvalue = 0,
                     const char *regSpace = nullptr,
                     UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV,
                     SmcEngine *pSmcEngine = nullptr,
                     bool overrideCtx = false);

    // if field is null, Get the full register
    int GetRegFldNum(const char *regName, 
                     const char *regFieldName, 
                     UINT32 *value, 
                     UINT32 *myvar = 0,
                     const char *regSpace = nullptr,
                     UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV,
                     SmcEngine *pSmcEngine = nullptr);

    // Get Register number
    int GetRegNum(const char *regName, 
                  UINT32 *Value, 
                  const char *regSpace = nullptr,
                  UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV,
                  LwRm *pLwRm = nullptr,
                  bool ignoreDomainBase = false);

    // Get Register field definition value by name, return 0 if success
    int GetRegFldDef(const char *regName, 
                     const char *regFieldName, 
                     const char *regFieldDef,
                     UINT32 *myvalue,
                     UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);

    // Test Register field by field value def
    bool TestRegFldDef(const char *regName, 
                       const char *regFieldName, 
                       const char *regFieldDef, 
                       UINT32 *myvalue = 0,
                       const char *regSpace = nullptr,
                       UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV,
                       SmcEngine *pSmcEngine = nullptr);

    // Test Register field by field value num
    bool TestRegFldNum(const char *regName, 
                       const char *regFieldName, 
                       UINT32 regFieldValue, 
                       UINT32 *myvalue = 0,
                       const char *regSpace = nullptr,
                       UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV,
                       SmcEngine *pSmcEngine = nullptr);

    RC GetRegFieldBitPositions
    (
        string regName,
        string regFieldName,
        UINT32* startBit,
        UINT32* endBit,
        UINT32 subdev = Gpu::UNSPECIFIED_DEV
    );

    // Poll Register value
    static bool PollRegValueFunc(void *pArgs);
    RC PollRegValue(UINT32 Address, UINT32 Value, UINT32 Mask, FLOAT64 TimeoutMs, UINT32 subdev, SmcEngine* pSmcEngine) const;


    // register space accesses
    UINT08 RegRd08(UINT32 offset, SmcEngine* pSmcEngine = nullptr) const
    {
        return RegRd08(offset, Gpu::UNSPECIFIED_SUBDEV, pSmcEngine);
    }
    UINT16 RegRd16(UINT32 offset, SmcEngine* pSmcEngine = nullptr) const
    {
        return RegRd16(offset, Gpu::UNSPECIFIED_SUBDEV, pSmcEngine);
    }

    UINT32 RegRd32(UINT32 offset, SmcEngine* pSmcEngine = nullptr) const
    {
        return RegRd32(offset, Gpu::UNSPECIFIED_SUBDEV, pSmcEngine);
    }

    // pSmcEngine specifies the SMC engine on which PGRAPH registers will be accessed.
    // For non PGRAPH registers or legacy mode. pSmcEngine can be ignored.
    // In SMC mode, if pSmcEngine is null, legacy PGRAPH space will be accessed.

    UINT08 RegRd08(UINT32 offset, UINT32 subdev, SmcEngine* pSmcEngine = nullptr) const;
    UINT16 RegRd16(UINT32 offset, UINT32 subdev, SmcEngine* pSmcEngine = nullptr) const;
    UINT32 RegRd32(UINT32 offset, UINT32 subdev, SmcEngine* pSmcEngine = nullptr) const;

    void RegWr08(UINT32 offset, UINT08 data, SmcEngine* pSmcEngine = nullptr)
    {
        RegWr08(offset, data, Gpu::UNSPECIFIED_SUBDEV, pSmcEngine);
    }
    void RegWr16(UINT32 offset, UINT16 data, SmcEngine* pSmcEngine = nullptr)
    {
        RegWr16(offset, data, Gpu::UNSPECIFIED_SUBDEV, pSmcEngine);
    }
    void RegWr32(UINT32 offset, UINT32 data, SmcEngine* pSmcEngine = nullptr)
    {
        RegWr32(offset, data, Gpu::UNSPECIFIED_SUBDEV, pSmcEngine);
    }

    // pSmcEngine specifies the SMC engine on which PGRAPH registers will be accessed.
    // For non PGRAPH registers or legacy mode. pSmcEngine can be ignored.
    // In SMC mode, if pSmcEngine is null, legacy PGRAPH space will be accessed.

    void RegWr08(UINT32 offset, UINT08 data, UINT32 subdev, SmcEngine* pSmcEngine = nullptr);
    void RegWr16(UINT32 offset, UINT16 data, UINT32 subdev, SmcEngine* pSmcEngine = nullptr);
    void RegWr32(UINT32 offset, UINT32 data, UINT32 subdev, SmcEngine* pSmcEngine= nullptr);


    // Access register by RegHal
    void Write32(ModsGpuRegValue value, UINT32 index, UINT32 subdev, SmcEngine* pSmcEngine = nullptr);

    PHYSADDR PhysFbBase() const;

    //! Get the GpuDevice for channel/object/FB alloc or Ctrl calls.
    GpuDevice * GetGpuDevice() const;

    //! Get the only GpuSubdevice for register/clock/FB stuff.
    //! For use by SLI-unaware code, asserts if NumSubdevices > 1.
    GpuSubdevice* GetGpuSubdevice() const;

    //! Get one of the GpuSubdevices for register/clock/FB stuff.
    //! Asserts if subInst >= NumSubdevices.
    GpuSubdevice* GetGpuSubdevice(UINT32 subInst) const;

    RC RegisterOperations();

    void* GetCrcChannelMutex() const { return m_CrcChannelMutex; }

    // functions to add/remove LWGpuTsg pointer crossing tests
    LWGpuTsg* GetSharedGpuTsg(string tsgname, SmcEngine *pSmcEngine);
    void AddSharedGpuTsg(LWGpuTsg* pGpuTsg, SmcEngine *pSmcEngine);
    void RemoveSharedGpuTsg(LWGpuTsg* pGpuTsg, SmcEngine *pSmcEngine);

    UINT32 GetRandChIdFromPool(pair<LwRm*, UINT32> lwRmEngineIdPair);
    void RemoveChIdFromPool(pair<LwRm*, UINT32> lwRmEngineIdPair, UINT32 chId);
    void AddChIdToPool(pair<LwRm*, UINT32> lwRmEngineIdPair, UINT32 chId);

    RandomStream* GetLocationRandomizer();

    RC AddSharedSurface(const char* name, MdiagSurf* surf);
    RC RemoveSharedSurface(const char* name);
    MdiagSurf* GetSharedSurface(const char* name);
    SharedSurfaceController * GetSharedSurfaceController() const {
        return m_pSharedSurfaceController;
    }
    RC AddActiveT3DTest(Trace3DTest *pTest);
    RC RemoveActiveT3DTest(Trace3DTest *pTest);
    set<Trace3DTest*> GetActiveT3DTests() const { return m_ActiveT3DTests; }

    void* GetFECSMailboxMutex() const { return m_FECSMailboxMutex; }

    inline UINT32 GetCfgBase() const { return m_cfg_base; }

    inline UINT08 CfgRd08(UINT32 offset) const { return Sys::CfgRd08(m_cfg_base + offset); }
    inline UINT16 CfgRd16(UINT32 offset) const { return Sys::CfgRd16(m_cfg_base + offset); }
    inline UINT32 CfgRd32(UINT32 offset) const { return Sys::CfgRd32(m_cfg_base + offset); }
    inline void CfgWr08(UINT32 offset, UINT08 data) { Sys::CfgWr08(m_cfg_base + offset, data); }
    inline void CfgWr16(UINT32 offset, UINT16 data) { Sys::CfgWr16(m_cfg_base + offset, data); }
    inline void CfgWr32(UINT32 offset, UINT32 data) { Sys::CfgWr32(m_cfg_base + offset, data); }

    // <clientId, VaspaceManager>
    typedef Trace3DResourceContainer<VaSpace> VaSpaceManager;
    typedef Trace3DResourceContainer<SubCtx> SubCtxManager;
    typedef map<LwRm*, VaSpaceManager * > VaSpaceManagers;
    typedef map<LwRm*, SubCtxManager * > SubCtxManagers;

    VaSpaceManager * GetVaSpaceManager(LwRm* pLwRm);
    SubCtxManager * GetSubCtxManager(LwRm* pLwRm);

    LWGpuContexScheduler::Pointer GetContextScheduler() const { return m_ContexScheduler; }

    bool IsMultiVasSupported() const { return m_IsMultiVasSupported; }

    string GetAddressSpaceName(LwRm::Handle vaSpaceHandle, LwRm* pLwRm);
    UINT32 GetAddressSpacePasid(LwRm::Handle vaSpaceHandle, LwRm* pLwRm);

    bool IsSyncTracesMethodsEnabled() const { return m_SyncTracesMethods; }

    bool HasGlobalZbcArguments();
    ZbcTable *GetZbcTable(LwRm* pLwRm);
    void ClearZbcTables(LwRm* pLwRm);

    SmcResourceController * GetSmcResourceController() const { return m_pSmcResourceController; }

    RC AllocRmClients();
    SmcEngine* GetSmcEngine(const string& smcEngineLabel);
    void SmcSanityCheck() const;

    bool IsSMCEnabled() const;
    bool IsPgraphReg(UINT32 offset) const;
    bool IsPerRunlistReg(UINT32 offset) const;
    bool IsCtxReg(UINT32 offset) const;

    RC ParseRegBlock(const ArgReader *params, const char* usage, const char* block, multimap<string, tuple<ArgDatabase*, ArgReader*>>* argMap, const ParamDecl *paramList);
    RC ParseRegBlock(const ArgReader *params, const char* usage, const char* block, unique_ptr<ArgDatabase>* argDB, unique_ptr<ArgReader>* argReader, const ParamDecl *paramList);
    string  ParseSingleRegBlock(const string &cmd, const char* usage, const char* block, ArgDatabase* argDB, ArgReader* argReader);
    void ParseHandleOptionalRegisters(const ArgReader *params, GpuSubdevice * pSubdev);
    RC ProcessRegBlock(const ArgReader *params, const char* paramName, vector<CRegisterControl>& regVec);

    vector<CRegisterControl>& GetPrivRegs() { return m_privRegControls; }

    LwRm* GetLwRmPtr(GpuPartition* pGpuPartition) const;
    LwRm* GetLwRmPtr(UINT32 swizzid);
    LwRm* GetLwRmPtr(SmcEngine* pSmcEngine) const;
    LwRm::Handle GetGpuPartitionHandle(GpuPartition* pGpuPartition) const;
    RegHal& GetRegHal(GpuSubdevice* pSubdev, LwRm* pLwRm, SmcEngine* pSmcEngine);
    RegHal& GetRegHal(UINT32 gpuSubdevIdx, LwRm* pLwRm, SmcEngine* pSmcEngine);
    UINT32 GetMaxSubCtxForSmcEngine(LwRm* pLwRm);
    bool MultipleGpuPartitionsExist();

    static set<LWGpuResource*> GetFlaExportGpus();
    static set<MdiagSurf*> GetFlaExportSurfaces();
    static MdiagSurf* GetFlaExportSurface(const string &name);
    RC FreeRmClient(GpuPartition * pGpuPartition);
    RC FreeRmClient(SmcEngine* pSmcEngine);
    RC FreeRmClients();
    RC AllocRmClient(GpuPartition* pGpuPartition);
    RC AllocRmClient(SmcEngine* pSmcEngine);
    void AllocSmcRegHal(SmcEngine* pSmcEngine);
    void AllocSmcRegHals();
    void FreeSmcRegHal(SmcEngine* pSmcEngine);
    void FreeSmcRegHals();
    void* GetSmcRegHals(vector<SmcRegHal*> *pSmcRegHals);
    UINT32 GetSwizzId(LwRm* pLwRm);
    string GetSmcEngineName(LwRm* pLwRm);
    bool IsSmcMemApi();

    UINT32 GetGpcCount(UINT32 subdev) { return std::get<FsInfo::GpcCount>(m_FsInfo[subdev]); }
    UINT32 GetGpcMask(UINT32 subdev) { return std::get<FsInfo::GpcMask>(m_FsInfo[subdev]); }
    UINT32 GetTpcCount(UINT32 subdev) { return std::get<FsInfo::TpcCount>(m_FsInfo[subdev]); }
    UINT32 GetTpcMask(UINT32 subdev) { return std::get<FsInfo::TpcMask>(m_FsInfo[subdev]); }
    UINT32 GetSmPerTpcCount(UINT32 subdev) { return std::get<FsInfo::SmPerTpc>(m_FsInfo[subdev]); }
    
    bool UsePcieConfigCycles() const 
    { 
        // Ada in cheetah uses PCIE Gen 5, and ig000 is ada now so included(Bug 3391344)
        return GetGpuDevice()->GetFamily() >= GpuDevice::Hopper ||
            GetGpuSubdevice()->DeviceIdString() == "AD10B" ||
            GetGpuSubdevice()->DeviceIdString() == "IG000" ;
    }

    SmcEngine* GetSmcEngine(LwRm* pLwRm);

private:
    unique_ptr<ArgReader> params;
    
    unique_ptr<IRegisterMap> m_RegMap;
    unique_ptr<PerformanceMonitor> m_PerfMonList[LW2080_MAX_SUBDEVICES];
    GpuDevice   *m_pGpuDevice;

    // Floorsweeping info.
    // For each subdevice, save GpcCount, GpcMask, TpcCount and TpcMask
    enum FsInfo
    {
        GpcCount = 0,
        GpcMask,
        TpcCount,
        TpcMask,
        SmPerTpc
    };

    tuple<UINT32, UINT32, UINT32, UINT32, UINT32> m_FsInfo[LW2080_MAX_SUBDEVICES];

    bool  m_ChannelLogging;

    multimap<string, tuple<ArgDatabase*, ArgReader*>> m_privArgMap;
    unique_ptr<ArgDatabase> m_confArgDB;
    unique_ptr<ArgReader>   m_confParams;

    LwGpuErrorHandler           m_registered_error_handler;
    void                        *m_registered_user_data;

    bool m_HandleIntr;

    typedef set<RegName> RegGroups;
    typedef set<RegRange> RegRanges;
    typedef list<MdiagSurf> Buffers;

    RegGroups m_DumpGroups;
    RegGroups m_SkipGroups;
    RegRanges m_DumpRanges;
    RegRanges m_SkipRanges;
    RegRanges m_DumpRangesNoCheck;

    Buffers m_Buffers;

    typedef vector<pair<UINT32, pair<UINT32, UINT32> > > CtxOverrides;
    CtxOverrides m_CtxOverrides;
    bool      m_RestorePrivReg;

    void* m_CrcChannelMutex;
    set<UINT32> m_UsedZValues;
    set<UINT32> m_UsedStencilValues;

    // For dma loading channel
    map<pair<LwRm*, LwRm::Handle>, LWGpuChannel*> m_DmaChannels;
    map<pair<LwRm*, LwRm::Handle>, UINT32> m_DmaObjHandles;

    // retain all the surfaces created by calls to
    // AllocSurfaceRC, for later deletion.
    map<uintptr_t, Surface2D*> m_AllocSurfaces;

    // cache the chip gr copy engine type
    UINT32 m_grCopyEngineType;

    // cache supported copy engine list
    map<SmcEngine*, vector<UINT32>> m_SupportedCopyEngines;


    // Private member functions
    RC HandleJsonDisplayConfig(ArgReader *params);
    RC HandleFlaConfig(LwRm *pLwRm);
    void DumpRegisters(UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV) const;
    bool MatchGroupName(const RegGroups&, const RefManualRegister*, RegName*) const;
    bool MatchRange(const RegRanges&, const RefManualRegister*, RegRange*) const;
    bool MatchRange(const RegRanges& ranges, UINT32 offset) const;
    void ProcessPriFile(const char* filename, bool dump);
    void AddCtxOverride(UINT32 addr, UINT32 andMask, UINT32 orMask);
    RC AllocBuffer(UINT32 size, _DMA_TARGET Target, const ArgReader *params,
                   LwRm* pLwRm, UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);
    void FreeDmaChannels(LwRm* pLwRm);
    RC CheckMemoryMinima();
    RC CheckDeviceRequirements();
    // Check Reg control handling result.
    // pOption may be "-reg" or "-reg_eos", etc.
    void CheckRegCtrlResult(const StickyRC& rc, const char *pOption);

    // Set the initial value for SM OOR address check mode.
    void InitSmOorAddrCheckMode();

    // Used by register calls for SMC
    UINT32 PgraphRegRd(UINT32 offset, UINT32 subdev, SmcEngine *pSmcEngine) const;
    void PgraphRegWr(UINT32 offset, UINT32 value, UINT32 subdev, SmcEngine *pSmcEngine);

    RC CreateFlaVaSpace(UINT64 size, UINT64 base, LwRm *pLwRm);
    RC ExportFlaRange(UINT64 size, const string &name, LwRm *pLwRm);
    RC SetFlaExportPageSize(const string &name, const string &pageSizeStr);
    RC SetFlaExportPteKind(const string &name, const char *pteKindStr);
    RC SetFlaExportAcr(const string &name, const string &value, UINT32 index);
    RC SetFlaExportAperture(const string &name, const string &aperture);
    RC SetFlaExportPeerId(const string &name, UINT32 peerId);
    RC SetFlaExportEgm(const string &name);
    RC AllocFlaExportSurfaces(LwRm *pLwRm);
    set<MdiagSurf*> GetFlaExportSurfacesLocal();
    MdiagSurf* GetFlaExportSurfaceLocal(const string &name);

    // a list of LWGpuTsg pointer shared among multiple tests
    // Each SmcEngine will have its own list of shared Tsgs
    typedef list<LWGpuTsg*> SharedLWGpuTsgList;
    // The key SmcEngine* can be nullptr in case of non-SMC mode
    map<SmcEngine*, unique_ptr<SharedLWGpuTsgList>> m_SharedLWGpuTsgMap;

    // available channel ID pool
    map<pair<LwRm*, UINT32>, vector<UINT08>> m_AvailableChIdPoolPerEngine;
    // used by random channel id allocation
    unique_ptr<RandomStream> m_ChIdRandomStream;
    // max number of channel id: 128 for fermi, 4k for kepler, 2k per engine for ampere.
    UINT32 m_MaxNumChId;

    // to generate random location
    unique_ptr<RandomStream> m_LocationRandomizer;

    vector<CRegisterControl> m_mdiagPrivRegs;
    vector<CRegisterControl> m_privRegControls;
    vector<CRegisterControl> m_confRegControls;

    // global shared surfaces
    map<string, MdiagSurf*> m_SharedSurfaces;

    // a set of active Trace3DTests which are using this resource
    set<Trace3DTest*> m_ActiveT3DTests;

    void* m_FECSMailboxMutex;

    UINT32 m_cfg_base;

    VaSpaceManagers m_VaSpaceManagers;
    SubCtxManagers m_SubCtxManagers;

    LWGpuContexScheduler::Pointer m_ContexScheduler;
    bool m_IsMultiVasSupported;

    bool m_SyncTracesMethods;
    unique_ptr<ZbcTable> m_ZbcTable;
    unique_ptr<DispPipeConfig::DispPipeConfigurator> m_pDispPipeConfigurator;
    SmcResourceController * m_pSmcResourceController;
    void* m_SmcRegHalMutex;
    map<GpuPartition*, LwRm*> m_MapPartitionLwRmPtr;
    map<GpuPartition*, LwRm::Handle> m_MapPartitionResourceHandle;
    map<SmcEngine*, LwRm*> m_MapSmcEngineLwRmPtr;
    map<SmcEngine*, LwRm::Handle> m_MapSmcEngineExelwtionPartitionHandle;
    map<SmcEngine*, LwRm::Handle> m_MapSmcEngineMemoryPartitionHandle;
    void PrintDumpRegisters
    (
        FILE *pFile,
        const RefManualRegister* pReg,
        UINT32 addr,
        int i,
        int j,
        UINT32 value,
        SmcEngine *pSmcEngine = nullptr
    ) const;

    void ProcessPrintDumpRegisters
    (
        FILE* pFile,
        const RefManualRegister* pReg,
        UINT32 addr,
        UINT32 subdev, 
        bool isConfig,
        int i = 0,
        int j = 0
    ) const;

    SharedSurfaceController * m_pSharedSurfaceController;

    // FLA export surfaces on this GPU, mapped by their names
    map<string, unique_ptr<MdiagSurf>> m_FlaExportSurfaces;
    vector<SmcRegHal*> m_SmcRegHals;

    bool m_DpcScanoutSet = false;
    bool m_DpcTestRun = false;
    bool m_DpcTestCleanup = false;
};

#endif
