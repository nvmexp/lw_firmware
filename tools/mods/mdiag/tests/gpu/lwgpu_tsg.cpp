/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/**
 * @file   lwgpu_tsg.h
 * @brief  LWGpuTsg: TSG in mdiag level;
 *                   Shared Tsgs cross mdiag tests;
 *                   Communicate with LWRM to alloc/free tsg handle.
 *
Relation diagram:

        ------------------------>------------------------------
        |                                                      |
    TraceTsg <====> TraceChannel ---> LWGpuChannel <====> LWGpuTsg(shared) <--- LWGpuResource
        ||===<====> TraceChannel ---> LWGpuChannel <====>===|| |                    |
                                                            || |                    |
    TraceTsg <====> TraceChannel ---> LWGpuChannel <====>===|| |                    |
         |                                                     |                    |
         --------------------->--------------------------------                     |
                                                                                    |
                                                      ... LWGpuTsg(shared) <--------|
                                                      ... LWGpuTsg(shared) <--------|

         ------------------------>-----------------------------
         |                                                     |
    TraceTsg <====> TraceChannel ---> LWGpuChannel <====> LWGpuTsg(Non-shared)
*/
#include <vector>
#include <string>
#include "mdiag/utils/utils.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/smc/smcengine.h"
#include "lwgpu_tsg.h"
#include "mdiag/utl/utl.h"

constexpr UINT32 s_MaxVeidCount = 64;

LWGpuTsg::LWGpuTsg
(
    const string& name,
    LWGpuResource* lwgpu,
    const ArgReader* params,
    LwRm::Handle hVASpace,
    bool isShared, 
    LwRm* pLwRm,
    SmcEngine* pSmcEngine
) :
    m_TsgName(name),
    m_pLWGpuRes(lwgpu),
    m_pParams(params),
    m_RefCount(0),
    m_IsShared(isShared),
    m_ErrCtxDmaHandle(0),
    m_PartitionMode(PEConfigureFile::NOT_SET),
    m_pLwRm(pLwRm),
    m_pSmcEngine(pSmcEngine),
    m_VeidManager(m_pSmcEngine ? m_pSmcEngine->GetVeidCount() : s_MaxVeidCount)
{
    SetVASpaceHandle(hVASpace);
    MASSERT(0 != m_pLWGpuRes);
    UINT32 seed0 = lwgpu->GetLwgpuArgs()->ParamUnsigned("-alloc_chid_randseed0", 0x1111);
    UINT32 seed1 = lwgpu->GetLwgpuArgs()->ParamUnsigned("-alloc_chid_randseed1", 0x2222);
    UINT32 seed2 = lwgpu->GetLwgpuArgs()->ParamUnsigned("-alloc_chid_randseed2", 0x3333);
    m_VeidManager.SetRandomSeed(seed0, seed1, seed2);
}

LWGpuTsg::~LWGpuTsg()
{
    MASSERT(0 == m_GpuChannels.size());

    if (Utl::HasInstance())
    {
        Utl::Instance()->RemoveTsg(this);
    }

    if (m_IsShared)
    {
        m_pLWGpuRes->RemoveSharedGpuTsg(this, m_pSmcEngine);
    }
}

/* static */
LWGpuTsg* LWGpuTsg::Factory
(
    const string& name,
    LWGpuResource* lwgpu,
    const ArgReader* params,
    UINT32 hwClass,
    LwRm::Handle hVASpace,
    bool isShared, LwRm* pLwRm,
    UINT32 engineId,
    SmcEngine* pSmcEngine
)
{
    LWGpuTsg* pGpuTsg = 0;
    if (isShared)
    {
        pGpuTsg = lwgpu->GetSharedGpuTsg(name, pSmcEngine);
        if (0 == pGpuTsg)
        {
            pGpuTsg = new LWGpuTsg(name, lwgpu, params, hVASpace, isShared, pLwRm, pSmcEngine);
            lwgpu->AddSharedGpuTsg(pGpuTsg, pSmcEngine);
        }
    }
    else
    {
        pGpuTsg = new LWGpuTsg(name, lwgpu, params, hVASpace, isShared, pLwRm, pSmcEngine);
    }

    // Alloc channel handle
    MASSERT(0 != pGpuTsg);
    if (0 == pGpuTsg->GetHandle())
    {
        pGpuTsg->GetTestConfiguration()->BindGpuDevice(lwgpu->GetGpuDevice());
        pGpuTsg->GetTestConfiguration()->BindRmClient(pLwRm);
        pGpuTsg->SetClass(hwClass);
        pGpuTsg->SetEngineId(engineId);

        RC rc = pGpuTsg->Alloc();
        if (OK != rc)
        {
            delete pGpuTsg;

            ErrPrintf("%s: Call RM to create tsg %s failed\n", __FUNCTION__, name.c_str());
            return 0;
        }

        InfoPrintf("Assigned tsg <%s> -> handle <0x%x>\n",
                    name.c_str(), pGpuTsg->GetHandle());
    }

    return pGpuTsg;
}

