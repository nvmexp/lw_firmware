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
#ifndef INCLUDED_GPUTRACE_H
#define INCLUDED_GPUTRACE_H

#include "mdiag/utils/types.h"
#include "mdiag/utils/tracefil.h"
#include "mdiag/resource/lwgpu/verif/GpuVerif.h"
#include "mdiag/gpu/surfmgr.h"

#include <map>
#include <set>
#include <deque>
#include <queue>

class Trace3DTest;
class ArgReader;
struct ParamDecl;
class SelfgildState;
struct TraceGpEntry;
struct TraceUpdateFile;
class GpuChannelHelper;
class TextureHeader;
struct TextureHeaderState;
class TraceChannel;
class TraceSubChannel;
class BufferDumper;
class DmaLoader;
// TraceOp is defined in traceop.h.
class TraceOp;
class TraceOpPmu;
class TraceOpSendGpEntry;
class TraceTsg;

typedef multimap<UINT32, TraceOp*> TraceOps;
typedef map<const TraceOp*, UINT32> TraceOpLineNos;
typedef map<string, UINT32> UserDefTraceOpIdMap;

// TraceModule is defined in tracemod.h
class TraceModule;
class GenericTraceModule;
class SurfaceTraceModule;
class PeerTraceModule;
typedef list<TraceModule *> TraceModules;
typedef TraceModules::const_iterator ModuleIter;
typedef std::priority_queue<UINT32,vector<UINT32>,greater<UINT32> > ImageList_t;
typedef multimap<UINT32, TraceModule *> DumpBufferMaps_t;
typedef map<string, vector<string> > DumpBufferMapOnEvent_t;

// subctx support
class SubCtx;
class ParseFile;
class ConfigureFile;
class WaterMarkImpl;

// The following flags are used with a MemoryCommandPropertyMap object.
// Each flag represents a memory command that can be used in a trace3d
// trace header.  The MemoryCommandPropertyMap object specifies which commands
// can be used for each option.
const UINT32 MC_ALLOC_SURFACE  = 0x0001;
const UINT32 MC_SURFACE_VIEW   = 0x0002;
const UINT32 MC_ALLOC_VIRTUAL  = 0x0004;
const UINT32 MC_ALLOC_PHYSICAL = 0x0008;
const UINT32 MC_MAP            = 0x0010;

const UINT32 MC_ALL_COMMANDS =
    MC_ALLOC_SURFACE |
    MC_SURFACE_VIEW |
    MC_ALLOC_VIRTUAL |
    MC_ALLOC_PHYSICAL |
    MC_MAP;

// Represents TraceOp types accordingly in trace.hdr
enum class TraceOpType
{
    AllocSurface, // ALLOC_SURFACE
    AllocVirtual, // ALLOC_VIRTUAL
    AllocPhysical, // ALLOC_PHYSICAL
    Map, // MAP
    SurfaceView, // SURFACE_VIEW
    FreeSurface, // FREE_SURFACE, FREE_VIRTUAL, FREE_PHYSICAL, UNMAP
    UpdateFile, // UPDATE_FILE
    CheckDynamicSurface, // CHECK_DYNAMICSURFACE
    Reloc64, // RELOC64
    // Adding other TraceOp types here

    Unknown
};

enum class TraceOpTriggerPoint : UINT32
{
    None,                       // 0x00, 0000b
    // Trigger point when trace.hdr parsing
    Parse = 0x0001,             // 0x01, 0001b
    // Trigger point before TraceOp Run()
    Before = (Parse << 1),      // 0x02, 0010b
    // Trigger point after TraceOp Run()
    After = (Parse << 2)        // 0x04, 0100b       
};

class GpuTrace
{
public:
    // A type holding string properties from trace.hdr
    //   of a TraceOp line.
    typedef map<string, string> HdrStrProps;

private:
    enum TraceSource
    {
        FROM_FS_FILE,
        FROM_INLINE_FILE,
        FROM_CONSTANT
    };

    enum ZbcClearFmt
    {
        INVALID             = 0x00000000,     //color and depth format has the same value for invalid.
        FP32                = 0x00000001,     //used for depth format.
        U8                  = 0x00000001,     //used for stencil format.
        ZERO                = 0x00000001,
        UNORM_ONE           = 0x00000002,
        RF32_GF32_BF32_AF32 = 0x00000004,
        R16_G16_B16_A16     = 0x00000008,
        RN16_GN16_BN16_AN16 = 0x0000000c,
        RS16_GS16_BS16_AS16 = 0x00000010,
        RU16_GU16_BU16_AU16 = 0x00000014,
        RF16_GF16_BF16_AF16 = 0x00000016,
        A8R8G8B8            = 0x00000018,
        A8RL8GL8BL8         = 0x0000001c,
        A2B10G10R10         = 0x00000020,
        AU2BU10GU10RU10     = 0x00000024,
        A8B8G8R8            = 0x00000028,
        A8BL8GL8RL8         = 0x0000002c,
        AN8BN8GN8RN8        = 0x00000030,
        AS8BS8GS8RS8        = 0x00000034,
        AU8BU8GU8RU8        = 0x00000038,
        A2R10G10B10         = 0x0000003c,
        BF10GF11RF11        = 0x00000040
    };

