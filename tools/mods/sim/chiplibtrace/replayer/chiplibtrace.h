/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef TRACE_H
#define TRACE_H
#include <string>
#include <vector>
#include <map>
#include <set>
#include <utility>
#include <fstream>
#include "core/include/types.h"
#include "replayer.h"
#include "core/include/refparse.h"
#include "core/include/color.h"
#include "core/include/rc.h"
#include "core/include/tee.h"

namespace Replayer{
class ChiplibTrace;
struct SurfInfo
{
    std::string name;
    ColorUtils::Format format;
    UINT32 width;
    UINT32 height;
};
class TraceOp
{
public:
    TraceOp(ChiplibTrace* trace, const std::vector<const char*>& tokens,
            const std::string& comment, int lineNum);
    virtual ~TraceOp(){};
    virtual const std::string& GetOpStr()const {return m_OpStr;};
    virtual const std::string& GetComment()const {return m_Comment;}
    virtual int GetLineNum()const {return m_LineNum;}
    virtual RC Replay(const ReplayOptions&, int* replayCount){return OK;}
protected:
    bool LoadDumpFile(const string& fileName, vector<UINT08>* pData,
            UINT32 size, bool allowArchive);

    std::string m_OpStr;
    std::string m_Comment;
    ChiplibTrace* m_Trace;
    int m_LineNum;
};
class BusMemOp:public TraceOp
{
public:
    enum MEM_OPTYPE{
        READ,
        WRITE,
        ILWALID_OP
    };
    enum APERTURE {
        BAR0,
        BAR1,
        BAR2,
        BARINST,
        SYSMEM,
        CFGSPACE0,
        CFGSPACE1,
        CFGSPACE2,
        ILWALID_APERTURE
    };

public:
    BusMemOp(ChiplibTrace* trace, const std::vector<const char*>& tokens,
            MEM_OPTYPE type, const std::string& comment, int lineNum);
    BusMemOp(ChiplibTrace* trace, const std::vector<const char*>& tokens,
            const std::string& comment, int lineNum):
        TraceOp(trace, tokens, comment, lineNum), m_Type(READ) {};
    MEM_OPTYPE GetOpType()const { return m_Type;}
    const std::string& GetAperture()const {return m_Aperture;}
    UINT64 GetOffset()const  { return m_Offset;}
    const std::vector<UINT08>& GetData()const { return m_Data;}
    size_t GetDataSize()const {return m_Data.size();}
    virtual RC Replay(const ReplayOptions&, int* replayCount);
protected:
    int ReplayWrite(UINT64 addr, const void* data, UINT32 count );
    int ReplayRead(UINT64 addr, void* data, UINT32 count );

    bool IsMustMatchBusMemOp(const ReplayOptions& option) const;

protected:
    MEM_OPTYPE m_Type;
    std::string m_Aperture;
    UINT64 m_Offset = 0;
    std::vector<UINT08> m_Data;
};
class EscapeOp:public TraceOp
{
public:
    enum ESCAPE_OPTYPE{
        READ,
        WRITE,
        GPU_READ,
        GPU_WRITE,
        ILWALID_OP
    };

public:
    EscapeOp(ChiplibTrace* trace, const std::vector<const char*>& tokens,
            ESCAPE_OPTYPE type, const std::string& comment, int lineNum);
    virtual RC Replay(const ReplayOptions&, int* replayCount);

protected:
    int ReplayEscapeRead(UINT32 count, void* data);
    int ReplayEscapeWrite(UINT32 count, const void* data);

protected:
    ESCAPE_OPTYPE m_Type;
    UINT08 m_GpuId;
    std::string m_Key;
    UINT32 m_Index;
    UINT32 m_Size;
    std::vector<UINT08> m_Data;
};

class DumpRawMemOp : public TraceOp
{
public:
    DumpRawMemOp(ChiplibTrace* trace, const std::vector<const char*>& tokens,
            const std::string& comment, int lineNum);
    virtual RC Replay(const ReplayOptions&, int* replayCount);
    virtual RC DumpViaBar1(const ReplayOptions&, bool);
    virtual RC DumpViaPramin(const ReplayOptions&, bool);
    virtual RC DumpViaDirectPhysRd(const ReplayOptions&, bool);

protected:
    int CompareDumpFiles();

