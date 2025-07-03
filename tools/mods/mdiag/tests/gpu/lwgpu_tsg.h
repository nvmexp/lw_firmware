/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef INCLUDED_LWGPUTSG_H
#define INCLUDED_LWGPUTSG_H

#ifndef INCLUDED_TSG_H
#include "gpu/utility/tsg.h"
#endif

class ArgReader;
class LWGpuChannel;
class LWGpuResource;
class RandomStream;
class SmcEngine;
typedef list<LWGpuChannel*> TsgGpuChannels;

#include "mdiag/tests/gpu/trace_3d/pe_configurefile.h"

//------------------------------------------------------------------------------
// TraceTsg -- holds per-Tsg resources for Trace3DTest.
//
class LWGpuTsg : public Tsg
{
public:
    LWGpuTsg(const string& name, LWGpuResource* lwgpu,
             const ArgReader* params, LwRm::Handle hVASpace,
             bool isShared, LwRm* pLwRm, SmcEngine* pSmcEngine);
    ~LWGpuTsg();

    static LWGpuTsg* Factory(const string& name, LWGpuResource* lwgpu,
                             const ArgReader *params, UINT32 hwClass,
                             LwRm::Handle hVASpace, bool isShared, LwRm* pLwRm,
                             UINT32 engineId, SmcEngine* pSmcEngine);

    string GetName() const { return m_TsgName; }
    bool IsShared() const { return m_IsShared; }
    SmcEngine* GetSmcEngine() const { return m_pSmcEngine; }
    LWGpuResource* GetLwGpuRes() const { return m_pLWGpuRes; }
    UINT32 GetRefCount() const { return m_RefCount; }

    bool HasGrEngineObjectCreated() const;
    bool HasVideoEngineObjectCreated() const;
    bool HasCopyEngineObjectCreated() const;

    RC AddLWGpuChannel(LWGpuChannel* gpuch);
    RC RemoveLWGpuChannel(LWGpuChannel* gpuch);

    void AddRef() { m_RefCount ++; }
    void RelRef() { m_RefCount --; }

    static UINT32 GetGrSubcontextVeid() { return VeidManager::GetGrSubcontextVeid(); }
    RC AllocVeid(UINT32* allocatedVeid, const shared_ptr<SubCtx>& pSubCtx) {
        return m_VeidManager.AllocVeid(allocatedVeid, pSubCtx); }

    LwRm::Handle GetErrCtxDmaHandle() { return m_ErrCtxDmaHandle; }

    RC SetPartitionMode(PEConfigureFile::PARTITION_MODE traceTsgMode, bool * bSetModeTohw);
    PEConfigureFile::PARTITION_MODE GetPartitionMode() { return m_PartitionMode; }
    TsgGpuChannels GetTsgGpuChannels() { return m_GpuChannels; }
    bool GetZlwllModeSet() const { return m_ZlwllModeSet; }
    void SetZlwllModeSet() { m_ZlwllModeSet = true; }

protected:
    RC AllocErrorNotifier(Surface2D *pErrorNotifier);

private:
    class VeidManager
    {
    public:
        VeidManager(UINT32 maxVeid);

        static UINT32 GetGrSubcontextVeid() { return GR_SUBCONTEXT_VEID; }
        RC AllocVeid(UINT32* allocatedVeid, const shared_ptr<SubCtx>& pSubCtx);
        void SetRandomSeed(UINT32 seed0, UINT32 seed1, UINT32 seed2);
    private:
        enum AllocMode
        {
            ALLOC_RANDOM = 0,
            ALLOC_SPECIFIED = 1,
            ALLOC_END
        };

        RC SetAllocationMode(VeidManager::AllocMode mode);
        RC AllocSpecifiedVeid(UINT32 veid);
        RC AllocRandomVeid(UINT32 *pRetVeid);
        RC RegisterVeid(UINT32 veid);

        UINT32 m_MaxVeid;

        static const UINT32 GR_SUBCONTEXT_VEID = 0;
        static const UINT32 SIZE = 64;
        bool m_VeidTable[SIZE];
        unique_ptr<RandomStream> m_pRandomStream;
        UINT32 m_FreeAsyncVeidCount;
        AllocMode m_AllocationMode;
    };

    string m_TsgName;
    TsgGpuChannels m_GpuChannels;
    LWGpuResource* m_pLWGpuRes;
    const ArgReader* m_pParams;
    UINT32 m_RefCount;
    bool m_IsShared;
    bool m_ZlwllModeSet = false;

    // The context DMA handle of the TSG's error notifier buffer.
    LwRm::Handle m_ErrCtxDmaHandle;

    PEConfigureFile::PARTITION_MODE m_PartitionMode;
    LwRm* m_pLwRm;
    SmcEngine* m_pSmcEngine;
    VeidManager m_VeidManager;
};
#endif
