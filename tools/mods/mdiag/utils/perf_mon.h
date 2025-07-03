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

//!
//! \file perf_mon.h
//! \brief Performance Monitor
//!
//! Header for the Performance Monitor class
//!

#pragma once

#include "pm_rpt.h"
#include "types.h"
#include "mdiag/lwgpu.h"
#include <utility>

#define PM_FILE_COMMAND_REG_READ         0
#define PM_FILE_COMMAND_REG_POLL         1
#define PM_FILE_COMMAND_REG_WRITE        2
#define PM_FILE_COMMAND_SET_MODEC_BUFF   3
#define PM_FILE_COMMAND_POLL_MODEC_BUFF  4
#define PM_FILE_COMMAND_BIND_MODEC_BUFF  5
#define PM_FILE_COMMAND_DUMP_MODEC_BUFF  6
#define PM_FILE_COMMAND_OPEN_FILE        7
#define PM_FILE_COMMAND_CLOSE_FILE       8
#define PM_FILE_COMMAND_RUN_SCRIPT       9
#define PM_FILE_COMMAND_REG_READ_ASSIGN  10

const int LW_PM_MAX_IDLE_COUNT = 5;

class XMLNode;
class HwpmReg;
class PerformanceMonitor;

/*
 * PM_MODE_C is to support ModeCAlloc which is not supported by PMLSplitter
 * and only used by FC PMA test bench(?).
 * From Turing timeframe, PM_MODE_E is added to support modeE and SMPC
 * However, in theory it can also be used to support modeC.
 * */
typedef enum {
    PM_MODE_A = 0,
    PM_MODE_B,
    PM_MODE_C,
    PM_MODE_E,
    PM_MODE_SMPC,
    PM_MODE_CAU,
    PM_MODE_UNKNOWN
} PmMode;

/*
 * PerfmonBuffer is to support streaming capture including ModeC, ModeE and SMPC.
 * */
class PerfmonBuffer
{
public:
    // Lwrrently PerfmonBuffer is used by either CreatePerfmonBuffer or Utl.PerfmonBuffer
    // and have consistent behavior regardless of PM modes. ModeCAlloc node is not used 
    // since Turing and can be deprecated later when we decide to not support older chips. 
    // For now keep the ModeCAlloc related codes for legacy compatibility.
    PerfmonBuffer(LWGpuResource* pGpuRes, MdiagSurf* pSurf, bool isModeCAlloc);
    PerfmonBuffer(UINT32 sizeInBytes, Memory::Location aperture, LWGpuResource* pGpuRes, bool isModeCAlloc);

    ~PerfmonBuffer();

    UINT32 GetSize() const;

    void Write(UINT32 offset, UINT32 value);

    UINT32 Read(UINT32 offset);

    bool OpenFile(const string& path);

    bool IsFileOpened() const;

    void CloseFile();

    bool IsModeCAlloc() const;

    void Setup(LwRm* pLwRm);

    void DumpToFile();

    void InitHwpmRegs();

protected:
    UINT32 m_SizeInBytes;
    FILE* m_File;
    bool m_IsModeCAlloc;
    LWGpuResource *m_pGpuRes;
    unique_ptr<MdiagSurf> m_OwnedSurface;
    // the surface object can be created inside this class or passed from argument
    // depends on the way this class object being constructed
    MdiagSurf *m_pSurface;

private:
    unique_ptr<HwpmReg> m_HwpmReg;

    void Allocate(LwRm* pLwRm);

    void ConfigurePMA();

    void DumpTextToFile();

    void DumpBinaryToFile();
};

class DomainInfo
{
public:
    DomainInfo(const int index, const char *name, PerformanceMonitor *pm);
    ~DomainInfo();

