/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2018,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwlwl_alloc_mgr.h"
#include "core/include/tee.h"
#include "gpu/tests/lwca/lwdawrap.h"

#include <memory>

using namespace LwLinkDataTestHelper;

//------------------------------------------------------------------------------
//! \brief Constructor
//!
AllocMgrLwda::AllocMgrLwda
(
    const Lwca::Instance *pLwda,
    GpuTestConfiguration * pTestConfig,
    Tee::Priority pri,
    bool bFailRelease
)
 :  AllocMgr(pTestConfig, pri, bFailRelease)
   ,m_pLwda(pLwda)
{
}

//------------------------------------------------------------------------------
//! Acquire a block of LWCA memory for use on the specified device either in
//! FB (device) or in sysmem (host)
//!
RC AllocMgrLwda::AcquireMemory
(
    Lwca::Device dev,
    UINT64 size,
    Memory::Location loc,
    LwdaMemory **ppMem
)
{
    // First try to find an unused block of memory that matches the size requested
    // in the correct location
    if (m_Memory.find(dev) != m_Memory.end())
    {
        for (size_t ii = 0; ii < m_Memory[dev].size(); ii++)
        {
            if (!m_Memory[dev][ii].bAcquired &&
                (m_Memory[dev][ii].pLwdaMemory->GetSize() == size) &&
                (m_Memory[dev][ii].pLwdaMemory->GetLocation() == loc))
            {
                *ppMem = m_Memory[dev][ii].pLwdaMemory.get();
                m_Memory[dev][ii].bAcquired = true;
                Printf(GetPrintPri(),
                       "%s : Acquired memory on device %s at offset 0x%llx\n",
                       __FUNCTION__,
                       dev.GetName().c_str(),
                       m_Memory[dev][ii].pLwdaMemory->GetDevicePtr());
                return OK;
            }
        }
    }
    else
    {
        vector<LwdaMemoryAlloc> emptyAlloc;
        m_Memory[dev] = move(emptyAlloc);
    }

    RC rc;
    auto pMemPriv = make_unique<LwdaMemoryPriv>(dev);
    CHECK_RC(pMemPriv->Alloc(m_pLwda, loc, size));
    m_Memory[dev].emplace_back(true, move(pMemPriv));
    *ppMem = m_Memory[dev].back().pLwdaMemory.get();

    Printf(GetPrintPri(),
           "%s : Created memory on device %s at offset 0x%llx\n",
           __FUNCTION__,
           dev.GetName().c_str(),
           m_Memory[dev].back().pLwdaMemory->GetDevicePtr());
    return rc;
}

//------------------------------------------------------------------------------
//! Acquire the specified LWCA function from the loaded module
//!
RC AllocMgrLwda::AcquireFunction
(
    Lwca::Device dev
   ,const string & functionName
   ,LwdaFunction **ppFunction
)
{
    RC rc;

    // First try to find an unused surface in the correct location
    if (m_Functions.find(dev) != m_Functions.end())
    {
        for (size_t ii = 0; ii < m_Functions[dev].size(); ii++)
        {
            if (!m_Functions[dev][ii].bAcquired &&
                (m_Functions[dev][ii].functionName == functionName))
            {
                *ppFunction = m_Functions[dev][ii].pFuncData.get();
                m_Functions[dev][ii].bAcquired = true;
                Printf(GetPrintPri(),
                       "%s : Acquired function %s on device %s at %p\n",
                       __FUNCTION__,
                       functionName.c_str(),
                       dev.GetName().c_str(),
                       m_Functions[dev][ii].pFuncData.get());
                return OK;
            }
        }
    }
    else
    {
        vector<FunctionAlloc> emptyAlloc;
        m_Functions[dev] = move(emptyAlloc);
    }

    // Lazy load the module on each device
    if (!m_Modules.count(dev))
    {
        m_Modules[dev] = make_unique<Lwca::Module>();
        CHECK_RC(m_pLwda->LoadNewestSupportedModule(dev, "lwlink", m_Modules[dev].get()));
    }
    auto pFunc = make_unique<LwdaFunction>();

    pFunc->device = dev;
    pFunc->stream = m_pLwda->CreateStream(dev);;

    if (!pFunc->stream.IsValid())
    {
        Printf(Tee::PriError,
               "%s : Failed to create on device %s for function %s\n",
               __FUNCTION__,
               dev.GetName().c_str(),
               functionName.c_str());
        return RC::SOFTWARE_ERROR;
    }

    pFunc->func = m_Modules[dev]->GetFunction(functionName.c_str());
    CHECK_RC(pFunc->func.InitCheck());

    m_Functions[dev].emplace_back(false, functionName, move(pFunc));
    *ppFunction = m_Functions[dev].back().pFuncData.get();

    Printf(GetPrintPri(),
           "%s : Created function %s on device %s at %p\n",
           __FUNCTION__,
           functionName.c_str(),
           dev.GetName().c_str(),
           *ppFunction);
    return rc;
}

