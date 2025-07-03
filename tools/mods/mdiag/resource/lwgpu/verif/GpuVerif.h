/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2016, 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_GPUVERIF_H
#define INCLUDED_GPUVERIF_H

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#ifndef INCLUDED_STL_LIST
#include <list>
#define INCLUDED_STL_LIST
#endif

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

#ifndef INCLUDED_STL_SET
#include <set>
#define INCLUDED_STL_SET
#endif

#include <sys/stat.h>

#include "mdiag/utils/buf.h"
#include "mdiag/utils/crc.h"
#include "gpu/utility/blocklin.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/tracefil.h"
#include "mdiag/tests/test_state_report.h"

#include "mdiag/utils/ICrcProfile.h"
#include "mdiag/utils/IGpuSurfaceMgr.h"
#include "mdiag/utils/ITestStateReport.h"
#include "sim/IChip.h"

#include "mdiag/resource/lwgpu/dmard.h"
#include "mdiag/resource/lwgpu/crcchain.h"
#include "mdiag/resource/lwgpu/surfasm.h"
//#include "GpuVerif.h"

#include "mdiag/tests/gpu/trace_3d/selfgild.h"

#include "DmaUtils.h"
#include "DumpUtils.h"
#include "SurfBufInfo.h"

class TraceChannel; // avoid cirlwlar include

typedef list<string> CrcChain;
typedef set<const MdiagSurf*> SurfaceSet;
typedef vector<SelfgildState*> SelfgildStates;
typedef vector<SelfgildStrategy*> SelfgildStrategies;
typedef CrcEnums::CRC_STATUS CrcStatus;

enum VerifCheckerType
{
    CT_CRC,
    CT_Reference,
    CT_Interrupts,
    CT_LwlCounters,
    NUM_CHECKERS
};

struct ICheckInfo
{
    ICheckInfo(ITestStateReport* report)
        : Report(report)
    {
    }

    ITestStateReport* Report;
};

//class GpuVerif;
struct CrcRect
{
    UINT32 x;
    UINT32 y;
    UINT32 width;
    UINT32 height;

    CrcRect() : x(0), y(0), width(0), height(0) {}
    CrcRect &operator=(const CrcRect &rect)
    {
        x      = rect.x;
        y      = rect.y;
        width  = rect.width;
        height = rect.height;
        return *this;
    }
};

class IVerifChecker
{
public:
    virtual ~IVerifChecker() {}
    virtual void Setup(GpuVerif* verifier) = 0;
    virtual TestEnums::TEST_STATUS Check(ICheckInfo* info) = 0;
};

class ICrcVerifier: public IVerifChecker
{
public:
    // Read stored crcs and compare with measured crc.
    // Return true if caller should dump image.
    // Implemented by CRC, Reference, and Selfgild checkers.
    virtual bool Verify
    (
        const char *key, // Key used to find stored crcs
        UINT32 measuredCrc, // Measured crc
        SurfaceInfo* surfInfo,
        UINT32 gpuSubdevice,
        ITestStateReport *report, // File to touch in case of error
        CrcStatus *crcStatus // Result of comparison
    ) = 0;

    virtual UINT32 CallwlateCrc(const string& checkName, UINT32 subdev,
            const SurfaceAssembler::Rectangle *rect) = 0;
};

class GpuVerif
{
public:
    // Regular
    GpuVerif( LwRm* pLwRm,
              SmcEngine* pSmcEngine,
              LWGpuResource *lwgpu,
              IGpuSurfaceMgr *surfMgr,
              const ArgReader *params,
              UINT32 traceArch,
              ICrcProfile* profile,
              ICrcProfile* goldProfile,
              TraceChannel* channel,
              TraceFileMgr* traceFileMgr,
              bool dmaCheck);

