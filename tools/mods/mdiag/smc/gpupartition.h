/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef GPU_PARTITION_H
#define GPU_PARTITION_H

class SmcEngine;
class LWGpuResource;
class LwRm;
class GpuSubdevice;
#include "mdiag/smc/smcresourcecontroller.h"

class GpuPartition
{
public:
    explicit GpuPartition(const string & configStr, LWGpuResource * pGpuRes, const string & partitionName,
            bool bUseAllGpcs = false);
    explicit GpuPartition(const string & configStr, UINT32 gpcCount, UINT32 ilwalidGpcCount, 
            SmcResourceController::PartitionFlag partitionFlag,LWGpuResource * pGpuRes,
            bool isValid, const string & partitionName);
    ~GpuPartition();
    GpuPartition(const GpuPartition &) = delete;
    GpuPartition & operator = (const GpuPartition &) = delete;
    bool operator == (const GpuPartition &);
    UINT32 GetGpcCount();
    bool UseAllGpcs() const { return m_bUseAllGpcs;  }
    SmcEngine * GetSmcEngine(UINT32 smcEngineId);
    SmcEngine * GetSmcEngine(SmcResourceController::SYS_PIPE sys);
    SmcEngine * GetSmcEngineByName(const string& name);
    void AddSmcEngine(SmcEngine * pSmcEngine);
    void RemoveSmcEngines(vector<SmcEngine *> smcEngines);
    UINT32 GetSwizzId() const { return m_SwizzId; }
    void SetSwizzId(UINT32 swizzId) { m_SwizzId = swizzId; }
    const string & GetConfigStr() const { return m_ConfigStr; }
    const vector<SmcEngine*>& GetActiveSmcEngines() const { return m_ActiveSmcEngines; }
    LWGpuResource * GetLWGpuResource() const { return m_pGpuResource; }
    RC AllocSmcEngines(vector<SmcEngine *> & smcEngines);
    RC FreeSmcEngines(vector<SmcEngine *> & smcEngines);
    RC AllocAllSmcEngines();
    RC FreeAllSmcEngines();
    bool IsValid() const { return m_IsValid; }
    void AddIlwalidGpcCount(UINT32 ilwalidGpcCount) { m_IlwalidGpcCount += ilwalidGpcCount; }
    RC UpdateGpuPartitionInfo();
    UINT32 GetSmcEngineGpcCount(UINT32 smcEngineId);
    SmcResourceController::SYS_PIPE GetSmcEngineSysPipe(UINT32 smcEngineId);
    SmcEngine* GetSmcEngineByType(UINT32 engineId);
    vector<SmcResourceController::SYS_PIPE> GetAllSyspipes() const { return m_AllSyspipes; }
    vector<SmcResourceController::SYS_PIPE> GetAvailableSyspipes() const { return m_AvailableSyspipes; }
    bool SmcEngineExists(SmcEngine * pNewAddedSmcEngine);
    void SanityCheck(LwRm* pLwRm, GpuSubdevice* pSubdev) const;
    SmcResourceController* GetSmcResourceController() const;
    void SetSyspipes();
    string GetName() const { return m_Name; }
    SmcResourceController::PartitionFlag GetPartitionFlag () const { return m_PartitionFlag; }
    void SetPartitionFlag (SmcResourceController::PartitionFlag partitionFlag) { m_PartitionFlag = partitionFlag; }
    static void AddGpuPartitionName(string addGpuPartitionName);
    static void RemoveGpuPartitionName(string removeGpuPartitionName);

protected:
    friend class SmcResourceController;
    void SetAllSyspipes(vector<SmcResourceController::SYS_PIPE> syspipes);
    void UpdateSmcEngineId(SmcEngine * pSmcEngine);

private:
    RC GetNonGrEngineCount();
    void PrintPartitionErrorMessage(SmcResourceController::SYS_PIPE sysPipe) const;
    void SetCheckName();

    UINT32 m_SwizzId;
    UINT32 m_CeCount;
    UINT32 m_LwDecCount;
    UINT32 m_LwEncCount;
    UINT32 m_LwJpgCount;
    UINT32 m_OfaCount;
    vector< SmcEngine * > m_ActiveSmcEngines;
    //Partition Flag rely on RM define
    UINT32 m_GpcCount;
    UINT32 m_IlwalidGpcCount;
    SmcResourceController::PartitionFlag m_PartitionFlag;
    // m_ConfigStr can uniquely identified the Gpu Partition
    const string m_ConfigStr;
    // Stores all the syspies (used/unused by SmcEngines) for this partition
    vector<SmcResourceController::SYS_PIPE> m_AllSyspipes;
    // Stores unused syspipes for this partition
    vector<SmcResourceController::SYS_PIPE> m_AvailableSyspipes;
    LWGpuResource * m_pGpuResource;
    bool m_IsValid;
    bool m_bUseAllGpcs;
    UINT32 m_SmcEngineCount;
    string m_Name;
    static UINT32 s_InternalNameIdx;
    static vector<string> s_GpuPartitionNames;
};

#endif
