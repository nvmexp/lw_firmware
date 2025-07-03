/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef INCLUDED_UTLVASPACE_H
#define INCLUDED_UTLVASPACE_H

#include "utlpython.h"
#include "core/include/lwrm.h"
#include "mdiag/vaspace.h"

class UtlTest;
class UtlGpu;

// This class represents a vaspace from the point of view of a
// UTL script.  Lwrrently this class is a wrapper around vaspace. 
//
class UtlVaSpace
{
public:
    static void Register(py::module module);
    static UtlVaSpace* Create(VaSpace * pVaSpace, UtlTest * pTest, UtlGpu * pGpu);
    static UtlVaSpace* CreatePy(py::kwargs kwargs);
    static UtlVaSpace * GetVaSpace(LwRm::Handle hVaSpace, UtlTest * pTest);
    static void FreeVaSpace(UtlTest * pTest, LwRm * pLwRm);
    UtlTest * GetTest() const { return m_pTest; }
    LwRm * GetLwRm() const { return m_pVaSpace->GetLwRmPtr(); }
    LwRm::Handle GetHandle() const { return m_pVaSpace->GetHandle(); }
    UINT32 GetVaSpaceId() const;
    bool IsAtsEnabled() const { return m_pVaSpace->IsAtsEnabled(); }
private:
    UtlVaSpace(VaSpace * pVaSpace, UtlTest * pTest, UtlGpu * pGpu);

    static vector<unique_ptr<UtlVaSpace> > s_VaSpaces; 
    VaSpace * m_pVaSpace;
    UtlTest * m_pTest;
    UtlGpu * m_pGpu;
};

#endif