//------------------------------------------------------------------------------
RC AllocMgrLwda::ReleaseMemory(const LwdaMemory * pMem)
{
    Lwca::Device dev = pMem->GetDevice();
    if (m_Memory.find(dev) == m_Memory.end())
    {
        Printf(GetReleaseFailPri(), "%s : No memory allocated on device %s\n",
               __FUNCTION__, dev.GetName().c_str());
        return GetReleaseFailRc();
    }

    for (size_t ii = 0; ii < m_Memory[dev].size(); ii++)
    {
        if (m_Memory[dev][ii].pLwdaMemory.get() == pMem)
        {
            m_Memory[dev][ii].bAcquired = false;
            Printf(GetPrintPri(),
                   "%s : Released memory on device %s at offset 0x%llx\n",
                   __FUNCTION__,
                   dev.GetName().c_str(),
                   m_Memory[dev][ii].pLwdaMemory->GetDevicePtr());
            return OK;
        }
    }
    Printf(GetReleaseFailPri(), "%s : Memory not found on device %s\n",
           __FUNCTION__, dev.GetName().c_str());
    return GetReleaseFailRc();
}

//------------------------------------------------------------------------------
RC AllocMgrLwda::ReleaseFunction(const LwdaFunction * pLwdaFunction)
{
    Lwca::Device dev = pLwdaFunction->device;
    if (m_Functions.find(dev) == m_Functions.end())
    {
        Printf(GetReleaseFailPri(), "%s : No functions allocated on device %s\n",
               __FUNCTION__, dev.GetName().c_str());
        return GetReleaseFailRc();
    }

    for (size_t ii = 0; ii < m_Functions[dev].size(); ii++)
    {
        if (m_Functions[dev][ii].pFuncData.get() == pLwdaFunction)
        {
            m_Functions[dev][ii].bAcquired = false;
            Printf(GetPrintPri(),
                   "%s : Released function %p on device %s\n",
                   __FUNCTION__,
                   pLwdaFunction,
                   dev.GetName().c_str());
            return OK;
        }
    }
    Printf(GetReleaseFailPri(), "%s : Function not found on device %s\n",
           __FUNCTION__, dev.GetName().c_str());
    return GetReleaseFailRc();
}

//------------------------------------------------------------------------------
/* virtual */ RC AllocMgrLwda::Shutdown()
{
    RC rc;

    for (auto & lwrDevMem : m_Memory)
    {
        for (auto & lwrMem : lwrDevMem.second)
        {
            if (lwrMem.bAcquired)
            {
                Printf(Tee::PriError,
                       "Multiple references to memory on device %s during Cleanup\n",
                       lwrDevMem.first.GetName().c_str());
                rc = RC::SOFTWARE_ERROR;
            }
            lwrMem.pLwdaMemory->Free();
        }
    }
    m_Memory.clear();

    for (auto & lwrDevFunc : m_Functions)
    {
        for (auto & lwrFunc : lwrDevFunc.second)
        {
            if (lwrFunc.bAcquired)
            {
                Printf(Tee::PriError,
                       "Multiple references to function on device %s during Cleanup\n",
                       lwrDevFunc.first.GetName().c_str());
                rc = RC::SOFTWARE_ERROR;
            }
            lwrFunc.pFuncData->stream.Free();
        }
    }
    m_Functions.clear();

    for (auto & lwrModule : m_Modules)
    {
        lwrModule.second->Unload();
    }
    m_Modules.clear();

    rc = AllocMgr::Shutdown();
    return rc;
}

