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

#include "utlsubctx.h"
#include "utlkwargsmgr.h"
#include "utltest.h"
#include "utlutil.h"
#include "utlgpu.h"
#include "mdiag/sysspec.h"

 vector<unique_ptr<UtlSubCtx> > UtlSubCtx::s_SubCtxs;

void UtlSubCtx::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();

    py::class_<UtlSubCtx> subCtx(module, "SubCtx", "Sub context");
    subCtx.def("GetVaSpace", &UtlSubCtx::GetVaSpace,
    	UTL_DOCSTRING("SubCtx.GetVaSpace", "Returns the VaSpace corresponding to the sub-context"), py::return_value_policy::reference);
    subCtx.def("__eq__", &UtlSubCtx::Equals);
    subCtx.def("__ne__", &UtlSubCtx::NotEquals);
}

bool UtlSubCtx::Equals(UtlSubCtx* o1, UtlSubCtx* o2)
{
	if (o1 == nullptr || o2 == nullptr)
	{
		return false;
	}

	return o1->m_pSubCtx == o2->m_pSubCtx;
}

bool UtlSubCtx::NotEquals(UtlSubCtx* o1, UtlSubCtx* o2)
{
	return !Equals(o1, o2);
}

UtlSubCtx::UtlSubCtx(shared_ptr<SubCtx> subCtx, UtlTest* pTest)
{
    if (subCtx)
    {
        m_pSubCtx = subCtx.get();
    }

    m_pTest = pTest;
}

UtlVaSpace* UtlSubCtx::GetVaSpace()
{
    shared_ptr<VaSpace> pVaSpace = m_pSubCtx->GetVaSpace();
    return UtlVaSpace::GetVaSpace(pVaSpace->GetHandle(), m_pTest);
}

UtlSubCtx* UtlSubCtx::GetFromSubCtxPtr(shared_ptr<SubCtx> subCtx, UtlTest* pTest)
{
    if (subCtx)
    {
        for (auto& subCtxPtr : s_SubCtxs)
        {
            if (subCtxPtr->GetRawPtr() == subCtx.get() && subCtxPtr->GetTest() == pTest)
            {
                return subCtxPtr.get();
            }
        }
    }

    return nullptr;
}

UtlSubCtx* UtlSubCtx::Create(shared_ptr<SubCtx> subCtx, UtlTest* pTest)
{
    if (GetFromSubCtxPtr(subCtx, pTest) == nullptr)
    {
        unique_ptr<UtlSubCtx> utlSubCtx(new UtlSubCtx(subCtx, pTest));
        s_SubCtxs.push_back(move(utlSubCtx));
        return s_SubCtxs.back().get();
    }
    else
    {
        // Do nothing
        return nullptr;
    }
}

void UtlSubCtx::FreeSubCtx(UtlTest* pTest)
{
    for (auto it = s_SubCtxs.begin(); it != s_SubCtxs.end(); )
    {
        if ((*it)->GetTest() == pTest)
        {
           it = s_SubCtxs.erase(it);
        }
        else
        {
            it++;
        }
    }
}

UtlSubCtx::~UtlSubCtx()
{
    m_pSubCtx = nullptr;
}