bool LWGpuTsg::HasGrEngineObjectCreated() const
{
    list<LWGpuChannel*>::const_iterator cit = m_GpuChannels.begin();
    for (; cit != m_GpuChannels.end(); cit ++)
    {
        if ((*cit)->HasGrEngineObjectCreated(false))
        {
            return true;
        }
    }

    return false;
}

bool LWGpuTsg::HasVideoEngineObjectCreated() const
{
    list<LWGpuChannel*>::const_iterator cit = m_GpuChannels.begin();
    for (; cit != m_GpuChannels.end(); cit ++)
    {
        if ((*cit)->HasVideoEngineObjectCreated(false))
        {
            return true;
        }
    }

    return false;
}

bool LWGpuTsg::HasCopyEngineObjectCreated() const
{
    list<LWGpuChannel*>::const_iterator cit = m_GpuChannels.begin();
    for (; cit != m_GpuChannels.end(); cit ++)
    {
        if ((*cit)->HasCopyEngineObjectCreated(false))
        {
            return true;
        }
    }

    return false;
}

RC LWGpuTsg::AddLWGpuChannel(LWGpuChannel* gpuch)
{
    list<LWGpuChannel*>::iterator it = m_GpuChannels.begin();
    for (; it != m_GpuChannels.end(); it ++)
    {
        if (*it == gpuch)
        {
            break;
        }
    }

    if (it == m_GpuChannels.end())
    {
        m_GpuChannels.push_back(gpuch);
    }

    return OK;
}

RC LWGpuTsg::RemoveLWGpuChannel(LWGpuChannel* gpuch)
{
    list<LWGpuChannel*>::iterator it = m_GpuChannels.begin();
    for (; it != m_GpuChannels.end(); it ++)
    {
        if (*it == gpuch)
        {
            m_GpuChannels.erase(it);
            break;
        }
    }

    return OK;
}

/* virtual */ RC LWGpuTsg::AllocErrorNotifier(Surface2D* pErrorNotifier)
{
    RC rc;

    CHECK_RC(LWGpuChannel::AllocErrorNotifierRC(m_pLWGpuRes,
            m_pParams,
            pErrorNotifier, m_pLwRm));

    // Save the error notifier buffer's context DMA handle so it can
    // be bound later when channels are allocated.  Channels in a TSG
    // don't have inidividual error notifiers; there is one error notifier
    // for the entire TSG.
    m_ErrCtxDmaHandle = pErrorNotifier->GetCtxDmaHandle();

    return rc;
}

RC LWGpuTsg::SetPartitionMode(PEConfigureFile::PARTITION_MODE traceTsgMode, bool * bSetModeTohw)
{
    RC rc;

    if(m_PartitionMode == PEConfigureFile::NOT_SET)
    {
        if(traceTsgMode == PEConfigureFile::NONE)
        {
            *bSetModeTohw = false;
        }
        else
        {
            // Example0: shared tsg0 STATIC trace0
            //           shared tsg0 STATIC trace1
            // Result0:  shared tsg0 STATIC
            //           call RM set partition table mode 1st time
            //           call RM set PE and PL every time
            *bSetModeTohw = true;
        }

        m_PartitionMode = traceTsgMode; // set the mode no matter it is none/static/dynamic
    }
    else
    {
        // Example0: shared tsg0 NONE trace0
        //           shared tsg0 STATIC trace1
        // Result0:  return error when set partition table at 2nd time
        // Example1: shared tsg0 STATIC trace0
        //           shared tsg0 NONE trace1
        // Result1:  shared tsg0 STATIC
        //           return error when set partition table at 2nd time
        // Example2: shared tsg0 STATIC trace0
        //           shared tsg0 DYNAMIC trace1
        // Result2:  MASSERT at the 2nd time
        if(m_PartitionMode != traceTsgMode)
        {
             ErrPrintf("%s: Trying to set two different partitioning mode(%s vs. %s) for same context\n",
                       __FUNCTION__, PEConfigureFile::GetPartitionModeName(m_PartitionMode),
                       PEConfigureFile::GetPartitionModeName(traceTsgMode));
             return RC::ILWALID_FILE_FORMAT;
        }

        // Example1: shared tsg0 STATIC trace0
        //           shared tsg0 STATIC trace1
        // Result1:  shared tsg0 STATIC
        //           *NOT* call RM set partition table at 2nd time
        *bSetModeTohw = false;
    }

    return rc;
}

