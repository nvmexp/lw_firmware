/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef CHIPLIB_TRACE_CAPTURE_SIM_H
#define CHIPLIB_TRACE_CAPTURE_SIM_H

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

#ifndef INCLUDED_STL_STACK
#include <stack>
#define INCLUDED_STL_STACK
#endif

#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif

#ifndef INCLUDED_STL_SET
#include <set>
#define INCLUDED_STL_SET
#endif

#include <tuple>
#include <unordered_map>
#include "core/include/color.h"
#include "core/include/tasker.h"
#include "core/include/refparse.h"

class BusMemOp;
class CfgPioOp;
class AnnotationOp;
class ChiplibOpBlock;
class EscapeOp;
class RawMemOp;
class ChiplibTraceDumper;
class FileIO;

//--------------------------------------------------------------------
//! \brief Abstract base class to represents a single operation between
//!        MODS and Chiplib.
class ChiplibOp
{
public:
    enum OpType
    {
        OPTYPE_BEGIN = 0,
        BUSMEM_READ = OPTYPE_BEGIN,
        BUSMEM_WRITE,
        BUSMEM_READMASK,
        ANNOTATION,
        CHIPLIBOP_BLOCK,
        ESCAPE_READ,
        ESCAPE_WRITE,
        CFG_READ,
        CFG_WRITE,
        DUMP_RAW_MEM,
        LOAD_RAW_MEM,
        DUMP_PHYS_MEM,
        LOAD_PHYS_MEM,
        OPTIONAL_BLOCK,
        OPTYPE_LAST
    };

    ChiplibOp(OpType type);
    virtual ~ChiplibOp() {}

    static const char *GetOpTypeName(OpType type);

    BusMemOp *GetBusMemOp(ChiplibOp *pOp);
    ChiplibOpBlock *GetChiplibOpBlock(ChiplibOp *pOp);

    OpType GetOpType() const { return m_Type; }
    void SetOpType(OpType type);

    virtual string GetComments() const;
    virtual string DumpToFile(FileIO *pTraceFile) const = 0;

    virtual RC PreProcessBeforeCapture(ChiplibOpBlock *pOpBlock) { return OK; }
    virtual RC PostProcessBeforeDump() { return OK; }

    UINT64 GetTimeNs() const { return m_TimeNs; }

protected:
    ChiplibTraceDumper *m_ChiplibTraceDumper;
    // A bit tricky, there is on exception that ChiplibOpBlock::DumpTailToFile will update m_TimeNs
    mutable UINT64 m_TimeNs;

    RC WriteTraceFile(const string & traceTrans, FileIO *pTraceFile) const;

private:
    UINT32 GetSysTime() const;

    OpType m_Type;

    enum ThreadInfo
    {
        THREAD_ID,
        THREAD_NAME
    };
    tuple<Tasker::ThreadID, string> m_Thread;
};

//--------------------------------------------------------------------
//! \brief To manage chiplibtracedumper global information;
//!
class ChiplibTraceDumper
{
public:
    static ChiplibTraceDumper *GetPtr() { return &s_ChiplibTraceDumper; }

    ChiplibTraceDumper();
    ~ChiplibTraceDumper();

    void SetDumpChiplibTrace() { m_DumpChiplibTrace = true; }
    bool DumpChiplibTrace() const { return m_DumpChiplibTrace; }

    enum class RawChiplibTraceDumpLevel {Light, Medium, Heavy, Unsupported};
    void SetRawDumpChiplibTrace();
    bool RawDumpChiplibTrace() const { return m_RawDumpChiplibTrace; }
    FileIO* GetRawTraceFile() { return m_RawTraceFile; }
    void SetRawDumpChiplibTraceLevel(UINT32 level);
    RawChiplibTraceDumpLevel GetRawDumpChiplibTraceLevel() const { return m_RawDumpChiplibTraceLevel; };

    bool IsChiplibTraceDumpEnabled();
    void EnableChiplibTraceDump();
    void DisableChiplibTraceDump();

    void EnablePramin() { m_EnablePramin = true; }
    bool IsPraminEnabled() { return m_EnablePramin; }
    void SetIgnoredRegFile(const string& fileName) { m_ignoredRegFile = fileName;}

    void SetDumpCompressed(bool compressed) { m_IsDumpCompressed = compressed; }
    void Cleanup();
    bool IgnoreAddr(PHYSADDR addr);

