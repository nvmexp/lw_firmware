/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _TRACE_3D_H_
#define _TRACE_3D_H_
#include <iostream>

#include "mdiag/channelallocinfo.h"
#include "mdiag/resource/lwgpu/verif/GpuVerif.h"
#include "mdiag/tests/conlwrrentTest.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/utils/StopWatch.h"
#include "mdiag/utils/buf.h"
#include "mdiag/utils/buffinfo.h"
#include "mdiag/utils/cregctrl.h"
#include "mdiag/utils/tex.h"
#include "mdiag/utils/tracefil.h"
#include "mdiag/utils/types.h"
#include "mdiag/utils/utils.h"

#include "gputrace.h"
#include "mailbox.h"
#include "pe_configurefile.h"
#include "plugin.h"
#include "p2psurf.h"
#include "slisurf.h"
#include "tracemod.h"
#include "tracechan.h"

class SmcEngine;

#ifndef _WIN32
class CompBitsTestIF;
#endif

class UtlGpuVerif;

// forward declarations for trace_3d plugin support
//
namespace T3dPlugin
{
    class HostMods;
    class EventMods;
}

class ArgReader;
class ICrcProfile;
class TraceTsg;
class PolicyManager;
class PmTest;
class PmVaSpace;

// fwd decl

struct TraceModuleCmp
{
    explicit TraceModuleCmp(const TraceModule* ModPtr) : m_ModPtr(ModPtr) {}
    bool operator()(const pair<TraceModule *, MdiagSurf>& that) const
    {
        return m_ModPtr == that.first;
    }
private:
    const TraceModule* m_ModPtr;
};


// In some cases, some arguments on one test will conflict with/have effect on the ones of other tests.
// We need to restore some status shared by all tests to handle this.

struct LwrrentTestsSetting
{
    LwrrentTestsSetting () : hasTuT (false), hasPmFileArg(false), hasPmSbFileArg(false) { }

    // Register handling is per SMC engine.
    // If multiple tests run single SMC engine in ctxswitch mode. the register option should
    // be handled only once for the SMC engine.
    // s_RegCtrlMap restores all the valid regCtrls to be handled for each SMC engine;
    map<string, vector<CRegisterControl>> regCtrlMap;

    // be handled only once for each channel handler
    // Store all valid regCtrls for each channel handler.
    map<LwRm::Handle, vector<CRegisterControl>> ctxRegCtrlMap;

    // If there is TUT in all the tests.
    bool hasTuT;

    // If there is test with -pmfile args in all the tests.
    bool hasPmFileArg;

    // If there is test with -pm_sb_file args in all the tests.
    bool hasPmSbFileArg;
};

class Trace3DTest: public ConlwrrentTest
{
public:
    Trace3DTest(ArgReader *reader,
                UINT32 deviceInst = Gpu::UNSPECIFIED_DEV);
    virtual ~Trace3DTest();

    static Test *Factory(ArgDatabase *args);

    int FailSetup();

    int RealSetup();

    // a little extra setup to be done
    virtual int Setup();
    virtual int Setup(int stage);
    virtual int NumSetupStages() { return 3; }

    void AbortTest() { m_bAborted = true; }
    bool AbortedTest() const { return m_bAborted; }

    void ClientExit() { m_clientExited = true; }
    TestStatus GetClientStatus() { return m_clientStatus; }
    void SetClientStatus(TestStatus status) { m_clientStatus = status; }
    bool HandleClientIRQStatus(T3dPlugin::MailBox::IRQStatus status);

    // methods for ConlwrrentTest

    virtual int BeginIteration();
    virtual int StimulateDevice(int deviceStatus);
    int GetDeviceStatus();
    virtual void EndIteration();
    virtual TestStatus CheckPassFail();
    virtual const char* GetTestName();
    virtual void PreRun();
    virtual void PostRun();  // Override behavior of test::PostRun()
    virtual void DumpStateAfterHang();

