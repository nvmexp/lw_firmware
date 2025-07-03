/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef SMC_RESOURCE_CONTAINER_H
#define SMC_RESOURCE_CONTAINER_H


class SmcEngine;
class GpuPartition;
class LWGpuResource;
class SmcParser;

#include "core/include/argdb.h"
#include "core/include/argread.h"
#include "core/include/argdb.h"
#include <list>
#include <map>
#include <deque>
#include "lwmisc.h"
#include "ctrl/ctrlxxxx.h" // LWXXXX_CTRL_CMD_GET_FEATURES required by ctrl2080gpu.h
#include "ctrl/ctrl2080/ctrl2080gpu.h"
#include "core/include/lwrm.h"
#include <memory>

#ifndef BIT
#define BIT(b)                  (1U<<(b))
#endif

#define ILWALID_GPC_COUNT ~0U
#define MODS_PARTITION_NAME "MODS_PART"
#define MODS_SMCENG_NAME "MODS_SMCENG"

/*
 * SmcResourceController manage all SMC related resources.
 */
class SmcResourceController
{
public:
    explicit SmcResourceController(LWGpuResource * pGpuResource,
                                   ArgReader * params);
    RC ConfigureSmc(string configure);

    enum PartitionFlag
    {
        FULL = LW2080_CTRL_GPU_PARTITION_FLAG_FULL_GPU,
        HALF = LW2080_CTRL_GPU_PARTITION_FLAG_ONE_HALF_GPU,
        MINI_HALF = DRF_DEF(2080, _CTRL_GPU_PARTITION_FLAG, _MEMORY_SIZE, _HALF) | DRF_DEF(2080, _CTRL_GPU_PARTITION_FLAG, _COMPUTE_SIZE, _MINI_HALF),
        QUARTER = LW2080_CTRL_GPU_PARTITION_FLAG_ONE_QUARTER_GPU,
        EIGHTH = LW2080_CTRL_GPU_PARTITION_FLAG_ONE_EIGHTHED_GPU,
        INVALID = ~0U
    };

    enum SYS_PIPE : UINT32
    {
        SYS0 = 0,
        SYS1,
        SYS2,
        SYS3,
        SYS4,
        SYS5,
        SYS6,
        SYS7,
        NOT_SUPPORT
    };

    enum PartitionMode : UINT32
    {
        LEGACY = LW2080_CTRL_GPU_SET_PARTITIONING_MODE_REPARTITIONING_LEGACY,
        MAX_PERF = LW2080_CTRL_GPU_SET_PARTITIONING_MODE_REPARTITIONING_MAX_PERF,
        REDUCED_PERF =  LW2080_CTRL_GPU_SET_PARTITIONING_MODE_REPARTITIONING_FAST_RECONFIG
    };

    GpuPartition * GetGpuPartition(UINT32 swizzid);
    const vector<GpuPartition*>& GetActiveGpuPartition() {return m_ActiveGpuPartitions;}
    LWGpuResource * GetGpuResource() const { return m_pGpuResource;  }
    SmcEngine * GetSmcEngine(const string & str);

    GpuPartition * CreateOnePartition
    (
        string partitionStr, 
        UINT32 gpcCount, 
        UINT32 ilwalidGpcCount, 
        SmcResourceController::PartitionFlag partitionFlag, 
        bool isValid, 
        string partitionName
    );
    SmcEngine * CreateOneSMCEngine(SmcResourceController::SYS_PIPE sys, UINT32 gpcCount, GpuPartition* pGpuPartition, string smcEngName);

    void AddGpuPartition(GpuPartition* pGpuPartition);

    RC AllocGpuPartitions(vector<GpuPartition *> & gpuPartition);
    RC FreeGpuPartitions(vector<GpuPartition *> & gpuPartition);
    RC AllocOrFreeSmcEngines(vector<GpuPartition *> & gpuPartition, bool isAlloc);