    typedef map<string, UINT32> MemoryCommandPropertyMap;
    typedef map<string, bool> PropertyUsageMap;
    typedef map<string, string> TagToBufferTypeMap;
    typedef map<string, UINT32> ZbcClearFmtMap;

    struct MemoryCommandProperties
    {
        MemoryCommandProperties();

        vector<string> tagList;
        HdrStrProps hdrStrProps;
        string file;
        string format;
        string sampleMode;
        string layout;
        string compression;
        string zlwll;
        string aperture;
        string type;
        string access;
        string shaderAccess;
        string privileged;
        string crcCheck;
        string referenceCheck;
        bool ilwertCrcCheck;
        UINT32 crcCheckCount;
        string gpuCache;
        string p2pGpuCache;
        string zbc;
        string crcRect;
        string parentSurface;
        string traceChannel;
        UINT32 width;
        UINT32 height;
        UINT32 depth;
        UINT32 pitch;
        UINT32 blockWidth;
        UINT32 blockHeight;
        UINT32 blockDepth;
        UINT32 arraySize;
        UINT32 swapSize;
        UINT64 physAlignment;
        UINT64 virtAlignment;
        UINT64 size;
        UINT32 attrOverride;
        UINT32 attr2Override;
        UINT32 typeOverride;
        bool loopback;
        bool raw;
        bool rawCrc;
        bool skedReflected;
        bool contig;
        UINT64 virtAddress;
        UINT64 physAddress;
        UINT64 virtAddressMin;
        UINT64 virtAddressMax;
        UINT64 physAddressMin;
        UINT64 physAddressMax;
        UINT64 addressOffset;
        bool hostReflected;
        string hostReflectedChName;
        string virtualAllocName;
        string physicalAllocName;
        UINT64 virtualOffset;
        UINT64 physicalOffset;
        UINT32 mipLevels;
        UINT32 border;
        bool lwbemap;
        string pageSize;
        string mapMode;
        UINT32 classNum;
        UINT08 fill8;
        UINT32 fill32;
        string vpr;
        string gpuSmmu;
        bool sparse;
        bool mapToBackingStore;
        UINT32 tileWidthInGobs;
        UINT32 textureSubimageWidth;
        UINT32 textureSubimageHeight;
        UINT32 textureSubimageDepth;
        UINT32 textureSubimageXOffset;
        UINT32 textureSubimageYOffset;
        UINT32 textureSubimageZOffset;
        UINT32 textureSubimageMipLevel;
        UINT32 textureSubimageDimension;
        UINT32 textureSubimageArrayIndex;
        UINT32 textureMipTailStartingLevel;
        UINT32 textureMipTailDimension;
        UINT32 textureMipTailArrayIndex;
        UINT32 atsPageSize;
        string vasName;
        bool htex;
        bool isShared;
        UINT32 smCount;
        UINT32 pteKind;
        string inheritPteKind;
        bool isTextureHeader;
    };

    bool m_OpDisabled; // disabled Op will not be inserted into queue to exlwte
public:

    // note: the order of these enums must match the order of the
    // text strings in gputrace.cpp/traceFileTypeStr[]
    enum TraceFileType {
        FT_FIRST_FILE_TYPE = 0,
        FT_VERTEX_BUFFER = 0,
        FT_INDEX_BUFFER,
        FT_TEXTURE,
        FT_PUSHBUFFER,
        FT_SHADER_PROGRAM,
        FT_CONSTANT_BUFFER,
        FT_TEXTURE_HEADER,
        FT_TEXTURE_SAMPLER,
        FT_SHADER_THREAD_MEMORY,
        FT_SHADER_THREAD_STACK,
        FT_SEMAPHORE,
        FT_SEMAPHORE_16,        // fake type for 16B alignment
        FT_NOTIFIER,
        FT_STREAM_OUTPUT,
        FT_SELFGILD,
        FT_VP2_0,
        FT_VP2_1,
        FT_VP2_2,
        FT_VP2_3,
        FT_VP2_4,
        FT_VP2_5,
        FT_VP2_6,
        FT_VP2_7,
        FT_VP2_8,
        FT_VP2_9,
        FT_VP2_10,
        FT_VP2_14,
        FT_CIPHER_A,
        FT_CIPHER_B,
        FT_CIPHER_C,
        FT_GMEM_A,
        FT_GMEM_B,
        FT_GMEM_C,
        FT_GMEM_D,
        FT_GMEM_E,
        FT_GMEM_F,
        FT_GMEM_G,
        FT_GMEM_H,
        FT_GMEM_I,
        FT_GMEM_J,
        FT_GMEM_K,
        FT_GMEM_L,
        FT_GMEM_M,
        FT_GMEM_N,
        FT_GMEM_O,
        FT_GMEM_P,
        FT_LOD_STAT,
        FT_ZLWLL_RAM,
        FT_PMU_0,
        FT_PMU_1,
        FT_PMU_2,
        FT_PMU_3,
        FT_PMU_4,
        FT_PMU_5,
        FT_PMU_6,
        FT_PMU_7,
        FT_ALLOC_SURFACE,
        FT_NUM_FILE_TYPE
    };
    struct FileTypeData
    {
        const char * ArgName;
        const char * Description;
        UINT32       DefaultAlignment;
        const char * AllocTableTypeName;
    };
    enum ChannelManagedMode
    {
        CPU_MANAGED = 0x1,
        GPU_MANAGED = 0x2,
        CPU_GPU_MANAGED = CPU_MANAGED | GPU_MANAGED
    };
private:
    static FileTypeData s_FileTypeData[FT_NUM_FILE_TYPE];
public:

    static FileTypeData GetFileTypeData(TraceFileType type) { return s_FileTypeData[type]; }
    static FileTypeData GetFileTypeData(int type)           { return s_FileTypeData[type]; }

    //-----------------------------------
    GpuTrace(const ArgReader *params, Trace3DTest *test);
    ~GpuTrace();

    // read a trace header, remember all the interesting data
    RC ReadHeader(const char *filename, UINT32 timeoutMs);
    // clear out all the header data
    void ClearHeader();

    UINT64 GetUsageByFileType(TraceFileType type, bool excl_specified, LwRm::Handle hVASpace) const;
    Memory::Protect GetProtectByFileType(TraceFileType type) const;
    UINT32 GetFileTypeAlign(TraceFileType type) const;

    // We first allocate memory for the surfaces, which gives us the offsets to
    // do relocations.  We then do the relocations on the system memory copies
    // of each surface.  Then, we finally load each surface into the GPU.
    RC AllocateSurfaces(UINT32 width, UINT32 height);
    RC DoRelocations(GpuDevice *pGpuDev);
    RC DoRelocations2SubDev(TraceModule *pMod, UINT32 subdev);
    RC LoadSurfaces();
    RC UpdateFiles(UINT32 PbOffset);
    bool ReadTrace();
    RC DoVP2TilingLayout();

    // This is the actual playback.
    RC RunTraceOps();

    void AddSelfgildState(SelfgildState* sg) { m_Selfgilds.push_back(sg);}

    UINT32 GetVersion() const { return m_Version; }

    bool HasDynamicTextures() const { return m_DynamicTextures; }
    bool HasFileUpdates() const { return m_HasFileUpdates; }
    bool HasDynamicModules() const { return m_HasDynamicModules; }
    bool HasRefCheckModules() const { return m_HasRefCheckModules; }

    ModuleIter      ModBegin(bool bIncludeGeneric = true,
                             bool bIncludePeer    = false) const;
    ModuleIter      ModEnd(bool bIncludeGeneric = true,
                           bool bIncludePeer    = false) const;
    TraceModule *   ModLast() const;
    TraceModule *   ModFind(const char *filename) const;
    TraceModule *   ModFindTexIndex(UINT32 index) const;
    TraceModule *   FindModuleBySurfaceType(SurfaceType surfaceType) const;

    const SelfgildStates &GetSelfgildStates() { return m_Selfgilds; }

    const RGBAFloat &GetColorClear() { return m_ColorClear; }
    const ZFloat &GetZetaClear() { return m_ZetaClear; }
    const Stencil &GetStencilClear() { return m_StencilClear; }

    UINT32 GetWidth() { return m_Width; }
    UINT32 GetHeight() { return m_Height; }

    SurfaceType FindSurface(const string& surf) const;
    bool BufferLwlled(const string& filename) const;
    void AddZLwllId(UINT32 id) { m_ZLwllIds.insert(id); }
    const set<UINT32>& GetZLwllIds() const { return m_ZLwllIds; }
    bool DmaCheckCeRequested() const { return m_DmaCheckCeRequested; }

    const TextureHeaderState* GetTextureHeaderState (UINT32 index) const;

    bool DumpSurfaceRequested() const;
    bool DumpBufRequested() const;
    ImageList_t* GetDumpImageEveryBeginList() { return &m_DumpImageEveryBeginList; }
    ImageList_t* GetDumpImageEveryEndList()   { return &m_DumpImageEveryEndList; }
    ImageList_t* GetDumpZlwllList()           { return &m_DumpZlwllList; }
    TraceModules* GetDumpBufferList()         { return &m_DumpBufferList; }
    TraceModules* GetDumpBufferEndList()      { return &m_DumpBufferEndList; }
    DumpBufferMaps_t* GetDumpBufferBeginMaps(){ return &m_DumpBufferBeginMaps; }
    DumpBufferMaps_t* GetDumpBufferEndMaps()  { return &m_DumpBufferEndMaps; }
    DumpBufferMaps_t::iterator* GetDumpBufferBeginMapsIter(){ return &m_DumpBufferBeginMapsIter; }
    DumpBufferMaps_t::iterator* GetDumpBufferEndMapsIter()  { return &m_DumpBufferEndMapsIter; }