    // Overload GetStatus() so that it becomes a public function.  Peer modules
    // need to check status for peer tests.
    Test::TestStatus GetStatus(void) const { return Test::GetStatus(); }
    // Overload SetStatus() so that it becomes a public function, dynamic CRC
    // check needs to modify the test result
    void SetStatus(TestEnums::TEST_STATUS s) { Test::SetStatus(s); }
    void SetStatus(TestStatus s)             { Test::SetStatus(s); }

    enum {
        WAIT1 =       START_OF_OTHER_ENUMS+0,
        WAIT2 =       START_OF_OTHER_ENUMS+1,
    };

    // a little extra cleanup to be done
    virtual void CleanUp();
    virtual void CleanUp(UINT32 stage);
    virtual void BeforeCleanup();
    virtual UINT32 NumCleanUpStages() const { return 2; }

    RC InitAllocator();

    // allocates memory on behalf of a GpuTrace -
    //  returns base of buffer if successful, MemoryResource::NO_ADDRESS if not
    //  fills in handle and offset if the trace provides non-null pointers
    bool TraceAllocate(TraceModule *,
                       GpuTrace::TraceFileType filetype,
                       UINT32 bytes_needed,
                       MdiagSurf **ppDmaBuf,
                       UINT64 *pOffset,
                       bool *alloc_separate);

    MdiagSurf *GetSurface(GpuTrace::TraceFileType filetype, LwRm::Handle hVas) const
        {
            map<LwRm::Handle, Chunk*>::const_iterator it = m_Chunks.find(hVas);
            if (it == m_Chunks.end())
            {
                ErrPrintf("chunk memory for vaspace handle %x not allocated yet!\n", hVas);
                MASSERT(false);
            }
            return & (it->second[filetype]).m_Surface;
        }

    MdiagSurf *GetSurfaceByName(const char* name);

    void AddRenderTargetsToChannelVerif(TraceChannel *channel,
        bool graphicsChannelFound);

    bool RenderTargetCheckedByChannel(TraceChannel *channel,
        SurfaceType surfaceType, bool graphicsChannelFound);

    bool NeedPeerMapping(MdiagSurf* Surf) const;
    bool NeedDmaCheck() const;
    LwRm::Handle GetSurfaceCtxDma(int filetype, LwRm::Handle hVASpace) const;
    IGpuSurfaceMgr *GetSurfaceMgr() { return gsm; }

    UINT32 GetMethodResetValue(UINT32 HwClass, UINT32 Method) const;
    UINT32 GetFermiMethodResetValue(UINT32 Method) const;
    UINT32 GetMaxwellBMethodResetValue(UINT32 method) const;

    GpuTrace *GetTrace() { return &m_Trace; }
    virtual TraceFileMgr *GetTraceFileMgr() { return &m_TraceFileMgr; }
    SliSurfaceMapper *GetSSM() { return &m_SSM; }
    P2pSurfaceMapper *GetP2pSM() { return &m_P2pSM; }
    MdiagSurf *LocateTextureBufByOffset( UINT64 offset );

    UINT32 ClassStr2Class(const char *ClassStr) const;

    // List of TraceChannels (just one item, in pre-V3 syntax).
    typedef list<TraceChannel*> TraceChannels;
    typedef GpuTrace::ChannelManagedMode ChannelManagedMode;
    TraceChannel * GetChannel(const string &chname, ChannelManagedMode mode = GpuTrace::CPU_GPU_MANAGED);
    TraceChannel * GetDefaultChannel();
    TraceChannel * GetDefaultChannelByVAS(LwRm::Handle hVASpace) const;
    TraceChannel * GetLwrrentChannel(ChannelManagedMode mode = GpuTrace::CPU_GPU_MANAGED);
    TraceChannel * GetGrChannel();
    void  GetComputeChannels(vector<TraceChannel*>& compute_channels);
    RC AddChannel(TraceChannel* traceChannel);    // add channel to list after it's allocated
    RC RemoveChannel(TraceChannel* traceChannel); // remove channel from list after it's freed
    const TraceChannels *GetChannels() const { return &m_TraceCpuManagedChannels; }

    typedef list<TraceTsg*> TraceTsgs;
    TraceTsg * GetTraceTsg(const string& tsgName);
    RC AddTraceTsg(TraceTsg* pTraceTsg);