    // Selfgild
    GpuVerif
    (
        SelfgildStates::const_iterator begin,
        SelfgildStates::const_iterator end,
        LwRm* pLwRm,
        SmcEngine* pSmcEngine,
        LWGpuResource *in_lwgpu,
        IGpuSurfaceMgr *surfMgr,
        const ArgReader *params,
        UINT32 trcArch,
        ICrcProfile* profile,
        ICrcProfile* goldProfile,
        TraceChannel* channel,
        TraceFileMgr* traceFileMgr,
        bool dmaCheck
    );

    ~GpuVerif();

private:
    void Initialize
    (
        LwRm* pLwRm,
        SmcEngine* pSmcEngine,
        LWGpuResource *lwgpu,
        IGpuSurfaceMgr *surfMgr,
        const ArgReader *params,
        UINT32 traceArch,
        ICrcProfile* profile,
        ICrcProfile* goldProfile,
        TraceChannel* channel,
        TraceFileMgr* traceFileMgr,
        bool dmaCheck
    );

    GpuVerif(const GpuVerif& other);

public:
    bool Setup(BufferConfig* config);

    const CrcChain* GetCrcChain() const
    {
        return &m_CrcChain->GetCrcChain();
    }

    void SetCrcChainManager(const ArgReader* params,
        GpuSubdevice* pSubdev, UINT32 defaultClass);

    bool DumpActiveSurface(MdiagSurf* surface, const char* fname, UINT32 subdev = 0);

    inline UINT32 ChecksPerformed() const { return m_ChecksPerformed; }

    RC AddDmaBuffer(MdiagSurf *pDmaBuf, CHECK_METHOD check, const char *Filename,
                            UINT64 Offset, UINT64 Size, UINT32 Granularity,
                            const VP2TilingParams *pTilingParams,
                            BlockLinear* pBLockLinear, UINT32 Width, UINT32 Height, bool CrcMatch);

    void AddSurfC(MdiagSurf* surf, int idx, const CrcRect &rect);
    void AddSurfZ(MdiagSurf* surf, const CrcRect &rect);
    RC AddAllocSurface(MdiagSurf* surface, const string &crcName,
        CHECK_METHOD check, UINT64 offset, UINT64 size, UINT32 granularity,
        bool crcMatch, const CrcRect &rect, bool rawCrc, UINT32 classNum);
    RC RemoveAllocSurface(const MdiagSurf *surface);

    RC DumpSurfOrBuf(MdiagSurf *surfOrBuf, string &fname, UINT32 gpuSubdevice);

    TestEnums::TEST_STATUS DoCrcCheck(ITestStateReport *report,
                                      UINT32 gpuSubdevice,
                                      const SurfaceSet *skipSurfaces);

    // called by functions outside gpuverif to do crc check on specified dynamic surface
    TestEnums::TEST_STATUS DoSurfaceCrcCheck(ITestStateReport* report,
        const string& checkName, const string& surfaceSuffix,
        UINT32 gpuSubdevice);

    TestEnums::TEST_STATUS DoIntrCheck(ITestStateReport *report);

    TestEnums::TEST_STATUS CheckIntrPassFail();

    IVerifChecker* GetCrcChecker(VerifCheckerType type)
    {
        MASSERT(type < NUM_CHECKERS);
        return m_checkers[type].get();
    }

    // Facade methods

    // Use RegisterChecker carefully.
    // Client code may new a checker and pass to this RegisterChecker(); but won't
    // delete this checker in client code, because GpuVerif will delete it later.
    void RegisterChecker(VerifCheckerType type, IVerifChecker* checker);

    // CRC callwlation capability
    UINT32 CallwlateCrc(const string& checkName, UINT32 subdev,
            const SurfaceAssembler::Rectangle *rect);
    
    void SetPreCrcValue(UINT32 preValue) { m_PreCrcValue = preValue; }
    UINT32 GetPreCrcValue() const { return m_PreCrcValue; }
public:
    // internal methods for the subsystem
    const ArgReader* Params() const { return m_params; };
    UINT32 TraceArchClass() const { return m_class; }
    LWGpuResource* LWGpu() { return m_lwgpu; }

