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


#ifndef SHARED_SURFACE_CONTAINER_H
#define SHARED_SURFACE_CONTAINER_H

class MdiagSurf;
#include <map>
#include "mdiag/vaspace.h"
using namespace std;

class SharedSurfaceController
{
public:
    MdiagSurf * GetSharedSurf(const string name, LwRm::Handle hVaSpace, LwRm * pLwRm);
    RC AllocSharedSurf(MdiagSurf * pSurf, LwRm* pLwRm);
    ~SharedSurfaceController();
    struct SharedSurfKey
    {
        SharedSurfKey(string name, LwRm::Handle hVaSpace, LwRm * pLwRm) :
            m_Name(name),
            m_hVaSpace(hVaSpace),
            m_pLwRm(pLwRm)
        {

        };
        bool operator < (const SharedSurfKey & key) const;
        bool operator == (const SharedSurfKey & key) const;

        string  m_Name;
        LwRm::Handle m_hVaSpace;
        LwRm * m_pLwRm;
    };
    void AcquireMutex(const string & name);
    void ReleaseMutex(const string & name);
    typedef tuple<MdiagSurf *, UINT32> SharedSurfRef;
    typedef map<SharedSurfKey, SharedSurfRef>::iterator iterator;
    iterator FreeSharedSurf(const string name, LwRm::Handle hVaSpace, LwRm * pLwRm);
    iterator FreeVirtSurf(const string name, LwRm::Handle hVaSpace, LwRm * pLwRm);
    map<string, SharedSurfRef>::iterator FreePhysSurf(const string name, LwRm * pLwRm);

private:
    void InitInternalSurf(MdiagSurf * pDestSurf, MdiagSurf * pSrcSurf); 
    RC AllocPhysSurf(MdiagSurf * pSurf, LwRm* pLwRm);
    RC AllocVirtSurf(MdiagSurf * pSurf, LwRm* pLwRm);
    RC MapVirtToPhys(MdiagSurf * pSurf, LwRm* pLwRm);
    map<SharedSurfKey, SharedSurfRef> m_SharedSurfs;
    map<SharedSurfKey, SharedSurfRef> m_VirtSurfs;
    map<string, SharedSurfRef> m_PhysSurfs;
    // mutex holder <shared surface name, mutex>
    map<string, void *> m_Mutexs;
};

#endif