    friend class GenericTraceModule;   // @@@ why?
    friend class TraceChannel; // To do: move protected member to private

    // PolicyManager interface
    //! \brief register this test to the PolicyManager
    // TODO_GPS: move it to the base class
    RC RegisterWithPolicyManager();

    //! Scissor rectangle.
    struct ScissorRect
    {
        UINT32 xMin, xMax, yMin, yMax;
    };

    virtual LWGpuResource * GetGpuResource() const;
    GpuDevice     * GetBoundGpuDevice();

    void BlockEvent( bool bBlock );
    void UnBlockEvent( bool bBlock );;

    enum TraceEventType
    {
        #undef DEFINE_TRACE_EVENT
        #define DEFINE_TRACE_EVENT(type) type,
        #include "t3event.h"
        Unknown
    };

    // Find the TraceEventType by string.
    // Return Unknown if not found.
    static TraceEventType GetEventTypeByStr(const string& eventName);

    static const map<TraceEventType, string> s_EventTypeMap;

    // for declaring plugin events throughout the trace_3d code.  Calling
    // traceEvent( TraceEventType ) will immediately ilwoke the callback event handler
    // function in every plugin which has registered to receive that event
    // This version of traceEvent creates a temporary event with no parameters,
    // sends the event, then deletes the temporary event.

    RC traceEvent(TraceEventType eventType, bool bBlock=false );

    // form of traceEvent that takes one UINT32 parameter
    //
    RC traceEvent(TraceEventType eventType, const char * pName, UINT32 pValue, bool bBlock=false );

    // form of traceEvent that takes one str parameter
    //
    RC traceEvent(TraceEventType eventType, const char * pName, const char *pValue, bool bBlock=false );

    // custom trace event. allow user to send custom event. should only be called from policy manager.
    RC lwstomTraceEvent(const char * eventName, const char * pName, const char * pValue, bool bBlock=false);

    // this version of traceEvent sends an already existing event
    //
    RC traceEvent( T3dPlugin::EventMods * pEvent, bool bBlock=false );

    // creates an empty traceEvent but doesn't send it.  This allows
    // the caller to set event parameters before sending it
    //
    T3dPlugin::EventMods * newTraceEvent(TraceEventType eventType);

    // utility function to create and send a BeforeMethodWrite
    // event of type WriteNop.  implemented in plgnsprt.cpp
    //
    RC BeforeMethodWriteNop_traceEvent( UINT32 origOffset,
                                        UINT32 channelNumber,
                                        UINT32 subchannelClass,
                                        const char* channelName );

    // utility function to create and send a BeforeMethodWrite
    // event of type MethodWriteN.  implemented in plgnsprt.cpp
    //
    RC BeforeMethodWriteN_traceEvent( UINT32 origOffset,
                                      UINT32 method_offset,
                                      UINT32 num_dwords,
                                      UINT32 * pData,
                                      const char * incrementType,
                                      UINT32 channelNumber,
                                      UINT32 subchannelClass,
                                      const char* channelName );

    // utility function to create and send a BeforeMethodWrite
    // event of type MethodWrite.  implemented in plgnsprt.cpp
    //
    RC BeforeMethodWrite_traceEvent( UINT32 pbOffsetHeader,
                                     UINT32 method_offset,
                                     UINT32 methodData,
                                     UINT32 channelNumber,
                                     UINT32 subchannelClass,
                                     const char* channelName );

    // utility function to create and send a PushbufferHeader
    // event.  Gathers return codes & messages from the plugin(s) and
    // packages them as an event action with optional data word.
    // returns true if there is an output action
    // returns false if there is no output action or the plugin system
    // is not enabled
    //
    bool PushbufferHeader_traceEvent( UINT32 pbOffsetHeader,
                                      const string & pbName,
                                      T3dPlugin::EventAction * pAction,
                                      UINT32 * eventData );

    RC FlushMethodsAllChannels( void );
    RC WaitForDMAPushAllChannels( void );
    RC WriteWfiMethodToAllChannels( void );

