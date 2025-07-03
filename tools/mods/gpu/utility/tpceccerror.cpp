/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "tpceccerror.h"
#include "gpu/include/gpusbdev.h"

/* static */ set<TpcEccError*> TpcEccError::s_Registry;
/* static */ Tasker::Mutex     TpcEccError::s_RegistryMutex = Tasker::NoMutex();

TpcEccError::TpcEccError(GpuSubdevice *pSubdev, const string & testName)
  : m_pSubdevice(pSubdev)
   ,m_TestName(testName)
{
    MASSERT(s_RegistryMutex != Tasker::NoMutex());

    ClearErrors();
    m_Mutex = Tasker::CreateMutex("TpcEccError", Tasker::mtxLast);

    Tasker::MutexHolder lockRegistry(s_RegistryMutex);
    s_Registry.insert(this);
}

TpcEccError::~TpcEccError()
{
    MASSERT(s_RegistryMutex != Tasker::NoMutex());
    Tasker::MutexHolder lockRegistry(s_RegistryMutex);
    s_Registry.erase(this);
    m_Mutex = Tasker::NoMutex();
    m_pSubdevice = nullptr;
}

UINT64 TpcEccError::GetErrorCount() const
{
    UINT64 errCount = 0ULL;

    for (const auto & lwrGpcCounts : m_ErrCounts.eccCounts)
    {
        for (const auto & lwrTpcCounts : lwrGpcCounts)
        {
            errCount += lwrTpcCounts.corr;
            errCount += lwrTpcCounts.uncorr;
        }
    }
    return errCount;
}

void TpcEccError::LogErrors(const Ecc::DetailedErrCounts &newErrors)
{
    Tasker::MutexHolder lock(m_Mutex);
    m_ErrCounts += newErrors;
}

RC TpcEccError::ResolveResult()
{
    Tasker::MutexHolder lock(m_Mutex);
    if (GetErrorCount() == 0ULL)
    {
        return RC::OK;
    }

    RC rc;

    Printf(Tee::PriNormal, "\n=== ECC ERRORS BY PHYSICAL GPC/TPC ===\n\n");

    Printf(Tee::PriNormal, "GPC  TPC           CORRECTABLE         UNCORRECTABLE\n"
                           "----------------------------------------------------\n");
    for (size_t virtGpc = 0; virtGpc < m_ErrCounts.eccCounts.size(); virtGpc++)
    {
        UINT32 hwGpc;
        CHECK_RC(m_pSubdevice->VirtualGpcToHwGpc(static_cast<UINT32>(virtGpc), &hwGpc));
        for (size_t virtTpc = 0; virtTpc < m_ErrCounts.eccCounts[virtGpc].size(); virtTpc++)
        {
            UINT32 hwTpc;
            CHECK_RC(m_pSubdevice->VirtualTpcToHwTpc(hwGpc,
                                                     static_cast<UINT32>(virtTpc),
                                                     &hwTpc));
            Printf(Tee::PriNormal, "%3u  %3u  %20llu  %20llu\n",
                   hwGpc,
                   hwTpc,
                   m_ErrCounts.eccCounts[virtGpc][virtTpc].corr,
                   m_ErrCounts.eccCounts[virtGpc][virtTpc].uncorr);
        }
    }

    return RC::BAD_MEMORY;
}

//------------------------------------------------------------------------------
/* static */ void TpcEccError::Initialize()
{
    if (s_RegistryMutex == Tasker::NoMutex())
    {
        s_RegistryMutex = Tasker::CreateMutex("TpcEccError::s_pRegistryMutex",
                                              Tasker::mtxNLast);
    }
}

//------------------------------------------------------------------------------
// Use LogEccError when an asynchronous ECC error oclwrs.
//------------------------------------------------------------------------------
/* static */ void TpcEccError::LogErrors
(
    const GpuSubdevice * pSubdev,
    const Ecc::DetailedErrCounts &newErrors
)
{
    if (s_RegistryMutex == Tasker::NoMutex())
        return;

    Tasker::MutexHolder lockRegistry(s_RegistryMutex);

    for (TpcEccError *pTpcEccError: s_Registry)
    {
        if (pTpcEccError->GetSubdevice() == pSubdev)
        {
            pTpcEccError->LogErrors(newErrors);
        }
    }
}

//------------------------------------------------------------------------------
/* static */ void TpcEccError::Shutdown()
{
    if (s_RegistryMutex != Tasker::NoMutex())
        s_RegistryMutex = Tasker::NoMutex();
}