    static SmcResourceController::SYS_PIPE Colwert2SysPipe(const string & str);
    static string ColwertSysPipe2Str(SmcResourceController::SYS_PIPE sys);
    UINT32 GetSysPipeMask() const { return m_SysPipeMask; }
    UINT32 GetGpcMask() const { return m_GpcMask; }

    void PrintTable();
    ~SmcResourceController();
    RC Init();

    void FreeGpuPartition(vector<GpuPartition *> retiredGpuPartition);

    RC SetGpuPartitionMode(SmcResourceController::PartitionMode partitionMode);
    const char * GetGpuPartitionModeName(SmcResourceController::PartitionMode partitionMode);
    map<SmcResourceController::PartitionFlag, UINT32> GetAvailablePartitionTypeCount
    (
        vector<SmcResourceController::PartitionFlag> partitionFlagList
    );

    string GetSmcConfigStr() const { return m_SmcConfiguration; }
    ArgReader* GetArgReader() { return m_Reader; }
    SmcParser* GetSmcParser() { return m_pSmcParser.get(); }
    void PrintSmcInfo();
    bool IsSmcMemApi() { return m_IsSmcMemApi; }
    LwRm* GetProfilingLwRm();

private:
    RC CreateSMCPartitioning();
    RC PartitioningGPU();
    RC PartitioningVfGpu();
    RC CreateSys0OnlyPartition();
    bool GpuPartitionExists(GpuPartition * pNewAddedGpuPartition);
    RC AllocPartitionsRMCtrlCall
    (
        LW2080_CTRL_GPU_SET_PARTITIONS_PARAMS* params,
        vector<GpuPartition*>& allocGpuPartitions
    );
    SmcResourceController::PartitionFlag CheckAndGetPartitionFlag
    (
        SmcResourceController::PartitionFlag requestedPartitionFlag,
        UINT32 requestedPartitionCount
    );
    
    vector< GpuPartition * > m_ActiveGpuPartitions;
    string m_SmcConfiguration; // Also stores -smc_mem string
    LWGpuResource * m_pGpuResource;
    ArgReader * m_Reader;
    UINT32 m_SysPipeMask;
    UINT32 m_GpcMask;

    bool m_EnableDynamicTpcFs;
    unique_ptr<SmcParser> m_pSmcParser;
    bool m_IsSmcMemApi = false;
    LwRm* m_pProfilingLwRm = nullptr;
    LwRm::Handle m_ProfilingHandle = 0;
};

class SmcPrintInfo
{
public:
    static SmcPrintInfo * Instance();
    enum COL_ID
    {
        COL_SYS_PIPE = 0,
        COL_GPC_NUM,
        COL_SWIZZID,
        COL_PARTITION_STR_PATTERN,
        COL_GPU_DEVICE_ID,
        COL_GPC_MASK,
        COL_NUM,
    };

    void InitHeader();

    // Each row is a Entry
    struct Entry
    {
        Entry()
            : m_Columns(COL_NUM, "n/a")
        {
        };

        std::vector<string> m_Columns;
    };

    using Entries = list<Entry>;
    void AddSmcEngine(SmcEngine * pSmcEngine);
    void Print();
private:
    SmcPrintInfo();
    vector<UINT32> m_MaxEntryLengths;
    static const std::map<COL_ID, std::string> m_ColumnName;
    static const UINT32 m_MaxEntryLength = 64;
    static SmcPrintInfo * m_pInstance;
    Entries m_Entries;
    void UpdateMaxLength(COL_ID colID, UINT32 length);
    void SetSysPipe(SmcResourceController::SYS_PIPE sys);
    void SetGpcNum(const UINT32 gpcNum);
    void SetSwizzId(const SmcEngine * pSmcEngine);
    void SetStrPattern(const SmcEngine * pSmcEngine);
    void SetGpuDeviceId(const SmcEngine * pSmcEngine);
    void SetGpcMask(const UINT32 gpcMask);
};
#endif