    static Trace3DTest *FindTest(string testName);
    static Trace3DTest *FindTest(UINT32 testId);
    static UINT32 ConlwrrentRunTestNum();
    void AddPeerMappedSurface(GpuDevice         *pPeerDevice,
                              string             surfaceName);

    RC WaitForIdleRmIdleAllChannels();
    RC WaitForIdleHostSemaphoreIdleAllChannels();
    RC WaitForIdleRmIdleOneChannel(UINT32 chNum);
    UINT32 GetMethodCountOneChannel(UINT32 chNum);
    UINT32 GetMethodWriteCountOneChannel(UINT32 chNum);

    // Wait until the test is done in WFI_INTR and WFI_NOTIFY mode.
    RC WaitForNotifiersAllChannels();

    // check for and receive a UINT32 message from the plugin
    // implemented in plgnsprt.cpp
    //
    bool getPluginMessage( const char * msg, UINT32 * pValue );

    void SetAllocSurfaceCommandPresent(bool present)
        { m_AllocSurfaceCommandPresent = present; }

    bool GetAllocSurfaceCommandPresent() const
        { return m_AllocSurfaceCommandPresent; }

    bool GetDidNullifyBeginEnd(UINT32 hwClass, UINT32 beginMethod);
    void SetDidNullifyBeginEnd(UINT32 hwClass, UINT32 beginMethod);

    // for BeforeTraceOp and AfterTraceOp events
    //
    bool TraceOp_traceEvent( TraceEventType eventType,
                             UINT32 traceOpNum,
                             T3dPlugin::EventAction * pAction,
                             UINT32 * eventData );

    // Returns whether this test is a context switching test.
    bool IsTestCtxSwitching() { return m_IsCtxSwitchTest; }

    BuffInfo *GetBuffInfo() { return &m_BuffInfo; }

    RC SetIndividualBufferAttr(MdiagSurf *surf);

    ITestStateReport* GetTestIStateReport() {  return getIStateReport();  }

    void FailTest(Test::TestStatus status = TEST_FAILED);

    string GetTraceHeaderFile() { return header_file; }

    void GetSurfaceCrcRect(MdiagSurf *surface,
        SurfaceType surfaceType, int index, CrcRect* rect);

    void AddRenderTargetVerif(SurfaceType surfaceType);

    const ArgReader* GetParam() const { return params; }

    RC SetupCrcChecker();
    RC SetupChannelCrcChecker(TraceChannel* channel, bool graphicsChannelFound);
    RC RegisterChannelToPolicyManager(PolicyManager* pPM,
        PmTest* pTest, TraceChannel* pTraceChannel);

    RC SetupCtxswZlwll(TraceChannel* channel);
    RC SetupCtxswPm(TraceChannel* channel);
    RC SetupCtxswSmpc(TraceChannel* channel);
    RC SetupCtxswPreemption(TraceChannel* channel);

    void ApplySLIScissorToChannel(TraceChannel* pTraceChannel);

    void SetSmcEngine(SmcEngine * pSmcEngine) override;

    // TRACE_OPTIONS support
    void SetWfiMethod(WfiMethod method)   { m_WfiMethod = method; }
    WfiMethod GetWfiMethod() const        { return m_WfiMethod;   }
    void SetNoPbMassage(bool NoPbMassage) { m_NoPbMassage = NoPbMassage; }
    bool GetNoPbMassage() const           { return m_NoPbMassage;        }
    void SetNoAutoFlush(bool NoAutoFlush) { m_NoAutoFlush = NoAutoFlush; }
    bool GetNoAutoFlush() const           { return m_NoAutoFlush;        }

    void SetTextureHeaderFormat(TextureHeader::HeaderFormat headerFormat)
        { m_TextureHeaderFormat = headerFormat; }
    TextureHeader::HeaderFormat GetTextureHeaderFormat() const
        { return m_TextureHeaderFormat; }

    GpuVerif* GetTestGpuVerif() { return m_TestGpuVerif; }

