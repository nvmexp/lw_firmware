/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011,2014-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_SUBCONTEXT_H
#define INCLUDED_SUBCONTEXT_H

#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

#include "lwos.h"

//----------------------------------------------------------------------
//! \brief Subcontext class
//!
//! This class manages a subcontext class FERMI_CONTEXT_SHARE_A.
//!
//! Subcontext is allocated as children of a TSG.
//!
//!
//!
class Subcontext
{
public:
    static const LwU32 SYNC  = LW_CTXSHARE_ALLOCATION_FLAGS_SUBCONTEXT_SYNC ;
    static const LwU32 ASYNC = LW_CTXSHARE_ALLOCATION_FLAGS_SUBCONTEXT_ASYNC;
    Subcontext(Tsg *pTsg, LwRm::Handle hVASpace);
    virtual ~Subcontext();
    RC Alloc(LwU32 flags);
    RC AllocSpecified(LwU32 id);
    void Free();
    LwRm::Handle GetHandle()  { return m_hSubctx; }
    LwRm::Handle GetVASpaceHandle()  { return m_hVASpace; }
    Tsg * GetTsg()  { return m_pTsg; }
    LwU32 GetId()  { return m_Id; }
private:
    RC AllocInternal(LwU32 flag, LwU32 id = 0);
    LwRm::Handle m_hVASpace;
    LwRm::Handle m_hSubctx;
    Tsg *m_pTsg;
    static const LwU32 SPECIFIED = LW_CTXSHARE_ALLOCATION_FLAGS_SUBCONTEXT_SPECIFIED;
    LwU32 m_Id;
};

#endif // INCLUDED_SUBCONTEXT_H