LWGpuTsg::VeidManager::VeidManager(UINT32 maxVeid) :
    m_MaxVeid(maxVeid),
    m_FreeAsyncVeidCount(maxVeid-1),
    m_AllocationMode(ALLOC_END)
{
    for (UINT32 i = 0; i < VeidManager::SIZE; i++)
        m_VeidTable[i] = false;
}

void LWGpuTsg::VeidManager::SetRandomSeed(UINT32 seed0, UINT32 seed1, UINT32 seed2)
{
    m_pRandomStream.reset(new RandomStream(seed0, seed1, seed2));
}

RC LWGpuTsg::VeidManager::AllocVeid(UINT32* allocatedVeid, const shared_ptr<SubCtx>& pSubCtx)
{
    RC rc;

    UINT32 veid = 0;
    if (pSubCtx->IsGrSubContext())
    {
        CHECK_RC(RegisterVeid(GR_SUBCONTEXT_VEID));
        veid = GR_SUBCONTEXT_VEID;
    }
    else if (pSubCtx->IsSpecifiedVeid())
    {
        veid = pSubCtx->GetSpecifiedVeid();
        CHECK_RC(AllocSpecifiedVeid(veid));
    }
    else
    {
        CHECK_RC(AllocRandomVeid(&veid));
    }

    *allocatedVeid = veid;
    return rc;
}

RC LWGpuTsg::VeidManager::AllocRandomVeid(UINT32 *pRetVeid)
{
    RC rc;
    CHECK_RC(SetAllocationMode(ALLOC_RANDOM));

    UINT32 veid;
    if (m_FreeAsyncVeidCount == 0)
    {
        // assign the !0 veid first, if 1-63 has been assigned then try 0
        // 1.if tsg has gr channel, 0 should be assigned before,
        // it will return error
        // 2. if channels in tsg are all computer channel, 0 should be ok
        veid = GR_SUBCONTEXT_VEID;
    }
    else
    {
        veid = m_pRandomStream->RandomUnsigned(1, m_MaxVeid - 1);
        while (m_VeidTable[veid])
        {
            veid = ((veid + 1) == m_MaxVeid) ? 1 : (veid + 1);
        }
    }

    InfoPrintf("Assign the random veid %d.\n", veid);
    CHECK_RC(RegisterVeid(veid));

    *pRetVeid = veid;
    return OK;
}

RC LWGpuTsg::VeidManager::AllocSpecifiedVeid(UINT32 veid)
{
    RC rc;
    CHECK_RC(SetAllocationMode(ALLOC_SPECIFIED));
    CHECK_RC(RegisterVeid(veid));
    return rc;
}
//---------------------------------------------------
// brief introduction: register a veid to the veid table
RC LWGpuTsg::VeidManager::RegisterVeid(UINT32 veid)
{
    RC rc;

    if (veid >= m_MaxVeid)
    {
        ErrPrintf("veid %d is out of range (Max VEID = %d) !\n", veid, m_MaxVeid - 1);
        return RC::ILWALID_FILE_FORMAT;
    }

    if (m_VeidTable[veid])
    {
        ErrPrintf("veid %d have been allocated !\n", veid);
        return RC::ILWALID_FILE_FORMAT;
    }

    m_VeidTable[veid] = true;
    //make sure 0 is the last available veid
    if ( veid != 0) --m_FreeAsyncVeidCount;
    return rc;
}

RC LWGpuTsg::VeidManager::SetAllocationMode(VeidManager::AllocMode mode)
{
    RC rc;
    if (m_AllocationMode == ALLOC_END)
    {
        m_AllocationMode = mode;
    }
    if (m_AllocationMode != mode)
    {
        ErrPrintf("user specified VEID cannot mix with MODS VEID allocation\n");
        rc = RC::ILWALID_FILE_FORMAT;
    }
    return rc;
}