    virtual void EnableEcoverDumpInSim(bool enabled) {} // bypass the default impl in Test
    virtual bool IsEcoverSupported() { return true; }

    void DeclareTegraSharedSurface(const char *name);

    void DeclareGlobalSharedSurface(const char *name, const char *global_name);

    RC SetEnableCEPrefetch(GpuSubdevice *gpuSubdev, bool enabled);

    RC SetDiGpuReady(GpuSubdevice *gpuSubdev);

    RC SetGpuPowerOnOff(GpuSubdevice *gpuSubdev, UINT32 action,
                        bool bUseHwTimer, UINT32 timeToWakeUs,
                        bool bIsRTD3Transition, bool bIsD3Hot);

    bool IsTopPlugin() { return m_IsTopPlugin; }
    UINT32 GetCoreId() { return m_CoreId; }
    UINT32 GetThreadId() { return m_ThreadId; }

    T3dPlugin::MailBox *GetPluginMailBox() { return m_PluginMailBox; }
    T3dPlugin::MailBox *GetTraceMailBox() { return m_TraceMailBox; }

    UINT32 GetTPCCount(UINT32 subdev);
    UINT32 GetComputeWarpsPerTPC(UINT32 subdev);
    UINT32 GetGraphicsWarpsPerTPC(UINT32 subdev);

    virtual UINT32 GetTestId() const { return m_TestId; }
    void CreateChunkMemory(LwRm::Handle  hVASpace);
    RC GetUniqueVAS(PmVaSpace ** pVaSpace);

    virtual LwRm* GetLwRmPtr() { return m_pLwRm;}
    virtual SmcEngine* GetSmcEngine() { return m_pSmcEngine; }

    //! Get the partiton enable table's configure file
    //! Realize on the pe_configureFile.h
    PEConfigureFile * GetPEConfigureFile() { return & m_PEConfigureFile; }

    // Returns whether this test is a master subctx. Only for subctx perf test
    bool IsCoordinatorSyncCtx() const;
    void SetCoordinatorSyncCtx(bool isCoordinator);

    // print allocated channel info;
    void PrintChannelAllocInfo();

    virtual UtlGpuVerif * CreateGpuVerif(LwRm::Handle hVaSpace, LWGpuChannel * pCh);
    RC HandleOptionalCtxRegisters(TraceChannel* pTraceChannel = nullptr);
protected:
    GpuTrace m_Trace;
    TraceFileMgr m_TraceFileMgr;

    MdiagSurf *surfZ;
    MdiagSurf *surfC[MAX_RENDER_TARGETS];

    const ArgReader *params;
    IGpuSurfaceMgr *gsm;

    // Data for Allocation
    struct Chunk{
        Chunk() : m_LwrOffset(0), m_NeedPeerMapping(false), m_SurfaceCtxDma(0)
        {
             // Set surfaces to a bogus memory space initially, so we can
             // whether they got overridden
             m_Surface.SetLocation((Memory::Location)0xFFFFFFFF);
        }
        MdiagSurf m_Surface;
        UINT64 m_LwrOffset;
        bool m_NeedPeerMapping;
        LwRm::Handle m_SurfaceCtxDma;
    };
    // every vaspace has its own chunks
    map<LwRm::Handle, Chunk*> m_Chunks;

    string m_PteKindName[GpuTrace::FT_NUM_FILE_TYPE];
    UINT32 m_Padding[GpuTrace::FT_NUM_FILE_TYPE];

    // In paging mode texture are allocated separatly
    typedef list<pair<TraceModule *, MdiagSurf> > Textures;
    Textures m_Textures;

    // This structure is used to hold information about
    // buffers to be allocated.  There are two basic types
    // of buffers in trace3d (large-chunk and individual)
    // and this structure is used for both.
    struct AllocInfo
    {
        MdiagSurf *m_Surface;
        GpuTrace::TraceFileType m_FileType;
        TraceModule *m_Module;

        AllocInfo(MdiagSurf *pSurface, GpuTrace::TraceFileType fileType, TraceModule *pModule)
        : m_Surface(pSurface), m_FileType(fileType), m_Module(pModule) {}