    ChiplibOpBlock* GetLwrrentChiplibOpBlock();
    void Push(ChiplibOpBlock *pOpBlock);
    void Pop();

    void DumpToFile();
    void RawDumpToFile(ChiplibOp *op);

    size_t GetStackSize() const;

    bool IsGpuInfoInitialized() const { return m_IsGpuInfoInitialized; }
    RC GetGpuSubdevInfo();

    RC SetPciCfgSpace(INT32 pciDomainNumber,
                      INT32 pciBusNumber,
                      INT32 pciDevNumber,
                      INT32 pciFunNumber,
                      UINT32 cfgSpaceBaseAddr);
    RC GetPciCfgSpaceIdx(UINT32 compAddrBase, UINT32 *pRtnIdx);

    enum AddrType
    {
        ADDR_BAR0 = 0,
        ADDR_BAR1,
        ADDR_INST,
        ADDR_SYSMEM,
        ADDR_UNKNOWN
    };

    string GetAddrTypeName(AddrType type, PHYSADDR addr) const;
    AddrType GetAddrTypeOffset(PHYSADDR addr, PHYSADDR *pOffset);
    bool GetBar0AddrOffset(PHYSADDR addr, PHYSADDR *pOffset);
    bool GetBar1AddrOffset(PHYSADDR addr, PHYSADDR *pOffset);
    bool GetInstAddrOffset(PHYSADDR addr, PHYSADDR *pOffset);

    void SaveSysmemSegment(PHYSADDR baseAddr, UINT64 size);
    UINT32 GetSysmemSegmentId(PHYSADDR addr) const;

    void SetStartTimeNs(UINT32 timeNs) { m_StartTimeNs = timeNs; }
    UINT64 GetStartTimeNs() const { return m_StartTimeNs; }

    struct RegOpInfo
    {
        UINT32 InterestedMask;
        UINT32 Flags;
    };
    const RegOpInfo *GetSpecialRegOpInfo(PHYSADDR addr);

    set<PHYSADDR> *GetBar0AccessAddrSet() { return &m_Bar0AccessAddrSet; }

    bool IsBar1DumpEnabled() { return IsChiplibTraceDumpEnabled() && m_IsBar1DumpEnabled; }
    void SetBar1DumpFlag(bool enabled) { m_IsBar1DumpEnabled = enabled; }

    bool IsSysmemDumpEnabled() { return IsChiplibTraceDumpEnabled() && m_IsSysmemDumpEnabled; }
    void SetSysmemDumpFlag(bool enabled) { m_IsSysmemDumpEnabled = enabled; }

    void SetTestNamePath(const string& testName, const string& tracePath);
    UINT32 GetTestNum() const { return (UINT32)(m_TestsInfo.size()); }

    void SetBackdoorArchiveId(const string& archiveId)
        { m_BackdoorArchiveId = archiveId; }
    PHYSADDR GetBar0Base()const {return m_Bar0Base;}

    void AddArchiveFile(const std::string& dumpFileName, const std::string& archiveFileName);
    RC DumpArchiveFiles();
private:
    static ChiplibTraceDumper s_ChiplibTraceDumper;

    RC ChiplibTraceDumpInit();
    RC RawChiplibTraceDumpInit();

    FileIO *m_RawTraceFile;

    void PrintFileHeader(FileIO *pTraceFile);
    void PrintCopyright(FileIO *pTraceFile);
    void PrintBuildInfo(FileIO *pTraceFile);
    void PrintGpuInfo(FileIO *pTraceFile);
    void PrintTestInfo(FileIO *pTraceFile);

    bool m_DumpChiplibTrace; // -dump_chiplibtrace is requested
    bool m_EnablePramin;
    bool m_RawDumpChiplibTrace;
    RawChiplibTraceDumpLevel m_RawDumpChiplibTraceLevel;

    set<Tasker::ThreadID> m_DisabledThreads;

    stack<ChiplibOpBlock*> m_ChiplibOpBlocks;
    map<Tasker::ThreadID, stack<ChiplibOpBlock*>> m_RawChiplibOpBlocks;

    bool m_IsGpuInfoInitialized;
    bool m_IsChiplibDumpInitialized;
    bool m_IsDumpCompressed;
    bool m_IsBar1DumpEnabled;
    bool m_IsSysmemDumpEnabled;