//------------------------------------------------------------------------------
// LwdaMemory allocation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Similar to the Lwca::Global::Get function.  If the wrapped memory is FB memory
// this will copy the memory into the host memory pointed to by the provided
// parameter
RC AllocMgrLwda::LwdaMemory::Get(LwdaMemory *pHost) const
{
    if (!m_DeviceMem.IsValid())
        return RC::SOFTWARE_ERROR;
    if (!pHost->m_HostMem.GetSize())
        return RC::SOFTWARE_ERROR;
    return m_DeviceMem.Get(&pHost->m_HostMem);
}

//------------------------------------------------------------------------------
UINT64 AllocMgrLwda::LwdaMemory::GetDevicePtr()
{
    if (m_HostMem.GetSize())
        return m_HostMem.GetDevicePtr(m_Device);
    if (m_DeviceMem.IsValid())
        return m_bLoopback ? m_DeviceMem.GetLoopbackDevicePtr() : m_DeviceMem.GetDevicePtr();

    return 0ULL;
}

//------------------------------------------------------------------------------
Memory::Location AllocMgrLwda::LwdaMemory::GetLocation() const
{
    if (m_HostMem.GetSize())
        return Memory::Coherent;
    if (m_DeviceMem.IsValid())
        return Memory::Fb;

    return Memory::Optimal;
}

//------------------------------------------------------------------------------
void * AllocMgrLwda::LwdaMemory::GetPtr()
{
    if (!m_HostMem.GetSize())
        return nullptr;
    return m_HostMem.GetPtr();
}

//------------------------------------------------------------------------------
UINT64 AllocMgrLwda::LwdaMemory::GetSize() const
{
    if (m_HostMem.GetSize())
        return m_HostMem.GetSize();
    if (m_DeviceMem.IsValid())
        return m_DeviceMem.GetSize();

    return 0ULL;
}

//------------------------------------------------------------------------------
RC AllocMgrLwda::LwdaMemory::MapLoopback()
{
    if (m_bLoopback)
        return RC::OK;

    if (!m_DeviceMem.IsValid())
    {
        Printf(Tee::PriError, "%s : Cannot map loopback on host memory!\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    RC rc;
    CHECK_RC(m_DeviceMem.MapLoopback());
    m_bLoopback = true;
    return rc;
}

//------------------------------------------------------------------------------
RC AllocMgrLwda::LwdaMemory::UnmapLoopback()
{
    if (!m_bLoopback)
        return RC::OK;

    if (!m_DeviceMem.IsValid())
    {
        Printf(Tee::PriError, "%s : Cannot unmap loopback on host memory!\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    RC rc;
    CHECK_RC(m_DeviceMem.UnmapLoopback());
    m_bLoopback = false;
    return rc;
}

//------------------------------------------------------------------------------
// LwdaMemoryPriv allocation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
AllocMgrLwda::LwdaMemoryPriv::~LwdaMemoryPriv()
{
    Free();
}

//------------------------------------------------------------------------------
RC AllocMgrLwda::LwdaMemoryPriv::Alloc
(
    const Lwca::Instance *pLwda
   ,Memory::Location loc
   ,UINT64 size
)
{
    if ((loc != Memory::Coherent) && (loc != Memory::Fb))
    {
        Printf(Tee::PriError, "%s : Only Coherent and FB memory allocations are supported!\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    RC rc;

    if (loc == Memory::Fb)
    {
        return pLwda->AllocDeviceMem(m_Device, size, &m_DeviceMem);
    }

    CHECK_RC(pLwda->AllocHostMem(m_Device, size, &m_HostMem));
    return OK;
}

//------------------------------------------------------------------------------
void AllocMgrLwda::LwdaMemoryPriv::Free()
{
    if (m_HostMem.GetSize())
        m_HostMem.Free();
    if (m_DeviceMem.IsValid())
        m_DeviceMem.Free();
}