        // Individual buffers have a TraceModule associated
        // with them and large-chunk buffers don't.
        bool IsChunkBuffer() { return m_Module == nullptr; }

        bool operator<(const AllocInfo &rhs) const;
        bool HasLockedAddress() const;
        bool HasAddressRange() const;
    };

    // These are used in -testContextObjects testing
    list<MdiagSurf> m_DmaBuffers;

    MdiagSurf m_GSSpillRegion;
    string m_GSSpillRegionPteKindName;

    bool scissorEnable;
    UINT32 scissorWidth;
    UINT32 scissorHeight;
    UINT32 scissorXmin;
    UINT32 scissorYmin;

    UINT32 m_streams;

    UINT32 traceDisplayWidth;
    UINT32 traceDisplayHeight;

    LWGpuResource *m_pBoundGpu;
    UINT32         m_GpuInst;

private:
    static LwrrentTestsSetting s_LwrrentTestsSetting;
    SliSurfaceMapper m_SSM;
    P2pSurfaceMapper m_P2pSM;
    unique_ptr<StopWatch> meter;  // compute time for a trace

    enum { DEFAULT_DISPLAY_WIDTH = 640,
           DEFAULT_DISPLAY_HEIGHT = 480 };

    TraceChannels m_TraceCpuManagedChannels;
    TraceChannels m_TraceGpuManagedChannels;
    TraceChannel* m_LwrrentChannel; // The latest channel just declared in the trace
    RC FreeChannels();
    RC FreeTsgs();
    RC FreeSubCtxs(UINT32 testId);
    RC FreeVaSpace(UINT32 testId);
    void FreeResource();
    void CheckStopTestSyncOnFail(const char* pCheckPointName);
    void TestSynlwp(const MDiagUtils::TestStage stage);

    void ProcessPreemptionStats();

    // local TraceTsg objects owned by the trace_3d instance
    TraceTsgs m_TraceTsgs;
    // local register control
    vector<CRegisterControl> m_traceRegCtrls;

    ChannelAllocInfo m_ChannelAllocInfo;

    string header_file; // test.hdr, including path

    int num_loops;

    UINT32 start_offset;
    UINT32 end_offset;
    bool conlwrrent;
    int state;

    enum PmMode {
        PM_MODE_NONE,
        PM_MODE_PM_SB_FILE,
        PM_MODE_PM_FILE
    };

    PmMode pmMode;
    UINT32 numPmTriggers;
    bool pmCapture;

    bool m_beginSwitchCalled;

    UINT32 m_stream_buffer;

    string output_filename;

    CrcEnums::CRC_MODE m_CrcDumpMode;
    unique_ptr<ICrcProfile> m_profile;
    unique_ptr<ICrcProfile> m_goldProfile;

    bool   m_DisplayActive;
    UINT32 m_DisplayCtxDma;
    UINT32 m_DisplayMemHandle;

    bool m_AllocSurfaceCommandPresent;

    BuffInfo m_BuffInfo;
    MdiagSurf m_PadBuffer;
    unique_ptr<EcovVerifier> m_EcovVerifier;

    bool m_bAborted;

    bool m_clientExited;

    // TRACE_OPTIONS
    WfiMethod m_WfiMethod;
    bool      m_NoPbMassage;
    bool      m_NoAutoFlush;

    TextureHeader::HeaderFormat m_TextureHeaderFormat =
        TextureHeader::HeaderFormat::Unknown;

    // SLI support
    //! Distributes the surfaces on the available subdevices, if any.
    RC MapSliSurfaces();
    //! Parses scissor and SLI scissor parameters.
    RC SetupScissor();
    //! Applies SLI scissor to pushbuffer by means of massaging.
    void ApplySLIScissor();

    //! SLI scissor relative scanline amounts (the sum equals to all scanlines).
    IGpuSurfaceMgr::SLIScissorSpec m_SLIScissorSpec;
    //! Scissor rectangles per-subdev in framebuffer space.
    vector<ScissorRect> m_SLIScissorRects;
    //! Indicates if SLI scissor is enabled.
    bool m_EnableSLIScissor;
    bool m_EnableSLISurfClip;

