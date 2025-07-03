/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "subctx.h"
#include "core/include/lwrm.h"
#include "class/cl9067.h" //FERMI_CONTEXT_SHARE_A
#include "core/include/tee.h"
#include "core/include/rc.h"
#include "core/include/massert.h"
#include "lwgpu.h"
#include "mdiag/tests/gpu/trace_3d/tracetsg.h"
#include "mdiag/tests/gpu/lwgpu_tsg.h"

SubCtx::SubCtx
(
    LWGpuResource * pGpuRes,
    LwRm* pLwRm,
    UINT32 testId,
    string name
):
    m_pGpuResource(pGpuRes),
    m_pLwRm(pLwRm),
    m_TestId(testId),
    m_Name(name),
    m_Handle(0),
    m_pTraceTsg(0),
    m_Veid(VEID_END),
    m_SpecifiedVeid(VEID_END),
    m_IsSetTpcMask(false),
    m_MaxTpcCount(0),
    m_MaxSingletonTpcGpcCount(0),
    m_MaxPluralTpcGpcCount(0),
    m_Watermark(UNINITIALIZED_WATERMARK_VALUE),
    m_HasSetWatermark(false)
{
    MASSERT(m_pGpuResource != NULL);
}

SubCtx::~SubCtx()
{
    m_pLwRm->Free(m_Handle);
}

LwRm::Handle SubCtx::GetHandle()
{
    if (m_Handle)
    {
        return m_Handle;
    }

    RC rc;
    LW_CTXSHARE_ALLOCATION_PARAMETERS  params = {0};
    params.hVASpace = GetVaSpace()->GetHandle();

    // 1. If clients want a SYNC subcontext then they should always set
    //    params.flags = LW_CTXSHARE_ALLOCATION_FLAGS_SUBCONTEXT_SYNC (RM will only assign VEID 0 to SYNC subcontext).
    //    No other flag must be set
    // 2. If clients want ASYNC subcontext then there are two options:
    //    a. params.flags = LW_CTXSHARE_ALLOCATION_FLAGS_SUBCONTEXT_ASYNC and let RM choose the VEID.
    //    b. params.flags = LW_CTXSHARE_ALLOCATION_FLAGS_SUBCONTEXT_SPECIFIED and
    //       params.subctxId = m_Veid (here VEID can also be 0). In this case, RM will try to give you back a VEID = m_Veid.

    // VEID0 cannot do graphics and async-compute
    // Graphics and async-compute cannot be mixed on PBDMA0

#ifdef LW_VERIF_FEATURES
    params.subctxId = m_Veid;
    if (IsGrSubContext() || IsI2memSubContext())
    {
        params.flags = LW_CTXSHARE_ALLOCATION_FLAGS_SUBCONTEXT_SYNC;
    }
    else
    {
        params.flags = LW_CTXSHARE_ALLOCATION_FLAGS_SUBCONTEXT_SPECIFIED;
    }
#else
    if (m_Veid > 0)
        params.flags = LW_CTXSHARE_ALLOCATION_FLAGS_SUBCONTEXT_ASYNC;
    else
        params.flags = LW_CTXSHARE_ALLOCATION_FLAGS_SUBCONTEXT_SYNC;
#endif

    if(m_pTraceTsg->GetLWGpuTsg()->GetHandle())
    {
        rc = m_pLwRm->Alloc(m_pTraceTsg->GetLWGpuTsg()->GetHandle(), &m_Handle, FERMI_CONTEXT_SHARE_A, &params);
    }

    MASSERT((rc == OK) && "alloc subctx failed.\n");

    return m_Handle;

}

RC SubCtx::SetVaSpace(shared_ptr<VaSpace> pVaSpace)
{
    RC rc;
    if (m_pVaSpace && m_pVaSpace->GetHandle() != pVaSpace->GetHandle())
    {
        ErrPrintf("cannot set vaspace %s to subctx %s which already has vaspace %s.\n",
                  (pVaSpace->GetName()).c_str(), m_Name.c_str(), (m_pVaSpace->GetName()).c_str());
        return RC::ILWALID_FILE_FORMAT;
    }
    m_pVaSpace = pVaSpace;
    return rc;
}

RC SubCtx::SetTraceTsg(TraceTsg * pTraceTsg)
{
    RC rc;
    if (m_pTraceTsg && m_pTraceTsg->GetName() != pTraceTsg->GetName())
    {
        ErrPrintf("cannot set tsg %s to subctx %s which already has tsg %s.\n",
                  (pTraceTsg->GetName()).c_str(), m_Name.c_str(), (m_pTraceTsg->GetName()).c_str());
        return RC::ILWALID_FILE_FORMAT;
    }
    m_pTraceTsg = pTraceTsg;
    return rc;
}