    const char *GetName() const { return m_DomainName.c_str(); }
    PmMode GetMode() const { return m_PmMode; }
    void SetMode(PmMode mode) { m_PmMode = mode; }
    bool IsInUse() const { return m_InUse; }
    void SetInUse(bool in_use) { m_InUse = in_use; }
    int GetIndex() const { return m_Index; }

private:
    const int m_Index;
    const string m_DomainName;
    PerformanceMonitor* m_PerfMon;
    bool m_InUse;
    PmMode m_PmMode;
};

typedef vector<DomainInfo *> DomainInfoList;

typedef tuple<SmcEngine*, UINT32> RegValInfo;
typedef vector<RegValInfo>  RegValInfoList;

class PerformanceMonitor
{
public:
    enum PM_ACTIONS
    {
        pm_wait_for_idle = 0,
        pm_trigger,
        pm_zoofar
    };

    PerformanceMonitor(LWGpuResource *lwgpu_res, UINT32 subdev,
        ArgReader *parameters);

    virtual ~PerformanceMonitor();

    int Initialize(const char *pm_report_option,
        const char *experiment_file_name);

    virtual void Start(LWGpuChannel *channel, UINT32 subchannel,
        UINT32 class_to_use);

    virtual void End(LWGpuChannel *channel, UINT32 subchannel,
        UINT32 class_to_use);

    virtual void HandleInterrupt();

    static bool IsDebugPm() { return m_PMDebugInfo; }

    GpuSubdevice* GetSubDev() { return m_pSubDev; }

    static void AddZLwllId(UINT32 id);
    static void AddSurface(const MdiagSurf* surf);
    static void ClearZLwllIds() { s_ZLwllIds.clear(); }
    static void ClearSurfaces() { s_RenderSurfaces.clear(); }

private:
    enum PM_COMMAND
    {
        pm_set = 0,
        pm_init,
        pm_finalize,
        pm_none
    };

    enum PM_OP
    {
        EQUAL,
        NOTEQUAL
    };

    enum PerfmonInstance
    {
        NONE = 0,
        TPC,
        ROP,
        FBP
    };

    enum RegWriteGroup
    {
        INIT,
        STOP,
        HOLD_TEMPORAL_CAPTURE,
        START_TEMPORAL_CAPTURE
    };

    struct PMCommandInfo
    {
        PMCommandInfo(PM_COMMAND command, UINT32 address, UINT32 value) :
            m_command(command), m_address(address), m_value(value) {}

        PM_COMMAND m_command;
        UINT32 m_address;
        UINT32 m_value;
    };

    struct RegValAssignInfo
    {
        RegValAssignInfo(string name, UINT32 mask, UINT32 rightShift) :
            m_name(move(name)), m_mask(mask), m_rightShift(rightShift)
            {}

        string m_name;
        UINT32 m_mask;
        UINT32 m_rightShift;
    };

    struct RegWriteInfo
    {
        RegWriteInfo(UINT32 address, UINT32 mask,
            UINT32 value, UINT32 instance_type,
            UINT32 instance, UINT32 write, string name, string sysPipe = "") :
            m_address(address), m_value(value),
            m_mask(mask),
            m_instanceType(instance_type), m_instance(instance),
            m_write(write), m_name(move(name)), m_sysPipe(move(sysPipe))
            {}

        UINT32 m_address;
        UINT32 m_value;
        UINT32 m_mask;
        UINT32 m_instanceType;
        UINT32 m_instance;
        UINT32 m_write;
        string m_name;
        string m_sysPipe;
        list<RegValAssignInfo> m_assignInfoList;
    };

    class RegWriteListGroup
    {
    public:
        RegWriteListGroup() = default;
        ~RegWriteListGroup() = default;

        template<typename... Args> list<RegWriteInfo>::reference
        AddRegWrite(RegWriteGroup group, INT32 groupIndex, Args&&... args);
        const map<INT32, list<RegWriteInfo>>& GetRegWriteInfo(RegWriteGroup group);

    private:
        map<RegWriteGroup, map<INT32, list<RegWriteInfo>>> m_RegWriteInfoGroup;
    };