    set<PHYSADDR> m_Bar0AccessAddrSet;
    map<PHYSADDR, PHYSADDR> m_IgnoreAddrMap;
    string m_ignoredRegFile;
    void ParseIgnoredRegisterFile();

    map<string, string> m_BackdoorArchiveFiles;
    struct TestInfo
    {
        string TestName;
        string TracePath;
    };
    vector<TestInfo> m_TestsInfo;

    struct ThreadDumpStateCache
    {
        Tasker::ThreadID LatestThreadId;
        bool             DumpEnabled;

        ThreadDumpStateCache(): LatestThreadId(Tasker::NULL_THREAD), DumpEnabled(false) {}
    };
    ThreadDumpStateCache m_ThreadDumpStateCache;

private:
    // test info
    //
    PHYSADDR m_Bar0Base;
    UINT32   m_Bar0Size;
    PHYSADDR m_Bar1Base;
    UINT64   m_Bar1Size;
    PHYSADDR m_InstBase;
    UINT64   m_InstSize;

    enum SysMemHeap
    {
        SYSMEM_SIZE,
        SYSMEM_ID
    };
    unordered_map<PHYSADDR, tuple<UINT64, UINT32>> m_SysMemSegments;

    struct PciCfgSpace
    {
        INT32 PciDomainNumber;
        INT32 PciBusNumber;
        INT32 PciDevNumber;
        INT32 PciFunNumber;
    };
    map<UINT32, PciCfgSpace> m_PciCfgSpaceMap;

    UINT32 m_PciDeviceId;

    map<PHYSADDR, RegOpInfo> m_SpecialRegs;

    UINT64 m_StartTimeNs;
    string m_TestName;
    string m_TracePath;
    string m_GpuName;
    string m_BackdoorArchiveId;
};

//--------------------------------------------------------------------
//! \brief A class to create a new chiplib operation scope
//!
class ChiplibOpScope
{
public:
    enum ScopeType
    {
        SCOPE_COMMON,
        SCOPE_POLL,
        SCOPE_IRQ,
        SCOPE_CRC_SURF_READ,
        SCOPE_OPTIONAL,
    };

    struct Crc_Surf_Read_Info
    {
        string SurfName;
        string SurfFormat;
        UINT32 SurfPitch;
        UINT32 SurfWidth;
        UINT32 SurfHeight;
        UINT32 SurfBytesPerPixel;
        UINT32 SurfNumLayers;
        UINT32 SurfArraySize;
        UINT32 RectSrcX;
        UINT32 RectSrcY;
        UINT32 RectSrcWidth;
        UINT32 RectSrcHeight;
        UINT32 RectDestX;
        UINT32 RectDestY;
    };

    ChiplibOpScope(const string& scopeInfo, UINT08 irq, ScopeType type, void* extraInfo, bool optional = false);
    ~ChiplibOpScope();

    void CancelOpBlock() { m_CancelOpBlock = true; }
    static string GetScopeDescription(const string& scopeInfo, UINT08 irq,
                                    ScopeType type, void* extraInfo);
private:
    bool m_CancelOpBlock;
};
class GpuSubdevice;
//--------------------------------------------------------------------
//! \brief A block of ChiplibOps produced in a ChiplibOpScope
//!
class ChiplibOpBlock : public ChiplibOp
{
public:
    ~ChiplibOpBlock();

    virtual string DumpToFile(FileIO *pTraceFile) const;
    virtual string GetComments() const;

    virtual RC PostProcessBeforeDump();

    BusMemOp *AddBusMemOp(const char *typeStr, PHYSADDR addr,
                          const void* data, UINT32 count);
    AnnotationOp *AddAnnotOp(const char* pLinePrefix, const char* pStr);
    ChiplibOpBlock *AddBlockOp(OpType type, ChiplibOpScope::ScopeType scopeType,
                               const string &blockDes, UINT32 stackDepth,
                               bool optional = false);
    EscapeOp *AddEscapeOp(const char *typeStr, UINT08 gpuId, const char *path,
                          UINT32 index, UINT32 size, const void *buf);
    CfgPioOp *AddCfgPioOp(const char *typeStr, INT32 domainNumber, INT32 busNumber,
                          INT32 deviceNumber, INT32 functionNumber,
                          INT32 address, const void* data, UINT32 count);
    RawMemOp *AddRawMemDumpOp(const string &memAperture,
                              UINT64 physAddress, UINT32 size,
                              const string &fileName);
    RawMemOp *AddRawMemLoadOp(const string &memAperture,
                              UINT64 physAddress, UINT32 size,
                              const string &fileName);
    RawMemOp *AddPhysMemDumpOp(const string &memAperture,
                               UINT64 physAddress, UINT32 size,
                               const string &fileName);
    RawMemOp *AddPhysMemLoadOp(const string &memAperture,
                               UINT64 physAddress, UINT32 size,
                               const string &fileName);