    std::string m_MemAperture;
    UINT64 m_PhysAddress;
    UINT32 m_Size;
    std::string m_FileName;
};

class LoadRawMemOp : public TraceOp
{
public:
    LoadRawMemOp(ChiplibTrace* trace, const std::vector<const char*>& tokens,
            const std::string& comment, int lineNum);
    virtual RC Replay(const ReplayOptions&, int* replayCount);
protected:
    virtual RC LoadViaPramin(const vector<UINT08> &data, const ReplayOptions&);
    virtual RC LoadViaBar1(const vector<UINT08> &data, const ReplayOptions&);
    virtual RC LoadViaDirectPhysWrite(const vector<UINT08> &data, const ReplayOptions&);

protected:
    std::string m_MemAperture;
    UINT64 m_PhysAddress;
    UINT32 m_Size;
    std::string m_FileName;
};

class AnnotationOp:public TraceOp
{
public:
    AnnotationOp(ChiplibTrace* trace, const std::vector<const char*>& tokens,
            const std::string& comment, int lineNum);
    std::string GetAnnotation(){return m_Annotation;};
    virtual RC Replay(const ReplayOptions&, int* replayCount);

protected:
    std::string m_Annotation;
    bool m_NeedReplay;
};
class Mask32ReadOp:public BusMemOp
{
public:
    Mask32ReadOp(ChiplibTrace* trace, const std::vector<const char*>& tokens,
            const std::string& comment, int lineNum);
    virtual RC Replay(const ReplayOptions&, int* replayCount);
protected:
    bool m_Pm;
    UINT32 m_Mask;
    bool m_Polling;
    bool (*m_CompFunc)(UINT32, UINT32);
};
class TagInfoOp:public TraceOp
{
public:
    TagInfoOp(ChiplibTrace* trace, std::vector<const char*> tokens,
            const std::string& comment, int lineNum);
    virtual RC Replay(const ReplayOptions&, int* replayCount);
protected:
    std::string m_Tag;
    std::string m_Value;
};
class ChiplibTrace
{
public:
    enum OPTYPE
    {
        OP_READ,
        OP_WRITE
    };
    ChiplibTrace(const ReplayOptions&,const string& refFile);
    ~ChiplibTrace();
    RC AddTraceOp(TraceOp* op, bool isOptional = false);
    std::vector<TraceOp*>& GetTraceOps() {return m_TraceOps;}
    RC Replay();
    void SetBaseAddr(const std::string& aperture, UINT64 addr, UINT64 size);
    std::pair<UINT64, UINT64> GetBaseAddr(const std::string& aperture) const;
    unsigned int GetApertureID(const std::string&);
    const std::string& GetRealAperture(const std::string&);
    void MapDeviceMemory(const std::string& aperture, UINT64 addr, UINT64 size);
    void* GetMappedVirtualAddr(const std::string& aperture) const;
    void PrintRegister(UINT64 offset, UINT32 data, UINT32 expected_data) const;
    RC Report(string& message);
    void AddPmCapture(UINT32 address, UINT32 value, const std::string& clock_name);
    void AddPmReplay(UINT32 address, UINT32 value);
    void PrintTraceOp(TraceOp* op, Tee::Priority level);
    void PrintTraceOp(TraceOp* op, Tee::PriDebugStub)
    {
        PrintTraceOp(op, Tee::PriSecret);
    }
    void CollectReplayStatistics(size_t bytes, size_t mismatched,
            const std::string &aperture, OPTYPE opType);
    void InsertSurfaceInfo();

    const SurfInfo& GetLwrSurfInfo()const{return m_SurfInfo;}
    void SetLwrSurfInfo(const SurfInfo& info){m_SurfInfo = info;}

    void SetGpuName(const string& gpuName);
    const string& GetGpuName() { return m_GpuName; }
    void SetTestName(const string& testName) { m_TestName = testName; }
    const string& GetTestName() { return m_TestName; }
    void SetTracePath(const string& tracePath) { m_TracePath = tracePath; }
    const string& GetTracePath() { return m_TracePath; }
    void SetBackdoorArchiveId(const string& archiveId)
        { m_BackdoorArchiveId = archiveId; }
    const string& GetBackdoorArchiveId() { return m_BackdoorArchiveId; }

    bool IsInsideCRCReading();
    void CollectReplayData(size_t size, size_t diff);
    void StartCRCChecking();
    void EndCRCChecking(const std::string& surfName);

    bool NeedRetry(unsigned int retryCnt, const ReplayOptions& option,
        const string& aperture);
    void WaitForRetry(const ReplayOptions& option);
    void PrintRetryMsg(const string& msg, unsigned int retryCnt, int maxPollCnt);

protected:
    static const std::string m_Bar0Prefix;
    static const std::string m_PmClockCheck;
    static const float m_PmTolerance;
    std::vector<TraceOp*> m_TraceOps;
    std::map<std::string, std::pair<UINT64, UINT64> > m_BaseAddrMap;
    std::map<std::string, unsigned int> m_ApertureMap;
    std::vector<std::pair<std::string, std::pair<UINT64, UINT64> > > m_BaseAddrArray;
    std::map<std::string, void*> m_MappedApertures;
    std::map<UINT32, std::pair<UINT32, UINT32> > m_PmsAll; // <address, <capture, replay> >
    std::set<UINT32> m_PmsCheck;
    RefManual* m_RefManual;
    ReplayOptions m_Option;

    size_t m_MismatchCnt;
    size_t m_ReplayCnt;
    size_t m_Bar0Read;
    size_t m_Bar1Read;
    size_t m_Bar2Read;
    size_t m_SysMemRead;
    size_t m_Bar0Write;
    size_t m_Bar1Write;
    size_t m_Bar2Write;
    size_t m_SysMemWrite;
    size_t m_Bar0Mismatch;
    size_t m_Bar1Mismatch;
    size_t m_Bar2Mismatch;
    size_t m_SysMemMismatch;
    size_t m_Transactions;

    SurfInfo m_SurfInfo;

    string m_GpuName;
    string m_TestName;
    string m_TracePath;
    string m_BackdoorArchiveId;

private:
    UINT64 m_MappedApertureCacheBase; // a one entry cache to avoid mapped aperture search
    UINT64 m_MappedApertureCacheSize;

    std::map<std::string, std::pair<UINT64, UINT64> > m_ReplayStat;
    bool m_StartCRCReading;
    size_t m_TotalSize;
    size_t m_TotalDiff;
};

    bool IsReplayingTrace();
}
#endif