    bool LoadPMFile(const string &filename);
    bool ProcessPMFile(const string &filename);
    bool ParsePMFile(const string &filename);
    bool ProcessPMFileRoot(XMLNode *root_node);
    string *GetNodeAttribute(XMLNode *node, const char *attribute_name,
        bool required);
    bool ProcessDomainListNode(XMLNode *node);
    bool ProcessDomainNode(XMLNode *node);
    bool ProcessSignalListNode(XMLNode *node);
    bool ProcessSignalNode(XMLNode *node);
    bool ProcessModeCAllocNode(XMLNode *node);
    bool ProcessCreatePerfmonBufferNode(XMLNode *node);
    bool ProcessCreatePerfmonBufferNode(XMLNode *node, UINT32 multiplyFactor, bool isModeCAlloc);
    bool ProcessRegWriteListNode(XMLNode *node);
    bool ProcessRegWriteNode(XMLNode *node, RegWriteGroup group, INT32 groupIndex);
    bool ProcessRegReadNode(XMLNode *node, RegWriteGroup group, INT32 groupIndex);
    bool ProcessAssignNode(XMLNode *node, RegWriteInfo* pRegWriteInfo);
    void AddSetInitCommand(UINT32 address, UINT32 value);
    void AddFinalizeInitCommand(UINT32 address, UINT32 value);
    void PMStreamCapture(RegWriteGroup group, LwRm* pLwRm);
    void PMRegInit(LwRm* pLwRm);
    void PMRegStart(LWGpuChannel *channel, UINT32 subchannel);
    void PMExelwteCommand(LWGpuChannel *channel, UINT32 subchannel,
        const PMCommandInfo &command);
    int WaitForState(DomainInfo *domain, UINT32 state, PM_OP op,
        int max_idle_count);
    UINT32 ReadPmState(DomainInfo *domain);
    void PMExitPoll(LWGpuChannel *channel, UINT32 subchannel);
    void ReportPmState(DomainInfo *domain);
    int Stop(LWGpuChannel *channel, UINT32 subchannel);
    void ExtendedCommands(const RegWriteInfo& regWrite, LwRm* pLwRm);

    vector<SmcEngine*> GetSmcEngines(const string& sysPipe);
    void RegWrite(UINT32 address, string name, UINT32 mask,
                  UINT32 value, UINT32 instance_type, UINT32 instance, const string& sysPipe = "");
    void RegComp(UINT32 address, string name, UINT32 mask,
                 UINT32 value, UINT32 instance_type, UINT32 instance, const string& sysPipe = "");
    RegValInfoList RegRead(UINT32 address, string name, UINT32 instance_type, UINT32 instance, const string& sysPipe = "");
    vector<pair<string, RegValInfoList>> RegRead(UINT32 address, string name, UINT32 instanceType, UINT32 instance,
                                        const list<PerformanceMonitor::RegValAssignInfo>& assignList, const string& sysPipe = "");
    static const char* GroupEnumToStr(RegWriteGroup group);

    LWGpuResource* m_pGpuRes;
    UINT32 m_subdev;
    GpuSubdevice* m_pSubDev;
    PMReportInfo m_PmReportInfo;
    string m_ExperimentFileName;
    UINT32 m_TriggerMethod;
    DomainInfoList m_DomainInfoList;
    ArgReader* m_Params;
    vector<PMCommandInfo> m_PmInitCommands;
    RegWriteListGroup m_RegWriteListGroup;
    unique_ptr<PerfmonBuffer> m_pPerfmonBuffer;
    UINT32 m_TpcMask;
    UINT32 m_FbMask;
    static bool m_PMDebugInfo;

    static vector<UINT32>    s_ZLwllIds;
    static vector<const MdiagSurf*> s_RenderSurfaces;
};

#define PMInfo if (PerformanceMonitor::IsDebugPm()) InfoPrintf