    ChiplibOpScope::ScopeType GetChiplibOpScopeType() const { return m_ChiplibOpScopeType; }

    RC DropLastOp();
private:
    friend ChiplibTraceDumper::ChiplibTraceDumper();
    friend void ChiplibTraceDumper::SetRawDumpChiplibTrace();
    friend void ChiplibTraceDumper::Cleanup();
    friend ChiplibOpBlock* ChiplibTraceDumper::GetLwrrentChiplibOpBlock();
    friend ChiplibOpScope::ChiplibOpScope(const string& scopeInfo,
                                          UINT08 irq, ScopeType type,
                                          void* extraInfo, bool optional);
    friend ChiplibOpScope::~ChiplibOpScope();
    ChiplibOpBlock(OpType type, ChiplibOpScope::ScopeType scopeType, const string &blockDes,
             UINT32 stackDepth, bool optional = false);

private:
    string GetName() const { return m_Name; }

    string GetBlockDescription() const { return m_BlockDescription; }
    UINT32 GetStackDepth() const { return m_StackDepth; }
    bool DumpPramin(GpuSubdevice* pSubdev, BusMemOp* pBusMemOp,
            PHYSADDR addr, const void *data, UINT32 count);

    string DumpHeadToFile(FileIO *pTraceFile) const;
    string DumpOpsToFile(FileIO *pTraceFile) const;
    string DumpTailToFile(FileIO *pTraceFile) const;

private:
    vector<ChiplibOp*> m_Operations;

    string m_BlockDescription;

    UINT32 m_StackDepth; //!< Stack depth of this ChiplibOpBlock in ChiplibTraceDumper

    string m_Name;

    ChiplibOpScope::ScopeType m_ChiplibOpScopeType;

    bool m_optional;

    static UINT32 m_PraminNextIndex;

    static FILE* m_PraminDumpFile;

    static std::vector<BusMemOp*> m_CachedOps;

    static UINT32 m_CachedDataSize;

    static PHYSADDR m_LastPhysAddr;

    AnnotationOp* m_pLastAnnotOp;
    bool m_bNewAnnotationLineStart;
};

//--------------------------------------------------------------------
//! \brief Busmem read or write operation
//!
class BusMemOp : public ChiplibOp
{
public:
    virtual string DumpToFile(FileIO *pTraceFile) const;
    virtual string GetComments() const;

    PHYSADDR GetStartAddr() const { return m_Address; }
    vector<UINT08> *GetData() { return &m_Data; }

    enum BusMemOpFlags
    {
        RD_MATCH = 0,    // polling until match
        RD_IGNORE,       // ignore returned data
        RD_OVERRIDE,     // overriden by followed rd
        RD_CHANGING,     // constantly changing rd
        RD_POLL_ONE,     // poll wait the interested bits to be 1
        RD_POLL_ZERO,    // poll wait the interested bits to be 0
        RD_POLL_GE,      // poll wait the interested bits great-equal to capture bits
        RD_NONPOLL_EQ,   // read once; equal match
        RD_DROP,         // Not output to chiplib trace
        RD_IGNORE_FIRST_REGRD, // Ignore first reg read in global range
        RD_FOLLOWED_WR,  // Read followed by a write on same address
        WRITE_OP,
        PM_COUNTER
    };

    void SetBusMemOpFlags(BusMemOpFlags flags) { m_BusMemOpFlags = flags; }

    virtual RC PreProcessBeforeCapture(ChiplibOpBlock *pOpBlock);
    virtual RC PostProcessBeforeDump();

    string GetRdMaskModeStr() const;

private:
    friend BusMemOp *ChiplibOpBlock::AddBusMemOp(
             const char *typeStr, PHYSADDR addr,
             const void* data, UINT32 count);
    BusMemOp(OpType type, PHYSADDR addr,
             const void* data, UINT32 count);

    UINT32 GetFirstUINT32() const;
private:
    PHYSADDR m_Address;
    vector<UINT08> m_Data;

