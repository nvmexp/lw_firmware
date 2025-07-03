/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2009,2013-2019,2021 by LWPU Corporation.  All rights reserved. 
* All information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/
#include "mdiag/advschd/pmsurf.h"
#include "pm_t3d.h"
#include "mdiag/advschd/pmchan.h"
#include "trace_3d.h"   // for Trace3DTest
#include "mdiag/advschd/pmsmcengine.h"
#include "mdiag/smc/smcengine.h"

// ////////////////////////////////////////////////////////////////////////////
// Trace3DTest
// ////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------
//! \brief Ctor
//!
PmTest_Trace3D::PmTest_Trace3D
(
    PolicyManager *pPolicyManager,
    Trace3DTest   *pTest
) :
    PmTest(pPolicyManager, pTest->GetLwRmPtr()),
    m_Ptr(pTest),
    m_pSmcEngine(nullptr)
{
    MASSERT(m_Ptr);
}

//--------------------------------------------------------------------
//! \brief Dtor
//!
/* virtual */ PmTest_Trace3D::~PmTest_Trace3D()
{
}

//--------------------------------------------------------------------
//! \brief Returns the name of the surface
//!
/* virtual */ string PmTest_Trace3D::GetName() const
{
    return string(m_Ptr->GetTestName());
}

//--------------------------------------------------------------------
//! \brief Returns the type-name of the test
//!
/* virtual */ string PmTest_Trace3D::GetTypeName() const
{
   return "Trace3DTest";
}

//--------------------------------------------------------------------
//! Return a pointer that's guaranteed to be the same if two
//! PmTests wrap around the same internal object.  Used to avoid
//! adding the same test twice.
//!
/* virtual */ void *PmTest_Trace3D::GetUniquePtr() const
{
    return m_Ptr;
}

//--------------------------------------------------------------------
//! \brief Return the test's bound GpuDevice
//!
/* virtual */ GpuDevice *PmTest_Trace3D::GetGpuDevice() const
{
    return m_Ptr->GetBoundGpuDevice();
}

//--------------------------------------------------------------------
//! \brief Abort the test
//!
/* virtual */ RC PmTest_Trace3D::Abort()
{
    m_Ptr->AbortTest();
    return OK;
}

//--------------------------------------------------------------------
//! \brief Check whether the test is aborting
//!
/* virtual */ bool PmTest_Trace3D::IsAborting() const
{
    return m_Ptr->AbortedTest();
}

//--------------------------------------------------------------------
//! \brief Register surfaces created by policy manager to mdiag
//!
/* virtual */ RC PmTest_Trace3D::RegisterPmSurface
(
    const char *Name,
    PmSurface  *pSurf
) const
{
    LWGpuResource *pLwGpuRes = m_Ptr->GetGpuResource();
    if (pLwGpuRes)
    {
        return pLwGpuRes->AddSharedSurface(Name, pSurf->GetMdiagSurf());
    }

    return RC::SOFTWARE_ERROR;
}

//--------------------------------------------------------------------
//! \brief UnRegister surfaces created by policy manager in mdiag
//!
/* virtual */ RC PmTest_Trace3D::UnRegisterPmSurface(const char *Name) const
{
    LWGpuResource *pLwGpuRes = m_Ptr->GetGpuResource();
    if (pLwGpuRes)
    {
        return pLwGpuRes->RemoveSharedSurface(Name);
    }

    return RC::SOFTWARE_ERROR;
}

//--------------------------------------------------------------------
//! \brief send trace_3d event
//!
/* virtual */ void PmTest_Trace3D::SendTraceEvent(const char *eventName, const char *surfaceName) const
{
    Trace3DTest::TraceEventType eventType = Trace3DTest::GetEventTypeByStr(eventName);

    if (eventType == Trace3DTest::TraceEventType::Unknown)
    {
        DebugPrintf( "PmTest_Trace3D::SendTraceEvent: custom eventName:%s\n", eventName);
        m_Ptr->lwstomTraceEvent(eventName, "SurfaceName", surfaceName, true);
    }
    else
    {
        DebugPrintf( "PmTest_Trace3D::SendTraceEvent: eventName:%s\n", eventName);
        m_Ptr->traceEvent(eventType, "SurfaceName", surfaceName, true);
    }
}

/* virtual */ RC PmTest_Trace3D::GetUniqueVAS(PmVaSpace ** pVaSpace)
{
    return m_Ptr->GetUniqueVAS(pVaSpace);
}


//--------------------------------------------------------------------
//! \brief get pmSmcEngine 
//!
/* virtual */ PmSmcEngine * PmTest_Trace3D::GetSmcEngine()
{
    if (m_pSmcEngine)
        return m_pSmcEngine;

    SmcEngine * pSmcEngine = m_Ptr->GetSmcEngine();
    PolicyManager * pPolicyManager = PolicyManager::Instance();
    for (const auto & smcEngine : pPolicyManager->GetActiveSmcEngines())
    {
        if (*(smcEngine->GetSmcEngine()) == *pSmcEngine)
        {
            m_pSmcEngine = smcEngine;
            return smcEngine;
        }
    }

    return nullptr;
}
