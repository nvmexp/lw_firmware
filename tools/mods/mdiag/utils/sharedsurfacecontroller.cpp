/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "sharedsurfacecontroller.h"
#include "mdiag/utils/mdiagsurf.h"
#include <map>
#include "mdiag/sysspec.h"
#include "core/include/tasker.h"

DEFINE_MSG_GROUP(Mdiag, Gpu, SharedSurface, false);
#define ID() __MSGID__(Mdiag, Gpu, SharedSurface)

// first time - create a basic physical surface
// next time - check the lwrm to decide whehter need to duplicate surface
RC SharedSurfaceController::AllocPhysSurf(MdiagSurf * pSurf, LwRm* pLwRm)
{
    RC rc;

    const string name = pSurf->GetName();
    GpuDevice * pGpuDev = pSurf->GetGpuDev();
    // Handle the case ALLOC_PHYSICAL ... SHARED...
    if (pSurf->IsPhysicalOnly())
    {
        static const LwRm::Handle hVaSpace = ~0x0;
        // Second time - if not same lwrm then duplicate the handle
        SharedSurfKey key(name, hVaSpace, pLwRm);
        if (m_SharedSurfs.count(key))
        {
            MdiagSurf * pPhysSurf = std::get<0>(m_SharedSurfs[key]);
            if (pPhysSurf->IsAllocated())
            {
                if (pLwRm == pPhysSurf->GetLwRmPtr())
                {
                    // Not duplicated physical
                    std::get<1>(m_SharedSurfs[key])++;
                    InfoPrintf(ID(), "%s: Physical Surface in Shared Surface(name = %s, pLwRm = 0x%x) reference count ++ = %d.\n",
                            __FUNCTION__, name.c_str(), pLwRm, std::get<1>(m_SharedSurfs[key]));
                    return rc;
                }
                else
                {
                    CHECK_RC(pPhysSurf->DuplicateSurface(pLwRm));
                    InfoPrintf(ID(), "%s: Duplicated a shared physical memory. lwRm = %x, name = %s.\n", 
                            __FUNCTION__, pLwRm, name.c_str());
                    InfoPrintf(ID(), "%s: Physical Surface in Shared Surface(name = %s, pLwRm = 0x%x) reference count = 0.\n",
                            __FUNCTION__, name.c_str(), pLwRm, std::get<1>(m_SharedSurfs[key]));
                    return rc;
                }
            }
        }
        else
        {
            // First time - create the surface and add into m_SharedSurfs
            MdiagSurf * pMappedSurf = new MdiagSurf();
            InitInternalSurf(pMappedSurf, pSurf);
            pMappedSurf->Alloc(pGpuDev, pLwRm);
            SharedSurfKey key(name, hVaSpace, pLwRm);
            m_SharedSurfs[key] = make_tuple(pSurf, 0);
            *pSurf = *pMappedSurf;
        }
    }
    // Handle the case ALLOC_SURFACE ... SHARED ...
    else
    {
        // Second time - if not same lwrm then duplicate the handle
        if (m_PhysSurfs.count(name))
        {
            MdiagSurf * pPhysSurf = std::get<0>(m_PhysSurfs[name]);
            if (pPhysSurf->IsAllocated())
            {
                if (pLwRm == pPhysSurf->GetLwRmPtr())
                {
                    // Not duplicated physical
                    std::get<1>(m_PhysSurfs[name])++;
                    InfoPrintf(ID(), "%s: Physical Surface in Shared Surface(name = %s, pLwRm = 0x%x) reference count ++ = %d.\n",
                            __FUNCTION__, name.c_str(), pLwRm, std::get<1>(m_PhysSurfs[name]));
                    return rc;
                }
                else
                {
                    CHECK_RC(pPhysSurf->DuplicateSurface(pLwRm));
                    InfoPrintf(ID(), "%s: Duplicated a shared physical memory. lwRm = %x, name = %s.\n", 
                            __FUNCTION__, pLwRm, name.c_str());
                    InfoPrintf(ID(), "%s: Physical Surface in Shared Surface(name = %s, pLwRm = 0x%x) reference count = 0.\n",
                            __FUNCTION__, name.c_str(), pLwRm, std::get<1>(m_PhysSurfs[name]));
                    return rc;
                }
            }
        }
        else
        {
            // First time - create the surface and add into m_PhysSurfs
            MdiagSurf * pPhysSurf = new MdiagSurf();
            InitInternalSurf(pPhysSurf, pSurf);
            pPhysSurf->SetSpecialization(Surface2D::PhysicalOnly);
            CHECK_RC(pPhysSurf->Alloc(pGpuDev, pLwRm));
            m_PhysSurfs[name] = make_tuple(pPhysSurf, 0);
        }
    }

    InfoPrintf(ID(), "%s: Physical Surface in Shared Surface(name = %s, pLwRm = 0x%x) reference count = 0.\n",
            __FUNCTION__, name.c_str(), pLwRm, std::get<1>(m_PhysSurfs[name]));
    return rc;
}