    BusMemOpFlags m_BusMemOpFlags;

    UINT32 m_ReadMask; // default value ~0 -> match all bits
};

//--------------------------------------------------------------------
//! \brief cfg or io space read or write operation
//!
class CfgPioOp : public ChiplibOp
{
public:
    virtual string DumpToFile(FileIO *pTraceFile) const;
    virtual string GetComments() const;

    UINT32 GetStartAddr() const { return m_CfgBase + m_Address; }

private:
    friend CfgPioOp *ChiplibOpBlock::AddCfgPioOp(const char *typeStr,
             INT32 domainNumber, INT32 busNumber,
             INT32 deviceNumber, INT32 functionNumber,
             INT32 address, const void* data, UINT32 count);
    CfgPioOp(OpType type, UINT32 cfgBase, UINT32 addr,
             const void* data, UINT32 count);

private:
    UINT32 m_CfgBase;
    UINT32 m_Address;
    vector<UINT08> m_Data;
};

//--------------------------------------------------------------------
//! \brief AnnotationOp
//!
class AnnotationOp : public ChiplibOp
{
public:
    virtual string DumpToFile(FileIO *pTraceFile) const;
    virtual string GetComments() const;
private:
    friend AnnotationOp *ChiplibOpBlock::AddAnnotOp(const char* pLinePrefix, const char* pStr);
    AnnotationOp(const string &annotation);
    void AppendAnnotaion(const string& str);

private:
    string m_Annotation;
};

//--------------------------------------------------------------------
//! \brief Escape rd/wr
//!
class EscapeOp : public ChiplibOp
{
public:
    virtual string DumpToFile(FileIO *pTraceFile) const;
    virtual string GetComments() const;
private:
    friend EscapeOp *ChiplibOpBlock::AddEscapeOp(const char *typeStr, UINT08 gpuId,
                                                 const char *path, UINT32 index,
                                                 UINT32 size, const void *value);
    EscapeOp(OpType type, UINT08 gpuId, const char *path,
             UINT32 index, UINT32 size, const void *buf);

private:
    UINT08 m_GpuId;
    string m_EscapeKey;
    UINT32 m_Index;
    UINT32 m_Size;
    vector<UINT08> m_Values;
};

//--------------------------------------------------------------------
//! \brief Chiplib op for dumping raw memory
//!
class RawMemOp : public ChiplibOp
{
public:
    virtual string DumpToFile(FileIO *pTraceFile) const;
    virtual string GetComments() const;
    UINT64  GetPhysAddress() const { return m_PhysAddress; }
    UINT32  GetSize() const { return m_Size; }
    const string&  GetFileName() const { return m_FileName; }

private:
    friend RawMemOp *ChiplibOpBlock::AddRawMemDumpOp(const string &memAperture, UINT64 physAddress, UINT32 size, const string &fileName);
    friend RawMemOp *ChiplibOpBlock::AddRawMemLoadOp(const string &memAperture, UINT64 physAddress, UINT32 size, const string &fileName);
    friend RawMemOp *ChiplibOpBlock::AddPhysMemDumpOp(const string &memAperture, UINT64 physAddress, UINT32 size, const string &fileName);
    friend RawMemOp *ChiplibOpBlock::AddPhysMemLoadOp(const string &memAperture, UINT64 physAddress, UINT32 size, const string &fileName);
    RawMemOp(OpType type, const string &memAperture, UINT64 physAddress, UINT32 size, const string &fileName);

    string m_MemAperture;
    UINT64 m_PhysAddress;
    UINT32 m_Size;
    string m_FileName;
};

#define GPU_READ_PRI_ERROR_MASK  0xFFF00000
#define GPU_READ_PRI_ERROR_CODE  0xBAD00000

#define NON_IRQ 0xFF
#define CHIPLIB_CALLSCOPE ChiplibOpScope newScope(__FUNCTION__, NON_IRQ, ChiplibOpScope::SCOPE_COMMON, NULL)

extern PHYSADDR GetSysmemBaseForPAddr(PHYSADDR Addr);
extern UINT64 GetSysmemSizeForPAddr(PHYSADDR Addr);
extern bool IsSysmemAccess(PHYSADDR Addr);
extern void WriteDataToBmp(const UINT08* data, UINT32 width, UINT32 height,
    ColorUtils::Format format, const std::string& surfName);

#endif