    bool GetClearInit() const { return m_HasClearInit; }
    void SetClearInit(bool b) { m_HasClearInit = b; }
    RC CreateBufferDumper();
    BufferDumper* GetDumper() const { return m_Dumper; }

    void AddTraceModule( TraceModule * pTraceModule );
    const TraceOps & GetTraceOps( void ) const
    {
        return m_TraceOps;
    }
    UINT32 GetTraceOpLineNumber(const TraceOp* pOp) const
    {
        TraceOpLineNos::const_iterator it = m_TraceOpLineNos.find(pOp);
        return it == m_TraceOpLineNos.end() ? 0 : it->second;
    }

    UINT32 GetLineNumber() const { return m_LineNumber; }
    bool ContainExelwtionDependency() const { return m_ContainExelwtionDependency; }
    TraceOp* GetTraceOp(UINT32 traceOpId);

    LWGpuSurfaceMgr::SurfaceTypeMap *GetSurfaceTypeMap() { return &m_SurfaceTypeMap; }

    TraceFileMgr *GetTraceFileMgr() { return m_TraceFileMgr; }

    // Used to modify SPH for shader files
    typedef map<string, UINT32> Attr2ValMap; // (argname, value)
    typedef map<string, Attr2ValMap> File2SphMap; // (filename, map)
    const File2SphMap& GetSphInfo() { return m_SphInfo; }
    RC ProcessSphArgs();

    void ReleaseAllCachedSurfs();
    DmaLoader* GetDmaLoader(TraceChannel *channel);

    RC EnableColorSurfaces(IGpuSurfaceMgr *surfaceManager);

    RC UpdateTrapHandler();
    GenericTraceModule* GetTrapHandlerModule(LwRm::Handle hVASpace);

    RC AddVirtualAllocations();

    // For unit test
    void UTAddSingleCommand(const string &command);
    void UTAddModule(GenericTraceModule *module);
    GenericTraceModule *UTAddModule(string name, GpuTrace::TraceFileType type, size_t size);
    void UTAddToken(const char *token);

    void UTClearModules();
    void UTClearTokens();

    RC ExelwteT3dCommands(const char* commands);

    bool GetSkipBufferDownloads() { return m_SkipBufferDownloads; }
    void SetSkipBufferDownloads(bool skip) { m_SkipBufferDownloads = skip; }
    void ClearDmaLoaders();
    UINT32 GetSyncPmTriggerPairNum() const { return m_numSyncPmTriggers; }
    ///////////////////////////////////////////////////////////////
    // static/dynamic tsg
    TraceTsg* GetTraceTsg(const string &tsgName);
    RC AddTraceTsg(const string &tsgName,
                   const shared_ptr<VaSpace> &pVaSpace, bool isShared);

private:
    //-----------------------------------
    friend class TraceModule;        // ugh
    friend class GenericTraceModule; // ugh

    RC AddSubdevMaskDataMap(TraceModule *pMod);

    void SetTextureHeaderState(UINT32*);
    RC SetTextureHeaderPromotionField(UINT32* header, UINT32 size);
    RC SetTextureLayoutToPitch(UINT32* header, UINT32 size);

    RC MatchFileType(const char *name, GpuTrace::TraceFileType *pftype);

    static UINT32 ClassStr2Class(const char *ClassStr, bool sli);

    RC SetupTextureParams();
    RC SetClassParam(UINT32 clsssnum, TraceChannel *pTraceChannel,
                     TraceSubChannel *pTraceSubChannel);

    RC ZlwllRamSanityCheck() const;

    void MoveModuleToBegin(GpuTrace::TraceFileType);

    Trace3DTest *m_Test;
    bool m_HasVersion;
    UINT32 m_Version;

    //! Buffer/surface dumping members
    bool             m_DumpImageEveryBegin;
    bool             m_DumpImageEveryEnd;
    bool             m_LogInfoEveryEnd;
    bool             m_SingleMethod;
    bool             m_DumpZlwllEveryEnd;

    //! List of surface/zlwll buffers that need to dump
    ImageList_t      m_DumpImageEveryBeginList;
    ImageList_t      m_DumpImageEveryEndList;
    ImageList_t      m_DumpZlwllList;

    // List of dmabuffers (associated tracemodeuls) which need to dump
    TraceModules     m_DumpBufferList;
    TraceModules     m_DumpBufferEndList;
    // nth begin/end to the tracemodule/dmabuffer mapping
    DumpBufferMaps_t m_DumpBufferBeginMaps;
    DumpBufferMaps_t m_DumpBufferEndMaps;
    DumpBufferMaps_t::iterator m_DumpBufferBeginMapsIter;
    DumpBufferMaps_t::iterator m_DumpBufferEndMapsIter;

    // Map of buffer dumping on events
    DumpBufferMapOnEvent_t m_BuffersDumpOnEvent; // Map for event and buffer
    void ProcessEventDumpArgs();

    RC  ProcessBufDumpArgs();