RC SharedSurfaceController::AllocVirtSurf(MdiagSurf * pSurf, LwRm* pLwRm)
{
    RC rc;
    LwRm::Handle hVaSpace = pSurf->GetGpuVASpace();
    const string name = pSurf->GetName();
    GpuDevice * pGpuDev = pSurf->GetGpuDev();
    
    SharedSurfKey key(name, hVaSpace, pLwRm);
    if (m_VirtSurfs.find(key) != m_VirtSurfs.end())
    {
        std::get<1>(m_VirtSurfs[key])++;
        InfoPrintf(ID(), "%s: Virtual Surface in Shared Surface(name = %s, hVaSpace = %x, pLwRm = 0x%x) reference count ++ = %d.\n",
                __FUNCTION__, name.c_str(), hVaSpace, pLwRm, std::get<1>(m_VirtSurfs[key]));
        return OK;
    }
    MdiagSurf * pVirtSurf = new MdiagSurf();
    // ToDo: the init from hdr need to be consider
    InitInternalSurf(pVirtSurf, pSurf);
    pVirtSurf->SetGpuVASpace(hVaSpace);
    pVirtSurf->SetSpecialization(Surface2D::VirtualOnly);

    if (pVirtSurf->Alloc(pGpuDev, pLwRm) != OK)
    {
        ErrPrintf(ID(), "%s: Failed to alloc virtual memory.\n",
                __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    
    m_VirtSurfs[key] = make_tuple(pVirtSurf, 0);
    InfoPrintf(ID(), "%s: Virtual Surface in Shared Surface(name = %s, hVaSpace = %x, pLwRm = 0x%x) reference count = 0.\n",
            __FUNCTION__, name.c_str(), hVaSpace, pLwRm);
    return rc;
}

RC SharedSurfaceController::MapVirtToPhys(MdiagSurf * pSurf, LwRm* pLwRm)
{
    RC rc;
    LwRm::Handle hVaSpace = pSurf->GetGpuVASpace();
    const string name = pSurf->GetName();

    if (!pSurf->HasVirtual()) 
    {
        ErrPrintf(ID(), "%s: Error surface %s configuration. Failed to allocated "
                "virtual address.\n", __FUNCTION__, pSurf->GetName().c_str());
    }

    CHECK_RC(AllocVirtSurf(pSurf, pLwRm));
    
    SharedSurfKey key(name, hVaSpace, pLwRm);
    if (m_SharedSurfs.find(key) != m_SharedSurfs.end())
    {
        std::get<1>(m_SharedSurfs[key])++;
        *pSurf = *(std::get<0>(m_SharedSurfs[key]));
        InfoPrintf(ID(), "%s: Mapped Surface in Shared Surface(name = %s, hVaSpace = %x, pLwRm = 0x%x) reference count ++ = %d.\n",
                __FUNCTION__, name.c_str(), hVaSpace, pLwRm, std::get<1>(m_SharedSurfs[key]));
        return rc;
    }

    MdiagSurf * pVirtSurf = std::get<0>(m_VirtSurfs[key]);
    MdiagSurf * pPhysSurf = std::get<0>(m_PhysSurfs[name]);
    GpuDevice * pGpuDev = pSurf->GetGpuDev();

    MdiagSurf * pMappedSurf = new MdiagSurf();
    InitInternalSurf(pMappedSurf, pSurf);
    pMappedSurf->SetSpecialization(Surface2D::MapOnly);
    pMappedSurf->SetDmaBufferAlloc(false);
    CHECK_RC(pMappedSurf->MapVirtToPhys(pGpuDev, pVirtSurf, pPhysSurf, 
                0, 0, pLwRm));

    m_SharedSurfs[key] = make_tuple(pMappedSurf, 0);
    *pSurf = *pMappedSurf;
    
    InfoPrintf(ID(), "%s: Mapped Surface in Shared Surface(name = %s, hVaSpace = %x, pLwRm = 0x%x) reference count = 0.\n",
            __FUNCTION__, name.c_str(), hVaSpace, pLwRm);

    return rc;
}

SharedSurfaceController::~SharedSurfaceController()
{
    for (auto it = m_SharedSurfs.begin();
            it != m_SharedSurfs.end(); )
    {
        SharedSurfKey key = (*it).first;
        it = FreeSharedSurf(key.m_Name, key.m_hVaSpace, key.m_pLwRm);
    }

    for (auto it = m_VirtSurfs.begin();
            it != m_VirtSurfs.end(); )
    {
        SharedSurfKey key = (*it).first;
        it = FreeVirtSurf(key.m_Name, key.m_hVaSpace, key.m_pLwRm);
    }

    for (auto it = m_PhysSurfs.begin();
            it != m_PhysSurfs.end(); )
    {
        MdiagSurf * pSurf = std::get<0>((*it).second);
        if (pSurf == nullptr)
            continue;
        it = FreePhysSurf(pSurf->GetName(), pSurf->GetLwRmPtr());
    }
    
    for (auto it = m_Mutexs.begin(); it != m_Mutexs.end(); )
    {
        Tasker::FreeMutex((*it).second);
    }
}

MdiagSurf * SharedSurfaceController::GetSharedSurf
(
    const string name,
    LwRm::Handle hVaSpace,
    LwRm * pLwRm
)
{
    SharedSurfKey key(name, hVaSpace, pLwRm);
    // ToDo: string compare in the struct will hit error.
    for (const auto & it : m_SharedSurfs)
    {
        if (key == it.first)
        {
            return std::get<0>(it.second);
        }
    }
    
    return nullptr;
}

SharedSurfaceController::iterator SharedSurfaceController::FreeSharedSurf
(
    const string name,
    LwRm::Handle hVaSpace,
    LwRm * pLwRm
)
{
    SharedSurfKey key(name, hVaSpace, pLwRm);
    for (auto & it : m_SharedSurfs)
    {
        if (key == it.first)
        {
            if (std::get<1>(it.second) != 0)
            {
                std::get<1>(it.second)--;
                InfoPrintf(ID(), "%s: Mapped Surface in Shared Surface(name = %s, hVaSpace = %x, pLwRm = 0x%x) reference count -- = %d.\n",
                        __FUNCTION__, name.c_str(), hVaSpace, pLwRm, std::get<1>(it.second));
                return m_SharedSurfs.find(key);
            }
            else
            {
                InfoPrintf(ID(), "%s: Free mapped surface in Shared Surface(name = %s, hVaSpace = %x, pLwRm = 0x%x).\n",
                        __FUNCTION__, name.c_str(), hVaSpace, pLwRm);
                delete std::get<0>(it.second); 
                return m_SharedSurfs.erase(m_SharedSurfs.find(key));
            }
        }
    }
    MASSERT("Can't find matched surface.");
    return m_SharedSurfs.end();
}

SharedSurfaceController::iterator SharedSurfaceController::FreeVirtSurf
(
    const string name,
    LwRm::Handle hVaSpace,
    LwRm * pLwRm
)
{
    SharedSurfKey key(name, hVaSpace, pLwRm);
    // ToDo: string compare in the struct will hit error.
    for (auto & it : m_VirtSurfs)
    {
        if (key == it.first)
        {
            if (std::get<1>(it.second) != 0)
            {
                std::get<1>(it.second)--;
                InfoPrintf(ID(), "%s: Virtual Surface in Shared Surface(name = %s, hVaSpace = %x, pLwRm = 0x%x) reference count -- = %d.\n",
                        __FUNCTION__, name.c_str(), hVaSpace, pLwRm, std::get<1>(it.second));
                return m_VirtSurfs.find(key);
            }
            else
            {
                InfoPrintf(ID(), "%s: Free virtual surface in Shared Surface(name = %s, hVaSpace = %x, pLwRm = 0x%x).\n",
                        __FUNCTION__, name.c_str(), hVaSpace, pLwRm);
                InfoPrintf("Free virtual address 0x%llx in shared surface %s.\n",
                        std::get<0>(it.second)->GetCtxDmaOffsetGpu(pLwRm), name.c_str());
                delete std::get<0>(it.second); 
                return m_VirtSurfs.erase(m_VirtSurfs.find(key));
            }
        }
    }
    MASSERT("Can't find matched surface.");
    return m_VirtSurfs.end();
}

map<string, SharedSurfaceController::SharedSurfRef>::iterator SharedSurfaceController::FreePhysSurf
(
    const string name,
    LwRm * pLwRm
)
{
    if (m_PhysSurfs.find(name) != m_PhysSurfs.end())
    {
        if (std::get<1>(m_PhysSurfs[name]) != 0)
        {
            // ToDO::Seperated the surface free with pLwRm
            std::get<1>(m_PhysSurfs[name])--;
            InfoPrintf(ID(), "%s: Physical Surface in Shared Surface(name = %s) reference count -- = %d.\n",
                    __FUNCTION__, name.c_str(), std::get<1>(m_PhysSurfs[name]));
            return m_PhysSurfs.find(name);
        }
        else
        {
            InfoPrintf(ID(), "%s: Free Physical Surface in Shared Surface(name = %s).\n",
                    __FUNCTION__, name.c_str());
            delete std::get<0>(m_PhysSurfs[name]); 
            return m_PhysSurfs.erase(m_PhysSurfs.find(name));
        }
    }
    MASSERT("Can't find matched surface.");
    return m_PhysSurfs.end();
}


RC SharedSurfaceController::AllocSharedSurf
(
    MdiagSurf * pSurf,
    LwRm* pLwRm
)
{
    RC rc;
    
    if (pSurf->HasPhysical()) 
    {
        CHECK_RC(AllocPhysSurf(pSurf, pLwRm));
    }

    if (pSurf->HasMap())
    {
        CHECK_RC(MapVirtToPhys(pSurf, pLwRm));
    }

    return rc;
}

void SharedSurfaceController::InitInternalSurf
(
    MdiagSurf * pDestSurf,
    MdiagSurf * pSrcSurf
)
{
    *pDestSurf = *pSrcSurf;
}

bool SharedSurfaceController::SharedSurfKey::operator <(const SharedSurfKey & key) const
{
    if (strcmp(m_Name.c_str(), key.m_Name.c_str()) < 0)
    {
        return true;
    }

    if (m_Name == key.m_Name)
    {
        if (m_hVaSpace < key.m_hVaSpace)
        {
            return true;
        }
        else if (m_hVaSpace == key.m_hVaSpace)
        {
            return m_pLwRm < key.m_pLwRm;
        }
    }

    return false;
}

bool SharedSurfaceController::SharedSurfKey::operator ==(const SharedSurfKey & key) const
{
    if ((m_Name == key.m_Name) &&
            (m_hVaSpace == key.m_hVaSpace) &&
            (m_pLwRm == key.m_pLwRm))
    {
        return true;
    }

    return false;
}

void SharedSurfaceController::AcquireMutex(const string & name)
{
    if (m_Mutexs.find(name) == m_Mutexs.end())
    {
        void * mutex = Tasker::AllocMutex(name.c_str(), Tasker::mtxUnchecked);
        m_Mutexs[name] = mutex;
    }

    Tasker::AcquireMutex(m_Mutexs[name]);
}

void SharedSurfaceController::ReleaseMutex(const string & name)
{
    if (m_Mutexs.find(name) == m_Mutexs.end())
    {
        ErrPrintf(ID(), "Can't release the mutex %s.\n", name.c_str());
        return;
    }

    Tasker::ReleaseMutex(m_Mutexs[name]);
}