    IGpuSurfaceMgr* SurfaceManager() { return m_surfMgr; }
    CrcChainManager* CrcChain_Manager() { return m_CrcChain; }
    TraceChannel* Channel() { return m_Channel; }
    TraceFileMgr* GetTraceFileMgr() { return m_traceFileMgr; }

    vector<SurfInfo2*>& Surfaces() { return m_surfaces; }
    SurfInfo2* SurfZ() { return m_surfZ; }
    vector<AllocSurfaceInfo2*>& AllocSurfaces() { return m_AllocSurfaces; }
    vector<DmaBufferInfo2*>& DmaBufferInfos() { return m_DmaBufferInfos; }
    vector<SurfaceInfo*>& AllSurfaces() { return m_allSurfaces; }

    bool ZSurfOK() const;

    CrcEnums::CRC_MODE CrcMode() { return m_crcMode; }
    _RAWMODE& RawImageMode() { return m_rawImageMode; }

    ICrcProfile* Profile() { return m_profile; }
    ICrcProfile* GoldProfile() { return m_goldProfile; }

    const string& GetGpuKey() const { return m_GpuKey; }
    void SetGpuKey(const string &gpuKey) { m_GpuKey = gpuKey; }

    void OpenReportFile();
    void CloseReportFile();

    bool ReportExists() const { return m_reportFile != NULL; }
    INT32 WriteToReport(const char * Format, ...);
    const string& OutFilename() const { return m_outFilename; }

    DmaUtils*   GetDmaUtils () { return m_DmaUtils;  }
    DumpUtils*  GetDumpUtils() { return m_DumpUtils; }

    SelfgildStrategies& GetSelfgildStrategies() { return m_Strategies; }
    void IncrementCheckCounter() { ++m_ChecksPerformed; }

    RC FlushGpuCache(UINT32 gpuSubdevice);
    LwRm* GetLwRmPtr() { return m_pLwRm; };
    SmcEngine* GetSmcEngine() { return m_pSmcEngine; };
    bool NeedDmaCheck() const { return m_DmaCheck; }
private:
    unique_ptr<IVerifChecker> m_checkers[NUM_CHECKERS];

    LwRm*               m_pLwRm;
    SmcEngine*          m_pSmcEngine;
    IGpuSurfaceMgr*     m_surfMgr;
    LWGpuResource*      m_lwgpu;
    std::string         m_outFilename;
    FILE*               m_reportFile;
    TraceFileMgr*       m_traceFileMgr;
    bool                m_DmaCheck;
    const ArgReader*    m_params;

    ICrcProfile *       m_profile;
    ICrcProfile *       m_goldProfile;

    TraceChannel*       m_Channel;
    UINT32              m_class;

    vector<SurfInfo2*>          m_surfaces;
    SurfInfo2*                  m_surfZ;
    vector<AllocSurfaceInfo2*>  m_AllocSurfaces;
    vector<DmaBufferInfo2*>     m_DmaBufferInfos;
    vector<SurfaceInfo*> m_allSurfaces; // all xxInfos linked together

    CrcChainManager*    m_CrcChain;

    // Stores the first gpu key used for a profile key string.
    // This is used to check for partial CRC chaining.
    string m_GpuKey;

    DmaUtils*           m_DmaUtils;
    DumpUtils*          m_DumpUtils;

    _RAWMODE                m_rawImageMode;
    CrcEnums::CRC_MODE      m_crcMode;
    static map<string,int>  m_namesInUse;

    bool                m_ForceDimInCrcKey;
    UINT32              m_ChecksPerformed;

    SelfgildStrategies m_Strategies; // Selfgild
    UINT32 m_PreCrcValue = 0; 
};

#endif /* INCLUDED_GPUVERIFY_H */