    bool m_DynamicTextures;
    bool m_HasFileUpdates;
    bool m_HasDynamicModules;
    bool m_HasRefCheckModules;
    bool m_ZlwllRamPresent;

    bool m_IgnoreTsg;
    bool m_DmaCheckCeRequested;

    // Holds the open TarArchive and trace path, handles file searches, etc.
    TraceFileMgr *m_TraceFileMgr;

    // test.hdr file data during parsing (move this out to TraceParser later).
    char *              m_TestHdrBuf;
    size_t              m_TestHdrBufSize;
    deque<const char*>  m_TestHdrTokens;
    deque<const char*>  m_InlineHdrTokens;
    UINT32              m_LineNumber;
    UINT32              m_TimeoutMs;
    bool                m_ContainExelwtionDependency;

    RC ParseTestHdr (const char * fname);
    void InsertMethodSendOps();
    void InsertOp(UINT32 seqNum, TraceOp* pOp, UINT32 opOffset);
    RC RetriveZblwINT32Field(const char* tok, const char* fieldName, UINT32 * fieldVal);
    RC RetriveZbcStringField(const char* tok, const char* fieldName, string * fieldVal);
    RC RequireVersion(UINT32 min, const char * feature);
    RC RequireNotVersion(UINT32 badver, const char * cmd);
    RC RequireModLast(const char * cmd);
    RC RequireModLocal(TraceModule *pMod, const char * cmd);

    RC ParseBoolFunc(int * pBoolFunc);
    RC ParseRelocPeer(string surfname, UINT32 *pPeerNum, UINT32* pPeerID);
    RC ParseRelocPeer(SurfaceType st, UINT32 *pPeer);
    RC ParseChannel(TraceChannel **ppTraceChan, ChannelManagedMode mode = CPU_GPU_MANAGED);
    RC ParseSubChannel(TraceChannel **ppTraceChan,
        TraceSubChannel **ppTraceSubChan);
    RC ParseFileName(string * pstr, const char* cmd);
    RC ParseFileType(TraceFileType * pTraceFileType);
    RC ParseModule(TraceModule ** ppTraceModule);
    RC ParseFileOrSurfaceOrFileType(string * pstr);
    RC ParseString(string * pstr);
    RC ParseSurfType(SurfaceType * pSurfaceType);
    RC ParseGeometry(vector<UINT32>*);
    RC ParseOffsetLong(pair<UINT32, UINT32>*);
    RC ParseUINT32(UINT32 * pUINT32);
    RC ParseUINT64(UINT64 * pUINT64);
    bool TestPredicate(const char *tok);

    RC ParseCFG_COMPARE();
    RC ParseCFG_WRITE();
    RC ParseCHANNEL(bool isAllocCommand);
    RC ParseSUBCHANNEL(bool isAllocCommand);
    RC ParseFREE_CHANNEL();
    RC ParseFREE_SUBCHANNEL();
    RC ParseCLASS();
    RC ParseCLEAR();
    RC ParseCLEAR_FLOAT_INT(const char * cmd);
    RC ParseCLEAR_INIT();
    RC ParseESCAPE_COMPARE();
    RC ParseESCAPE_WRITE();
    RC ParseESCAPE_WRITE_FILE();
    RC ParseFILE(UINT32 DebugModuleSize, TraceSource Src);
    RC ParseALLOC_SURFACE();
    RC ParseSURFACE_VIEW();
    RC ParseALLOC_VIRTUAL();
    RC ParseALLOC_PHYSICAL();
    RC ParseMAP();
    RC ParseFREE_SURFACE();
    RC ParseInlineFileData(GenericTraceModule * pTraceModule);
    RC ParseConstantData(TraceModule * pTraceModule);
    RC ParseFLUSH();
    RC ParseGPENTRY();
    RC ParseGPENTRY_CONTINUE();
    RC ParseMETHODS();
    RC ParsePRI_COMPARE();
    RC ParsePRI_WRITE();
    RC ParseReg(UINT32 * pSubdevMask, string * pReg, string * pField,
                UINT32 * pMask, const char * feature);
    RC ParseRELOC();
    RC ParseRELOC40();
    RC ParseRELOC64();
    RC ParseRELOC_ACTIVE_REGION();
    RC ParseRELOC_BRANCH();
    RC ParseRELOC_CTXDMA_HANDLE();
    RC ParseRELOC_D();
    RC ParseRELOC_FREEZE();
    RC ParseRELOC_MS32();
    RC ParseRELOC_RESET_METHOD();
    RC ParseRELOC_SIZE();
    RC ParseRELOC_SIZE_LONG();
    RC ParseRELOC_TYPE();
    RC ParseRELOC_VP2_PITCH();
    RC ParseRELOC_VP2_TILEMODE();
    RC ParseRELOC_SURFACE();
    RC ParseRELOC_SURFACE_PROPERTY();
    RC ParseRELOC_LONG();
    RC ParseRELOC_CLEAR();
    RC ParseRELOC_ZLWLL();
    RC ParseRELOC_SCALE();
    RC ParseRUNLIST();
    RC ParseSIZE();
    RC ParseTIMESLICE();
    RC ParseTRUSTED_CTX();
    RC ParseUPDATE_FILE();
    RC ParseVERSION();
    RC ParseWAIT_FOR_IDLE();
    RC ParseWAIT_FOR_VALUE32();
    RC ParseRELOC_CONST32();
    RC ParseSEC_KEY();
    RC ParsePMU_CMD();
    RC ParseSYSMEM_CACHE_CTRL();
    RC ParseFLUSH_CACHE_CTRL();
    RC ParseWAIT_PMU_CMD();
    RC ParsePmuCmdId(const char *cmd, UINT32 *pCmdId);
    RC ParseENABLE_CHANNEL();
    RC ParseDISABLE_CHANNEL();
    RC ParsePMTriggerEvent();
    RC ParsePMTriggerSyncEvent();
    RC ParseEvent();
    RC ParseSyncEvent();
    RC ParseTIMESLICEGROUP();
    RC ParseCHECK_DYNAMICSURFACE();
    RC ParseTRACE_OPTIONS();
    RC ParsePROGRAM_ZBC_COLOR();
    RC ParsePROGRAM_ZBC_DEPTH();
    RC ParsePROGRAM_ZBC_STENCIL();
    RC ParseSUBCONTEXT();
    RC ParseADDRESS_SPACE();
    RC ParseDEPENDENCY();
    RC ParseUTL_SCRIPT();
    RC ParseUTL_RUN();
    RC ParseUTL_RUN_BEGIN();