    //! the configure file which configure partition enable table
    PEConfigureFile m_PEConfigureFile;

    string m_TestName;
    static map <string, Trace3DTest *> s_Trace3DName2Test;
    static map <UINT32, Trace3DTest *> s_Trace3DId2Test;

    bool CheckPMSyncNumInConlwrrentTests();

    map< string, set<GpuDevice *> > m_PeerMappedSurfaces;

    // Private functions:
    RC GetBoundResources();
    RC GetGpuResourcesByInst();
    RC GetGpuResourcesByClasses();

    int SetupStage0();
    int SetupStage1();
    int SetupStage2();
    int SetupSurfaces();
    RC  MapPeerSurfaces();
    int FinishSetup();

    std::string StripToLwdiag(std::string& path) const;
    std::string GetChipName() const;
    RC RunCosim(
        const char* elfName,
        const char* secondaryElf,
        const char* backdoorLibPath,
        const char* cosim_args) const;

    void CleanUpStage0();
    void CleanUpStage1();

    virtual bool BeforeEachMethodGroup(unsigned num_methods) { return true; }
    virtual bool BeforeExelwteMethods()  { return true; }
    virtual bool RunBeforeExelwteMethods() { return true; }
    virtual bool BeforeEachMethod() { return true; }
    virtual bool AfterExelwteMethods() { return true; }
    virtual bool RunAfterExelwteMethods();

    RC BeginChannelSwitch();
    void EndChannelSwitch();
    void SplitStreamBuffers() const;
    RC AllocChannels();
    RC SetupClasses();
    bool LoadPushBuffers();
    int SetupPM(const ArgReader* params);

    RC DumpAllBuffers();

    TestEnums::TEST_STATUS VerifySurfaces(ITestStateReport *report);
    TestEnums::TEST_STATUS VerifySurfaces2SubDev(ITestStateReport *report, UINT32 subdev);

    RC SendNotifiers();
    RC WaitForIdle();
    RC SetupChecker();
    RC DisplaySurface();
    RC StopDisplay();
    void StopPM();

    // Policy manager
    RC PolicyManagerStartTest();
    RC PolicyManagerEndTest();
    bool StartPolicyManagerAfterSetup() const;

    // m_pPluginHost is the top-level manager object of all the trace_3d plugins
    //
    T3dPlugin::HostMods * m_pPluginHost;

    // Loads and initializes a plugin
    //
    RC LoadPlugin( const char * pluginInfo );
    void ShutdownPlugins( void );
    RC PollAckAndServeApiRequest(T3dPlugin::MailBox::Command ack);
    RC PluginStartupSender();
    RC PluginInitializeSender(const char *pluginArgs);
    RC PluginShutdownSender();
    RC LoadTopPlugin(const char *pluginArgs);
    RC ShutdownTopPlugins();
    void ShutdownAsim();

    // DLL handle for the plugin library
    //
    void * m_pluginLibraryHandle;
    string m_pluginName;

    //record all plugin Hosts which belong to a same dll lib in different T3dTests
    struct RefHosts
    {
        RefHosts():refCount(0){}
        UINT32 refCount;
        vector<T3dPlugin::HostMods*> hosts;
    };
    typedef map<string, RefHosts> PluginHosts;
    static PluginHosts s_Hosts;

    bool IsPeer();

    void ParseArgsForType(GpuTrace::TraceFileType,
        const ArgReader *params, const char *MemTypeName, LwRm::Handle hVASpace);

    RC CheckCtxRegCtrl(LwRm::Handle handle, const CRegisterControl& testRegCtrl);
    RC AddSmcRegCtrl(const CRegisterControl& testRegCtrl);
    RC AddRegCtrl(const CRegisterControl& testRegCtrl);
    RC ParseOptionalRegisters();
    RC GetHandleByRegSpace(string regSpace, LwRm::Handles& handles, TraceChannel* pTraceChannel = nullptr);
    RC HandleOptionalRegisters();

    RC ParseOptionalPmFile();

