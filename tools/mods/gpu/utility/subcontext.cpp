/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "subcontext.h"
#include "tsg.h"
#include "gpu/include/gpudev.h"
#include "gpu/tests/gputestc.h"
#include "core/include/utility.h"
#include "class/cl9067.h"   // FERMI_CONTEXT_SHARE_A
#include <algorithm>

//--------------------------------------------------------------------
//! \brief Constructor
//!
Subcontext::Subcontext(Tsg *pTsg, LwRm::Handle hVASpace)
{
    m_pTsg     = pTsg;
    m_hVASpace = hVASpace;
    m_hSubctx  = 0;
    m_Id       = 0;
}

RC Subcontext::Alloc(LwU32 flags)
{
    return AllocInternal(flags);
}

RC Subcontext::AllocSpecified(LwU32 id)
{
    return AllocInternal(SPECIFIED, id);
}

RC Subcontext::AllocInternal(LwU32 flags, LwU32 id)
{
    RC                                rc;
    LW_CTXSHARE_ALLOCATION_PARAMETERS params = {0};

    params.hVASpace = m_hVASpace;
    params.flags    = flags;

    if (flags == SPECIFIED)
    {
        params.subctxId = id;
    }
    else
    {
        if (id != 0)
        {
            MASSERT(0);
            return RC::BAD_PARAMETER;
        }
    }

    CHECK_RC(m_pTsg->GetRmClient()->Alloc(m_pTsg->GetHandle(), &m_hSubctx, FERMI_CONTEXT_SHARE_A, &params));

    m_Id = params.subctxId;

    return rc;
}

void Subcontext::Free()
{
    if (m_hSubctx == 0)
    {
        MASSERT(!"Subcontext not allocated.");
        return;
    }

    m_pTsg->GetRmClient()->Free(GetHandle());
    m_hSubctx  = 0;
    m_Id       = 0;
}

Subcontext::~Subcontext()
{
    m_pTsg     = NULL;
    m_hVASpace = 0;
}