    MdiagSurf* GetZLwllStorage(UINT32 index, LwRm::Handle hVASpace);
    RC CreateTrapHandlers(UINT32 hwClass);
    void CreateTrapHandlerForVAS(shared_ptr<VaSpace> pVa, UINT08* data, UINT32 size);

    RC ParseMemoryCommandProperties(PropertyUsageMap *propertyUsed,
        MemoryCommandProperties *options, UINT32 commandFlag);

    RC ProcessPropertyCommandLineArguments(PropertyUsageMap *propertyUsed,
        MemoryCommandProperties *properties);

    RC InitSurfaceFromProperties(const string &name, MdiagSurf *surface,
        SurfaceType surfaceType, const MemoryCommandProperties &properties,
        PropertyUsageMap *propertyUsed);

    void InitMemoryCommandPropertyMap();
    void InitZbcClearFmtMap();

    RC SetSurfaceAAMode(MdiagSurf *surface, const string &mode);
    RC SetSurfaceLayout(MdiagSurf *surface, const string &layout);
    RC SetSurfaceCompression(MdiagSurf *surface, const string &compression);
    RC SetSurfaceZlwll(MdiagSurf *surface, const string &zlwll);
    RC SetSurfaceAperture(MdiagSurf *surface, const string &aperture);
    RC SetSurfaceType(MdiagSurf *surface, const string &type);
    RC SetSurfaceProtect(MdiagSurf *surface, const string &protect);
    RC SetSurfaceShaderProtect(MdiagSurf *surface, const string &shaderProtect);
    RC SetSurfacePrivileged(MdiagSurf *surface, const string &privileged);
    RC SetSurfaceGpuCache(MdiagSurf *surface, const string &value);
    RC SetSurfaceP2PGpuCache(MdiagSurf *surface, const string &value);
    RC SetSurfaceZbc(MdiagSurf *surface, const string &value);
    RC SetSurfacePageSize(MdiagSurf *surface, const string &pageSize);
    RC SetSurfacePhysPageSize(MdiagSurf *surface, const string &pageSize);
    RC SetSurfaceMapMode(MdiagSurf *surface, const string &mapMode);
    RC SetSurfaceFbSpeed(MdiagSurf *surface, const string &fbSpeed);
    RC SetSurfaceVpr(MdiagSurf *surface, const string &value);
    RC SetSurfaceUpr(MdiagSurf *surface, const string &value);
    RC SetSurfaceAtsReadPermission(MdiagSurf *surface, const string &value);
    RC SetSurfaceAtsWritePermission(MdiagSurf *surface, const string &value);
    RC SetSurfaceAcr(MdiagSurf *surface, const string &value, UINT32 index);
    RC SetSurfaceFlaImportMem(MdiagSurf *surface, const string &name);
    RC SetSurfaceInheritPteKind(MdiagSurf *surface, const string &kind);
    RC ProcessGlobalArguments(MdiagSurf *surface, SurfaceType surfaceType);
    RC GetAllocSurfaceParameters(MdiagSurf *surface, const string &name);
    RC ProcessArgumentAliases(MdiagSurf *surface, const string &name);
    void InitTagToBufferTypeMap();

    RC GetMemoryCommandFileSize(
        const MemoryCommandProperties &properties,
        PropertyUsageMap *propertyUsed, size_t *fileSize);

    RC SetMemoryModuleProperties(SurfaceTraceModule *module,
        MdiagSurf *surface, SurfaceType surfaceType,
        const MemoryCommandProperties &properties,
        PropertyUsageMap *propertyUsed);