    RC CheckAddressRange(const string &name, UINT64 lwrrentAddress,
        UINT64 size, UINT64 addressMin, UINT64 addressMax) const;

    RC InitChunkBuffer(vector<AllocInfo>* pBuffersToAlloc);
    RC InitChunkBuffer(LwRm::Handle hVASpace, Chunk* pChunks, vector<AllocInfo>* pBuffersToAlloc);
    RC InitIndividualBuffer(vector<AllocInfo>* pBuffersToAlloc);

    RC AllocateBuffer(vector<AllocInfo>* pBuffersToAlloc);
    RC AllocateChunkBuffer(AllocInfo *info);
    RC AllocateIndividualBuffer(AllocInfo *info);

    RC MapLoopbackChunkBuffer();

    RegHal& Regs(UINT32 subdev = 0);

    // record of what hwClass,beginMethod combinations have already been
    // nullified via NullifyBeginEndMethodsFermi
    //
    set< pair<UINT32,UINT32> > m_nullifyBeginEndSet;

    TestStatus m_clientStatus;

    RC SwitchElpg(bool onOff);
    //! Attempts to set the pstate for mscg, but does nothing if mscg not enabled in mask
    RC SetPStateForMSCG(UINT32 elpgMask);

    bool m_IsCtxSwitchTest; // whether this test is a context switching test.

    RC SelfGildArgCheck();

    UINT32 GetGrInfo(UINT32 index, UINT32 subdevIndex);

#ifndef _WIN32
    RC CreateCompbitTest();
#endif

    GpuVerif* m_TestGpuVerif; // GpuVerif of test in case no channel in trace.

    bool m_SkipDevInit;

    set<string> m_TegraSharedSurfNames;
    RC SetupTegraSharedModules();
    RC CheckIfAnyTegraSharedSurfaceLeft();

    //map local surface name to global surface name
    typedef map<string, string> GlobalSharedSurfNames;
    GlobalSharedSurfNames m_GlobalSharedSurfNames;

    bool m_IsTopPlugin;

    static UINT32 s_ThreadCount;

    UINT32 m_CoreId;
    UINT32 m_ThreadId;

    //These two flags are used to avoid race condition issue.
    //For example, PM thread and T3D thread send trace
    //event with the same T3D object to plugin at the same time.

    //The flag m_IsProcessingEvent stands for: Mods has sent some event
    //to plugin, but plugin has not send the response.
    bool m_IsProcessingEvent;
    //This flag stands for whether mods need wait until the previous
    //events receives its response
    bool m_NeedBlockEvent;

    T3dPlugin::MailBox *m_PluginMailBox;
    T3dPlugin::MailBox *m_TraceMailBox;

    SmcEngine  *m_pSmcEngine;
    LwRm       *m_pLwRm;

#ifndef _WIN32
    unique_ptr<CompBitsTestIF> m_CompBitsTest;
#endif

    UINT32 m_TestId;

    bool m_CoordinatorSyncCtx;
    static void SpecifyCoordinatorSyncCtx(Trace3DTest* test);

    // Ensure TestCleanUp won't be synced multiple times.
    bool m_CleanupDone = false;

    string m_SmcEngineName;

    // If this test is TUT.
    bool m_IsTuT;
};

class Trace3DEventLock
{
public:
    Trace3DEventLock(Trace3DTest *pTest, bool bBlock): m_Ptr(pTest), m_bBlock(bBlock) { m_Ptr->BlockEvent(m_bBlock);}
    ~Trace3DEventLock() {m_Ptr->UnBlockEvent(m_bBlock);}

private:
    Trace3DTest *m_Ptr;
    bool m_bBlock;
};

enum GRPreemptionEnum
{
    GR_PREEMPTION_WFI = 1,
    GR_PREEMPTION_GFXP
};

enum ComputePreemptionEnum
{
    COMPUTE_PREEMPTION_WFI = 1,
    COMPUTE_PREEMPTION_CTA,
    COMPUTE_PREEMPTION_CILP
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(trace_3d, Trace3DTest, "plays back GPU traces");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &trace_3d_testentry
#endif

#endif