RC SubCtx::SetSpecifiedVeid(UINT32 veid)
{
    RC rc;
    if (IsSpecifiedVeid() && m_SpecifiedVeid != veid)
    {
        ErrPrintf("Cannot specify veid %d to SubCtx %s which already has veid %d.\n",
                  veid, m_Name.c_str(), m_SpecifiedVeid);
        return RC::ILWALID_FILE_FORMAT;
    }

    m_SpecifiedVeid = veid;
    return rc;
}

RC SubCtx::SetGrSubContext()
{
    DebugPrintf("%s: set subcontext %s to Gr, specified VEID %d, VEID %d.\n",
                __FUNCTION__, m_Name.c_str(), m_SpecifiedVeid, m_Veid);

    // Sanity check
    const UINT32 grVeid = LWGpuTsg::GetGrSubcontextVeid();
    if (IsSpecifiedVeid() && m_SpecifiedVeid != grVeid)
    {
        ErrPrintf("%s: Subcontext %s has GR channel. Expect VEID %d but the specified VEID is %d\n",
                  __FUNCTION__, m_Name.c_str(), grVeid, m_SpecifiedVeid);
        return RC::ILWALID_FILE_FORMAT;
    }

    if (IsVeidValid() && m_Veid != grVeid)
    {
        ErrPrintf("%s: Subcontext %s has GR channel. Expect VEID %d but the VEID which has been set is %d\n",
                  __FUNCTION__, m_Name.c_str(), grVeid, m_Veid);
        return RC::ILWALID_FILE_FORMAT;
    }

    // Tag the subcontext
    m_IsGrSubContext = true;

    return OK;
}

RC SubCtx::SetI2memSubContext()
{
    DebugPrintf("%s: set subcontext %s to I2MEM, specified VEID %d, VEID %d.\n",
                __FUNCTION__, m_Name.c_str(), m_SpecifiedVeid, m_Veid);

    // Tag the subcontext
    m_IsI2memSubContext = true;

    return OK;
}

RC SubCtx::SetTpcMask(const vector<UINT32> & values)
{
    RC rc;

    if(m_IsSetTpcMask)
    {
        if(values.size() == m_TpcMasks.size())
        {
            for(UINT32 i = 0; i < m_TpcMasks.size(); ++i)
            {
                if(values[i] != m_TpcMasks[i])
                {
                    ErrPrintf("The subctx tpc mask doesn't have the unique values. One is 0x%08x and the other is 0x%08x.\n",
                              m_TpcMasks[i], values[i]);
                    return RC::ILWALID_FILE_FORMAT;
                }
            }
        }
        else
        {
            ErrPrintf("The subctx tpc mask doesn't have the unique values.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
    }
    else
    {
        m_TpcMasks = values;
        m_IsSetTpcMask = true;
        ConfigValidPL();
    }

    return rc;
}

//!------------------------------------------------------------------------------------------
//! \brief Configure valid Partitioning Lmem (PL) Table where TPC is enable in the
//!  Partitioning Enable (PE) Table
//!
void SubCtx::ConfigValidPL()
{
    // the m_TpcMasks is little endian. Example 4'b1010 which means the TPC 1 and TPC 3 is enable.
    // in step1, m_ValidPLs wants to collect all the valid TPC number per subcontext into the map. It just sets key not value here.
    // in step2, it will accumulate the value to assign the local memory number to each subcontext.For this case, it will
    // assign 2 local memory to this subcontext. The first local memory is assigned to TPC 0 and the second local memory is assigned
    // to TPC 2.

    #define FOUR_BIT_VALUE 16

    // 1. set valid TPC number
    for(UINT32 i = 0; i < m_TpcMasks.size(); i++)
    {
        for(UINT32 j = 0; j < FOUR_BIT_VALUE; j++)
        {
            if(m_TpcMasks[i] & (1ULL << j))
            {
                UINT32 TPCValue = i * FOUR_BIT_VALUE + j;
                m_ValidPLs[TPCValue] = 0;
            }
        }
    }

    // For dynamic mode, RM will allocate the lmem inside. Mods just
    // need to set all zero lmem id to RM.
    if(m_pTraceTsg->GetPartitionMode() == PEConfigureFile::STATIC)
    {
        // 2. set the increment
        UINT32 lmemCount = 0;
        for(ValidPLs::iterator it = m_ValidPLs.begin();
                it != m_ValidPLs.end(); ++it)
        {
            it->second = lmemCount;
            lmemCount++;
        }
    }

}