    RC GetPageSizeFromStr(const string &pageSizeStr, UINT32 *pageSizeNum) const;

    RC AddInternalSyncEvent();

    std::map<TraceChannel*, DmaLoader*> m_DmaLoaders;
    std::map<TraceChannel*, UINT32> m_NumGpEntries;

    TraceModules m_Modules;
    TraceModules m_GenericModules;
    TraceModules m_PeerModules;

    SelfgildStates m_Selfgilds;
    vector<TextureHeaderState> m_TextureHeaders;

    const ArgReader *params;

    RGBAFloat m_ColorClear;
    ZFloat m_ZetaClear;
    Stencil m_StencilClear;

    UINT32 m_Width;
    UINT32 m_Height;

    set<string> m_LwlledBuffers;
    set<UINT32> m_ZLwllIds;

    typedef map<UINT32, MdiagSurf*> ZLwllStorage;
    ZLwllStorage m_ZLwllStorage;

    bool m_HasClearInit;

    // Sequencing:
    //
    // In trace versions 0, 1, and 2, sequencing is as follows:
    //  a) channel, object, and surfaces are allocated
    //  b) initial values for all surfaces copied in from files,
    //     backdoor C/Z clears done
    //  c) set-object and trace_3d generated init methods sent
    //  d) HW-clear methods for C/Z sent (if not backdoor cleared)
    //  e) methods from test.dma are sent to the channel,
    //     e.1) possibly intermixed with WAIT_FOR_IDLE, UPDATE_FILE, and GPENTRY
    //          operations specified by the offset in the method stream at
    //          which to send them
    //  f) notifier methods added to pushbuffer by trace_3d
    //  e) flush and wait for idle
    //  g) readback surfaces and CRC check
    //
    //
    // In trace version 3, this is the same except that 2+ channels are allowed
    // so we loop the c,d,e,f steps, then add the notifier methods and flush
    // all the channels at once.
    // Since pushbuffer offsets are no longer unambiguous sequence points
    // (offset in which channel?) v3 disallows WAIT_FOR_IDLE, UPDATE_FILE, and
    // GPENTRY entirely.
    //
    // Note that autoflush is on in the channels by default, so we don't
    // get real simultanious flushes unless the -single_kick arg is used.
    //
    //
    // In trace version 4, the user specifies the sequencing within step (e)
    // so that WAIT_FOR_IDLE, UPDATE_FILE, GPENTRY, and a couple new operations
    // can be used again.
    // Steps a, b, c, d, f, and g are still done in fixed sequence.
    //
    //
    // Someday, (v5?) we can add dynamic surface alloc/free operations, but
    // that will be a big, scary change.
    //
    //
    // We aren't allowed to drop support for versions 1 to 3, so we will
    // internally colwert such traces to version 4 (sort of) so that the
    // runtime playback is the same, even if the parsing is different.

    // List of the TraceOps to be exelwted, filled in by ParseTestHdr.
    // Sorted by the TraceOp's sequence number (i.e. line number in v4+, or
    // pushbuffer offset in v0 to v3).
    TraceOps m_TraceOps;
    TraceOpLineNos m_TraceOpLineNos;
    UserDefTraceOpIdMap m_UserDefTraceOpIdMap;

    // Used by GpuTrace::InsertOp, these help insure correct sequencing of
    // pre-v4 traces in the m_TraceOps container.
    enum
    {
        OP_SEQUENCE_SHIFT = 2,

        OP_SEQUENCE_WAIT_FOR_IDLE = 0,
        OP_SEQUENCE_UPDATE_FILE   = 1,
        OP_SEQUENCE_GPENTRY       = 2,
        OP_SEQUENCE_METHODS       = 3,
        OP_SEQUENCE_OTHER         = 3
    };

    RC SetupVP2Params();

    RC SetCeType(TraceChannel *pTraceChannel, TraceSubChannel *pTraceSubChannel,
                 UINT32 ceType, GpuSubdevice *subdev);

    UINT32 m_LwEncOffset;
    UINT32 m_LwDecOffset;
    UINT32 m_LwJpgOffset;

    RC GetEnginesList(TraceSubChannel *pTraceSubChannel, vector<UINT32>* engineList);
    RC SetLwDecOffset(TraceSubChannel *pTraceSubChannel);
    RC SetLwEncOffset(TraceSubChannel *pTraceSubChannel);
    RC SetLwJpgOffset(TraceSubChannel *pTraceSubChannel);

    BufferDumper *m_Dumper;

    LWGpuSurfaceMgr::SurfaceTypeMap m_SurfaceTypeMap;

    // Used to track any traceOps which can create a PMU Command that are given
    // an explicit command ID.  Only those TraceOps with a specific command
    // ID should be put in this list
    vector<TraceOpPmu *> m_PmuCmdTraceOps;
    TraceOpPmu *FindPmuOp(UINT32 cmdId);

    struct StrPropertyValue
    {
        string value;
        bool isUsed;

