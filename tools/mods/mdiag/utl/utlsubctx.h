/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLSUBCTX_H
#define INCLUDED_UTLSUBCTX_H

#include "mdiag/subctx.h"
#include "utlpython.h"
#include "utlvaspace.h"
#include "utltest.h"

// This class represents the C++ backing for the UTL Python SubCtx object.
// This class is a wrapper around SubCtx.
//
class UtlSubCtx
{
public:
    static void Register(py::module module);

    UtlSubCtx(shared_ptr<SubCtx> subCtx, UtlTest* pTest);
    ~UtlSubCtx();
    UtlVaSpace* GetVaSpace();

    static bool Equals(UtlSubCtx* o1, UtlSubCtx* o2);
    static bool NotEquals(UtlSubCtx* o1, UtlSubCtx* o2);

    SubCtx* GetRawPtr() { return m_pSubCtx; }
    UtlTest* GetTest() { return m_pTest; }

    static UtlSubCtx* Create(shared_ptr<SubCtx> subCtx, UtlTest* pTest);
    static UtlSubCtx* GetFromSubCtxPtr(shared_ptr<SubCtx> subCtx, UtlTest* pTest);
    static void FreeSubCtx(UtlTest* pTest);

    UtlSubCtx() = delete;
private:
    SubCtx* m_pSubCtx;
    UtlTest* m_pTest;

    static vector<unique_ptr<UtlSubCtx> > s_SubCtxs;
};
 
#endif
