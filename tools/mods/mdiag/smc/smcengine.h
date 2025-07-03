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

#ifndef SMC_ENGINE_H
#define SMC_ENGINE_H

#include "mdiag/smc/smcresourcecontroller.h"
#include "ctrl/ctrl2080/ctrl2080gr.h"
class GpuPartition;

class SmcEngine
{
public:
    explicit SmcEngine(UINT32 gpcCount, SmcResourceController::SYS_PIPE sys, 
            GpuPartition * pGpuPartition, string engName, bool useAllGpc = false);
    ~SmcEngine();
    SmcEngine(const SmcEngine &) = delete;
    SmcEngine & operator = (const SmcEngine &) = delete;
    bool operator == (const SmcEngine &) const;
    bool UseAllGpcs() const { return m_bUseAllGpcs; }
    UINT32 GetGpcCount() const { return m_GpcCount; }
    UINT32 GetSmcEngineId() const { return m_SmcEngineId; }
    SmcResourceController::SYS_PIPE GetSysPipe() const { return m_SysPipe; }
    GpuPartition * GetGpuPartition() const { return m_pGpuPartition; }
    void SetVeidCount(UINT32 veidCount);
    UINT32 GetVeidCount() const;
    bool IsAlloc() const { return m_IsAlloc; }
    void SetAlloc() { m_IsAlloc = true; }
    string GetInfoStr();
    void SetGpcMask(UINT32 gpcMask) { m_GpcMask = gpcMask; }
    UINT32 GetGpcMask() { return m_GpcMask; }
    static SmcEngine* GetFakeSmcEngine();
    static void FreeFakeSmcEngine() { delete s_pFakeSmcEngine; s_pFakeSmcEngine = nullptr; }
    string GetName() const { return m_Name; }
    void SetExelwtionPartitionId(UINT32 exelwtionPartitionId) { m_ExelwtionPartitionId = exelwtionPartitionId; }
    UINT32 GetExelwtionPartitionId() const { return m_ExelwtionPartitionId; }
    static const vector<string>& GetSmcEngineNames() { return s_SmcEngineNames; }
    static void AddSmcEngineName(string addSmcEngineName);
    static void RemoveSmcEngineName(string removeSmcEngineName);
    
    const static UINT32 m_UknownSmcId = -1;

protected:
    friend class GpuPartition;
    void SetSmcEngineId(UINT32 smcEngineId);

private:
    void SetCheckName();

    UINT32 m_GpcCount;
    GpuPartition * m_pGpuPartition;
    // physical smc ID
    // It just works for user to specify which smc engine it will be append to this trace
    SmcResourceController::SYS_PIPE m_SysPipe;
    // a per Gpu partition local SMC ID
    // It works for RM and not transparency for user
    UINT32 m_SmcEngineId;
    UINT32 m_VeidCount;
    // mark whether it is allocated.
    bool m_IsAlloc;
    const bool m_bUseAllGpcs;
    UINT32 m_GpcMask;
    string m_Name;
    UINT32 m_ExelwtionPartitionId;
    static UINT32 s_InternalNameIdx;
    static vector<string> s_SmcEngineNames;

    // A special SMC engine which is used for accessing legacy PGRAPH space in SMC mode.
    static SmcEngine *s_pFakeSmcEngine;
};

#endif