        StrPropertyValue() : isUsed(false) {};
    };

    struct IntPropertyValue
    {
        UINT32 value;
        bool isUsed;

        IntPropertyValue() : isUsed(false) {};
    };

    struct TogglePropertyValue
    {
        bool value;
        bool isUsed;

        TogglePropertyValue() : isUsed(false) {};
    };

    // Used to modify SPH for shader file
    File2SphMap m_SphInfo;

    // Last inserted TraceOpSendGpEntry
    TraceOpSendGpEntry* m_LastGpEntryOp;

    vector<GenericTraceModule*> m_TrapHandlerModules;

    MemoryCommandPropertyMap m_MemoryCommandPropertyMap;
    ZbcClearFmtMap m_ZbcClearFmtMap;

    UINT32 m_numPmTriggers;
    UINT32 m_numSyncPmTriggers;

    bool m_HaveSyncEventOp;

    UINT32 m_ExtraVirtualAllocations;

    // A map of special ALLOC_SURFACE TAG names to
    // buffer type names.
    TagToBufferTypeMap m_TagToBufferTypeMap;

    // Friend unit test classes
    // friend class UTParseSingleCommand;
    friend class CaseParseSingleCommand;
    friend class CaseParseRELOC_SIZE;
    friend class CaseParseUINT32;
    friend class CaseParseUINT64;

    bool HackDummyVEID0();  // temporary solution for bug 200115737

public:
    // List of TraceChannels used in header file parsing stage
    typedef list<TraceChannel> TraceChannelObjects;
    // List of TraceTsg used in header file parsing stage
    typedef list<TraceTsg> TraceTsgObjects;

    TraceChannelObjects* GetChannels() { return &m_TraceChannelObjects; }
protected:
    ///////////////////////////////////////////////////////////////
    // cpu/gpu mananged, static/dynamic channels
    RC AddChannel(const string &chname,
                  const string &tsgname,
                  ChannelManagedMode mode,
                  const shared_ptr<VaSpace> &pVaSpace,
                  const shared_ptr<SubCtx> &pSubCtx);
    TraceChannel* GetChannel(const string &chname,
                             ChannelManagedMode mode = CPU_GPU_MANAGED);
    TraceChannel* GetLwrrentChannel(ChannelManagedMode mode = CPU_GPU_MANAGED);
    TraceChannel* GetLastChannelByVASpace(LwRm::Handle hVASpace);
    TraceChannel* GetGrChannel(ChannelManagedMode mode = CPU_GPU_MANAGED);
    TraceChannel* GetChannelHasSubch(const string &name,
                                     ChannelManagedMode mode = CPU_GPU_MANAGED);
    void  GetComputeChannels(vector<TraceChannel*>& compute_channels,
                             ChannelManagedMode mode = CPU_GPU_MANAGED);

    TraceSubChannel* GetSubChannel(TraceChannel* pTraceChannel,
                                   const string& subChannelName);

    RC DoParseTestHdr();

private:
    TraceTsgObjects m_TraceTsgObjects;
    TraceChannelObjects m_TraceChannelObjects;
    LwRm::Handle GetVASHandle(const string &name);
    RC SanityCheckTSG(shared_ptr<SubCtx> pSubCtx, string * pTsgName);
    shared_ptr<VaSpace> GetVAS(const string& name);
    shared_ptr<SubCtx> GetSubCtx(const string& name);
    RC CreateCommandLineResource();
    bool m_SkipBufferDownloads;
    set<string> m_TraceVasNames;

    unique_ptr<ParseFile> m_pParseFile;
    unique_ptr<WaterMarkImpl> m_pWatermarkImpl;

private:
    void IncrementTraceChannelGpEntries(TraceChannel* pTraceChannel);
    void UpdateTraceChannelMaxGpEntries(TraceChannel* pTraceChannel);
    void UpdateAllTraceChannelMaxGpEntries();
    RC ConfigureWatermark();
    RC SetWatermark(shared_ptr<SubCtx> pSubctx);
    const WaterMarkImpl * GetWatermarkImpl() { return m_pWatermarkImpl.get(); }
    RC ConfigureSubCtxPETable(shared_ptr<SubCtx>);

    RC AddTraceOpDependency(const string & traceOpName, const string & dependentTraceOpName);
    
    UINT32 GetUserDefTraceOpId(const string & traceOpName) const
    {
        UserDefTraceOpIdMap::const_iterator it = m_UserDefTraceOpIdMap.find(traceOpName);
        return it == m_UserDefTraceOpIdMap.end() ? 0 : it->second;
    }

    void InitTraceOpsStatus();
    // Loop trace ops in specified closed interval [beginTraceOpIdx, endTraceOpIdx].
    // And setting the traceop attributes according to the input func
    void ForEachTraceOps(UINT32 beginTraceOpIdx, UINT32 endTraceOpIdx, 
            std::function<void(TraceOp *)> func);
    void TriggerTraceOpEvent(TraceOp* op, const TraceOpTriggerPoint point);
};
#endif

